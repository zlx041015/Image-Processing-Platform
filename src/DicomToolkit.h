#pragma once

#include <QImage>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QVector3D>

struct DicomSeriesData {
    QVector<QImage> rawSlices;
    QVector<double> slicePositions;
    QStringList sliceNames;
    QStringList sopInstanceUids;
    QString seriesInstanceUid;
    QString modality;
    int width = 0;
    int height = 0;
    int bitDepth = 0;
    double pixelSpacingX = 1.0;
    double pixelSpacingY = 1.0;
    double sliceThickness = 1.0;
    QVector3D origin = QVector3D(0, 0, 0);
    QVector3D rowDirection = QVector3D(1, 0, 0);
    QVector3D columnDirection = QVector3D(0, 1, 0);
    QVector3D sliceDirection = QVector3D(0, 0, 1);
    double defaultWindowCenter = 40.0;
    double defaultWindowWidth = 400.0;
};

class DicomToolkit {
public:
    static bool loadDicomFile(const QString& filePath, DicomSeriesData* outSeries, QString* errorMessage = nullptr);
    static bool loadDicomDirectory(
        const QString& directoryPath,
        DicomSeriesData* outSeries,
        QString* errorMessage = nullptr,
        const QString& preferredSeriesUid = QString()
    );
    static QImage applyWindowLevel(const QImage& image, double windowCenter, double windowWidth);
    static QImage buildCoronalSlice(const QVector<QImage>& slices, int rowIndex);
    static QImage buildSagittalSlice(const QVector<QImage>& slices, int columnIndex);
    static QImage renderVolumePreview(const QVector<QImage>& slices, int axialSlice, int coronalSlice, int sagittalSlice);
};
