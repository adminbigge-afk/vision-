#pragma once
#include <QIcon>
#include <QPainter>
#include <QPushButton>

namespace vision {
inline void setTransportIcon(QPushButton* button,const QString& name)
{
    if(button->property("transportIcon").toString()==name)return;
    QIcon icon;
    for(int size:{20,30,40,60}){
        QPixmap image(size,size);image.fill(Qt::transparent);QPainter p(&image);
        p.setRenderHint(QPainter::Antialiasing);p.scale(size/20.0,size/20.0);p.setPen(Qt::NoPen);p.setBrush(QColor("#dedede"));
        if(name=="play")p.drawPolygon(QPolygonF{{6,3},{17,10},{6,17}});
        else if(name=="pause"){p.drawRect(QRectF(5,3,4,14));p.drawRect(QRectF(12,3,4,14));}
        else if(name=="stop")p.drawRect(QRectF(5,5,11,11));
        else if(name=="open"){p.drawPolygon(QPolygonF{{4,11},{10,4},{16,11}});p.drawRect(QRectF(4,14,12,2));}
        else{if(name=="next"){p.translate(20,0);p.scale(-1,1);}p.drawRect(QRectF(4,4,3,12));p.drawPolygon(QPolygonF{{7,10},{16,4},{16,16}});}
        p.end();icon.addPixmap(image);
    }
    button->setText({});button->setIcon(icon);button->setIconSize(QSize(18,18));button->setProperty("transportIcon",name);
}
}
