#pragma once

#include <QImage>
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

    static SpectrumData computeSpectrum(const QImage& image, bool useLog = false);
    static QImage spectrumToImage(const SpectrumData& spectrum);
    static SpectrumData filteredSpectrum(const SpectrumData& spectrum, FrequencyFilter filter, double cutoff, int order);
    static QImage inverseToImage(const SpectrumData& spectrum);
    static QImage highBoostImage(const QImage& original, const SpectrumData& highPassSpectrum, double amount);
    static QImage homomorphicImage(const QImage& image, double cutoff, double gammaLow, double gammaHigh, double c, QImage* spectrumImage);

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
