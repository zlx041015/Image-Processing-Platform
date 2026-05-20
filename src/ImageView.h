#pragma once

#include <QGraphicsView>
#include <QImage>
#include <QPoint>

class QGraphicsScene;
class QGraphicsPixmapItem;

class ImageView : public QGraphicsView {
    Q_OBJECT

public:
    explicit ImageView(QWidget* parent = nullptr);
    void setImages(const QImage& original, const QImage& filtered);
    void setZoomFactor(double zoom);
    double fitToView();
    double zoomFactor() const { return m_zoom; }

signals:
    void hoverPixel(int x, int y, QRgb rgba);
    void clickPixel(int x, int y, QRgb rgba);
    void hoverOutside();
    void zoomRequested(int delta);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void updatePixmap();
    bool mapToImagePixel(const QPoint& viewPos, QPoint* imagePos) const;

    QGraphicsScene* m_scene = nullptr;
    QGraphicsPixmapItem* m_item = nullptr;
    QImage m_originalImage;
    QImage m_filteredImage;
    double m_zoom = 1.0;
    bool m_dragging = false;
    QPoint m_pressPos;
    const int m_dragThreshold = 5;
};
