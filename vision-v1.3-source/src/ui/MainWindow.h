#pragma once

#include <QMainWindow>
#include <QJsonObject>
#include <QTimer>
#include <QPointer>
#include <QCursor>
#include "storage/Library.h"

class QLabel; class QSlider; class QPushButton; class QComboBox; class QListWidget;
class QTabWidget; class QMenu; class QCloseEvent; class QDragEnterEvent; class QDropEvent;

namespace vision {
class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr, const QString& dataRoot = {}, bool headless = false);
    ~MainWindow() override;
    void openPaths(const QStringList& paths);
protected:
    void closeEvent(QCloseEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;
    bool eventFilter(QObject*, QEvent*) override;
    bool nativeEvent(const QByteArray&, void*, qintptr*) override;
private:
    void buildUi(); void buildMenu(); void shortcuts(); void refresh();
    void openDialog(); void openDirectory(); void openIndex(int index);
    void onLoaded(); void saveCurrent(); void saveSettings(); void refreshLists();
    void subtitleDialog(); void loadSubtitle(const QString& path); void screenshot();
    void settingsDialog(); void syncDialog(); void showInfo(); void toast(const QString& text);
    void fullscreen(); void compact(); void borderless(); void pin(); void showControls();
    void setControlsVisible(bool visible); void adjustVolume(int delta); void updateTrackMenus();
    void toggleList(); void updatePinIcon(); void positionFloatingList();
    void stopPlayback(); void buildChannelMenu();
    void fitCurrentVideo(); void updateChrome(int widthHint = -1);
    class MpvEngine* engine_ = nullptr;
    class VideoSurface* video_ = nullptr;
    Library library_;
    QJsonObject settings_, restore_;
    QStringList queue_;
    QStringList pendingPaths_;
    QString externalSubtitle_;
    int index_ = -1;
    QWidget *top_ = nullptr, *bottom_ = nullptr, *sidebar_ = nullptr;
    QLabel *title_ = nullptr, *time_ = nullptr, *details_ = nullptr, *notice_ = nullptr;
    QSlider *progress_ = nullptr, *volume_ = nullptr;
    QPushButton *play_ = nullptr, *mute_ = nullptr, *favorite_ = nullptr;
    QPushButton* pinButton_ = nullptr;
    QPushButton *codecInfo_ = nullptr, *channelInfo_ = nullptr;
    QMenu* channelMenu_ = nullptr;
    QComboBox* speed_ = nullptr;
    QListWidget *queueList_ = nullptr, *historyList_ = nullptr, *favoriteList_ = nullptr;
    QMenu *menu_ = nullptr, *audioMenu_ = nullptr, *subtitleMenu_ = nullptr, *recentMenu_ = nullptr;
    QTimer persistTimer_, hideTimer_, noticeTimer_;
    QTimer heldFrameTimer_;
    int heldFrameKey_ = 0;
    QByteArray normalGeometry_, compactGeometry_;
    bool initialized_ = false, compact_ = false, pinned_ = false, frameless_ = true;
    bool headless_ = false, pendingFit_ = false;
    bool fullPinned_ = false, fullSidebar_ = false, controlsVisible_ = true;
    bool lastPaused_ = true;
    QPointer<QWidget> resizeCursorWidget_;
    QCursor savedResizeCursor_;
};
}
