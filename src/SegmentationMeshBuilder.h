#pragma once

#include <QVector>
#include <QVector3D>
#include <QImage>

struct SegmentationSurfaceData {
    QVector<QVector3D> vertices;
    QVector<QVector3D> normals;
    QVector<int> indices;
    QVector3D volumeSize = QVector3D(1, 1, 1);

    bool isEmpty() const {
        return vertices.isEmpty() || indices.isEmpty();
    }
};

class SegmentationMeshBuilder {
public:
    static SegmentationSurfaceData buildSurfaceMesh(
        const QVector<QImage>& maskSlices,
        double spacingX = 1.0,
        double spacingY = 1.0,
        double spacingZ = 1.0,
        int samplingStep = 1
    );
};
