#include "ImageView.h"

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>
#include <QPixmap>

#include <algorithm>
#include <cstdlib>

ImageView::ImageView(QWidget* parent) : QGraphicsView(parent) {
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);
    m_item = m_scene->addPixmap(QPixmap());

    setBackgroundBrush(QColor("#090d13"));
    setFrameShape(QFrame::NoFrame);
    setMouseTracking(true);
    setAlignment(Qt::AlignCenter);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setResizeAnchor(QGraphicsView::NoAnchor);
    setDragMode(QGraphicsView::NoDrag);
}

void ImageView::setImages(const QImage& original, const QImage& filtered) {
    m_originalImage = original;
    m_filteredImage = filtered;
    updatePixmap();
}

void ImageView::setZoomFactor(double zoom) {
    m_zoom = zoom;
    updatePixmap();
}

double ImageView::fitToView() {
    if (m_filteredImage.isNull()) {
        return m_zoom;
    }

    const QSize available = viewport() ? viewport()->size() : size();
    if (available.width() <= 2 || available.height() <= 2) {
        return m_zoom;
    }

    const double xScale = static_cast<double>(available.width() - 2) / m_filteredImage.width();
    const double yScale = static_cast<double>(available.height() - 2) / m_filteredImage.height();
    m_zoom = std::max(0.01, std::min(xScale, yScale));
    updatePixmap();
    return m_zoom;
}

void ImageView::updatePixmap() {
    if (m_filteredImage.isNull()) {
        m_item->setPixmap(QPixmap());
        m_scene->setSceneRect(QRectF());
        return;
    }

    QSize targetSize(
        std::max(1, static_cast<int>(m_filteredImage.width() * m_zoom)),
        std::max(1, static_cast<int>(m_filteredImage.height() * m_zoom))
    );

    Qt::TransformationMode mode = m_zoom >= 1.0 ? Qt::FastTransformation : Qt::SmoothTransformation;
    QPixmap pixmap = QPixmap::fromImage(m_filteredImage.scaled(targetSize, Qt::IgnoreAspectRatio, mode));
    m_item->setPixmap(pixmap);
    m_item->setPos(0, 0);
    m_scene->setSceneRect(0, 0, pixmap.width(), pixmap.height());
    centerOn(m_scene->sceneRect().center());
}

bool ImageView::mapToImagePixel(const QPoint& viewPos, QPoint* imagePos) const {
    if (m_originalImage.isNull()) {
        return false;
    }

    QPointF scenePos = mapToScene(viewPos);
    double x = scenePos.x() / m_zoom;
    double y = scenePos.y() / m_zoom;

    int ix = static_cast<int>(x);
    int iy = static_cast<int>(y);
    if (ix < 0 || iy < 0 || ix >= m_originalImage.width() || iy >= m_originalImage.height()) {
        return false;
    }

    if (imagePos) {
        *imagePos = QPoint(ix, iy);
    }
    return true;
}

void ImageView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && !m_originalImage.isNull()) {
        m_dragging = false;
        m_pressPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
    QGraphicsView::mousePressEvent(event);
}

void ImageView::mouseMoveEvent(QMouseEvent* event) {
    if (!m_originalImage.isNull()) {
        QPoint imgPos;
        if (mapToImagePixel(event->pos(), &imgPos)) {
            emit hoverPixel(imgPos.x(), imgPos.y(), m_originalImage.pixel(imgPos));
        } else {
            emit hoverOutside();
        }
    }

    if ((event->buttons() & Qt::LeftButton) && !m_originalImage.isNull()) {
        QPoint delta = event->pos() - m_pressPos;
        if (std::abs(delta.x()) > m_dragThreshold || std::abs(delta.y()) > m_dragThreshold) {
            m_dragging = true;
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
            m_pressPos = event->pos();
        }
    }

    QGraphicsView::mouseMoveEvent(event);
}

void ImageView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && !m_originalImage.isNull()) {
        setCursor(Qt::CrossCursor);
        if (!m_dragging) {
            QPoint imgPos;
            if (mapToImagePixel(event->pos(), &imgPos)) {
                emit clickPixel(imgPos.x(), imgPos.y(), m_originalImage.pixel(imgPos));
            }
        }
        m_dragging = false;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void ImageView::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        emit zoomRequested(event->angleDelta().y());
        event->accept();
        return;
    }

    if (event->modifiers() & Qt::ShiftModifier) {
        int delta = event->angleDelta().y();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() + (delta < 0 ? 60 : -60));
        event->accept();
        return;
    }

    int delta = event->angleDelta().y();
    verticalScrollBar()->setValue(verticalScrollBar()->value() + (delta < 0 ? 60 : -60));
    event->accept();
}
