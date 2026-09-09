#include <QApplication>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QThread>
#include <QScreen>
#include <QPushButton>
#include <QTextStream>
#include <QMenu>
#include <QFile>
#include <QDir>
#include <QFontDatabase>
#include "ui/MainWindow.h"
#include "ui/VideoSurface.h"
#include "ui/VideoGeometry.h"
#include "ui/WindowInteraction.h"
#include <QMouseEvent>
#include "app/LaunchFiles.h"
#include "player/MpvEngine.h"
#include "Version.h"

bool waitFor(const std::function<bool()>& done,int timeout=8000)
{
    QElapsedTimer timer;timer.start();
    while(timer.elapsed()<timeout){QApplication::processEvents();if(done())return true;QThread::msleep(5);}return false;
}
int main(int argc,char** argv)
{
    qputenv("QT_QPA_PLATFORM","offscreen");
    const auto files=vision::launchFiles(QString::fromWCharArray(GetCommandLineW()));
    QApplication app(argc,argv);QApplication::setStyle("Fusion");QTextStream out(stdout);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");QFontDatabase::addApplicationFont("C:/Windows/Fonts/seguisym.ttf");
    if(files.size()!=1)return 2;
    const QString example=QStringLiteral("C:\\测试 视频\\样片 中文 01.mp4");
    if(vision::launchFiles("\"C:\\Program Files\\vision\\vision.exe\" \""+example+"\"")!=QStringList{QDir::fromNativeSeparators(example)})return 3;
    for(const QSize screen:QList<QSize>{{1920,1040},{1280,680},{800,760}})
        for(double dpi:{1.0,1.25,1.5,2.0})for(const QSize source:QList<QSize>{{720,960},{1920,1080},{7680,4320},{1080,1920}}){
            const QSize fit=vision::fittedVideoWindow(source,dpi,screen,90);
            if(fit.width()>screen.width()||fit.height()>screen.height()||fit.height()<=90)return 4;
            if(std::abs(double(fit.width())/(fit.height()-90)-double(source.width())/source.height())>0.012)return 4;
        }
    QTemporaryDir data;
    const QString unicode=data.path()+QStringLiteral("/测试 视频 中文 01.mp4");
    if(!QFile::copy(files.front(),unicode))return 5;
    vision::MainWindow window(nullptr,data.path()+"/settings",true);
    // Exactly the launch order in main: queue the Explorer path before the engine exists.
    window.openPaths(vision::launchFiles("vision.exe \""+unicode+"\""));window.show();
    auto* engine=window.findChild<vision::MpvEngine*>();
    if(!waitFor([&]{return engine->ready()&&!engine->displaySize().isEmpty();})){out<<"FAIL startup "<<engine->failure()<<Qt::endl;return 6;}
    if(files.front().contains("rotated")&&engine->displaySize()!=QSize(180,320))return 11;
    engine->set("pause","yes");
    waitFor([&]{return !engine->flag("seeking");});
    if(engine->path()!=unicode||!window.windowFlags().testFlag(Qt::FramelessWindowHint)||window.windowTitle()!="vision v"+QString(vision::kVersion))return 7;
    auto* video=window.findChild<vision::VideoSurface*>();
    const QRect available=window.screen()->availableGeometry();
    out<<"version="<<window.windowTitle()<<" dpr="<<window.devicePixelRatioF()<<" window="<<window.width()<<"x"<<window.height()<<" video="<<video->width()<<"x"<<video->height()<<Qt::endl;
    if(!available.contains(window.frameGeometry())||video->width()!=window.width()-10||video->height()!=window.height()-100)return 8;
    const QSize wanted=vision::fittedVideoWindow(engine->displaySize(),window.devicePixelRatioF(),available.size()-QSize(18,18),90)+QSize(10,10);
    if(window.size()!=wanted){out<<"wanted="<<wanted.width()<<"x"<<wanted.height()<<Qt::endl;return 9;}
    if(engine->text("video-unscaled")!="no"||engine->number("panscan")!=0)return 10;
    const QSize area(400,300);
    const QList<QPair<QPoint,Qt::Edges>> hits={{{0,150},Qt::LeftEdge},{{399,150},Qt::RightEdge},{{200,0},Qt::TopEdge},{{200,299},Qt::BottomEdge},{{0,0},Qt::LeftEdge|Qt::TopEdge},{{399,0},Qt::RightEdge|Qt::TopEdge},{{0,299},Qt::LeftEdge|Qt::BottomEdge},{{399,299},Qt::RightEdge|Qt::BottomEdge}};
    for(const auto& hit:hits)if(vision::resizeEdges(hit.first,area)!=hit.second)return 12;
    if(vision::resizeEdges({200,150},area))return 12;
    const QRect other(500,100,300,400);
    if(vision::snappedWindow(QRect(192,106,300,400),{other},12)!=QRect(200,100,300,400))return 13;
    if(vision::snappedWindow(QRect(808,94,300,400),{other},12)!=QRect(800,100,300,400))return 13;
    if(vision::snappedWindow(QRect(507,508,300,400),{other},12)!=QRect(500,500,300,400))return 13;
    if(vision::snappedWindow(QRect(-808,106,300,400),{QRect(-500,100,300,400)},12)!=QRect(-800,100,300,400))return 13;
    const QRect distantRect(100,900,300,400);if(vision::snappedWindow(distantRect,{other},12)!=distantRect)return 13;
    auto* surface=window.centralWidget();
    for(const auto& hit:hits){
        QPoint point(hit.first.x()==399?window.width()-1:hit.first.x(),hit.first.y()==299?window.height()-1:hit.first.y());
        // Use real edge midpoints even for narrow native-size windows.
        if(hit.first.x()==200)point.setX(window.width()/2);if(hit.first.y()==150)point.setY(window.height()/2);
        QMouseEvent move(QEvent::MouseMove,QPointF(point),QPointF(window.mapToGlobal(point)),Qt::NoButton,Qt::NoButton,Qt::NoModifier);
        QApplication::sendEvent(surface,&move);
        if(surface->cursor().shape()!=vision::resizeCursor(hit.second)){out<<"cursor at "<<point.x()<<","<<point.y()<<" got="<<surface->cursor().shape()<<" wanted="<<vision::resizeCursor(hit.second)<<Qt::endl;return 14;}
    }
    out<<"PASS eight resize edges/cursors, adjacent window snapping, negative monitor coordinates, distant-window rejection"<<Qt::endl;
    if(files.front().endsWith("cfr.mp4")){
        video->setFocus();int presented=0;QObject::connect(engine,&vision::MpvEngine::framePresented,&app,[&](double){++presented;});
        auto pump=[&](int ms){QElapsedTimer t;t.start();waitFor([&]{return t.elapsed()>=ms;});};
        engine->seek(0,true);pump(250);
        auto key=[&](QEvent::Type type,int code){QKeyEvent event(type,code,Qt::NoModifier);QApplication::sendEvent(video,&event);};
        key(QEvent::KeyPress,Qt::Key_Right);key(QEvent::KeyRelease,Qt::Key_Right);
        if(!waitFor([&]{return presented==1;}))return 15;pump(150);if(presented!=1)return 15;
        key(QEvent::KeyPress,Qt::Key_Right);pump(700);key(QEvent::KeyRelease,Qt::Key_Right);pump(150);
        if(presented<4)return 16;const int stopped=presented;pump(200);if(presented!=stopped)return 16;
        const double position=engine->number("time-pos");
        key(QEvent::KeyPress,Qt::Key_Left);pump(550);key(QEvent::KeyRelease,Qt::Key_Left);pump(150);
        if(engine->number("time-pos")>=position||!engine->flag("pause"))return 17;
        out<<"PASS tap exactly one frame, held right/left repeat, release stops queued repeats"<<Qt::endl;
    }
    window.grab().save(QCoreApplication::applicationDirPath()+"/v1.3-ui-check.png");
    out<<"PASS Unicode Explorer launch before initialization, frameless chrome, aspect fit, screen bounds, no side gutters, version"<<Qt::endl;
    return 0;
}
