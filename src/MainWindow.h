#pragma once

#include <QImage>
#include <QMainWindow>
#include <QStringList>
#include <QVector>
#include <memory>

class QListWidget;
class QLabel;
class QComboBox;
class QLineEdit;
class ImageView;
class QListWidgetItem;
class QDragEnterEvent;
class QDropEvent;
class QPoint;
class QPushButton;
class QSlider;
class QStackedWidget;
class QTabWidget;
class QSpinBox;
class QResizeEvent;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void selectFolder();
    void openSingleFile();
    void onFileSelected(QListWidgetItem* item);
    void onFilterChanged();
    void onExperimentSelected(int row);
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void updateHoverInfo(int x, int y, QRgb rgba);
    void updateHoverOutside();
    void showPixelInfo(int x, int y, QRgb rgba);
    void doLinearStretch();
    void doEqualize();
    void doHistMatch();
    void loadNoiseExperimentImage();
    void addNoiseToExperiment();
    void applyNoiseFilterToExperiment();
    void resetNoiseExperiment();
    void onExperimentNoiseControlsChanged();
    void loadEdgeExperimentImage();
    void applyEdgeDetectionToExperiment();
    void resetEdgeExperiment();
    void onExperimentEdgeControlsChanged();
    void loadEnhancementExperimentImage();
    void startEnhancementExperiment();
    void stepEnhancementExperiment();
    void resetEnhancementExperiment();
    void showEnhancementPreviewDialog();
    void saveNoiseExperimentResult();
    void saveEdgeExperimentResult();
    void saveEnhancementExperimentResult();
    void undoProcessing();
    void redoProcessing();
    void resetProcessing();

private:
    void createUi();
    QWidget* createExperimentPage();
    QWidget* createNoiseExperimentPage();
    QWidget* createEdgeExperimentPage();
    QWidget* createEnhancementExperimentPage();
    void updateNoiseExperimentPreview();
    void updateEdgeExperimentPreview();
    void updateEnhancementExperimentPreview();
    void updateExperimentImageLabel(QLabel* label, const QImage& image);
    bool saveImageToFile(const QImage& image, const QString& suggestedName);
    void loadImageFiles(const QString& folder);
    void displayImage(const QString& filePath);
    void renderCurrentImage();
    void updateHistogram();
    void handleZoomWheel(int delta);
    void applyProcessingAction(const QString& actionName);
    void pushProcessingResult(const QImage& image, const QString& actionName);
    QImage currentProcessingInput() const;
    void refreshWorkbenchImages();
    void updateProcessingStatus(const QString& message = QString());
    void updateParameterStatus();
    bool isDropOnOriginalView(const QPoint& windowPos) const;
    bool isSupportedImageFile(const QString& filePath) const;
    QImage applyFilter(const QImage& img, const QString& filterName) const;
    QImage convolve(const QImage& img, const QVector<float>& kernel, float divisor, float bias = 0.0f) const;
    QImage addSaltPepperNoise(const QImage& img, double density) const;
    QImage addImpulseNoise(const QImage& img, double density) const;
    QImage meanFilter(const QImage& img, int kernelSize) const;
    QImage medianFilter(const QImage& img, int kernelSize) const;
    QImage maxFilter(const QImage& img, int kernelSize) const;
    QImage sobelEdgeDetect(const QImage& img, int kernelSize, int threshold) const;
    QImage prewittEdgeDetect(const QImage& img, int kernelSize, int threshold) const;
    QImage laplacianEdgeDetect(const QImage& img, int kernelSize, int threshold) const;
    QImage step1_originalImage(const QImage& img) const;
    QImage step2_laplacianProcess(const QImage& img) const;
    QImage step3_sharpenImage(const QImage& original, const QImage& laplacian) const;
    QImage step4_sobelProcess(const QImage& img) const;
    QImage step5_meanFilterGradient(const QImage& sobel) const;
    QImage step6_maskImage(const QImage& sharpened, const QImage& gradient) const;
    QImage step7_addOriginalAndMask(const QImage& original, const QImage& mask) const;
    QImage step8_gammaTransform(const QImage& enhanced, double gamma = 0.8) const;
    static int clampToByte(int value);
    QImage loadGrayscaleImageFromFile(const QString& filePath, QString* errorMessage = nullptr) const;

    std::unique_ptr<Ui::MainWindow> ui;

    QString m_currentFolder;
    QString m_currentFile;
    QImage m_originalImage;
    QImage m_filteredImage;
    int m_currentWidth = 0;
    int m_currentHeight = 0;
    int m_currentBitCount = 0;
    uint32_t m_currentCompression = 0;
    double m_zoom = 1.0;
    const double m_minZoom = 0.1;
    const double m_maxZoom = 8.0;
    const double m_zoomStep = 0.01;

    QListWidget* m_fileList = nullptr;
    QLabel* m_infoLabel = nullptr;
    QLabel* m_histogramLabel = nullptr;
    QLabel* m_folderLabel = nullptr;
    QLabel* m_zoomLabel = nullptr;
    QLabel* m_previewHint = nullptr;
    QLabel* m_chainLabel = nullptr;
    QLabel* m_parameterLabel = nullptr;
    QComboBox* m_chainCombo = nullptr;
    QComboBox* m_parameterCombo = nullptr;
    QComboBox* m_filterCombo = nullptr;
    ImageView* m_imageView = nullptr;
    ImageView* m_resultView = nullptr;
    QTabWidget* m_mainTabs = nullptr;
    QListWidget* m_experimentList = nullptr;
    QStackedWidget* m_experimentStackedWidget = nullptr;
    QLabel* m_experimentStatusLabel = nullptr;

    QWidget* m_noiseExperimentPage = nullptr;
    QLineEdit* m_noiseInputPathEdit = nullptr;
    QComboBox* m_noiseTypeCombo = nullptr;
    QSlider* m_noiseStrengthSlider = nullptr;
    QLabel* m_noiseStrengthValueLabel = nullptr;
    QComboBox* m_noiseFilterCombo = nullptr;
    QSpinBox* m_noiseKernelSpin = nullptr;
    QPushButton* m_noiseLoadButton = nullptr;
    QPushButton* m_noiseAddButton = nullptr;
    QPushButton* m_noiseFilterButton = nullptr;
    QPushButton* m_noiseSaveButton = nullptr;
    QPushButton* m_noiseResetButton = nullptr;
    QLabel* m_noiseOriginalLabel = nullptr;
    QLabel* m_noiseNoisyLabel = nullptr;
    QLabel* m_noiseDenoisedLabel = nullptr;

    QImage m_noiseSourceImage;
    QImage m_noiseNoisyImage;
    QImage m_noiseDenoisedImage;
    QString m_noiseSourcePath;

    QVector<QImage> m_resultHistory;
    QVector<QStringList> m_chainHistory;
    QStringList m_processingChain;
    QVector<QStringList> m_parameterHistory;
    QStringList m_parameterRecords;
    int m_historyIndex = -1;
    bool m_fitOnNextRefresh = false;
    QImage m_frequencyIfftSource;
    QPushButton* m_undoButton = nullptr;
    QPushButton* m_redoButton = nullptr;
    QPushButton* m_resetButton = nullptr;

    QWidget* m_edgeExperimentPage = nullptr;
    QLineEdit* m_edgeInputPathEdit = nullptr;
    QComboBox* m_edgeTypeCombo = nullptr;
    QSpinBox* m_edgeKernelSpin = nullptr;
    QSlider* m_edgeThresholdSlider = nullptr;
    QLabel* m_edgeThresholdValueLabel = nullptr;
    QPushButton* m_edgeLoadButton = nullptr;
    QPushButton* m_edgeApplyButton = nullptr;
    QPushButton* m_edgeSaveButton = nullptr;
    QPushButton* m_edgeResetButton = nullptr;
    QLabel* m_edgeOriginalLabel = nullptr;
    QLabel* m_edgeResultLabel = nullptr;

    QImage m_edgeSourceImage;
    QImage m_edgeResultImage;
    QString m_edgeSourcePath;

    QWidget* m_enhancementExperimentPage = nullptr;
    QLineEdit* m_enhancementInputPathEdit = nullptr;
    QPushButton* m_enhancementLoadButton = nullptr;
    QPushButton* m_enhancementStartButton = nullptr;
    QPushButton* m_enhancementStepButton = nullptr;
    QPushButton* m_enhancementSaveButton = nullptr;
    QPushButton* m_enhancementResetButton = nullptr;
    QLabel* m_enhancementInfoLabel = nullptr;
    QLabel* m_enhancementOriginalLabel = nullptr;
    QLabel* m_enhancementFinalLabel = nullptr;
    QImage m_enhancementSourceImage;
    QImage m_enhancementStep1Image;
    QImage m_enhancementStep2Image;
    QImage m_enhancementStep3Image;
    QImage m_enhancementStep4Image;
    QImage m_enhancementStep5Image;
    QImage m_enhancementStep6Image;
    QImage m_enhancementStep7Image;
    QImage m_enhancementStep8Image;
    QString m_enhancementSourcePath;
    int m_enhancementPreviewStep = 0;
};

