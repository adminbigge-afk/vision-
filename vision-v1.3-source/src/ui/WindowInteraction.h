#pragma once
#include <QRect>
#include <QList>
#include <Qt>
#include <cmath>

namespace vision {
inline constexpr int resizeBorder=5;
inline Qt::Edges resizeEdges(QPoint point,QSize size)
{
    if(!QRect(QPoint(),size).contains(point))return {};
    Qt::Edges edges;
    if(point.x()<resizeBorder)edges|=Qt::LeftEdge;
    if(point.x()>=size.width()-resizeBorder)edges|=Qt::RightEdge;
    if(point.y()<resizeBorder)edges|=Qt::TopEdge;
    if(point.y()>=size.height()-resizeBorder)edges|=Qt::BottomEdge;
    if(edges){
        if(point.x()<12)edges|=Qt::LeftEdge;
        if(point.x()>=size.width()-12)edges|=Qt::RightEdge;
        if(point.y()<12)edges|=Qt::TopEdge;
        if(point.y()>=size.height()-12)edges|=Qt::BottomEdge;
    }
    return edges;
}
inline Qt::CursorShape resizeCursor(Qt::Edges edges)
{
    if(edges==(Qt::LeftEdge|Qt::TopEdge)||edges==(Qt::RightEdge|Qt::BottomEdge))return Qt::SizeFDiagCursor;
    if(edges==(Qt::RightEdge|Qt::TopEdge)||edges==(Qt::LeftEdge|Qt::BottomEdge))return Qt::SizeBDiagCursor;
    return edges.testFlag(Qt::LeftEdge)||edges.testFlag(Qt::RightEdge)?Qt::SizeHorCursor:Qt::SizeVerCursor;
}
// Rectangles and threshold share physical screen coordinates, including negative monitors.
inline QRect snappedWindow(QRect moving,const QList<QRect>& others,int threshold)
{
    int dx=threshold+1,dy=threshold+1;
    auto choose=[threshold](int value,int& best){if(std::abs(value)<=threshold&&std::abs(value)<std::abs(best))best=value;};
    for(const QRect& target:others){
        const bool nearY=moving.top()<=target.bottom()+threshold&&moving.bottom()+threshold>=target.top();
        const bool nearX=moving.left()<=target.right()+threshold&&moving.right()+threshold>=target.left();
        if(nearY)for(int delta:{target.x()+target.width()-moving.x(),target.x()-moving.x()-moving.width(),target.x()-moving.x(),target.x()+target.width()-moving.x()-moving.width()})choose(delta,dx);
        if(nearX)for(int delta:{target.y()+target.height()-moving.y(),target.y()-moving.y()-moving.height(),target.y()-moving.y(),target.y()+target.height()-moving.y()-moving.height()})choose(delta,dy);
    }
    return moving.translated(std::abs(dx)<=threshold?dx:0,std::abs(dy)<=threshold?dy:0);
}
}
