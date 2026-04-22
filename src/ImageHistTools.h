#pragma once
#include <QImage>
#include <QPixmap>

class ImageHistTools
{
public:
    static QPixmap drawGrayHistogram(const QImage &img, int w = 280, int h = 200);
    static QImage linearStretch(const QImage &img);
    static QImage equalizeHist(const QImage &img);
    static QImage matchHist(const QImage &src, const QImage &target);
};