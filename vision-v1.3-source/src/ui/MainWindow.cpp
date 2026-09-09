#include "MainWindow.h"
#include "VideoSurface.h"
#include "player/MpvEngine.h"
#include "Version.h"
#include "VideoGeometry.h"
#include "WindowInteraction.h"
#include "TransportIcons.h"
#include <QtWidgets>
#include <QWindow>
#include <QScreen>
#include <QJsonArray>
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <cmath>

namespace vision {
namespace {
class SeekSlider final : public QSlider {
public:
    explicit SeekSlider(QWidget* parent) : QSlider(Qt::Horizontal,parent) { setMouseTracking(true); }
    double duration = 0;
protected:
    void mousePressEvent(QMouseEvent* e) override {
        if(e->button()!=Qt::LeftButton){QSlider::mousePressEvent(e);return;}
        setSliderDown(true);setValue(QStyle::sliderValueFromPosition(minimum(),maximum(),qRound(e->position().x()),qMax(1,width())));e->accept();
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        int value=QStyle::sliderValueFromPosition(minimum(),maximum(),qRound(e->position().x()),qMax(1,width()));
        if(isSliderDown())setValue(value);
        QToolTip::showText(e->globalPosition().toPoint(),QString::number(duration*value/qMax(1,maximum()),'f',3)+QStringLiteral(" 秒"),this);e->accept();
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if(e->button()==Qt::LeftButton&&isSliderDown()){setValue(QStyle::sliderValueFromPosition(minimum(),maximum(),qRound(e->position().x()),qMax(1,width())));setSliderDown(false);e->accept();}
        else QSlider::mouseReleaseEvent(e);
    }
};
QString stamp(double seconds, bool milliseconds = false)
{
    qint64 ms = qMax<qint64>(0, qRound64(seconds * 1000));
    QString s = QString("%1:%2:%3").arg(ms/3600000,2,10,QChar('0')).arg(ms/60000%60,2,10,QChar('0')).arg(ms/1000%60,2,10,QChar('0'));
    return milliseconds ? s + QString(".%1").arg(ms%1000,3,10,QChar('0')) : s;
}
QPushButton* button(const QString& text, const QString& tooltip, QWidget* parent)
{
    auto* b = new QPushButton(text, parent); b->setToolTip(tooltip); b->setCursor(Qt::PointingHandCursor); b->setFocusPolicy(Qt::NoFocus); b->setMinimumHeight(32); return b;
}
}
MainWindow::MainWindow(QWidget* parent, const QString& dataRoot, bool headless) : QMainWindow(parent), library_(dataRoot), settings_(library_.settings()), headless_(headless)
{
    setWindowTitle(QString("vision v%1").arg(kVersion)); setWindowFlag(Qt::FramelessWindowHint); resize(720,480); setMinimumSize(90,100); setAcceptDrops(true);
    setStyleSheet(QStringLiteral(
        "QMainWindow,QDialog{background:#14171d;color:#e8ecf4;} QWidget{font-family:'Segoe UI','Microsoft YaHei UI';font-size:12px;color:#e8ecf4;}"
        "QPushButton,QToolButton,QComboBox{background:#232833;border:1px solid #303747;border-radius:6px;padding:5px 10px;}"
        "QPushButton:hover,QToolButton:hover{background:#303a4c;border-color:#005fff;} QPushButton:pressed{background:#005fff;}"
        "QPushButton#primary{background:#005fff;border:0;font-weight:600;} QPushButton#primary:hover{background:#2475ff;}"
        "QLabel#brand{font-size:22px;font-weight:600;color:#ffffff;} QLabel#muted{color:#8c98ac;}"
        "QSlider::groove:horizontal{height:4px;background:#333b4b;border-radius:2px;} QSlider::sub-page:horizontal{background:#005fff;border-radius:2px;}"
        "QSlider::handle:horizontal{background:#f1f5ff;border:2px solid #005fff;width:10px;margin:-5px 0;border-radius:6px;}"
        "QListWidget{background:#191e27;border:0;outline:0;} QListWidget::item{padding:10px;border-bottom:1px solid #242b37;}"
        "QListWidget::item:selected{background:#005fff;} QTabWidget::pane{border:0;} QTabBar::tab{background:#202631;padding:10px;}"
        "QTabBar::tab:selected{border-bottom:2px solid #005fff;} QMenu{background:#202631;border:1px solid #39445a;padding:5px;}"
        "QMenu::item{padding:7px 24px;} QMenu::item:selected{background:#005fff;border-radius:3px;}"
        "QLineEdit,QDoubleSpinBox,QSpinBox,QTextBrowser{background:#202631;border:1px solid #39445a;border-radius:4px;padding:5px;}"
        "QCheckBox{padding:5px;} QToolTip{background:#202631;color:white;border:1px solid #005fff;}"));
    setWindowIcon(QIcon(":/resources/windows/app-icon.png"));
    buildUi();
    engine_ = new MpvEngine(this);
    buildMenu(); shortcuts();
    buildChannelMenu();
    connect(&heldFrameTimer_,&QTimer::timeout,this,[this]{
        heldFrameTimer_.setInterval(40);
        if(heldFrameKey_&&!engine_->stepping())engine_->step(heldFrameKey_==Qt::Key_Left?-1:1);
    });
    connect(engine_, &MpvEngine::changed, this, &MainWindow::refresh);
    connect(engine_, &MpvEngine::loaded, this, &MainWindow::onLoaded);
    connect(engine_, &MpvEngine::error, this, [this](const QString& message){ toast(message); });
    connect(engine_, &MpvEngine::ended, this, [this]{ saveCurrent(); if (index_+1 < queue_.size()) openIndex(index_+1); });
    connect(engine_, &MpvEngine::framePresented, this, [this](double pts){ toast(QStringLiteral("当前帧  %1").arg(stamp(pts,true))); });
    connect(video_, &VideoSurface::clicked, this, [this]{ engine_->togglePause(); showControls(); });
    connect(video_, &VideoSurface::doubleClicked, this, &MainWindow::fullscreen);
    connect(video_, &VideoSurface::activity, this, &MainWindow::showControls);
    connect(video_, &VideoSurface::volumeWheel, this, [this](int step){ adjustVolume(step*5); });
    connect(video_, &VideoSurface::contextRequested, this, [this](const QPoint& point){ showControls(); menu_->exec(point); });
    connect(&persistTimer_, &QTimer::timeout, this, &MainWindow::saveCurrent); persistTimer_.start(5000);
    hideTimer_.setSingleShot(true); hideTimer_.setInterval(2200);
    connect(&hideTimer_, &QTimer::timeout, this, [this]{
        if (isFullScreen() && engine_->ready() && !engine_->flag("pause") && !QApplication::activePopupWidget() && !QApplication::activeModalWidget() && !progress_->isSliderDown() && !bottom_->underMouse()) setControlsVisible(false);
    });
    noticeTimer_.setSingleShot(true); connect(&noticeTimer_, &QTimer::timeout, this, [this]{notice_->hide();title_->show();});
    move(screen()->availableGeometry().center()-rect().center());
    for (const auto& value : settings_["queue"].toArray()) if (MpvEngine::isLocalMedia(value.toString())) queue_.append(value.toString());
    refreshLists();
    QTimer::singleShot(0, this, [this]{
        initialized_ = engine_->initialize(headless_?0:video_->winId(), !headless_&&settings_["hardware"].toBool(true), headless_);
        if (!initialized_) { toast(QStringLiteral("播放引擎初始化失败：") + engine_->failure()); return; }
        engine_->set("volume", QString::number(settings_["volume"].toInt(70)));
        engine_->set("mute", settings_["mute"].toBool() ? "yes" : "no");
        engine_->set("audio-channels",settings_["audioChannels"].toString("auto-safe"));
        if(!headless_){
            auto* route = new QTimer(this); connect(route,&QTimer::timeout,video_,&VideoSurface::routeNativeMouse); route->start(300);
        }
        const auto paths=pendingPaths_;pendingPaths_.clear();
        if(!paths.isEmpty())openPaths(paths);
        if (!library_.ready()) toast(QStringLiteral("本地记录不可用：") + library_.error());
    });
    if(!headless_)SetPropW(reinterpret_cast<HWND>(winId()),L"vision.PlayerWindow",reinterpret_cast<HANDLE>(1));
    qApp->installEventFilter(this);
}
MainWindow::~MainWindow() { if(!headless_)RemovePropW(reinterpret_cast<HWND>(winId()),L"vision.PlayerWindow"); qApp->removeEventFilter(this); delete engine_; }
void MainWindow::buildUi()
{
    setStyleSheet(styleSheet()+QStringLiteral(
        "QMainWindow{background:#191919;} QWidget#top,QWidget#bottom{background:#202020;}"
        "QWidget#top QPushButton,QWidget#bottom QPushButton{background:transparent;color:#bfc1c4;border:0;border-radius:0;padding:0;min-height:0;}"
        "QWidget#top QPushButton:hover,QWidget#bottom QPushButton:hover{background:#343434;color:white;}"
        "QWidget#top QPushButton#close:hover{background:#c42b1c;}"
        "QSlider::groove:horizontal{height:2px;background:#424242;} QSlider::handle:horizontal{width:7px;margin:-3px 0;border:1px solid #d0d0d0;background:#ddd;border-radius:4px;}"));
    auto* root=new QWidget(this);auto* layout=new QVBoxLayout(root);layout->setContentsMargins(resizeBorder,resizeBorder,resizeBorder,resizeBorder);layout->setSpacing(0);root->setObjectName("windowFrame");root->setStyleSheet("QWidget#windowFrame{background:#000;}");setCentralWidget(root);
    top_=new QWidget(root);top_->setObjectName("top");top_->setFixedHeight(30);
    auto* bar=new QHBoxLayout(top_);bar->setContentsMargins(0,0,0,0);bar->setSpacing(0);
    auto make=[this](const QString& text,const QString& tip,QWidget* parent,int width,auto fn){auto* b=button(text,tip,parent);b->setMinimumHeight(0);b->setFixedSize(width,30);connect(b,&QPushButton::clicked,this,fn);return b;};
    auto* brand=make("vision ⌄",QStringLiteral("菜单"),top_,78,[this]{menu_->exec(top_->mapToGlobal(QPoint(0,30)));});bar->addWidget(brand);brand->setProperty("hideBelow",280);
    title_=new QLabel(QStringLiteral("本地视频播放器"),top_);title_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);title_->setContentsMargins(8,0,0,0);bar->addWidget(title_,1);
    notice_=new QLabel(top_);notice_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);notice_->setStyleSheet("color:#75a6ff;padding-left:8px;");bar->addWidget(notice_,1);notice_->hide();
    pinButton_=make({},"Ctrl+T",top_,28,[this]{pin();});pinButton_->setCheckable(true);pinButton_->setProperty("hideBelow",240);bar->addWidget(pinButton_);updatePinIcon();
    bar->addWidget(make("—",QStringLiteral("最小化"),top_,28,[this]{showMinimized();}));
    bar->addWidget(make("□",QStringLiteral("最大化 / 还原"),top_,28,[this]{if(isMaximized())showNormal();else showMaximized();}));
    auto* full=make("⛶",QStringLiteral("全屏 F"),top_,28,[this]{fullscreen();});full->setProperty("hideBelow",240);bar->addWidget(full);
    auto* close=make("×",QStringLiteral("关闭"),top_,28,[this]{this->close();});close->setObjectName("close");bar->addWidget(close);layout->addWidget(top_);
    auto* body=new QWidget(root);auto* middle=new QHBoxLayout(body);middle->setContentsMargins(0,0,0,0);middle->setSpacing(0);
    video_=new VideoSurface(body);video_->setObjectName("videoSurface");middle->addWidget(video_,1);
    sidebar_=new QWidget(body);sidebar_->setFixedWidth(280);auto* side=new QVBoxLayout(sidebar_);side->setContentsMargins(0,0,0,0);
    auto* search=new QLineEdit(sidebar_);search->setObjectName("listSearch");search->setPlaceholderText(QStringLiteral("搜索列表 / 历史 / 收藏"));search->setClearButtonEnabled(true);side->addWidget(search);
    auto* tabs=new QTabWidget(sidebar_);queueList_=new QListWidget(tabs);historyList_=new QListWidget(tabs);favoriteList_=new QListWidget(tabs);
    tabs->addTab(queueList_,QStringLiteral("播放列表"));tabs->addTab(historyList_,QStringLiteral("历史"));tabs->addTab(favoriteList_,QStringLiteral("收藏"));side->addWidget(tabs);
    connect(search,&QLineEdit::textChanged,this,[this](const QString& text){for(auto* list:{queueList_,historyList_,favoriteList_})for(int i=0;i<list->count();++i)list->item(i)->setHidden(!list->item(i)->text().contains(text,Qt::CaseInsensitive));});
    auto* remove=button(QStringLiteral("从列表移除选中项"),QStringLiteral("不删除视频文件"),sidebar_);side->addWidget(remove);
    connect(remove,&QPushButton::clicked,this,[this,tabs]{if(tabs->currentWidget()==queueList_){int row=queueList_->currentRow();if(row>=0){queue_.removeAt(row);if(index_>=row)--index_;refreshLists();saveSettings();}}else if(tabs->currentWidget()==favoriteList_&&favoriteList_->currentItem()){library_.toggleFavorite(favoriteList_->currentItem()->data(Qt::UserRole).toString());refreshLists();}});
    connect(queueList_,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem* item){openIndex(queueList_->row(item));});
    for(auto* list:{historyList_,favoriteList_})connect(list,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem* item){openPaths({item->data(Qt::UserRole).toString()});});
    middle->addWidget(sidebar_);sidebar_->hide();layout->addWidget(body,1);
    bottom_=new QWidget(root);bottom_->setObjectName("bottom");bottom_->setFixedHeight(60);auto* lower=new QVBoxLayout(bottom_);lower->setContentsMargins(0,0,0,0);lower->setSpacing(0);
    auto* sliders=new QHBoxLayout;sliders->setContentsMargins(7,0,7,0);sliders->setSpacing(6);
    progress_=new SeekSlider(bottom_);progress_->setRange(0,100000);progress_->setFocusPolicy(Qt::NoFocus);progress_->setFixedHeight(20);sliders->addWidget(progress_,1);
    connect(progress_,&QSlider::sliderReleased,this,[this]{engine_->seek(engine_->number("duration")*progress_->value()/100000.0,true);});
    mute_=make("◖",QStringLiteral("静音 M"),bottom_,20,[this]{engine_->set("mute",engine_->flag("mute")?"no":"yes");});mute_->setFixedHeight(20);sliders->addWidget(mute_);
    volume_=new QSlider(Qt::Horizontal,bottom_);volume_->setRange(0,100);volume_->setValue(settings_["volume"].toInt(70));volume_->setFixedSize(65,20);volume_->setProperty("hideBelow",240);volume_->setFocusPolicy(Qt::NoFocus);sliders->addWidget(volume_);connect(volume_,&QSlider::valueChanged,this,[this](int v){engine_->set("volume",QString::number(v));});lower->addLayout(sliders);
    auto* row=new QHBoxLayout;row->setContentsMargins(0,0,0,0);row->setSpacing(0);
    auto control=[&](const QString& text,const QString& tip,int below,auto fn){auto* b=make(text,tip,bottom_,40,fn);b->setFixedHeight(40);b->setProperty("hideBelow",below);row->addWidget(b);const QMap<QString,QString> icons={{"▶","play"},{"■","stop"},{"|◀","previous"},{"▶|","next"},{"⏏","open"}};if(icons.contains(text))setTransportIcon(b,icons[text]);return b;};
    play_=control("▶",QStringLiteral("播放 / 暂停 空格"),0,[this]{engine_->togglePause();});
    control("■",QStringLiteral("停止"),160,[this]{stopPlayback();});
    control("|◀",QStringLiteral("上一个视频"),440,[this]{openIndex(index_-1);});
    control("▶|",QStringLiteral("下一个视频"),440,[this]{openIndex(index_+1);});
    control("⏏",QStringLiteral("打开视频 Ctrl+O"),480,[this]{openDialog();});
    time_=new QLabel("00:00 / 00:00",bottom_);time_->setContentsMargins(8,0,0,0);time_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);row->addWidget(time_,1);
    codecInfo_=control("S/W",QStringLiteral("播放信息"),560,[this]{showInfo();});
    channelInfo_=control(QStringLiteral("声道"),QStringLiteral("输出声道"),700,[this]{channelMenu_->exec(channelInfo_->mapToGlobal(QPoint(0,0)));});
    control("☷",QStringLiteral("列表"),320,[this]{toggleList();});control("⚙",QStringLiteral("设置"),400,[this]{settingsDialog();});
    control("☰",QStringLiteral("菜单"),0,[this]{menu_->exec(bottom_->mapToGlobal(QPoint(qMax(0,bottom_->width()-200),0)));});lower->addLayout(row);layout->addWidget(bottom_);
    // Less frequent controls remain in the menu, without a second toolbar row.
    favorite_=new QPushButton(root);favorite_->hide();
    speed_=new QComboBox(root);for(double value:{0.25,0.5,0.75,1.0,1.25,1.5,2.0,3.0,4.0})speed_->addItem(QString::number(value)+"×",value);speed_->hide();
    details_=new QLabel(root);details_->hide();
    const QString flat="QPushButton{background:transparent;border:0;border-right:1px solid #080808;border-radius:0;padding:0;color:#bfc1c4;} QPushButton:hover{background:#343434;color:white;} QPushButton#close:hover{background:#c42b1c;}";
    setMouseTracking(true);for(auto* widget:findChildren<QWidget*>())widget->setMouseTracking(true);
    top_->setStyleSheet(flat);bottom_->setStyleSheet(flat);top_->setAttribute(Qt::WA_StyledBackground);bottom_->setAttribute(Qt::WA_StyledBackground);
    updateChrome();
}
void MainWindow::buildMenu()
{
    menu_=new QMenu(this);menu_->addAction(QStringLiteral("打开文件…  Ctrl+O"),this,&MainWindow::openDialog);menu_->addAction(QStringLiteral("打开文件夹…"),this,&MainWindow::openDirectory);
    recentMenu_=menu_->addMenu(QStringLiteral("最近播放"));connect(recentMenu_,&QMenu::aboutToShow,this,[this]{recentMenu_->clear();auto rows=library_.entries();for(int i=0;i<qMin(15,int(rows.size()));++i){QString path=rows[i]["path"].toString();recentMenu_->addAction(QFileInfo(path).fileName(),this,[this,path]{openPaths({path});});}});
    menu_->addSeparator();menu_->addAction(QStringLiteral("播放 / 暂停  空格"),this,[this]{engine_->togglePause();});
    menu_->addAction(QStringLiteral("停止"),this,&MainWindow::stopPlayback);
    menu_->addAction(QStringLiteral("上一个视频"),this,[this]{openIndex(index_-1);});
    menu_->addAction(QStringLiteral("下一个视频"),this,[this]{openIndex(index_+1);});
    menu_->addAction(QStringLiteral("上一帧  ←"),this,[this]{engine_->step(-1);});menu_->addAction(QStringLiteral("下一帧  →"),this,[this]{engine_->step(1);});
    menu_->addAction(QStringLiteral("快退 30 秒  Ctrl+←"),this,[this]{engine_->seek(-30);});menu_->addAction(QStringLiteral("快进 30 秒  Ctrl+→"),this,[this]{engine_->seek(30);});
    audioMenu_=menu_->addMenu(QStringLiteral("音轨"));subtitleMenu_=menu_->addMenu(QStringLiteral("字幕"));connect(audioMenu_,&QMenu::aboutToShow,this,&MainWindow::updateTrackMenus);connect(subtitleMenu_,&QMenu::aboutToShow,this,&MainWindow::updateTrackMenus);
    menu_->addAction(QStringLiteral("字幕与音画同步…"),this,&MainWindow::syncDialog);
    menu_->addAction(QStringLiteral("原尺寸（不超出屏幕）"),this,[this]{engine_->originalSize();pendingFit_=true;fitCurrentVideo();});
    auto* rates=menu_->addMenu(QStringLiteral("播放速度"));
    for(int i=0;i<speed_->count();++i){const double rate=speed_->itemData(i).toDouble();rates->addAction(speed_->itemText(i),this,[this,rate]{engine_->set("speed",QString::number(rate));});}
    menu_->addAction(QStringLiteral("收藏 / 取消收藏"),this,[this]{if(engine_->ready()){library_.toggleFavorite(engine_->path());refreshLists();}});
    auto* ratio=menu_->addMenu(QStringLiteral("画面比例"));
    for(const auto& item : QList<QPair<QString,QString>>{{QStringLiteral("原始比例"),"-1"},{"16:9","16:9"},{"4:3","4:3"},{"2.35:1","2.35:1"}}) ratio->addAction(item.first,this,[this,item]{engine_->set("video-unscaled","no");engine_->set("video-aspect-override",item.second);engine_->set("panscan","0");});
    ratio->addAction(QStringLiteral("裁切填满"),this,[this]{engine_->set("video-unscaled","no");engine_->set("video-aspect-override","-1");engine_->set("panscan","1");});
    ratio->addAction(QStringLiteral("拉伸至窗口"),this,[this]{engine_->set("video-unscaled","no");engine_->set("video-aspect-override",QString::number(double(video_->width())/qMax(1,video_->height())));});
    menu_->addAction(QStringLiteral("截图…  Ctrl+S"),this,&MainWindow::screenshot);
    auto* window=menu_->addMenu(QStringLiteral("窗口"));window->addAction(QStringLiteral("全屏  F"),this,&MainWindow::fullscreen);window->addAction(QStringLiteral("置顶  Ctrl+T"),this,&MainWindow::pin);window->addAction(QStringLiteral("小窗 / 还原"),this,&MainWindow::compact);window->addAction(QStringLiteral("无边框 / 还原"),this,&MainWindow::borderless);
    auto* screens=window->addMenu(QStringLiteral("在显示器上全屏"));connect(screens,&QMenu::aboutToShow,this,[this,screens]{screens->clear();for(auto* screen:QGuiApplication::screens()){screens->addAction(screen->name(),this,[this,screen]{if(isFullScreen())fullscreen();move(screen->availableGeometry().topLeft()+QPoint(30,30));fullscreen();});}});
    menu_->addSeparator();menu_->addAction(QStringLiteral("播放信息"),this,&MainWindow::showInfo);menu_->addAction(QStringLiteral("设置…"),this,&MainWindow::settingsDialog);
    menu_->addAction(QStringLiteral("关于 vision"),this,[this]{QMessageBox::about(this,QString("vision v%1").arg(kVersion),QStringLiteral("vision v%1\n本地离线视频播放器\nC++ / Qt / libmpv\n点缀色 #005fff\n第三方许可与来源位于安装目录 licenses。").arg(kVersion));});
}
void MainWindow::shortcuts()
{
    auto bind=[this](const QKeySequence& keys, auto callback){auto* shortcut=new QShortcut(keys,this);shortcut->setAutoRepeat(false);connect(shortcut,&QShortcut::activated,this,[this,callback]{
        auto* focus=QApplication::focusWidget();if(qobject_cast<QLineEdit*>(focus)||qobject_cast<QAbstractSpinBox*>(focus)||qobject_cast<QListWidget*>(focus))return;callback();showControls();});};
    bind(Qt::Key_Space,[this]{engine_->togglePause();});
    bind(QKeySequence("Ctrl+Left"),[this]{engine_->seek(-30);});bind(QKeySequence("Ctrl+Right"),[this]{engine_->seek(30);});bind(Qt::Key_F,[this]{fullscreen();});bind(Qt::Key_Escape,[this]{if(isFullScreen())fullscreen();});
    bind(Qt::Key_M,[this]{engine_->set("mute",engine_->flag("mute")?"no":"yes");});bind(Qt::Key_Up,[this]{adjustVolume(5);});bind(Qt::Key_Down,[this]{adjustVolume(-5);});
    bind(QKeySequence("Ctrl+O"),[this]{openDialog();});bind(QKeySequence("Ctrl+Shift+O"),[this]{subtitleDialog();});bind(QKeySequence("Ctrl+S"),[this]{screenshot();});bind(QKeySequence("Ctrl+T"),[this]{pin();});
}
void MainWindow::openDialog(){const auto paths=QFileDialog::getOpenFileNames(this,QStringLiteral("打开视频"),settings_["lastFolder"].toString(),"Video (*.mp4 *.mkv *.avi *.mov *.webm *.flv)");if(!paths.isEmpty())openPaths(paths);}
void MainWindow::openDirectory(){QString path=QFileDialog::getExistingDirectory(this,QStringLiteral("打开文件夹"),settings_["lastFolder"].toString());if(path.isEmpty())return;QStringList paths;for(const auto& f:QDir(path).entryInfoList(QDir::Files,QDir::Name|QDir::IgnoreCase))if(MpvEngine::isLocalMedia(f.absoluteFilePath()))paths.append(f.absoluteFilePath());openPaths(paths);}
void MainWindow::openPaths(const QStringList& paths)
{
    if(paths.isEmpty())return;
    if(!initialized_){pendingPaths_+=paths;return;}
    QStringList accepted;for(const auto& path:paths)if(MpvEngine::isLocalMedia(QFileInfo(path).absoluteFilePath()))accepted.append(QFileInfo(path).absoluteFilePath());
    if(accepted.isEmpty()){toast(QStringLiteral("未找到可播放的本地视频"));return;}
    saveCurrent();queue_=accepted;index_=-1;openIndex(0);refreshLists();
}
void MainWindow::openIndex(int index)
{
    if(index<0||index>=queue_.size())return;saveCurrent();
    restore_=settings_["remember"].toBool(true)?library_.state(queue_[index]):QJsonObject{};externalSubtitle_.clear();
    index_=index;if(engine_->open(queue_[index])){title_->setText(QFileInfo(queue_[index]).suffix().toUpper()+"  |  "+QFileInfo(queue_[index]).fileName());title_->setToolTip(queue_[index]);settings_["lastFolder"]=QFileInfo(queue_[index]).absolutePath();video_->setFocus();toast(QStringLiteral("正在打开…"));}
}
void MainWindow::onLoaded()
{
    engine_->originalSize();pendingFit_=true;fitCurrentVideo();
    engine_->set("sub-delay","0");engine_->set("audio-delay","0");engine_->set("speed","1");
    const double duration=engine_->number("duration"), position=restore_["position"].toDouble();
    if(settings_["resume"].toBool(true) && position>0 && duration-position>qMin(30.0,duration*0.05))engine_->seek(position,true);
    for(const auto& key:QStringList{"speed","sub-delay","audio-delay"})if(restore_.contains(key))engine_->set(key,restore_[key].toString());
    for(const auto& key:QStringList{"aid","sid"})if(restore_.contains(key)){
        const auto wanted=restore_[key].toString();bool valid=wanted=="no";for(const auto& track:engine_->tracks())if(track["type"]==(key=="aid"?"audio":"sub")&&track["id"].toString()==wanted)valid=true;if(valid)engine_->set(key,wanted);
    }
    const auto subtitle=restore_["subtitle"].toString();if(QFileInfo(subtitle).isFile())loadSubtitle(subtitle);
    engine_->set("pause","no");restore_={};refreshLists();showControls();
    const auto path=engine_->path();QTimer::singleShot(500,this,[this,path]{if(engine_->ready()&&engine_->path()==path&&!engine_->flag("seeking")){saveCurrent();refreshLists();}});
}
void MainWindow::refresh()
{
    if(!engine_->ready())return;
    if(pendingFit_)fitCurrentVideo();
    const QString decoder=engine_->text("hwdec-current");
    codecInfo_->setText(decoder.isEmpty()||decoder=="no"?"S/W":"H/W");codecInfo_->setToolTip(engine_->text("video-format").toUpper()+" / "+engine_->text("audio-codec-name").toUpper()+QStringLiteral(" · 点击查看播放信息"));
    const QString channels=engine_->text("audio-out-params/hr-channels");
    channelInfo_->setText(channels.isEmpty()?QStringLiteral("声道"):channels);
    channelInfo_->setToolTip(QStringLiteral("输入：%1\n实际输出：%2\n点击选择输出声道；最终布局受系统与设备影响").arg(engine_->text("audio-params/hr-channels"),channels));
    const double duration=engine_->number("duration"),position=engine_->number("video-pts",engine_->number("time-pos"));
    static_cast<SeekSlider*>(progress_)->duration=duration;
    time_->setText(stamp(position).remove(0,duration<3600?3:0)+" / "+stamp(duration).remove(0,duration<3600?3:0));time_->setToolTip(stamp(position,true)+" / "+stamp(duration));
    if(!progress_->isSliderDown())progress_->setValue(duration>0?qBound(0,int(position/duration*100000),100000):0);
    progress_->setEnabled(duration>0);setTransportIcon(play_,engine_->flag("pause")?"play":"pause");
    {QSignalBlocker blocker(volume_);volume_->setValue(int(engine_->number("volume",70)));}
    mute_->setText(engine_->flag("mute")?"×":"◖");mute_->setToolTip(QStringLiteral("音量 %1% · M 静音").arg(volume_->value()));
    const double rate=engine_->number("speed",1);for(int i=0;i<speed_->count();++i)if(std::abs(speed_->itemData(i).toDouble()-rate)<0.001)speed_->setCurrentIndex(i);
    const auto hw=engine_->text("hwdec-current");const double fps=engine_->number("container-fps");
    details_->setText(QString("%1×%2  ·  %3 FPS（标称）  ·  %4%5").arg(int(engine_->number("width"))).arg(int(engine_->number("height"))).arg(fps>0?QString::number(fps,'f',3):QStringLiteral("未知")).arg(hw.isEmpty()||hw=="no"?QStringLiteral("软件解码"):hw).arg(engine_->stepping()?QStringLiteral("  ·  逐帧定位中"):QString()));
    const bool paused=engine_->flag("pause");if(paused&&!lastPaused_){saveCurrent();refreshLists();}lastPaused_=paused;
    if(engine_->flag("pause")&&!controlsVisible_)showControls();
}
void MainWindow::saveCurrent()
{
    if(!engine_||!engine_->ready()||!settings_["remember"].toBool(true))return;
    QJsonObject state;for(const auto& key:QStringList{"aid","sid","speed","sub-delay","audio-delay"})state[key]=engine_->text(key.toUtf8().constData());state["subtitle"]=externalSubtitle_;
    if(!library_.save(engine_->path(),engine_->number("video-pts",engine_->number("time-pos")),engine_->number("duration"),state))toast(QStringLiteral("播放记录保存失败"));
}
void MainWindow::saveSettings()
{
    settings_["geometry"]=QString::fromLatin1((isFullScreen()?normalGeometry_:saveGeometry()).toBase64());
    if(engine_){settings_["volume"]=int(engine_->number("volume",70));settings_["mute"]=engine_->flag("mute");}
    QJsonArray queue;for(const auto& path:queue_)queue.append(path);settings_["queue"]=queue;
    if(!library_.saveSettings(settings_))toast(QStringLiteral("设置保存失败"));
}
void MainWindow::refreshLists()
{
    if(engine_&&engine_->ready())favorite_->setText(library_.favorite(engine_->path())?"★":"☆");
    queueList_->clear();for(const auto& path:queue_){auto* item=new QListWidgetItem(QFileInfo(path).fileName(),queueList_);item->setToolTip(path);}if(index_>=0)queueList_->setCurrentRow(index_);
    for(auto pair:QList<QPair<QListWidget*,bool>>{{historyList_,false},{favoriteList_,true}}){pair.first->clear();for(const auto& row:library_.entries(pair.second)){const auto path=row["path"].toString();auto* item=new QListWidgetItem(QFileInfo(path).fileName()+"\n"+stamp(row["position"].toDouble()),pair.first);item->setData(Qt::UserRole,path);item->setToolTip(path);}}
    if(auto* search=sidebar_->findChild<QLineEdit*>("listSearch"))for(auto* list:{queueList_,historyList_,favoriteList_})for(int i=0;i<list->count();++i)list->item(i)->setHidden(!list->item(i)->text().contains(search->text(),Qt::CaseInsensitive));
}
void MainWindow::stopPlayback()
{
    if(!engine_->ready())return;
    engine_->set("pause","yes");engine_->seek(0,true);toast(QStringLiteral("已停止，回到视频开头"));
}
void MainWindow::updateChrome(int widthHint)
{
    if(!top_||!bottom_)return;
    for(auto* parent:{top_,bottom_})for(auto* widget:parent->findChildren<QWidget*>()){
        const int threshold=widget->property("hideBelow").toInt();
        if(threshold>0)widget->setVisible((widthHint<0?width():widthHint)>=threshold);
    }
}
void MainWindow::fitCurrentVideo()
{
    const QSize pixels=engine_->displaySize();if(pixels.isEmpty())return;
    pendingFit_=false;
    if(sidebar_->isVisible())toggleList();
    if(isFullScreen())fullscreen();if(isMaximized())showNormal();
    top_->show();bottom_->setFixedHeight(60);controlsVisible_=true;compact_=false;
    const QRect available=screen()->availableGeometry().adjusted(4,4,-4,-4);
    const QSize frame=frameGeometry().size()-size();
    const QSize target=fittedVideoWindow(pixels,devicePixelRatioF(),available.size()-frame-QSize(2*resizeBorder,2*resizeBorder),90)+QSize(2*resizeBorder,2*resizeBorder);
    updateChrome(target.width());centralWidget()->layout()->activate();layout()->activate();
    resize(target);move(containedPosition(pos(),target+frame,available));
    updateChrome();centralWidget()->layout()->activate();
}
void MainWindow::buildChannelMenu()
{
    channelMenu_=menu_->addMenu(QStringLiteral("输出声道"));
    auto* group=new QActionGroup(channelMenu_);group->setExclusive(true);
    const QList<QPair<QString,QString>> layouts={
        {QStringLiteral("系统推荐（默认）"),"auto-safe"},
        {QStringLiteral("1.0 单声道"),"mono"},{QStringLiteral("2.0 立体声"),"stereo"},
        {QStringLiteral("3.0 前置三声道"),"fl-fr-fc"},{QStringLiteral("3.0 环绕"),"fl-fr-bc"},
        {QStringLiteral("4.0 环绕"),"fl-fr-fc-bc"},{QStringLiteral("4.0 四声道"),"fl-fr-bl-br"},
        {QStringLiteral("5.0 声道"),"fl-fr-fc-bl-br"},{QStringLiteral("6.0 声道"),"fl-fr-fc-bc-sl-sr"},
        {QStringLiteral("7.0 声道"),"fl-fr-fc-bl-br-sl-sr"},
        {QStringLiteral("1.1 单声道 + LFE"),"fc-lfe"},{QStringLiteral("2.1 立体声 + LFE"),"fl-fr-lfe"},
        {QStringLiteral("3.1 前置三声道 + LFE"),"fl-fr-fc-lfe"},{QStringLiteral("3.1 环绕 + LFE"),"fl-fr-bc-lfe"},
        {QStringLiteral("4.1 环绕 + LFE"),"fl-fr-fc-bc-lfe"},{QStringLiteral("4.1 四声道 + LFE"),"fl-fr-bl-br-lfe"},
        {QStringLiteral("5.1 声道"),"5.1"},{QStringLiteral("6.1 声道"),"6.1"},{QStringLiteral("7.1 声道"),"7.1"},
        {QStringLiteral("源（输入）作为输出"),"auto"}};
    for(const auto& item:layouts){auto* action=channelMenu_->addAction(item.first);action->setCheckable(true);action->setData(item.second);group->addAction(action);
        connect(action,&QAction::triggered,this,[this,item]{engine_->set("audio-channels",item.second);settings_["audioChannels"]=item.second;saveSettings();toast(QStringLiteral("请求输出：%1；实际输出见声道按钮").arg(item.first));});}
    connect(channelMenu_,&QMenu::aboutToShow,this,[this,group]{const QString chosen=settings_["audioChannels"].toString("auto-safe");for(auto* action:group->actions())action->setChecked(action->data().toString()==chosen);});
}
void MainWindow::updateTrackMenus()
{
    audioMenu_->clear();subtitleMenu_->clear();subtitleMenu_->addAction(QStringLiteral("加载字幕…"),this,&MainWindow::subtitleDialog);
    auto* off=subtitleMenu_->addAction(QStringLiteral("关闭字幕"),this,[this]{engine_->set("sid","no");});off->setCheckable(true);off->setChecked(engine_->text("sid")=="no");subtitleMenu_->addSeparator();
    for(const auto& track:engine_->tracks()){
        QString type=track["type"].toString();if(type!="audio"&&type!="sub")continue;
        QString label=QString("#%1  %2  %3  %4").arg(track["id"].toString(),track["lang"].toString(),track["title"].toString(),track["codec"].toString());
        auto* menu=type=="audio"?audioMenu_:subtitleMenu_;auto* action=menu->addAction(label,this,[this,type,track]{engine_->set(type=="audio"?"aid":"sid",track["id"].toString());});action->setCheckable(true);action->setChecked(track["selected"].toBool());
    }
}
void MainWindow::subtitleDialog(){QString path=QFileDialog::getOpenFileName(this,QStringLiteral("加载字幕"),settings_["lastFolder"].toString(),"Subtitles (*.srt *.ass *.ssa *.vtt *.sub)");if(!path.isEmpty())loadSubtitle(path);}
void MainWindow::loadSubtitle(const QString& path)
{
    const QFileInfo file(path);if(!engine_->ready()||!file.isFile()||QDir::fromNativeSeparators(path).startsWith("//"))return;
    externalSubtitle_=file.absoluteFilePath();engine_->command({"sub-add",externalSubtitle_,"select"});toast(QStringLiteral("已加载字幕"));
}
void MainWindow::screenshot()
{
    if(!engine_->ready())return;QString dir=settings_["screenshots"].toString(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)+"/vision");QDir().mkpath(dir);
    QString suggested=dir+"/"+QFileInfo(engine_->path()).completeBaseName()+"-"+QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz")+".png";
    QString path=QFileDialog::getSaveFileName(this,QStringLiteral("保存当前帧"),suggested,"PNG (*.png)");if(path.isEmpty())return;
    engine_->command({"screenshot-to-file",path,settings_["screenshotSubtitles"].toBool(true)?"subtitles":"video"});
    QTimer::singleShot(700,this,[this,path]{toast(QFileInfo(path).size()>0?QStringLiteral("截图已保存"):QStringLiteral("截图处理中，失败时请查看提示"));});
}
void MainWindow::syncDialog()
{
    QDialog dialog(this);dialog.setWindowTitle(QStringLiteral("字幕与音画同步"));auto* form=new QFormLayout(&dialog);
    auto* sub=new QDoubleSpinBox(&dialog);sub->setRange(-600,600);sub->setDecimals(3);sub->setSingleStep(0.1);sub->setValue(engine_->number("sub-delay"));
    auto* audio=new QDoubleSpinBox(&dialog);audio->setRange(-60,60);audio->setDecimals(3);audio->setSingleStep(0.05);audio->setValue(engine_->number("audio-delay"));
    form->addRow(QStringLiteral("字幕延后（秒；负数提前）"),sub);form->addRow(QStringLiteral("声音延后（秒；负数提前）"),audio);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel|QDialogButtonBox::Reset,&dialog);form->addRow(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);connect(buttons->button(QDialogButtonBox::Reset),&QPushButton::clicked,&dialog,[sub,audio]{sub->setValue(0);audio->setValue(0);});
    if(dialog.exec()==QDialog::Accepted){engine_->set("sub-delay",QString::number(sub->value(),'f',3));engine_->set("audio-delay",QString::number(audio->value(),'f',3));}
}
void MainWindow::settingsDialog()
{
    QDialog dialog(this);dialog.setWindowTitle(QStringLiteral("vision 设置"));dialog.resize(470,360);auto* form=new QFormLayout(&dialog);
    auto* hardware=new QCheckBox(QStringLiteral("GPU 硬件解码（失败时由引擎回退）"),&dialog);hardware->setChecked(settings_["hardware"].toBool(true));form->addRow(hardware);
    auto* remember=new QCheckBox(QStringLiteral("保存播放历史与位置"),&dialog);remember->setChecked(settings_["remember"].toBool(true));form->addRow(remember);
    auto* resume=new QCheckBox(QStringLiteral("打开时自动继续上次位置"),&dialog);resume->setChecked(settings_["resume"].toBool(true));form->addRow(resume);
    auto* subtitles=new QCheckBox(QStringLiteral("截图包含字幕"),&dialog);subtitles->setChecked(settings_["screenshotSubtitles"].toBool(true));form->addRow(subtitles);
    auto* clear=button(QStringLiteral("清空历史（保留收藏和视频）"),{},&dialog);form->addRow(clear);connect(clear,&QPushButton::clicked,&dialog,[this,&dialog]{if(QMessageBox::question(&dialog,QStringLiteral("清空历史"),QStringLiteral("清空播放记录？视频和收藏保留。"))==QMessageBox::Yes){if(!library_.clearHistory())toast(QStringLiteral("清空历史失败"));refreshLists();}});
    auto* shortcutsLabel=new QLabel(QStringLiteral("← / → 上一帧 / 下一帧\nCtrl+← / → ±30秒 · 空格 播放暂停\nF 全屏 · M 静音 · ↑ / ↓ 音量\nCtrl+Shift+O 字幕 · Ctrl+S 截图\n\n数据：%1").arg(library_.root()),&dialog);shortcutsLabel->setWordWrap(true);form->addRow(shortcutsLabel);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted){settings_["hardware"]=hardware->isChecked();settings_["remember"]=remember->isChecked();settings_["resume"]=resume->isChecked();settings_["screenshotSubtitles"]=subtitles->isChecked();engine_->set("hwdec",hardware->isChecked()?"auto-safe":"no");saveSettings();}
}
void MainWindow::showInfo()
{
    QString info=QStringLiteral("%1\n\n引擎：%2\n视频编码：%3\n尺寸：%4 × %5\n容器声明帧率：%6 FPS\n短时估测帧率：%7 FPS\n当前帧展示时间：%8\n解码：%9\n丢帧统计：%10\n\n帧率读数不作为逐帧步长。VFR 按实际帧展示顺序查看；当前不伪造全片精确帧号。").arg(engine_->path(),engine_->text("mpv-version"),engine_->text("video-codec"),engine_->text("width"),engine_->text("height"),engine_->text("container-fps"),engine_->text("estimated-vf-fps"),stamp(engine_->number("video-pts",engine_->number("time-pos")),true),engine_->text("hwdec-current"),engine_->text("frame-drop-count"));
    QMessageBox::information(this,QStringLiteral("播放信息"),info);
}
void MainWindow::toast(const QString& message){notice_->setText(message);notice_->setToolTip(message);title_->hide();notice_->show();noticeTimer_.start(3500);if(!controlsVisible_)showControls();}
void MainWindow::updatePinIcon()
{
    const qreal scale=devicePixelRatioF();
    QPixmap pixmap(qRound(20*scale),qRound(20*scale));pixmap.setDevicePixelRatio(scale);pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(10,10);if(!pinned_)painter.rotate(90);
    painter.setPen(Qt::NoPen);painter.setBrush(QColor(pinned_?"#ffe600":"#858585"));
    painter.drawPolygon(QPolygonF{{-5,-7},{5,-7},{5,-5},{3,-4},{3,1},{6,4},{1,4},{0,9},{-1,4},{-6,4},{-3,1},{-3,-4},{-5,-5}});
    painter.end();pinButton_->setIcon(QIcon(pixmap));pinButton_->setIconSize(QSize(20,20));
    pinButton_->setChecked(pinned_);pinButton_->setToolTip(pinned_?QStringLiteral("取消置顶  Ctrl+T"):QStringLiteral("置顶  Ctrl+T"));
    pinButton_->setAccessibleName(pinned_?QStringLiteral("取消置顶"):QStringLiteral("置顶"));
}
void MainWindow::positionFloatingList()
{
    if(!sidebar_->isWindow()||!sidebar_->isVisible())return;
    const QRect available=screen()->availableGeometry();
    sidebar_->resize(sidebar_->width(),qMin(height(),available.height()));
    sidebar_->move(qBound(available.left(),frameGeometry().right()+1,available.right()-sidebar_->width()+1),
                   qBound(available.top(),frameGeometry().top(),available.bottom()-sidebar_->height()+1));
}
void MainWindow::toggleList()
{
    auto* layout=qobject_cast<QHBoxLayout*>(video_->parentWidget()->layout());
    if(sidebar_->isVisible()){
        const bool floating=sidebar_->isWindow();sidebar_->hide();
        if(!floating)resize(width()-sidebar_->width(),height());
        return;
    }
    const QRect available=screen()->availableGeometry();
    const bool fits=!isMaximized()&&!isFullScreen()&&frameGeometry().width()+sidebar_->width()<=available.width();
    if(fits){
        if(sidebar_->isWindow()){sidebar_->setParent(video_->parentWidget());layout->addWidget(sidebar_);}
        const QSize expanded(width()+sidebar_->width(),height());
        sidebar_->show();resize(expanded);
        if(frameGeometry().right()>available.right())move(pos()+QPoint(available.right()-frameGeometry().right(),0));
    }else{
        // When the screen cannot contain both, keep the video viewport unchanged.
        if(!sidebar_->isWindow()){layout->removeWidget(sidebar_);sidebar_->setParent(this,Qt::Tool);sidebar_->setWindowTitle(QStringLiteral("vision · 列表"));}
        sidebar_->show();positionFloatingList();
    }
}
void MainWindow::adjustVolume(int delta){engine_->set("volume",QString::number(qBound(0,int(engine_->number("volume",70))+delta,100)));}
void MainWindow::showControls(){setControlsVisible(true);hideTimer_.start();video_->unsetCursor();}
void MainWindow::setControlsVisible(bool visible)
{
    if(visible==controlsVisible_)return;controlsVisible_=visible;
    bottom_->setFixedHeight(visible?60:0);if(!visible)video_->setCursor(Qt::BlankCursor);
}
void MainWindow::fullscreen()
{
    if(isFullScreen()){showNormal();pinned_=fullPinned_;updatePinIcon();SetWindowPos(reinterpret_cast<HWND>(winId()),pinned_?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);restoreGeometry(normalGeometry_);top_->setVisible(!compact_);sidebar_->setVisible(fullSidebar_);positionFloatingList();showControls();}
    else{normalGeometry_=saveGeometry();fullPinned_=pinned_;fullSidebar_=sidebar_->isVisible();sidebar_->hide();top_->hide();showFullScreen();showControls();}
    video_->setFocus();
}
void MainWindow::compact(){if(isFullScreen())fullscreen();if(compact_){compact_=false;restoreGeometry(compactGeometry_);}else{if(sidebar_->isVisible())toggleList();compactGeometry_=saveGeometry();compact_=true;const QSize pixels=engine_->displaySize();if(!pixels.isEmpty())resize(fittedVideoWindow(pixels,devicePixelRatioF(),QSize(420,420)-QSize(2*resizeBorder,2*resizeBorder),90)+QSize(2*resizeBorder,2*resizeBorder));}}
void MainWindow::borderless(){if(isFullScreen())fullscreen();frameless_=!frameless_;auto geometry=saveGeometry();setWindowFlag(Qt::FramelessWindowHint,frameless_);show();restoreGeometry(geometry);toast(frameless_?QStringLiteral("无边框：拖动顶部空白处，边缘可缩放"):QStringLiteral("已恢复窗口边框"));}
void MainWindow::pin(){pinned_=!pinned_;updatePinIcon();SetWindowPos(reinterpret_cast<HWND>(winId()),pinned_?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);toast(pinned_?QStringLiteral("已置顶"):QStringLiteral("已取消置顶"));}
void MainWindow::closeEvent(QCloseEvent* event){saveCurrent();saveSettings();QMainWindow::closeEvent(event);}
void MainWindow::dragEnterEvent(QDragEnterEvent* event){if(event->mimeData()->hasUrls())event->acceptProposedAction();}
void MainWindow::dropEvent(QDropEvent* event)
{
    QStringList paths;for(const auto& url:event->mimeData()->urls())if(url.isLocalFile())paths.append(url.toLocalFile());
    if(paths.size()==1&&QStringList{"srt","ass","ssa","vtt","sub"}.contains(QFileInfo(paths[0]).suffix().toLower()))loadSubtitle(paths[0]);else openPaths(paths);event->acceptProposedAction();
}
bool MainWindow::eventFilter(QObject* watched,QEvent* event)
{
    if(event->type()==QEvent::WindowDeactivate&&watched==this){heldFrameKey_=0;heldFrameTimer_.stop();}
    if(watched==this&&event->type()==QEvent::WinIdChange&&!headless_&&internalWinId())SetPropW(reinterpret_cast<HWND>(internalWinId()),L"vision.PlayerWindow",reinterpret_cast<HANDLE>(1));
    if(watched==resizeCursorWidget_&&event->type()==QEvent::Leave){resizeCursorWidget_->setCursor(savedResizeCursor_);resizeCursorWidget_.clear();}
    if(watched==this&&event->type()==QEvent::WindowStateChange&&centralWidget()){const int border=isFullScreen()||isMaximized()?0:resizeBorder;centralWidget()->layout()->setContentsMargins(border,border,border,border);}
    if(watched==this&&(event->type()==QEvent::Move||event->type()==QEvent::Resize)&&sidebar_){positionFloatingList();updateChrome();}
    if(auto* widget=qobject_cast<QWidget*>(watched);widget&&widget->window()==this){
        if(event->type()==QEvent::FocusOut){heldFrameKey_=0;heldFrameTimer_.stop();}
        if(event->type()==QEvent::KeyPress||event->type()==QEvent::KeyRelease){
            auto* key=static_cast<QKeyEvent*>(event);
            if(key->key()==Qt::Key_Left||key->key()==Qt::Key_Right){
                if(event->type()==QEvent::KeyRelease&&key->key()==heldFrameKey_){if(!key->isAutoRepeat()){heldFrameKey_=0;heldFrameTimer_.stop();}return true;}
                auto* focus=QApplication::focusWidget();
                if(event->type()==QEvent::KeyPress&&key->modifiers()==Qt::NoModifier&&!qobject_cast<QLineEdit*>(focus)&&!qobject_cast<QAbstractSpinBox*>(focus)&&!qobject_cast<QListWidget*>(focus)){
                    if(!key->isAutoRepeat()){heldFrameKey_=key->key();engine_->step(heldFrameKey_==Qt::Key_Left?-1:1);heldFrameTimer_.start(350);}
                    return true;
                }
            }
        }
        if(frameless_&&!isFullScreen()&&!isMaximized()&&(event->type()==QEvent::MouseMove||event->type()==QEvent::MouseButtonPress)){
            auto* mouse=static_cast<QMouseEvent*>(event);
            const auto edges=resizeEdges(mapFromGlobal(mouse->globalPosition().toPoint()),size());
            if(resizeCursorWidget_&&(resizeCursorWidget_!=widget||!edges)){resizeCursorWidget_->setCursor(savedResizeCursor_);resizeCursorWidget_.clear();}
            if(edges){if(!resizeCursorWidget_){resizeCursorWidget_=widget;savedResizeCursor_=widget->cursor();}widget->setCursor(resizeCursor(edges));
                if(event->type()==QEvent::MouseButtonPress&&mouse->button()==Qt::LeftButton&&windowHandle()){
                    if(windowHandle()->startSystemResize(edges))return true;
                }
                if(event->type()==QEvent::MouseMove)return true;
            }
        }
        if(event->type()==QEvent::MouseMove && isFullScreen())showControls();
        if(frameless_ && event->type()==QEvent::MouseButtonPress && (watched==top_||watched==title_||watched==notice_)){auto* mouse=static_cast<QMouseEvent*>(event);if(mouse->button()==Qt::LeftButton&&windowHandle())windowHandle()->startSystemMove();}
    }return QMainWindow::eventFilter(watched,event);
}
bool MainWindow::nativeEvent(const QByteArray& type,void* message,qintptr* result)
{
    auto* msg=static_cast<MSG*>(message);
    if(frameless_&&!isFullScreen()&&!isMaximized()&&msg->message==WM_NCHITTEST){RECT rect;GetWindowRect(reinterpret_cast<HWND>(winId()),&rect);int x=GET_X_LPARAM(msg->lParam),y=GET_Y_LPARAM(msg->lParam);int b=qRound(7*devicePixelRatioF());bool l=x<rect.left+b,r=x>=rect.right-b,t=y<rect.top+b,d=y>=rect.bottom-b;int hit=t?(l?HTTOPLEFT:r?HTTOPRIGHT:HTTOP):d?(l?HTBOTTOMLEFT:r?HTBOTTOMRIGHT:HTBOTTOM):l?HTLEFT:r?HTRIGHT:HTCLIENT;if(hit!=HTCLIENT){*result=hit;return true;}}
    if(msg->message==WM_MOVING&&!isFullScreen()&&!isMaximized()){
        auto* rect=reinterpret_cast<RECT*>(msg->lParam);
        struct Candidates { HWND self; QList<QRect> rectangles; } candidates{reinterpret_cast<HWND>(winId()),{}};
        EnumWindows([](HWND other,LPARAM context)->BOOL{
            auto* found=reinterpret_cast<Candidates*>(context);
            if(other!=found->self&&IsWindowVisible(other)&&!IsIconic(other)&&GetPropW(other,L"vision.PlayerWindow")){
                RECT r;if(GetWindowRect(other,&r))found->rectangles.append(QRect(r.left,r.top,r.right-r.left,r.bottom-r.top));
            }return TRUE;
        },reinterpret_cast<LPARAM>(&candidates));
        const QRect snapped=snappedWindow(QRect(rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top),candidates.rectangles,qRound(12*devicePixelRatioF()));
        rect->left=snapped.x();rect->top=snapped.y();rect->right=snapped.x()+snapped.width();rect->bottom=snapped.y()+snapped.height();*result=TRUE;return true;
    }
    if(msg->message==WM_DISPLAYCHANGE){QTimer::singleShot(300,this,[this]{bool intersects=false;for(auto* screen:QGuiApplication::screens())intersects|=screen->availableGeometry().intersects(frameGeometry());if(!intersects){if(isFullScreen())fullscreen();move(QGuiApplication::primaryScreen()->availableGeometry().topLeft()+QPoint(40,40));}});}
    return QMainWindow::nativeEvent(type,message,result);
}
}
