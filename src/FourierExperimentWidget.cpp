#include "FourierExperimentWidget.h"

#include "BMPReader.h"

#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QSpinBox>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr double kPi = 3.14159265358979323846;
using Complex = std::complex<double>;

int nextPowerOfTwo(int value) {
    int result = 1;
    while (result < value) {
        result <<= 1;
    }
    return result;
}

int clampToByte(double value) {
    return std::clamp(static_cast<int>(std::lround(value)), 0, 255);
}

}

FourierExperimentWidget::FourierExperimentWidget(QWidget* parent) : QWidget(parent) {
}

void FourierExperimentWidget::initializeFromDesignerUi() {
    if (m_uiInitialized) {
        return;
    }

    auto* openButton = findChild<QPushButton*>(QStringLiteral("fourierOpenButton"));
    auto* fftButton = findChild<QPushButton*>(QStringLiteral("fourierFftButton"));
    auto* ifftButton = findChild<QPushButton*>(QStringLiteral("fourierIfftButton"));
    auto* idealLowButton = findChild<QPushButton*>(QStringLiteral("fourierIdealLowButton"));
    auto* butterLowButton = findChild<QPushButton*>(QStringLiteral("fourierButterLowButton"));
    auto* idealHighButton = findChild<QPushButton*>(QStringLiteral("fourierIdealHighButton"));
    auto* butterHighButton = findChild<QPushButton*>(QStringLiteral("fourierButterHighButton"));
    auto* homoButton = findChild<QPushButton*>(QStringLiteral("fourierHomoButton"));

    m_pathEdit = findChild<QLineEdit*>(QStringLiteral("fourierPathEdit"));
    m_cutoffSpin = findChild<QSpinBox*>(QStringLiteral("fourierCutoffSpin"));
    m_orderSpin = findChild<QSpinBox*>(QStringLiteral("fourierOrderSpin"));
    m_gammaLowSpin = findChild<QDoubleSpinBox*>(QStringLiteral("fourierGammaLowSpin"));
    m_gammaHighSpin = findChild<QDoubleSpinBox*>(QStringLiteral("fourierGammaHighSpin"));
    m_homomorphicCSpin = findChild<QDoubleSpinBox*>(QStringLiteral("fourierHomomorphicCSpin"));
    m_saveButton = findChild<QPushButton*>(QStringLiteral("fourierSaveButton"));
    m_imageInfoLabel = findChild<QLabel*>(QStringLiteral("fourierImageInfoLabel"));
    m_originalLabel = findChild<QLabel*>(QStringLiteral("fourierOriginalLabel"));
    m_spectrumLabel = findChild<QLabel*>(QStringLiteral("fourierSpectrumLabel"));
    m_resultLabel = findChild<QLabel*>(QStringLiteral("fourierResultLabel"));
    m_statusLabel = findChild<QLabel*>(QStringLiteral("fourierStatusLabel"));

    if (m_saveButton) {
        m_saveButton->setEnabled(false);
    }

    connect(openButton, &QPushButton::clicked, this, &FourierExperimentWidget::openImage);
    connect(fftButton, &QPushButton::clicked, this, &FourierExperimentWidget::runForwardFft);
    connect(ifftButton, &QPushButton::clicked, this, &FourierExperimentWidget::runInverseFft);
    connect(idealLowButton, &QPushButton::clicked, this, &FourierExperimentWidget::applyIdealLowPass);
    connect(butterLowButton, &QPushButton::clicked, this, &FourierExperimentWidget::applyButterworthLowPass);
    connect(idealHighButton, &QPushButton::clicked, this, &FourierExperimentWidget::applyIdealHighPass);
    connect(butterHighButton, &QPushButton::clicked, this, &FourierExperimentWidget::applyButterworthHighPass);
    connect(homoButton, &QPushButton::clicked, this, &FourierExperimentWidget::applyHomomorphicFilter);
    connect(m_saveButton, &QPushButton::clicked, this, &FourierExperimentWidget::saveResult);

    m_uiInitialized = true;
    setStatus(QString::fromUtf8(u8"状态：请加载图像后开始频域分析"));
}

void FourierExperimentWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    refreshPreviewLabels();
}

void FourierExperimentWidget::openImage() {
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        QString::fromUtf8(u8"打开频域分析图像"),
        QString(),
        QString::fromUtf8(u8"图像文件 (*.bmp *.jpg *.jpeg *.png *.tif *.tiff);;所有文件 (*.*)")
    );
    if (!filePath.isEmpty()) {
        loadImageFromPath(filePath);
    }
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
    setStatus(QString::fromUtf8(u8"状态：已加载图像，可执行 FFT 或频域滤波"));
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
    SpectrumData filtered;
    switch (filter) {
    case FrequencyFilter::IdealLowPass:
        filtered = moduleIdealLowPassFilter(m_spectrum, cutoff);
        break;
    case FrequencyFilter::ButterworthLowPass:
        filtered = moduleButterworthLowPassFilter(m_spectrum, cutoff, order);
        break;
    case FrequencyFilter::IdealHighPass:
        filtered = moduleIdealHighPassFilter(m_spectrum, cutoff);
        break;
    case FrequencyFilter::ButterworthHighPass:
        filtered = moduleButterworthHighPassFilter(m_spectrum, cutoff, order);
        break;
    }
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
    m_resultPreview = moduleHomomorphicFilter(
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
        QString::fromUtf8(u8"保存频域分析结果"),
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
        m_spectrum = moduleFourierTransform2D(m_originalImage);
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

QImage FourierExperimentWidget::processFrequencyImage(
    const QImage& image,
    const QString& actionName,
    double cutoff,
    int order,
    double gammaLow,
    double gammaHigh,
    double c
) {
    if (image.isNull()) {
        return {};
    }

    if (actionName == QString::fromUtf8(u8"同态滤波")) {
        return moduleHomomorphicFilter(image, cutoff, gammaLow, gammaHigh, c, nullptr);
    }

    SpectrumData spectrum = moduleFourierTransform2D(image);
    if (!spectrum.isValid()) {
        return {};
    }

    if (actionName == QString::fromUtf8(u8"FFT 频谱")) {
        return spectrumToImage(spectrum);
    }
    if (actionName == QString::fromUtf8(u8"IFFT 重建")) {
        return inverseToImage(spectrum);
    }
    if (actionName == QString::fromUtf8(u8"理想低通")) {
        return inverseToImage(moduleIdealLowPassFilter(spectrum, cutoff));
    }
    if (actionName == QString::fromUtf8(u8"巴特沃斯低通")) {
        return inverseToImage(moduleButterworthLowPassFilter(spectrum, cutoff, order));
    }
    if (actionName == QString::fromUtf8(u8"理想高通")) {
        return highBoostImage(image, moduleIdealHighPassFilter(spectrum, cutoff), 1.0);
    }
    if (actionName == QString::fromUtf8(u8"巴特沃斯高通")) {
        return highBoostImage(image, moduleButterworthHighPassFilter(spectrum, cutoff, order), 1.0);
    }
    return {};
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

QImage FourierExperimentWidget::inverseToImage(const SpectrumData& spectrum) {
    if (!spectrum.isValid()) {
        return {};
    }

    QVector<Complex> spatial = spectrum.values;
    moduleFftAndIfft(spatial, spectrum.width, spectrum.height, true);

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
    moduleFftAndIfft(spatial, highPassSpectrum.width, highPassSpectrum.height, true);

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
// ==================== 频域分析算法模块 ====================

// 1. FFT 和 IFFT：inverse=false 时执行 FFT，inverse=true 时执行 IFFT。
void FourierExperimentWidget::moduleFftAndIfft(QVector<Complex>& data, int width, int height
    , bool inverse) {
    QVector<Complex> line(std::max(width, height));

    for (int y = 0; y < height; ++y) {
        line.resize(width);
        for (int x = 0; x < width; ++x) {
            line[x] = data[y * width + x];
        }

        for (int i = 1, j = 0; i < width; ++i) {
            int bit = width >> 1;
            for (; j & bit; bit >>= 1) {
                j ^= bit;
            }
            j ^= bit;
            if (i < j) {
                std::swap(line[i], line[j]);
            }
        }

        for (int len = 2; len <= width; len <<= 1) {
            const double angle = 2.0 * kPi / len * (inverse ? 1.0 : -1.0);
            const Complex wLen(std::cos(angle), std::sin(angle));
            for (int i = 0; i < width; i += len) {
                Complex w(1.0, 0.0);
                for (int j = 0; j < len / 2; ++j) {
                    const Complex u = line[i + j];
                    const Complex v = line[i + j + len / 2] * w;
                    line[i + j] = u + v;
                    line[i + j + len / 2] = u - v;
                    w *= wLen;
                }
            }
        }

        if (inverse) {
            for (Complex& value : line) {
                value /= static_cast<double>(width);
            }
        }

        for (int x = 0; x < width; ++x) {
            data[y * width + x] = line[x];
        }
    }

    for (int x = 0; x < width; ++x) {
        line.resize(height);
        for (int y = 0; y < height; ++y) {
            line[y] = data[y * width + x];
        }

        for (int i = 1, j = 0; i < height; ++i) {
            int bit = height >> 1;
            for (; j & bit; bit >>= 1) {
                j ^= bit;
            }
            j ^= bit;
            if (i < j) {
                std::swap(line[i], line[j]);
            }
        }
        for (int len = 2; len <= height; len <<= 1) {
            const double angle = 2.0 * kPi / len * (inverse ? 1.0 : -1.0);
            const Complex wLen(std::cos(angle), std::sin(angle));
            for (int i = 0; i < height; i += len) {
                Complex w(1.0, 0.0);
                for (int j = 0; j < len / 2; ++j) {
                    const Complex u = line[i + j];
                    const Complex v = line[i + j + len / 2] * w;
                    line[i + j] = u + v;
                    line[i + j + len / 2] = u - v;
                    w *= wLen;
                }
            }
        }
        if (inverse) {
            for (Complex& value : line) {
                value /= static_cast<double>(height);
            }
        }

        for (int y = 0; y < height; ++y) {
            data[y * width + x] = line[y];
        }
    }
}

// 2. 对一幅二维数字图像进行傅里叶变换。
FourierExperimentWidget::SpectrumData FourierExperimentWidget::moduleFourierTransform2D(const 
    QImage& image) {
    SpectrumData spectrum;
    if (image.isNull()) {
        return spectrum;
    }

    spectrum.originalWidth = image.width();
    spectrum.originalHeight = image.height();
    spectrum.width = nextPowerOfTwo(image.width());
    spectrum.height = nextPowerOfTwo(image.height());

    QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    spectrum.values.resize(spectrum.width * spectrum.height);
    for (int y = 0; y < spectrum.height; ++y) {
        for (int x = 0; x < spectrum.width; ++x) {
            double value = 0.0;
            if (x < gray.width() && y < gray.height()) {
                value = qGray(gray.pixel(x, y));
            }
            if ((x + y) & 1) {
                value = -value;
            }
            spectrum.values[y * spectrum.width + x] = Complex(value, 0.0);
        }
    }

    moduleFftAndIfft(spectrum.values, spectrum.width, spectrum.height, false);
    return spectrum;
}

// 3. 理想低通滤波器函数。
FourierExperimentWidget::SpectrumData FourierExperimentWidget::moduleIdealLowPassFilter(
    const SpectrumData& spectrum,
    double cutoff
) {
    SpectrumData out = spectrum;
    if (!out.isValid()) {
        return out;
    }

    cutoff = std::max(1.0, cutoff);
    const double centerX = out.width / 2.0;
    const double centerY = out.height / 2.0;
    for (int y = 0; y < out.height; ++y) {
        for (int x = 0; x < out.width; ++x) {
            const double dx = x - centerX;
            const double dy = y - centerY;
            const double distance = std::sqrt(dx * dx + dy * dy);
            const double gain = distance <= cutoff ? 1.0 : 0.0;
            out.values[y * out.width + x] *= gain;
        }
    }
    return out;
}

// 4. 巴特沃斯低通滤波器。
FourierExperimentWidget::SpectrumData FourierExperimentWidget::moduleButterworthLowPassFilter(
    const SpectrumData& spectrum,
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
            const double gain = 1.0 / (1.0 + std::pow(distance / cutoff, 2.0 * order));
            out.values[y * out.width + x] *= gain;
        }
    }
    return out;
}

// 5. 理想高通滤波器。
FourierExperimentWidget::SpectrumData FourierExperimentWidget::moduleIdealHighPassFilter(
    const SpectrumData& spectrum,
    double cutoff
) {
    SpectrumData out = spectrum;
    if (!out.isValid()) {
        return out;
    }

    cutoff = std::max(1.0, cutoff);
    const double centerX = out.width / 2.0;
    const double centerY = out.height / 2.0;
    for (int y = 0; y < out.height; ++y) {
        for (int x = 0; x < out.width; ++x) {
            const double dx = x - centerX;
            const double dy = y - centerY;
            const double distance = std::sqrt(dx * dx + dy * dy);
            const double gain = distance > cutoff ? 1.0 : 0.0;
            out.values[y * out.width + x] *= gain;
        }
    }
    return out;
}

// 6. 巴特沃斯高通滤波器。
FourierExperimentWidget::SpectrumData FourierExperimentWidget::moduleButterworthHighPassFilter(
    const SpectrumData& spectrum,
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
            double gain = 0.0;
            if (distance > std::numeric_limits<double>::epsilon()) {
                gain = 1.0 / (1.0 + std::pow(cutoff / distance, 2.0 * order));
            }
            out.values[y * out.width + x] *= gain;
        }
    }
    return out;
}

// 7. 同态滤波器。
QImage FourierExperimentWidget::moduleHomomorphicFilter(
    const QImage& image,
    double cutoff,
    double gammaLow,
    double gammaHigh,
    double c,
    QImage* spectrumImage
) {
    SpectrumData spectrum;
    if (image.isNull()) {
        return {};
    }

    spectrum.originalWidth = image.width();
    spectrum.originalHeight = image.height();
    spectrum.width = nextPowerOfTwo(image.width());
    spectrum.height = nextPowerOfTwo(image.height());

    QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    spectrum.values.resize(spectrum.width * spectrum.height);
    for (int y = 0; y < spectrum.height; ++y) {
        for (int x = 0; x < spectrum.width; ++x) {
            double value = 0.0;
            if (x < gray.width() && y < gray.height()) {
                value = std::log1p(qGray(gray.pixel(x, y)));
            }
            if ((x + y) & 1) {
                value = -value;
            }
            spectrum.values[y * spectrum.width + x] = Complex(value, 0.0);
        }
    }

    moduleFftAndIfft(spectrum.values, spectrum.width, spectrum.height, false);

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
        QVector<double> magnitudes(spectrum.values.size());
        double maxValue = 0.0;
        for (int i = 0; i < spectrum.values.size(); ++i) {
            const double value = std::log1p(std::abs(spectrum.values[i]));
            magnitudes[i] = value;
            maxValue = std::max(maxValue, value);
        }

        QImage view(spectrum.width, spectrum.height, QImage::Format_Grayscale8);
        if (maxValue <= std::numeric_limits<double>::epsilon()) {
            view.fill(Qt::black);
        } else {
            for (int y = 0; y < spectrum.height; ++y) {
                uchar* dst = view.scanLine(y);
                for (int x = 0; x < spectrum.width; ++x) {
                    const double normalized = magnitudes[y * spectrum.width + x] / maxValue;
                    dst[x] = static_cast<uchar>(clampToByte(normalized * 255.0));
                }
            }
        }
        *spectrumImage = view;
    }

    QVector<Complex> spatial = spectrum.values;
    moduleFftAndIfft(spatial, spectrum.width, spectrum.height, true);

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
