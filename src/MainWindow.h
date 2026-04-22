#pragma once

#include <QMainWindow>
#include <QImage>
#include <QVector>

class QListWidget;
class QLabel;
class QComboBox;
class ImageView;
class QListWidgetItem;
class QDragEnterEvent;
class QDropEvent;

// 主窗口类，负责整体界面布局、事件处理和图像显示
class MainWindow : public QMainWindow {
    Q_OBJECT    //让操作（点击）可以触发函数
public:
    explicit MainWindow(QWidget* parent = nullptr); // 构造函数，初始化主窗口

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;   // 拖拽进入事件处理，判断是否接受拖拽
    void dropEvent(QDropEvent* event) override; // 拖拽释放事件处理，获取文件路径并加载图像

private slots:
    void selectFolder();    // 打开文件夹选择对话框，加载选定文件夹中的图像文件
    void openSingleFile();  // 打开单个图像文件，加载并显示该图像
    void onFileSelected(QListWidgetItem* item); // 文件列表项被点击时的处理，显示选定的图像
    void onFilterChanged(); // 滤镜选项改变时的处理，应用选定的滤镜并更新显示
    void zoomIn();  // 放大图像，增加缩放比例并更新显示
    void zoomOut(); // 缩小图像，减少缩放比例并更新显示
    void resetZoom();   // 重置缩放，恢复到原始大小并更新显示
    void updateHoverInfo(int x, int y, QRgb rgba);  // 鼠标悬停在图像上时更新像素信息显示
    void updateHoverOutside();  // 鼠标移出图像区域时清除像素信息显示
    void showPixelInfo(int x, int y, QRgb rgba);    // 鼠标点击图像时显示像素详细信息的对话框
    void showHistogram();
    void doLinearStretch();
    void doEqualize();
    void doHistMatch();

private:
    void createMenus(); // 创建菜单栏，当前实现中隐藏了菜单栏
    void createUi();    // 创建用户界面，设置布局和控件
    void loadImageFiles(const QString& folder); // 加载指定文件夹中的图像文件，更新文件列表
    void displayImage(const QString& filePath); // 加载并显示指定路径的图像，更新状态栏信息
    void renderCurrentImage();  // 根据当前原始图像和选定的滤镜渲染处理后的图像，并更新显示
    bool isSupportedImageFile(const QString& filePath) const;   // 判断文件是否是支持的图像格式（BMP/JPG/JPEG）
    QImage applyFilter(const QImage& img, const QString& filterName) const; // 根据滤镜名称应用相应的图像处理算法，返回处理后的图像
    QImage convolve(const QImage& img, const QVector<float>& kernel, float divisor, float bias = 0.0f) const;   // 对图像进行卷积处理，使用指定的卷积核、除数和偏移量，返回处理后的图像
    static int clampToByte(int value);  // 将整数值限制在0-255范围内，常用于颜色分量的计算结果

    QString m_currentFolder;    // 当前文件夹路径
    QString m_currentFile;  // 当前文件路径
    QImage m_originalImage; // 当前原始图像数据
    QImage m_filteredImage; // 当前滤镜处理后的图像数据
    int m_currentWidth = 0; // 当前图像宽度
    int m_currentHeight = 0;    // 当前图像高度
    int m_currentBitCount = 0;  // 当前图像位深
    uint32_t m_currentCompression = 0;  // 当前图像压缩类型（仅对 BMP 有效）

    double m_zoom = 1.0;    // 当前缩放比例
    const double m_minZoom = 0.1;   // 最小缩放比例
    const double m_maxZoom = 8.0;   // 最大缩放比例
    const double m_zoomStep = 0.01;  // 每次缩放调整的步长

    // UI 控件指针
    QListWidget* m_fileList = nullptr;  // 文件列表控件
    QLabel* m_infoLabel = nullptr;  //  图像信息显示标签
    QLabel* m_folderLabel = nullptr;    // 当前文件夹显示标签
    QLabel* m_zoomLabel = nullptr;  // 缩放比例显示标签
    QLabel* m_previewHint = nullptr;    // 预览提示标签
    QComboBox* m_filterCombo = nullptr; // 滤镜选择下拉框
    ImageView* m_imageView = nullptr;   // 图像显示控件，支持显示原始图像和滤镜处理后的图像，并处理鼠标事件
};