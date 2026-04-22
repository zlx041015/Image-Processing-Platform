#include "ImageView.h"

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>
#include <QPixmap>

#include <cstdlib>
#include <algorithm>

//创建窗口并设置背景颜色、鼠标跟踪、对齐方式等属性
ImageView::ImageView(QWidget* parent) : QGraphicsView(parent) {
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);
    m_item = m_scene->addPixmap(QPixmap());

    setBackgroundBrush(QColor("#0f172a"));  //设置背景颜色
    setFrameShape(QFrame::NoFrame); //去掉边框
    setMouseTracking(true); //启用鼠标跟踪
    setAlignment(Qt::AlignLeft | Qt::AlignTop);//设置对齐方式
    setTransformationAnchor(QGraphicsView::NoAnchor);//设置缩放时的锚点为无，保持图像位置不变
    setResizeAnchor(QGraphicsView::NoAnchor);   //设置调整大小时的锚点为无，保持图像位置不变
    setDragMode(QGraphicsView::NoDrag); //默认不启用拖动，鼠标按下时根据情况切换到滚动模式
}

//设置原始图像和滤镜处理后的图像，并更新显示
void ImageView::setImages(const QImage& original, const QImage& filtered) {
    m_originalImage = original; //保存原始图像数据
    m_filteredImage = filtered; //保存滤镜处理后的图像数据
    updatePixmap(); //更新显示的图像
    horizontalScrollBar()->setValue(0); //重置水平滚动条位置
    verticalScrollBar()->setValue(0);   //重置垂直滚动条位置
}

//设置当前的缩放因子，并更新显示
void ImageView::setZoomFactor(double zoom) {
    m_zoom = zoom;  //保存当前的缩放因子
    updatePixmap(); //更新显示的图像
}

//刷新图片显示，根据当前的原始图像、滤镜图像和缩放因子进行调整
void ImageView::updatePixmap() {
    if (m_filteredImage.isNull()) { //如果没有图像数据，则清空显示
        m_item->setPixmap(QPixmap());   //设置空的像素图
        m_scene->setSceneRect(QRectF());    //设置空的场景矩形
        return;
    }
    
    //计算缩放后的目标尺寸，确保宽高至少为1像素，避免出现无效的尺寸
    QSize targetSize(
        std::max(1, static_cast<int>(m_filteredImage.width() * m_zoom)),
        std::max(1, static_cast<int>(m_filteredImage.height() * m_zoom))
    );

    //根据当前的缩放因子选择合适的缩放模式，缩放因子大于等于1时使用快速缩放，较小的缩放因子使用平滑缩放以获得更好的质量
    Qt::TransformationMode mode = m_zoom >= 1.0 ? Qt::FastTransformation : Qt::SmoothTransformation;
    QPixmap pixmap = QPixmap::fromImage(m_filteredImage.scaled(targetSize, Qt::IgnoreAspectRatio, mode));
    m_item->setPixmap(pixmap);//设置显示的像素图
    m_item->setPos(20, 20); //将图像位置设置为(20, 20)，在视图中留出边距
    m_scene->setSceneRect(0, 0, pixmap.width() + 40, pixmap.height() + 40); //设置场景矩形大小，确保包含图像和边距
}

//将窗口坐标转换成图片中的点坐标
bool ImageView::mapToImagePixel(const QPoint& viewPos, QPoint* imagePos) const {
    if (m_originalImage.isNull()) { //如果没有图像数据，则无法映射坐标
        return false;
    }

    //将视图坐标转换为场景坐标，并根据当前的缩放因子计算对应的图像像素坐标，考虑到图像在视图中的位置偏移（20像素的边距）
    QPointF scenePos = mapToScene(viewPos);
    //视图位置转为画布位置
    double x = (scenePos.x() - 20.0) / m_zoom;
    double y = (scenePos.y() - 20.0) / m_zoom;

    //转为整数坐标，检查是否超出图像范围
    int ix = static_cast<int>(x);
    int iy = static_cast<int>(y);
    if (ix < 0 || iy < 0 || ix >= m_originalImage.width() || iy >= m_originalImage.height()) {
        return false;
    }

    //不在图片范围内，返回 false
    if (imagePos) {
        *imagePos = QPoint(ix, iy);
    }
    return true;
}

//处理鼠标按下事件，根据情况切换到滚动模式或发送点击像素信号
void ImageView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && !m_originalImage.isNull()) {   //仅在左键按下且有图像数据时处理
        m_dragging = false; //重置拖动状态
        m_pressPos = event->pos();  //记录鼠标按下时的坐标
        setCursor(Qt::ClosedHandCursor);    //切换到闭合手形状，表示可以拖动
    }
    QGraphicsView::mousePressEvent(event);  //调用基类的事件处理，确保其他事件正常传递
}

//处理鼠标移动事件，更新悬停像素信息或执行拖动操作
void ImageView::mouseMoveEvent(QMouseEvent* event) {
    if (!m_originalImage.isNull()) {    //如果有图像数据，根据鼠标位置更新悬停信息
        QPoint imgPos;  //转换为图像坐标
        if (mapToImagePixel(event->pos(), &imgPos)) {   //如果鼠标在图像范围内，发送悬停像素信号
            emit hoverPixel(imgPos.x(), imgPos.y(), m_originalImage.pixel(imgPos)); //发送像素坐标和颜色信息
        } else {    //如果鼠标不在图像范围内，发送悬停在外部的信号
            emit hoverOutside();    //发送鼠标悬停在图像外部的信号
        }
    }

    //如果左键按下且有图像数据，检查是否超过拖动阈值，如果是则执行拖动操作，更新滚动条位置
    if ((event->buttons() & Qt::LeftButton) && !m_originalImage.isNull()) { //仅在左键按下且有图像数据时处理
        QPoint delta = event->pos() - m_pressPos;   //计算鼠标移动的距离
        if (std::abs(delta.x()) > m_dragThreshold || std::abs(delta.y()) > m_dragThreshold) {   //如果移动距离超过拖动阈值，认为是拖动操作
            m_dragging = true;  //更新滚动条位置，实现图像的平移效果
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());    //水平滚动条根据鼠标移动的距离进行调整，向左移动时增加滚动值，向右移动时减少滚动值
            verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());    //垂直滚动条根据鼠标移动的距离进行调整，向上移动时增加滚动值，向下移动时减少滚动值
            m_pressPos = event->pos();  //更新按下位置为当前鼠标位置，继续跟踪后续的移动距离
        }
    }

    QGraphicsView::mouseMoveEvent(event);   //调用基类的事件处理，确保其他事件正常传递
}

//处理鼠标释放事件，如果是点击操作则发送点击像素信号，重置拖动状态和光标
void ImageView::mouseReleaseEvent(QMouseEvent* event) { //仅在左键释放且有图像数据时处理
    if (event->button() == Qt::LeftButton && !m_originalImage.isNull()) {   //切换回十字光标，表示可以选择像素
        setCursor(Qt::CrossCursor);   //如果不是拖动操作，认为是点击操作，发送点击像素信号
        if (!m_dragging) {  //转换为图像坐标并发送点击像素信号
            QPoint imgPos;  //转换为图像坐标
            if (mapToImagePixel(event->pos(), &imgPos)) {   //如果鼠标在图像范围内，发送点击像素信号
                emit clickPixel(imgPos.x(), imgPos.y(), m_originalImage.pixel(imgPos)); //发送像素坐标和颜色信息
            }
        }
        m_dragging = false; //重置拖动状态
    }
    QGraphicsView::mouseReleaseEvent(event);    //调用基类的事件处理，确保其他事件正常传递
}

//处理鼠标滚轮事件，根据是否按下 Shift 键来区分水平滚动和垂直滚动
void ImageView::wheelEvent(QWheelEvent* event) {    //如果按下 Shift 键，则执行水平滚动，否则执行垂直滚动
    if (event->modifiers() & Qt::ShiftModifier) {   //水平滚动，根据滚轮的旋转方向调整水平滚动条的位置，向左滚动时增加滚动值，向右滚动时减少滚动值
        int delta = event->angleDelta().y();    //水平滚动条根据鼠标滚轮的旋转方向进行调整，向左滚动时增加滚动值，向右滚动时减少滚动值
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() + (delta < 0 ? 60 : -60));   //处理滚动条
        event->accept();    //垂直滚动，根据滚轮的旋转方向调整垂直滚动条的位置，向上滚动时增加滚动值，向下滚动时减少滚动值
        return; //如果没有按下 Shift 键，则执行垂直滚动，根据滚轮的旋转方向调整垂直滚动条的位置，向上滚动时增加滚动值，向下滚动时减少滚动值
    }

   int delta = event->angleDelta().y(); //垂直滚动条根据鼠标滚轮的旋转方向进行调整，向上滚动时增加滚动值，向下滚动时减少滚动值
    verticalScrollBar()->setValue(verticalScrollBar()->value() + (delta < 0 ? 60 : -60));   //处理滚动条
    event->accept();
}
