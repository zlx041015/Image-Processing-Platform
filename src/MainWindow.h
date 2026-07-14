#pragma once

#include "DicomSegLoader.h"
#include "DicomToolkit.h"
#include "DicomProcessingPipeline.h"
#include "ImageRestoration.h"
#include "SegmentationMeshBuilder.h"
#include "SliceNavigationController.h"
#include "SegmentVolume.h"

#include <QImage>
#include <QFutureWatcher>
#include <QMainWindow>
#include <QStringList>
#include <QVector>
#include <limits>

class QListWidget;
class QLabel;
class QComboBox;
class QCheckBox;
class QLineEdit;
class QPlainTextEdit;
class ImageView;
class Segmentation3DView;
class QListWidgetItem;
class QDragEnterEvent;
class QDropEvent;
class QPoint;
class QPushButton;
class QSlider;
class QStackedWidget;
class QSpinBox;
class QResizeEvent;
class QFrame;

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
    void importSegFile();
    void updateDicomSliceIndex(int index);
    void updateDicomWindowWidth(int value);
    void updateDicomWindowCenter(int value);
    void runDicomSegmentation();
    void clearSegmentationSeeds();
    void updateSegmentationControls();
    void clearLoadedSeg();
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
    enum class SeedEditMode {
        Navigate,
        Foreground,
        Background
    };

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
    void switchToDicomMode(bool enabled);
    void updateDicomViews();
    void loadDicomSeries(const QString& path);
    void applyDicomAction(const QString& actionName);
    QVector<QImage> currentDicomProcessingInput() const;
    void rebuildSegmentationPreview();
    void invalidateSegmentationSurfaceCache();
    void refreshSegmentationSurfaceCache();
    void scheduleSegmentationSurfaceBuild();
    void applySegmentationSurfaceResult(const SegmentationSurfaceData& surface, int generation, bool finalResult);
    void updateSegmentationSurfaceView();
    void clearSeedHistory();
    void pushSeedHistorySnapshot(const QVector<SegmentationSeedPoint>& seeds);
    void applyCurrentSeedHistory(const QString& message = QString());
    bool canUndoSeedHistory() const;
    bool canRedoSeedHistory() const;
    bool isSeedEditingEnabled() const;
    void appendSeedPoint(const SegmentationSeedPoint& seed);
    QImage buildCoronalMaskSlice() const;
    QImage buildSagittalMaskSlice() const;
    bool findSegmentationCentroid(int* outX, int* outY, int* outZ) const;
    int findBestSegmentationAxialSlice() const;
    QString segmentationDebugSummary() const;
    void onSliceNavigationChanged(const SliceNavigationController::State& state);
    bool isDropOnOriginalView(const QPoint& windowPos) const;
    bool isSupportedImageFile(const QString& filePath) const;
    bool isSupportedDicomFile(const QString& filePath) const;
    QImage applyFilter(const QImage& img, const QString& filterName) const;
    QImage convolve(const QImage& img, const QVector<float>& kernel, float divisor, float bias = 0.0f) const;
    static int clampToByte(int value);
    QImage currentDisplayImage() const;

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
    QPlainTextEdit* m_infoLabel = nullptr;
    QLabel* m_histogramLabel = nullptr;
    QLabel* m_folderLabel = nullptr;
    QLabel* m_zoomLabel = nullptr;
    QLabel* m_chainLabel = nullptr;
    QLabel* m_parameterLabel = nullptr;
    QComboBox* m_chainCombo = nullptr;
    QComboBox* m_parameterCombo = nullptr;
    QComboBox* m_filterCombo = nullptr;
    QStackedWidget* m_viewModeStack = nullptr;
    QWidget* m_standardViewPage = nullptr;
    QWidget* m_dicomViewPage = nullptr;
    ImageView* m_imageView = nullptr;
    ImageView* m_resultView = nullptr;
    ImageView* m_axialView = nullptr;
    ImageView* m_coronalView = nullptr;
    ImageView* m_sagittalView = nullptr;
    Segmentation3DView* m_volumeView = nullptr;
    QSlider* m_axialSlider = nullptr;
    QSlider* m_coronalSlider = nullptr;
    QSlider* m_sagittalSlider = nullptr;
    QSlider* m_windowWidthSlider = nullptr;
    QSlider* m_windowCenterSlider = nullptr;
    QLabel* m_windowWidthValueLabel = nullptr;
    QLabel* m_windowCenterValueLabel = nullptr;
    QComboBox* m_dicomProcessingScopeCombo = nullptr;
    QComboBox* m_dicomProcessingTargetCombo = nullptr;
    QComboBox* m_segmentationMethodCombo = nullptr;
    QComboBox* m_seedModeCombo = nullptr;
    QSlider* m_segmentationThresholdSlider = nullptr;
    QSlider* m_segmentationToleranceSlider = nullptr;
    QLabel* m_segmentationThresholdValueLabel = nullptr;
    QLabel* m_segmentationToleranceValueLabel = nullptr;
    QStackedWidget* m_segmentationPanelStack = nullptr;
    QWidget* m_algorithmSegmentationPage = nullptr;
    QWidget* m_segInfoPage = nullptr;
    QLabel* m_segInfoLabel = nullptr;
    QCheckBox* m_showSegmentationCheck = nullptr;
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
    QImage m_lastDegradedRawImage;
    RestorationModel m_lastDegradationModel = RestorationModel::AtmosphericTurbulence;
    RestorationParams m_lastDegradationParams;
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

    double m_lastNoiseDensity = 0.08;
    double m_lastGaussianMean = 0.0;
    double m_lastGaussianSigma = 15.0;
    int m_lastKernelSize = 3;
    int m_lastEdgeKernelSize = 3;
    int m_lastEdgeThreshold = 80;
    double m_lastGamma = 0.8;
    double m_lastFrequencyCutoff = 40.0;
    int m_lastFrequencyOrder = 2;
    double m_lastHomomorphicGammaLow = 0.5;
    double m_lastHomomorphicGammaHigh = 1.8;
    double m_lastHomomorphicC = 1.0;
    RestorationModel m_lastRestorationModel = RestorationModel::AtmosphericTurbulence;
    double m_lastRestorationTurbulenceK = 0.0025;
    double m_lastRestorationMotionA = 0.1;
    double m_lastRestorationMotionB = 0.1;
    double m_lastRestorationMotionT = 1.0;
    double m_lastRestorationCutoff = 30.0;
    double m_lastRestorationWienerK = 0.05;

    bool m_dicomMode = false;
    DicomSeriesData m_dicomSeries;
    QVector<QImage> m_windowedSlices;
    double m_cachedWindowWidth = -1.0;
    double m_cachedWindowCenter = std::numeric_limits<double>::lowest();
    QVector<QImage> m_dicomProcessedSlices;
    QVector<QImage> m_dicomSegmentationInputSlices;
    QVector<QImage> m_segmentationMaskSlices;
    SegmentationSurfaceData m_segmentationSurfaceCache;
    bool m_segmentationSurfaceDirty = true;
    bool m_segmentationSurfaceFinalReady = false;
    int m_segmentationSurfaceGeneration = 0;
    QFutureWatcher<SegmentationSurfaceData>* m_segmentationPreviewWatcher = nullptr;
    QFutureWatcher<SegmentationSurfaceData>* m_segmentationFullWatcher = nullptr;
    QVector<SegmentationSeedPoint> m_segmentationSeeds;
    QVector<QVector<SegmentationSeedPoint>> m_seedHistory;
    int m_seedHistoryIndex = -1;
    SeedEditMode m_seedEditMode = SeedEditMode::Navigate;
    DicomSegData m_loadedSegData;
    int m_dicomAxialIndex = 0;
    int m_dicomCoronalIndex = 0;
    int m_dicomSagittalIndex = 0;
    double m_dicomWindowWidth = 400.0;
    double m_dicomWindowCenter = 40.0;
    SegmentationMethod m_segmentationMethod = SegmentationMethod::Threshold;
    SegmentationParams m_segmentationParams;
    bool m_showSegmentation = true;
    SliceNavigationController* m_sliceNavigationController = nullptr;
};

