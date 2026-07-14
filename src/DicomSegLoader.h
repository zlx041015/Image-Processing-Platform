#pragma once

#include "DicomToolkit.h"

#include <QImage>
#include <QString>
#include <QStringList>
#include <QVector>

struct DicomSegData {
    QString segmentLabel;
    QString segmentAlgorithmType;
    QString referencedSeriesInstanceUid;
    QVector<QImage> alignedMaskSlices;
    QStringList referencedSopInstanceUids;
    bool valid = false;
};

class DicomSegLoader {
public:
    static bool loadSegFile(
        const QString& segFilePath,
        const DicomSeriesData& ctSeries,
        DicomSegData* outSeg,
        QString* errorMessage = nullptr
    );

    static bool findAndLoadSegForSeries(
        const QString& rootPath,
        const DicomSeriesData& ctSeries,
        DicomSegData* outSeg,
        QString* errorMessage = nullptr
    );
};
