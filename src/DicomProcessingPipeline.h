#pragma once

#include <QImage>
#include <QString>
#include <QVector>

enum class DicomProcessingScope {
    CurrentSlice,
    WholeVolume
};

enum class DicomProcessingTarget {
    Original,
    SegmentationInput,
    PostSegmentationDisplay
};

struct DicomProcessRequest {
    QString actionName;
    int kernelSize = 3;
    int threshold = 128;
    int order = 2;
    double gamma = 0.8;
    double cutoff = 40.0;
    double gammaLow = 0.5;
    double gammaHigh = 1.8;
    double c = 1.0;
};

class DicomProcessingPipeline {
public:
    static bool apply(
        const QVector<QImage>& inputSlices,
        DicomProcessingScope scope,
        int currentSliceIndex,
        const DicomProcessRequest& request,
        QVector<QImage>* outputSlices,
        QString* errorMessage = nullptr
    );
};
