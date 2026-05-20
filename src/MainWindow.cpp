#include "MainWindow.h"

#include "BMPReader.h"
#include "FourierExperimentWidget.h"
#include "ImageHistTools.h"
#include "ImageLabProcessor.h"
#include "ImageView.h"
#include "ui_MainWindow.h"

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
#include <QLabel>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QResizeEvent>
#include <QStatusBar>
#include <QStackedWidget>
#include <QSplitter>
#include <QTabWidget>
#include <QStringList>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("医学影像分析平台"));
    resize(1280, 800);
    setMinimumSize(1050, 680);
    setAcceptDrops(true);
    createUi();
}

MainWindow::~MainWindow() = default;

void MainWindow::createUi() {
    ui = std::make_unique<Ui::MainWindow>();
    ui->setupUi(this);

    auto* workbench = new QWidget(this);
    setCentralWidget(workbench);

    m_fileList = nullptr;
    m_filterCombo = nullptr;
    setWindowTitle(QStringLiteral("医学影像分析平台"));

    setStyleSheet(R"(
        QMainWindow, QWidget {
            color: #dbe4ef;
            background: #101722;
        }
        QFrame#TopBar {
            background: #172230;
            border: 1px solid #263447;
            border-radius: 8px;
        }
        QFrame#ImagePanel {
            background: #111827;
            border: 1px solid #2a384c;
            border-radius: 8px;
        }
        QLineEdit, QSpinBox, QSlider, QDoubleSpinBox {
            color: #e5edf7;
            background: #0f172a;
            border: 1px solid #3b4b61;
            border-radius: 5px;
            min-height: 26px;
        }
        QPushButton, QToolButton {
            color: #dbeafe;
            background: #1e293b;
            border: 1px solid #334155;
            border-radius: 6px;
            padding: 7px 11px;
        }
        QPushButton:hover, QToolButton:hover {
            color: #ffffff;
            background: #2563eb;
            border-color: #60a5fa;
        }
        QPushButton:pressed, QToolButton:pressed {
            background: #1d4ed8;
            border-color: #93c5fd;
        }
        QPushButton:disabled, QToolButton:disabled {
            color: #64748b;
            background: #182233;
            border-color: #263447;
        }
        QMenu {
            color: #dbeafe;
            background: #172230;
            border: 1px solid #334155;
            padding: 5px;
        }
        QMenu::item {
            padding: 7px 22px;
            border-radius: 5px;
        }
        QMenu::item:selected {
            color: #ffffff;
            background: #2563eb;
        }
        QToolButton::menu-indicator {
            image: none;
            width: 0px;
        }
        QLabel#PanelTitle {
            color: #f8fafc;
            font-weight: 700;
        }
        QLabel#MutedText {
            color: #9fb0c6;
        }
        QLabel#HistogramPreview {
            color: #9fb0c6;
            background: #0f172a;
            border: 1px solid #263447;
            border-radius: 6px;
        }
        QDialog {
            color: #dbe4ef;
            background: #172230;
        }
        QDialog QLabel {
            color: #dbe4ef;
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

    auto makeMenuButton = [this, topBar](const QString& text, QMenu* menu) {
        auto* button = new QToolButton(topBar);
        button->setText(text);
        button->setPopupMode(QToolButton::InstantPopup);
        button->setMenu(menu);
        button->setCursor(Qt::PointingHandCursor);
        return button;
    };
    auto addAction = [this](QMenu* menu, const QString& text) {
        QAction* action = menu->addAction(text);
        connect(action, &QAction::triggered, this, [this, text]() {
            applyProcessingAction(text);
        });
    };

    auto* inputMenu = new QMenu(this);
    inputMenu->addAction(QString::fromUtf8(u8"打开影像"), this, &MainWindow::openSingleFile);
    inputMenu->addAction(QString::fromUtf8(u8"打开文件夹"), this, &MainWindow::selectFolder);
    inputMenu->addSeparator();
    inputMenu->addAction(QString::fromUtf8(u8"保存结果"), this, [this]() {
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
    for (const QString& item : {QString::fromUtf8(u8"椒盐噪声"), QString::fromUtf8(u8"脉冲噪声"),
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

    topLayout->addWidget(makeMenuButton(QString::fromUtf8(u8"影像输入"), inputMenu));
    topLayout->addWidget(makeMenuButton(QString::fromUtf8(u8"基础显示"), displayMenu));
    topLayout->addWidget(makeMenuButton(QString::fromUtf8(u8"直方图增强"), histMenu));
    topLayout->addWidget(makeMenuButton(QString::fromUtf8(u8"噪声与滤波"), noiseMenu));
    topLayout->addWidget(makeMenuButton(QString::fromUtf8(u8"边缘结构"), edgeMenu));
    topLayout->addWidget(makeMenuButton(QString::fromUtf8(u8"图像增强"), enhanceMenu));
    topLayout->addWidget(makeMenuButton(QString::fromUtf8(u8"频域分析"), frequencyMenu));
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

    auto* imageRow = new QSplitter(Qt::Horizontal, workbench);
    imageRow->setChildrenCollapsible(false);
    auto buildImagePanel = [this, imageRow](const QString& titleText, ImageView** viewPtr) {
        auto* panel = new QFrame(imageRow);
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
    buildImagePanel(QString::fromUtf8(u8"原始影像"), &m_imageView);
    buildImagePanel(QString::fromUtf8(u8"处理结果"), &m_resultView);
    imageRow->setSizes({640, 640});
    rootLayout->addWidget(imageRow, 1);

    auto* statusRow = new QHBoxLayout();
    m_infoLabel = new QLabel(QString::fromUtf8(u8"未加载影像"), workbench);
    m_infoLabel->setObjectName(QStringLiteral("MutedText"));
    m_chainLabel = new QLabel(QString::fromUtf8(u8"当前处理：无"), workbench);
    m_chainLabel->setObjectName(QStringLiteral("MutedText"));
    m_zoomLabel = new QLabel(QStringLiteral("100%"), workbench);
    m_zoomLabel->setObjectName(QStringLiteral("MutedText"));
    m_histogramLabel = new QLabel(QString::fromUtf8(u8"直方图"), workbench);
    m_histogramLabel->setObjectName(QStringLiteral("HistogramPreview"));
    m_histogramLabel->setFixedSize(180, 80);
    m_histogramLabel->setAlignment(Qt::AlignCenter);
    m_folderLabel = new QLabel(workbench);
    m_folderLabel->setVisible(false);
    m_previewHint = nullptr;
    statusRow->addWidget(m_infoLabel, 2);
    statusRow->addWidget(m_chainLabel, 3);
    statusRow->addWidget(m_histogramLabel, 0);
    statusRow->addWidget(m_zoomLabel, 0);
    rootLayout->addLayout(statusRow);

    connect(m_imageView, &ImageView::hoverPixel, this, &MainWindow::updateHoverInfo);
    connect(m_imageView, &ImageView::clickPixel, this, &MainWindow::showPixelInfo);
    connect(m_imageView, &ImageView::hoverOutside, this, &MainWindow::updateHoverOutside);
    connect(m_imageView, &ImageView::zoomRequested, this, &MainWindow::handleZoomWheel);
    connect(m_resultView, &ImageView::hoverPixel, this, &MainWindow::updateHoverInfo);
    connect(m_resultView, &ImageView::clickPixel, this, &MainWindow::showPixelInfo);
    connect(m_resultView, &ImageView::hoverOutside, this, &MainWindow::updateHoverOutside);
    connect(m_resultView, &ImageView::zoomRequested, this, &MainWindow::handleZoomWheel);
    connect(m_undoButton, &QPushButton::clicked, this, &MainWindow::undoProcessing);
    connect(m_redoButton, &QPushButton::clicked, this, &MainWindow::redoProcessing);
    connect(m_resetButton, &QPushButton::clicked, this, &MainWindow::resetProcessing);

    refreshWorkbenchImages();
    updateProcessingStatus(QString::fromUtf8(u8"请选择影像"));
}

QWidget* MainWindow::createExperimentPage() {
    auto* page = new QWidget(this);
    page->setObjectName(QStringLiteral("labExperimentPage"));

    auto* rootLayout = new QVBoxLayout(page);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(12);

    auto* titleCard = new QFrame(page);
    titleCard->setObjectName("Card");
    auto* titleLayout = new QVBoxLayout(titleCard);
    titleLayout->setContentsMargins(16, 16, 16, 16);
    titleLayout->setSpacing(4);
    auto* title = new QLabel(QString::fromUtf8(u8"功能分析"), titleCard);
    title->setObjectName("SectionTitle");
    auto* subtitle = new QLabel(QString::fromUtf8(u8"左侧选择功能模块，右侧加载对应处理界面"), titleCard);
    subtitle->setObjectName("SmallLabel");
    titleLayout->addWidget(title);
    titleLayout->addWidget(subtitle);
    rootLayout->addWidget(titleCard);
    titleCard->setVisible(false);
    titleCard->setMaximumHeight(0);

    auto* themeCard = new QFrame(page);
    themeCard->setObjectName("Card");
    auto* themeLayout = new QHBoxLayout(themeCard);
    themeLayout->setContentsMargins(16, 10, 16, 10);
    themeLayout->setSpacing(10);
    auto makeThemeBadge = [themeCard](const QString& text) {
        auto* badge = new QLabel(text, themeCard);
        badge->setObjectName(QStringLiteral("ThemeBadge"));
        return badge;
    };
    themeLayout->addWidget(makeThemeBadge(QStringLiteral("噪声抑制")));
    themeLayout->addWidget(makeThemeBadge(QStringLiteral("边缘分析")));
    themeLayout->addWidget(makeThemeBadge(QStringLiteral("图像增强")));
    themeLayout->addStretch(1);

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
    auto* leftTitle = new QLabel(QString::fromUtf8(u8"功能模块"), leftPane);
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
    auto* rightTitle = new QLabel(QString::fromUtf8(u8"处理工作区"), rightPane);
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

#if 0
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
    m_enhancementStepButton = new QPushButton(QString::fromUtf8(u8"分步预览"), controlCard);
    m_enhancementResetButton = new QPushButton(QString::fromUtf8(u8"重置"), controlCard);

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
    auto* previewLayout = new QGridLayout(previewCard);
    previewLayout->setContentsMargins(16, 16, 16, 16);
    previewLayout->setHorizontalSpacing(10);
    previewLayout->setVerticalSpacing(10);

    m_enhancementStepLabels.clear();
    m_enhancementStepLabels.reserve(8);

    for (int i = 0; i < 8; ++i) {
        auto* frame = new QFrame(previewCard);
        frame->setObjectName("Card");
        auto* frameLayout = new QVBoxLayout(frame);
        frameLayout->setContentsMargins(10, 10, 10, 10);
        frameLayout->setSpacing(6);

        auto* title = new QLabel(QString::number(i + 1) + QString::fromUtf8(u8" 步"), frame);
        title->setObjectName("SectionTitle");
        auto* image = new QLabel(QString::fromUtf8(u8"等待处理"), frame);
        image->setAlignment(Qt::AlignCenter);
        image->setMinimumSize(220, 170);
        image->setObjectName("ImageView");
        image->setWordWrap(true);
        frameLayout->addWidget(title);
        frameLayout->addWidget(image, 1);
        m_enhancementStepLabels.push_back(image);
        previewLayout->addWidget(frame, i / 4, i % 4);
    }

    for (int col = 0; col < 4; ++col) {
        previewLayout->setColumnStretch(col, 1);
    }

    auto* infoCard = new QFrame(page);
    infoCard->setObjectName("Card");
    auto* infoLayout = new QVBoxLayout(infoCard);
    infoLayout->setContentsMargins(16, 14, 16, 14);
    m_enhancementInfoLabel = new QLabel(QString::fromUtf8(u8"处理提示：请选择图像并开始增强。"), infoCard);
    m_enhancementInfoLabel->setObjectName("SmallLabel");
    m_enhancementInfoLabel->setWordWrap(true);
    infoLayout->addWidget(m_enhancementInfoLabel);

    rootLayout->addWidget(controlCard);
    rootLayout->addWidget(previewCard, 1);
    rootLayout->addWidget(infoCard);
    infoCard->setVisible(false);
    infoCard->setMaximumHeight(0);

    auto* detailCard = new QFrame(page);
    detailCard->setObjectName("Card");
    auto* detailLayout = new QVBoxLayout(detailCard);
    detailLayout->setContentsMargins(16, 14, 16, 14);
    detailLayout->setSpacing(8);
    auto* detailTop = new QHBoxLayout();
    detailTop->setContentsMargins(0, 0, 0, 0);
    detailTop->setSpacing(8);
    m_enhancementDetailBadgeLabel = new QLabel(QString::fromUtf8(u8"第1步"), detailCard);
    m_enhancementDetailBadgeLabel->setObjectName(QStringLiteral("ThemeBadge"));
    detailTop->addWidget(m_enhancementDetailBadgeLabel, 0, Qt::AlignLeft);
    m_enhancementInfoLabel = new QLabel(QString::fromUtf8(u8"处理提示：请选择图像并开始增强。"), detailCard);
    m_enhancementInfoLabel->setObjectName("SmallLabel");
    m_enhancementInfoLabel->setWordWrap(true);
    detailTop->addWidget(m_enhancementInfoLabel, 1);
    m_enhancementDetailImageLabel = new QLabel(QString::fromUtf8(u8"等待加载图像"), detailCard);
    m_enhancementDetailImageLabel->setAlignment(Qt::AlignCenter);
    m_enhancementDetailImageLabel->setMinimumSize(640, 360);
    m_enhancementDetailImageLabel->setObjectName("ImageView");
    m_enhancementDetailImageLabel->setWordWrap(true);
    detailLayout->addLayout(detailTop);
    detailLayout->addWidget(m_enhancementDetailImageLabel, 1);
    rootLayout->addWidget(detailCard);

    connect(m_enhancementLoadButton, &QPushButton::clicked, this, &MainWindow::loadEnhancementExperimentImage);
    connect(m_enhancementStartButton, &QPushButton::clicked, this, &MainWindow::startEnhancementExperiment);
    connect(m_enhancementStepButton, &QPushButton::clicked, this, &MainWindow::stepEnhancementExperiment);
    connect(m_enhancementResetButton, &QPushButton::clicked, this, &MainWindow::resetEnhancementExperiment);

    updateEnhancementExperimentPreview();
    return page;
}

#endif

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
    m_enhancementInfoLabel = new QLabel(QString::fromUtf8(u8"处理提示：请选择图像并开始增强。"), infoCard);
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
        QMessageBox::critical(this, QString::fromUtf8(u8"保存失败"), QString::fromUtf8(u8"图像保存失败，请检查文件路径或格式。"));
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

#if 0
void MainWindow::updateEnhancementExperimentPreview() {
    const QVector<QImage> images = {
        m_enhancementStep1Image,
        m_enhancementStep2Image,
        m_enhancementStep3Image,
        m_enhancementStep4Image,
        m_enhancementStep5Image,
        m_enhancementStep6Image,
        m_enhancementStep7Image,
        m_enhancementStep8Image
    };

    for (int i = 0; i < m_enhancementStepLabels.size() && i < images.size(); ++i) {
        updateExperimentImageLabel(m_enhancementStepLabels[i], images[i]);
    }

    const int previewIndex = std::clamp(m_enhancementPreviewStep - 1, 0, 7);
    const QVector<QImage> previewImages = {
        m_enhancementStep1Image,
        m_enhancementStep2Image,
        m_enhancementStep3Image,
        m_enhancementStep4Image,
        m_enhancementStep5Image,
        m_enhancementStep6Image,
        m_enhancementStep7Image,
        m_enhancementStep8Image
    };
    const QVector<QString> previewNotes = {
        QString::fromUtf8(u8"步骤1：原图像"),
        QString::fromUtf8(u8"步骤2：拉普拉斯处理"),
        QString::fromUtf8(u8"步骤3：锐化图像"),
        QString::fromUtf8(u8"步骤4：Sobel 梯度"),
        QString::fromUtf8(u8"步骤5：均值滤波梯度"),
        QString::fromUtf8(u8"步骤6：掩膜"),
        QString::fromUtf8(u8"步骤7：原图与掩膜相加"),
        QString::fromUtf8(u8"步骤8：伽马变换")
    };
    updateEnhancementDetailPreview(previewIndex + 1, previewImages[previewIndex], previewNotes[previewIndex]);
}

void MainWindow::updateEnhancementDetailPreview(int stepIndex, const QImage& image, const QString& description) {
    if (m_enhancementDetailBadgeLabel) {
        m_enhancementDetailBadgeLabel->setText(QString::fromUtf8(u8"第%1步").arg(stepIndex <= 0 ? 1 : stepIndex));
    }
    if (m_enhancementInfoLabel) {
        m_enhancementInfoLabel->setText(description);
    }
    if (m_enhancementDetailImageLabel) {
        if (image.isNull()) {
            m_enhancementDetailImageLabel->setPixmap(QPixmap());
            m_enhancementDetailImageLabel->setText(QString::fromUtf8(u8"等待加载图像"));
        } else {
            QSize target = m_enhancementDetailImageLabel->size();
            if (target.width() < 2 || target.height() < 2) {
                target = m_enhancementDetailImageLabel->minimumSize();
            }
            m_enhancementDetailImageLabel->setText(QString());
            m_enhancementDetailImageLabel->setPixmap(QPixmap::fromImage(image).scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
}

#endif

QImage MainWindow::loadGrayscaleImageFromFile(const QString& filePath, QString* errorMessage) const {
    QFileInfo info(filePath);
    const QString suffix = info.suffix().toLower();

    if (suffix == QStringLiteral("bmp")) {
        BMPReader reader(filePath);
        if (!reader.load(errorMessage)) {
            return {};
        }
        QImage image = reader.decode(errorMessage);
        return image.convertToFormat(QImage::Format_Grayscale8);
    }

    QImage image(filePath);
    if (image.isNull()) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(u8"无法加载图像文件");
        }
        return {};
    }
    return image.convertToFormat(QImage::Format_Grayscale8);
}

QImage MainWindow::addSaltPepperNoise(const QImage& img, double density) const {
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    QImage out = gray.copy();
    density = std::clamp(density, 0.0, 1.0);
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    for (int y = 0; y < out.height(); ++y) {
        uchar* line = out.scanLine(y);
        for (int x = 0; x < out.width(); ++x) {
            if (dist(rng) < density) {
                line[x] = dist(rng) < 0.5 ? 0 : 255;
            }
        }
    }
    return out;
}

QImage MainWindow::addImpulseNoise(const QImage& img, double density) const {
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    QImage out = gray.copy();
    density = std::clamp(density, 0.0, 1.0);
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    std::uniform_int_distribution<int> impulse(0, 255);
    for (int y = 0; y < out.height(); ++y) {
        uchar* line = out.scanLine(y);
        for (int x = 0; x < out.width(); ++x) {
            if (dist(rng) < density) {
                line[x] = static_cast<uchar>(impulse(rng));
            }
        }
    }
    return out;
}

QImage MainWindow::meanFilter(const QImage& img, int kernelSize) const {
    kernelSize = std::max(3, kernelSize | 1);
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }
    QImage out(gray.size(), QImage::Format_Grayscale8);
    const int radius = kernelSize / 2;
    const int area = kernelSize * kernelSize;
    for (int y = 0; y < gray.height(); ++y) {
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            int sum = 0;
            for (int ky = -radius; ky <= radius; ++ky) {
                for (int kx = -radius; kx <= radius; ++kx) {
                    sum += qGray(gray.pixel(std::clamp(x + kx, 0, gray.width() - 1), std::clamp(y + ky, 0, gray.height() - 1)));
                }
            }
            dst[x] = static_cast<uchar>(clampToByte(sum / area));
        }
    }
    return out;
}

QImage MainWindow::medianFilter(const QImage& img, int kernelSize) const {
    kernelSize = std::max(3, kernelSize | 1);
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }
    QImage out(gray.size(), QImage::Format_Grayscale8);
    const int radius = kernelSize / 2;
    std::vector<int> window;
    window.reserve(kernelSize * kernelSize);
    for (int y = 0; y < gray.height(); ++y) {
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            window.clear();
            for (int ky = -radius; ky <= radius; ++ky) {
                for (int kx = -radius; kx <= radius; ++kx) {
                    window.push_back(qGray(gray.pixel(std::clamp(x + kx, 0, gray.width() - 1), std::clamp(y + ky, 0, gray.height() - 1))));
                }
            }
            auto mid = window.begin() + static_cast<std::ptrdiff_t>(window.size() / 2);
            std::nth_element(window.begin(), mid, window.end());
            dst[x] = static_cast<uchar>(*mid);
        }
    }
    return out;
}

QImage MainWindow::maxFilter(const QImage& img, int kernelSize) const {
    kernelSize = std::max(3, kernelSize | 1);
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }
    QImage out(gray.size(), QImage::Format_Grayscale8);
    const int radius = kernelSize / 2;
    for (int y = 0; y < gray.height(); ++y) {
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            int best = 0;
            for (int ky = -radius; ky <= radius; ++ky) {
                for (int kx = -radius; kx <= radius; ++kx) {
                    best = std::max(best, qGray(gray.pixel(std::clamp(x + kx, 0, gray.width() - 1), std::clamp(y + ky, 0, gray.height() - 1))));
                }
            }
            dst[x] = static_cast<uchar>(best);
        }
    }
    return out;
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
        m_noiseNoisyImage = ImageLabProcessor::addSaltPepperNoise(m_noiseSourceImage, density);
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
    auto* noisyButton = chooser.addButton(QString::fromUtf8(u8"保存加噪图"), QMessageBox::AcceptRole);
    auto* denoisedButton = chooser.addButton(QString::fromUtf8(u8"保存去噪图"), QMessageBox::AcceptRole);
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
            m_noiseNoisyImage = ImageLabProcessor::addSaltPepperNoise(m_noiseSourceImage, density);
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
        m_experimentStatusLabel->setText(QString::fromUtf8(u8"状态：已加载边缘检测输入图像"));
    }
    statusBar()->showMessage(QString::fromUtf8(u8"边缘检测输入图像已加载：") + QFileInfo(filePath).fileName());
}

QImage MainWindow::sobelEdgeDetect(const QImage& img, int kernelSize, int threshold) const {
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }

    const int size = kernelSize >= 5 ? 5 : 3;
    const QVector<int> deriv = (size == 5)
        ? QVector<int>{-1, -2, 0, 2, 1}
        : QVector<int>{-1, 0, 1};
    const QVector<int> smooth = (size == 5)
        ? QVector<int>{1, 4, 6, 4, 1}
        : QVector<int>{1, 2, 1};
    const double scale = (size == 5) ? 48.0 : 4.0;
    const int radius = size / 2;
    threshold = std::clamp(threshold, 0, 255);

    QImage out(gray.size(), QImage::Format_Grayscale8);
    out.fill(Qt::black);
    auto sample = [&](int x, int y) {
        x = std::clamp(x, 0, gray.width() - 1);
        y = std::clamp(y, 0, gray.height() - 1);
        return qGray(gray.pixel(x, y));
    };

    for (int y = 0; y < gray.height(); ++y) {
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            double gx = 0.0;
            double gy = 0.0;
            for (int ky = -radius; ky <= radius; ++ky) {
                for (int kx = -radius; kx <= radius; ++kx) {
                    const int value = sample(x + kx, y + ky);
                    const int sx = deriv[kx + radius] * smooth[ky + radius];
                    const int sy = smooth[kx + radius] * deriv[ky + radius];
                    gx += value * sx;
                    gy += value * sy;
                }
            }
            const double magnitude = std::sqrt(gx * gx + gy * gy) / scale;
            dst[x] = static_cast<uchar>(clampToByte(static_cast<int>(magnitude)) >= threshold ? 255 : 0);
        }
    }
    return out;
}

QImage MainWindow::prewittEdgeDetect(const QImage& img, int kernelSize, int threshold) const {
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }

    const int size = kernelSize >= 5 ? 5 : 3;
    const QVector<int> deriv = (size == 5)
        ? QVector<int>{-2, -1, 0, 1, 2}
        : QVector<int>{-1, 0, 1};
    const QVector<int> smooth = QVector<int>(size, 1);
    const double scale = (size == 5) ? 25.0 : 3.0;
    const int radius = size / 2;
    threshold = std::clamp(threshold, 0, 255);

    QImage out(gray.size(), QImage::Format_Grayscale8);
    out.fill(Qt::black);
    auto sample = [&](int x, int y) {
        x = std::clamp(x, 0, gray.width() - 1);
        y = std::clamp(y, 0, gray.height() - 1);
        return qGray(gray.pixel(x, y));
    };

    for (int y = 0; y < gray.height(); ++y) {
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            double gx = 0.0;
            double gy = 0.0;
            for (int ky = -radius; ky <= radius; ++ky) {
                for (int kx = -radius; kx <= radius; ++kx) {
                    const int value = sample(x + kx, y + ky);
                    const int sx = deriv[kx + radius] * smooth[ky + radius];
                    const int sy = smooth[kx + radius] * deriv[ky + radius];
                    gx += value * sx;
                    gy += value * sy;
                }
            }
            const double magnitude = std::sqrt(gx * gx + gy * gy) / scale;
            dst[x] = static_cast<uchar>(clampToByte(static_cast<int>(magnitude)) >= threshold ? 255 : 0);
        }
    }
    return out;
}

QImage MainWindow::laplacianEdgeDetect(const QImage& img, int kernelSize, int threshold) const {
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }

    threshold = std::clamp(threshold, 0, 255);
    QImage out(gray.size(), QImage::Format_Grayscale8);
    out.fill(Qt::black);
    auto sample = [&](int x, int y) {
        x = std::clamp(x, 0, gray.width() - 1);
        y = std::clamp(y, 0, gray.height() - 1);
        return qGray(gray.pixel(x, y));
    };

    if (kernelSize >= 5) {
        const int kernel[5][5] = {
            {0, 0, -1, 0, 0},
            {0, -1, -2, -1, 0},
            {-1, -2, 16, -2, -1},
            {0, -1, -2, -1, 0},
            {0, 0, -1, 0, 0}
        };
        const int radius = 2;
        const double scale = 16.0;
        for (int y = 0; y < gray.height(); ++y) {
            uchar* dst = out.scanLine(y);
            for (int x = 0; x < gray.width(); ++x) {
                double sum = 0.0;
                for (int ky = -radius; ky <= radius; ++ky) {
                    for (int kx = -radius; kx <= radius; ++kx) {
                        sum += sample(x + kx, y + ky) * kernel[ky + radius][kx + radius];
                    }
                }
                const int intensity = clampToByte(static_cast<int>(std::abs(sum) / scale));
                dst[x] = static_cast<uchar>(intensity >= threshold ? 255 : 0);
            }
        }
        return out;
    }

    const int kernel[3][3] = {
        {0, -1, 0},
        {-1, 4, -1},
        {0, -1, 0}
    };
    const int radius = 1;
    const double scale = 4.0;
    for (int y = 0; y < gray.height(); ++y) {
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            double sum = 0.0;
            for (int ky = -radius; ky <= radius; ++ky) {
                for (int kx = -radius; kx <= radius; ++kx) {
                    sum += sample(x + kx, y + ky) * kernel[ky + radius][kx + radius];
                }
            }
            const int intensity = clampToByte(static_cast<int>(std::abs(sum) / scale));
            dst[x] = static_cast<uchar>(intensity >= threshold ? 255 : 0);
        }
    }
    return out;
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
    m_enhancementSourceImage = image;
    resetEnhancementExperiment();
    m_enhancementSourceImage = image;
    m_enhancementStep1Image = ImageLabProcessor::step1_originalImage(image);
    m_enhancementPreviewStep = 1;
    if (m_enhancementInputPathEdit) {
        m_enhancementInputPathEdit->setText(filePath);
    }
    updateEnhancementExperimentPreview();
    if (m_enhancementInfoLabel) {
        m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：图像已加载，可以开始执行增强流程。"));
    }
    if (m_experimentStatusLabel) {
        m_experimentStatusLabel->setText(QString::fromUtf8(u8"状态：已加载图像增强输入图像"));
    }
    statusBar()->showMessage(QString::fromUtf8(u8"图像增强输入图像已加载：") + QFileInfo(filePath).fileName());
}

QImage MainWindow::step1_originalImage(const QImage& img) const {
    return img.convertToFormat(QImage::Format_Grayscale8);
}

QImage MainWindow::step2_laplacianProcess(const QImage& img) const {
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }

    QImage out(gray.size(), QImage::Format_Grayscale8);
    out.fill(Qt::black);
    const int kernel[3][3] = {
        {0, -1, 0},
        {-1, 4, -1},
        {0, -1, 0}
    };
    for (int y = 0; y < gray.height(); ++y) {
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            int sum = 0;
            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    const int sx = std::clamp(x + kx, 0, gray.width() - 1);
                    const int sy = std::clamp(y + ky, 0, gray.height() - 1);
                    sum += qGray(gray.pixel(sx, sy)) * kernel[ky + 1][kx + 1];
                }
            }
            dst[x] = static_cast<uchar>(clampToByte(std::abs(sum)));
        }
    }
    return out;
}

QImage MainWindow::step3_sharpenImage(const QImage& original, const QImage& laplacian) const {
    QImage src = original.convertToFormat(QImage::Format_Grayscale8);
    QImage lap = laplacian.convertToFormat(QImage::Format_Grayscale8);
    if (src.isNull() || lap.isNull() || src.size() != lap.size()) {
        return {};
    }

    QImage out(src.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < src.height(); ++y) {
        uchar* dst = out.scanLine(y);
        const uchar* sLine = src.constScanLine(y);
        const uchar* lLine = lap.constScanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            dst[x] = static_cast<uchar>(clampToByte(sLine[x] + lLine[x]));
        }
    }
    return out;
}

QImage MainWindow::step4_sobelProcess(const QImage& img) const {
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }

    QImage out(gray.size(), QImage::Format_Grayscale8);
    out.fill(Qt::black);
    for (int y = 0; y < gray.height(); ++y) {
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            int gx = 0;
            int gy = 0;
            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    const int sx = std::clamp(x + kx, 0, gray.width() - 1);
                    const int sy = std::clamp(y + ky, 0, gray.height() - 1);
                    const int p = qGray(gray.pixel(sx, sy));
                    const int sxKernel[3][3] = {
                        {-1, 0, 1},
                        {-2, 0, 2},
                        {-1, 0, 1}
                    };
                    const int syKernel[3][3] = {
                        {1, 2, 1},
                        {0, 0, 0},
                        {-1, -2, -1}
                    };
                    gx += p * sxKernel[ky + 1][kx + 1];
                    gy += p * syKernel[ky + 1][kx + 1];
                }
            }
            const int magnitude = clampToByte(static_cast<int>(std::sqrt(static_cast<double>(gx * gx + gy * gy)) / 4.0));
            dst[x] = static_cast<uchar>(magnitude);
        }
    }
    return out;
}

QImage MainWindow::step5_meanFilterGradient(const QImage& sobel) const {
    return meanFilter(sobel, 3);
}

QImage MainWindow::step6_maskImage(const QImage& sharpened, const QImage& gradient) const {
    QImage a = sharpened.convertToFormat(QImage::Format_Grayscale8);
    QImage b = gradient.convertToFormat(QImage::Format_Grayscale8);
    if (a.isNull() || b.isNull() || a.size() != b.size()) {
        return {};
    }

    QImage out(a.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < a.height(); ++y) {
        uchar* dst = out.scanLine(y);
        const uchar* aLine = a.constScanLine(y);
        const uchar* bLine = b.constScanLine(y);
        for (int x = 0; x < a.width(); ++x) {
            dst[x] = static_cast<uchar>((aLine[x] * bLine[x]) / 255);
        }
    }
    return out;
}

QImage MainWindow::step7_addOriginalAndMask(const QImage& original, const QImage& mask) const {
    QImage a = original.convertToFormat(QImage::Format_Grayscale8);
    QImage b = mask.convertToFormat(QImage::Format_Grayscale8);
    if (a.isNull() || b.isNull() || a.size() != b.size()) {
        return {};
    }

    QImage out(a.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < a.height(); ++y) {
        uchar* dst = out.scanLine(y);
        const uchar* aLine = a.constScanLine(y);
        const uchar* bLine = b.constScanLine(y);
        for (int x = 0; x < a.width(); ++x) {
            dst[x] = static_cast<uchar>(clampToByte(aLine[x] + bLine[x]));
        }
    }
    return out;
}

QImage MainWindow::step8_gammaTransform(const QImage& enhanced, double gamma) const {
    QImage gray = enhanced.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }

    gamma = std::max(0.1, gamma);
    QImage out(gray.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < gray.height(); ++y) {
        uchar* dst = out.scanLine(y);
        const uchar* line = gray.constScanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            const double normalized = line[x] / 255.0;
            const int value = static_cast<int>(std::pow(normalized, gamma) * 255.0 + 0.5);
            dst[x] = static_cast<uchar>(clampToByte(value));
        }
    }
    return out;
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
        m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：已完成 8 步图像增强流程，可点击任意步骤查看对应中间结果。"));
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
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：全部步骤已完成，可重新开始或重置。"));
        }
        statusBar()->showMessage(QString::fromUtf8(u8"图像增强步骤已全部完成"));
        return;
    }

    switch (m_enhancementPreviewStep) {
    case 0:
        m_enhancementStep1Image = ImageLabProcessor::step1_originalImage(m_enhancementSourceImage);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：步骤1，显示并缓存原图像。"));
        }
        break;
    case 1:
        m_enhancementStep2Image = ImageLabProcessor::step2_laplacianProcess(m_enhancementStep1Image);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：步骤2，对原图像进行拉普拉斯处理，提取高频细节。"));
        }
        break;
    case 2:
        m_enhancementStep3Image = ImageLabProcessor::step3_sharpenImage(m_enhancementStep1Image, m_enhancementStep2Image);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：步骤3，将原图像与拉普拉斯结果相加，形成锐化图像。"));
        }
        break;
    case 3:
        m_enhancementStep4Image = ImageLabProcessor::step4_sobelProcess(m_enhancementStep1Image);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：步骤4，对原图像应用 Sobel 算子，提取梯度信息。"));
        }
        break;
    case 4:
        m_enhancementStep5Image = ImageLabProcessor::step5_meanFilterGradient(m_enhancementStep4Image);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：步骤5，对 Sobel 结果进行均值滤波，得到平滑梯度图像。"));
        }
        break;
    case 5:
        m_enhancementStep6Image = ImageLabProcessor::step6_maskImage(m_enhancementStep3Image, m_enhancementStep5Image);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：步骤6，将锐化图像与平滑梯度图像相乘，生成掩膜。"));
        }
        break;
    case 6:
        m_enhancementStep7Image = ImageLabProcessor::step7_addOriginalAndMask(m_enhancementStep1Image, m_enhancementStep6Image);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：步骤7，将原图像与掩膜求和，得到增强图像。"));
        }
        break;
    case 7:
        m_enhancementStep8Image = ImageLabProcessor::step8_gammaTransform(m_enhancementStep7Image);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：步骤8，对增强图像进行伽马变换，优化对比度。"));
        }
        break;
    default:
        break;
    }

    ++m_enhancementPreviewStep;
    updateEnhancementExperimentPreview();
    if (m_experimentStatusLabel) {
        m_experimentStatusLabel->setText(QString::fromUtf8(u8"状态：已执行增强步骤 ") + QString::number(m_enhancementPreviewStep));
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
        m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：已重置，请重新加载图像。"));
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
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：图像增强流程已完成，左侧显示原图，右侧显示最终结果。"));
        } else if (!m_enhancementStep1Image.isNull()) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：已载入输入图像，可点击“开始增强”或“分布预览”。"));
        } else {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"处理提示：请选择图像并开始增强。"));
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
    auto* prevButton = new QPushButton(QString::fromUtf8(u8"上一页"), &dialog);
    auto* nextButton = new QPushButton(QString::fromUtf8(u8"下一页"), &dialog);
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
        QString::fromUtf8(u8"均值滤波梯度"),
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

        displayImage(filePath);
        event->acceptProposedAction();
        return;
    }

    event->ignore();
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
    QString folder = QFileDialog::getExistingDirectory(this, QStringLiteral("请选择图像文件夹"));
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
            {"*.bmp", "*.BMP", "*.jpg", "*.JPG", "*.jpeg", "*.JPEG"},
            QDir::Files,
            QDir::Name
        );
        if (!files.isEmpty()) {
            displayImage(dir.filePath(files.first()));
        }
    }
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

    displayImage(filePath);
}

void MainWindow::loadImageFiles(const QString& folder) {
    if (m_fileList) {
        m_fileList->clear();
    }

    QDir dir(folder);
    QStringList files = dir.entryList(
        {"*.bmp", "*.BMP", "*.jpg", "*.JPG", "*.jpeg", "*.JPEG"},
        QDir::Files,
        QDir::Name
    );

    for (const auto& f : files) {
        if (m_fileList) {
            m_fileList->addItem(f);
        }
    }

    statusBar()->showMessage(QStringLiteral("状态：在文件夹中找到 %1 个支持的图像文件").arg(files.size()));
    if (files.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("该文件夹中没有 BMP / JPG / JPEG 文件"));
        return;
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
    m_fitOnNextRefresh = true;
    m_resultHistory.clear();
    m_chainHistory.clear();
    m_processingChain.clear();
    m_historyIndex = -1;
    m_frequencyIfftSource = {};

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

    refreshWorkbenchImages();
    updateProcessingStatus(QString::fromUtf8(u8"影像已加载"));
    statusBar()->showMessage(QStringLiteral("状态：已加载 %1").arg(QFileInfo(filePath).fileName()));
}

void MainWindow::renderCurrentImage() {
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
        : m_processingChain.join(QString::fromUtf8(u8" → "));

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
    QRgb filteredRgba = rgba;
    const QString processText = m_processingChain.isEmpty()
        ? QString::fromUtf8(u8"无")
        : m_processingChain.join(QString::fromUtf8(u8" → "));

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

    m_processingChain.append(actionName);
    m_resultHistory.append(image);
    m_chainHistory.append(m_processingChain);
    m_historyIndex = m_resultHistory.size() - 1;
    m_filteredImage = image;
    refreshWorkbenchImages();
    updateProcessingStatus(QString::fromUtf8(u8"当前结果已更新"));
}

void MainWindow::refreshWorkbenchImages() {
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
    if (m_chainLabel) {
        const QString chain = m_processingChain.isEmpty()
            ? QString::fromUtf8(u8"当前处理：无")
            : QString::fromUtf8(u8"当前处理：") + m_processingChain.join(QString::fromUtf8(u8" → "));
        m_chainLabel->setText(chain);
    }
    if (m_undoButton) {
        m_undoButton->setEnabled(m_historyIndex >= 0);
    }
    if (m_redoButton) {
        m_redoButton->setEnabled(m_historyIndex + 1 < m_resultHistory.size());
    }
    if (m_resetButton) {
        m_resetButton->setEnabled(!m_resultHistory.isEmpty());
    }
    if (!message.isEmpty()) {
        statusBar()->showMessage(message);
    }
}

void MainWindow::undoProcessing() {
    if (m_historyIndex < 0) {
        return;
    }
    --m_historyIndex;
    m_processingChain = m_historyIndex >= 0 ? m_chainHistory.value(m_historyIndex) : QStringList();
    refreshWorkbenchImages();
    updateProcessingStatus(QString::fromUtf8(u8"已撤回上一步处理"));
}

void MainWindow::redoProcessing() {
    if (m_historyIndex + 1 >= m_resultHistory.size()) {
        return;
    }
    ++m_historyIndex;
    m_processingChain = m_chainHistory.value(m_historyIndex);
    refreshWorkbenchImages();
    updateProcessingStatus(QString::fromUtf8(u8"已前进到下一步处理"));
}

void MainWindow::resetProcessing() {
    m_resultHistory.clear();
    m_chainHistory.clear();
    m_processingChain.clear();
    m_historyIndex = -1;
    m_filteredImage = {};
    m_frequencyIfftSource = {};
    refreshWorkbenchImages();
    updateProcessingStatus(QString::fromUtf8(u8"处理链已重置"));
}

void MainWindow::applyProcessingAction(const QString& actionName) {
    if (m_originalImage.isNull()) {
        QMessageBox::information(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先加载影像"));
        return;
    }

    auto askNoiseDensity = [this](double* density) {
        QDialog dialog(this);
        dialog.setWindowTitle(QString::fromUtf8(u8"噪声参数"));
        auto* layout = new QGridLayout(&dialog);
        auto* slider = new QSlider(Qt::Horizontal, &dialog);
        auto* value = new QLabel(QStringLiteral("8%"), &dialog);
        auto* apply = new QPushButton(QString::fromUtf8(u8"应用"), &dialog);
        slider->setRange(0, 100);
        slider->setValue(8);
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
        return true;
    };

    auto askKernel = [this](int* kernelSize) {
        QDialog dialog(this);
        dialog.setWindowTitle(QString::fromUtf8(u8"滤波参数"));
        auto* layout = new QGridLayout(&dialog);
        auto* spin = new QSpinBox(&dialog);
        auto* apply = new QPushButton(QString::fromUtf8(u8"应用"), &dialog);
        spin->setRange(3, 15);
        spin->setSingleStep(2);
        spin->setValue(3);
        layout->addWidget(new QLabel(QString::fromUtf8(u8"核大小"), &dialog), 0, 0);
        layout->addWidget(spin, 0, 1);
        layout->addWidget(apply, 1, 1);
        connect(apply, &QPushButton::clicked, &dialog, &QDialog::accept);
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }
        *kernelSize = spin->value();
        return true;
    };

    auto askEdgeParams = [this](int* kernelSize, int* threshold) {
        QDialog dialog(this);
        dialog.setWindowTitle(QString::fromUtf8(u8"边缘参数"));
        auto* layout = new QGridLayout(&dialog);
        auto* spin = new QSpinBox(&dialog);
        auto* slider = new QSlider(Qt::Horizontal, &dialog);
        auto* value = new QLabel(QStringLiteral("80"), &dialog);
        auto* apply = new QPushButton(QString::fromUtf8(u8"应用"), &dialog);
        spin->setRange(3, 5);
        spin->setSingleStep(2);
        spin->setValue(3);
        slider->setRange(0, 255);
        slider->setValue(80);
        layout->addWidget(new QLabel(QString::fromUtf8(u8"核大小"), &dialog), 0, 0);
        layout->addWidget(spin, 0, 1);
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
        *kernelSize = spin->value();
        *threshold = slider->value();
        return true;
    };

    auto askGamma = [this](double* gamma) {
        QDialog dialog(this);
        dialog.setWindowTitle(QString::fromUtf8(u8"Gamma 参数"));
        auto* layout = new QGridLayout(&dialog);
        auto* spin = new QDoubleSpinBox(&dialog);
        auto* apply = new QPushButton(QString::fromUtf8(u8"应用"), &dialog);
        spin->setRange(0.1, 5.0);
        spin->setSingleStep(0.1);
        spin->setValue(0.8);
        layout->addWidget(new QLabel(QStringLiteral("Gamma"), &dialog), 0, 0);
        layout->addWidget(spin, 0, 1);
        layout->addWidget(apply, 1, 1);
        connect(apply, &QPushButton::clicked, &dialog, &QDialog::accept);
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }
        *gamma = spin->value();
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
        auto addSpinRow = [&](const QString& labelText, QSpinBox* spin) {
            spin->setMinimumWidth(190);
            formLayout->addWidget(new QLabel(labelText, &dialog), row, 0);
            formLayout->addWidget(spin, row, 1);
            ++row;
        };
        auto addDoubleRow = [&](const QString& labelText, QDoubleSpinBox* spin) {
            spin->setMinimumWidth(190);
            formLayout->addWidget(new QLabel(labelText, &dialog), row, 0);
            formLayout->addWidget(spin, row, 1);
            ++row;
        };

        auto* cutoffSpin = new QSpinBox(&dialog);
        cutoffSpin->setRange(1, 4096);
        cutoffSpin->setValue(static_cast<int>(*cutoff));
        addSpinRow(QString::fromUtf8(u8"截止半径"), cutoffSpin);

        QSpinBox* orderSpin = nullptr;
        if (needsOrder) {
            orderSpin = new QSpinBox(&dialog);
            orderSpin->setRange(1, 10);
            orderSpin->setValue(*order);
            addSpinRow(QString::fromUtf8(u8"阶数"), orderSpin);
        }

        QDoubleSpinBox* gammaLowSpin = nullptr;
        QDoubleSpinBox* gammaHighSpin = nullptr;
        QDoubleSpinBox* cSpin = nullptr;
        if (needsHomomorphic) {
            gammaLowSpin = new QDoubleSpinBox(&dialog);
            gammaLowSpin->setRange(0.01, 5.0);
            gammaLowSpin->setSingleStep(0.05);
            gammaLowSpin->setValue(*gammaLow);
            addDoubleRow(QStringLiteral("γL"), gammaLowSpin);

            gammaHighSpin = new QDoubleSpinBox(&dialog);
            gammaHighSpin->setRange(0.01, 5.0);
            gammaHighSpin->setSingleStep(0.05);
            gammaHighSpin->setValue(*gammaHigh);
            addDoubleRow(QStringLiteral("γH"), gammaHighSpin);

            cSpin = new QDoubleSpinBox(&dialog);
            cSpin->setRange(0.01, 10.0);
            cSpin->setSingleStep(0.1);
            cSpin->setValue(*c);
            addDoubleRow(QStringLiteral("c"), cSpin);
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

        *cutoff = cutoffSpin->value();
        if (orderSpin) {
            *order = orderSpin->value();
        }
        if (gammaLowSpin) {
            *gammaLow = gammaLowSpin->value();
        }
        if (gammaHighSpin) {
            *gammaHigh = gammaHighSpin->value();
        }
        if (cSpin) {
            *c = cSpin->value();
        }
        return true;
    };

    QImage input = currentProcessingInput();
    QImage result;

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
        result = actionName == QString::fromUtf8(u8"椒盐噪声")
            ? ImageLabProcessor::addSaltPepperNoise(input, density)
            : ImageLabProcessor::addImpulseNoise(input, density);
    } else if (actionName == QString::fromUtf8(u8"均值滤波") ||
               actionName == QString::fromUtf8(u8"中值滤波") ||
               actionName == QString::fromUtf8(u8"最大值滤波")) {
        int kernelSize = 3;
        if (!askKernel(&kernelSize)) {
            return;
        }
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
