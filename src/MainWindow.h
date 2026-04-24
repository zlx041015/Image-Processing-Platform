#pragma once

#include <QImage>
#include <QMainWindow>
#include <QVector>
#include <memory>

class QListWidget;
class QLabel;
class QComboBox;
class ImageView;
class QListWidgetItem;
class QDragEnterEvent;
class QDropEvent;

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

private slots:
    void selectFolder();
    void openSingleFile();
    void onFileSelected(QListWidgetItem* item);
    void onFilterChanged();
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void updateHoverInfo(int x, int y, QRgb rgba);
    void updateHoverOutside();
    void showPixelInfo(int x, int y, QRgb rgba);
    void doLinearStretch();
    void doEqualize();
    void doHistMatch();

private:
    void createUi();
    void loadImageFiles(const QString& folder);
    void displayImage(const QString& filePath);
    void renderCurrentImage();
    void updateHistogram();
    void handleZoomWheel(int delta);
    bool isSupportedImageFile(const QString& filePath) const;
    QImage applyFilter(const QImage& img, const QString& filterName) const;
    QImage convolve(const QImage& img, const QVector<float>& kernel, float divisor, float bias = 0.0f) const;
    static int clampToByte(int value);

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
    QComboBox* m_filterCombo = nullptr;
    ImageView* m_imageView = nullptr;
};
