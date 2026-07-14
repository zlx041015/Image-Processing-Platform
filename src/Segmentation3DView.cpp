#include "Segmentation3DView.h"

#include <QMouseEvent>
#include <QPainter>
#include <QVector3D>
#include <QWheelEvent>

#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkDataArray.h>
#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkPlaneSource.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkWindowToImageFilter.h>

#include <algorithm>

namespace {
struct PlaneBundle {
    vtkSmartPointer<vtkPlaneSource> source;
    vtkSmartPointer<vtkPolyDataMapper> mapper;
    vtkSmartPointer<vtkActor> actor;
};

PlaneBundle createPlaneBundle(const QColor& color) {
    PlaneBundle plane;
    plane.source = vtkSmartPointer<vtkPlaneSource>::New();
    plane.mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    plane.mapper->SetInputConnection(plane.source->GetOutputPort());
    plane.actor = vtkSmartPointer<vtkActor>::New();
    plane.actor->SetMapper(plane.mapper);
    plane.actor->GetProperty()->SetColor(color.redF(), color.greenF(), color.blueF());
    plane.actor->GetProperty()->SetOpacity(0.22);
    plane.actor->GetProperty()->EdgeVisibilityOn();
    plane.actor->GetProperty()->SetEdgeColor(color.redF(), color.greenF(), color.blueF());
    plane.actor->GetProperty()->SetLineWidth(1.5f);
    plane.actor->GetProperty()->LightingOff();
    return plane;
}

QVector3D readVector(const double value[3]) {
    return QVector3D(
        static_cast<float>(value[0]),
        static_cast<float>(value[1]),
        static_cast<float>(value[2])
    );
}

void writeVector(const QVector3D& value, double output[3]) {
    output[0] = value.x();
    output[1] = value.y();
    output[2] = value.z();
}
}

struct Segmentation3DView::Impl {
    vtkSmartPointer<vtkRenderer> renderer;
    vtkSmartPointer<vtkRenderWindow> renderWindow;
    vtkSmartPointer<vtkWindowToImageFilter> captureFilter;

    vtkSmartPointer<vtkPolyData> surfacePolyData;
    vtkSmartPointer<vtkPolyDataMapper> surfaceMapper;
    vtkSmartPointer<vtkActor> surfaceActor;

    PlaneBundle axialPlane;
    PlaneBundle coronalPlane;
    PlaneBundle sagittalPlane;

    QImage cachedFrame;
    QSize cachedPixelSize;
    bool sceneDirty = true;
    bool cameraInitialized = false;
};

Segmentation3DView::Segmentation3DView(QWidget* parent)
    : QWidget(parent),
      m_impl(std::make_unique<Impl>()) {
    setMinimumSize(320, 260);
    setMouseTracking(true);
    setAutoFillBackground(false);

    m_impl->renderer = vtkSmartPointer<vtkRenderer>::New();
    m_impl->renderer->SetBackground(0.02, 0.03, 0.05);
    m_impl->renderer->SetBackground2(0.08, 0.11, 0.15);
    m_impl->renderer->GradientBackgroundOn();

    m_impl->renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    m_impl->renderWindow->SetOffScreenRendering(1);
    m_impl->renderWindow->SetMultiSamples(0);
    m_impl->renderWindow->SetAlphaBitPlanes(1);
    m_impl->renderWindow->AddRenderer(m_impl->renderer);

    m_impl->surfacePolyData = vtkSmartPointer<vtkPolyData>::New();
    m_impl->surfaceMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_impl->surfaceMapper->SetInputData(m_impl->surfacePolyData);

    m_impl->surfaceActor = vtkSmartPointer<vtkActor>::New();
    m_impl->surfaceActor->SetMapper(m_impl->surfaceMapper);
    m_impl->surfaceActor->GetProperty()->SetColor(0.09, 0.71, 0.15);
    m_impl->surfaceActor->GetProperty()->SetOpacity(1.0);
    m_impl->surfaceActor->GetProperty()->SetInterpolationToPhong();
    m_impl->surfaceActor->GetProperty()->SetAmbient(0.12);
    m_impl->surfaceActor->GetProperty()->SetDiffuse(0.88);
    m_impl->surfaceActor->GetProperty()->SetSpecular(0.10);
    m_impl->surfaceActor->GetProperty()->SetSpecularPower(12.0);
    m_impl->surfaceActor->GetProperty()->BackfaceCullingOn();
    m_impl->surfaceActor->SetVisibility(false);
    m_impl->renderer->AddActor(m_impl->surfaceActor);

    m_impl->axialPlane = createPlaneBundle(QColor("#ef4444"));
    m_impl->coronalPlane = createPlaneBundle(QColor("#22c55e"));
    m_impl->sagittalPlane = createPlaneBundle(QColor("#eab308"));
    m_impl->renderer->AddActor(m_impl->axialPlane.actor);
    m_impl->renderer->AddActor(m_impl->coronalPlane.actor);
    m_impl->renderer->AddActor(m_impl->sagittalPlane.actor);

    m_impl->captureFilter = vtkSmartPointer<vtkWindowToImageFilter>::New();
    m_impl->captureFilter->SetInput(m_impl->renderWindow);
    m_impl->captureFilter->SetInputBufferTypeToRGBA();
    m_impl->captureFilter->ReadFrontBufferOff();

    updateSlicePlaneActors();
}

Segmentation3DView::~Segmentation3DView() = default;

void Segmentation3DView::setSurfaceData(const SegmentationSurfaceData& data) {
    m_surface = data;
    rebuildSurfaceActor();
    invalidateScene();
}

void Segmentation3DView::clearSurfaceData() {
    m_surface = {};
    rebuildSurfaceActor();
    invalidateScene();
}

void Segmentation3DView::setSlicePlanes(int axial, int coronal, int sagittal) {
    m_axialIndex = std::clamp(axial, 0, std::max(0, m_depth - 1));
    m_coronalIndex = std::clamp(coronal, 0, std::max(0, m_height - 1));
    m_sagittalIndex = std::clamp(sagittal, 0, std::max(0, m_width - 1));
    updateSlicePlaneActors();
    invalidateScene();
}

void Segmentation3DView::setSliceGeometry(int width, int height, int depth, double spacingX, double spacingY, double spacingZ) {
    const int newWidth = std::max(1, width);
    const int newHeight = std::max(1, height);
    const int newDepth = std::max(1, depth);
    const double newSpacingX = std::max(0.001, spacingX);
    const double newSpacingY = std::max(0.001, spacingY);
    const double newSpacingZ = std::max(0.001, spacingZ);

    const bool geometryChanged =
        newWidth != m_width ||
        newHeight != m_height ||
        newDepth != m_depth ||
        !qFuzzyCompare(newSpacingX, m_spacingX) ||
        !qFuzzyCompare(newSpacingY, m_spacingY) ||
        !qFuzzyCompare(newSpacingZ, m_spacingZ);

    m_width = newWidth;
    m_height = newHeight;
    m_depth = newDepth;
    m_spacingX = newSpacingX;
    m_spacingY = newSpacingY;
    m_spacingZ = newSpacingZ;
    m_axialIndex = std::clamp(m_axialIndex, 0, std::max(0, m_depth - 1));
    m_coronalIndex = std::clamp(m_coronalIndex, 0, std::max(0, m_height - 1));
    m_sagittalIndex = std::clamp(m_sagittalIndex, 0, std::max(0, m_width - 1));

    updateSlicePlaneActors();
    invalidateScene(geometryChanged);
}

void Segmentation3DView::paintEvent(QPaintEvent*) {
    ensureRenderedFrame();

    QPainter painter(this);
    painter.fillRect(rect(), QColor("#05080c"));
    if (!m_impl->cachedFrame.isNull()) {
        painter.drawImage(rect(), m_impl->cachedFrame);
    }

    if (m_surface.isEmpty()) {
        painter.setPen(QColor("#d8ffe0"));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("等待分割结果以生成 VTK 三维模型"));
    }
}

void Segmentation3DView::mousePressEvent(QMouseEvent* event) {
    m_lastMousePos = event->pos();
    if (event->button() == Qt::LeftButton) {
        m_rotating = true;
    } else if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        m_panning = true;
    }
    event->accept();
}

void Segmentation3DView::mouseMoveEvent(QMouseEvent* event) {
    const QPoint delta = event->pos() - m_lastMousePos;
    m_lastMousePos = event->pos();

    if (m_rotating) {
        rotateCamera(static_cast<float>(delta.x()), static_cast<float>(delta.y()));
        invalidateScene();
    } else if (m_panning) {
        panCamera(delta);
        invalidateScene();
    }

    event->accept();
}

void Segmentation3DView::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    m_rotating = false;
    m_panning = false;
}

void Segmentation3DView::wheelEvent(QWheelEvent* event) {
    zoomCamera(event->angleDelta().y() > 0 ? 1.12 : 0.89);
    invalidateScene();
    event->accept();
}

void Segmentation3DView::invalidateScene(bool resetCamera) {
    m_impl->sceneDirty = true;
    if (resetCamera) {
        m_impl->cameraInitialized = false;
    }
    update();
}

void Segmentation3DView::rebuildSurfaceActor() {
    if (m_surface.isEmpty()) {
        m_impl->surfacePolyData->Initialize();
        m_impl->surfaceActor->SetVisibility(false);
        return;
    }

    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    points->SetNumberOfPoints(m_surface.vertices.size());
    for (int i = 0; i < m_surface.vertices.size(); ++i) {
        const QVector3D& v = m_surface.vertices[i];
        points->SetPoint(i, v.x(), v.y(), v.z());
    }

    vtkSmartPointer<vtkCellArray> triangles = vtkSmartPointer<vtkCellArray>::New();
    for (int i = 0; i + 2 < m_surface.indices.size(); i += 3) {
        const vtkIdType ids[3] = {
            static_cast<vtkIdType>(m_surface.indices[i]),
            static_cast<vtkIdType>(m_surface.indices[i + 1]),
            static_cast<vtkIdType>(m_surface.indices[i + 2])
        };
        triangles->InsertNextCell(3, ids);
    }

    m_impl->surfacePolyData->SetPoints(points);
    m_impl->surfacePolyData->SetPolys(triangles);

    if (m_surface.normals.size() == m_surface.vertices.size()) {
        vtkSmartPointer<vtkFloatArray> normals = vtkSmartPointer<vtkFloatArray>::New();
        normals->SetName("Normals");
        normals->SetNumberOfComponents(3);
        normals->SetNumberOfTuples(m_surface.normals.size());
        for (int i = 0; i < m_surface.normals.size(); ++i) {
            const QVector3D& n = m_surface.normals[i];
            normals->SetTuple3(i, n.x(), n.y(), n.z());
        }
        m_impl->surfacePolyData->GetPointData()->SetNormals(normals);
    } else {
        m_impl->surfacePolyData->GetPointData()->SetNormals(static_cast<vtkDataArray*>(nullptr));
    }

    m_impl->surfacePolyData->Modified();
    m_impl->surfaceActor->SetVisibility(true);
}

void Segmentation3DView::updateSlicePlaneActors() {
    const double volumeWidth = m_width * m_spacingX;
    const double volumeHeight = m_height * m_spacingY;
    const double volumeDepth = m_depth * m_spacingZ;

    const double axialZ = m_axialIndex * m_spacingZ;
    m_impl->axialPlane.source->SetOrigin(0.0, 0.0, axialZ);
    m_impl->axialPlane.source->SetPoint1(volumeWidth, 0.0, axialZ);
    m_impl->axialPlane.source->SetPoint2(0.0, volumeHeight, axialZ);

    const double coronalY = m_coronalIndex * m_spacingY;
    m_impl->coronalPlane.source->SetOrigin(0.0, coronalY, 0.0);
    m_impl->coronalPlane.source->SetPoint1(volumeWidth, coronalY, 0.0);
    m_impl->coronalPlane.source->SetPoint2(0.0, coronalY, volumeDepth);

    const double sagittalX = m_sagittalIndex * m_spacingX;
    m_impl->sagittalPlane.source->SetOrigin(sagittalX, 0.0, 0.0);
    m_impl->sagittalPlane.source->SetPoint1(sagittalX, volumeHeight, 0.0);
    m_impl->sagittalPlane.source->SetPoint2(sagittalX, 0.0, volumeDepth);

    m_impl->axialPlane.actor->SetVisibility(true);
    m_impl->coronalPlane.actor->SetVisibility(true);
    m_impl->sagittalPlane.actor->SetVisibility(true);
}

void Segmentation3DView::ensureRenderedFrame() {
    const QSize pixelSize(
        std::max(1, qRound(width() * devicePixelRatioF())),
        std::max(1, qRound(height() * devicePixelRatioF()))
    );
    if (m_impl->sceneDirty || m_impl->cachedPixelSize != pixelSize) {
        renderSceneToImage();
    }
}

void Segmentation3DView::renderSceneToImage() {
    if (width() <= 0 || height() <= 0 || !hasRenderableGeometry()) {
        m_impl->cachedFrame = {};
        m_impl->cachedPixelSize = {};
        m_impl->sceneDirty = false;
        return;
    }

    const double dpr = devicePixelRatioF();
    const int renderWidth = std::max(1, qRound(width() * dpr));
    const int renderHeight = std::max(1, qRound(height() * dpr));
    m_impl->renderWindow->SetSize(renderWidth, renderHeight);

    if (!m_impl->cameraInitialized) {
        m_impl->renderer->ResetCamera();
        vtkCamera* camera = m_impl->renderer->GetActiveCamera();
        camera->ParallelProjectionOn();
        camera->Azimuth(-18.0);
        camera->Elevation(8.0);
        camera->Zoom(1.18);
        camera->OrthogonalizeViewUp();
        m_impl->renderer->ResetCameraClippingRange();
        m_impl->cameraInitialized = true;
    }

    m_impl->renderWindow->Render();
    m_impl->captureFilter->Modified();
    m_impl->captureFilter->Update();

    vtkImageData* imageData = m_impl->captureFilter->GetOutput();
    if (!imageData) {
        m_impl->cachedFrame = {};
        m_impl->sceneDirty = false;
        return;
    }

    int dimensions[3] = {0, 0, 0};
    imageData->GetDimensions(dimensions);
    const int componentCount = imageData->GetNumberOfScalarComponents();
    if (dimensions[0] <= 0 || dimensions[1] <= 0 || componentCount < 3) {
        m_impl->cachedFrame = {};
        m_impl->sceneDirty = false;
        return;
    }

    unsigned char* buffer = static_cast<unsigned char*>(imageData->GetScalarPointer());
    if (!buffer) {
        m_impl->cachedFrame = {};
        m_impl->sceneDirty = false;
        return;
    }

    const int bytesPerLine = dimensions[0] * componentCount;
    const QImage::Format format = componentCount >= 4 ? QImage::Format_RGBA8888 : QImage::Format_RGB888;
    QImage raw(buffer, dimensions[0], dimensions[1], bytesPerLine, format);
    m_impl->cachedFrame = raw.copy().mirrored(false, true);
    m_impl->cachedFrame.setDevicePixelRatio(dpr);
    m_impl->cachedPixelSize = QSize(renderWidth, renderHeight);
    m_impl->sceneDirty = false;
}

bool Segmentation3DView::hasRenderableGeometry() const {
    return m_width > 0 && m_height > 0 && m_depth > 0;
}

void Segmentation3DView::rotateCamera(float dx, float dy) {
    if (!m_impl->cameraInitialized) {
        renderSceneToImage();
    }

    vtkCamera* camera = m_impl->renderer->GetActiveCamera();
    camera->Azimuth(-dx * 0.45);
    camera->Elevation(-dy * 0.30);
    camera->OrthogonalizeViewUp();
    m_impl->renderer->ResetCameraClippingRange();
}

void Segmentation3DView::panCamera(const QPoint& delta) {
    if (!m_impl->cameraInitialized) {
        renderSceneToImage();
    }

    vtkCamera* camera = m_impl->renderer->GetActiveCamera();
    double positionRaw[3] = {0.0, 0.0, 0.0};
    double focalRaw[3] = {0.0, 0.0, 0.0};
    double upRaw[3] = {0.0, 0.0, 0.0};
    camera->GetPosition(positionRaw);
    camera->GetFocalPoint(focalRaw);
    camera->GetViewUp(upRaw);

    QVector3D position = readVector(positionRaw);
    QVector3D focalPoint = readVector(focalRaw);
    QVector3D viewUp = readVector(upRaw).normalized();
    QVector3D direction = (focalPoint - position).normalized();
    QVector3D right = QVector3D::crossProduct(direction, viewUp).normalized();
    viewUp = QVector3D::crossProduct(right, direction).normalized();

    const double verticalSpan = camera->GetParallelScale() * 2.0;
    const double horizontalSpan = verticalSpan * static_cast<double>(width()) / std::max(1, height());

    const QVector3D translation =
        right * static_cast<float>(-delta.x() * horizontalSpan / std::max(1, width())) +
        viewUp * static_cast<float>(delta.y() * verticalSpan / std::max(1, height()));

    position += translation;
    focalPoint += translation;
    writeVector(position, positionRaw);
    writeVector(focalPoint, focalRaw);
    camera->SetPosition(positionRaw);
    camera->SetFocalPoint(focalRaw);
    m_impl->renderer->ResetCameraClippingRange();
}

void Segmentation3DView::zoomCamera(double factor) {
    if (!m_impl->cameraInitialized) {
        renderSceneToImage();
    }

    vtkCamera* camera = m_impl->renderer->GetActiveCamera();
    camera->Zoom(std::clamp(factor, 0.2, 5.0));
    m_impl->renderer->ResetCameraClippingRange();
}
