#include "MainWindow.h"

#include "BMPReader.h"
#include "ImageHistTools.h"
#include "ImageView.h"
#include "ui_MainWindow.h"

#include <QComboBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMimeData>
#include <QPixmap>
#include <QPushButton>
#include <QStatusBar>
#include <QUrl>

#include <algorithm>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("BMP / JPG 图片查看器（C++ / Qt 版）"));
    resize(1280, 800);
    setMinimumSize(1050, 680);
    setAcceptDrops(true);
    createUi();
}

MainWindow::~MainWindow() = default;

void MainWindow::createUi() {
    ui = std::make_unique<Ui::MainWindow>();
    ui->setupUi(this);

    m_fileList = ui->fileListWidget;
    m_infoLabel = ui->infoLabel;
    m_histogramLabel = ui->histogramLabel;
    m_folderLabel = ui->folderLabel;
    m_zoomLabel = ui->zoomLabel;
    m_previewHint = ui->previewHintLabel;
    m_filterCombo = ui->filterComboBox;
    m_imageView = ui->imageView;

    connect(ui->selectFolderButton, &QPushButton::clicked, this, &MainWindow::selectFolder);
    connect(ui->openFileButton, &QPushButton::clicked, this, &MainWindow::openSingleFile);
    connect(ui->zoomInButton, &QPushButton::clicked, this, &MainWindow::zoomIn);
    connect(ui->zoomOutButton, &QPushButton::clicked, this, &MainWindow::zoomOut);
    connect(ui->resetZoomButton, &QPushButton::clicked, this, &MainWindow::resetZoom);
    connect(ui->linearStretchButton, &QPushButton::clicked, this, &MainWindow::doLinearStretch);
    connect(ui->equalizeButton, &QPushButton::clicked, this, &MainWindow::doEqualize);
    connect(ui->histMatchButton, &QPushButton::clicked, this, &MainWindow::doHistMatch);

    connect(m_fileList, &QListWidget::itemClicked, this, &MainWindow::onFileSelected);
    connect(m_filterCombo, &QComboBox::currentTextChanged, this, &MainWindow::onFilterChanged);
    connect(m_imageView, &ImageView::hoverPixel, this, &MainWindow::updateHoverInfo);
    connect(m_imageView, &ImageView::clickPixel, this, &MainWindow::showPixelInfo);
    connect(m_imageView, &ImageView::hoverOutside, this, &MainWindow::updateHoverOutside);
    connect(m_imageView, &ImageView::zoomRequested, this, &MainWindow::handleZoomWheel);

    ui->mainSplitter->setStretchFactor(0, 0);
    ui->mainSplitter->setStretchFactor(1, 1);
    ui->mainSplitter->setSizes({420, 980});
    ui->leftInnerSplitter->setSizes({520, 180});

    statusBar()->showMessage(QStringLiteral("状态：就绪"));
}

bool MainWindow::isSupportedImageFile(const QString& filePath) const {
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return suffix == "bmp" || suffix == "jpg" || suffix == "jpeg";
}

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

void MainWindow::dropEvent(QDropEvent* event) {
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

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
        m_currentFolder = info.absolutePath();
        m_folderLabel->setText(QStringLiteral("当前文件夹：") + m_currentFolder);
        loadImageFiles(m_currentFolder);

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

void MainWindow::selectFolder() {
    QString folder = QFileDialog::getExistingDirectory(this, QStringLiteral("请选择图像文件夹"));
    if (folder.isEmpty()) {
        return;
    }

    m_currentFolder = folder;
    m_folderLabel->setText(QStringLiteral("当前文件夹：") + folder);
    loadImageFiles(folder);
}

void MainWindow::openSingleFile() {
    QString filePath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("打开图像文件"),
        QString(),
        QStringLiteral("图像文件 (*.bmp *.jpg *.jpeg);;所有文件 (*.*)")
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

    displayImage(filePath);
}

void MainWindow::loadImageFiles(const QString& folder) {
    m_fileList->clear();

    QDir dir(folder);
    QStringList files = dir.entryList(
        {"*.bmp", "*.BMP", "*.jpg", "*.JPG", "*.jpeg", "*.JPEG"},
        QDir::Files,
        QDir::Name
    );

    for (const auto& f : files) {
        m_fileList->addItem(f);
    }

    statusBar()->showMessage(QStringLiteral("状态：在文件夹中找到 %1 个支持的图像文件").arg(files.size()));
    if (files.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("该文件夹中没有 BMP / JPG / JPEG 文件"));
    }
}

void MainWindow::onFileSelected(QListWidgetItem* item) {
    if (!item || m_currentFolder.isEmpty()) {
        return;
    }

    displayImage(QDir(m_currentFolder).filePath(item->text()));
}

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
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("暂不支持该图像格式"));
        return;
    }

    m_currentFile = filePath;
    m_originalImage = image;
    m_zoom = 1.0;

    QString compressionText = (suffix == "bmp")
        ? QString::number(m_currentCompression)
        : QStringLiteral("由 Qt 内部解码");

    m_infoLabel->setText(
        QStringLiteral("文件：%1\n格式：%2\n尺寸：%3 x %4\n位深：%5 位\n压缩/解码：%6\n像素总数：%7")
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

void MainWindow::renderCurrentImage() {
    if (m_originalImage.isNull()) {
        return;
    }

    m_filteredImage = applyFilter(m_originalImage, m_filterCombo->currentText());
    m_imageView->setImages(m_originalImage, m_filteredImage);
    m_imageView->setZoomFactor(m_zoom);
    m_zoomLabel->setText(QStringLiteral("%1%").arg(static_cast<int>(m_zoom * 100)));
    updateHistogram();
}

void MainWindow::onFilterChanged() {
    if (m_originalImage.isNull()) {
        return;
    }

    renderCurrentImage();
}

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

void MainWindow::resetZoom() {
    if (m_originalImage.isNull()) {
        return;
    }

    m_zoom = 1.0;
    m_imageView->setZoomFactor(m_zoom);
    m_zoomLabel->setText(QStringLiteral("100%"));
}

void MainWindow::updateHoverInfo(int x, int y, QRgb rgba) {
    QRgb filteredRgba = rgba;

    if (!m_filteredImage.isNull() &&
        x >= 0 && y >= 0 &&
        x < m_filteredImage.width() &&
        y < m_filteredImage.height()) {
        filteredRgba = m_filteredImage.pixel(x, y);
    }

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
            float rr = 0;
            float gg = 0;
            float bb = 0;
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

int MainWindow::clampToByte(int value) {
    return std::max(0, std::min(255, value));
}

void MainWindow::updateHistogram() {
    if (m_originalImage.isNull()) {
        if (m_histogramLabel) {
            m_histogramLabel->setPixmap(QPixmap());
            m_histogramLabel->setText(QStringLiteral("暂无直方图"));
        }
        return;
    }

    if (!m_histogramLabel) {
        return;
    }

    const QImage& source = m_filteredImage.isNull() ? m_originalImage : m_filteredImage;
    QPixmap hist = ImageHistTools::drawGrayHistogram(source, 360, 180);
    m_histogramLabel->setPixmap(hist);
    m_histogramLabel->setText(QString());
}

void MainWindow::handleZoomWheel(int delta) {
    if (delta > 0) {
        zoomIn();
    } else if (delta < 0) {
        zoomOut();
    }
}

void MainWindow::doLinearStretch() {
    if (m_originalImage.isNull()) {
        return;
    }

    QImage res = ImageHistTools::linearStretch(m_originalImage);
    m_filteredImage = res;
    m_imageView->setImages(m_originalImage, m_filteredImage);
    m_imageView->setZoomFactor(m_zoom);
    m_zoomLabel->setText(QStringLiteral("%1%").arg(static_cast<int>(m_zoom * 100)));
    updateHistogram();
}

void MainWindow::doEqualize() {
    if (m_originalImage.isNull()) {
        return;
    }

    QImage res = ImageHistTools::equalizeHist(m_originalImage);
    m_filteredImage = res;
    m_imageView->setImages(m_originalImage, m_filteredImage);
    m_imageView->setZoomFactor(m_zoom);
    m_zoomLabel->setText(QStringLiteral("%1%").arg(static_cast<int>(m_zoom * 100)));
    updateHistogram();
}

void MainWindow::doHistMatch() {
    if (m_originalImage.isNull()) {
        return;
    }

    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择目标颜色图"));
    if (path.isEmpty()) {
        return;
    }

    QImage target(path);
    if (target.isNull()) {
        return;
    }

    QImage res = ImageHistTools::matchHist(m_originalImage, target);
    m_filteredImage = res;
    m_imageView->setImages(m_originalImage, m_filteredImage);
    m_imageView->setZoomFactor(m_zoom);
    m_zoomLabel->setText(QStringLiteral("%1%").arg(static_cast<int>(m_zoom * 100)));
    updateHistogram();
}
