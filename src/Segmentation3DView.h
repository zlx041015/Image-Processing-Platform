#pragma once

#include "SegmentationMeshBuilder.h"

#include <QImage>
#include <QPoint>
#include <QWidget>

#include <memory>

class Segmentation3DView : public QWidget {
    Q_OBJECT

public:
    explicit Segmentation3DView(QWidget* parent = nullptr);
    ~Segmentation3DView() override;

    void setSurfaceData(const SegmentationSurfaceData& data);
    void clearSurfaceData();
    void setSlicePlanes(int axial, int coronal, int sagittal);
    void setSliceGeometry(int width, int height, int depth, double spacingX, double spacingY, double spacingZ);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    struct Impl;

    void invalidateScene(bool resetCamera = false);
    void rebuildSurfaceActor();
    void updateSlicePlaneActors();
    void ensureRenderedFrame();
    void renderSceneToImage();
    bool hasRenderableGeometry() const;
    void rotateCamera(float dx, float dy);
    void panCamera(const QPoint& delta);
    void zoomCamera(double factor);

    SegmentationSurfaceData m_surface;
    int m_axialIndex = 0;
    int m_coronalIndex = 0;
    int m_sagittalIndex = 0;
    int m_width = 1;
    int m_height = 1;
    int m_depth = 1;
    double m_spacingX = 1.0;
    double m_spacingY = 1.0;
    double m_spacingZ = 1.0;
    QPoint m_lastMousePos;
    bool m_rotating = false;
    bool m_panning = false;
    std::unique_ptr<Impl> m_impl;
};
