#include "MainWindow.h"
#include "BMPReader.h"
#include "ImageView.h"
#include "ImageHistTools.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

// 构造函数，初始化主窗口
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("BMP / JPG 图像查看器（C++ / Qt 版）"));
    resize(1280, 800);
    setMinimumSize(1050, 680);
    setAcceptDrops(true);   // 允许拖拽文件到窗口

    createUi(); // 创建用户界面
}

void MainWindow::createUi() {
    auto* central = new QWidget(this);  // 创建一个中央部件
    central->setStyleSheet("background:#edf2f7;");  // 背景颜色
    setCentralWidget(central);  // 中心部件

    auto* rootLayout = new QVBoxLayout(central);    // 垂直分布
    rootLayout->setContentsMargins(8, 8, 8, 8); // 布局边距
    rootLayout->setSpacing(8);  // 元素间距

    auto* header = new QWidget(central);    // 标题
    header->setStyleSheet("background:#2563eb;border-radius:10px;");    // 蓝色+圆角
    header->setFixedHeight(64); // 固定高度
    auto* headerLayout = new QVBoxLayout(header);   // 从上往下排
    headerLayout->setContentsMargins(18, 10, 18, 10);

    auto* title = new QLabel(QStringLiteral("BMP / JPG 图像查看器"), header);   // 标题
    title->setStyleSheet("color:white;font-size:22px;font-weight:700;");    // 文字样式
    // 垂直居中
    headerLayout->addStretch(1);
    headerLayout->addWidget(title);
    headerLayout->addStretch(1);

    rootLayout->addWidget(header);  // 置于顶部

    auto* mainSplitter = new QSplitter(Qt::Horizontal, central);    // 水平分布
    mainSplitter->setChildrenCollapsible(false);    // 拉伸限制
    rootLayout->addWidget(mainSplitter, 1); // 占满空间

    // 左侧面板：工具栏 + 文件列表 + 图像信息
    auto* leftPanel = new QWidget(mainSplitter);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    //工具栏
    auto* toolbarCard = new QWidget(leftPanel);
    toolbarCard->setStyleSheet("background:white;border:1px solid #dbe3ee;border-radius:10px;");
    auto* toolbarLayout = new QVBoxLayout(toolbarCard);
    toolbarLayout->setContentsMargins(10, 10, 10, 10);
    toolbarLayout->setSpacing(8);

    //工具栏第一行
    auto* row1 = new QWidget(toolbarCard);
    auto* row1Layout = new QHBoxLayout(row1);
    row1Layout->setContentsMargins(0, 0, 0, 0);
    row1Layout->setSpacing(6);

    //创建按键
    auto makeButton = [&](const QString& text, auto slot) {
        auto* btn = new QPushButton(text, row1);
        btn->setCursor(Qt::PointingHandCursor); // 鼠标变为手形
        btn->setStyleSheet(
            "QPushButton{background:#f8fafc;color:#1e293b;border:1px solid #dbe3ee;border-radius:8px;padding:8px 12px;}"
            "QPushButton:hover{background:#dbeafe;}"
        );
        connect(btn, &QPushButton::clicked, this, slot);    // 连接至对应槽函数
        row1Layout->addWidget(btn); // 添加到第一行布局
    };

    makeButton(QStringLiteral("选择文件夹"), &MainWindow::selectFolder);
    makeButton(QStringLiteral("打开图片"), &MainWindow::openSingleFile);
    makeButton(QStringLiteral("放大"), &MainWindow::zoomIn);
    makeButton(QStringLiteral("缩小"), &MainWindow::zoomOut);
    makeButton(QStringLiteral("原始大小"), &MainWindow::resetZoom);
    makeButton(QStringLiteral("直方图"), &MainWindow::showHistogram);
    makeButton(QStringLiteral("线性增强"), &MainWindow::doLinearStretch);
    makeButton(QStringLiteral("均衡化"), &MainWindow::doEqualize);
    makeButton(QStringLiteral("直方图匹配"), &MainWindow::doHistMatch);

    toolbarLayout->addWidget(row1); // 将第一行添加到工具栏布局中，使其显示在工具栏的顶部

    // 当前文件夹单独一行
    auto* row2 = new QWidget(toolbarCard);
    auto* row2Layout = new QHBoxLayout(row2);
    row2Layout->setContentsMargins(0, 0, 0, 0);
    row2Layout->setSpacing(8);

    m_folderLabel = new QLabel(QStringLiteral("当前文件夹：未选择"), row2);
    m_folderLabel->setStyleSheet("color:#475569;font-size:13px;");
    m_folderLabel->setWordWrap(true);   //自动换行
    row2Layout->addWidget(m_folderLabel, 1);

    toolbarLayout->addWidget(row2); //第二行

    // 滤镜和缩放单独一行
    auto* row3 = new QWidget(toolbarCard);
    auto* row3Layout = new QHBoxLayout(row3);
    row3Layout->setContentsMargins(0, 0, 0, 0);
    row3Layout->setSpacing(8);

    auto* filterLabel = new QLabel(QStringLiteral("滤镜："), row3);
    filterLabel->setStyleSheet("color:#1e293b;font-size:14px;");
    row3Layout->addWidget(filterLabel);

    m_filterCombo = new QComboBox();

    // 添加选项
    m_filterCombo->addItems({
        QStringLiteral("原图"),
        QStringLiteral("灰度"),
        QStringLiteral("反相"),
        QStringLiteral("二值化"),
        QStringLiteral("暖色"),
        QStringLiteral("冷色"),
        QStringLiteral("边缘增强"),
        QStringLiteral("锐化")
    });

    m_filterCombo->setStyleSheet(R"(
        QComboBox {
            background:#f8fafc;
            color:#000000;
            border:1px solid #dbe3ee;
            border-radius:8px;
            padding:6px 10px;
            min-width:120px;
        }
        QComboBox QAbstractItemView {
            color:#000000;
            background:#ffffff;
            selection-color:#000000;
            selection-background-color:#e0e0e0;
        }
    )");

    connect(m_filterCombo, &QComboBox::currentTextChanged, this, &MainWindow::onFilterChanged);

    row3Layout->addWidget(m_filterCombo);

    m_zoomLabel = new QLabel(QStringLiteral("100%"), row3);
    m_zoomLabel->setStyleSheet("color:#1e293b;font-size:14px;font-weight:600;");
    row3Layout->addWidget(m_zoomLabel);

    row3Layout->addStretch(1); //靠左显示

    toolbarLayout->addWidget(row3); //第三行

    leftLayout->addWidget(toolbarCard, 0);  // 固定空间

    //第二部分和第三部分分割
    auto* leftInnerSplitter = new QSplitter(Qt::Vertical, leftPanel);
    leftInnerSplitter->setChildrenCollapsible(false);   // 不可折叠文字
    leftLayout->addWidget(leftInnerSplitter, 1);

    auto* listCard = new QWidget(leftInnerSplitter);
    listCard->setStyleSheet("background:white;border:1px solid #dbe3ee;border-radius:10px;");
    auto* listLayout = new QVBoxLayout(listCard);
    listLayout->setContentsMargins(12, 12, 12, 12);
    listLayout->setSpacing(8);

    auto* listTitle = new QLabel(QStringLiteral("图片文件列表"), listCard);
    listTitle->setStyleSheet("color:#1e293b;font-size:15px;font-weight:700;");
    listLayout->addWidget(listTitle);

    m_fileList = new QListWidget(listCard); // 列表
    m_fileList->setStyleSheet(
        "QListWidget{background:#f8fafc;color:#1e293b;border:none;border-radius:8px;padding:6px;font-family:Consolas;}"
        "QListWidget::item:selected{background:#bfdbfe;color:#0f172a;border-radius:6px;}"
        );
    connect(m_fileList, &QListWidget::itemClicked, this, &MainWindow::onFileSelected);
    listLayout->addWidget(m_fileList, 1);

    auto* infoCard = new QWidget(leftInnerSplitter);    //图像信息
    infoCard->setStyleSheet("background:white;border:1px solid #dbe3ee;border-radius:10px;");
    auto* infoLayout = new QVBoxLayout(infoCard);
    infoLayout->setContentsMargins(12, 12, 12, 12);
    infoLayout->setSpacing(8);

    auto* infoTitle = new QLabel(QStringLiteral("图像信息"), infoCard);
    infoTitle->setStyleSheet("color:#1e293b;font-size:15px;font-weight:700;");
    infoLayout->addWidget(infoTitle);

    m_infoLabel = new QLabel(QStringLiteral("图像信息：未加载文件"), infoCard);
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);    // 顶部+左对齐
    m_infoLabel->setStyleSheet("color:#475569;font-size:13px;");
    infoLayout->addWidget(m_infoLabel, 1);

    leftInnerSplitter->setSizes({520, 180});    //左侧分割器初始大小

    //右侧面板
    auto* rightPanel = new QWidget(mainSplitter);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    auto* canvasCard = new QWidget(rightPanel); // 画布
    canvasCard->setStyleSheet("background:white;border:1px solid #dbe3ee;border-radius:10px;");
    auto* canvasLayout = new QVBoxLayout(canvasCard);
    canvasLayout->setContentsMargins(6, 6, 6, 6);
    canvasLayout->setSpacing(6);

    auto* canvasTop = new QWidget(canvasCard);
    canvasTop->setStyleSheet("background:#f8fafc;border-radius:8px;");
    canvasTop->setFixedHeight(30);
    auto* canvasTopLayout = new QHBoxLayout(canvasTop);
    canvasTopLayout->setContentsMargins(10, 3, 10, 3);

    auto* previewTitle = new QLabel(QStringLiteral("图像预览"), canvasTop);
    previewTitle->setStyleSheet("color:#1e293b;font-size:14px;font-weight:700;");

    m_previewHint = new QLabel(QStringLiteral("拖拽平移｜滚轮滚动｜点击查看像素｜支持拖拽图片打开"), canvasTop);
    m_previewHint->setStyleSheet("color:#475569;font-size:12px;");

    canvasTopLayout->addWidget(previewTitle); 
    canvasTopLayout->addStretch(1);
    canvasTopLayout->addWidget(m_previewHint);

    canvasLayout->addWidget(canvasTop);

    m_imageView = new ImageView(canvasCard);
    connect(m_imageView, &ImageView::hoverPixel, this, &MainWindow::updateHoverInfo);
    connect(m_imageView, &ImageView::clickPixel, this, &MainWindow::showPixelInfo);
    canvasLayout->addWidget(m_imageView, 1);

    rightLayout->addWidget(canvasCard, 1); 

    mainSplitter->addWidget(leftPanel); // 将左侧面板添加到主分割器中，使其显示在窗口的左侧
    mainSplitter->addWidget(rightPanel);    // 将右侧面板添加到主分割器中，使其显示在窗口的右侧
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 1); 
    mainSplitter->setSizes({420, 980});
}

bool MainWindow::isSupportedImageFile(const QString& filePath) const {
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return suffix == "bmp" || suffix == "jpg" || suffix == "jpeg";
}

// 拖拽进入事件处理，判断是否接受拖拽
void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl& url : urls) {
        if (url.isLocalFile() && isSupportedImageFile(url.toLocalFile())) {
            event->acceptProposedAction();
            return;
        }
    }

    event->ignore();
}

// 拖拽释放事件处理，获取文件路径并加载图像
void MainWindow::dropEvent(QDropEvent* event) {
    //判断是否有路径
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

    //遍历路径，找到第一个支持的图片文件进行加载
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl& url : urls) {
        if (!url.isLocalFile()) {
            continue;
        }

        const QString filePath = url.toLocalFile();
        if (!isSupportedImageFile(filePath)) {
            continue;
        }

        QFileInfo info(filePath);
        m_currentFolder = info.absolutePath();  //更新图片文件列表
        m_folderLabel->setText(QStringLiteral("当前文件夹：") + m_currentFolder);   //更新文件夹路径
        loadImageFiles(m_currentFolder);

        //匹配文件列表项并选中
        QList<QListWidgetItem*> items = m_fileList->findItems(info.fileName(), Qt::MatchExactly);
        if (!items.isEmpty()) {
            m_fileList->setCurrentItem(items.first());
        }

        displayImage(filePath);
        event->acceptProposedAction();
        return;
    }

    event->ignore();
}

// 选择文件夹按钮的槽函数
void MainWindow::selectFolder() {
    QString folder = QFileDialog::getExistingDirectory(this, QStringLiteral("请选择图片文件夹"));
    if (folder.isEmpty()) {
        return;
    }

    m_currentFolder = folder;
    m_folderLabel->setText(QStringLiteral("当前文件夹：") + folder);
    loadImageFiles(folder);
}

// 打开图片按钮的槽函数
void MainWindow::openSingleFile() {
    QString filePath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("打开图片文件"),
        QString(),
        QStringLiteral("图片文件 (*.bmp *.jpg *.jpeg);;所有文件 (*.*)")
    );  
    if (filePath.isEmpty()) {
        return;
    }

    QFileInfo info(filePath);
    m_currentFolder = info.absolutePath();
    m_folderLabel->setText(QStringLiteral("当前文件夹：") + m_currentFolder);
    loadImageFiles(m_currentFolder);

    QList<QListWidgetItem*> items = m_fileList->findItems(info.fileName(), Qt::MatchExactly);
    if (!items.isEmpty()) {
        m_fileList->setCurrentItem(items.first());
    }

    displayImage(filePath); //高亮
}

// 加载图片文件列表
void MainWindow::loadImageFiles(const QString& folder) {
    m_fileList->clear();

    QDir dir(folder);
    QStringList files = dir.entryList(
        {"*.bmp", "*.BMP", "*.jpg", "*.JPG", "*.jpeg", "*.JPEG"},
        QDir::Files,
        QDir::Name
    );

    //显示文件名
    for (const auto& f : files) {
        m_fileList->addItem(f);
    }

    //反馈
    statusBar()->showMessage(QStringLiteral("状态：在文件夹中找到 %1 个支持的图片文件").arg(files.size()));
    if (files.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("该文件夹中没有 BMP / JPG / JPEG 文件"));
    }
}

// 图片文件列表项被点击时的槽函数
void MainWindow::onFileSelected(QListWidgetItem* item) {
    if (!item || m_currentFolder.isEmpty()) {
        return;
    }

    displayImage(QDir(m_currentFolder).filePath(item->text()));
}

//图片信息展示
void MainWindow::displayImage(const QString& filePath) {
    QImage image;
    QString error;
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();

    if (suffix == "bmp") {
        BMPReader reader(filePath);
        if (!reader.load(&error)) {
            QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("打开 BMP 文件失败：\n") + error);
            return;
        }

        image = reader.decode(&error);
        if (image.isNull()) {
            QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("解码 BMP 失败：\n") + error);
            return;
        }

        const auto& info = reader.info();
        m_currentWidth = info.width;
        m_currentHeight = info.height;
        m_currentBitCount = info.bitCount;
        m_currentCompression = static_cast<int>(info.compression);
    } else if (suffix == "jpg" || suffix == "jpeg") {
        image = QImage(filePath);
        if (image.isNull()) {
            QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("打开 JPG 文件失败"));
            return;
        }

        m_currentWidth = image.width();
        m_currentHeight = image.height();
        m_currentBitCount = image.depth();
        m_currentCompression = 0;
    } else {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("暂不支持该图片格式"));
        return;
    }

    m_currentFile = filePath;
    m_originalImage = image;
    m_zoom = 1.0;

    QString compressionText;
    if (suffix == "bmp") {
        compressionText = QString::number(m_currentCompression);
    } else {
        compressionText = QStringLiteral("由 Qt 内部解码");
    }

    m_infoLabel->setText(
        QStringLiteral("文件：%1\n格式：%2\n尺寸：%3 × %4\n位深：%5 位\n压缩/解码：%6\n像素总数：%7")
            .arg(QFileInfo(filePath).fileName())
            .arg(suffix.toUpper())
            .arg(m_currentWidth)
            .arg(m_currentHeight)
            .arg(m_currentBitCount)
            .arg(compressionText)
            .arg(static_cast<long long>(m_currentWidth) * m_currentHeight)
    );

    renderCurrentImage();
    statusBar()->showMessage(QStringLiteral("状态：已加载 %1").arg(QFileInfo(filePath).fileName()));
}

// 滤镜渲染
void MainWindow::renderCurrentImage() {
    if (m_originalImage.isNull()) {
        return;
    }

    m_filteredImage = applyFilter(m_originalImage, m_filterCombo->currentText());
    m_imageView->setImages(m_originalImage, m_filteredImage);
    m_imageView->setZoomFactor(m_zoom);
    m_zoomLabel->setText(QStringLiteral("%1%").arg(static_cast<int>(m_zoom * 100)));
}

// 滤镜选项改变时的槽函数
void MainWindow::onFilterChanged() {
    if (m_originalImage.isNull()) {
        return;
    }
    renderCurrentImage();
}

// 放大按钮的槽函数
void MainWindow::zoomIn() {
    if (m_originalImage.isNull()) {
        return;
    }

    double newZoom = qRound((m_zoom + m_zoomStep) * 100.0) / 100.0;
    if (newZoom <= m_maxZoom) {
        m_zoom = newZoom;
        m_imageView->setZoomFactor(m_zoom);
        m_zoomLabel->setText(QStringLiteral("%1%").arg(static_cast<int>(m_zoom * 100)));
    }
}

// 缩小按钮的槽函数
void MainWindow::zoomOut() {
    if (m_originalImage.isNull()) {
        return;
    }

    double newZoom = qRound((m_zoom - m_zoomStep) * 100.0) / 100.0; 
    if (newZoom >= m_minZoom) {
        m_zoom = newZoom;
        m_imageView->setZoomFactor(m_zoom);
        m_zoomLabel->setText(QStringLiteral("%1%").arg(static_cast<int>(m_zoom * 100)));
    }
}

// 重置缩放按钮的槽函数
void MainWindow::resetZoom() {
    if (m_originalImage.isNull()) {
        return;
    }

    m_zoom = 1.0;
    m_imageView->setZoomFactor(m_zoom);
    m_zoomLabel->setText(QStringLiteral("100%"));
}

// 更新状态栏显示当前鼠标悬停位置的像素信息
void MainWindow::updateHoverInfo(int x, int y, QRgb rgba) {
    QRgb filteredRgba = rgba;

    if (!m_filteredImage.isNull() &&
        x >= 0 && y >= 0 &&
        x < m_filteredImage.width() &&
        y < m_filteredImage.height()) {
        filteredRgba = m_filteredImage.pixel(x, y);
    }

    //刷新状态栏
    statusBar()->showMessage(
        QStringLiteral(
            "状态：文件=%1 | 坐标=(%2, %3) | 原始 RGBA=(%4, %5, %6, %7) | 滤镜后 RGBA=(%8, %9, %10, %11) | 滤镜=%12 | 缩放=%13%"
        )
            .arg(QFileInfo(m_currentFile).fileName())
            .arg(x)
            .arg(y)
            .arg(qRed(rgba))
            .arg(qGreen(rgba))
            .arg(qBlue(rgba))
            .arg(qAlpha(rgba))
            .arg(qRed(filteredRgba))
            .arg(qGreen(filteredRgba))
            .arg(qBlue(filteredRgba))
            .arg(qAlpha(filteredRgba))
            .arg(m_filterCombo->currentText())
            .arg(static_cast<int>(m_zoom * 100))
    );
}

void MainWindow::updateHoverOutside() {
    statusBar()->showMessage(
        QStringLiteral("状态：文件=%1 | 鼠标未在图像范围内")
            .arg(m_currentFile.isEmpty() ? QStringLiteral("无") : QFileInfo(m_currentFile).fileName())
    );
}

//小窗信息展示
void MainWindow::showPixelInfo(int x, int y, QRgb rgba) {
    QRgb filteredRgba = rgba;

    if (!m_filteredImage.isNull() &&
        x >= 0 && y >= 0 &&
        x < m_filteredImage.width() &&
        y < m_filteredImage.height()) {
        filteredRgba = m_filteredImage.pixel(x, y);
    }

    QMessageBox::information(
        this,
        QStringLiteral("像素信息"),
        QStringLiteral(
            "文件：%1\n"
            "坐标：(%2, %3)\n"
            "原始颜色 RGBA：(%4, %5, %6, %7)\n"
            "滤镜后颜色 RGBA：(%8, %9, %10, %11)\n"
            "当前滤镜：%12\n"
            "缩放：%13%"
        )
            .arg(QFileInfo(m_currentFile).fileName())
            .arg(x)
            .arg(y)
            .arg(qRed(rgba))
            .arg(qGreen(rgba))
            .arg(qBlue(rgba))
            .arg(qAlpha(rgba))
            .arg(qRed(filteredRgba))
            .arg(qGreen(filteredRgba))
            .arg(qBlue(filteredRgba))
            .arg(qAlpha(filteredRgba))
            .arg(m_filterCombo->currentText())
            .arg(static_cast<int>(m_zoom * 100))
    );
}

//滤镜细节处理
QImage MainWindow::applyFilter(const QImage& img, const QString& filterName) const {
    QImage src = img.convertToFormat(QImage::Format_RGBA8888);

    if (filterName == QStringLiteral("原图")) {
        return src.copy();
    }

    QImage out = src.copy();
    const int w = out.width();
    const int h = out.height();

    if (filterName == QStringLiteral("灰度")) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                QColor c = out.pixelColor(x, y);
                int gray = qGray(c.rgb());
                out.setPixelColor(x, y, QColor(gray, gray, gray, c.alpha()));
            }
        }
        return out;
    }

    if (filterName == QStringLiteral("反相")) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                QColor c = out.pixelColor(x, y);
                out.setPixelColor(x, y, QColor(255 - c.red(), 255 - c.green(), 255 - c.blue(), c.alpha()));
            }
        }
        return out;
    }

    if (filterName == QStringLiteral("二值化")) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                QColor c = out.pixelColor(x, y);
                int gray = qGray(c.rgb());
                int bw = gray > 128 ? 255 : 0;
                out.setPixelColor(x, y, QColor(bw, bw, bw, c.alpha()));
            }
        }
        return out;
    }

    if (filterName == QStringLiteral("暖色") || filterName == QStringLiteral("冷色")) {
        const double rFactor = (filterName == QStringLiteral("暖色")) ? 1.15 : 0.90;
        const double gFactor = (filterName == QStringLiteral("暖色")) ? 1.05 : 1.00;
        const double bFactor = (filterName == QStringLiteral("暖色")) ? 0.90 : 1.15;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                QColor c = out.pixelColor(x, y);
                out.setPixelColor(x, y, QColor(
                    clampToByte(static_cast<int>(c.red() * rFactor)),
                    clampToByte(static_cast<int>(c.green() * gFactor)),
                    clampToByte(static_cast<int>(c.blue() * bFactor)),
                    c.alpha()
                ));
            }
        }
        return out;
    }

    if (filterName == QStringLiteral("边缘增强")) {
        return convolve(src, {0, -1, 0, -1, 5, -1, 0, -1, 0}, 1.0f);
    }

    if (filterName == QStringLiteral("锐化")) {
        return convolve(src, {-1, -1, -1, -1, 9, -1, -1, -1, -1}, 1.0f);
    }

    return src.copy();
}

//3*3卷积
QImage MainWindow::convolve(const QImage& img, const QVector<float>& kernel, float divisor, float bias) const {
    QImage src = img.convertToFormat(QImage::Format_RGBA8888);
    QImage out = src.copy();
    const int w = src.width();
    const int h = src.height();

    auto sample = [&](int x, int y) {
        x = std::clamp(x, 0, w - 1);
        y = std::clamp(y, 0, h - 1);
        return QColor::fromRgba(src.pixel(x, y));
    };

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float rr = 0, gg = 0, bb = 0;
            QColor center = QColor::fromRgba(src.pixel(x, y));
            int k = 0;
            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    QColor c = sample(x + kx, y + ky);
                    float kv = kernel[k++];
                    rr += c.red() * kv;
                    gg += c.green() * kv;
                    bb += c.blue() * kv;
                }
            }
            rr = rr / divisor + bias;
            gg = gg / divisor + bias;
            bb = bb / divisor + bias;
            out.setPixel(x, y, qRgba(
                clampToByte(static_cast<int>(rr)),
                clampToByte(static_cast<int>(gg)),
                clampToByte(static_cast<int>(bb)),
                center.alpha()
            ));
        }
    }
    return out;
}

// 将整数值限制在0到255之间，确保像素值在有效范围内
int MainWindow::clampToByte(int value) {
    return std::max(0, std::min(255, value));
}

//直方图
void MainWindow::showHistogram()
{
    if (m_originalImage.isNull())
        return;

    QPixmap hist = ImageHistTools::drawGrayHistogram(m_originalImage);

    QLabel *label = new QLabel();
    label->setPixmap(hist);
    label->setWindowTitle("灰度直方图");
    label->show();
}

//线性增强
void MainWindow::doLinearStretch()
{
    if (m_originalImage.isNull())
        return;

    QImage res = ImageHistTools::linearStretch(m_originalImage);
    m_filteredImage = res;
    m_imageView->setImages(m_originalImage, m_filteredImage);
}

//均衡化
void MainWindow::doEqualize()
{
    if (m_originalImage.isNull())
        return;

    QImage res = ImageHistTools::equalizeHist(m_originalImage);
    m_filteredImage = res;
    m_imageView->setImages(m_originalImage, m_filteredImage);
}

//直方图匹配
void MainWindow::doHistMatch()
{
    if (m_originalImage.isNull())
        return;

    QString path = QFileDialog::getOpenFileName(this, "选择目标颜色图");
    if (path.isEmpty()) return;

    QImage target(path);
    if (target.isNull()) return;

    QImage res = ImageHistTools::matchHist(m_originalImage, target);
    m_filteredImage = res;
    m_imageView->setImages(m_originalImage, m_filteredImage);
}