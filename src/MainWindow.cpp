#include "MainWindow.h"

#include "BMPReader.h"
#include "FourierExperimentWidget.h"
#include "ImageHistTools.h"
#include "ImageLabProcessor.h"
#include "ImageRestoration.h"
#include "ImageView.h"
#include "SegmentationMeshBuilder.h"
#include "Segmentation3DView.h"
#include "SegmentationOverlay.h"

#include <QComboBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QColor>
#include <QCheckBox>
#include <QInputDialog>
#include <QLabel>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QResizeEvent>
#include <QStatusBar>
#include <QStackedWidget>
#include <QSplitter>
#include <QtConcurrent>
#include <QStringList>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QToolBar>
#include <QTransform>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {
QImage rotateSagittalForDisplay(const QImage& image) {
    if (image.isNull()) {
        return {};
    }
    return image.transformed(QTransform().rotate(90.0), Qt::FastTransformation);
}

QPoint mapDisplayedSagittalPointToSource(int rotatedX, int rotatedY, int originalHeight) {
    return QPoint(rotatedY, std::max(0, originalHeight - 1 - rotatedX));
}
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("医学影像分析平台"));
    resize(1280, 800);
    setMinimumSize(1050, 680);
    setAcceptDrops(true);
    createUi();

    m_segmentationPreviewWatcher = new QFutureWatcher<SegmentationSurfaceData>(this);
    connect(m_segmentationPreviewWatcher, &QFutureWatcher<SegmentationSurfaceData>::finished, this, [this]() {
        const int generation = m_segmentationPreviewWatcher->property("generation").toInt();
        applySegmentationSurfaceResult(m_segmentationPreviewWatcher->result(), generation, false);
    });

    m_segmentationFullWatcher = new QFutureWatcher<SegmentationSurfaceData>(this);
    connect(m_segmentationFullWatcher, &QFutureWatcher<SegmentationSurfaceData>::finished, this, [this]() {
        const int generation = m_segmentationFullWatcher->property("generation").toInt();
        applySegmentationSurfaceResult(m_segmentationFullWatcher->result(), generation, true);
    });
}

MainWindow::~MainWindow() = default;

void MainWindow::createUi() {
    auto* workbench = new QWidget(this);
    setCentralWidget(workbench);
    m_sliceNavigationController = new SliceNavigationController(this);

    m_fileList = nullptr;
    m_filterCombo = nullptr;
    setWindowTitle(QStringLiteral("医学影像分析平台"));

    setStyleSheet(R"(
        QMainWindow, QWidget {
            color: #e5eef8;
            background: #0b111a;
        }
        QFrame#TopBar {
            background: #121c29;
            border: 1px solid #1f2d3d;
            border-radius: 8px;
        }
        QFrame#ImagePanel {
            background: #090d13;
            border: 1px solid #243244;
            border-radius: 8px;
        }
        QFrame#ImagePanel:hover {
            border-color: #33465f;
        }
        QLineEdit, QSpinBox, QSlider, QDoubleSpinBox {
            color: #e5eef8;
            background: #182436;
            border: 1px solid #33465f;
            border-radius: 5px;
            min-height: 26px;
            selection-background-color: #1e6f86;
        }
        QSpinBox, QDoubleSpinBox {
            padding-right: 18px;
        }
        QSpinBox::up-button, QDoubleSpinBox::up-button {
            subcontrol-origin: border;
            subcontrol-position: top right;
            width: 16px;
            border-left: 1px solid #33465f;
            border-bottom: 1px solid #263447;
            border-top-right-radius: 5px;
            background: #1b2a3e;
        }
        QSpinBox::down-button, QDoubleSpinBox::down-button {
            subcontrol-origin: border;
            subcontrol-position: bottom right;
            width: 16px;
            border-left: 1px solid #33465f;
            border-bottom-right-radius: 5px;
            background: #1b2a3e;
        }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover,
        QDoubleSpinBox::up-button:hover, QDoubleSpinBox::down-button:hover {
            background: #22344d;
            border-left-color: #38bdf8;
        }
        QLineEdit:hover, QSpinBox:hover, QSlider:hover, QDoubleSpinBox:hover {
            border-color: #4a5f78;
            background: #1b2a3e;
        }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus {
            border-color: #38bdf8;
            background: #182b42;
        }
        QComboBox {
            color: #dbe7f5;
            background: #182436;
            border: 1px solid #33465f;
            border-radius: 6px;
            padding: 5px 28px 5px 8px;
            min-height: 24px;
        }
        QComboBox:hover {
            color: #f8fbff;
            background: #22344d;
            border-color: #38bdf8;
        }
        QComboBox:focus {
            border-color: #38bdf8;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 24px;
            border-left: 1px solid #33465f;
        }
        QComboBox::down-arrow {
            image: none;
            width: 0px;
            height: 0px;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid #8fa3bb;
            margin-right: 8px;
        }
        QComboBox QAbstractItemView {
            color: #e5eef8;
            background: #121c29;
            border: 1px solid #33465f;
            selection-background-color: #22344d;
            selection-color: #ffffff;
            outline: 0;
        }
        QPushButton, QToolButton {
            color: #dbe7f5;
            background: #182436;
            border: 1px solid #33465f;
            border-radius: 6px;
            padding: 7px 12px;
        }
        QPushButton:hover, QToolButton:hover {
            color: #f8fbff;
            background: #22344d;
            border-color: #38bdf8;
        }
        QPushButton:pressed, QToolButton:pressed {
            background: #1b2f46;
            border-color: #2dd4bf;
        }
        QPushButton:focus, QToolButton:focus {
            border-color: #38bdf8;
        }
        QPushButton:disabled, QToolButton:disabled {
            color: #5f7085;
            background: #121b29;
            border-color: #243244;
        }
        QMenu {
            color: #e5eef8;
            background: #121c29;
            border: 1px solid #33465f;
            padding: 5px;
        }
        QMenu::item {
            padding: 7px 24px;
            border-radius: 5px;
        }
        QMenu::item:selected {
            color: #f8fbff;
            background: #22344d;
        }
        QMenu::item:disabled {
            color: #5f7085;
        }
        QToolTip {
            color: #e5eef8;
            background: #121c29;
            border: 1px solid #33465f;
            padding: 5px;
        }
        QToolButton::menu-indicator {
            image: none;
            width: 0px;
        }
        QLabel#PanelTitle {
            color: #e5eef8;
            font-weight: 700;
        }
        QLabel#MutedText {
            color: #9aaabd;
        }
        QLabel#HistogramPreview {
            color: #9aaabd;
            background: #0d141f;
            border: 1px solid #243244;
            border-radius: 6px;
        }
        QLabel#HistogramPreview:hover {
            border-color: #33465f;
        }
        QStatusBar {
            color: #9aaabd;
            background: #0b111a;
            border-top: 1px solid #1f2d3d;
        }
        QScrollBar:vertical, QScrollBar:horizontal {
            background: #0b111a;
            border: none;
            width: 10px;
            height: 10px;
            margin: 0px;
        }
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
            background: #243244;
            border-radius: 5px;
            min-height: 24px;
            min-width: 24px;
        }
        QScrollBar::handle:hover {
            background: #33465f;
        }
        QScrollBar::add-line, QScrollBar::sub-line {
            width: 0px;
            height: 0px;
        }
        QSlider::groove:horizontal {
            height: 4px;
            background: #243244;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            width: 14px;
            height: 14px;
            margin: -5px 0;
            background: #38bdf8;
            border: 1px solid #a7e7ff;
            border-radius: 7px;
        }
        QSlider::handle:horizontal:hover {
            background: #2dd4bf;
            border-color: #a7fff3;
        }
        QDialog {
            color: #e5eef8;
            background: #121c29;
        }
        QDialog QLabel {
            color: #e5eef8;
        }
        QFrame#StepperFrame {
            background: #182436;
            border: 1px solid #33465f;
            border-radius: 5px;
        }
        QFrame#StepperFrame:focus-within {
            border-color: #38bdf8;
        }
        QLineEdit#StepperEdit {
            border: none;
            border-radius: 0px;
            background: transparent;
            padding: 0 6px;
            min-height: 28px;
        }
        QPushButton#StepperButton {
            min-width: 18px;
            max-width: 18px;
            min-height: 14px;
            max-height: 14px;
            padding: 0px;
            border-radius: 0px;
            border: none;
            border-left: 1px solid #33465f;
            background: #1b2a3e;
            color: #e5eef8;
            font-size: 9px;
        }
        QPushButton#StepperButton:hover {
            background: #22344d;
            color: #ffffff;
        }
    )");

    auto* rootLayout = new QVBoxLayout(workbench);
    rootLayout->setContentsMargins(14, 12, 14, 12);
    rootLayout->setSpacing(10);

    auto* topBar = new QFrame(workbench);
    topBar->setObjectName(QStringLiteral("TopBar"));
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(12, 8, 12, 8);
    topLayout->setSpacing(8);

    auto* productTitle = new QLabel(QString::fromUtf8(u8"医学影像分析平台"), topBar);
    productTitle->setObjectName(QStringLiteral("PanelTitle"));
    topLayout->addWidget(productTitle);
    topLayout->addSpacing(8);

    auto addAction = [this](QMenu* menu, const QString& text) {
        QAction* action = menu->addAction(text);
        connect(action, &QAction::triggered, this, [this, text]() {
            applyProcessingAction(text);
        });
    };

    auto* inputMenu = new QMenu(this);
    QAction* openImageAction = inputMenu->addAction(QString::fromUtf8(u8"打开影像"), this, &MainWindow::openSingleFile);
    QAction* openFolderAction = inputMenu->addAction(QString::fromUtf8(u8"打开文件夹"), this, &MainWindow::selectFolder);
    QAction* importSegAction = inputMenu->addAction(QString::fromUtf8(u8"导入 SEG"), this, &MainWindow::importSegFile);
    inputMenu->addSeparator();
    QAction* saveResultAction = inputMenu->addAction(QString::fromUtf8(u8"保存结果"), this, [this]() {
        saveImageToFile(m_filteredImage, QStringLiteral("processed_result.png"));
    });

    auto* displayMenu = new QMenu(this);
    for (const QString& item : {QString::fromUtf8(u8"原图"), QString::fromUtf8(u8"灰度"), QString::fromUtf8(u8"反相"),
         QString::fromUtf8(u8"二值化"), QString::fromUtf8(u8"暖色"), QString::fromUtf8(u8"冷色"),
         QString::fromUtf8(u8"边缘增强"), QString::fromUtf8(u8"锐化")}) {
        addAction(displayMenu, item);
    }

    auto* histMenu = new QMenu(this);
    for (const QString& item : {QString::fromUtf8(u8"线性拉伸"), QString::fromUtf8(u8"均衡化"), QString::fromUtf8(u8"直方图匹配")}) {
        addAction(histMenu, item);
    }

    auto* noiseMenu = new QMenu(this);
    for (const QString& item : {QString::fromUtf8(u8"椒盐噪声"), QString::fromUtf8(u8"脉冲噪声"), QString::fromUtf8(u8"高斯噪声"),
         QString::fromUtf8(u8"均值滤波"), QString::fromUtf8(u8"中值滤波"), QString::fromUtf8(u8"最大值滤波")}) {
        addAction(noiseMenu, item);
    }

    auto* edgeMenu = new QMenu(this);
    for (const QString& item : {QString::fromUtf8(u8"Sobel"), QString::fromUtf8(u8"Prewitt"), QString::fromUtf8(u8"Laplacian")}) {
        addAction(edgeMenu, item);
    }

    auto* enhanceMenu = new QMenu(this);
    for (const QString& item : {QString::fromUtf8(u8"Laplacian 处理"), QString::fromUtf8(u8"锐化处理"),
         QString::fromUtf8(u8"Sobel 梯度"), QString::fromUtf8(u8"均值平滑梯度"),
         QString::fromUtf8(u8"掩膜增强"), QString::fromUtf8(u8"原图叠加掩膜"), QString::fromUtf8(u8"Gamma 变换")}) {
        addAction(enhanceMenu, item);
    }

    auto* frequencyMenu = new QMenu(this);
    for (const QString& item : {QString::fromUtf8(u8"FFT 频谱"), QString::fromUtf8(u8"IFFT 重建"),
         QString::fromUtf8(u8"理想低通"), QString::fromUtf8(u8"巴特沃斯低通"),
         QString::fromUtf8(u8"理想高通"), QString::fromUtf8(u8"巴特沃斯高通"), QString::fromUtf8(u8"同态滤波")}) {
        addAction(frequencyMenu, item);
    }

    auto* restorationMenu = new QMenu(this);
    for (const QString& item : {QString::fromUtf8(u8"图像退化"), QString::fromUtf8(u8"逆滤波复原"),
         QString::fromUtf8(u8"截止半径逆滤波"), QString::fromUtf8(u8"维纳滤波复原")}) {
        addAction(restorationMenu, item);
    }

    QMenuBar* nativeMenuBar = menuBar();
    nativeMenuBar->setNativeMenuBar(false);
    nativeMenuBar->clear();
    auto cloneMenuActions = [](QMenu* destination, const QMenu* source) {
        for (QAction* action : source->actions()) {
            destination->addAction(action);
        }
    };
    auto* fileNativeMenu = nativeMenuBar->addMenu(QString::fromUtf8(u8"文件"));
    auto* displayNativeMenu = nativeMenuBar->addMenu(QString::fromUtf8(u8"显示"));
    auto* histogramNativeMenu = nativeMenuBar->addMenu(QString::fromUtf8(u8"直方图增强"));
    auto* noiseNativeMenu = nativeMenuBar->addMenu(QString::fromUtf8(u8"噪声与滤波"));
    auto* edgeNativeMenu = nativeMenuBar->addMenu(QString::fromUtf8(u8"边缘结构"));
    auto* enhanceNativeMenu = nativeMenuBar->addMenu(QString::fromUtf8(u8"图像增强"));
    auto* frequencyNativeMenu = nativeMenuBar->addMenu(QString::fromUtf8(u8"频域分析"));
    auto* restorationNativeMenu = nativeMenuBar->addMenu(QString::fromUtf8(u8"图像复原"));
    cloneMenuActions(fileNativeMenu, inputMenu);
    cloneMenuActions(displayNativeMenu, displayMenu);
    cloneMenuActions(histogramNativeMenu, histMenu);
    cloneMenuActions(noiseNativeMenu, noiseMenu);
    cloneMenuActions(edgeNativeMenu, edgeMenu);
    cloneMenuActions(enhanceNativeMenu, enhanceMenu);
    cloneMenuActions(frequencyNativeMenu, frequencyMenu);
    cloneMenuActions(restorationNativeMenu, restorationMenu);

    auto* primaryToolBar = addToolBar(QString::fromUtf8(u8"快捷工具"));
    primaryToolBar->setObjectName(QStringLiteral("PrimaryToolBar"));
    primaryToolBar->setMovable(false);
    primaryToolBar->addAction(openImageAction);
    primaryToolBar->addAction(openFolderAction);
    primaryToolBar->addAction(importSegAction);
    primaryToolBar->addSeparator();
    primaryToolBar->addAction(saveResultAction);
    primaryToolBar->addSeparator();
    primaryToolBar->addAction(QString::fromUtf8(u8"运行分割"), this, &MainWindow::runDicomSegmentation);
    primaryToolBar->addAction(QString::fromUtf8(u8"清空种子"), this, &MainWindow::clearSegmentationSeeds);
    primaryToolBar->addAction(QString::fromUtf8(u8"移除 SEG"), this, &MainWindow::clearLoadedSeg);
    primaryToolBar->addSeparator();
    primaryToolBar->addAction(QString::fromUtf8(u8"放大"), this, &MainWindow::zoomIn);
    primaryToolBar->addAction(QString::fromUtf8(u8"缩小"), this, &MainWindow::zoomOut);
    primaryToolBar->addAction(QString::fromUtf8(u8"重置缩放"), this, &MainWindow::resetZoom);

    topLayout->addStretch(1);

    m_undoButton = new QPushButton(QString::fromUtf8(u8"撤回"), topBar);
    m_redoButton = new QPushButton(QString::fromUtf8(u8"前进"), topBar);
    m_resetButton = new QPushButton(QString::fromUtf8(u8"重置"), topBar);
    m_undoButton->setCursor(Qt::PointingHandCursor);
    m_redoButton->setCursor(Qt::PointingHandCursor);
    m_resetButton->setCursor(Qt::PointingHandCursor);
    topLayout->addWidget(m_undoButton);
    topLayout->addWidget(m_redoButton);
    topLayout->addWidget(m_resetButton);
    rootLayout->addWidget(topBar);

    m_viewModeStack = new QStackedWidget(workbench);

    auto buildImagePanel = [this](QWidget* parent, const QString& titleText, ImageView** viewPtr) {
        auto* panel = new QFrame(parent);
        panel->setObjectName(QStringLiteral("ImagePanel"));
        auto* layout = new QVBoxLayout(panel);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(8);
        auto* title = new QLabel(titleText, panel);
        title->setObjectName(QStringLiteral("PanelTitle"));
        auto* view = new ImageView(panel);
        view->setMinimumSize(420, 420);
        layout->addWidget(title);
        layout->addWidget(view, 1);
        *viewPtr = view;
        return panel;
    };

    m_standardViewPage = new QWidget(m_viewModeStack);
    auto* standardLayout = new QVBoxLayout(m_standardViewPage);
    standardLayout->setContentsMargins(0, 0, 0, 0);
    auto* imageRow = new QSplitter(Qt::Horizontal, m_standardViewPage);
    imageRow->setChildrenCollapsible(false);
    buildImagePanel(imageRow, QString::fromUtf8(u8"原始影像"), &m_imageView);
    buildImagePanel(imageRow, QString::fromUtf8(u8"处理结果"), &m_resultView);
    imageRow->setSizes({640, 640});
    standardLayout->addWidget(imageRow);
    m_viewModeStack->addWidget(m_standardViewPage);

    m_dicomViewPage = new QWidget(m_viewModeStack);
    auto* dicomLayout = new QVBoxLayout(m_dicomViewPage);
    dicomLayout->setContentsMargins(0, 0, 0, 0);
    dicomLayout->setSpacing(10);

    m_windowWidthSlider = new QSlider(Qt::Horizontal, workbench);
    m_windowCenterSlider = new QSlider(Qt::Horizontal, workbench);
    m_windowWidthSlider->setRange(1, 2000);
    m_windowCenterSlider->setRange(-1000, 1000);
    m_windowWidthValueLabel = new QLabel(QStringLiteral("400"), workbench);
    m_windowCenterValueLabel = new QLabel(QStringLiteral("40"), workbench);

    auto* dicomGrid = new QGridLayout();
    dicomGrid->setContentsMargins(0, 0, 0, 0);
    dicomGrid->setHorizontalSpacing(10);
    dicomGrid->setVerticalSpacing(10);

    auto makeDicomPanel = [this](QWidget* parent, const QString& titleText, ImageView** viewPtr, QSlider** sliderPtr) {
        auto* panel = new QFrame(parent);
        panel->setObjectName(QStringLiteral("ImagePanel"));
        auto* layout = new QVBoxLayout(panel);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(8);
        auto* title = new QLabel(titleText, panel);
        title->setObjectName(QStringLiteral("PanelTitle"));
        auto* view = new ImageView(panel);
        view->setMinimumSize(320, 260);
        auto* slider = new QSlider(Qt::Horizontal, panel);
        slider->setRange(0, 0);
        layout->addWidget(title);
        layout->addWidget(view, 1);
        layout->addWidget(slider);
        *viewPtr = view;
        *sliderPtr = slider;
        return panel;
    };

    auto* axialPanel = makeDicomPanel(m_dicomViewPage, QString::fromUtf8(u8"轴状位"), &m_axialView, &m_axialSlider);
    auto* coronalPanel = makeDicomPanel(m_dicomViewPage, QString::fromUtf8(u8"冠状位"), &m_coronalView, &m_coronalSlider);
    auto* sagittalPanel = makeDicomPanel(m_dicomViewPage, QString::fromUtf8(u8"矢状位"), &m_sagittalView, &m_sagittalSlider);
    auto* volumePanel = new QFrame(m_dicomViewPage);
    volumePanel->setObjectName(QStringLiteral("ImagePanel"));
    auto* volumeLayout = new QVBoxLayout(volumePanel);
    volumeLayout->setContentsMargins(10, 10, 10, 10);
    volumeLayout->setSpacing(8);
    auto* volumeTitle = new QLabel(QString::fromUtf8(u8"三维视图"), volumePanel);
    volumeTitle->setObjectName(QStringLiteral("PanelTitle"));
    m_volumeView = new Segmentation3DView(volumePanel);
    m_volumeView->setMinimumSize(320, 260);
    volumeLayout->addWidget(volumeTitle);
    volumeLayout->addWidget(m_volumeView, 1);

    dicomGrid->addWidget(axialPanel, 0, 0);
    dicomGrid->addWidget(volumePanel, 0, 1);
    dicomGrid->addWidget(coronalPanel, 1, 0);
    dicomGrid->addWidget(sagittalPanel, 1, 1);
    dicomGrid->setColumnStretch(0, 1);
    dicomGrid->setColumnStretch(1, 1);
    dicomGrid->setRowStretch(0, 1);
    dicomGrid->setRowStretch(1, 1);
    dicomLayout->addLayout(dicomGrid, 1);
    m_viewModeStack->addWidget(m_dicomViewPage);

    auto* workspacePage = new QWidget(workbench);
    auto* workspaceLayout = new QVBoxLayout(workspacePage);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(0);

    auto* workspaceSplitter = new QSplitter(Qt::Horizontal, workspacePage);
    workspaceSplitter->setChildrenCollapsible(false);

    auto* filePanel = new QFrame(workspaceSplitter);
    filePanel->setObjectName(QStringLiteral("ImagePanel"));
    auto* fileLayout = new QVBoxLayout(filePanel);
    fileLayout->setContentsMargins(10, 10, 10, 10);
    fileLayout->setSpacing(8);
    auto* fileTitle = new QLabel(QString::fromUtf8(u8"数据树 / 文件列表"), filePanel);
    fileTitle->setObjectName(QStringLiteral("PanelTitle"));
    m_folderLabel = new QLabel(QString::fromUtf8(u8"当前文件夹：未选择"), filePanel);
    m_folderLabel->setObjectName(QStringLiteral("MutedText"));
    m_folderLabel->setWordWrap(true);
    m_fileList = new QListWidget(filePanel);
    fileLayout->addWidget(fileTitle);
    fileLayout->addWidget(m_folderLabel);
    fileLayout->addWidget(m_fileList, 1);

    auto* centerPanel = new QFrame(workspaceSplitter);
    centerPanel->setObjectName(QStringLiteral("ImagePanel"));
    auto* centerLayout = new QVBoxLayout(centerPanel);
    centerLayout->setContentsMargins(10, 10, 10, 10);
    centerLayout->setSpacing(8);
    centerLayout->addWidget(m_viewModeStack, 1);

    auto* parameterPanel = new QFrame(workspaceSplitter);
    parameterPanel->setObjectName(QStringLiteral("ImagePanel"));
    auto* parameterShellLayout = new QVBoxLayout(parameterPanel);
    parameterShellLayout->setContentsMargins(10, 10, 10, 10);
    parameterShellLayout->setSpacing(0);

    auto* parameterScrollArea = new QScrollArea(parameterPanel);
    parameterScrollArea->setWidgetResizable(true);
    parameterScrollArea->setFrameShape(QFrame::NoFrame);
    parameterScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    parameterScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    parameterScrollArea->setObjectName(QStringLiteral("ParameterScrollArea"));

    auto* parameterContent = new QWidget(parameterScrollArea);
    auto* parameterLayout = new QVBoxLayout(parameterContent);
    parameterLayout->setContentsMargins(0, 0, 0, 0);
    parameterLayout->setSpacing(8);

    auto* parameterTitle = new QLabel(QString::fromUtf8(u8"统一参数面板"), parameterContent);
    parameterTitle->setObjectName(QStringLiteral("PanelTitle"));
    m_infoLabel = new QPlainTextEdit(parameterContent);
    m_infoLabel->setObjectName(QStringLiteral("MutedText"));
    m_infoLabel->setReadOnly(true);
    m_infoLabel->setMinimumHeight(120);
    m_infoLabel->setMaximumHeight(180);
    m_infoLabel->setPlainText(QString::fromUtf8(u8"未加载影像"));
    m_chainLabel = new QLabel(QString::fromUtf8(u8"当前处理：无"), parameterContent);
    m_chainLabel->setObjectName(QStringLiteral("MutedText"));
    m_parameterLabel = new QLabel(QString::fromUtf8(u8"参数记录：无"), parameterContent);
    m_parameterLabel->setObjectName(QStringLiteral("MutedText"));
    m_chainCombo = new QComboBox(parameterContent);
    m_chainCombo->setCursor(Qt::PointingHandCursor);
    m_chainCombo->setMinimumWidth(260);
    m_chainCombo->addItem(QString::fromUtf8(u8"当前处理：无"));
    m_parameterCombo = new QComboBox(parameterContent);
    m_parameterCombo->setCursor(Qt::PointingHandCursor);
    m_parameterCombo->setMinimumWidth(260);
    m_parameterCombo->addItem(QString::fromUtf8(u8"参数记录：无"));
    m_zoomLabel = new QLabel(QStringLiteral("100%"), parameterContent);
    m_zoomLabel->setObjectName(QStringLiteral("MutedText"));
    m_histogramLabel = new QLabel(QString::fromUtf8(u8"直方图"), parameterContent);
    m_histogramLabel->setObjectName(QStringLiteral("HistogramPreview"));
    m_histogramLabel->setFixedSize(180, 80);
    m_histogramLabel->setAlignment(Qt::AlignCenter);

    auto* wwLabel = new QLabel(QString::fromUtf8(u8"窗宽"), parameterContent);
    wwLabel->setObjectName(QStringLiteral("PanelTitle"));
    auto* wcLabel = new QLabel(QString::fromUtf8(u8"窗位"), parameterContent);
    wcLabel->setObjectName(QStringLiteral("PanelTitle"));
    auto* wwRow = new QHBoxLayout();
    wwRow->addWidget(wwLabel);
    wwRow->addWidget(m_windowWidthSlider, 1);
    wwRow->addWidget(m_windowWidthValueLabel);
    auto* wcRow = new QHBoxLayout();
    wcRow->addWidget(wcLabel);
    wcRow->addWidget(m_windowCenterSlider, 1);
    wcRow->addWidget(m_windowCenterValueLabel);
    auto* processScopeLabel = new QLabel(QString::fromUtf8(u8"处理范围"), parameterContent);
    processScopeLabel->setObjectName(QStringLiteral("PanelTitle"));
    m_dicomProcessingScopeCombo = new QComboBox(parameterContent);
    m_dicomProcessingScopeCombo->addItem(QString::fromUtf8(u8"当前切片"));
    m_dicomProcessingScopeCombo->addItem(QString::fromUtf8(u8"全体数据"));
    auto* processTargetLabel = new QLabel(QString::fromUtf8(u8"处理目标"), parameterContent);
    processTargetLabel->setObjectName(QStringLiteral("PanelTitle"));
    m_dicomProcessingTargetCombo = new QComboBox(parameterContent);
    m_dicomProcessingTargetCombo->addItem(QString::fromUtf8(u8"原图"));
    m_dicomProcessingTargetCombo->addItem(QString::fromUtf8(u8"分割前预处理"));
    m_dicomProcessingTargetCombo->addItem(QString::fromUtf8(u8"分割后显示"));

    auto* segMethodLabel = new QLabel(QString::fromUtf8(u8"分割方法"), parameterContent);
    segMethodLabel->setObjectName(QStringLiteral("PanelTitle"));
    m_segmentationMethodCombo = new QComboBox(parameterContent);
    m_segmentationMethodCombo->addItem(QString::fromUtf8(u8"阈值分割"));
    m_segmentationMethodCombo->addItem(QString::fromUtf8(u8"Otsu 自动阈值"));
    m_segmentationMethodCombo->addItem(QString::fromUtf8(u8"多种子区域生长"));
    auto* seedModeLabel = new QLabel(QString::fromUtf8(u8"种子模式"), parameterContent);
    seedModeLabel->setObjectName(QStringLiteral("PanelTitle"));
    m_seedModeCombo = new QComboBox(parameterContent);
    m_seedModeCombo->addItem(QString::fromUtf8(u8"仅定位（不加种子）"));
    m_seedModeCombo->addItem(QString::fromUtf8(u8"添加前景种子"));
    m_seedModeCombo->addItem(QString::fromUtf8(u8"添加背景种子"));
    auto* segThresholdLabel = new QLabel(QString::fromUtf8(u8"阈值"), parameterContent);
    segThresholdLabel->setObjectName(QStringLiteral("PanelTitle"));
    m_segmentationThresholdSlider = new QSlider(Qt::Horizontal, parameterContent);
    m_segmentationThresholdSlider->setRange(0, 255);
    m_segmentationThresholdSlider->setValue(m_segmentationParams.threshold);
    m_segmentationThresholdValueLabel = new QLabel(QString::number(m_segmentationParams.threshold), parameterContent);
    auto* segToleranceLabel = new QLabel(QString::fromUtf8(u8"容差"), parameterContent);
    segToleranceLabel->setObjectName(QStringLiteral("PanelTitle"));
    m_segmentationToleranceSlider = new QSlider(Qt::Horizontal, parameterContent);
    m_segmentationToleranceSlider->setRange(1, 80);
    m_segmentationToleranceSlider->setValue(m_segmentationParams.tolerance);
    m_segmentationToleranceValueLabel = new QLabel(QString::number(m_segmentationParams.tolerance), parameterContent);
    m_showSegmentationCheck = new QCheckBox(QString::fromUtf8(u8"显示分割"), parameterContent);
    m_showSegmentationCheck->setChecked(true);
    m_segmentationPanelStack = new QStackedWidget(parameterContent);
    m_algorithmSegmentationPage = new QWidget(m_segmentationPanelStack);
    auto* algorithmSegLayout = new QVBoxLayout(m_algorithmSegmentationPage);
    algorithmSegLayout->setContentsMargins(0, 0, 0, 0);
    algorithmSegLayout->setSpacing(8);
    auto* segThresholdRow = new QHBoxLayout();
    segThresholdRow->addWidget(segThresholdLabel);
    segThresholdRow->addWidget(m_segmentationThresholdSlider, 1);
    segThresholdRow->addWidget(m_segmentationThresholdValueLabel);
    auto* segToleranceRow = new QHBoxLayout();
    segToleranceRow->addWidget(segToleranceLabel);
    segToleranceRow->addWidget(m_segmentationToleranceSlider, 1);
    segToleranceRow->addWidget(m_segmentationToleranceValueLabel);
    auto* segToolbarHint = new QLabel(QString::fromUtf8(u8"分割操作请使用上方工具栏：运行分割 / 清空种子 / 移除 SEG"), m_algorithmSegmentationPage);
    segToolbarHint->setObjectName(QStringLiteral("MutedText"));
    segToolbarHint->setWordWrap(true);
    algorithmSegLayout->addWidget(segMethodLabel);
    algorithmSegLayout->addWidget(m_segmentationMethodCombo);
    algorithmSegLayout->addWidget(seedModeLabel);
    algorithmSegLayout->addWidget(m_seedModeCombo);
    algorithmSegLayout->addLayout(segThresholdRow);
    algorithmSegLayout->addLayout(segToleranceRow);
    algorithmSegLayout->addWidget(segToolbarHint);

    m_segInfoPage = new QWidget(m_segmentationPanelStack);
    auto* segInfoLayout = new QVBoxLayout(m_segInfoPage);
    segInfoLayout->setContentsMargins(0, 0, 0, 0);
    segInfoLayout->setSpacing(8);
    auto* segSourceTitle = new QLabel(QString::fromUtf8(u8"SEG 信息"), parameterContent);
    segSourceTitle->setObjectName(QStringLiteral("PanelTitle"));
    m_segInfoLabel = new QLabel(QString::fromUtf8(u8"当前未导入 SEG"), parameterContent);
    m_segInfoLabel->setObjectName(QStringLiteral("MutedText"));
    m_segInfoLabel->setWordWrap(true);
    segInfoLayout->addWidget(segSourceTitle);
    segInfoLayout->addWidget(m_segInfoLabel);
    segInfoLayout->addWidget(m_showSegmentationCheck);

    m_segmentationPanelStack->addWidget(m_algorithmSegmentationPage);
    m_segmentationPanelStack->addWidget(m_segInfoPage);
    m_segmentationPanelStack->setCurrentWidget(m_algorithmSegmentationPage);

    parameterLayout->addWidget(parameterTitle);
    parameterLayout->addWidget(m_infoLabel);
    parameterLayout->addLayout(wwRow);
    parameterLayout->addLayout(wcRow);
    parameterLayout->addWidget(processScopeLabel);
    parameterLayout->addWidget(m_dicomProcessingScopeCombo);
    parameterLayout->addWidget(processTargetLabel);
    parameterLayout->addWidget(m_dicomProcessingTargetCombo);
    parameterLayout->addWidget(m_segmentationPanelStack);
    parameterLayout->addWidget(m_chainCombo);
    parameterLayout->addWidget(m_parameterCombo);
    parameterLayout->addWidget(m_histogramLabel, 0, Qt::AlignCenter);
    parameterLayout->addWidget(m_zoomLabel, 0, Qt::AlignLeft);
    parameterLayout->addStretch(1);
    parameterScrollArea->setWidget(parameterContent);
    parameterShellLayout->addWidget(parameterScrollArea, 1);

    workspaceSplitter->addWidget(filePanel);
    workspaceSplitter->addWidget(centerPanel);
    workspaceSplitter->addWidget(parameterPanel);
    parameterPanel->setMinimumWidth(340);
    workspaceSplitter->setSizes({220, 820, 360});
    workspaceLayout->addWidget(workspaceSplitter, 1);

    rootLayout->addWidget(workspacePage, 1);

    connect(m_imageView, &ImageView::hoverPixel, this, &MainWindow::updateHoverInfo);
    connect(m_imageView, &ImageView::clickPixel, this, &MainWindow::showPixelInfo);
    connect(m_imageView, &ImageView::hoverOutside, this, &MainWindow::updateHoverOutside);
    connect(m_imageView, &ImageView::zoomRequested, this, &MainWindow::handleZoomWheel);
    connect(m_resultView, &ImageView::hoverPixel, this, &MainWindow::updateHoverInfo);
    connect(m_resultView, &ImageView::clickPixel, this, &MainWindow::showPixelInfo);
    connect(m_resultView, &ImageView::hoverOutside, this, &MainWindow::updateHoverOutside);
    connect(m_resultView, &ImageView::zoomRequested, this, &MainWindow::handleZoomWheel);
    connect(m_axialView, &ImageView::zoomRequested, this, &MainWindow::handleZoomWheel);
    connect(m_coronalView, &ImageView::zoomRequested, this, &MainWindow::handleZoomWheel);
    connect(m_sagittalView, &ImageView::zoomRequested, this, &MainWindow::handleZoomWheel);
    connect(m_axialSlider, &QSlider::valueChanged, this, &MainWindow::updateDicomSliceIndex);
    connect(m_coronalSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_sliceNavigationController) {
            m_sliceNavigationController->setCoronalIndex(value);
        }
    });
    connect(m_sagittalSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_sliceNavigationController) {
            m_sliceNavigationController->setSagittalIndex(value);
        }
    });
    connect(m_windowWidthSlider, &QSlider::valueChanged, this, &MainWindow::updateDicomWindowWidth);
    connect(m_windowCenterSlider, &QSlider::valueChanged, this, &MainWindow::updateDicomWindowCenter);
    connect(m_segmentationMethodCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index == 0) {
            m_segmentationMethod = SegmentationMethod::Threshold;
        } else if (index == 1) {
            m_segmentationMethod = SegmentationMethod::Otsu;
        } else {
            m_segmentationMethod = SegmentationMethod::RegionGrow;
        }
        if (m_segmentationMethod != SegmentationMethod::RegionGrow) {
            m_seedEditMode = SeedEditMode::Navigate;
            if (m_seedModeCombo) {
                m_seedModeCombo->blockSignals(true);
                m_seedModeCombo->setCurrentIndex(0);
                m_seedModeCombo->blockSignals(false);
            }
            if (!m_segmentationSeeds.isEmpty()) {
                pushSeedHistorySnapshot({});
                applyCurrentSeedHistory(QString::fromUtf8(u8"已切换分割方法，种子已清空"));
            }
        }
        updateSegmentationControls();
    });
    connect(m_seedModeCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        SeedEditMode newMode = SeedEditMode::Navigate;
        if (index == 1) {
            newMode = SeedEditMode::Foreground;
        } else if (index == 2) {
            newMode = SeedEditMode::Background;
        }
        if (newMode == m_seedEditMode) {
            return;
        }
        m_seedEditMode = newMode;
        if (!m_segmentationSeeds.isEmpty()) {
            pushSeedHistorySnapshot({});
            applyCurrentSeedHistory(QString::fromUtf8(u8"已切换种子模式，原有种子已释放"));
        } else {
            rebuildSegmentationPreview();
            updateProcessingStatus(QString::fromUtf8(u8"已切换种子模式"));
        }
    });
    connect(m_segmentationThresholdSlider, &QSlider::valueChanged, this, [this](int value) {
        m_segmentationParams.threshold = value;
        if (m_segmentationThresholdValueLabel) {
            m_segmentationThresholdValueLabel->setText(QString::number(value));
        }
    });
    connect(m_segmentationToleranceSlider, &QSlider::valueChanged, this, [this](int value) {
        m_segmentationParams.tolerance = value;
        if (m_segmentationToleranceValueLabel) {
            m_segmentationToleranceValueLabel->setText(QString::number(value));
        }
    });
    connect(m_showSegmentationCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_showSegmentation = checked;
        rebuildSegmentationPreview();
    });
    connect(m_fileList, &QListWidget::itemClicked, this, &MainWindow::onFileSelected);
    connect(m_axialView, &ImageView::clickPixel, this, [this](int x, int y, QRgb) {
        if (m_sliceNavigationController) {
            m_sliceNavigationController->pickFromAxialView(x, y);
        }
        if (isSeedEditingEnabled()) {
            appendSeedPoint({x, y, m_dicomAxialIndex, m_seedEditMode == SeedEditMode::Foreground});
        } else {
            rebuildSegmentationPreview();
        }
    });
    connect(m_coronalView, &ImageView::clickPixel, this, [this](int x, int y, QRgb) {
        if (m_sliceNavigationController) {
            m_sliceNavigationController->pickFromCoronalView(x, y);
        }
        if (isSeedEditingEnabled()) {
            appendSeedPoint({x, m_dicomCoronalIndex, m_dicomAxialIndex, m_seedEditMode == SeedEditMode::Foreground});
        } else {
            rebuildSegmentationPreview();
        }
    });
    connect(m_sagittalView, &ImageView::clickPixel, this, [this](int x, int y, QRgb) {
        const QPoint sourcePoint = mapDisplayedSagittalPointToSource(x, y, m_dicomSeries.height);
        if (m_sliceNavigationController) {
            m_sliceNavigationController->pickFromSagittalView(sourcePoint.x(), sourcePoint.y());
        }
        if (isSeedEditingEnabled()) {
            appendSeedPoint({
                m_dicomSagittalIndex,
                sourcePoint.y(),
                m_dicomAxialIndex,
                m_seedEditMode == SeedEditMode::Foreground
            });
        } else {
            rebuildSegmentationPreview();
        }
    });
    connect(m_sliceNavigationController, &SliceNavigationController::stateChanged, this, &MainWindow::onSliceNavigationChanged);
    connect(m_undoButton, &QPushButton::clicked, this, &MainWindow::undoProcessing);
    connect(m_redoButton, &QPushButton::clicked, this, &MainWindow::redoProcessing);
    connect(m_resetButton, &QPushButton::clicked, this, &MainWindow::resetProcessing);

    refreshWorkbenchImages();
    switchToDicomMode(false);
    updateSegmentationControls();
    updateProcessingStatus(QString::fromUtf8(u8"请选择影像"));
}

QWidget* MainWindow::createExperimentPage() {
    auto* page = new QWidget(this);
    page->setObjectName(QStringLiteral("labExperimentPage"));

    auto* rootLayout = new QVBoxLayout(page);
    rootLayout->setContentsMargins(0, 6, 0, 0);
    rootLayout->setSpacing(8);

    auto* body = new QFrame(page);
    body->setObjectName("Card");
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(12, 12, 12, 12);
    bodyLayout->setSpacing(10);

    auto* splitter = new QSplitter(Qt::Horizontal, body);
    splitter->setChildrenCollapsible(false);

    auto* leftPane = new QFrame(splitter);
    leftPane->setObjectName("Card");
    auto* leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(12, 12, 12, 12);
    leftLayout->setSpacing(8);
    auto* leftTitle = new QLabel(QString::fromUtf8(u8"实验功能模块"), leftPane);
    leftTitle->setObjectName("SectionTitle");
    leftLayout->addWidget(leftTitle);

    m_experimentList = new QListWidget(leftPane);
    m_experimentList->setObjectName(QStringLiteral("labExperimentList"));
    m_experimentList->addItem(QString::fromUtf8(u8"噪声建模与抑制"));
    m_experimentList->addItem(QString::fromUtf8(u8"边缘结构分析"));
    m_experimentList->addItem(QString::fromUtf8(u8"图像增强处理"));
    leftLayout->addWidget(m_experimentList, 1);

    auto* rightPane = new QFrame(splitter);
    rightPane->setObjectName("Card");
    auto* rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(12, 12, 12, 12);
    rightLayout->setSpacing(8);
    auto* rightTitle = new QLabel(QString::fromUtf8(u8"实验处理区"), rightPane);
    rightTitle->setObjectName("SectionTitle");
    rightLayout->addWidget(rightTitle);

    m_experimentStackedWidget = new QStackedWidget(rightPane);
    m_experimentStackedWidget->setObjectName(QStringLiteral("labExperimentStackedWidget"));

    auto* emptyPage = new QWidget(m_experimentStackedWidget);
    auto* emptyLayout = new QVBoxLayout(emptyPage);
    emptyLayout->addStretch(1);
    auto* emptyHint = new QLabel(QString::fromUtf8(u8"请选择左侧功能模块"), emptyPage);
    emptyHint->setAlignment(Qt::AlignCenter);
    emptyHint->setObjectName("SmallLabel");
    emptyLayout->addWidget(emptyHint);
    emptyLayout->addStretch(1);
    m_experimentStackedWidget->addWidget(emptyPage);

    m_noiseExperimentPage = createNoiseExperimentPage();
    m_experimentStackedWidget->addWidget(m_noiseExperimentPage);

    m_edgeExperimentPage = createEdgeExperimentPage();
    m_experimentStackedWidget->addWidget(m_edgeExperimentPage);

    m_enhancementExperimentPage = createEnhancementExperimentPage();
    m_experimentStackedWidget->addWidget(m_enhancementExperimentPage);

    rightLayout->addWidget(m_experimentStackedWidget, 1);
    bodyLayout->addWidget(splitter, 1);
    rootLayout->addWidget(body, 1);

    m_experimentStatusLabel = new QLabel(QString::fromUtf8(u8"状态：未选择功能模块"), page);
    m_experimentStatusLabel->setObjectName("SmallLabel");
    m_experimentStatusLabel->setMinimumHeight(28);
    rootLayout->addWidget(m_experimentStatusLabel);

    connect(m_experimentList, &QListWidget::currentRowChanged, this, &MainWindow::onExperimentSelected);
    m_experimentList->setCurrentRow(-1);
    m_experimentStackedWidget->setCurrentIndex(0);

    return page;
}

void MainWindow::onExperimentSelected(int row) {
    if (!m_experimentStackedWidget || !m_experimentStatusLabel) {
        return;
    }

    if (row < 0) {
        m_experimentStackedWidget->setCurrentIndex(0);
        m_experimentStatusLabel->setText(QString::fromUtf8(u8"状态：未选择功能模块"));
        statusBar()->showMessage(QString::fromUtf8(u8"已进入空间域分析·请选择左侧功能模块"));
        return;
    }

    const QVector<int> pageMap = {1, 2, 3};
    if (row >= pageMap.size()) {
        return;
    }

    m_experimentStackedWidget->setCurrentIndex(pageMap[row]);
    const QStringList names = {
        QString::fromUtf8(u8"噪声建模与抑制"),
        QString::fromUtf8(u8"边缘结构分析"),
        QString::fromUtf8(u8"图像增强处理")
    };
    const QString name = (row >= 0 && row < names.size()) ? names[row] : QString();
    m_experimentStatusLabel->setText(QString::fromUtf8(u8"\u72b6\u6001\uff1a\u5df2\u9009\u62e9 ") + name);
    statusBar()->showMessage(QString::fromUtf8(u8"当前功能：") + name + QString::fromUtf8(u8" · 已加载处理工作区"));
}

QWidget* MainWindow::createNoiseExperimentPage() {
    auto* page = new QWidget(m_experimentStackedWidget);
    page->setObjectName(QStringLiteral("labNoiseExperimentPage"));

    auto* rootLayout = new QVBoxLayout(page);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(12);

    auto* controlCard = new QFrame(page);
    controlCard->setObjectName("Card");
    auto* controlLayout = new QGridLayout(controlCard);
    controlLayout->setContentsMargins(16, 16, 16, 16);
    controlLayout->setHorizontalSpacing(12);
    controlLayout->setVerticalSpacing(10);

    m_noiseLoadButton = new QPushButton(QString::fromUtf8(u8"选择影像"), controlCard);
    m_noiseInputPathEdit = new QLineEdit(controlCard);
    m_noiseInputPathEdit->setReadOnly(true);
    m_noiseInputPathEdit->setPlaceholderText(QString::fromUtf8(u8"选择一张灰度影像"));

    m_noiseTypeCombo = new QComboBox(controlCard);
    m_noiseTypeCombo->addItems({
        QString::fromUtf8(u8"椒盐噪声"),
        QString::fromUtf8(u8"脉冲噪声"),
        QString::fromUtf8(u8"高斯噪声")
    });

    m_noiseStrengthSlider = new QSlider(Qt::Horizontal, controlCard);
    m_noiseStrengthSlider->setRange(0, 100);
    m_noiseStrengthSlider->setValue(8);
    m_noiseStrengthValueLabel = new QLabel(QString::fromUtf8(u8"8%"), controlCard);
    m_noiseStrengthValueLabel->setObjectName("SectionTitle");

    m_noiseFilterCombo = new QComboBox(controlCard);
    m_noiseFilterCombo->addItems({
        QString::fromUtf8(u8"均值滤波"),
        QString::fromUtf8(u8"中值滤波"),
        QString::fromUtf8(u8"最大值滤波")
    });

    m_noiseKernelSpin = new QSpinBox(controlCard);
    m_noiseKernelSpin->setRange(3, 15);
    m_noiseKernelSpin->setSingleStep(2);
    m_noiseKernelSpin->setValue(3);

    m_noiseAddButton = new QPushButton(QString::fromUtf8(u8"添加噪声"), controlCard);
    m_noiseFilterButton = new QPushButton(QString::fromUtf8(u8"应用滤波"), controlCard);
    m_noiseSaveButton = new QPushButton(QString::fromUtf8(u8"保存结果"), controlCard);
    m_noiseResetButton = new QPushButton(QString::fromUtf8(u8"重置"), controlCard);

    controlLayout->addWidget(new QLabel(QString::fromUtf8(u8"输入图像"), controlCard), 0, 0);
    controlLayout->addWidget(m_noiseLoadButton, 0, 1);
    controlLayout->addWidget(m_noiseInputPathEdit, 0, 2, 1, 3);
    controlLayout->addWidget(new QLabel(QString::fromUtf8(u8"噪声类型"), controlCard), 1, 0);
    controlLayout->addWidget(m_noiseTypeCombo, 1, 1);
    controlLayout->addWidget(new QLabel(QString::fromUtf8(u8"噪声强度"), controlCard), 1, 2);
    controlLayout->addWidget(m_noiseStrengthSlider, 1, 3);
    controlLayout->addWidget(m_noiseStrengthValueLabel, 1, 4);
    controlLayout->addWidget(new QLabel(QString::fromUtf8(u8"滤波算法"), controlCard), 2, 0);
    controlLayout->addWidget(m_noiseFilterCombo, 2, 1);
    controlLayout->addWidget(new QLabel(QString::fromUtf8(u8"模板大小"), controlCard), 2, 2);
    controlLayout->addWidget(m_noiseKernelSpin, 2, 3);
    controlLayout->addWidget(m_noiseAddButton, 3, 1);
    controlLayout->addWidget(m_noiseFilterButton, 3, 2);
    controlLayout->addWidget(m_noiseSaveButton, 3, 3);
    controlLayout->addWidget(m_noiseResetButton, 3, 4);

    auto* previewCard = new QFrame(page);
    previewCard->setObjectName("Card");
    auto* previewLayout = new QGridLayout(previewCard);
    previewLayout->setContentsMargins(16, 16, 16, 16);
    previewLayout->setHorizontalSpacing(12);
    previewLayout->setVerticalSpacing(12);

    auto buildPreviewColumn = [](QWidget* parent, const QString& titleText, QLabel** imageLabel) {
        auto* column = new QVBoxLayout();
        auto* title = new QLabel(titleText, parent);
        title->setObjectName("SectionTitle");
        auto* image = new QLabel(QString::fromUtf8(u8"等待加载"), parent);
        image->setAlignment(Qt::AlignCenter);
        image->setMinimumSize(320, 260);
        image->setObjectName("ImageView");
        image->setWordWrap(true);
        column->addWidget(title);
        column->addWidget(image, 1);
        *imageLabel = image;
        return column;
    };

    previewLayout->addLayout(buildPreviewColumn(previewCard, QString::fromUtf8(u8"原图像"), &m_noiseOriginalLabel), 0, 0);
    previewLayout->addLayout(buildPreviewColumn(previewCard, QString::fromUtf8(u8"加噪后图像"), &m_noiseNoisyLabel), 0, 1);
    previewLayout->addLayout(buildPreviewColumn(previewCard, QString::fromUtf8(u8"去噪后图像"), &m_noiseDenoisedLabel), 0, 2);
    previewLayout->setColumnStretch(0, 1);
    previewLayout->setColumnStretch(1, 1);
    previewLayout->setColumnStretch(2, 1);

    rootLayout->addWidget(controlCard);
    rootLayout->addWidget(previewCard, 1);

    connect(m_noiseLoadButton, &QPushButton::clicked, this, &MainWindow::loadNoiseExperimentImage);
    connect(m_noiseAddButton, &QPushButton::clicked, this, &MainWindow::addNoiseToExperiment);
    connect(m_noiseFilterButton, &QPushButton::clicked, this, &MainWindow::applyNoiseFilterToExperiment);
    connect(m_noiseSaveButton, &QPushButton::clicked, this, &MainWindow::saveNoiseExperimentResult);
    connect(m_noiseResetButton, &QPushButton::clicked, this, &MainWindow::resetNoiseExperiment);
    connect(m_noiseTypeCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onExperimentNoiseControlsChanged);
    connect(m_noiseFilterCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onExperimentNoiseControlsChanged);
    connect(m_noiseStrengthSlider, &QSlider::valueChanged, this, &MainWindow::onExperimentNoiseControlsChanged);
    connect(m_noiseKernelSpin, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::onExperimentNoiseControlsChanged);

    updateExperimentImageLabel(m_noiseOriginalLabel, {});
    updateExperimentImageLabel(m_noiseNoisyLabel, {});
    updateExperimentImageLabel(m_noiseDenoisedLabel, {});
    return page;
}

QWidget* MainWindow::createEdgeExperimentPage() {
    auto* page = new QWidget(m_experimentStackedWidget);
    page->setObjectName(QStringLiteral("labEdgeExperimentPage"));

    auto* rootLayout = new QVBoxLayout(page);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(12);

    auto* controlCard = new QFrame(page);
    controlCard->setObjectName("Card");
    auto* controlLayout = new QGridLayout(controlCard);
    controlLayout->setContentsMargins(16, 16, 16, 16);
    controlLayout->setHorizontalSpacing(12);
    controlLayout->setVerticalSpacing(10);

    m_edgeLoadButton = new QPushButton(QString::fromUtf8(u8"选择影像"), controlCard);
    m_edgeInputPathEdit = new QLineEdit(controlCard);
    m_edgeInputPathEdit->setReadOnly(true);
    m_edgeInputPathEdit->setPlaceholderText(QString::fromUtf8(u8"选择一张灰度影像"));

    m_edgeTypeCombo = new QComboBox(controlCard);
    m_edgeTypeCombo->addItems({
        QString::fromUtf8(u8"Sobel算子"),
        QString::fromUtf8(u8"Prewitt算子"),
        QString::fromUtf8(u8"Laplacian算子")
    });

    m_edgeKernelSpin = new QSpinBox(controlCard);
    m_edgeKernelSpin->setRange(3, 5);
    m_edgeKernelSpin->setSingleStep(2);
    m_edgeKernelSpin->setValue(3);

    m_edgeThresholdSlider = new QSlider(Qt::Horizontal, controlCard);
    m_edgeThresholdSlider->setRange(0, 255);
    m_edgeThresholdSlider->setValue(80);
    m_edgeThresholdValueLabel = new QLabel(QStringLiteral("80"), controlCard);
    m_edgeThresholdValueLabel->setObjectName("SectionTitle");

    m_edgeApplyButton = new QPushButton(QString::fromUtf8(u8"应用检测"), controlCard);
    m_edgeResetButton = new QPushButton(QString::fromUtf8(u8"重置"), controlCard);
    m_edgeSaveButton = new QPushButton(QString::fromUtf8(u8"保存结果"), controlCard);

    controlLayout->addWidget(new QLabel(QString::fromUtf8(u8"输入图像"), controlCard), 0, 0);
    controlLayout->addWidget(m_edgeLoadButton, 0, 1);
    controlLayout->addWidget(m_edgeInputPathEdit, 0, 2, 1, 3);
    controlLayout->addWidget(new QLabel(QString::fromUtf8(u8"边缘算法"), controlCard), 1, 0);
    controlLayout->addWidget(m_edgeTypeCombo, 1, 1);
    controlLayout->addWidget(new QLabel(QString::fromUtf8(u8"模板大小"), controlCard), 1, 2);
    controlLayout->addWidget(m_edgeKernelSpin, 1, 3);
    controlLayout->addWidget(new QLabel(QString::fromUtf8(u8"阈值"), controlCard), 1, 4);
    controlLayout->addWidget(m_edgeThresholdSlider, 1, 5);
    controlLayout->addWidget(m_edgeThresholdValueLabel, 1, 6);
    controlLayout->addWidget(m_edgeApplyButton, 2, 2);
    controlLayout->addWidget(m_edgeSaveButton, 2, 3);
    controlLayout->addWidget(m_edgeResetButton, 2, 4);
    controlLayout->setColumnStretch(2, 2);
    controlLayout->setColumnStretch(5, 2);

    auto* previewCard = new QFrame(page);
    previewCard->setObjectName("Card");
    auto* previewLayout = new QGridLayout(previewCard);
    previewLayout->setContentsMargins(16, 16, 16, 16);
    previewLayout->setHorizontalSpacing(12);
    previewLayout->setVerticalSpacing(12);

    auto buildPreviewColumn = [](QWidget* parent, const QString& titleText, QLabel** imageLabel) {
        auto* column = new QVBoxLayout();
        auto* title = new QLabel(titleText, parent);
        title->setObjectName("SectionTitle");
        auto* image = new QLabel(QString::fromUtf8(u8"等待加载"), parent);
        image->setAlignment(Qt::AlignCenter);
        image->setMinimumSize(360, 300);
        image->setObjectName("ImageView");
        image->setWordWrap(true);
        column->addWidget(title);
        column->addWidget(image, 1);
        *imageLabel = image;
        return column;
    };

    previewLayout->addLayout(buildPreviewColumn(previewCard, QString::fromUtf8(u8"原图像"), &m_edgeOriginalLabel), 0, 0);
    previewLayout->addLayout(buildPreviewColumn(previewCard, QString::fromUtf8(u8"边缘检测结果图像"), &m_edgeResultLabel), 0, 1);
    previewLayout->setColumnStretch(0, 1);
    previewLayout->setColumnStretch(1, 1);

    rootLayout->addWidget(controlCard);
    rootLayout->addWidget(previewCard, 1);

    connect(m_edgeLoadButton, &QPushButton::clicked, this, &MainWindow::loadEdgeExperimentImage);
    connect(m_edgeApplyButton, &QPushButton::clicked, this, &MainWindow::applyEdgeDetectionToExperiment);
    connect(m_edgeSaveButton, &QPushButton::clicked, this, &MainWindow::saveEdgeExperimentResult);
    connect(m_edgeResetButton, &QPushButton::clicked, this, &MainWindow::resetEdgeExperiment);
    connect(m_edgeTypeCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onExperimentEdgeControlsChanged);
    connect(m_edgeKernelSpin, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::onExperimentEdgeControlsChanged);
    connect(m_edgeThresholdSlider, &QSlider::valueChanged, this, &MainWindow::onExperimentEdgeControlsChanged);

    updateExperimentImageLabel(m_edgeOriginalLabel, {});
    updateExperimentImageLabel(m_edgeResultLabel, {});
    if (m_edgeThresholdValueLabel) {
        m_edgeThresholdValueLabel->setText(QStringLiteral("80"));
    }

    return page;
}

QWidget* MainWindow::createEnhancementExperimentPage() {
    auto* page = new QWidget(m_experimentStackedWidget);
    page->setObjectName(QStringLiteral("labEnhancementExperimentPage"));

    auto* rootLayout = new QVBoxLayout(page);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(12);

    auto* controlCard = new QFrame(page);
    controlCard->setObjectName("Card");
    auto* controlLayout = new QGridLayout(controlCard);
    controlLayout->setContentsMargins(16, 16, 16, 16);
    controlLayout->setHorizontalSpacing(12);
    controlLayout->setVerticalSpacing(10);

    m_enhancementLoadButton = new QPushButton(QString::fromUtf8(u8"选择影像"), controlCard);
    m_enhancementInputPathEdit = new QLineEdit(controlCard);
    m_enhancementInputPathEdit->setReadOnly(true);
    m_enhancementInputPathEdit->setPlaceholderText(QString::fromUtf8(u8"选择灰度影像作为增强输入"));

    m_enhancementStartButton = new QPushButton(QString::fromUtf8(u8"开始增强"), controlCard);
    m_enhancementStepButton = new QPushButton(QString::fromUtf8(u8"分布预览"), controlCard);
    m_enhancementResetButton = new QPushButton(QString::fromUtf8(u8"重置"), controlCard);
    m_enhancementSaveButton = new QPushButton(QString::fromUtf8(u8"保存结果"), controlCard);

    controlLayout->addWidget(new QLabel(QString::fromUtf8(u8"输入图像"), controlCard), 0, 0);
    controlLayout->addWidget(m_enhancementLoadButton, 0, 1);
    controlLayout->addWidget(m_enhancementInputPathEdit, 0, 2, 1, 4);
    controlLayout->addWidget(m_enhancementStartButton, 1, 2);
    controlLayout->addWidget(m_enhancementStepButton, 1, 3);
    controlLayout->addWidget(m_enhancementSaveButton, 1, 4);
    controlLayout->addWidget(m_enhancementResetButton, 1, 5);
    controlLayout->setColumnStretch(2, 2);

    auto* previewCard = new QFrame(page);
    previewCard->setObjectName("Card");
    auto* previewLayout = new QHBoxLayout(previewCard);
    previewLayout->setContentsMargins(16, 16, 16, 16);
    previewLayout->setSpacing(12);

    auto buildDisplayColumn = [](QWidget* parent, const QString& titleText, QLabel** imageLabel) {
        auto* column = new QVBoxLayout();
        auto* title = new QLabel(titleText, parent);
        title->setObjectName("SectionTitle");
        auto* image = new QLabel(QString::fromUtf8(u8"等待加载"), parent);
        image->setAlignment(Qt::AlignCenter);
        image->setMinimumSize(420, 320);
        image->setObjectName("ImageView");
        image->setWordWrap(true);
        column->addWidget(title);
        column->addWidget(image, 1);
        *imageLabel = image;
        return column;
    };

    previewLayout->addLayout(buildDisplayColumn(previewCard, QString::fromUtf8(u8"原图像"), &m_enhancementOriginalLabel), 1);
    previewLayout->addLayout(buildDisplayColumn(previewCard, QString::fromUtf8(u8"最终结果图像"), &m_enhancementFinalLabel), 1);

    auto* infoCard = new QFrame(page);
    infoCard->setObjectName("Card");
    auto* infoLayout = new QVBoxLayout(infoCard);
    infoLayout->setContentsMargins(16, 12, 16, 12);
    m_enhancementInfoLabel = new QLabel(QString::fromUtf8(u8"处理提示：请选择图像并开始增强"), infoCard);
    m_enhancementInfoLabel->setObjectName("SmallLabel");
    m_enhancementInfoLabel->setWordWrap(true);
    infoLayout->addWidget(m_enhancementInfoLabel);

    rootLayout->addWidget(controlCard);
    rootLayout->addWidget(previewCard, 1);
    rootLayout->addWidget(infoCard);

    connect(m_enhancementLoadButton, &QPushButton::clicked, this, &MainWindow::loadEnhancementExperimentImage);
    connect(m_enhancementStartButton, &QPushButton::clicked, this, &MainWindow::startEnhancementExperiment);
    connect(m_enhancementStepButton, &QPushButton::clicked, this, &MainWindow::showEnhancementPreviewDialog);
    connect(m_enhancementSaveButton, &QPushButton::clicked, this, &MainWindow::saveEnhancementExperimentResult);
    connect(m_enhancementResetButton, &QPushButton::clicked, this, &MainWindow::resetEnhancementExperiment);

    updateEnhancementExperimentPreview();
    return page;
}

void MainWindow::updateExperimentImageLabel(QLabel* label, const QImage& image) {
    if (!label) {
        return;
    }

    if (image.isNull()) {
        label->setPixmap(QPixmap());
        label->setText(QString::fromUtf8(u8"暂无图像"));
        return;
    }

    QSize target = label->size();
    if (target.width() < 2 || target.height() < 2) {
        target = label->minimumSize();
    }
    label->setText(QString());
    label->setPixmap(QPixmap::fromImage(image).scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

bool MainWindow::saveImageToFile(const QImage& image, const QString& suggestedName) {
    if (image.isNull()) {
        QMessageBox::information(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"当前没有可保存的图像"));
        return false;
    }

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        QString::fromUtf8(u8"保存图像"),
        suggestedName,
        QString::fromUtf8(u8"PNG 图像 (*.png);;BMP 图像 (*.bmp);;JPEG 图像 (*.jpg *.jpeg)")
    );
    if (filePath.isEmpty()) {
        return false;
    }

    if (!image.save(filePath)) {
        QMessageBox::critical(this, QString::fromUtf8(u8"保存失败"), QString::fromUtf8(u8"图像保存失败，请检查文件路径或格式："));
        return false;
    }

    statusBar()->showMessage(QString::fromUtf8(u8"图像已保存：") + QFileInfo(filePath).fileName());
    return true;
}

void MainWindow::updateNoiseExperimentPreview() {
    updateExperimentImageLabel(m_noiseOriginalLabel, m_noiseSourceImage);
    updateExperimentImageLabel(m_noiseNoisyLabel, m_noiseNoisyImage);
    updateExperimentImageLabel(m_noiseDenoisedLabel, m_noiseDenoisedImage);
}

void MainWindow::updateEdgeExperimentPreview() {
    updateExperimentImageLabel(m_edgeOriginalLabel, m_edgeSourceImage);
    updateExperimentImageLabel(m_edgeResultLabel, m_edgeResultImage);
}

void MainWindow::loadNoiseExperimentImage() {
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        QString::fromUtf8(u8"选择影像"),
        QString(),
        QString::fromUtf8(u8"图像文件 (*.bmp *.jpg *.jpeg);;所有文件 (*.*)")
    );
    if (filePath.isEmpty()) {
        return;
    }

    QString error;
    QImage image = ImageLabProcessor::loadGrayscaleImageFromFile(filePath, &error);
    if (image.isNull()) {
        QMessageBox::critical(this, QString::fromUtf8(u8"加载失败"), error.isEmpty() ? QString::fromUtf8(u8"无法加载图像") : error);
        return;
    }

    m_noiseSourcePath = filePath;
    m_noiseSourceImage = image;
    m_noiseNoisyImage = {};
    m_noiseDenoisedImage = {};
    if (m_noiseInputPathEdit) {
        m_noiseInputPathEdit->setText(filePath);
    }
    updateNoiseExperimentPreview();
    if (m_experimentStatusLabel) {
        m_experimentStatusLabel->setText(QString::fromUtf8(u8"状态：已加载输入图像"));
    }
    statusBar()->showMessage(QString::fromUtf8(u8"输入图像已加载：") + QFileInfo(filePath).fileName());
}

void MainWindow::addNoiseToExperiment() {
    if (m_noiseSourceImage.isNull()) {
        QMessageBox::information(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先加载输入图像"));
        return;
    }

    const double density = m_noiseStrengthSlider ? m_noiseStrengthSlider->value() / 100.0 : 0.08;
    const QString noiseType = m_noiseTypeCombo ? m_noiseTypeCombo->currentText() : QString::fromUtf8(u8"椒盐噪声");
    if (noiseType == QString::fromUtf8(u8"椒盐噪声")) {
        m_noiseNoisyImage = ImageLabProcessor::addSaltPepperNoise(m_noiseSourceImage, density);
    } else if (noiseType == QString::fromUtf8(u8"脉冲噪声")) {
        m_noiseNoisyImage = ImageLabProcessor::addImpulseNoise(m_noiseSourceImage, density);
    } else {
        m_noiseNoisyImage = ImageLabProcessor::addGaussianNoise(m_noiseSourceImage, 0.0, density * 100.0);
    }
    updateNoiseExperimentPreview();
    if (m_experimentStatusLabel) {
        m_experimentStatusLabel->setText(QString::fromUtf8(u8"状态：已添加噪声"));
    }
    statusBar()->showMessage(QString::fromUtf8(u8"已生成加噪结果"));
}

void MainWindow::applyNoiseFilterToExperiment() {
    if (m_noiseSourceImage.isNull()) {
        QMessageBox::information(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先加载输入图像"));
        return;
    }

    const int kernelSize = m_noiseKernelSpin ? m_noiseKernelSpin->value() : 3;
    const QString filterName = m_noiseFilterCombo ? m_noiseFilterCombo->currentText() : QString::fromUtf8(u8"中值滤波");
    if (filterName == QString::fromUtf8(u8"均值滤波")) {
        m_noiseDenoisedImage = ImageLabProcessor::meanFilter(m_noiseSourceImage, kernelSize);
    } else if (filterName == QString::fromUtf8(u8"中值滤波")) {
        m_noiseDenoisedImage = ImageLabProcessor::medianFilter(m_noiseSourceImage, kernelSize);
    } else {
        m_noiseDenoisedImage = ImageLabProcessor::maxFilter(m_noiseSourceImage, kernelSize);
    }
    updateNoiseExperimentPreview();
    if (m_experimentStatusLabel) {
        m_experimentStatusLabel->setText(QString::fromUtf8(u8"状态：已应用滤波"));
    }
    statusBar()->showMessage(QString::fromUtf8(u8"去噪结果已更新"));
}

void MainWindow::resetNoiseExperiment() {
    m_noiseSourceImage = {};
    m_noiseNoisyImage = {};
    m_noiseDenoisedImage = {};
    m_noiseSourcePath.clear();
    if (m_noiseInputPathEdit) {
        m_noiseInputPathEdit->clear();
    }
    if (m_noiseTypeCombo) {
        m_noiseTypeCombo->setCurrentIndex(0);
    }
    if (m_noiseFilterCombo) {
        m_noiseFilterCombo->setCurrentIndex(1);
    }
    if (m_noiseStrengthSlider) {
        m_noiseStrengthSlider->setValue(8);
    }
    if (m_noiseKernelSpin) {
        m_noiseKernelSpin->setValue(3);
    }
    updateNoiseExperimentPreview();
    if (m_experimentStatusLabel) {
        m_experimentStatusLabel->setText(QString::fromUtf8(u8"状态：噪声处理已重置"));
    }
    statusBar()->showMessage(QString::fromUtf8(u8"噪声处理已重置"));
}
void MainWindow::saveNoiseExperimentResult() {
    if (m_noiseSourceImage.isNull() && m_noiseNoisyImage.isNull() && m_noiseDenoisedImage.isNull()) {
        QMessageBox::information(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"当前没有可保存的噪声处理图像"));
        return;
    }

    QMessageBox chooser(this);
    chooser.setWindowTitle(QString::fromUtf8(u8"选择保存结果"));
    chooser.setText(QString::fromUtf8(u8"请选择要保存的噪声处理结果图像"));
    auto* noisyButton = chooser.addButton(QString::fromUtf8(u8"保存加噪无"), QMessageBox::AcceptRole);
    auto* denoisedButton = chooser.addButton(QString::fromUtf8(u8"保存去噪无"), QMessageBox::AcceptRole);
    chooser.addButton(QString::fromUtf8(u8"取消"), QMessageBox::RejectRole);
    chooser.exec();

    if (chooser.clickedButton() == noisyButton) {
        if (!m_noiseNoisyImage.isNull()) {
            saveImageToFile(m_noiseNoisyImage, QStringLiteral("noise_noisy.png"));
        } else {
            QMessageBox::information(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"当前没有加噪图可保存"));
        }
    } else if (chooser.clickedButton() == denoisedButton) {
        if (!m_noiseDenoisedImage.isNull()) {
            saveImageToFile(m_noiseDenoisedImage, QStringLiteral("noise_denoised.png"));
        } else {
            QMessageBox::information(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"当前没有去噪图可保存"));
        }
    }
}

void MainWindow::onExperimentNoiseControlsChanged() {
    if (m_noiseStrengthValueLabel && m_noiseStrengthSlider) {
        m_noiseStrengthValueLabel->setText(QString::number(m_noiseStrengthSlider->value()) + "%");
    }
    if (!m_noiseSourceImage.isNull()) {
        if (!m_noiseNoisyImage.isNull()) {
            const double density = m_noiseStrengthSlider ? m_noiseStrengthSlider->value() / 100.0 : 0.08;
            const QString noiseType = m_noiseTypeCombo ? m_noiseTypeCombo->currentText() : QString::fromUtf8(u8"椒盐噪声");
            if (noiseType == QString::fromUtf8(u8"椒盐噪声")) {
            m_noiseNoisyImage = ImageLabProcessor::addSaltPepperNoise(m_noiseSourceImage, density);
        } else if (noiseType == QString::fromUtf8(u8"脉冲噪声")) {
            m_noiseNoisyImage = ImageLabProcessor::addImpulseNoise(m_noiseSourceImage, density);
        } else {
            m_noiseNoisyImage = ImageLabProcessor::addGaussianNoise(m_noiseSourceImage, 0.0, density * 100.0);
        }
        }
        if (!m_noiseDenoisedImage.isNull()) {
            applyNoiseFilterToExperiment();
        }
    }
}

void MainWindow::loadEdgeExperimentImage() {
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        QString::fromUtf8(u8"选择影像"),
        QString(),
        QString::fromUtf8(u8"图像文件 (*.bmp *.jpg *.jpeg);;所有文件 (*.*)")
    );
    if (filePath.isEmpty()) {
        return;
    }

    QString error;
    QImage image = ImageLabProcessor::loadGrayscaleImageFromFile(filePath, &error);
    if (image.isNull()) {
        QMessageBox::critical(this, QString::fromUtf8(u8"加载失败"), error.isEmpty() ? QString::fromUtf8(u8"无法加载图像") : error);
        return;
    }

    m_edgeSourcePath = filePath;
    m_edgeSourceImage = image;
    m_edgeResultImage = {};
    if (m_edgeInputPathEdit) {
        m_edgeInputPathEdit->setText(filePath);
    }
    updateEdgeExperimentPreview();
    if (m_experimentStatusLabel) {
        m_experimentStatusLabel->setText(QString::fromUtf8(u8"状态：已加载边缘检测输入图无"));
    }
    statusBar()->showMessage(QString::fromUtf8(u8"边缘检测输入图像已加载无") + QFileInfo(filePath).fileName());
}

void MainWindow::applyEdgeDetectionToExperiment() {
    if (m_edgeSourceImage.isNull()) {
        QMessageBox::information(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先加载输入图像"));
        return;
    }

    const int kernelSize = m_edgeKernelSpin ? m_edgeKernelSpin->value() : 3;
    const int threshold = m_edgeThresholdSlider ? m_edgeThresholdSlider->value() : 80;
    const int algorithm = m_edgeTypeCombo ? m_edgeTypeCombo->currentIndex() : 0;

    switch (algorithm) {
    case 0:
        m_edgeResultImage = ImageLabProcessor::sobelEdgeDetect(m_edgeSourceImage, kernelSize, threshold);
        break;
    case 1:
        m_edgeResultImage = ImageLabProcessor::prewittEdgeDetect(m_edgeSourceImage, kernelSize, threshold);
        break;
    default:
        m_edgeResultImage = ImageLabProcessor::laplacianEdgeDetect(m_edgeSourceImage, kernelSize, threshold);
        break;
    }

    updateEdgeExperimentPreview();
    if (m_experimentStatusLabel) {
        m_experimentStatusLabel->setText(QString::fromUtf8(u8"状态：已生成边缘检测结果"));
    }
    statusBar()->showMessage(QString::fromUtf8(u8"边缘检测结果已更新"));
}

void MainWindow::resetEdgeExperiment() {
    m_edgeSourceImage = {};
    m_edgeResultImage = {};
    m_edgeSourcePath.clear();
    if (m_edgeInputPathEdit) {
        m_edgeInputPathEdit->clear();
    }
    if (m_edgeTypeCombo) {
        m_edgeTypeCombo->setCurrentIndex(0);
    }
    if (m_edgeKernelSpin) {
        m_edgeKernelSpin->setValue(3);
    }
    if (m_edgeThresholdSlider) {
        m_edgeThresholdSlider->setValue(80);
    }
    if (m_edgeThresholdValueLabel) {
        m_edgeThresholdValueLabel->setText(QStringLiteral("80"));
    }
    updateEdgeExperimentPreview();
    if (m_experimentStatusLabel) {
        m_experimentStatusLabel->setText(QString::fromUtf8(u8"状态：边缘分析已重置"));
    }
    statusBar()->showMessage(QString::fromUtf8(u8"边缘分析已重置"));
}
void MainWindow::saveEdgeExperimentResult() {
    const QImage image = !m_edgeResultImage.isNull() ? m_edgeResultImage : m_edgeSourceImage;
    saveImageToFile(image, QStringLiteral("edge_result.png"));
}

void MainWindow::onExperimentEdgeControlsChanged() {
    if (m_edgeThresholdValueLabel && m_edgeThresholdSlider) {
        m_edgeThresholdValueLabel->setText(QString::number(m_edgeThresholdSlider->value()));
    }
    if (!m_edgeSourceImage.isNull()) {
        applyEdgeDetectionToExperiment();
    }
}

void MainWindow::loadEnhancementExperimentImage() {
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        QString::fromUtf8(u8"选择影像"),
        QString(),
        QString::fromUtf8(u8"图像文件 (*.bmp *.jpg *.jpeg);;所有文件 (*.*)")
    );
    if (filePath.isEmpty()) {
        return;
    }

    QString error;
    QImage image = ImageLabProcessor::loadGrayscaleImageFromFile(filePath, &error);
    if (image.isNull()) {
        QMessageBox::critical(this, QString::fromUtf8(u8"加载失败"), error.isEmpty() ? QString::fromUtf8(u8"无法加载图像") : error);
        return;
    }

    m_enhancementSourcePath = filePath;
    resetEnhancementExperiment();
    m_enhancementSourcePath = filePath;
    m_enhancementSourceImage = image;
    m_enhancementStep1Image = ImageLabProcessor::step1_originalImage(image);
    m_enhancementPreviewStep = 1;
    if (m_enhancementInputPathEdit) {
        m_enhancementInputPathEdit->setText(filePath);
    }
    updateEnhancementExperimentPreview();
    if (m_enhancementInfoLabel) {
        m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：图像已加载，可以开始执行增强流程无"));
    }
    if (m_experimentStatusLabel) {
        m_experimentStatusLabel->setText(QString::fromUtf8(u8"状态：已加载图像增强输入图像"));
    }
    statusBar()->showMessage(QString::fromUtf8(u8"图像增强输入图像已加载：") + QFileInfo(filePath).fileName());
}

void MainWindow::startEnhancementExperiment() {
    if (m_enhancementSourceImage.isNull()) {
        QMessageBox::information(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先加载输入图像"));
        return;
    }

    m_enhancementStep1Image = ImageLabProcessor::step1_originalImage(m_enhancementSourceImage);
    m_enhancementStep2Image = ImageLabProcessor::step2_laplacianProcess(m_enhancementStep1Image);
    m_enhancementStep3Image = ImageLabProcessor::step3_sharpenImage(m_enhancementStep1Image, m_enhancementStep2Image);
    m_enhancementStep4Image = ImageLabProcessor::step4_sobelProcess(m_enhancementStep1Image);
    m_enhancementStep5Image = ImageLabProcessor::step5_meanFilterGradient(m_enhancementStep4Image);
    m_enhancementStep6Image = ImageLabProcessor::step6_maskImage(m_enhancementStep3Image, m_enhancementStep5Image);
    m_enhancementStep7Image = ImageLabProcessor::step7_addOriginalAndMask(m_enhancementStep1Image, m_enhancementStep6Image);
    m_enhancementStep8Image = ImageLabProcessor::step8_gammaTransform(m_enhancementStep7Image);
    m_enhancementPreviewStep = 8;

    updateEnhancementExperimentPreview();
    if (m_enhancementInfoLabel) {
        m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：已完成 8 步图像增强流程，可点击任意步骤查看对应中间结果无"));
    }
    if (m_experimentStatusLabel) {
        m_experimentStatusLabel->setText(QString::fromUtf8(u8"状态：图像增强流程已完成"));
    }
    statusBar()->showMessage(QString::fromUtf8(u8"图像增强处理已完成"));
}

void MainWindow::stepEnhancementExperiment() {
    if (m_enhancementSourceImage.isNull()) {
        QMessageBox::information(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先加载输入图像"));
        return;
    }

    if (m_enhancementPreviewStep >= 8) {
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：全部步骤已完成，可重新开始或重置无"));
        }
        statusBar()->showMessage(QString::fromUtf8(u8"图像增强步骤已全部完成"));
        return;
    }

    switch (m_enhancementPreviewStep) {
    case 0:
        m_enhancementStep1Image = ImageLabProcessor::step1_originalImage(m_enhancementSourceImage);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：步骤1，显示并缓存原图像"));
        }
        break;
    case 1:
        m_enhancementStep2Image = ImageLabProcessor::step2_laplacianProcess(m_enhancementStep1Image);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：步骤2，对原图像进行拉普拉斯处理，提取高频细节"));
        }
        break;
    case 2:
        m_enhancementStep3Image = ImageLabProcessor::step3_sharpenImage(m_enhancementStep1Image, m_enhancementStep2Image);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：步骤3，将原图像与拉普拉斯结果相加，形成锐化图像"));
        }
        break;
    case 3:
        m_enhancementStep4Image = ImageLabProcessor::step4_sobelProcess(m_enhancementStep1Image);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：步骤4，对原图像应用 Sobel 算子，提取梯度信息"));
        }
        break;
    case 4:
        m_enhancementStep5Image = ImageLabProcessor::step5_meanFilterGradient(m_enhancementStep4Image);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：步骤5，对 Sobel 结果进行均值滤波，得到平滑梯度图像"));
        }
        break;
    case 5:
        m_enhancementStep6Image = ImageLabProcessor::step6_maskImage(m_enhancementStep3Image, m_enhancementStep5Image);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：步骤6，将锐化图像与平滑梯度图像相乘，生成掩膜"));
        }
        break;
    case 6:
        m_enhancementStep7Image = ImageLabProcessor::step7_addOriginalAndMask(m_enhancementStep1Image, m_enhancementStep6Image);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：步骤7，将原图像与掩膜求和，得到增强图像"));
        }
        break;
    case 7:
        m_enhancementStep8Image = ImageLabProcessor::step8_gammaTransform(m_enhancementStep7Image);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：步骤8，对增强图像进行伽马变换，优化对比度"));
        }
        break;
    default:
        break;
    }

    ++m_enhancementPreviewStep;
    updateEnhancementExperimentPreview();
    if (m_experimentStatusLabel) {
        m_experimentStatusLabel->setText(QString::fromUtf8(u8"状态：已执行增强步骤") + QString::number(m_enhancementPreviewStep));
    }
    statusBar()->showMessage(QString::fromUtf8(u8"图像增强处理步骤已更新"));
}

void MainWindow::resetEnhancementExperiment() {
    m_enhancementSourceImage = {};
    m_enhancementStep1Image = {};
    m_enhancementStep2Image = {};
    m_enhancementStep3Image = {};
    m_enhancementStep4Image = {};
    m_enhancementStep5Image = {};
    m_enhancementStep6Image = {};
    m_enhancementStep7Image = {};
    m_enhancementStep8Image = {};
    m_enhancementSourcePath.clear();
    m_enhancementPreviewStep = 0;
    if (m_enhancementInputPathEdit) {
        m_enhancementInputPathEdit->clear();
    }
    updateEnhancementExperimentPreview();
    if (m_enhancementInfoLabel) {
        m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：已重置，请重新加载图像"));
    }
    if (m_experimentStatusLabel) {
        m_experimentStatusLabel->setText(QString::fromUtf8(u8"状态：图像增强处理已重置"));
    }
    statusBar()->showMessage(QString::fromUtf8(u8"图像增强处理已重置"));
}
void MainWindow::saveEnhancementExperimentResult() {
    const QImage image = !m_enhancementStep8Image.isNull()
        ? m_enhancementStep8Image
        : (!m_enhancementStep7Image.isNull() ? m_enhancementStep7Image : m_enhancementSourceImage);
    saveImageToFile(image, QStringLiteral("enhancement_result.png"));
}

void MainWindow::updateEnhancementExperimentPreview() {
    updateExperimentImageLabel(m_enhancementOriginalLabel, m_enhancementStep1Image.isNull() ? m_enhancementSourceImage : m_enhancementStep1Image);
    updateExperimentImageLabel(m_enhancementFinalLabel, m_enhancementStep8Image.isNull() ? m_enhancementStep7Image : m_enhancementStep8Image);
    if (m_enhancementInfoLabel) {
        if (!m_enhancementStep8Image.isNull()) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：图像增强流程已完成，左侧显示原图，右侧显示最终结果无"));
        } else if (!m_enhancementStep1Image.isNull()) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：已载入输入图像，可点击“开始增强”或“分布预览”无"));
        } else {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：请选择图像并开始增强"));
        }
    }
}

void MainWindow::showEnhancementPreviewDialog() {
    if (m_enhancementSourceImage.isNull()) {
        QMessageBox::information(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先加载输入图像"));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromUtf8(u8"分布预览"));
    dialog.resize(980, 760);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    auto* topRow = new QHBoxLayout();
    auto* badge = new QLabel(QString::fromUtf8(u8"第1步"), &dialog);
    badge->setObjectName(QStringLiteral("ThemeBadge"));
    auto* note = new QLabel(QString::fromUtf8(u8"原图像"), &dialog);
    note->setObjectName(QStringLiteral("SmallLabel"));
    topRow->addWidget(badge);
    topRow->addWidget(note, 1);

    auto* imageFrame = new QFrame(&dialog);
    imageFrame->setObjectName("Card");
    auto* imageLayout = new QVBoxLayout(imageFrame);
    imageLayout->setContentsMargins(12, 12, 12, 12);
    auto* imageLabel = new QLabel(imageFrame);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setObjectName("ImageView");
    imageLabel->setMinimumSize(900, 600);
    imageLayout->addWidget(imageLabel, 1);

    auto* buttonRow = new QHBoxLayout();
    auto* prevButton = new QPushButton(QString::fromUtf8(u8"上一无"), &dialog);
    auto* nextButton = new QPushButton(QString::fromUtf8(u8"下一无"), &dialog);
    auto* closeButton = new QPushButton(QString::fromUtf8(u8"关闭"), &dialog);
    buttonRow->addWidget(prevButton);
    buttonRow->addWidget(nextButton);
    buttonRow->addStretch(1);
    buttonRow->addWidget(closeButton);

    layout->addLayout(topRow);
    layout->addWidget(imageFrame, 1);
    layout->addLayout(buttonRow);

    const QVector<QImage> steps = {
        m_enhancementStep1Image,
        m_enhancementStep2Image,
        m_enhancementStep3Image,
        m_enhancementStep4Image,
        m_enhancementStep5Image,
        m_enhancementStep6Image,
        m_enhancementStep7Image,
        m_enhancementStep8Image
    };
    const QVector<QString> notes = {
        QString::fromUtf8(u8"原图像"),
        QString::fromUtf8(u8"拉普拉斯处理"),
        QString::fromUtf8(u8"锐化图像"),
        QString::fromUtf8(u8"Sobel 梯度"),
        QString::fromUtf8(u8"均值滤波梯无"),
        QString::fromUtf8(u8"掩膜"),
        QString::fromUtf8(u8"原图与掩膜相加"),
        QString::fromUtf8(u8"伽马变换")
    };

    auto refresh = [&]() {
        const int index = qBound(0, dialog.property("stepIndex").toInt(), 7);
        dialog.setProperty("stepIndex", index);
        badge->setText(QString::fromUtf8(u8"第%1步").arg(index + 1));
        note->setText(notes[index]);
        const QImage& img = steps[index];
        if (img.isNull()) {
            imageLabel->setPixmap(QPixmap());
            imageLabel->setText(QString::fromUtf8(u8"暂无图像"));
        } else {
            QSize target = imageLabel->size();
            if (target.width() < 2 || target.height() < 2) {
                target = imageLabel->minimumSize();
            }
            imageLabel->setText(QString());
            imageLabel->setPixmap(QPixmap::fromImage(img).scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        prevButton->setEnabled(index > 0);
        nextButton->setEnabled(index < 7);
    };

    dialog.setProperty("stepIndex", 0);
    QObject::connect(prevButton, &QPushButton::clicked, &dialog, [&]() {
        dialog.setProperty("stepIndex", dialog.property("stepIndex").toInt() - 1);
        refresh();
    });
    QObject::connect(nextButton, &QPushButton::clicked, &dialog, [&]() {
        dialog.setProperty("stepIndex", dialog.property("stepIndex").toInt() + 1);
        refresh();
    });
    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    refresh();
    dialog.exec();
}

bool MainWindow::isSupportedImageFile(const QString& filePath) const {
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return suffix == "bmp" || suffix == "jpg" || suffix == "jpeg";
}

bool MainWindow::isSupportedDicomFile(const QString& filePath) const {
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return suffix == "dcm" || suffix == "dicom" || suffix == "ima";
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (!isDropOnOriginalView(event->position().toPoint())) {
        event->ignore();
        return;
    }

    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl& url : urls) {
        if (url.isLocalFile() &&
            (isSupportedImageFile(url.toLocalFile()) || isSupportedDicomFile(url.toLocalFile()))) {
            event->acceptProposedAction();
            return;
        }
    }

    event->ignore();
}

void MainWindow::dropEvent(QDropEvent* event) {
    if (!isDropOnOriginalView(event->position().toPoint())) {
        event->ignore();
        return;
    }

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
        if (!isSupportedImageFile(filePath) && !isSupportedDicomFile(filePath)) {
            continue;
        }

        QFileInfo info(filePath);
        m_currentFolder = info.absolutePath();
        if (m_folderLabel) {
            m_folderLabel->setText(QStringLiteral("当前文件夹：") + m_currentFolder);
        }
        if (m_fileList) {
        loadImageFiles(m_currentFolder);
            QList<QListWidgetItem*> items = m_fileList->findItems(info.fileName(), Qt::MatchExactly);
            if (!items.isEmpty()) {
                m_fileList->setCurrentItem(items.first());
            }
        }

        if (isSupportedDicomFile(filePath)) {
            loadDicomSeries(filePath);
        } else {
            displayImage(filePath);
        }
        event->acceptProposedAction();
        return;
    }

    event->ignore();
}

bool MainWindow::isDropOnOriginalView(const QPoint& windowPos) const {
    if (!m_imageView || !m_imageView->isVisible()) {
        return false;
    }

    const QPoint viewPos = m_imageView->mapFrom(this, windowPos);
    return m_imageView->rect().contains(viewPos);
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    if (!m_originalImage.isNull()) {
        renderCurrentImage();
    }
    updateNoiseExperimentPreview();
    updateEdgeExperimentPreview();
    updateEnhancementExperimentPreview();
}

void MainWindow::selectFolder() {
    QString folder = QFileDialog::getExistingDirectory(this, QStringLiteral("Select image folder"));
    if (folder.isEmpty()) {
        return;
    }

    m_currentFolder = folder;
    if (m_folderLabel) {
        m_folderLabel->setText(QStringLiteral("当前文件夹：") + folder);
    }
    loadImageFiles(folder);
    if (!m_fileList) {
        QDir dir(folder);
        QStringList files = dir.entryList(
            {"*.bmp", "*.BMP", "*.jpg", "*.JPG", "*.jpeg", "*.JPEG", "*.dcm", "*.DICOM", "*.ima", "*.IMA"},
            QDir::Files,
            QDir::Name
        );
        if (!files.isEmpty()) {
            const QString firstPath = dir.filePath(files.first());
            if (isSupportedDicomFile(firstPath)) {
                loadDicomSeries(folder);
            } else {
                displayImage(firstPath);
            }
        }
    }
}

void MainWindow::openSingleFile() {
    QString filePath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("打开图像文件"),
        QString(),
        QStringLiteral("图像文件 (*.bmp *.jpg *.jpeg *.dcm *.dicom *.ima);;所有文件 (*.*)")
    );
    if (filePath.isEmpty()) {
        return;
    }

    QFileInfo info(filePath);
    m_currentFolder = info.absolutePath();
    if (m_folderLabel) {
        m_folderLabel->setText(QStringLiteral("当前文件夹：") + m_currentFolder);
    }
    if (m_fileList) {
        loadImageFiles(m_currentFolder);
        QList<QListWidgetItem*> items = m_fileList->findItems(info.fileName(), Qt::MatchExactly);
        if (!items.isEmpty()) {
            m_fileList->setCurrentItem(items.first());
        }
    }

    if (isSupportedDicomFile(filePath)) {
        loadDicomSeries(filePath);
    } else {
        displayImage(filePath);
    }
}

void MainWindow::loadImageFiles(const QString& folder) {
    if (m_fileList) {
        m_fileList->clear();
    }

    QDir dir(folder);
    QStringList files = dir.entryList(
        {"*.bmp", "*.BMP", "*.jpg", "*.JPG", "*.jpeg", "*.JPEG", "*.dcm", "*.DICOM", "*.ima", "*.IMA"},
        QDir::Files,
        QDir::Name
    );

    for (const auto& f : files) {
        if (m_fileList) {
            m_fileList->addItem(f);
        }
    }

    statusBar()->showMessage(QStringLiteral("状态：在文件夹中找无%1 个支持的图像文件").arg(files.size()));
    if (files.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("该文件夹中没有可识别的 BMP / JPG / JPEG / DICOM 文件"));
        return;
    }
}

void MainWindow::onFileSelected(QListWidgetItem* item) {
    if (!item || m_currentFolder.isEmpty()) {
        return;
    }

    const QString path = QDir(m_currentFolder).filePath(item->text());
    if (isSupportedDicomFile(path)) {
        loadDicomSeries(path);
    } else {
        displayImage(path);
    }
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
    m_fitOnNextRefresh = true;
    m_resultHistory.clear();
    m_chainHistory.clear();
    m_processingChain.clear();
    m_parameterHistory.clear();
    m_parameterRecords.clear();
    m_historyIndex = -1;
    m_frequencyIfftSource = {};
    m_dicomMode = false;
    m_dicomSeries = {};
    m_windowedSlices.clear();
    switchToDicomMode(false);

    QString compressionText = (suffix == "bmp")
        ? QString::number(m_currentCompression)
        : QStringLiteral("无Qt 内部解码");

    m_infoLabel->setPlainText(
        QStringLiteral("文件：%1\n格式：%2\n尺寸：%3 x %4\n位深：%5 位\n压缩/解码：%6\n像素总数：%7")
            .arg(QFileInfo(filePath).fileName())
            .arg(suffix.toUpper())
            .arg(m_currentWidth)
            .arg(m_currentHeight)
            .arg(m_currentBitCount)
            .arg(compressionText)
            .arg(static_cast<long long>(m_currentWidth) * m_currentHeight)
    );

    refreshWorkbenchImages();
    updateProcessingStatus(QString::fromUtf8(u8"影像已加载"));
    statusBar()->showMessage(QStringLiteral("状态：已加载%1").arg(QFileInfo(filePath).fileName()));
}

void MainWindow::switchToDicomMode(bool enabled) {
    if (!m_viewModeStack || !m_standardViewPage || !m_dicomViewPage) {
        return;
    }
    m_dicomMode = enabled;
    m_viewModeStack->setCurrentWidget(enabled ? m_dicomViewPage : m_standardViewPage);
}

void MainWindow::loadDicomSeries(const QString& path) {
    DicomSeriesData series;
    QString error;
    QString sourcePath = path;

    if (QFileInfo(path).isDir()) {
        if (!DicomToolkit::loadDicomDirectory(path, &series, &error)) {
            QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("打开 DICOM 目录失败：\n") + error);
            return;
        }
    } else {
        DicomSeriesData oneFile;
        if (!DicomToolkit::loadDicomFile(path, &oneFile, &error)) {
            QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("打开 DICOM 文件失败：\n") + error);
            return;
        }
        const QString folder = QFileInfo(path).absolutePath();
        if (!DicomToolkit::loadDicomDirectory(folder, &series, &error, oneFile.seriesInstanceUid)) {
            series = oneFile;
        } else {
            sourcePath = folder;
        }
    }

    m_dicomSeries = series;
    m_windowedSlices = series.rawSlices;
    m_dicomProcessedSlices.clear();
    m_dicomSegmentationInputSlices.clear();
    m_cachedWindowWidth = -1.0;
    m_cachedWindowCenter = std::numeric_limits<double>::lowest();
    m_dicomAxialIndex = std::clamp(static_cast<int>(series.rawSlices.size() / 2), 0, std::max(0, static_cast<int>(series.rawSlices.size()) - 1));
    m_dicomCoronalIndex = std::clamp(series.height / 2, 0, std::max(0, series.height - 1));
    m_dicomSagittalIndex = std::clamp(series.width / 2, 0, std::max(0, series.width - 1));
    m_dicomWindowWidth = series.defaultWindowWidth;
    m_dicomWindowCenter = series.defaultWindowCenter;

    m_currentFile = QFileInfo(path).isDir() ? sourcePath : path;
    m_currentFolder = QFileInfo(sourcePath).isDir() ? sourcePath : QFileInfo(sourcePath).absolutePath();
    m_currentWidth = series.width;
    m_currentHeight = series.height;
    m_currentBitCount = series.bitDepth;
    m_currentCompression = 0;
    m_originalImage = {};
    m_filteredImage = {};
    m_resultHistory.clear();
    m_chainHistory.clear();
    m_processingChain.clear();
    m_parameterHistory.clear();
    m_parameterRecords.clear();
    m_historyIndex = -1;
    m_segmentationMaskSlices.clear();
    invalidateSegmentationSurfaceCache();
    clearSeedHistory();
    m_seedEditMode = SeedEditMode::Navigate;
    if (m_seedModeCombo) {
        m_seedModeCombo->blockSignals(true);
        m_seedModeCombo->setCurrentIndex(0);
        m_seedModeCombo->blockSignals(false);
    }
    m_loadedSegData = {};

    if (m_windowWidthSlider) m_windowWidthSlider->setValue(static_cast<int>(m_dicomWindowWidth));
    if (m_windowCenterSlider) m_windowCenterSlider->setValue(static_cast<int>(m_dicomWindowCenter));
    if (m_axialSlider) m_axialSlider->setRange(0, std::max(0, static_cast<int>(series.rawSlices.size()) - 1));
    if (m_coronalSlider) m_coronalSlider->setRange(0, std::max(0, series.height - 1));
    if (m_sagittalSlider) m_sagittalSlider->setRange(0, std::max(0, series.width - 1));
    if (m_sliceNavigationController) {
        SliceNavigationController::VolumeGeometry geometry;
        geometry.width = series.width;
        geometry.height = series.height;
        geometry.depth = static_cast<int>(series.rawSlices.size());
        geometry.spacingX = series.pixelSpacingX;
        geometry.spacingY = series.pixelSpacingY;
        geometry.spacingZ = series.sliceThickness;
        geometry.origin = series.origin;
        geometry.rowDirection = series.rowDirection;
        geometry.columnDirection = series.columnDirection;
        geometry.sliceDirection = series.sliceDirection;
        m_sliceNavigationController->configure(geometry);
    } else {
        if (m_axialSlider) m_axialSlider->setValue(m_dicomAxialIndex);
        if (m_coronalSlider) m_coronalSlider->setValue(m_dicomCoronalIndex);
        if (m_sagittalSlider) m_sagittalSlider->setValue(m_dicomSagittalIndex);
    }

    switchToDicomMode(true);
    updateDicomViews();
    updateSegmentationControls();
    updateProcessingStatus(QString::fromUtf8(u8"DICOM 序列已加载"));
    statusBar()->showMessage(QStringLiteral("状态：已加载 DICOM 序列，切片数 %1").arg(series.rawSlices.size()));
}

void MainWindow::importSegFile() {
    if (!m_dicomMode || m_dicomSeries.rawSlices.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先加载对应的 CT DICOM 序列，再导入 SEG"));
        return;
    }

    const QString segPath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("导入 DICOM SEG"),
        m_currentFolder,
        QStringLiteral("DICOM SEG (*.dcm *.dicom *.ima);;所有文件 (*.*)")
    );
    if (segPath.isEmpty()) {
        return;
    }

    DicomSegData seg;
    QString error;
    if (!DicomSegLoader::loadSegFile(segPath, m_dicomSeries, &seg, &error)) {
        QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("导入 SEG 失败：\n") + error);
        return;
    }

    m_loadedSegData = seg;
    m_segmentationMaskSlices = seg.alignedMaskSlices;
    invalidateSegmentationSurfaceCache();
    clearSeedHistory();
    m_seedEditMode = SeedEditMode::Navigate;
    if (m_seedModeCombo) {
        m_seedModeCombo->blockSignals(true);
        m_seedModeCombo->setCurrentIndex(0);
        m_seedModeCombo->blockSignals(false);
    }
    m_showSegmentation = true;
    if (m_showSegmentationCheck) {
        m_showSegmentationCheck->setChecked(true);
    }
    m_parameterRecords.append(QStringLiteral("SEG: 标签=%1, 算法=%2, 帧数=%3")
                                  .arg(m_loadedSegData.segmentLabel)
                                  .arg(m_loadedSegData.segmentAlgorithmType)
                                  .arg(m_loadedSegData.alignedMaskSlices.size()));
    int cx = 0, cy = 0, cz = 0;
    const int bestAxial = findBestSegmentationAxialSlice();
    if (findSegmentationCentroid(&cx, &cy, &cz)) {
        if (bestAxial >= 0) {
            cz = bestAxial;
        }
        if (m_sliceNavigationController) {
            m_sliceNavigationController->setSagittalIndex(cx);
            m_sliceNavigationController->setCoronalIndex(cy);
            m_sliceNavigationController->setAxialIndex(cz);
        } else {
            m_dicomSagittalIndex = cx;
            m_dicomCoronalIndex = cy;
            m_dicomAxialIndex = cz;
        }
    }
    updateSegmentationControls();
    rebuildSegmentationPreview();
    m_parameterRecords.append(segmentationDebugSummary());
    updateProcessingStatus(QString::fromUtf8(u8"SEG 已导入并完成对齐"));
    statusBar()->showMessage(segmentationDebugSummary());
}

void MainWindow::updateDicomSliceIndex(int index) {
    if (m_sliceNavigationController) {
        m_sliceNavigationController->setAxialIndex(index);
    } else {
        m_dicomAxialIndex = index;
        updateDicomViews();
    }
}

void MainWindow::updateDicomWindowWidth(int value) {
    m_dicomWindowWidth = std::max(1, value);
    if (m_windowWidthValueLabel) {
        m_windowWidthValueLabel->setText(QString::number(static_cast<int>(m_dicomWindowWidth)));
    }
    updateDicomViews();
}

void MainWindow::updateDicomWindowCenter(int value) {
    m_dicomWindowCenter = value;
    if (m_windowCenterValueLabel) {
        m_windowCenterValueLabel->setText(QString::number(static_cast<int>(m_dicomWindowCenter)));
    }
    updateDicomViews();
}

void MainWindow::onSliceNavigationChanged(const SliceNavigationController::State& state) {
    m_dicomAxialIndex = state.axialIndex;
    m_dicomCoronalIndex = state.coronalIndex;
    m_dicomSagittalIndex = state.sagittalIndex;

    if (m_axialSlider && m_axialSlider->value() != state.axialIndex) {
        m_axialSlider->blockSignals(true);
        m_axialSlider->setValue(state.axialIndex);
        m_axialSlider->blockSignals(false);
    }
    if (m_coronalSlider && m_coronalSlider->value() != state.coronalIndex) {
        m_coronalSlider->blockSignals(true);
        m_coronalSlider->setValue(state.coronalIndex);
        m_coronalSlider->blockSignals(false);
    }
    if (m_sagittalSlider && m_sagittalSlider->value() != state.sagittalIndex) {
        m_sagittalSlider->blockSignals(true);
        m_sagittalSlider->setValue(state.sagittalIndex);
        m_sagittalSlider->blockSignals(false);
    }
    updateDicomViews();
    statusBar()->showMessage(
        QStringLiteral("状态：序列=%1 | 切片 A/C/S=%2/%3/%4 | WW/WL=%5/%6 | Voxel=(%7,%8,%9) | World=(%10,%11,%12)")
            .arg(QFileInfo(m_currentFile).fileName())
            .arg(state.axialIndex)
            .arg(state.coronalIndex)
            .arg(state.sagittalIndex)
            .arg(static_cast<int>(m_dicomWindowWidth))
            .arg(static_cast<int>(m_dicomWindowCenter))
            .arg(static_cast<int>(state.voxel.x()))
            .arg(static_cast<int>(state.voxel.y()))
            .arg(static_cast<int>(state.voxel.z()))
            .arg(state.world.x(), 0, 'f', 2)
            .arg(state.world.y(), 0, 'f', 2)
            .arg(state.world.z(), 0, 'f', 2)
    );
}

void MainWindow::updateSegmentationControls() {
    const bool segLoaded = m_loadedSegData.valid;
    if (m_segmentationPanelStack) {
        m_segmentationPanelStack->setCurrentWidget(segLoaded ? m_segInfoPage : m_algorithmSegmentationPage);
    }
    if (m_segInfoLabel) {
        m_segInfoLabel->setText(segLoaded
            ? QStringLiteral("标签：%1\n算法：%2\n引用序列：%3\n帧数：%4\n来源：DICOM SEG")
                  .arg(m_loadedSegData.segmentLabel)
                  .arg(m_loadedSegData.segmentAlgorithmType)
                  .arg(m_loadedSegData.referencedSeriesInstanceUid)
                  .arg(m_loadedSegData.alignedMaskSlices.size())
            : QString::fromUtf8(u8"当前未导入 SEG，将使用算法分割"));
    }
    const bool regionGrow = m_segmentationMethod == SegmentationMethod::RegionGrow;
    const bool manualThreshold = m_segmentationMethod == SegmentationMethod::Threshold;
    if (m_seedModeCombo) {
        m_seedModeCombo->setEnabled(regionGrow && !segLoaded);
    }
    if (m_segmentationThresholdSlider) {
        m_segmentationThresholdSlider->setEnabled(manualThreshold && !segLoaded);
    }
    if (m_segmentationToleranceSlider) {
        m_segmentationToleranceSlider->setEnabled(regionGrow && !segLoaded);
    }
}

void MainWindow::clearLoadedSeg() {
    m_loadedSegData = {};
    m_segmentationMaskSlices.clear();
    invalidateSegmentationSurfaceCache();
    m_showSegmentation = false;
    clearSeedHistory();
    m_seedEditMode = SeedEditMode::Navigate;
    if (m_seedModeCombo) {
        m_seedModeCombo->blockSignals(true);
        m_seedModeCombo->setCurrentIndex(0);
        m_seedModeCombo->blockSignals(false);
    }
    if (m_showSegmentationCheck) {
        m_showSegmentationCheck->setChecked(false);
    }
    updateSegmentationControls();
    rebuildSegmentationPreview();
}

void MainWindow::runDicomSegmentation() {
    if (!m_dicomMode || m_windowedSlices.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先加载 DICOM 序列"));
        return;
    }
    if (m_segmentationMethod == SegmentationMethod::RegionGrow && m_segmentationSeeds.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("区域生长模式需要先在视图中添加前景/背景种子"));
        return;
    }
    const QVector<QImage> segmentationInput = !m_dicomSegmentationInputSlices.isEmpty()
        ? m_dicomSegmentationInputSlices
        : currentDicomProcessingInput();
    m_segmentationMaskSlices = SegmentVolume::run(
        segmentationInput,
        m_segmentationMethod,
        m_segmentationSeeds,
        m_segmentationParams
    );
    invalidateSegmentationSurfaceCache();
    rebuildSegmentationPreview();
    updateProcessingStatus(QString::fromUtf8(u8"分割已完成"));
}

void MainWindow::clearSegmentationSeeds() {
    if (!m_segmentationSeeds.isEmpty()) {
        pushSeedHistorySnapshot({});
        applyCurrentSeedHistory(QString::fromUtf8(u8"已清空种子"));
        return;
    }
    rebuildSegmentationPreview();
}

void MainWindow::clearSeedHistory() {
    m_segmentationSeeds.clear();
    m_seedHistory.clear();
    m_seedHistory.append(QVector<SegmentationSeedPoint>());
    m_seedHistoryIndex = 0;
}

void MainWindow::pushSeedHistorySnapshot(const QVector<SegmentationSeedPoint>& seeds) {
    if (m_seedHistoryIndex < 0) {
        clearSeedHistory();
    }
    if (m_seedHistoryIndex >= 0 && m_seedHistoryIndex < m_seedHistory.size() &&
        m_seedHistory[m_seedHistoryIndex] == seeds) {
        return;
    }
    while (m_seedHistory.size() > m_seedHistoryIndex + 1) {
        m_seedHistory.removeLast();
    }
    m_seedHistory.append(seeds);
    m_seedHistoryIndex = m_seedHistory.size() - 1;
}

void MainWindow::applyCurrentSeedHistory(const QString& message) {
    if (m_seedHistoryIndex >= 0 && m_seedHistoryIndex < m_seedHistory.size()) {
        m_segmentationSeeds = m_seedHistory[m_seedHistoryIndex];
    } else {
        m_segmentationSeeds.clear();
    }
    rebuildSegmentationPreview();
    updateProcessingStatus(message);
}

bool MainWindow::canUndoSeedHistory() const {
    return m_dicomMode && m_seedHistoryIndex > 0;
}

bool MainWindow::canRedoSeedHistory() const {
    return m_dicomMode && m_seedHistoryIndex >= 0 && m_seedHistoryIndex + 1 < m_seedHistory.size();
}

bool MainWindow::isSeedEditingEnabled() const {
    return m_dicomMode &&
        !m_loadedSegData.valid &&
        m_segmentationMethod == SegmentationMethod::RegionGrow &&
        (m_seedEditMode == SeedEditMode::Foreground || m_seedEditMode == SeedEditMode::Background);
}

void MainWindow::appendSeedPoint(const SegmentationSeedPoint& seed) {
    QVector<SegmentationSeedPoint> nextSeeds = m_segmentationSeeds;
    nextSeeds.append(seed);
    pushSeedHistorySnapshot(nextSeeds);
    applyCurrentSeedHistory(QString::fromUtf8(u8"已添加种子点"));
}

bool MainWindow::findSegmentationCentroid(int* outX, int* outY, int* outZ) const {
    if (m_segmentationMaskSlices.isEmpty()) {
        return false;
    }

    double sumX = 0.0;
    double sumY = 0.0;
    double sumZ = 0.0;
    double weight = 0.0;
    for (int z = 0; z < m_segmentationMaskSlices.size(); ++z) {
        const QImage mask = m_segmentationMaskSlices[z].convertToFormat(QImage::Format_Grayscale8);
        for (int y = 0; y < mask.height(); ++y) {
            const uchar* row = mask.constScanLine(y);
            for (int x = 0; x < mask.width(); ++x) {
                if (row[x] == 0) {
                    continue;
                }
                sumX += x;
                sumY += y;
                sumZ += z;
                weight += 1.0;
            }
        }
    }

    if (weight <= 0.0) {
        return false;
    }
    if (outX) *outX = static_cast<int>(std::round(sumX / weight));
    if (outY) *outY = static_cast<int>(std::round(sumY / weight));
    if (outZ) *outZ = static_cast<int>(std::round(sumZ / weight));
    return true;
}

int MainWindow::findBestSegmentationAxialSlice() const {
    if (m_segmentationMaskSlices.isEmpty()) {
        return -1;
    }
    int bestIndex = -1;
    int bestSum = -1;
    for (int z = 0; z < m_segmentationMaskSlices.size(); ++z) {
        const QImage mask = m_segmentationMaskSlices[z].convertToFormat(QImage::Format_Grayscale8);
        int sum = 0;
        for (int y = 0; y < mask.height(); ++y) {
            const uchar* row = mask.constScanLine(y);
            for (int x = 0; x < mask.width(); ++x) {
                if (row[x] > 0) {
                    ++sum;
                }
            }
        }
        if (sum > bestSum) {
            bestSum = sum;
            bestIndex = z;
        }
    }
    return bestIndex;
}

QString MainWindow::segmentationDebugSummary() const {
    if (m_segmentationMaskSlices.isEmpty()) {
        return QString::fromUtf8(u8"SEG 调试：当前没有 mask");
    }

    int first = -1;
    int last = -1;
    int count = 0;
    int currentCount = 0;
    int bestIndex = -1;
    int bestSum = -1;
    for (int z = 0; z < m_segmentationMaskSlices.size(); ++z) {
        const QImage mask = m_segmentationMaskSlices[z].convertToFormat(QImage::Format_Grayscale8);
        int sum = 0;
        for (int y = 0; y < mask.height(); ++y) {
            const uchar* row = mask.constScanLine(y);
            for (int x = 0; x < mask.width(); ++x) {
                if (row[x] > 0) {
                    ++sum;
                }
            }
        }
        if (sum > 0) {
            if (first < 0) {
                first = z;
            }
            last = z;
            ++count;
        }
        if (sum > bestSum) {
            bestSum = sum;
            bestIndex = z;
        }
        if (z == m_dicomAxialIndex) {
            currentCount = sum;
        }
    }

    int cx = 0, cy = 0, cz = 0;
    const bool hasCentroid = findSegmentationCentroid(&cx, &cy, &cz);
    return QStringLiteral("SEG 调试：非空切片=%1, 范围=[%2,%3], 最佳Axial=%4, 当前切片=%5, 当前mask像素=%6, 重心=(%7,%8,%9)")
        .arg(count)
        .arg(first)
        .arg(last)
        .arg(bestIndex)
        .arg(m_dicomAxialIndex)
        .arg(currentCount)
        .arg(hasCentroid ? cx : -1)
        .arg(hasCentroid ? cy : -1)
        .arg(hasCentroid ? cz : -1);
}

QImage MainWindow::buildCoronalMaskSlice() const {
    if (m_segmentationMaskSlices.isEmpty()) {
        return {};
    }
    return DicomToolkit::buildCoronalSlice(m_segmentationMaskSlices, m_dicomCoronalIndex);
}

QImage MainWindow::buildSagittalMaskSlice() const {
    if (m_segmentationMaskSlices.isEmpty()) {
        return {};
    }
    return DicomToolkit::buildSagittalSlice(m_segmentationMaskSlices, m_dicomSagittalIndex);
}

void MainWindow::rebuildSegmentationPreview() {
    if (!m_dicomMode || m_windowedSlices.isEmpty()) {
        return;
    }
    refreshSegmentationSurfaceCache();
    updateDicomViews();
}

void MainWindow::invalidateSegmentationSurfaceCache() {
    m_segmentationSurfaceCache = {};
    m_segmentationSurfaceDirty = true;
    m_segmentationSurfaceFinalReady = false;
    ++m_segmentationSurfaceGeneration;
}

void MainWindow::refreshSegmentationSurfaceCache() {
    if (!m_segmentationSurfaceDirty) {
        return;
    }
    scheduleSegmentationSurfaceBuild();
}

void MainWindow::scheduleSegmentationSurfaceBuild() {
    m_segmentationSurfaceDirty = false;
    m_segmentationSurfaceFinalReady = false;

    if (!m_showSegmentation || m_segmentationMaskSlices.isEmpty()) {
        m_segmentationSurfaceCache = {};
        updateSegmentationSurfaceView();
        return;
    }

    const int generation = m_segmentationSurfaceGeneration;
    const QVector<QImage> masks = m_segmentationMaskSlices;
    const double spacingX = m_dicomSeries.pixelSpacingX;
    const double spacingY = m_dicomSeries.pixelSpacingY;
    const double spacingZ = m_dicomSeries.sliceThickness;
    const int maxDim = std::max({m_dicomSeries.width, m_dicomSeries.height, static_cast<int>(m_segmentationMaskSlices.size())});
    const int previewStep = maxDim >= 512 ? 4 : (maxDim >= 256 ? 2 : 1);

    m_segmentationSurfaceCache = {};
    updateSegmentationSurfaceView();

    m_segmentationPreviewWatcher->setProperty("generation", generation);
    m_segmentationPreviewWatcher->setFuture(QtConcurrent::run([masks, spacingX, spacingY, spacingZ, previewStep]() {
        return SegmentationMeshBuilder::buildSurfaceMesh(masks, spacingX, spacingY, spacingZ, previewStep);
    }));

    m_segmentationFullWatcher->setProperty("generation", generation);
    m_segmentationFullWatcher->setFuture(QtConcurrent::run([masks, spacingX, spacingY, spacingZ]() {
        return SegmentationMeshBuilder::buildSurfaceMesh(masks, spacingX, spacingY, spacingZ, 1);
    }));
}

void MainWindow::applySegmentationSurfaceResult(const SegmentationSurfaceData& surface, int generation, bool finalResult) {
    if (generation != m_segmentationSurfaceGeneration || !m_showSegmentation) {
        return;
    }

    if (!surface.isEmpty() || m_segmentationSurfaceCache.isEmpty()) {
        m_segmentationSurfaceCache = surface;
    }
    if (finalResult) {
        m_segmentationSurfaceFinalReady = true;
    }
    updateSegmentationSurfaceView();
}

void MainWindow::updateSegmentationSurfaceView() {
    if (!m_volumeView) {
        return;
    }

    const int depth = std::max(1, static_cast<int>(m_dicomSeries.rawSlices.size()));
    m_volumeView->setSliceGeometry(
        std::max(1, m_dicomSeries.width),
        std::max(1, m_dicomSeries.height),
        depth,
        m_dicomSeries.pixelSpacingX,
        m_dicomSeries.pixelSpacingY,
        m_dicomSeries.sliceThickness
    );
    m_volumeView->setSlicePlanes(m_dicomAxialIndex, m_dicomCoronalIndex, m_dicomSagittalIndex);

    if (m_showSegmentation && !m_segmentationSurfaceCache.isEmpty()) {
        m_volumeView->setSurfaceData(m_segmentationSurfaceCache);
    } else {
        m_volumeView->clearSurfaceData();
    }
}

void MainWindow::updateDicomViews() {
    if (!m_dicomMode || m_dicomSeries.rawSlices.isEmpty()) {
        return;
    }

    if (m_windowedSlices.size() != m_dicomSeries.rawSlices.size() ||
        !qFuzzyCompare(m_cachedWindowWidth + 1.0, m_dicomWindowWidth + 1.0) ||
        !qFuzzyCompare(m_cachedWindowCenter + 1.0, m_dicomWindowCenter + 1.0)) {
        m_windowedSlices.clear();
        m_windowedSlices.reserve(m_dicomSeries.rawSlices.size());
        for (const QImage& slice : m_dicomSeries.rawSlices) {
            m_windowedSlices.append(DicomToolkit::applyWindowLevel(slice, m_dicomWindowCenter, m_dicomWindowWidth));
        }
        m_cachedWindowWidth = m_dicomWindowWidth;
        m_cachedWindowCenter = m_dicomWindowCenter;
    }

    const QVector<QImage> displaySource = m_dicomProcessedSlices.isEmpty() ? m_windowedSlices : m_dicomProcessedSlices;

    const int depth = static_cast<int>(displaySource.size());
    m_dicomAxialIndex = std::clamp(m_dicomAxialIndex, 0, depth - 1);
    m_dicomCoronalIndex = std::clamp(m_dicomCoronalIndex, 0, std::max(0, m_dicomSeries.height - 1));
    m_dicomSagittalIndex = std::clamp(m_dicomSagittalIndex, 0, std::max(0, m_dicomSeries.width - 1));

    QImage axial = displaySource[m_dicomAxialIndex];
    QImage coronal = DicomToolkit::buildCoronalSlice(displaySource, m_dicomCoronalIndex);
    QImage sagittal = DicomToolkit::buildSagittalSlice(displaySource, m_dicomSagittalIndex);

    if (m_showSegmentation && !m_segmentationMaskSlices.isEmpty()) {
        axial = SegmentationOverlay::overlayMask(axial, m_segmentationMaskSlices.value(m_dicomAxialIndex), QColor("#17b51f"));
        coronal = SegmentationOverlay::overlayMask(coronal, buildCoronalMaskSlice(), QColor("#17b51f"));
        sagittal = SegmentationOverlay::overlayMask(sagittal, buildSagittalMaskSlice(), QColor("#17b51f"));
    }
    axial = SegmentationOverlay::drawSeeds(axial, m_segmentationSeeds, m_dicomAxialIndex, Qt::Orientation::Horizontal, -1);
    coronal = SegmentationOverlay::drawSeeds(coronal, m_segmentationSeeds, depth - 1, Qt::Orientation::Horizontal, m_dicomCoronalIndex);
    sagittal = SegmentationOverlay::drawSeeds(sagittal, m_segmentationSeeds, depth - 1, Qt::Orientation::Vertical, m_dicomSagittalIndex);
    sagittal = rotateSagittalForDisplay(sagittal);

    if (m_axialView) { m_axialView->setImages(axial, axial); m_axialView->setZoomFactor(m_zoom); }
    if (m_coronalView) { m_coronalView->setImages(coronal, coronal); m_coronalView->setZoomFactor(m_zoom); }
    if (m_sagittalView) { m_sagittalView->setImages(sagittal, sagittal); m_sagittalView->setZoomFactor(m_zoom); }
    if (m_volumeView) {
        updateSegmentationSurfaceView();
    }

    m_filteredImage = axial;
    if (m_infoLabel) {
        m_infoLabel->setPlainText(
            QStringLiteral("文件：%1\n格式：DICOM 序列\n尺寸：%2 x %3\n切片数：%4\n窗宽/窗位：%5 / %6\nSEG：%7")
                .arg(QFileInfo(m_currentFile).fileName())
                .arg(m_dicomSeries.width)
                .arg(m_dicomSeries.height)
                .arg(depth)
                .arg(static_cast<int>(m_dicomWindowWidth))
                .arg(static_cast<int>(m_dicomWindowCenter))
                .arg(m_loadedSegData.valid
                    ? QStringLiteral("%1 (%2)").arg(m_loadedSegData.segmentLabel, m_loadedSegData.segmentAlgorithmType)
                    : QString::fromUtf8(u8"未加载"))
        );
    }
}

QImage MainWindow::currentDisplayImage() const {
    if (m_dicomMode && !m_windowedSlices.isEmpty()) {
        return m_windowedSlices.value(m_dicomAxialIndex);
    }
    if (!m_filteredImage.isNull()) {
        return m_filteredImage;
    }
    return m_originalImage;
}

QVector<QImage> MainWindow::currentDicomProcessingInput() const {
    if (!m_dicomProcessedSlices.isEmpty()) {
        return m_dicomProcessedSlices;
    }
    return m_windowedSlices;
}

void MainWindow::applyDicomAction(const QString& actionName) {
    if (!m_dicomMode || m_windowedSlices.isEmpty()) {
        return;
    }

    DicomProcessRequest request;
    request.actionName = actionName;

    if (actionName == QString::fromUtf8(u8"均值滤波") ||
        actionName == QString::fromUtf8(u8"中值滤波") ||
        actionName == QString::fromUtf8(u8"最大值滤波")) {
        bool ok = false;
        const int kernel = QInputDialog::getInt(this, QString::fromUtf8(u8"滤波参数"), QString::fromUtf8(u8"核大小"), 3, 3, 15, 2, &ok);
        if (!ok) return;
        request.kernelSize = kernel | 1;
    } else if (actionName == QString::fromUtf8(u8"Gamma 变换")) {
        bool ok = false;
        const double gamma = QInputDialog::getDouble(this, QString::fromUtf8(u8"Gamma 参数"), QStringLiteral("Gamma"), 0.8, 0.1, 5.0, 2, &ok);
        if (!ok) return;
        request.gamma = gamma;
    } else if (actionName == QString::fromUtf8(u8"理想低通") ||
               actionName == QString::fromUtf8(u8"巴特沃斯低通") ||
               actionName == QString::fromUtf8(u8"理想高通") ||
               actionName == QString::fromUtf8(u8"巴特沃斯高通") ||
               actionName == QString::fromUtf8(u8"同态滤波")) {
        bool ok = false;
        const double cutoff = QInputDialog::getDouble(this, QString::fromUtf8(u8"频域参数"), QString::fromUtf8(u8"截止半径"), 40.0, 1.0, 512.0, 1, &ok);
        if (!ok) return;
        request.cutoff = cutoff;
    }

    const DicomProcessingScope scope = (m_dicomProcessingScopeCombo && m_dicomProcessingScopeCombo->currentIndex() == 1)
        ? DicomProcessingScope::WholeVolume
        : DicomProcessingScope::CurrentSlice;
    const int targetIndex = m_dicomProcessingTargetCombo ? m_dicomProcessingTargetCombo->currentIndex() : 0;
    QVector<QImage> input = currentDicomProcessingInput();
    if (targetIndex == 1 && !m_dicomSegmentationInputSlices.isEmpty()) {
        input = m_dicomSegmentationInputSlices;
    }

    QVector<QImage> output;
    QString error;
    if (!DicomProcessingPipeline::apply(input, scope, m_dicomAxialIndex, request, &output, &error)) {
        QMessageBox::warning(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"DICOM 处理失败：\n") + error);
        return;
    }

    if (targetIndex == 0) {
        m_dicomProcessedSlices = output;
    } else if (targetIndex == 1) {
        m_dicomSegmentationInputSlices = output;
        m_dicomProcessedSlices = output;
    } else {
        m_dicomProcessedSlices = output;
    }

    m_parameterRecords.append(QStringLiteral("%1: 模式=%2, 目标=%3")
        .arg(actionName)
        .arg(scope == DicomProcessingScope::WholeVolume ? QString::fromUtf8(u8"全体数据") : QString::fromUtf8(u8"当前切片"))
        .arg(m_dicomProcessingTargetCombo ? m_dicomProcessingTargetCombo->currentText() : QString::fromUtf8(u8"原图")));
    rebuildSegmentationPreview();
    updateProcessingStatus(QString::fromUtf8(u8"DICOM 处理已完成"));
}

void MainWindow::renderCurrentImage() {
    if (m_dicomMode) {
        updateDicomViews();
        return;
    }
    if (m_originalImage.isNull()) {
        return;
    }

    if (m_filterCombo) {
        m_filteredImage = applyFilter(m_originalImage, m_filterCombo->currentText());
    }
    refreshWorkbenchImages();
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
        refreshWorkbenchImages();
    }
}

void MainWindow::zoomOut() {
    if (m_originalImage.isNull()) {
        return;
    }

    double newZoom = qRound((m_zoom - m_zoomStep) * 100.0) / 100.0;
    if (newZoom >= m_minZoom) {
        m_zoom = newZoom;
        refreshWorkbenchImages();
    }
}

void MainWindow::resetZoom() {
    if (m_originalImage.isNull()) {
        return;
    }

    m_zoom = 1.0;
    refreshWorkbenchImages();
}

void MainWindow::updateHoverInfo(int x, int y, QRgb rgba) {
    QRgb filteredRgba = rgba;
    const QString processText = m_processingChain.isEmpty()
        ? QString::fromUtf8(u8"无")
        : m_processingChain.join(QString::fromUtf8(u8" 步"));

    if (!m_filteredImage.isNull() &&
        x >= 0 && y >= 0 &&
        x < m_filteredImage.width() &&
        y < m_filteredImage.height()) {
        filteredRgba = m_filteredImage.pixel(x, y);
    }

    statusBar()->showMessage(
        QStringLiteral(
            "状态：文件=%1 | 坐标=(%2, %3) | 原始 RGBA=(%4, %5, %6, %7) | 滤镜 RGBA=(%8, %9, %10, %11) | 滤镜=%12 | 缩放=%13%"
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
            .arg(processText)
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
    QRgb originalRgba = rgba;
    QRgb filteredRgba = rgba;
    const QString processText = m_processingChain.isEmpty()
        ? QString::fromUtf8(u8"无")
        : m_processingChain.join(QString::fromUtf8(u8" 步"));

    if (!m_originalImage.isNull() &&
        x >= 0 && y >= 0 &&
        x < m_originalImage.width() &&
        y < m_originalImage.height()) {
        originalRgba = m_originalImage.pixel(x, y);
    }

    if (!m_filteredImage.isNull() &&
        x >= 0 && y >= 0 &&
        x < m_filteredImage.width() &&
        y < m_filteredImage.height()) {
        filteredRgba = m_filteredImage.pixel(x, y);
    } else if (sender() == m_resultView) {
        filteredRgba = rgba;
    }

    QMessageBox::information(
        this,
        QStringLiteral("像素信息"),
        QStringLiteral(
            "文件：%1\n"
            "坐标：(%2, %3)\n"
            "原始颜色 RGBA：(%4, %5, %6, %7)\n"
            "处理后颜色 RGBA：(%8, %9, %10, %11)\n"
            "当前处理：%12\n"
            "缩放：%13%"
        )
            .arg(m_currentFile.isEmpty() ? QString::fromUtf8(u8"无") : QFileInfo(m_currentFile).fileName())
            .arg(x)
            .arg(y)
            .arg(qRed(originalRgba))
            .arg(qGreen(originalRgba))
            .arg(qBlue(originalRgba))
            .arg(qAlpha(originalRgba))
            .arg(qRed(filteredRgba))
            .arg(qGreen(filteredRgba))
            .arg(qBlue(filteredRgba))
            .arg(qAlpha(filteredRgba))
            .arg(processText)
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
    const QImage source = currentDisplayImage();
    if (source.isNull()) {
        if (m_histogramLabel) {
            m_histogramLabel->setPixmap(QPixmap());
            m_histogramLabel->setText(QStringLiteral("暂无直方图"));
        }
        return;
    }

    if (!m_histogramLabel) {
        return;
    }

    const QSize target = m_histogramLabel->size().isValid() ? m_histogramLabel->size() : QSize(180, 80);
    QPixmap hist = ImageHistTools::drawGrayHistogram(source, target.width(), target.height());
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

QImage MainWindow::currentProcessingInput() const {
    if (m_historyIndex >= 0 && m_historyIndex < m_resultHistory.size()) {
        return m_resultHistory[m_historyIndex];
    }
    return m_originalImage;
}

void MainWindow::pushProcessingResult(const QImage& image, const QString& actionName) {
    if (image.isNull()) {
        QMessageBox::warning(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"当前处理没有生成有效结果"));
        return;
    }

    while (m_resultHistory.size() > m_historyIndex + 1) {
        m_resultHistory.removeLast();
    }
    while (m_chainHistory.size() > m_historyIndex + 1) {
        m_chainHistory.removeLast();
    }
    while (m_parameterHistory.size() > m_historyIndex + 1) {
        m_parameterHistory.removeLast();
    }

    m_processingChain.append(actionName);
    m_resultHistory.append(image);
    m_chainHistory.append(m_processingChain);
    m_parameterHistory.append(m_parameterRecords);
    m_historyIndex = m_resultHistory.size() - 1;
    m_filteredImage = image;
    refreshWorkbenchImages();
    updateProcessingStatus(QString::fromUtf8(u8"当前结果已更新"));
}

void MainWindow::refreshWorkbenchImages() {
    if (m_dicomMode) {
        updateDicomViews();
        if (m_zoomLabel) {
            m_zoomLabel->setText(QStringLiteral("%1%").arg(static_cast<int>(m_zoom * 100)));
        }
        updateHistogram();
        return;
    }

    if (m_imageView) {
        m_imageView->setImages(m_originalImage, m_originalImage);
        m_imageView->setZoomFactor(m_zoom);
    }

    if (m_historyIndex >= 0 && m_historyIndex < m_resultHistory.size()) {
        m_filteredImage = m_resultHistory[m_historyIndex];
    } else {
        m_filteredImage = {};
    }

    if (m_resultView) {
        const QImage result = m_filteredImage.isNull() ? m_originalImage : m_filteredImage;
        m_resultView->setImages(result, result);
        m_resultView->setZoomFactor(m_zoom);
    }

    if (m_fitOnNextRefresh && !m_originalImage.isNull()) {
        double fittedZoom = m_zoom;
        if (m_imageView) {
            fittedZoom = m_imageView->fitToView();
        }
        if (m_resultView) {
            m_resultView->setZoomFactor(fittedZoom);
        }
        m_zoom = fittedZoom;
        m_fitOnNextRefresh = false;
    }

    if (m_zoomLabel) {
        m_zoomLabel->setText(QStringLiteral("%1%").arg(static_cast<int>(m_zoom * 100)));
    }
    updateHistogram();
}

void MainWindow::updateProcessingStatus(const QString& message) {
    if (m_chainCombo) {
        m_chainCombo->clear();
        if (m_processingChain.isEmpty()) {
            m_chainCombo->addItem(QString::fromUtf8(u8"当前处理：无"));
        } else {
            m_chainCombo->addItem(QString::fromUtf8(u8"当前处理"));
            for (int i = 0; i < m_processingChain.size(); ++i) {
                m_chainCombo->addItem(QStringLiteral("%1. %2").arg(i + 1).arg(m_processingChain.at(i)));
            }
        }
        m_chainCombo->setCurrentIndex(0);
    }
    if (m_chainLabel) {
        const QString chain = m_processingChain.isEmpty()
            ? QString::fromUtf8(u8"当前处理：无")
            : QString::fromUtf8(u8"当前处理：") + m_processingChain.join(QString::fromUtf8(u8" 步"));
        m_chainLabel->setText(chain);
    }
    const bool useSeedHistory = m_dicomMode && (!m_seedHistory.isEmpty() || !m_segmentationSeeds.isEmpty());
    if (m_undoButton) {
        m_undoButton->setEnabled(useSeedHistory ? canUndoSeedHistory() : (m_historyIndex >= 0));
    }
    if (m_redoButton) {
        m_redoButton->setEnabled(useSeedHistory ? canRedoSeedHistory() : (m_historyIndex + 1 < m_resultHistory.size()));
    }
    if (m_resetButton) {
        m_resetButton->setEnabled(useSeedHistory ? (!m_seedHistory.isEmpty() && (m_seedHistory.size() > 1 || !m_segmentationSeeds.isEmpty()))
                                                 : !m_resultHistory.isEmpty());
    }
    if (!message.isEmpty()) {
        statusBar()->showMessage(message);
    }
    updateParameterStatus();
}

void MainWindow::updateParameterStatus() {
    if (m_parameterCombo) {
        m_parameterCombo->clear();
        if (m_parameterRecords.isEmpty()) {
            m_parameterCombo->addItem(QString::fromUtf8(u8"参数记录：无"));
        } else {
            m_parameterCombo->addItem(QString::fromUtf8(u8"参数记录"));
            for (int i = 0; i < m_parameterRecords.size(); ++i) {
                m_parameterCombo->addItem(QStringLiteral("%1. %2").arg(i + 1).arg(m_parameterRecords.at(i)));
            }
        }
        m_parameterCombo->setCurrentIndex(0);
    }
    if (!m_parameterLabel) {
        return;
    }
    const QString text = m_parameterRecords.isEmpty()
        ? QString::fromUtf8(u8"参数记录：无")
        : QString::fromUtf8(u8"参数记录：") + m_parameterRecords.join(QString::fromUtf8(u8"无"));
    m_parameterLabel->setText(text);
}

void MainWindow::undoProcessing() {
    if (canUndoSeedHistory()) {
        --m_seedHistoryIndex;
        applyCurrentSeedHistory(QString::fromUtf8(u8"已撤回上一个种子记录"));
        return;
    }
    if (m_historyIndex < 0) {
        return;
    }
    --m_historyIndex;
    m_processingChain = m_historyIndex >= 0 ? m_chainHistory.value(m_historyIndex) : QStringList();
    m_parameterRecords = m_historyIndex >= 0 ? m_parameterHistory.value(m_historyIndex) : QStringList();
    refreshWorkbenchImages();
    updateProcessingStatus(QString::fromUtf8(u8"已撤回上一步处无"));
}

void MainWindow::redoProcessing() {
    if (canRedoSeedHistory()) {
        ++m_seedHistoryIndex;
        applyCurrentSeedHistory(QString::fromUtf8(u8"已恢复下一个种子记录"));
        return;
    }
    if (m_historyIndex + 1 >= m_resultHistory.size()) {
        return;
    }
    ++m_historyIndex;
    m_processingChain = m_chainHistory.value(m_historyIndex);
    m_parameterRecords = m_parameterHistory.value(m_historyIndex);
    refreshWorkbenchImages();
    updateProcessingStatus(QString::fromUtf8(u8"已前进到下一步处无"));
}

void MainWindow::resetProcessing() {
    if (m_dicomMode && (!m_seedHistory.isEmpty() && (m_seedHistory.size() > 1 || !m_segmentationSeeds.isEmpty()))) {
        clearSeedHistory();
        rebuildSegmentationPreview();
        updateProcessingStatus(QString::fromUtf8(u8"种子记录已重置"));
        return;
    }
    m_resultHistory.clear();
    m_chainHistory.clear();
    m_processingChain.clear();
    m_parameterHistory.clear();
    m_parameterRecords.clear();
    m_historyIndex = -1;
    m_filteredImage = {};
    m_frequencyIfftSource = {};
    m_lastDegradedRawImage = {};
    refreshWorkbenchImages();
    updateProcessingStatus(QString::fromUtf8(u8"处理链已重置"));
}

void MainWindow::applyProcessingAction(const QString& actionName) {
    if (m_dicomMode) {
        applyDicomAction(actionName);
        return;
    }
    if (m_originalImage.isNull()) {
        QMessageBox::information(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先加载影像"));
        return;
    }

    auto makeNumberStepper = [](QDialog* dialog, double initial, double minValue, double maxValue, double step, int decimals, QLineEdit** editOut) {
        auto* frame = new QFrame(dialog);
        frame->setObjectName(QStringLiteral("StepperFrame"));
        frame->setFixedSize(128, 30);
        auto* layout = new QHBoxLayout(frame);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        auto* edit = new QLineEdit(frame);
        edit->setObjectName(QStringLiteral("StepperEdit"));
        edit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        edit->setText(decimals == 0
            ? QString::number(static_cast<int>(initial))
            : QString::number(initial, 'f', decimals));

        auto* buttonColumn = new QVBoxLayout();
        buttonColumn->setContentsMargins(0, 0, 0, 0);
        buttonColumn->setSpacing(0);
        auto* upButton = new QPushButton(QString::fromUtf8(u8"▲"), frame);
        auto* downButton = new QPushButton(QString::fromUtf8(u8"▼"), frame);
        upButton->setObjectName(QStringLiteral("StepperButton"));
        downButton->setObjectName(QStringLiteral("StepperButton"));
        upButton->setCursor(Qt::PointingHandCursor);
        downButton->setCursor(Qt::PointingHandCursor);
        buttonColumn->addWidget(upButton);
        buttonColumn->addWidget(downButton);

        auto parseValue = [edit, initial]() {
            bool ok = false;
            const double value = edit->text().toDouble(&ok);
            return ok ? value : initial;
        };
        auto setValue = [edit, minValue, maxValue, decimals](double value) {
            value = std::clamp(value, minValue, maxValue);
            edit->setText(decimals == 0
                ? QString::number(static_cast<int>(std::round(value)))
                : QString::number(value, 'f', decimals));
        };
        connect(upButton, &QPushButton::clicked, frame, [parseValue, setValue, step]() {
            setValue(parseValue() + step);
        });
        connect(downButton, &QPushButton::clicked, frame, [parseValue, setValue, step]() {
            setValue(parseValue() - step);
        });
        connect(edit, &QLineEdit::editingFinished, frame, [parseValue, setValue]() {
            setValue(parseValue());
        });

        layout->addWidget(edit, 1);
        layout->addLayout(buttonColumn);
        *editOut = edit;
        return frame;
    };
    auto stepperValue = [](QLineEdit* edit, double fallback) {
        bool ok = false;
        const double value = edit ? edit->text().toDouble(&ok) : fallback;
        return ok ? value : fallback;
    };

    auto askNoiseDensity = [this](double* density) {
        QDialog dialog(this);
        dialog.setWindowTitle(QString::fromUtf8(u8"噪声参数"));
        auto* layout = new QGridLayout(&dialog);
        auto* slider = new QSlider(Qt::Horizontal, &dialog);
        auto* value = new QLabel(QStringLiteral("8%"), &dialog);
        auto* apply = new QPushButton(QString::fromUtf8(u8"应用"), &dialog);
        slider->setRange(0, 100);
        slider->setValue(static_cast<int>(std::round(m_lastNoiseDensity * 100.0)));
        layout->addWidget(new QLabel(QString::fromUtf8(u8"强度"), &dialog), 0, 0);
        layout->addWidget(slider, 0, 1);
        layout->addWidget(value, 0, 2);
        layout->addWidget(apply, 1, 2);
        connect(slider, &QSlider::valueChanged, value, [value](int v) {
            value->setText(QString::number(v) + QStringLiteral("%"));
        });
        connect(apply, &QPushButton::clicked, &dialog, &QDialog::accept);
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }
        *density = slider->value() / 100.0;
        m_lastNoiseDensity = *density;
        return true;
    };

    auto askGaussianNoiseParams = [this, &makeNumberStepper, &stepperValue](double* mean, double* sigma) {
        QDialog dialog(this);
        dialog.setWindowTitle(QString::fromUtf8(u8"高斯噪声参数"));
        auto* layout = new QGridLayout(&dialog);
        QLineEdit* meanEdit = nullptr;
        QLineEdit* sigmaEdit = nullptr;
        QWidget* meanStepper = makeNumberStepper(&dialog, m_lastGaussianMean, -100.0, 100.0, 1.0, 1, &meanEdit);
        QWidget* sigmaStepper = makeNumberStepper(&dialog, m_lastGaussianSigma, 0.0, 100.0, 1.0, 1, &sigmaEdit);
        auto* apply = new QPushButton(QString::fromUtf8(u8"应用"), &dialog);
        layout->addWidget(new QLabel(QString::fromUtf8(u8"均无"), &dialog), 0, 0);
        layout->addWidget(meanStepper, 0, 1, Qt::AlignLeft);
        layout->addWidget(new QLabel(QString::fromUtf8(u8"标准无"), &dialog), 1, 0);
        layout->addWidget(sigmaStepper, 1, 1, Qt::AlignLeft);
        layout->addWidget(apply, 2, 1);
        connect(apply, &QPushButton::clicked, &dialog, &QDialog::accept);
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }
        *mean = stepperValue(meanEdit, m_lastGaussianMean);
        *sigma = std::max(0.0, stepperValue(sigmaEdit, m_lastGaussianSigma));
        m_lastGaussianMean = *mean;
        m_lastGaussianSigma = *sigma;
        return true;
    };

    auto askKernel = [this, &makeNumberStepper, &stepperValue](int* kernelSize) {
        QDialog dialog(this);
        dialog.setWindowTitle(QString::fromUtf8(u8"滤波参数"));
        auto* layout = new QGridLayout(&dialog);
        QLineEdit* kernelEdit = nullptr;
        QWidget* kernelStepper = makeNumberStepper(&dialog, m_lastKernelSize, 3.0, 15.0, 2.0, 0, &kernelEdit);
        auto* apply = new QPushButton(QString::fromUtf8(u8"应用"), &dialog);
        layout->addWidget(new QLabel(QString::fromUtf8(u8"核大小"), &dialog), 0, 0);
        layout->addWidget(kernelStepper, 0, 1, Qt::AlignLeft);
        layout->addWidget(apply, 1, 1);
        connect(apply, &QPushButton::clicked, &dialog, &QDialog::accept);
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }
        *kernelSize = static_cast<int>(std::round(stepperValue(kernelEdit, 3.0)));
        if (*kernelSize % 2 == 0) {
            ++(*kernelSize);
        }
        *kernelSize = std::clamp(*kernelSize, 3, 15);
        m_lastKernelSize = *kernelSize;
        return true;
    };

    auto askEdgeParams = [this, &makeNumberStepper, &stepperValue](int* kernelSize, int* threshold) {
        QDialog dialog(this);
        dialog.setWindowTitle(QString::fromUtf8(u8"边缘参数"));
        auto* layout = new QGridLayout(&dialog);
        QLineEdit* kernelEdit = nullptr;
        QWidget* kernelStepper = makeNumberStepper(&dialog, m_lastEdgeKernelSize, 3.0, 5.0, 2.0, 0, &kernelEdit);
        auto* slider = new QSlider(Qt::Horizontal, &dialog);
        auto* value = new QLabel(QStringLiteral("80"), &dialog);
        auto* apply = new QPushButton(QString::fromUtf8(u8"应用"), &dialog);
        slider->setRange(0, 255);
        slider->setValue(m_lastEdgeThreshold);
        layout->addWidget(new QLabel(QString::fromUtf8(u8"核大小"), &dialog), 0, 0);
        layout->addWidget(kernelStepper, 0, 1, Qt::AlignLeft);
        layout->addWidget(new QLabel(QString::fromUtf8(u8"阈值"), &dialog), 1, 0);
        layout->addWidget(slider, 1, 1);
        layout->addWidget(value, 1, 2);
        layout->addWidget(apply, 2, 2);
        connect(slider, &QSlider::valueChanged, value, [value](int v) {
            value->setText(QString::number(v));
        });
        connect(apply, &QPushButton::clicked, &dialog, &QDialog::accept);
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }
        *kernelSize = static_cast<int>(std::round(stepperValue(kernelEdit, 3.0)));
        if (*kernelSize % 2 == 0) {
            ++(*kernelSize);
        }
        *kernelSize = std::clamp(*kernelSize, 3, 5);
        *threshold = slider->value();
        m_lastEdgeKernelSize = *kernelSize;
        m_lastEdgeThreshold = *threshold;
        return true;
    };

    auto askGamma = [this, &makeNumberStepper, &stepperValue](double* gamma) {
        QDialog dialog(this);
        dialog.setWindowTitle(QString::fromUtf8(u8"Gamma 参数"));
        auto* layout = new QGridLayout(&dialog);
        QLineEdit* gammaEdit = nullptr;
        QWidget* gammaStepper = makeNumberStepper(&dialog, m_lastGamma, 0.1, 5.0, 0.1, 2, &gammaEdit);
        auto* apply = new QPushButton(QString::fromUtf8(u8"应用"), &dialog);
        layout->addWidget(new QLabel(QStringLiteral("Gamma"), &dialog), 0, 0);
        layout->addWidget(gammaStepper, 0, 1, Qt::AlignLeft);
        layout->addWidget(apply, 1, 1);
        connect(apply, &QPushButton::clicked, &dialog, &QDialog::accept);
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }
        *gamma = std::clamp(stepperValue(gammaEdit, m_lastGamma), 0.1, 5.0);
        m_lastGamma = *gamma;
        return true;
    };

    auto askFrequencyParams = [this](const QString& name, double* cutoff, int* order, double* gammaLow, double* gammaHigh, double* c) {
        const bool noParams = name == QString::fromUtf8(u8"FFT 频谱") || name == QString::fromUtf8(u8"IFFT 重建");
        if (noParams) {
            return true;
        }

        const bool needsOrder = name == QString::fromUtf8(u8"巴特沃斯低通") || name == QString::fromUtf8(u8"巴特沃斯高通");
        const bool needsHomomorphic = name == QString::fromUtf8(u8"同态滤波");

        QDialog dialog(this);
        dialog.setWindowTitle(QString::fromUtf8(u8"频域参数 - 医学影像分析平台"));
        dialog.setModal(true);
        dialog.setMinimumWidth(430);

        auto* rootLayout = new QVBoxLayout(&dialog);
        rootLayout->setContentsMargins(18, 16, 18, 16);
        rootLayout->setSpacing(14);

        auto* title = new QLabel(QString::fromUtf8(u8"频域分析参数"), &dialog);
        title->setObjectName(QStringLiteral("PanelTitle"));
        auto* subtitle = new QLabel(name, &dialog);
        subtitle->setObjectName(QStringLiteral("MutedText"));
        rootLayout->addWidget(title);
        rootLayout->addWidget(subtitle);

        auto* formLayout = new QGridLayout();
        formLayout->setHorizontalSpacing(14);
        formLayout->setVerticalSpacing(12);
        formLayout->setColumnStretch(1, 1);

        int row = 0;
        auto makeNumberStepper = [&](double initial, double minValue, double maxValue, double step, int decimals, QLineEdit** editOut) {
            auto* frame = new QFrame(&dialog);
            frame->setObjectName(QStringLiteral("StepperFrame"));
            frame->setFixedSize(128, 30);
            auto* layout = new QHBoxLayout(frame);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(0);

            auto* edit = new QLineEdit(frame);
            edit->setObjectName(QStringLiteral("StepperEdit"));
            edit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            edit->setText(decimals == 0
                ? QString::number(static_cast<int>(initial))
                : QString::number(initial, 'f', decimals));

            auto* buttonColumn = new QVBoxLayout();
            buttonColumn->setContentsMargins(0, 0, 0, 0);
            buttonColumn->setSpacing(0);
            auto* upButton = new QPushButton(QString::fromUtf8(u8"▲"), frame);
            auto* downButton = new QPushButton(QString::fromUtf8(u8"▼"), frame);
            upButton->setObjectName(QStringLiteral("StepperButton"));
            downButton->setObjectName(QStringLiteral("StepperButton"));
            upButton->setCursor(Qt::PointingHandCursor);
            downButton->setCursor(Qt::PointingHandCursor);
            buttonColumn->addWidget(upButton);
            buttonColumn->addWidget(downButton);

            auto parseValue = [edit, initial]() {
                bool ok = false;
                const double value = edit->text().toDouble(&ok);
                return ok ? value : initial;
            };
            auto setValue = [edit, minValue, maxValue, decimals](double value) {
                value = std::clamp(value, minValue, maxValue);
                edit->setText(decimals == 0
                    ? QString::number(static_cast<int>(std::round(value)))
                    : QString::number(value, 'f', decimals));
            };
            connect(upButton, &QPushButton::clicked, frame, [parseValue, setValue, step]() {
                setValue(parseValue() + step);
            });
            connect(downButton, &QPushButton::clicked, frame, [parseValue, setValue, step]() {
                setValue(parseValue() - step);
            });
            connect(edit, &QLineEdit::editingFinished, frame, [parseValue, setValue]() {
                setValue(parseValue());
            });

            layout->addWidget(edit, 1);
            layout->addLayout(buttonColumn);
            *editOut = edit;
            return frame;
        };
        auto stepperValue = [](QLineEdit* edit, double fallback) {
            bool ok = false;
            const double value = edit ? edit->text().toDouble(&ok) : fallback;
            return ok ? value : fallback;
        };
        auto addNumberRow = [&](const QString& labelText, QWidget* editor) {
            formLayout->addWidget(new QLabel(labelText, &dialog), row, 0);
            formLayout->addWidget(editor, row, 1, Qt::AlignLeft);
            ++row;
        };

        QLineEdit* cutoffEdit = nullptr;
        addNumberRow(QString::fromUtf8(u8"截止半径"), makeNumberStepper(m_lastFrequencyCutoff, 1.0, 4096.0, 1.0, 0, &cutoffEdit));

        QLineEdit* orderEdit = nullptr;
        if (needsOrder) {
            addNumberRow(QString::fromUtf8(u8"阶数"), makeNumberStepper(m_lastFrequencyOrder, 1.0, 10.0, 1.0, 0, &orderEdit));
        }

        QLineEdit* gammaLowEdit = nullptr;
        QLineEdit* gammaHighEdit = nullptr;
        QLineEdit* cEdit = nullptr;
        if (needsHomomorphic) {
            addNumberRow(QStringLiteral("γL"), makeNumberStepper(m_lastHomomorphicGammaLow, 0.01, 5.0, 0.05, 2, &gammaLowEdit));
            addNumberRow(QStringLiteral("γH"), makeNumberStepper(m_lastHomomorphicGammaHigh, 0.01, 5.0, 0.05, 2, &gammaHighEdit));
            addNumberRow(QStringLiteral("c"), makeNumberStepper(m_lastHomomorphicC, 0.01, 10.0, 0.1, 2, &cEdit));
        }

        rootLayout->addLayout(formLayout);

        auto* buttonRow = new QHBoxLayout();
        buttonRow->addStretch(1);
        auto* cancel = new QPushButton(QString::fromUtf8(u8"取消"), &dialog);
        auto* apply = new QPushButton(QString::fromUtf8(u8"应用"), &dialog);
        cancel->setCursor(Qt::PointingHandCursor);
        apply->setCursor(Qt::PointingHandCursor);
        buttonRow->addWidget(cancel);
        buttonRow->addWidget(apply);
        rootLayout->addLayout(buttonRow);

        connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(apply, &QPushButton::clicked, &dialog, &QDialog::accept);

        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }

        *cutoff = stepperValue(cutoffEdit, m_lastFrequencyCutoff);
        if (orderEdit) {
            *order = static_cast<int>(std::round(stepperValue(orderEdit, m_lastFrequencyOrder)));
        }
        if (gammaLowEdit) {
            *gammaLow = stepperValue(gammaLowEdit, m_lastHomomorphicGammaLow);
        }
        if (gammaHighEdit) {
            *gammaHigh = stepperValue(gammaHighEdit, m_lastHomomorphicGammaHigh);
        }
        if (cEdit) {
            *c = stepperValue(cEdit, m_lastHomomorphicC);
        }
        m_lastFrequencyCutoff = *cutoff;
        m_lastFrequencyOrder = *order;
        m_lastHomomorphicGammaLow = *gammaLow;
        m_lastHomomorphicGammaHigh = *gammaHigh;
        m_lastHomomorphicC = *c;
        return true;
    };

    auto askRestorationParams = [this, &makeNumberStepper, &stepperValue](
        const QString& name,
        RestorationModel* model,
        RestorationParams* params
    ) {
        QDialog dialog(this);
        dialog.setWindowTitle(QString::fromUtf8(u8"图像复原参数"));
        dialog.setModal(true);
        dialog.setMinimumWidth(430);

        auto* rootLayout = new QVBoxLayout(&dialog);
        rootLayout->setContentsMargins(18, 16, 18, 16);
        rootLayout->setSpacing(14);

        auto* title = new QLabel(QString::fromUtf8(u8"图像复原参数"), &dialog);
        title->setObjectName(QStringLiteral("PanelTitle"));
        auto* subtitle = new QLabel(name, &dialog);
        subtitle->setObjectName(QStringLiteral("MutedText"));
        rootLayout->addWidget(title);
        rootLayout->addWidget(subtitle);

        auto* formLayout = new QGridLayout();
        formLayout->setHorizontalSpacing(14);
        formLayout->setVerticalSpacing(12);
        formLayout->setColumnStretch(1, 1);

        int row = 0;
        auto* modelCombo = new QComboBox(&dialog);
        modelCombo->addItem(QString::fromUtf8(u8"大气湍流"), static_cast<int>(RestorationModel::AtmosphericTurbulence));
        modelCombo->addItem(QString::fromUtf8(u8"运动模糊"), static_cast<int>(RestorationModel::MotionBlur));
        modelCombo->setCurrentIndex(m_lastRestorationModel == RestorationModel::AtmosphericTurbulence ? 0 : 1);
        formLayout->addWidget(new QLabel(QString::fromUtf8(u8"退化模型"), &dialog), row, 0);
        formLayout->addWidget(modelCombo, row, 1);
        ++row;

        auto addNumberRow = [&](const QString& labelText, QWidget* editor, QLabel** labelOut) {
            auto* label = new QLabel(labelText, &dialog);
            formLayout->addWidget(label, row, 0);
            formLayout->addWidget(editor, row, 1, Qt::AlignLeft);
            if (labelOut) {
                *labelOut = label;
            }
            ++row;
        };

        QLabel* turbulenceLabel = nullptr;
        QLabel* motionALabel = nullptr;
        QLabel* motionBLabel = nullptr;
        QLabel* motionTLabel = nullptr;
        QLabel* cutoffLabel = nullptr;
        QLabel* wienerLabel = nullptr;
        QLineEdit* turbulenceEdit = nullptr;
        QLineEdit* motionAEdit = nullptr;
        QLineEdit* motionBEdit = nullptr;
        QLineEdit* motionTEdit = nullptr;
        QLineEdit* cutoffEdit = nullptr;
        QLineEdit* wienerEdit = nullptr;
        QWidget* turbulenceStepper = makeNumberStepper(&dialog, m_lastRestorationTurbulenceK, 0.00001, 0.01, 0.00005, 5, &turbulenceEdit);
        QWidget* motionAStepper = makeNumberStepper(&dialog, m_lastRestorationMotionA, -1.0, 1.0, 0.01, 3, &motionAEdit);
        QWidget* motionBStepper = makeNumberStepper(&dialog, m_lastRestorationMotionB, -1.0, 1.0, 0.01, 3, &motionBEdit);
        QWidget* motionTStepper = makeNumberStepper(&dialog, m_lastRestorationMotionT, 0.01, 10.0, 0.1, 2, &motionTEdit);
        QWidget* cutoffStepper = makeNumberStepper(&dialog, m_lastRestorationCutoff, 1.0, 4096.0, 1.0, 0, &cutoffEdit);
        QWidget* wienerStepper = makeNumberStepper(&dialog, m_lastRestorationWienerK, 0.0, 1.0, 0.001, 4, &wienerEdit);

        addNumberRow(QString::fromUtf8(u8"湍流系数 k"), turbulenceStepper, &turbulenceLabel);
        addNumberRow(QStringLiteral("a"), motionAStepper, &motionALabel);
        addNumberRow(QStringLiteral("b"), motionBStepper, &motionBLabel);
        addNumberRow(QStringLiteral("T"), motionTStepper, &motionTLabel);
        addNumberRow(QString::fromUtf8(u8"截止半径"), cutoffStepper, &cutoffLabel);
        addNumberRow(QString::fromUtf8(u8"维纳参数 K"), wienerStepper, &wienerLabel);

        auto setRowVisible = [](QLabel* label, QWidget* editor, bool visible) {
            if (label) {
                label->setVisible(visible);
            }
            if (editor) {
                editor->setVisible(visible);
            }
        };
        auto refreshVisibleParams = [&]() {
            const auto selectedModel = static_cast<RestorationModel>(modelCombo->currentData().toInt());
            const bool atmospheric = selectedModel == RestorationModel::AtmosphericTurbulence;
            const bool needsCutoff = name == QString::fromUtf8(u8"截止半径逆滤波");
            const bool needsWiener = name == QString::fromUtf8(u8"维纳滤波复原");

            setRowVisible(turbulenceLabel, turbulenceStepper, atmospheric);
            setRowVisible(motionALabel, motionAStepper, !atmospheric);
            setRowVisible(motionBLabel, motionBStepper, !atmospheric);
            setRowVisible(motionTLabel, motionTStepper, !atmospheric);
            setRowVisible(cutoffLabel, cutoffStepper, needsCutoff);
            setRowVisible(wienerLabel, wienerStepper, needsWiener);
        };

        connect(modelCombo, &QComboBox::currentIndexChanged, &dialog, refreshVisibleParams);
        refreshVisibleParams();

        rootLayout->addLayout(formLayout);

        auto* buttonRow = new QHBoxLayout();
        buttonRow->addStretch(1);
        auto* cancel = new QPushButton(QString::fromUtf8(u8"取消"), &dialog);
        auto* apply = new QPushButton(QString::fromUtf8(u8"应用"), &dialog);
        cancel->setCursor(Qt::PointingHandCursor);
        apply->setCursor(Qt::PointingHandCursor);
        buttonRow->addWidget(cancel);
        buttonRow->addWidget(apply);
        rootLayout->addLayout(buttonRow);

        connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(apply, &QPushButton::clicked, &dialog, &QDialog::accept);

        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }

        *model = static_cast<RestorationModel>(modelCombo->currentData().toInt());
        params->turbulenceK = stepperValue(turbulenceEdit, m_lastRestorationTurbulenceK);
        params->motionA = stepperValue(motionAEdit, m_lastRestorationMotionA);
        params->motionB = stepperValue(motionBEdit, m_lastRestorationMotionB);
        params->motionT = stepperValue(motionTEdit, m_lastRestorationMotionT);
        params->inverseCutoff = stepperValue(cutoffEdit, m_lastRestorationCutoff);
        params->wienerK = stepperValue(wienerEdit, m_lastRestorationWienerK);
        m_lastRestorationModel = *model;
        m_lastRestorationTurbulenceK = params->turbulenceK;
        m_lastRestorationMotionA = params->motionA;
        m_lastRestorationMotionB = params->motionB;
        m_lastRestorationMotionT = params->motionT;
        m_lastRestorationCutoff = params->inverseCutoff;
        m_lastRestorationWienerK = params->wienerK;
        return true;
    };

    QImage input = currentProcessingInput();
    QImage result;
    QString parameterRecord;

    if (actionName == QString::fromUtf8(u8"原图")) {
        result = m_originalImage;
    } else if (actionName == QString::fromUtf8(u8"灰度") ||
               actionName == QString::fromUtf8(u8"反相") ||
               actionName == QString::fromUtf8(u8"二值化") ||
               actionName == QString::fromUtf8(u8"暖色") ||
               actionName == QString::fromUtf8(u8"冷色") ||
               actionName == QString::fromUtf8(u8"边缘增强") ||
               actionName == QString::fromUtf8(u8"锐化")) {
        result = applyFilter(input, actionName);
    } else if (actionName == QString::fromUtf8(u8"线性拉伸")) {
        result = ImageHistTools::linearStretch(input);
    } else if (actionName == QString::fromUtf8(u8"均衡化")) {
        result = ImageHistTools::equalizeHist(input);
    } else if (actionName == QString::fromUtf8(u8"直方图匹配")) {
        QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8(u8"选择匹配目标影像"));
        if (path.isEmpty()) {
            return;
        }
        QImage target(path);
        if (target.isNull()) {
            QMessageBox::warning(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"无法加载目标影像"));
            return;
        }
        result = ImageHistTools::matchHist(input, target);
    } else if (actionName == QString::fromUtf8(u8"椒盐噪声") || actionName == QString::fromUtf8(u8"脉冲噪声")) {
        double density = 0.08;
        if (!askNoiseDensity(&density)) {
            return;
        }
        parameterRecord = QStringLiteral("%1: 强度=%2%").arg(actionName).arg(static_cast<int>(density * 100));
        result = actionName == QString::fromUtf8(u8"椒盐噪声")
            ? ImageLabProcessor::addSaltPepperNoise(input, density)
            : ImageLabProcessor::addImpulseNoise(input, density);
    } else if (actionName == QString::fromUtf8(u8"高斯噪声")) {
        double mean = 0.0;
        double sigma = 15.0;
        if (!askGaussianNoiseParams(&mean, &sigma)) {
            return;
        }
        parameterRecord = QStringLiteral("%1: 均值=%2, 标准差%3")
            .arg(actionName)
            .arg(mean, 0, 'f', 1)
            .arg(sigma, 0, 'f', 1);
        result = ImageLabProcessor::addGaussianNoise(input, mean, sigma);
    } else if (actionName == QString::fromUtf8(u8"均值滤波") ||
               actionName == QString::fromUtf8(u8"中值滤波") ||
               actionName == QString::fromUtf8(u8"最大值滤波")) {
        int kernelSize = 3;
        if (!askKernel(&kernelSize)) {
            return;
        }
        parameterRecord = QStringLiteral("%1: 核大小%2").arg(actionName).arg(kernelSize);
        if (actionName == QString::fromUtf8(u8"均值滤波")) {
            result = ImageLabProcessor::meanFilter(input, kernelSize);
        } else if (actionName == QString::fromUtf8(u8"中值滤波")) {
            result = ImageLabProcessor::medianFilter(input, kernelSize);
        } else {
            result = ImageLabProcessor::maxFilter(input, kernelSize);
        }
    } else if (actionName == QString::fromUtf8(u8"Sobel") ||
               actionName == QString::fromUtf8(u8"Prewitt") ||
               actionName == QString::fromUtf8(u8"Laplacian")) {
        int kernelSize = 3;
        int threshold = 80;
        if (!askEdgeParams(&kernelSize, &threshold)) {
            return;
        }
        parameterRecord = QStringLiteral("%1: 核大小%2, 阈值%3").arg(actionName).arg(kernelSize).arg(threshold);
        if (actionName == QString::fromUtf8(u8"Sobel")) {
            result = ImageLabProcessor::sobelEdgeDetect(input, kernelSize, threshold);
        } else if (actionName == QString::fromUtf8(u8"Prewitt")) {
            result = ImageLabProcessor::prewittEdgeDetect(input, kernelSize, threshold);
        } else {
            result = ImageLabProcessor::laplacianEdgeDetect(input, kernelSize, threshold);
        }
    } else if (actionName == QString::fromUtf8(u8"Laplacian 处理")) {
        result = ImageLabProcessor::step2_laplacianProcess(input);
    } else if (actionName == QString::fromUtf8(u8"锐化处理")) {
        result = ImageLabProcessor::step3_sharpenImage(input, ImageLabProcessor::step2_laplacianProcess(input));
    } else if (actionName == QString::fromUtf8(u8"Sobel 梯度")) {
        result = ImageLabProcessor::step4_sobelProcess(input);
    } else if (actionName == QString::fromUtf8(u8"均值平滑梯度")) {
        result = ImageLabProcessor::step5_meanFilterGradient(input);
    } else if (actionName == QString::fromUtf8(u8"掩膜增强")) {
        result = ImageLabProcessor::step6_maskImage(input, ImageLabProcessor::step5_meanFilterGradient(ImageLabProcessor::step4_sobelProcess(input)));
    } else if (actionName == QString::fromUtf8(u8"原图叠加掩膜")) {
        result = ImageLabProcessor::step7_addOriginalAndMask(m_originalImage, input);
    } else if (actionName == QString::fromUtf8(u8"Gamma 变换")) {
        double gamma = 0.8;
        if (!askGamma(&gamma)) {
            return;
        }
        parameterRecord = QStringLiteral("%1: Gamma=%2").arg(actionName).arg(gamma, 0, 'f', 2);
        result = ImageLabProcessor::step8_gammaTransform(input, gamma);
    } else if (actionName == QString::fromUtf8(u8"FFT 频谱") ||
               actionName == QString::fromUtf8(u8"IFFT 重建") ||
               actionName == QString::fromUtf8(u8"理想低通") ||
               actionName == QString::fromUtf8(u8"巴特沃斯低通") ||
               actionName == QString::fromUtf8(u8"理想高通") ||
               actionName == QString::fromUtf8(u8"巴特沃斯高通") ||
               actionName == QString::fromUtf8(u8"同态滤波")) {
        double cutoff = 40.0;
        int order = 2;
        double gammaLow = 0.5;
        double gammaHigh = 1.8;
        double c = 1.0;
        if (!askFrequencyParams(actionName, &cutoff, &order, &gammaLow, &gammaHigh, &c)) {
            return;
        }
        if (actionName == QString::fromUtf8(u8"理想低通") || actionName == QString::fromUtf8(u8"理想高通")) {
            parameterRecord = QStringLiteral("%1: 截止半径=%2").arg(actionName).arg(cutoff, 0, 'f', 0);
        } else if (actionName == QString::fromUtf8(u8"巴特沃斯低通") || actionName == QString::fromUtf8(u8"巴特沃斯高通")) {
            parameterRecord = QStringLiteral("%1: 截止半径=%2, 阶数=%3").arg(actionName).arg(cutoff, 0, 'f', 0).arg(order);
        } else if (actionName == QString::fromUtf8(u8"同态滤波")) {
            parameterRecord = QStringLiteral("%1: 截止半径=%2, γL=%3, γH=%4, c=%5")
                .arg(actionName)
                .arg(cutoff, 0, 'f', 0)
                .arg(gammaLow, 0, 'f', 2)
                .arg(gammaHigh, 0, 'f', 2)
                .arg(c, 0, 'f', 2);
        }
        if (actionName == QString::fromUtf8(u8"FFT 频谱")) {
            m_frequencyIfftSource = input;
            result = FourierExperimentWidget::processFrequencyImage(input, actionName, cutoff, order, gammaLow, gammaHigh, c);
        } else if (actionName == QString::fromUtf8(u8"IFFT 重建") &&
                   !m_frequencyIfftSource.isNull() &&
                   !m_processingChain.isEmpty() &&
                   m_processingChain.last() == QString::fromUtf8(u8"FFT 频谱")) {
            result = FourierExperimentWidget::processFrequencyImage(m_frequencyIfftSource, actionName, cutoff, order, gammaLow, gammaHigh, c);
        } else {
            result = FourierExperimentWidget::processFrequencyImage(input, actionName, cutoff, order, gammaLow, gammaHigh, c);
        }
    } else if (actionName == QString::fromUtf8(u8"图像退化") ||
               actionName == QString::fromUtf8(u8"逆滤波复原") ||
               actionName == QString::fromUtf8(u8"截止半径逆滤波") ||
               actionName == QString::fromUtf8(u8"维纳滤波复原")) {
        RestorationModel model = RestorationModel::AtmosphericTurbulence;
        RestorationParams params;
        if (!askRestorationParams(actionName, &model, &params)) {
            return;
        }

        QString modelName = model == RestorationModel::AtmosphericTurbulence
            ? QString::fromUtf8(u8"大气湍流")
            : QString::fromUtf8(u8"运动模糊");

        if (actionName == QString::fromUtf8(u8"图像退化")) {
            if (model == RestorationModel::AtmosphericTurbulence) {
                parameterRecord = QStringLiteral("%1: 模型=%2, k=%3")
                    .arg(actionName)
                    .arg(modelName)
                    .arg(params.turbulenceK, 0, 'f', 5);
                m_lastDegradedRawImage = ImageRestoration::atmosphericTurbulenceDegrade(input, params.turbulenceK);
            } else {
                parameterRecord = QStringLiteral("%1: 模型=%2, a=%3, b=%4, T=%5")
                    .arg(actionName)
                    .arg(modelName)
                    .arg(params.motionA, 0, 'f', 3)
                    .arg(params.motionB, 0, 'f', 3)
                    .arg(params.motionT, 0, 'f', 2);
                m_lastDegradedRawImage = ImageRestoration::motionBlurDegrade(input, params.motionA, params.motionB, params.motionT);
            }
            m_lastDegradationModel = model;
            m_lastDegradationParams = params;
            result = ImageRestoration::normalizeForDisplay(m_lastDegradedRawImage);
        } else if (actionName == QString::fromUtf8(u8"逆滤波复原")) {
            const bool reuseDegradationContext =
                !m_lastDegradedRawImage.isNull() &&
                !m_processingChain.isEmpty() &&
                m_processingChain.last() == QString::fromUtf8(u8"图像退化");
            const QImage restorationInput = reuseDegradationContext ? m_lastDegradedRawImage : input;
            if (reuseDegradationContext) {
                model = m_lastDegradationModel;
                params = m_lastDegradationParams;
                modelName = model == RestorationModel::AtmosphericTurbulence
                    ? QString::fromUtf8(u8"大气湍流")
                    : QString::fromUtf8(u8"运动模糊");
            }
            if (model == RestorationModel::AtmosphericTurbulence) {
                parameterRecord = QStringLiteral("%1: 模型=%2, k=%3")
                    .arg(actionName)
                    .arg(modelName)
                    .arg(params.turbulenceK, 0, 'f', 4);
            } else {
                parameterRecord = QStringLiteral("%1: 模型=%2, a=%3, b=%4, T=%5")
                    .arg(actionName)
                    .arg(modelName)
                    .arg(params.motionA, 0, 'f', 3)
                    .arg(params.motionB, 0, 'f', 3)
                    .arg(params.motionT, 0, 'f', 2);
            }
            result = ImageRestoration::inverseFilter(restorationInput, model, params);
        } else if (actionName == QString::fromUtf8(u8"截止半径逆滤波")) {
            const bool reuseDegradationContext =
                !m_lastDegradedRawImage.isNull() &&
                !m_processingChain.isEmpty() &&
                m_processingChain.last() == QString::fromUtf8(u8"图像退化");
            const QImage restorationInput = reuseDegradationContext ? m_lastDegradedRawImage : input;
            if (reuseDegradationContext) {
                model = m_lastDegradationModel;
                params.turbulenceK = m_lastDegradationParams.turbulenceK;
                params.motionA = m_lastDegradationParams.motionA;
                params.motionB = m_lastDegradationParams.motionB;
                params.motionT = m_lastDegradationParams.motionT;
                modelName = model == RestorationModel::AtmosphericTurbulence
                    ? QString::fromUtf8(u8"大气湍流")
                    : QString::fromUtf8(u8"运动模糊");
            }
            if (model == RestorationModel::AtmosphericTurbulence) {
                parameterRecord = QStringLiteral("%1: 模型=%2, 截止半径=%3, k=%4")
                    .arg(actionName)
                    .arg(modelName)
                    .arg(params.inverseCutoff, 0, 'f', 0)
                    .arg(params.turbulenceK, 0, 'f', 4);
            } else {
                parameterRecord = QStringLiteral("%1: 模型=%2, 截止半径=%3, a=%4, b=%5, T=%6")
                    .arg(actionName)
                    .arg(modelName)
                    .arg(params.inverseCutoff, 0, 'f', 0)
                    .arg(params.motionA, 0, 'f', 3)
                    .arg(params.motionB, 0, 'f', 3)
                    .arg(params.motionT, 0, 'f', 2);
            }
            result = ImageRestoration::inverseFilterWithCutoff(restorationInput, model, params);
        } else {
            const bool reuseDegradationContext =
                !m_lastDegradedRawImage.isNull() &&
                !m_processingChain.isEmpty() &&
                m_processingChain.last() == QString::fromUtf8(u8"图像退化");
            const QImage restorationInput = reuseDegradationContext ? m_lastDegradedRawImage : input;
            if (reuseDegradationContext) {
                model = m_lastDegradationModel;
                params.turbulenceK = m_lastDegradationParams.turbulenceK;
                params.motionA = m_lastDegradationParams.motionA;
                params.motionB = m_lastDegradationParams.motionB;
                params.motionT = m_lastDegradationParams.motionT;
                modelName = model == RestorationModel::AtmosphericTurbulence
                    ? QString::fromUtf8(u8"大气湍流")
                    : QString::fromUtf8(u8"运动模糊");
            }
            if (model == RestorationModel::AtmosphericTurbulence) {
                parameterRecord = QStringLiteral("%1: 模型=%2, K=%3, k=%4")
                    .arg(actionName)
                    .arg(modelName)
                    .arg(params.wienerK, 0, 'f', 4)
                    .arg(params.turbulenceK, 0, 'f', 4);
            } else {
                parameterRecord = QStringLiteral("%1: 模型=%2, K=%3, a=%4, b=%5, T=%6")
                    .arg(actionName)
                    .arg(modelName)
                    .arg(params.wienerK, 0, 'f', 4)
                    .arg(params.motionA, 0, 'f', 3)
                    .arg(params.motionB, 0, 'f', 3)
                    .arg(params.motionT, 0, 'f', 2);
            }
            result = ImageRestoration::wienerFilter(restorationInput, model, params);
        }
    }

    if (!parameterRecord.isEmpty()) {
        m_parameterRecords.append(parameterRecord);
    }
    pushProcessingResult(result, actionName);
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


