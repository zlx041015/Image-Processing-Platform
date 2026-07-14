#pragma once

#include "SegmentVolume.h"

#include <QColor>
#include <QImage>
#include <QVector>

class SegmentationOverlay {
public:
    static QImage overlayMask(const QImage& base, const QImage& mask, const QColor& color, double alpha = 0.45);
    static QImage drawSeeds(
        const QImage& image,
        const QVector<SegmentationSeedPoint>& seeds,
        int sliceIndex,
        Qt::Orientation orientation,
        int orthogonalIndex = -1
    );
};
