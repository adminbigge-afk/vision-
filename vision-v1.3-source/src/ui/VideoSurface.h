#pragma once
#include <QWidget>
#include <QTimer>

namespace vision {
class VideoSurface final : public QWidget {
    Q_OBJECT
public:
    explicit VideoSurface(QWidget* parent = nullptr);
    void routeNativeMouse();
signals:
    void clicked();
    void doubleClicked();
    void activity();
    void contextRequested(const QPoint& point);
    void volumeWheel(int direction);
protected:
    void mousePressEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void paintEvent(QPaintEvent*) override;
private:
    QTimer clickTimer_;
};
}
