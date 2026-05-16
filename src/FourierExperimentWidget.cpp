#include "FourierExperimentWidget.h"

#include "BMPReader.h"

#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImageWriter>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr double kPi = 3.14159265358979323846;

QLabel* makeTitleLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("SectionTitle"));
    return label;
}

QFrame* makeCard(QWidget* parent) {
    auto* frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("Card"));
    return frame;
}
}

FourierExperimentWidget::FourierExperimentWidget(QWidget* parent) : QWidget(parent) {
    buildUi();
    setStatus(QString::fromUtf8(u8"状态：请加载图像后开始实验四"));
}

void FourierExperimentWidget::buildUi() {
    setObjectName(QStringLiteral("fourierExperimentWidget"));
    setStyleSheet(R"(
        #fourierExperimentWidget {
            background: #f5f8ff;
        }
        QFrame#Card {
            background: #ffffff;
            border: 1px solid #d7e3f5;
            border-radius: 12px;
        }
        QLabel#SectionTitle {
            color: #0f172a;
            font-weight: 700;
        }
        QLabel#SmallLabel {
            color: #52637a;
        }
        QLabel#ImagePanel {
            background: #0f172a;
            color: #dbeafe;
            border: 1px solid #d7e3f5;
            border-radius: 10px;
        }
        QLineEdit, QSpinBox, QDoubleSpinBox, QTextEdit {
            color: #0f172a;
            background: #ffffff;
            border: 1px solid #cfdcf0;
            border-radius: 8px;
            padding: 6px 8px;
        }
        QPushButton {
            color: #0f172a;
            background: #f8fbff;
            border: 1px solid #cfdcf0;
            border-radius: 8px;
            padding: 8px 12px;
        }
        QPushButton:hover {
            background: #e6f0ff;
        }
        QPushButton:disabled {
            color: #94a3b8;
            background: #f1f5f9;
        }
    )");

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(10);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget(scrollArea);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(12, 12, 12, 12);
    contentLayout->setSpacing(12);

    auto* headerCard = makeCard(content);
    auto* headerLayout = new QVBoxLayout(headerCard);
    headerLayout->setContentsMargins(16, 14, 16, 14);
    headerLayout->setSpacing(6);
    auto* title = makeTitleLabel(QString::fromUtf8(u8"实验四：图像的二维傅里叶变换及变换域滤波"), headerCard);
    auto* subtitle = new QLabel(
        QString::fromUtf8(u8"实现 FFT/IFFT、理想/巴特沃斯低通与高通滤波、同态滤波，并对实验图像进行频域处理。"),
        headerCard
    );
    subtitle->setObjectName(QStringLiteral("SmallLabel"));
    subtitle->setWordWrap(true);
    headerLayout->addWidget(title);
    headerLayout->addWidget(subtitle);
    contentLayout->addWidget(headerCard);

    auto* controlCard = makeCard(content);
    auto* controlLayout = new QGridLayout(controlCard);
    controlLayout->setContentsMargins(16, 16, 16, 16);
    controlLayout->setHorizontalSpacing(10);
    controlLayout->setVerticalSpacing(10);

    auto* openButton = new QPushButton(QString::fromUtf8(u8"打开图像"), controlCard);
    auto* petButton = new QPushButton(QString::fromUtf8(u8"加载 PET_image.bmp"), controlCard);
    auto* factoryButton = new QPushButton(QString::fromUtf8(u8"加载 factory.bmp"), controlCard);
    m_pathEdit = new QLineEdit(controlCard);
    m_pathEdit->setReadOnly(true);
    m_pathEdit->setPlaceholderText(QString::fromUtf8(u8"请选择 BMP/JPG/JPEG 图像"));

    m_cutoffSpin = new QSpinBox(controlCard);
    m_cutoffSpin->setRange(1, 2048);
    m_cutoffSpin->setValue(40);
    m_cutoffSpin->setSuffix(QString::fromUtf8(u8" px"));

    m_orderSpin = new QSpinBox(controlCard);
    m_orderSpin->setRange(1, 10);
    m_orderSpin->setValue(2);

    m_gammaLowSpin = new QDoubleSpinBox(controlCard);
    m_gammaLowSpin->setRange(0.05, 2.0);
    m_gammaLowSpin->setDecimals(2);
    m_gammaLowSpin->setSingleStep(0.05);
    m_gammaLowSpin->setValue(0.50);

    m_gammaHighSpin = new QDoubleSpinBox(controlCard);
    m_gammaHighSpin->setRange(0.1, 5.0);
    m_gammaHighSpin->setDecimals(2);
    m_gammaHighSpin->setSingleStep(0.1);
    m_gammaHighSpin->setValue(1.80);

    m_homomorphicCSpin = new QDoubleSpinBox(controlCard);
    m_homomorphicCSpin->setRange(0.1, 10.0);
    m_homomorphicCSpin->setDecimals(2);
    m_homomorphicCSpin->setSingleStep(0.1);
    m_homomorphicCSpin->setValue(1.0);

    auto* fftButton = new QPushButton(QString::fromUtf8(u8"FFT 频谱"), controlCard);
    auto* ifftButton = new QPushButton(QString::fromUtf8(u8"IFFT 重建"), controlCard);
    auto* idealLowButton = new QPushButton(QString::fromUtf8(u8"理想低通"), controlCard);
    auto* butterLowButton = new QPushButton(QString::fromUtf8(u8"巴特沃斯低通"), controlCard);
    auto* idealHighButton = new QPushButton(QString::fromUtf8(u8"理想高通"), controlCard);
    auto* butterHighButton = new QPushButton(QString::fromUtf8(u8"巴特沃斯高通"), controlCard);
    auto* homoButton = new QPushButton(QString::fromUtf8(u8"同态滤波"), controlCard);
    m_saveButton = new QPushButton(QString::fromUtf8(u8"保存结果"), controlCard);
    m_saveButton->setEnabled(false);

    int row = 0;
    controlLayout->addWidget(openButton, row, 0);
    controlLayout->addWidget(petButton, row, 1);
    controlLayout->addWidget(factoryButton, row, 2);
    controlLayout->addWidget(m_pathEdit, row, 3, 1, 5);

    ++row;
    controlLayout->addWidget(new QLabel(QString::fromUtf8(u8"截止半径"), controlCard), row, 0);
    controlLayout->addWidget(m_cutoffSpin, row, 1);
    controlLayout->addWidget(new QLabel(QString::fromUtf8(u8"巴特沃斯阶数"), controlCard), row, 2);
    controlLayout->addWidget(m_orderSpin, row, 3);
    controlLayout->addWidget(new QLabel(QString::fromUtf8(u8"γL"), controlCard), row, 4);
    controlLayout->addWidget(m_gammaLowSpin, row, 5);
    controlLayout->addWidget(new QLabel(QString::fromUtf8(u8"γH"), controlCard), row, 6);
    controlLayout->addWidget(m_gammaHighSpin, row, 7);

    ++row;
    controlLayout->addWidget(new QLabel(QString::fromUtf8(u8"同态 c"), controlCard), row, 0);
    controlLayout->addWidget(m_homomorphicCSpin, row, 1);
    controlLayout->addWidget(fftButton, row, 2);
    controlLayout->addWidget(ifftButton, row, 3);
    controlLayout->addWidget(idealLowButton, row, 4);
    controlLayout->addWidget(butterLowButton, row, 5);
    controlLayout->addWidget(idealHighButton, row, 6);
    controlLayout->addWidget(butterHighButton, row, 7);

    ++row;
    controlLayout->addWidget(homoButton, row, 2);
    controlLayout->addWidget(m_saveButton, row, 3);
    m_imageInfoLabel = new QLabel(QString::fromUtf8(u8"图像信息：未加载"), controlCard);
    m_imageInfoLabel->setObjectName(QStringLiteral("SmallLabel"));
    controlLayout->addWidget(m_imageInfoLabel, row, 4, 1, 4);

    for (int i = 0; i < 8; ++i) {
        controlLayout->setColumnStretch(i, i == 7 ? 1 : 0);
    }
    contentLayout->addWidget(controlCard);

    auto* previewCard = makeCard(content);
    auto* previewLayout = new QGridLayout(previewCard);
    previewLayout->setContentsMargins(16, 16, 16, 16);
    previewLayout->setHorizontalSpacing(12);
    previewLayout->setVerticalSpacing(10);

    auto createImageColumn = [previewCard](const QString& titleText, QLabel** imageLabel) {
        auto* box = new QVBoxLayout();
        box->setSpacing(8);
        auto* titleLabel = makeTitleLabel(titleText, previewCard);
        auto* label = new QLabel(QString::fromUtf8(u8"等待图像"), previewCard);
        label->setObjectName(QStringLiteral("ImagePanel"));
        label->setAlignment(Qt::AlignCenter);
        label->setMinimumSize(280, 260);
        label->setWordWrap(true);
        box->addWidget(titleLabel);
        box->addWidget(label, 1);
        *imageLabel = label;
        return box;
    };

    previewLayout->addLayout(createImageColumn(QString::fromUtf8(u8"原图"), &m_originalLabel), 0, 0);
    previewLayout->addLayout(createImageColumn(QString::fromUtf8(u8"频谱/滤波器频域"), &m_spectrumLabel), 0, 1);
    previewLayout->addLayout(createImageColumn(QString::fromUtf8(u8"结果图像"), &m_resultLabel), 0, 2);
    previewLayout->setColumnStretch(0, 1);
    previewLayout->setColumnStretch(1, 1);
    previewLayout->setColumnStretch(2, 1);
    contentLayout->addWidget(previewCard, 1);

    auto* notesCard = makeCard(content);
    auto* notesLayout = new QVBoxLayout(notesCard);
    notesLayout->setContentsMargins(16, 14, 16, 14);
    notesLayout->setSpacing(8);
    notesLayout->addWidget(makeTitleLabel(QString::fromUtf8(u8"实验记录"), notesCard));
    m_notesEdit = new QTextEdit(notesCard);
    m_notesEdit->setReadOnly(true);
    m_notesEdit->setMinimumHeight(96);
    m_notesEdit->setText(QString::fromUtf8(
        u8"1. FFT/IFFT：对图像乘以 (-1)^(x+y) 后做二维 FFT，使频谱中心移到图像中心。\n"
        u8"2. 理想滤波器：按距离 D 与截止半径 D0 的关系直接保留或抑制频率。\n"
        u8"3. 巴特沃斯滤波器：阶数越高，过渡带越陡，通常比理想滤波器振铃更弱。\n"
        u8"4. 同态滤波：对 log(1+I) 做频域高频增强和低频抑制，再指数反变换。"
    ));
    notesLayout->addWidget(m_notesEdit);
    contentLayout->addWidget(notesCard);

    m_statusLabel = new QLabel(content);
    m_statusLabel->setObjectName(QStringLiteral("SmallLabel"));
    m_statusLabel->setMinimumHeight(26);
    contentLayout->addWidget(m_statusLabel);

    scrollArea->setWidget(content);
    rootLayout->addWidget(scrollArea);

    connect(openButton, &QPushButton::clicked, this, &FourierExperimentWidget::openImage);
    connect(petButton, &QPushButton::clicked, this, &FourierExperimentWidget::loadPetImage);
    connect(factoryButton, &QPushButton::clicked, this, &FourierExperimentWidget::loadFactoryImage);
    connect(fftButton, &QPushButton::clicked, this, &FourierExperimentWidget::runForwardFft);
    connect(ifftButton, &QPushButton::clicked, this, &FourierExperimentWidget::runInverseFft);
    connect(idealLowButton, &QPushButton::clicked, this, &FourierExperimentWidget::applyIdealLowPass);
    connect(butterLowButton, &QPushButton::clicked, this, &FourierExperimentWidget::applyButterworthLowPass);
    connect(idealHighButton, &QPushButton::clicked, this, &FourierExperimentWidget::applyIdealHighPass);
    connect(butterHighButton, &QPushButton::clicked, this, &FourierExperimentWidget::applyButterworthHighPass);
    connect(homoButton, &QPushButton::clicked, this, &FourierExperimentWidget::applyHomomorphicFilter);
    connect(m_saveButton, &QPushButton::clicked, this, &FourierExperimentWidget::saveResult);
}

void FourierExperimentWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    refreshPreviewLabels();
}

void FourierExperimentWidget::openImage() {
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        QString::fromUtf8(u8"打开实验四图像"),
        QString(),
        QString::fromUtf8(u8"图像文件 (*.bmp *.jpg *.jpeg *.png *.tif *.tiff);;所有文件 (*.*)")
    );
    if (!filePath.isEmpty()) {
        loadImageFromPath(filePath);
    }
}

void FourierExperimentWidget::loadPetImage() {
    const QString path = findSampleImage(QStringLiteral("PET_image.bmp"));
    if (path.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"未找到 PET_image.bmp，请手动打开图像。"));
        return;
    }
    loadImageFromPath(path);
}

void FourierExperimentWidget::loadFactoryImage() {
    const QString path = findSampleImage(QStringLiteral("factory.bmp"));
    if (path.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"未找到 factory.bmp，请手动打开图像。"));
        return;
    }
    loadImageFromPath(path);
}

void FourierExperimentWidget::loadImageFromPath(const QString& filePath) {
    QFileInfo info(filePath);
    const QString suffix = info.suffix().toLower();
    QImage image;
    QString error;
    int bitDepth = 0;

    if (suffix == QStringLiteral("bmp")) {
        BMPReader reader(filePath);
        if (!reader.load(&error)) {
            QMessageBox::critical(this, QString::fromUtf8(u8"错误"), QString::fromUtf8(u8"打开 BMP 失败：\n") + error);
            return;
        }
        image = reader.decode(&error);
        if (image.isNull()) {
            QMessageBox::critical(this, QString::fromUtf8(u8"错误"), QString::fromUtf8(u8"解码 BMP 失败：\n") + error);
            return;
        }
        bitDepth = reader.info().bitCount;
    } else {
        image = QImage(filePath);
        if (image.isNull()) {
            QMessageBox::critical(this, QString::fromUtf8(u8"错误"), QString::fromUtf8(u8"无法加载该图像文件。"));
            return;
        }
        bitDepth = image.depth();
    }

    m_currentPath = filePath;
    m_currentBitDepth = bitDepth;
    m_originalImage = image.convertToFormat(QImage::Format_Grayscale8);
    m_spectrum = {};
    m_originalPreview = m_originalImage;
    m_spectrumPreview = {};
    m_resultPreview = {};
    m_saveButton->setEnabled(false);
    m_pathEdit->setText(filePath);
    m_imageInfoLabel->setText(describeCurrentImage());
    refreshPreviewLabels();
    setStatus(QString::fromUtf8(u8"状态：已加载图像，可执行 FFT 或滤波实验"));
}

void FourierExperimentWidget::runForwardFft() {
    if (!ensureSpectrum()) {
        return;
    }
    m_spectrumPreview = spectrumToImage(m_spectrum);
    m_resultPreview = {};
    m_saveButton->setEnabled(false);
    refreshPreviewLabels();
    setStatus(QString::fromUtf8(u8"状态：已完成二维 FFT 并显示中心化幅度谱"));
}

void FourierExperimentWidget::runInverseFft() {
    if (!ensureSpectrum()) {
        return;
    }
    m_spectrumPreview = spectrumToImage(m_spectrum);
    m_resultPreview = inverseToImage(m_spectrum);
    m_saveButton->setEnabled(!m_resultPreview.isNull());
    refreshPreviewLabels();
    setStatus(QString::fromUtf8(u8"状态：已通过 IFFT 从频域重建图像"));
}

void FourierExperimentWidget::applyIdealLowPass() {
    applyFrequencyFilter(FrequencyFilter::IdealLowPass);
}

void FourierExperimentWidget::applyButterworthLowPass() {
    applyFrequencyFilter(FrequencyFilter::ButterworthLowPass);
}

void FourierExperimentWidget::applyIdealHighPass() {
    applyFrequencyFilter(FrequencyFilter::IdealHighPass);
}

void FourierExperimentWidget::applyButterworthHighPass() {
    applyFrequencyFilter(FrequencyFilter::ButterworthHighPass);
}

void FourierExperimentWidget::applyFrequencyFilter(FrequencyFilter filter) {
    if (!ensureSpectrum()) {
        return;
    }

    const double cutoff = static_cast<double>(m_cutoffSpin->value());
    const int order = m_orderSpin->value();
    SpectrumData filtered = filteredSpectrum(m_spectrum, filter, cutoff, order);
    m_spectrumPreview = spectrumToImage(filtered);
    if (filter == FrequencyFilter::IdealHighPass || filter == FrequencyFilter::ButterworthHighPass) {
        m_resultPreview = highBoostImage(m_originalImage, filtered, 1.0);
    } else {
        m_resultPreview = inverseToImage(filtered);
    }
    m_saveButton->setEnabled(!m_resultPreview.isNull());
    refreshPreviewLabels();

    QString name;
    switch (filter) {
    case FrequencyFilter::IdealLowPass:
        name = QString::fromUtf8(u8"理想低通");
        break;
    case FrequencyFilter::ButterworthLowPass:
        name = QString::fromUtf8(u8"巴特沃斯低通");
        break;
    case FrequencyFilter::IdealHighPass:
        name = QString::fromUtf8(u8"理想高通");
        break;
    case FrequencyFilter::ButterworthHighPass:
        name = QString::fromUtf8(u8"巴特沃斯高通");
        break;
    }
    const QString mode = (filter == FrequencyFilter::IdealHighPass || filter == FrequencyFilter::ButterworthHighPass)
        ? QString::fromUtf8(u8"高频分量已叠加回原图用于锐化")
        : QString::fromUtf8(u8"已生成平滑滤波结果");
    setStatus(QString::fromUtf8(u8"状态：已应用 %1，截止半径 D0=%2，阶数 n=%3，%4").arg(name).arg(cutoff).arg(order).arg(mode));
}

void FourierExperimentWidget::applyHomomorphicFilter() {
    if (m_originalImage.isNull()) {
        QMessageBox::warning(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先加载图像。"));
        return;
    }

    QImage spectrumImage;
    m_resultPreview = homomorphicImage(
        m_originalImage,
        static_cast<double>(m_cutoffSpin->value()),
        m_gammaLowSpin->value(),
        m_gammaHighSpin->value(),
        m_homomorphicCSpin->value(),
        &spectrumImage
    );
    m_spectrumPreview = spectrumImage;
    m_saveButton->setEnabled(!m_resultPreview.isNull());
    refreshPreviewLabels();
    setStatus(QString::fromUtf8(u8"状态：已完成同态滤波，低频抑制 γL=%1，高频增强 γH=%2")
        .arg(m_gammaLowSpin->value())
        .arg(m_gammaHighSpin->value()));
}

void FourierExperimentWidget::saveResult() {
    if (m_resultPreview.isNull()) {
        return;
    }

    const QString suggested = QFileInfo(m_currentPath).completeBaseName() + QStringLiteral("_fourier_result.png");
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        QString::fromUtf8(u8"保存实验四结果"),
        suggested,
        QString::fromUtf8(u8"PNG 图像 (*.png);;BMP 图像 (*.bmp);;JPEG 图像 (*.jpg)")
    );
    if (filePath.isEmpty()) {
        return;
    }

    if (!m_resultPreview.save(filePath)) {
        QMessageBox::critical(this, QString::fromUtf8(u8"错误"), QString::fromUtf8(u8"保存失败。"));
        return;
    }
    setStatus(QString::fromUtf8(u8"状态：结果已保存到 %1").arg(filePath));
}

bool FourierExperimentWidget::ensureSpectrum() {
    if (m_originalImage.isNull()) {
        QMessageBox::warning(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先加载图像。"));
        return false;
    }
    if (!m_spectrum.isValid()) {
        m_spectrum = computeSpectrum(m_originalImage, false);
    }
    if (!m_spectrum.isValid()) {
        QMessageBox::critical(this, QString::fromUtf8(u8"错误"), QString::fromUtf8(u8"FFT 计算失败。"));
        return false;
    }
    return true;
}

void FourierExperimentWidget::refreshPreviewLabels() {
    setPreviewImage(m_originalLabel, m_originalPreview);
    setPreviewImage(m_spectrumLabel, m_spectrumPreview);
    setPreviewImage(m_resultLabel, m_resultPreview);
}

void FourierExperimentWidget::setPreviewImage(QLabel* label, const QImage& image) {
    if (!label) {
        return;
    }

    if (image.isNull()) {
        label->setPixmap(QPixmap());
        label->setText(QString::fromUtf8(u8"等待图像"));
        return;
    }

    QSize target = label->contentsRect().size();
    if (target.width() < 2 || target.height() < 2) {
        target = label->minimumSize();
    }

    label->setText(QString());
    label->setPixmap(QPixmap::fromImage(image).scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void FourierExperimentWidget::setStatus(const QString& text) {
    if (m_statusLabel) {
        m_statusLabel->setText(text);
    }
}

QString FourierExperimentWidget::findSampleImage(const QString& fileName) const {
    const QStringList candidates = {
        QDir::current().filePath(QStringLiteral("图片/") + fileName),
        QDir::current().filePath(fileName),
        QStringLiteral("D:/大学/课程/大三/生物医学图像处理（双语）/实验四 图像的二维傅里叶变换及变换域滤波/") + fileName
    };

    for (const QString& path : candidates) {
        if (QFileInfo::exists(path)) {
            return path;
        }
    }
    return {};
}

QString FourierExperimentWidget::describeCurrentImage() const {
    if (m_originalImage.isNull()) {
        return QString::fromUtf8(u8"图像信息：未加载");
    }

    return QString::fromUtf8(u8"图像信息：%1 | %2 x %3 | 原始位深 %4 | 灰度处理")
        .arg(QFileInfo(m_currentPath).fileName())
        .arg(m_originalImage.width())
        .arg(m_originalImage.height())
        .arg(m_currentBitDepth);
}

int FourierExperimentWidget::nextPowerOfTwo(int value) {
    int result = 1;
    while (result < value) {
        result <<= 1;
    }
    return result;
}

int FourierExperimentWidget::clampToByte(double value) {
    return std::clamp(static_cast<int>(std::lround(value)), 0, 255);
}

QVector<FourierExperimentWidget::Complex> FourierExperimentWidget::makeCenteredSamples(
    const QImage& image,
    int paddedWidth,
    int paddedHeight,
    bool useLog
) {
    QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    QVector<Complex> data(paddedWidth * paddedHeight);
    for (int y = 0; y < paddedHeight; ++y) {
        for (int x = 0; x < paddedWidth; ++x) {
            double value = 0.0;
            if (x < gray.width() && y < gray.height()) {
                value = qGray(gray.pixel(x, y));
                if (useLog) {
                    value = std::log1p(value);
                }
            }
            if ((x + y) & 1) {
                value = -value;
            }
            data[y * paddedWidth + x] = Complex(value, 0.0);
        }
    }
    return data;
}

void FourierExperimentWidget::fft1D(QVector<Complex>& data, bool inverse) {
    const int n = data.size();
    if (n <= 1) {
        return;
    }

    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(data[i], data[j]);
        }
    }

    for (int len = 2; len <= n; len <<= 1) {
        const double angle = 2.0 * kPi / len * (inverse ? 1.0 : -1.0);
        const Complex wLen(std::cos(angle), std::sin(angle));
        for (int i = 0; i < n; i += len) {
            Complex w(1.0, 0.0);
            for (int j = 0; j < len / 2; ++j) {
                const Complex u = data[i + j];
                const Complex v = data[i + j + len / 2] * w;
                data[i + j] = u + v;
                data[i + j + len / 2] = u - v;
                w *= wLen;
            }
        }
    }

    if (inverse) {
        for (Complex& value : data) {
            value /= static_cast<double>(n);
        }
    }
}

void FourierExperimentWidget::fft2D(QVector<Complex>& data, int width, int height, bool inverse) {
    QVector<Complex> line(std::max(width, height));

    for (int y = 0; y < height; ++y) {
        line.resize(width);
        for (int x = 0; x < width; ++x) {
            line[x] = data[y * width + x];
        }
        fft1D(line, inverse);
        for (int x = 0; x < width; ++x) {
            data[y * width + x] = line[x];
        }
    }

    for (int x = 0; x < width; ++x) {
        line.resize(height);
        for (int y = 0; y < height; ++y) {
            line[y] = data[y * width + x];
        }
        fft1D(line, inverse);
        for (int y = 0; y < height; ++y) {
            data[y * width + x] = line[y];
        }
    }
}

FourierExperimentWidget::SpectrumData FourierExperimentWidget::computeSpectrum(const QImage& image, bool useLog) {
    SpectrumData spectrum;
    if (image.isNull()) {
        return spectrum;
    }

    spectrum.originalWidth = image.width();
    spectrum.originalHeight = image.height();
    spectrum.width = nextPowerOfTwo(image.width());
    spectrum.height = nextPowerOfTwo(image.height());
    spectrum.values = makeCenteredSamples(image, spectrum.width, spectrum.height, useLog);
    fft2D(spectrum.values, spectrum.width, spectrum.height, false);
    return spectrum;
}

QImage FourierExperimentWidget::spectrumToImage(const SpectrumData& spectrum) {
    if (!spectrum.isValid()) {
        return {};
    }

    QVector<double> magnitudes(spectrum.values.size());
    double maxValue = 0.0;
    for (int i = 0; i < spectrum.values.size(); ++i) {
        const double value = std::log1p(std::abs(spectrum.values[i]));
        magnitudes[i] = value;
        maxValue = std::max(maxValue, value);
    }

    QImage image(spectrum.width, spectrum.height, QImage::Format_Grayscale8);
    if (maxValue <= std::numeric_limits<double>::epsilon()) {
        image.fill(Qt::black);
        return image;
    }

    for (int y = 0; y < spectrum.height; ++y) {
        uchar* dst = image.scanLine(y);
        for (int x = 0; x < spectrum.width; ++x) {
            const double normalized = magnitudes[y * spectrum.width + x] / maxValue;
            dst[x] = static_cast<uchar>(clampToByte(normalized * 255.0));
        }
    }
    return image;
}

FourierExperimentWidget::SpectrumData FourierExperimentWidget::filteredSpectrum(
    const SpectrumData& spectrum,
    FrequencyFilter filter,
    double cutoff,
    int order
) {
    SpectrumData out = spectrum;
    if (!out.isValid()) {
        return out;
    }

    cutoff = std::max(1.0, cutoff);
    order = std::max(1, order);
    const double centerX = out.width / 2.0;
    const double centerY = out.height / 2.0;

    for (int y = 0; y < out.height; ++y) {
        for (int x = 0; x < out.width; ++x) {
            const double dx = x - centerX;
            const double dy = y - centerY;
            const double distance = std::sqrt(dx * dx + dy * dy);
            double gain = 1.0;

            switch (filter) {
            case FrequencyFilter::IdealLowPass:
                gain = distance <= cutoff ? 1.0 : 0.0;
                break;
            case FrequencyFilter::IdealHighPass:
                gain = distance > cutoff ? 1.0 : 0.0;
                break;
            case FrequencyFilter::ButterworthLowPass:
                gain = 1.0 / (1.0 + std::pow(distance / cutoff, 2.0 * order));
                break;
            case FrequencyFilter::ButterworthHighPass:
                if (distance <= std::numeric_limits<double>::epsilon()) {
                    gain = 0.0;
                } else {
                    gain = 1.0 / (1.0 + std::pow(cutoff / distance, 2.0 * order));
                }
                break;
            }

            out.values[y * out.width + x] *= gain;
        }
    }
    return out;
}

QImage FourierExperimentWidget::inverseToImage(const SpectrumData& spectrum) {
    if (!spectrum.isValid()) {
        return {};
    }

    QVector<Complex> spatial = spectrum.values;
    fft2D(spatial, spectrum.width, spectrum.height, true);

    QImage out(spectrum.originalWidth, spectrum.originalHeight, QImage::Format_Grayscale8);
    for (int y = 0; y < out.height(); ++y) {
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < out.width(); ++x) {
            double value = spatial[y * spectrum.width + x].real();
            if ((x + y) & 1) {
                value = -value;
            }
            dst[x] = static_cast<uchar>(clampToByte(value));
        }
    }
    return out;
}

QImage FourierExperimentWidget::highBoostImage(const QImage& original, const SpectrumData& highPassSpectrum, double amount) {
    if (original.isNull() || !highPassSpectrum.isValid()) {
        return {};
    }

    QVector<Complex> spatial = highPassSpectrum.values;
    fft2D(spatial, highPassSpectrum.width, highPassSpectrum.height, true);

    QImage gray = original.convertToFormat(QImage::Format_Grayscale8);
    QImage out(highPassSpectrum.originalWidth, highPassSpectrum.originalHeight, QImage::Format_Grayscale8);
    for (int y = 0; y < out.height(); ++y) {
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < out.width(); ++x) {
            double high = spatial[y * highPassSpectrum.width + x].real();
            if ((x + y) & 1) {
                high = -high;
            }
            const double base = qGray(gray.pixel(x, y));
            dst[x] = static_cast<uchar>(clampToByte(base + amount * high));
        }
    }
    return out;
}

QImage FourierExperimentWidget::homomorphicImage(
    const QImage& image,
    double cutoff,
    double gammaLow,
    double gammaHigh,
    double c,
    QImage* spectrumImage
) {
    SpectrumData spectrum = computeSpectrum(image, true);
    if (!spectrum.isValid()) {
        return {};
    }

    cutoff = std::max(1.0, cutoff);
    gammaLow = std::max(0.01, gammaLow);
    gammaHigh = std::max(gammaLow, gammaHigh);
    c = std::max(0.01, c);

    const double centerX = spectrum.width / 2.0;
    const double centerY = spectrum.height / 2.0;
    for (int y = 0; y < spectrum.height; ++y) {
        for (int x = 0; x < spectrum.width; ++x) {
            const double dx = x - centerX;
            const double dy = y - centerY;
            const double d2 = dx * dx + dy * dy;
            const double gain = (gammaHigh - gammaLow) * (1.0 - std::exp(-c * d2 / (cutoff * cutoff))) + gammaLow;
            spectrum.values[y * spectrum.width + x] *= gain;
        }
    }

    if (spectrumImage) {
        *spectrumImage = spectrumToImage(spectrum);
    }

    QVector<Complex> spatial = spectrum.values;
    fft2D(spatial, spectrum.width, spectrum.height, true);

    QImage out(spectrum.originalWidth, spectrum.originalHeight, QImage::Format_Grayscale8);
    double minValue = std::numeric_limits<double>::max();
    double maxValue = std::numeric_limits<double>::lowest();
    QVector<double> values(out.width() * out.height());

    for (int y = 0; y < out.height(); ++y) {
        for (int x = 0; x < out.width(); ++x) {
            double value = spatial[y * spectrum.width + x].real();
            if ((x + y) & 1) {
                value = -value;
            }
            value = std::expm1(value);
            values[y * out.width() + x] = value;
            minValue = std::min(minValue, value);
            maxValue = std::max(maxValue, value);
        }
    }

    const double range = std::max(1.0, maxValue - minValue);
    for (int y = 0; y < out.height(); ++y) {
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < out.width(); ++x) {
            const double normalized = (values[y * out.width() + x] - minValue) / range;
            dst[x] = static_cast<uchar>(clampToByte(normalized * 255.0));
        }
    }
    return out;
}
