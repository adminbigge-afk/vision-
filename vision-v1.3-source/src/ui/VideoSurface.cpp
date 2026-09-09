#include "VideoSurface.h"
#include <QApplication>
#include <QStyleHints>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <windows.h>

namespace vision {
VideoSurface::VideoSurface(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_NativeWindow); setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true); setFocusPolicy(Qt::StrongFocus);
    clickTimer_.setSingleShot(true);
    connect(&clickTimer_, &QTimer::timeout, this, &VideoSurface::clicked);
}
void VideoSurface::routeNativeMouse()
{
    if(QApplication::platformName()=="offscreen")return;
    HWND parent=reinterpret_cast<HWND>(winId());
    SetWindowLongPtrW(parent,GWL_STYLE,GetWindowLongPtrW(parent,GWL_STYLE)|WS_CLIPCHILDREN|WS_CLIPSIBLINGS);
    EnumChildWindows(reinterpret_cast<HWND>(winId()), [](HWND child, LPARAM) -> BOOL {
        EnableWindow(child, FALSE); return TRUE;
    }, 0);
}
void VideoSurface::mousePressEvent(QMouseEvent* e)
{
    setFocus(); emit activity();
    if (e->button() == Qt::RightButton) emit contextRequested(e->globalPosition().toPoint());
    if (e->button() == Qt::LeftButton) clickTimer_.start(QApplication::styleHints()->mouseDoubleClickInterval());
}
void VideoSurface::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) { clickTimer_.stop(); emit doubleClicked(); }
}
void VideoSurface::mouseMoveEvent(QMouseEvent*) { emit activity(); }
void VideoSurface::wheelEvent(QWheelEvent* e) { emit activity(); emit volumeWheel(e->angleDelta().y() > 0 ? 1 : -1); e->accept(); }
void VideoSurface::paintEvent(QPaintEvent*)
{
    QPainter painter(this); painter.fillRect(rect(), QColor("#0a0c10"));
    painter.setPen(QColor("#e4e8f0")); QFont font = painter.font(); font.setPointSize(34); font.setWeight(QFont::Light); painter.setFont(font);
    painter.drawText(rect().adjusted(0,-25,0,-25), Qt::AlignCenter, "vision");
    font.setPointSize(11); painter.setFont(font); painter.setPen(QColor("#7c879a"));
    painter.drawText(rect().adjusted(0,70,0,70), Qt::AlignCenter, QStringLiteral("拖入视频开始播放  ·  Ctrl+O 打开文件"));
}
}
