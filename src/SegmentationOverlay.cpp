#include "SegmentationOverlay.h"

#include <QPainter>

QImage SegmentationOverlay::overlayMask(const QImage& base, const QImage& mask, const QColor& color, double alpha) {
    const QImage gray = base.convertToFormat(QImage::Format_RGB32);
    if (gray.isNull() || mask.isNull() || gray.size() != mask.size()) {
        return {};
    }
    QImage out = gray.copy();
    const int a = static_cast<int>(255 * std::clamp(alpha, 0.0, 1.0));
    for (int y = 0; y < out.height(); ++y) {
        QRgb* dst = reinterpret_cast<QRgb*>(out.scanLine(y));
        const uchar* m = mask.constScanLine(y);
        for (int x = 0; x < out.width(); ++x) {
            if (m[x] == 0) continue;
            const QColor baseColor = QColor::fromRgb(dst[x]);
            const int r = (baseColor.red() * (255 - a) + color.red() * a) / 255;
            const int g = (baseColor.green() * (255 - a) + color.green() * a) / 255;
            const int b = (baseColor.blue() * (255 - a) + color.blue() * a) / 255;
            dst[x] = qRgb(r, g, b);
        }
    }
    return out;
}

QImage SegmentationOverlay::drawSeeds(
    const QImage& image,
    const QVector<SegmentationSeedPoint>& seeds,
    int sliceIndex,
    Qt::Orientation orientation,
    int orthogonalIndex
) {
    if (image.isNull()) {
        return {};
    }
    QImage out = image.convertToFormat(QImage::Format_RGB32);
    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const auto& seed : seeds) {
        QPoint p;
        bool visible = false;
        if (orientation == Qt::Horizontal) {
            visible = orthogonalIndex >= 0 && seed.y == orthogonalIndex;
            p = QPoint(seed.x, sliceIndex - seed.z - 1);
        } else if (orientation == Qt::Vertical) {
            visible = orthogonalIndex >= 0 && seed.x == orthogonalIndex;
            p = QPoint(sliceIndex - seed.z - 1, seed.y);
        } else {
            visible = seed.z == sliceIndex;
            p = QPoint(seed.x, seed.y);
        }
        if (!visible) continue;
        painter.setPen(QPen(seed.foreground ? QColor("#ff7f0e") : QColor("#00bcd4"), 2));
        painter.setBrush(seed.foreground ? QColor("#ff7f0e") : QColor("#00bcd4"));
        painter.drawEllipse(p, 4, 4);
    }
    painter.end();
    return out;
}
