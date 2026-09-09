#pragma once
#include <QRect>
#include <QSize>
#include <algorithm>
#include <cmath>

namespace vision {
// Video dimensions are physical display pixels; Qt window coordinates are logical.
inline QSize fittedVideoWindow(QSize pixels,qreal dpr,QSize available,int chromeHeight)
{
    if(pixels.isEmpty()||dpr<=0)return {};
    const double w=pixels.width()/dpr,h=pixels.height()/dpr;
    const double scale=std::min({1.0,available.width()/w,qMax(1,available.height()-chromeHeight)/h});
    return {qMax(1,int(std::floor(w*scale))),qMax(1,int(std::floor(h*scale)))+chromeHeight};
}
inline QPoint containedPosition(QPoint preferred,QSize size,QRect available)
{
    return {qBound(available.left(),preferred.x(),qMax(available.left(),available.right()-size.width()+1)),
            qBound(available.top(),preferred.y(),qMax(available.top(),available.bottom()-size.height()+1))};
}
}
