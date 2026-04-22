#pragma once

#include <QGraphicsView>
#include <QImage>
#include <QPoint>

class QGraphicsScene;
class QGraphicsPixmapItem;

/// 自定义的图像显示组件，基于 QGraphicsView 实现，支持显示图像、缩放、平移，以及鼠标交互
class ImageView : public QGraphicsView {
    Q_OBJECT    // 该类负责显示图像，并处理用户的鼠标交互，如悬停、点击、缩放和平移
public:
    explicit ImageView(QWidget* parent = nullptr);  //创建一个图像显示窗口

    void setImages(const QImage& original, const QImage& filtered);  // 设置原始图像和滤镜处理后的图像，并更新显示
    void setZoomFactor(double zoom);    // 设置当前的缩放因子，并更新显示
    double zoomFactor() const { return m_zoom; }    // 获取当前的缩放因子

signals:
    void hoverPixel(int x, int y, QRgb rgba);   // 当鼠标悬停在图像上时，发送像素坐标和颜色信息
    void clickPixel(int x, int y, QRgb rgba);   // 当鼠标点击图像时，发送像素坐标和颜色信息
    void hoverOutside();   // 当鼠标悬停在图像外部时，发送信号

protected:
    void mousePressEvent(QMouseEvent* event) override;  // 处理鼠标按下事件
    void mouseMoveEvent(QMouseEvent* event) override;   // 处理鼠标移动事件
    void mouseReleaseEvent(QMouseEvent* event) override;    // 处理鼠标释放事件
    void wheelEvent(QWheelEvent* event) override;   // 处理鼠标滚轮事件，用于实现缩放功能

private:
    void updatePixmap();   // 更新显示的图像，根据当前的原始图像、滤镜图像和缩放因子进行调整
    bool mapToImagePixel(const QPoint& viewPos, QPoint* imagePos) const;    // 将窗口坐标转换成图片中的点坐标

    QGraphicsScene* m_scene = nullptr;  // 画布，用于显示图像
    QGraphicsPixmapItem* m_item = nullptr;  //贴在画布上的图片
    QImage m_originalImage; //原图像素数据
    QImage m_filteredImage; //滤镜处理后的图像数据
    double m_zoom = 1.0;    // 当前的缩放因子
    bool m_dragging = false;    // 是否正在拖动图像
    QPoint m_pressPos;  // 鼠标按下时的坐标
    const int m_dragThreshold = 5;  // 拖动阈值，鼠标移动超过该距离则认为是拖动而非点击
};
