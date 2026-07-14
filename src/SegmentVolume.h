#pragma once

#include <QImage>
#include <QVector>

struct SegmentationSeedPoint {
    int x = 0;
    int y = 0;
    int z = 0;
    bool foreground = true;

    bool operator==(const SegmentationSeedPoint& other) const {
        return x == other.x &&
            y == other.y &&
            z == other.z &&
            foreground == other.foreground;
    }
};

enum class SegmentationMethod {
    Threshold,
    Otsu,
    RegionGrow
};

struct SegmentationParams {
    int threshold = 140;
    int tolerance = 25;
    int minComponentSize = 80;
    int openIterations = 1;
    int closeIterations = 1;
    int holeFillIterations = 1;
};

class SegmentVolume {
public:
    static QVector<QImage> run(
        const QVector<QImage>& slices,
        SegmentationMethod method,
        const QVector<SegmentationSeedPoint>& seeds,
        const SegmentationParams& params
    );
};
