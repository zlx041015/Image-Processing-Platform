#pragma once

#include <QImage>
#include <QString>
#include <QVector>
#include <QWidget>

#include <complex>

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QDoubleSpinBox;
class QResizeEvent;

class FourierExperimentWidget : public QWidget {
    Q_OBJECT

public:
    explicit FourierExperimentWidget(QWidget* parent = nullptr);
    void initializeFromDesignerUi();
    static QImage processFrequencyImage(
        const QImage& image,
        const QString& actionName,
        double cutoff,
        int order,
        double gammaLow,
        double gammaHigh,
        double c
    );

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void openImage();
    void runForwardFft();
    void runInverseFft();
    void applyIdealLowPass();
    void applyButterworthLowPass();
    void applyIdealHighPass();
    void applyButterworthHighPass();
    void applyHomomorphicFilter();
    void saveResult();

private:
    using Complex = std::complex<double>;

    struct SpectrumData {
        int width = 0;
        int height = 0;
        int originalWidth = 0;
        int originalHeight = 0;
        QVector<Complex> values;

        bool isValid() const {
            return width > 0 && height > 0 && values.size() == width * height;
        }
    };

    enum class FrequencyFilter {
        IdealLowPass,
        ButterworthLowPass,
        IdealHighPass,
        ButterworthHighPass
    };

    void loadImageFromPath(const QString& filePath);
    bool ensureSpectrum();
    void applyFrequencyFilter(FrequencyFilter filter);
    void refreshPreviewLabels();
    void setPreviewImage(QLabel* label, const QImage& image);
    void setStatus(const QString& text);

    QString describeCurrentImage() const;

    // 频域分析算法模块，按处理流程集中组织。
    static void moduleFftAndIfft(QVector<Complex>& data, int width, int height, bool inverse);
    static SpectrumData moduleFourierTransform2D(const QImage& image);
    static SpectrumData moduleIdealLowPassFilter(const SpectrumData& spectrum, double cutoff);
    static SpectrumData moduleButterworthLowPassFilter(const SpectrumData& spectrum, double cutoff, int order);
    static SpectrumData moduleIdealHighPassFilter(const SpectrumData& spectrum, double cutoff);
    static SpectrumData moduleButterworthHighPassFilter(const SpectrumData& spectrum, double cutoff, int order);
    static QImage moduleHomomorphicFilter(
        const QImage& image,
        double cutoff,
        double gammaLow,
        double gammaHigh,
        double c,
        QImage* spectrumImage
    );

    static QImage spectrumToImage(const SpectrumData& spectrum);
    static QImage inverseToImage(const SpectrumData& spectrum);
    static QImage highBoostImage(const QImage& original, const SpectrumData& highPassSpectrum, double amount);

    QLineEdit* m_pathEdit = nullptr;
    QLabel* m_imageInfoLabel = nullptr;
    QLabel* m_originalLabel = nullptr;
    QLabel* m_spectrumLabel = nullptr;
    QLabel* m_resultLabel = nullptr;
    QLabel* m_statusLabel = nullptr;

    QSpinBox* m_cutoffSpin = nullptr;
    QSpinBox* m_orderSpin = nullptr;
    QDoubleSpinBox* m_gammaLowSpin = nullptr;
    QDoubleSpinBox* m_gammaHighSpin = nullptr;
    QDoubleSpinBox* m_homomorphicCSpin = nullptr;

    QPushButton* m_saveButton = nullptr;

    QString m_currentPath;
    int m_currentBitDepth = 0;
    QImage m_originalImage;
    SpectrumData m_spectrum;
    QImage m_originalPreview;
    QImage m_spectrumPreview;
    QImage m_resultPreview;
    bool m_uiInitialized = false;
};
