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
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

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
    setWindowTitle(QStringLiteral("医学影像处理软件"));

    setStyleSheet(R"(
        QMainWindow, QWidget {
            color: #162033;
            background: #f5f8ff;
        }
        QTabWidget::pane {
            border: 1px solid #d7e3f5;
            border-radius: 16px;
            background: #ffffff;
        }
        QTabBar::tab {
            color: #0f172a;
            background: #e8effc;
            border: 1px solid #d7e3f5;
            border-bottom: none;
            border-top-left-radius: 11px;
            border-top-right-radius: 11px;
            padding: 9px 16px;
            margin-right: 4px;
        }
        QTabBar::tab:selected {
            color: #0f172a;
            background: #ffffff;
        }
        QComboBox {
            color: #0f172a;
            background: #ffffff;
        }
        QComboBox QAbstractItemView {
            color: #0f172a;
            background: #ffffff;
            selection-color: #000000;
            selection-background-color: #dbeafe;
        }
        QLineEdit, QSpinBox, QSlider, QDoubleSpinBox {
            color: #0f172a;
            background: #ffffff;
        }
        QPushButton {
            color: #0f172a;
            background: #f8fbff;
            border: 1px solid #cfdcf0;
            border-radius: 9px;
            padding: 8px 12px;
        }
        QPushButton:hover {
            background: #e6f0ff;
        }
        QFrame#ThemeStrip,
        QFrame#ThemeChip {
            background: #ffffff;
            border: 1px solid #d9e5f6;
            border-radius: 14px;
        }
        QLabel#ThemeLead {
            color: #1d4ed8;
            font-weight: 700;
        }
        QLabel#ThemeText {
            color: #53657f;
        }
    )");

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

    auto* rootLayout = qobject_cast<QVBoxLayout*>(centralWidget() ? centralWidget()->layout() : nullptr);
    if (rootLayout) {
        rootLayout->removeWidget(ui->mainSplitter);

        auto* themeStrip = new QFrame(centralWidget());
        themeStrip->setObjectName(QStringLiteral("ThemeStrip"));
        themeStrip->setMinimumHeight(56);
        themeStrip->setMaximumHeight(56);
        auto* themeLayout = new QHBoxLayout(themeStrip);
        themeLayout->setContentsMargins(16, 8, 16, 8);
        themeLayout->setSpacing(10);

        auto makeThemeChip = [themeStrip](const QString& lead, const QString& text) {
            auto* chip = new QFrame(themeStrip);
            chip->setObjectName(QStringLiteral("ThemeChip"));
            auto* layout = new QVBoxLayout(chip);
            layout->setContentsMargins(12, 6, 12, 6);
            layout->setSpacing(0);
            auto* leadLabel = new QLabel(lead, chip);
            leadLabel->setObjectName(QStringLiteral("ThemeLead"));
            auto* textLabel = new QLabel(text, chip);
            textLabel->setObjectName(QStringLiteral("ThemeText"));
            layout->addWidget(leadLabel);
            layout->addWidget(textLabel);
            return chip;
        };

        themeLayout->addWidget(makeThemeChip(QStringLiteral("医学影像"), QStringLiteral("查看、实验、增强一体化")));
        themeLayout->addWidget(makeThemeChip(QStringLiteral("当前模式"), QStringLiteral("实验页面按需切换")));
        themeLayout->addWidget(makeThemeChip(QStringLiteral("输入支持"), QStringLiteral("BMP / JPG / JPEG")));
        themeLayout->addStretch(1);

        m_mainTabs = new QTabWidget(centralWidget());
        m_mainTabs->setObjectName(QStringLiteral("labMainTabWidget"));

        auto* viewerPage = new QWidget(m_mainTabs);
        auto* viewerLayout = new QVBoxLayout(viewerPage);
        viewerLayout->setContentsMargins(0, 0, 0, 0);
        viewerLayout->setSpacing(0);
        viewerLayout->addWidget(ui->mainSplitter);
        m_mainTabs->addTab(viewerPage, QString::fromUtf8(u8"\u67e5\u770b\u5668"));

        QWidget* experimentPage = createExperimentPage();
        m_mainTabs->addTab(experimentPage, QString::fromUtf8(u8"\u5b9e\u9a8c"));

        auto* fourierExperimentPage = new FourierExperimentWidget(m_mainTabs);
        m_mainTabs->addTab(fourierExperimentPage, QString::fromUtf8(u8"\u5b9e\u9a8c\u56db"));

        rootLayout->addWidget(themeStrip);
        rootLayout->addWidget(m_mainTabs);
        setWindowTitle(QStringLiteral("医学影像处理软件"));
    }

    statusBar()->showMessage(QStringLiteral("状态：就绪"));
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
    auto* title = new QLabel(QString::fromUtf8(u8"实验"), titleCard);
    title->setObjectName("SectionTitle");
    auto* subtitle = new QLabel(QString::fromUtf8(u8"左侧选择实验，右侧加载对应界面框架"), titleCard);
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
    themeLayout->addWidget(makeThemeBadge(QStringLiteral("噪声")));
    themeLayout->addWidget(makeThemeBadge(QStringLiteral("边缘")));
    themeLayout->addWidget(makeThemeBadge(QStringLiteral("增强")));
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
    auto* leftTitle = new QLabel(QString::fromUtf8(u8"实验选择"), leftPane);
    leftTitle->setObjectName("SectionTitle");
    leftLayout->addWidget(leftTitle);

    m_experimentList = new QListWidget(leftPane);
    m_experimentList->setObjectName(QStringLiteral("labExperimentList"));
    m_experimentList->addItem(QString::fromUtf8(u8"1. 噪声添加与去噪"));
    m_experimentList->addItem(QString::fromUtf8(u8"2. 边缘检测"));
    m_experimentList->addItem(QString::fromUtf8(u8"3. 图像增强综合实验"));
    leftLayout->addWidget(m_experimentList, 1);

    auto* rightPane = new QFrame(splitter);
    rightPane->setObjectName("Card");
    auto* rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(12, 12, 12, 12);
    rightLayout->setSpacing(8);
    auto* rightTitle = new QLabel(QString::fromUtf8(u8"实验主显示区"), rightPane);
    rightTitle->setObjectName("SectionTitle");
    rightLayout->addWidget(rightTitle);

    m_experimentStackedWidget = new QStackedWidget(rightPane);
    m_experimentStackedWidget->setObjectName(QStringLiteral("labExperimentStackedWidget"));

    auto* emptyPage = new QWidget(m_experimentStackedWidget);
    auto* emptyLayout = new QVBoxLayout(emptyPage);
    emptyLayout->addStretch(1);
    auto* emptyHint = new QLabel(QString::fromUtf8(u8"请选择左侧实验项目"), emptyPage);
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

    m_experimentStatusLabel = new QLabel(QString::fromUtf8(u8"状态：未选择实验"), page);
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
        m_experimentStatusLabel->setText(QString::fromUtf8(u8"\u72b6\u6001\uff1a\u672a\u9009\u62e9\u5b9e\u9a8c"));
        statusBar()->showMessage(QString::fromUtf8(u8"\u5df2\u79fb\u81f3\u5b9e\u9a8c\u9875\u9762\u00b7\u8bf7\u9009\u62e9\u5de6\u4fa7\u5b9e\u9a8c"));
        return;
    }

    const QVector<int> pageMap = {1, 2, 3};
    if (row >= pageMap.size()) {
        return;
    }

    m_experimentStackedWidget->setCurrentIndex(pageMap[row]);
    const QStringList names = {
        QString::fromUtf8(u8"\u566a\u58f0\u6dfb\u52a0\u4e0e\u53bb\u566a"),
        QString::fromUtf8(u8"\u8fb9\u7f18\u68c0\u6d4b"),
        QString::fromUtf8(u8"\u56fe\u50cf\u589e\u5f3a\u7efc\u5408\u5b9e\u9a8c")
    };
    const QString name = (row >= 0 && row < names.size()) ? names[row] : QString();
    m_experimentStatusLabel->setText(QString::fromUtf8(u8"\u72b6\u6001\uff1a\u5df2\u9009\u62e9 ") + name);
    statusBar()->showMessage(QString::fromUtf8(u8"\u5f53\u524d\u5b9e\u9a8c\uff1a") + name + QString::fromUtf8(u8" \u00b7 \u52a0\u8f7d\u5bf9\u5e94\u754c\u9762\u6846\u67b6"));
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

    m_noiseLoadButton = new QPushButton(QString::fromUtf8(u8"选择BMP图像"), controlCard);
    m_noiseInputPathEdit = new QLineEdit(controlCard);
    m_noiseInputPathEdit->setReadOnly(true);
    m_noiseInputPathEdit->setPlaceholderText(QString::fromUtf8(u8"选择一张BMP灰度图像"));

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

    m_edgeLoadButton = new QPushButton(QString::fromUtf8(u8"选择BMP图像"), controlCard);
    m_edgeInputPathEdit = new QLineEdit(controlCard);
    m_edgeInputPathEdit->setReadOnly(true);
    m_edgeInputPathEdit->setPlaceholderText(QString::fromUtf8(u8"选择一张灰度图像"));

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

    m_enhancementLoadButton = new QPushButton(QString::fromUtf8(u8"选择BMP图像"), controlCard);
    m_enhancementInputPathEdit = new QLineEdit(controlCard);
    m_enhancementInputPathEdit->setReadOnly(true);
    m_enhancementInputPathEdit->setPlaceholderText(QString::fromUtf8(u8"选择灰度图像作为增强输入"));

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
    m_enhancementInfoLabel = new QLabel(QString::fromUtf8(u8"步骤说明：请选择图像并开始增强。"), infoCard);
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
    m_enhancementInfoLabel = new QLabel(QString::fromUtf8(u8"步骤说明：请选择图像并开始增强。"), detailCard);
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

    m_enhancementLoadButton = new QPushButton(QString::fromUtf8(u8"选择BMP图像"), controlCard);
    m_enhancementInputPathEdit = new QLineEdit(controlCard);
    m_enhancementInputPathEdit->setReadOnly(true);
    m_enhancementInputPathEdit->setPlaceholderText(QString::fromUtf8(u8"选择灰度图像作为增强输入"));

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
    m_enhancementInfoLabel = new QLabel(QString::fromUtf8(u8"步骤说明：请选择图像并开始增强。"), infoCard);
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
        QString::fromUtf8(u8"选择BMP图像"),
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
        m_experimentStatusLabel->setText(QString::fromUtf8(u8"状态：已加载实验输入图像"));
    }
    statusBar()->showMessage(QString::fromUtf8(u8"实验输入图像已加载：") + QFileInfo(filePath).fileName());
}

void MainWindow::addNoiseToExperiment() {
    if (m_noiseSourceImage.isNull()) {
        QMessageBox::information(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先加载实验输入图像"));
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
        QMessageBox::information(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先加载实验输入图像"));
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
        m_experimentStatusLabel->setText(QString::fromUtf8(u8"状态：已重置实验"));
    }
    statusBar()->showMessage(QString::fromUtf8(u8"实验1已重置"));
}
void MainWindow::saveNoiseExperimentResult() {
    if (m_noiseSourceImage.isNull() && m_noiseNoisyImage.isNull() && m_noiseDenoisedImage.isNull()) {
        QMessageBox::information(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"当前没有可保存的噪声实验图像"));
        return;
    }

    QMessageBox chooser(this);
    chooser.setWindowTitle(QString::fromUtf8(u8"选择保存结果"));
    chooser.setText(QString::fromUtf8(u8"请选择要保存的噪声实验结果图像"));
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
        QString::fromUtf8(u8"选择BMP图像"),
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
        QMessageBox::information(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先加载实验输入图像"));
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
        m_experimentStatusLabel->setText(QString::fromUtf8(u8"状态：已重置边缘检测实验"));
    }
    statusBar()->showMessage(QString::fromUtf8(u8"边缘检测实验已重置"));
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
        QString::fromUtf8(u8"选择BMP图像"),
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
        m_enhancementInfoLabel->setText(QString::fromUtf8(u8"步骤说明：图像已加载，可以开始执行增强流程。"));
    }
    if (m_experimentStatusLabel) {
        m_experimentStatusLabel->setText(QString::fromUtf8(u8"状态：已加载图像增强实验输入图像"));
    }
    statusBar()->showMessage(QString::fromUtf8(u8"图像增强实验输入图像已加载：") + QFileInfo(filePath).fileName());
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
        QMessageBox::information(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先加载实验输入图像"));
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
        m_enhancementInfoLabel->setText(QString::fromUtf8(u8"步骤说明：已完成 8 步图像增强流程，可点击任意步骤查看对应中间结果。"));
    }
    if (m_experimentStatusLabel) {
        m_experimentStatusLabel->setText(QString::fromUtf8(u8"状态：图像增强流程已完成"));
    }
    statusBar()->showMessage(QString::fromUtf8(u8"图像增强综合实验已完成"));
}

void MainWindow::stepEnhancementExperiment() {
    if (m_enhancementSourceImage.isNull()) {
        QMessageBox::information(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先加载实验输入图像"));
        return;
    }

    if (m_enhancementPreviewStep >= 8) {
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"步骤说明：全部步骤已完成，可重新开始或重置。"));
        }
        statusBar()->showMessage(QString::fromUtf8(u8"图像增强步骤已全部完成"));
        return;
    }

    switch (m_enhancementPreviewStep) {
    case 0:
        m_enhancementStep1Image = ImageLabProcessor::step1_originalImage(m_enhancementSourceImage);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"步骤说明：步骤1，显示并缓存原图像。"));
        }
        break;
    case 1:
        m_enhancementStep2Image = ImageLabProcessor::step2_laplacianProcess(m_enhancementStep1Image);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"步骤说明：步骤2，对原图像进行拉普拉斯处理，提取高频细节。"));
        }
        break;
    case 2:
        m_enhancementStep3Image = ImageLabProcessor::step3_sharpenImage(m_enhancementStep1Image, m_enhancementStep2Image);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"步骤说明：步骤3，将原图像与拉普拉斯结果相加，形成锐化图像。"));
        }
        break;
    case 3:
        m_enhancementStep4Image = ImageLabProcessor::step4_sobelProcess(m_enhancementStep1Image);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"步骤说明：步骤4，对原图像应用 Sobel 算子，提取梯度信息。"));
        }
        break;
    case 4:
        m_enhancementStep5Image = ImageLabProcessor::step5_meanFilterGradient(m_enhancementStep4Image);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"步骤说明：步骤5，对 Sobel 结果进行均值滤波，得到平滑梯度图像。"));
        }
        break;
    case 5:
        m_enhancementStep6Image = ImageLabProcessor::step6_maskImage(m_enhancementStep3Image, m_enhancementStep5Image);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"步骤说明：步骤6，将锐化图像与平滑梯度图像相乘，生成掩膜。"));
        }
        break;
    case 6:
        m_enhancementStep7Image = ImageLabProcessor::step7_addOriginalAndMask(m_enhancementStep1Image, m_enhancementStep6Image);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"步骤说明：步骤7，将原图像与掩膜求和，得到增强图像。"));
        }
        break;
    case 7:
        m_enhancementStep8Image = ImageLabProcessor::step8_gammaTransform(m_enhancementStep7Image);
        if (m_enhancementInfoLabel) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"步骤说明：步骤8，对增强图像进行伽马变换，优化对比度。"));
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
    statusBar()->showMessage(QString::fromUtf8(u8"图像增强实验步骤已更新"));
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
        m_enhancementInfoLabel->setText(QString::fromUtf8(u8"步骤说明：已重置，请重新加载图像。"));
    }
    if (m_experimentStatusLabel) {
        m_experimentStatusLabel->setText(QString::fromUtf8(u8"状态：图像增强实验已重置"));
    }
    statusBar()->showMessage(QString::fromUtf8(u8"图像增强综合实验已重置"));
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
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"步骤说明：图像增强流程已完成，左侧显示原图，右侧显示最终结果。"));
        } else if (!m_enhancementStep1Image.isNull()) {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"步骤说明：已载入输入图像，可点击“开始增强”或“分布预览”。"));
        } else {
            m_enhancementInfoLabel->setText(QString::fromUtf8(u8"步骤说明：请选择图像并开始增强。"));
        }
    }
}

void MainWindow::showEnhancementPreviewDialog() {
    if (m_enhancementSourceImage.isNull()) {
        QMessageBox::information(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先加载实验输入图像"));
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
