#include "app/MainWindowInternal.h"

namespace ppocr {
QWidget* MainWindow::buildOverviewPage() {
    auto* page = new QWidget(this);
    page->setObjectName("OverviewRoot");
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(10, 8, 10, 10);
    outer->setSpacing(0);
    outer->setSizeConstraint(QLayout::SetNoConstraint);

    auto* scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget(scroll);
    content->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->setSizeConstraint(QLayout::SetNoConstraint);

    auto* header = new QWidget(content);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);
    headerLayout->setSizeConstraint(QLayout::SetNoConstraint);
    auto* titleBox = new QVBoxLayout();
    titleBox->setSpacing(4);
    auto* title = new QLabel(QStringLiteral("概况"), header);
    title->setObjectName("OverviewTitle");
    auto* overviewSubtitle = new QLabel(QStringLiteral("欢迎使用 PP-OCR 工作台，快速了解训练任务与模型状态"), header);
    overviewSubtitle->setObjectName("OverviewMuted");
    titleBox->addWidget(title);
    titleBox->addWidget(overviewSubtitle);
    headerLayout->addLayout(titleBox, 1);

    recentProjectsCombo_ = new QComboBox(header);
    recentProjectsCombo_->setMinimumWidth(240);
    recentProjectsCombo_->setMaximumWidth(420);
    recentProjectsCombo_->setMinimumContentsLength(16);
    recentProjectsCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    recentProjectsCombo_->setObjectName("OverviewProject");
    projectLabel_ = mutedLabel(QStringLiteral("● 状态：未打开"), header);
    projectLabel_->setObjectName("OverviewProjectStatus");
    auto* newButton = primaryButton(QStringLiteral("新建"), header);
    auto* openButton = new QPushButton(QStringLiteral("打开"), header);
    connect(newButton, &QPushButton::clicked, this, &MainWindow::newProjectDialog);
    connect(openButton, &QPushButton::clicked, this, &MainWindow::openProjectDialog);
    connect(recentProjectsCombo_, QOverload<int>::of(&QComboBox::activated), this, [this](int) {
        openRecentProjectFromCombo();
    });
    headerLayout->addWidget(recentProjectsCombo_);
    headerLayout->addWidget(projectLabel_);
    headerLayout->addWidget(newButton);
    headerLayout->addWidget(openButton);

    auto* metrics = new QGridLayout();
    metrics->setHorizontalSpacing(14);
    metrics->setVerticalSpacing(10);
    metrics->setSizeConstraint(QLayout::SetNoConstraint);
    auto addMetric = [content, metrics](int column, const QString& title, const QString& delta, const QString& icon, const QColor& color, QLabel** valueLabel) {
        auto* box = workbenchCard(content, QStringLiteral("OverviewCard"));
        box->setMinimumWidth(0);
        box->setMinimumHeight(92);
        auto* boxLayout = new QHBoxLayout(box);
        boxLayout->setContentsMargins(12, 12, 12, 12);
        boxLayout->setSpacing(10);
        auto* iconLabel = new QLabel(icon, box);
        iconLabel->setObjectName("OverviewMetricIcon");
        iconLabel->setFixedSize(44, 68);
        iconLabel->setStyleSheet(QStringLiteral("background:%1;").arg(color.name()));
        auto* textBox = new QVBoxLayout();
        textBox->setSpacing(3);
        auto* titleLabel = new QLabel(title, box);
        titleLabel->setObjectName("OverviewMuted");
        titleLabel->setWordWrap(true);
        titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        auto* value = new QLabel(QStringLiteral("0"), box);
        value->setObjectName("OverviewMetricValue");
        auto* deltaLabel = new QLabel(delta, box);
        deltaLabel->setObjectName("OverviewMuted");
        deltaLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        textBox->addWidget(titleLabel);
        textBox->addWidget(value);
        textBox->addWidget(deltaLabel);
        boxLayout->addWidget(iconLabel);
        boxLayout->addLayout(textBox, 1);
        metrics->addWidget(box, 0, column);
        *valueLabel = value;
    };
    addMetric(0, QStringLiteral("训练任务总数"), QStringLiteral("较昨日 ↑ 0"), QStringLiteral("↗"), QColor("#2563eb"), &overviewPagesMetric_);
    addMetric(1, QStringLiteral("进行中任务"), QStringLiteral("较昨日 ↑ 0"), QStringLiteral("▶"), QColor("#14786d"), &overviewLabeledMetric_);
    addMetric(2, QStringLiteral("已完成任务"), QStringLiteral("较昨日 ↑ 0"), QStringLiteral("✓"), QColor("#46d39a"), &overviewSplitMetric_);
    addMetric(3, QStringLiteral("失败任务"), QStringLiteral("较昨日 ↓ 0"), QStringLiteral("!"), QColor("#b0444d"), &overviewRegionsMetric_);
    addMetric(4, QStringLiteral("可用模型"), QStringLiteral("较昨日 ↑ 0"), QStringLiteral("□"), QColor("#7c3aed"), &overviewExportsMetric_);
    addMetric(5, QStringLiteral("数据集"), QStringLiteral("较昨日 ↑ 0"), QStringLiteral("≡"), QColor("#d79b35"), &overviewVersionsMetric_);
    for (int column = 0; column < 6; ++column) {
        metrics->setColumnStretch(column, 1);
    }

    auto* middle = new QGridLayout();
    middle->setSpacing(12);
    middle->setSizeConstraint(QLayout::SetNoConstraint);
    auto* trendCard = workbenchCard(content, QStringLiteral("OverviewCard"));
    trendCard->setMinimumHeight(320);
    auto* trendLayout = new QVBoxLayout(trendCard);
    trendLayout->setContentsMargins(12, 10, 12, 10);
    auto* trendHeader = new QHBoxLayout();
    trendHeader->addWidget(sectionLabel(QStringLiteral("训练任务趋势"), trendCard));
    trendHeader->addStretch(1);
    overviewTrendRangeCombo_ = new QComboBox(trendCard);
    overviewTrendRangeCombo_->addItems({QStringLiteral("近 7 天"), QStringLiteral("近 14 天"), QStringLiteral("近 30 天")});
    connect(overviewTrendRangeCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::refreshOverviewStats);
    trendHeader->addWidget(overviewTrendRangeCombo_);
    overviewTrendChart_ = new DashboardLineChart(trendCard);
    trendLayout->addLayout(trendHeader);
    trendLayout->addWidget(overviewTrendChart_, 1);

    auto* donutCard = workbenchCard(content, QStringLiteral("OverviewCard"));
    donutCard->setMinimumHeight(320);
    auto* donutLayout = new QVBoxLayout(donutCard);
    donutLayout->setContentsMargins(12, 10, 12, 10);
    donutLayout->addWidget(sectionLabel(QStringLiteral("任务状态分布"), donutCard));
    overviewDonutChart_ = new DashboardDonutChart(donutCard);
    donutLayout->addWidget(overviewDonutChart_, 1);

    auto* recentCard = workbenchCard(content, QStringLiteral("OverviewCard"));
    recentCard->setMinimumHeight(320);
    auto* recentLayout = new QVBoxLayout(recentCard);
    recentLayout->setContentsMargins(12, 10, 12, 10);
    auto* recentHeader = new QHBoxLayout();
    recentHeader->addWidget(sectionLabel(QStringLiteral("最近任务"), recentCard));
    recentHeader->addStretch(1);
    auto* allTrainingButton = new QPushButton(QStringLiteral("查看全部"), recentCard);
    connect(allTrainingButton, &QPushButton::clicked, this, [this]() { stack_->setCurrentIndex(2); });
    recentHeader->addWidget(allTrainingButton);
    overviewRecentTasksTable_ = tableWidget(3, {QStringLiteral("任务"), QStringLiteral("状态"), QStringLiteral("时间")}, recentCard);
    overviewRecentTasksTable_->setObjectName("OverviewTable");
    overviewRecentTasksTable_->setMinimumWidth(0);
    overviewRecentTasksTable_->setMinimumHeight(260);
    overviewRecentTasksTable_->verticalHeader()->setDefaultSectionSize(34);
    overviewRecentTasksTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    overviewRecentTasksTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    overviewRecentTasksTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    recentLayout->addLayout(recentHeader);
    recentLayout->addWidget(overviewRecentTasksTable_, 1);

    middle->addWidget(trendCard, 0, 0, 1, 2);
    middle->addWidget(donutCard, 0, 2);
    middle->addWidget(recentCard, 0, 3);
    middle->setColumnStretch(0, 2);
    middle->setColumnStretch(1, 2);
    middle->setColumnStretch(2, 2);
    middle->setColumnStretch(3, 2);

    auto* dataGrid = new QGridLayout();
    dataGrid->setSpacing(12);
    dataGrid->setSizeConstraint(QLayout::SetNoConstraint);
    auto* usageCard = workbenchCard(content, QStringLiteral("OverviewCard"));
    usageCard->setMinimumHeight(230);
    auto* usageLayout = new QVBoxLayout(usageCard);
    usageLayout->setContentsMargins(12, 10, 12, 10);
    usageLayout->addWidget(sectionLabel(QStringLiteral("训练使用情况"), usageCard));
    auto* usageCards = new QHBoxLayout();
    usageCards->setSpacing(8);
    usageCards->setSizeConstraint(QLayout::SetNoConstraint);
    overviewGpuCard_ = new DashboardSparklineCard(QStringLiteral("GPU 使用率"), QColor("#2563eb"), usageCard);
    overviewCpuCard_ = new DashboardSparklineCard(QStringLiteral("CPU 使用率"), QColor("#14786d"), usageCard);
    overviewMemoryCard_ = new DashboardSparklineCard(QStringLiteral("内存使用率"), QColor("#46d39a"), usageCard);
    overviewDiskCard_ = new DashboardSparklineCard(QStringLiteral("存储使用率"), QColor("#d79b35"), usageCard);
    for (auto* card : {overviewGpuCard_, overviewCpuCard_, overviewMemoryCard_, overviewDiskCard_}) {
        usageCards->addWidget(card, 1);
    }
    usageLayout->addLayout(usageCards, 1);

    auto* datasetCard = workbenchCard(content, QStringLiteral("OverviewCard"));
    datasetCard->setMinimumHeight(230);
    auto* datasetLayout = new QVBoxLayout(datasetCard);
    datasetLayout->setContentsMargins(12, 10, 12, 10);
    auto* datasetHeader = new QHBoxLayout();
    datasetHeader->addWidget(sectionLabel(QStringLiteral("数据集概览"), datasetCard));
    datasetHeader->addStretch(1);
    auto* allDatasetButton = new QPushButton(QStringLiteral("查看全部"), datasetCard);
    connect(allDatasetButton, &QPushButton::clicked, this, [this]() { stack_->setCurrentIndex(1); });
    datasetHeader->addWidget(allDatasetButton);
    overviewDatasetTable_ = tableWidget(4, {QStringLiteral("数据集名称"), QStringLiteral("类型"), QStringLiteral("数量"), QStringLiteral("修改时间")}, datasetCard);
    overviewDatasetTable_->setObjectName("OverviewTable");
    overviewDatasetTable_->setMinimumWidth(0);
    overviewDatasetTable_->setMinimumHeight(180);
    overviewDatasetTable_->verticalHeader()->setDefaultSectionSize(34);
    overviewDatasetTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    overviewDatasetTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    overviewDatasetTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    overviewDatasetTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    datasetLayout->addLayout(datasetHeader);
    datasetLayout->addWidget(overviewDatasetTable_, 1);

    dataGrid->addWidget(usageCard, 0, 0, 1, 2);
    dataGrid->addWidget(datasetCard, 0, 2);
    dataGrid->setColumnStretch(0, 2);
    dataGrid->setColumnStretch(1, 2);
    dataGrid->setColumnStretch(2, 3);

    overviewResourceTimer_ = new QTimer(page);
    overviewResourceTimer_->setInterval(5000);
    connect(overviewResourceTimer_, &QTimer::timeout, this, &MainWindow::refreshOverviewResources);

    auto* quickCard = workbenchCard(content, QStringLiteral("OverviewCard"));
    quickCard->setMinimumHeight(98);
    auto* quickLayout = new QVBoxLayout(quickCard);
    quickLayout->setContentsMargins(12, 10, 12, 12);
    quickLayout->addWidget(sectionLabel(QStringLiteral("快捷入口"), quickCard));
    auto* quickRow = new QHBoxLayout();
    quickRow->setSpacing(10);
    quickRow->setSizeConstraint(QLayout::SetNoConstraint);
    auto addQuick = [this, quickCard, quickRow](const QString& title, const QString& subtitle, const QColor& color, int pageIndex) {
        auto* item = workbenchCard(quickCard, QStringLiteral("OverviewQuick"));
        item->setMinimumWidth(0);
        item->setMinimumHeight(66);
        auto* itemLayout = new QHBoxLayout(item);
        itemLayout->setContentsMargins(12, 12, 12, 12);
        auto* icon = new QLabel(QStringLiteral("→"), item);
        icon->setObjectName("OverviewMetricIcon");
        icon->setFixedSize(44, 44);
        icon->setStyleSheet(QStringLiteral("background:%1;").arg(color.name()));
        auto* textBox = new QVBoxLayout();
        textBox->setSpacing(2);
        auto* titleLabel = new QLabel(title, item);
        titleLabel->setStyleSheet(QStringLiteral("font-weight:700;"));
        titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        auto* subtitleLabel = mutedLabel(subtitle, item);
        subtitleLabel->setWordWrap(true);
        subtitleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        textBox->addWidget(titleLabel);
        textBox->addWidget(subtitleLabel);
        auto* button = new QPushButton(QStringLiteral("→"), item);
        button->setObjectName("OverviewArrow");
        button->setMaximumWidth(44);
        connect(button, &QPushButton::clicked, this, [this, pageIndex]() { stack_->setCurrentIndex(pageIndex); });
        itemLayout->addWidget(icon);
        itemLayout->addLayout(textBox, 1);
        itemLayout->addWidget(button);
        quickRow->addWidget(item, 1);
    };
    addQuick(QStringLiteral("训练控制台"), QStringLiteral("查看模型训练状态与输出"), QColor("#2563eb"), 2);
    addQuick(QStringLiteral("模型配置"), QStringLiteral("配置训练参数与超参数设置"), QColor("#14786d"), 2);
    addQuick(QStringLiteral("数据集管理"), QStringLiteral("管理和查看所有数据集"), QColor("#4ebf8f"), 1);
    addQuick(QStringLiteral("训练日志"), QStringLiteral("查看任务训练过程日志与指标"), QColor("#4b86f5"), 2);
    addQuick(QStringLiteral("可视化分析"), QStringLiteral("查看训练指标对比与分析"), QColor("#37a08e"), 2);
    quickLayout->addLayout(quickRow);

    layout->addWidget(header);
    layout->addLayout(metrics);
    layout->addLayout(middle, 6);
    layout->addLayout(dataGrid, 4);
    layout->addWidget(quickCard);
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);
    refreshRecentProjects();
    refreshOverviewStats();
    return page;
}

QWidget* MainWindow::buildLabelPage() {
    auto* page = new QWidget(this);
    page->setObjectName("LabelRoot");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(10);

    auto* header = workbenchCard(page, QStringLiteral("HeaderPanel"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 10, 14, 10);
    headerLayout->setSpacing(10);

    auto* titleBox = new QVBoxLayout();
    titleBox->setSpacing(3);
    auto* title = new QLabel(QStringLiteral("标注工作台"), header);
    title->setObjectName("PageTitle");
    labelWorkspaceSummary_ = new QLabel(QStringLiteral("未打开项目"), header);
    labelWorkspaceSummary_->setObjectName("Muted");
    titleBox->addWidget(title);
    titleBox->addWidget(labelWorkspaceSummary_);

    auto* headerActions = new QHBoxLayout();
    headerActions->setSpacing(8);
    auto* headerImportButton = new QPushButton(QStringLiteral("导入图片"), header);
    auto* headerImportPdfButton = new QPushButton(QStringLiteral("导入PDF"), header);
    auto* createTrainingButton = primaryButton(QStringLiteral("+ 创建训练任务"), header);
    auto* headerPrelabelButton = primaryButton(QStringLiteral("预标注"), header);
    auto* headerValidateButton = new QPushButton(QStringLiteral("校验"), header);
    auto* headerExportButton = new QPushButton(QStringLiteral("导出"), header);
    connect(headerImportButton, &QPushButton::clicked, this, &MainWindow::importImagesDialog);
    connect(headerImportPdfButton, &QPushButton::clicked, this, &MainWindow::importPdfsDialog);
    connect(createTrainingButton, &QPushButton::clicked, this, [this]() { stack_->setCurrentIndex(2); });
    connect(headerPrelabelButton, &QPushButton::clicked, this, &MainWindow::prelabelSelectedPage);
    connect(headerValidateButton, &QPushButton::clicked, this, &MainWindow::validateProject);
    connect(headerExportButton, &QPushButton::clicked, this, &MainWindow::exportProject);
    headerActions->addWidget(headerImportButton);
    headerActions->addWidget(headerImportPdfButton);
    headerActions->addSpacing(8);
    headerActions->addWidget(createTrainingButton);
    headerActions->addWidget(headerPrelabelButton);
    headerActions->addWidget(headerValidateButton);
    headerActions->addWidget(headerExportButton);
    headerLayout->addLayout(titleBox, 1);
    headerLayout->addLayout(headerActions);

    auto* splitter = new QSplitter(Qt::Horizontal, page);
    splitter->setHandleWidth(6);

    auto* left = workbenchCard(splitter, QStringLiteral("Panel"));
    left->setMinimumWidth(200);
    left->setMaximumWidth(310);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(12, 12, 12, 12);
    leftLayout->setSpacing(8);

    pageSearchEdit_ = new QLineEdit(left);
    pageSearchEdit_->hide();
    pageSplitFilterCombo_ = new QComboBox(left);
    pageSplitFilterCombo_->addItem(QStringLiteral("全部集合"), QString());
    pageSplitFilterCombo_->addItem(QStringLiteral("训练集"), QStringLiteral("train"));
    pageSplitFilterCombo_->addItem(QStringLiteral("验证集"), QStringLiteral("val"));
    pageSplitFilterCombo_->hide();
    pageStatusFilterCombo_ = new QComboBox(left);
    pageStatusFilterCombo_->addItem(QStringLiteral("全部状态"), QString());
    pageStatusFilterCombo_->addItem(QStringLiteral("未标注"), QStringLiteral("unlabeled"));
    pageStatusFilterCombo_->addItem(QStringLiteral("已标注"), QStringLiteral("labeled"));
    pageStatusFilterCombo_->addItem(QStringLiteral("已校验"), QStringLiteral("checked"));
    pageStatusFilterCombo_->hide();
    pageSplitCombo_ = new QComboBox(left);
    pageSplitCombo_->addItem(QStringLiteral("训练集"), QStringLiteral("train"));
    pageSplitCombo_->addItem(QStringLiteral("验证集"), QStringLiteral("val"));
    pageSplitCombo_->hide();
    pageStatusCombo_ = new QComboBox(left);
    pageStatusCombo_->addItem(QStringLiteral("未标注"), QStringLiteral("unlabeled"));
    pageStatusCombo_->addItem(QStringLiteral("已标注"), QStringLiteral("labeled"));
    pageStatusCombo_->addItem(QStringLiteral("已校验"), QStringLiteral("checked"));
    pageStatusCombo_->hide();
    connect(pageSearchEdit_, &QLineEdit::textChanged, this, &MainWindow::filterPages);
    connect(pageSplitFilterCombo_, &QComboBox::currentTextChanged, this, &MainWindow::filterPages);
    connect(pageStatusFilterCombo_, &QComboBox::currentTextChanged, this, &MainWindow::filterPages);
    connect(pageSplitCombo_, &QComboBox::currentTextChanged, this, &MainWindow::savePageMetadata);
    connect(pageStatusCombo_, &QComboBox::currentTextChanged, this, &MainWindow::savePageMetadata);

    auto* pageHeader = new QHBoxLayout();
    pageHeader->setSpacing(6);
    pageHeader->addWidget(sectionLabel(QStringLiteral("项目文件"), left));
    pageHeader->addStretch(1);
    pageCountLabel_ = new QLabel(QStringLiteral("0 页"), left);
    pageCountLabel_->setObjectName("Muted");
    pageHeader->addWidget(pageCountLabel_);
    pageMetaLabel_ = new QLabel(QStringLiteral("未打开项目"), left);
    pageMetaLabel_->setObjectName("Muted");
    pageMetaLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pageList_ = new QListWidget(left);
    pageList_->setObjectName("ProjectPageList");
    pageList_->setIconSize(QSize(0, 0));
    pageList_->setUniformItemSizes(false);
    pageList_->setSpacing(0);
    leftLayout->addLayout(pageHeader);
    leftLayout->addWidget(pageMetaLabel_);
    leftLayout->addWidget(pageList_, 1);

    auto* center = new QWidget(splitter);
    center->setObjectName("CanvasPanel");
    auto* centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(8);

    auto* canvasHeader = workbenchCard(center, QStringLiteral("CanvasHeader"));
    auto* canvasHeaderLayout = new QHBoxLayout(canvasHeader);
    canvasHeaderLayout->setContentsMargins(12, 10, 12, 10);
    canvasHeaderLayout->setSpacing(8);
    auto* fileTitleBox = new QVBoxLayout();
    fileTitleBox->setSpacing(2);
    fileTitleLabel_ = sectionLabel(QStringLiteral("未选择图片"), canvasHeader);
    fileMetaLabel_ = new QLabel(QStringLiteral("选择左侧图片开始标注"), canvasHeader);
    fileMetaLabel_->setObjectName("Muted");
    fileTitleBox->addWidget(fileTitleLabel_);
    fileTitleBox->addWidget(fileMetaLabel_);
    canvasHeaderLayout->addLayout(fileTitleBox, 1);

    selectToolButton_ = new QPushButton(QStringLiteral("选择"), canvasHeader);
    drawOcrToolButton_ = new QPushButton(QStringLiteral("OCR框"), canvasHeader);
    drawLayoutToolButton_ = new QPushButton(QStringLiteral("版面框"), canvasHeader);
    panToolButton_ = new QPushButton(QStringLiteral("平移"), canvasHeader);
    panToolButton_->hide();
    for (auto* button : {selectToolButton_, drawOcrToolButton_, drawLayoutToolButton_}) {
        button->setObjectName("ModeButton");
        button->setCheckable(true);
        canvasHeaderLayout->addWidget(button);
    }
    selectToolButton_->setChecked(true);
    connect(selectToolButton_, &QPushButton::clicked, this, [this]() { setCanvasToolMode(QStringLiteral("select")); });
    connect(drawOcrToolButton_, &QPushButton::clicked, this, [this]() { setCanvasToolMode(QStringLiteral("drawOcr")); });
    connect(drawLayoutToolButton_, &QPushButton::clicked, this, [this]() { setCanvasToolMode(QStringLiteral("drawLayout")); });
    connect(panToolButton_, &QPushButton::clicked, this, [this]() { setCanvasToolMode(QStringLiteral("pan")); });

    auto* fitButton = new QPushButton(QStringLiteral("适应"), canvasHeader);
    auto* actualButton = new QPushButton(QStringLiteral("100%"), canvasHeader);
    connect(fitButton, &QPushButton::clicked, this, [this]() {
        if (auto* root = canvasWidget_ ? canvasWidget_->rootObject() : nullptr) {
            QMetaObject::invokeMethod(root, "resetView");
        }
    });
    connect(actualButton, &QPushButton::clicked, this, [this]() {
        if (auto* root = canvasWidget_ ? canvasWidget_->rootObject() : nullptr) {
            root->setProperty("zoomScale", 1.0);
            root->setProperty("panX", 0.0);
            root->setProperty("panY", 0.0);
        }
    });
    canvasHeaderLayout->addWidget(fitButton);
    canvasHeaderLayout->addWidget(actualButton);

    canvasWidget_ = new QQuickWidget(center);
    canvasWidget_->setObjectName("AnnotationCanvas");
    canvasWidget_->setResizeMode(QQuickWidget::SizeRootObjectToView);
    canvasWidget_->setSource(QUrl(QStringLiteral("qrc:/qml/AnnotationCanvas.qml")));
    connectCanvasSignals();

    auto* canvasFooter = new QHBoxLayout();
    canvasModeLabel_ = new QLabel(QStringLiteral("当前模式：选择"), center);
    canvasModeLabel_->setObjectName("Muted");
    auto* canvasHint = new QLabel(QStringLiteral("滚轮缩放 | 拖动画布 | OCR/版面框模式下拖拽画框"), center);
    canvasHint->setObjectName("Muted");
    canvasFooter->addWidget(canvasModeLabel_);
    canvasFooter->addStretch(1);
    canvasFooter->addWidget(canvasHint);
    centerLayout->addWidget(canvasHeader);
    centerLayout->addWidget(canvasWidget_, 1);
    centerLayout->addLayout(canvasFooter);

    auto* inspectorScroll = new QScrollArea(splitter);
    inspectorScroll->setWidgetResizable(true);
    inspectorScroll->setFrameShape(QFrame::NoFrame);
    inspectorScroll->setMinimumWidth(300);
    inspectorScroll->setMaximumWidth(560);
    auto* inspector = new QWidget(inspectorScroll);
    inspector->setObjectName("InspectorPanel");
    auto* inspectorLayout = new QVBoxLayout(inspector);
    inspectorLayout->setContentsMargins(10, 10, 10, 10);
    inspectorLayout->setSpacing(8);
    inspectorLayout->addWidget(sectionLabel(QStringLiteral("属性检查器"), inspector));

    auto* tabs = new QTabWidget(inspector);
    tabs->setObjectName("InspectorTabs");

    undoAnnotationButton_ = new QPushButton(QStringLiteral("撤销"), inspector);
    redoAnnotationButton_ = new QPushButton(QStringLiteral("重做"), inspector);
    connect(undoAnnotationButton_, &QPushButton::clicked, this, &MainWindow::undoAnnotationEdit);
    connect(redoAnnotationButton_, &QPushButton::clicked, this, &MainWindow::redoAnnotationEdit);
    auto* undoShortcut = new QShortcut(QKeySequence::Undo, page);
    auto* redoShortcut = new QShortcut(QKeySequence::Redo, page);
    connect(undoShortcut, &QShortcut::activated, this, &MainWindow::undoAnnotationEdit);
    connect(redoShortcut, &QShortcut::activated, this, &MainWindow::redoAnnotationEdit);

    selectedRegionLabel_ = new QLabel(QStringLiteral("ID: -"), inspector);
    selectedRegionLabel_->setObjectName("Muted");
    regionTextEdit_ = new QPlainTextEdit(inspector);
    regionTextEdit_->setPlaceholderText(QStringLiteral("OCR 文本内容"));
    regionTextEdit_->setMinimumHeight(160);
    layoutLabelCombo_ = new QComboBox(inspector);
    regionReadingOrderSpin_ = new QSpinBox(inspector);
    regionReadingOrderSpin_->setRange(0, 999999);
    regionCheckedCheck_ = new QCheckBox(QStringLiteral("checked（已校验）"), inspector);
    regionIgnoreCheck_ = new QCheckBox(QStringLiteral("ignore（忽略）"), inspector);
    saveRegionButton_ = primaryButton(QStringLiteral("更新当前框"), inspector);
    deleteRegionButton_ = dangerButton(QStringLiteral("删除当前框"), inspector);
    auto* generateCropsButton = new QPushButton(QStringLiteral("生成 Rec Crop"), inspector);
    auto* confirmCurrentButton = new QPushButton(QStringLiteral("批量确认当前页"), inspector);
    auto* confirmAllButton = new QPushButton(QStringLiteral("批量确认全项目"), inspector);
    auto* clearPageButton = dangerButton(QStringLiteral("清空当前页标注"), inspector);
    auto* clearAllButton = dangerButton(QStringLiteral("一键删除所有标注"), inspector);
    connect(saveRegionButton_, &QPushButton::clicked, this, &MainWindow::saveSelectedRegionText);
    connect(deleteRegionButton_, &QPushButton::clicked, this, &MainWindow::deleteSelectedRegion);
    connect(generateCropsButton, &QPushButton::clicked, this, &MainWindow::generateCurrentRecCrops);
    connect(confirmCurrentButton, &QPushButton::clicked, this, &MainWindow::confirmCurrentPageAnnotations);
    connect(confirmAllButton, &QPushButton::clicked, this, &MainWindow::confirmAllProjectAnnotations);
    connect(clearPageButton, &QPushButton::clicked, this, &MainWindow::clearCurrentPageAnnotations);
    connect(clearAllButton, &QPushButton::clicked, this, &MainWindow::clearAllAnnotations);

    auto* regionTab = new QWidget(tabs);
    auto* regionLayout = new QVBoxLayout(regionTab);
    regionLayout->setContentsMargins(10, 10, 10, 10);
    regionLayout->setSpacing(8);
    regionLayout->addWidget(sectionLabel(QStringLiteral("文本框 / 版面框属性"), regionTab));
    regionLayout->addWidget(selectedRegionLabel_);
    auto* textRow = new QHBoxLayout();
    auto* textLabel = new QLabel(QStringLiteral("文本内容"), regionTab);
    textRow->addWidget(textLabel);
    textRow->addWidget(regionTextEdit_, 1);
    regionLayout->addLayout(textRow);
    regionLayout->addWidget(regionIgnoreCheck_);
    regionLayout->addWidget(regionCheckedCheck_);
    auto* orderRow = new QHBoxLayout();
    orderRow->addWidget(new QLabel(QStringLiteral("阅读顺序"), regionTab));
    orderRow->addWidget(regionReadingOrderSpin_, 1);
    regionLayout->addLayout(orderRow);
    auto* layoutLabelRow = new QHBoxLayout();
    layoutLabelRow->addWidget(new QLabel(QStringLiteral("版面类别"), regionTab));
    layoutLabelRow->addWidget(layoutLabelCombo_, 1);
    regionLayout->addLayout(layoutLabelRow);
    auto* historyLayout = new QHBoxLayout();
    historyLayout->addWidget(undoAnnotationButton_);
    historyLayout->addWidget(redoAnnotationButton_);
    regionLayout->addWidget(saveRegionButton_);
    regionLayout->addWidget(generateCropsButton);
    regionLayout->addWidget(confirmCurrentButton);
    regionLayout->addWidget(confirmAllButton);
    regionLayout->addWidget(deleteRegionButton_);
    regionLayout->addWidget(clearPageButton);
    regionLayout->addWidget(clearAllButton);
    regionLayout->addLayout(historyLayout);
    regionLayout->addStretch(1);

    docOrientationCombo_ = new QComboBox(inspector);
    textlineOrientationCombo_ = new QComboBox(inspector);
    tableClassificationCombo_ = new QComboBox(inspector);
    connect(docOrientationCombo_, &QComboBox::currentTextChanged, this, &MainWindow::saveImageLabels);
    connect(textlineOrientationCombo_, &QComboBox::currentTextChanged, this, &MainWindow::saveImageLabels);
    connect(tableClassificationCombo_, &QComboBox::currentTextChanged, this, &MainWindow::saveImageLabels);

    auto* classTab = new QWidget(tabs);
    auto* classLayout = new QVBoxLayout(classTab);
    classLayout->setContentsMargins(10, 10, 10, 10);
    classLayout->setSpacing(8);
    classLayout->addWidget(sectionLabel(QStringLiteral("分类标注"), classTab));
    auto addClassRow = [classLayout, classTab](const QString& labelText, QComboBox* combo, const QString& hintText) {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel(labelText, classTab));
        row->addWidget(combo, 1);
        classLayout->addLayout(row);
        if (!hintText.isEmpty()) {
            auto* hint = mutedLabel(hintText, classTab);
            hint->setWordWrap(true);
            hint->setContentsMargins(86, 0, 0, 0);
            classLayout->addWidget(hint);
        }
    };
    addClassRow(QStringLiteral("文档方向"), docOrientationCombo_, QStringLiteral("文档方向：整张图要不要旋转（以顺时针角度为准）"));
    addClassRow(QStringLiteral("文本行方向"), textlineOrientationCombo_, QStringLiteral("文本行方向：文字行是不是上下颠倒，只管 0/180"));
    addClassRow(QStringLiteral("表格类型"), tableClassificationCombo_, QString());
    auto* applyClassButton = primaryButton(QStringLiteral("应用分类标签"), classTab);
    connect(applyClassButton, &QPushButton::clicked, this, &MainWindow::saveImageLabels);
    classLayout->addWidget(applyClassButton);
    classLayout->addStretch(1);

    auto* modelTab = new QWidget(tabs);
    auto* modelLayout = new QVBoxLayout(modelTab);
    modelLayout->setContentsMargins(10, 10, 10, 10);
    modelLayout->setSpacing(8);
    modelLayout->addWidget(sectionLabel(QStringLiteral("本地模型管理"), modelTab));
    auto addModelRow = [modelLayout, modelTab](const QString& labelText, QComboBox* combo) {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel(labelText, modelTab));
        combo->setMinimumWidth(150);
        combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        row->addWidget(combo, 1);
        modelLayout->addLayout(row);
    };
    labelDetModelCombo_ = new QComboBox(modelTab);
    labelRecModelCombo_ = new QComboBox(modelTab);
    labelTextlineClsModelCombo_ = new QComboBox(modelTab);
    labelDocOrientationModelCombo_ = new QComboBox(modelTab);
    labelDocUnwarpingModelCombo_ = new QComboBox(modelTab);
    labelLayoutModelCombo_ = new QComboBox(modelTab);
    addModelRow(QStringLiteral("Det 检测模型"), labelDetModelCombo_);
    addModelRow(QStringLiteral("Rec 识别模型"), labelRecModelCombo_);
    addModelRow(QStringLiteral("Cls 方向模型"), labelTextlineClsModelCombo_);
    addModelRow(QStringLiteral("文档方向模型"), labelDocOrientationModelCombo_);
    addModelRow(QStringLiteral("文档矫正模型"), labelDocUnwarpingModelCombo_);
    addModelRow(QStringLiteral("Layout 版面模型"), labelLayoutModelCombo_);

    labelUseGpuCheck_ = new QCheckBox(QStringLiteral("使用 GPU"), modelTab);
    labelEnableTextlineClsCheck_ = new QCheckBox(QStringLiteral("启用文本行方向分类"), modelTab);
    labelEnableDocOrientationCheck_ = new QCheckBox(QStringLiteral("启用文档方向分类"), modelTab);
    labelEnableDocUnwarpingCheck_ = new QCheckBox(QStringLiteral("启用文档矫正"), modelTab);
    prelabelScoreThresholdSpin_ = new QDoubleSpinBox(modelTab);
    prelabelScoreThresholdSpin_->setRange(0.0, 1.0);
    prelabelScoreThresholdSpin_->setDecimals(2);
    prelabelScoreThresholdSpin_->setSingleStep(0.05);
    prelabelScoreThresholdSpin_->setValue(0.50);
    prelabelScoreThresholdSpin_->setToolTip(QStringLiteral("用于 OCR 识别分数和 Layout 版面框分数。降低阈值会保留更多低置信度结果，也可能增加误标。"));
    modelLayout->addWidget(labelUseGpuCheck_);
    modelLayout->addWidget(labelEnableTextlineClsCheck_);
    modelLayout->addWidget(labelEnableDocOrientationCheck_);
    modelLayout->addWidget(labelEnableDocUnwarpingCheck_);
    auto* thresholdRow = new QHBoxLayout();
    thresholdRow->addWidget(new QLabel(QStringLiteral("预标注置信度阈值"), modelTab));
    thresholdRow->addWidget(prelabelScoreThresholdSpin_);
    thresholdRow->addStretch(1);
    modelLayout->addLayout(thresholdRow);
    auto* modelHint = mutedLabel(QStringLiteral("提示：文档方向会单独写入分类标签；文档矫正不参与 OCR 框预标注，避免坐标偏移；预标注生成的框默认为 unchecked，确认后再导出训练集。"), modelTab);
    modelHint->setWordWrap(true);
    modelLayout->addWidget(modelHint);
    auto* checkModelButton = new QPushButton(QStringLiteral("检查模型"), modelTab);
    auto* prelabelCurrentButton = primaryButton(QStringLiteral("预标注当前页"), modelTab);
    auto* prelabelAllButton = new QPushButton(QStringLiteral("一键预标注全部图片"), modelTab);
    labelModelStatus_ = mutedLabel(QStringLiteral("状态：未配置"), modelTab);
    connect(checkModelButton, &QPushButton::clicked, this, [this]() {
        saveLabelModelConfig();
        const QStringList missing = labelModelMissingReasons();
        if (missing.isEmpty()) {
            if (labelModelStatus_) {
                labelModelStatus_->setText(QStringLiteral("状态：模型目录可用"));
            }
            statusBar()->showMessage(QStringLiteral("模型检查通过"), 3000);
        } else {
            if (labelModelStatus_) {
                labelModelStatus_->setText(QStringLiteral("状态：模型不完整"));
            }
            QMessageBox::warning(this, QStringLiteral("模型检查"), missing.join(QLatin1Char('\n')));
        }
    });
    connect(prelabelCurrentButton, &QPushButton::clicked, this, [this]() {
        saveLabelModelConfig();
        const bool runOrientation =
            (labelEnableTextlineClsCheck_ && labelEnableTextlineClsCheck_->isChecked())
            || (labelEnableDocOrientationCheck_ && labelEnableDocOrientationCheck_->isChecked());
        const bool runLayout = !selectedPredictionModelDir(labelLayoutModelCombo_).isEmpty();
        startPrelabelCurrentPageAsync(
            true,
            runOrientation,
            false,
            runLayout,
            QStringLiteral("预标注当前页"));
    });
    connect(prelabelAllButton, &QPushButton::clicked, this, [this]() {
        startOneClickPrelabelAll();
    });
    for (auto* combo : {labelDetModelCombo_, labelRecModelCombo_, labelTextlineClsModelCombo_,
                       labelDocOrientationModelCombo_, labelDocUnwarpingModelCombo_, labelLayoutModelCombo_}) {
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::saveLabelModelConfig);
    }
    for (auto* checkbox : {labelUseGpuCheck_, labelEnableTextlineClsCheck_, labelEnableDocOrientationCheck_, labelEnableDocUnwarpingCheck_}) {
        connect(checkbox, &QCheckBox::toggled, this, &MainWindow::saveLabelModelConfig);
    }
    connect(prelabelScoreThresholdSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::saveLabelModelConfig);
    modelLayout->addWidget(checkModelButton);
    modelLayout->addWidget(prelabelCurrentButton);
    modelLayout->addWidget(prelabelAllButton);
    modelLayout->addWidget(labelModelStatus_);
    modelLayout->addStretch(1);

    exportDetCheck_ = new QCheckBox(QStringLiteral("PP-OCR Det"), inspector);
    exportRecCheck_ = new QCheckBox(QStringLiteral("PP-OCR Rec"), inspector);
    exportClsCheck_ = new QCheckBox(QStringLiteral("文档方向 Cls"), inspector);
    exportTextlineClsCheck_ = new QCheckBox(QStringLiteral("文本行方向 Cls"), inspector);
    exportTableClsCheck_ = new QCheckBox(QStringLiteral("表格类型 Cls"), inspector);
    exportCocoCheck_ = new QCheckBox(QStringLiteral("Layout COCO Detection"), inspector);
    exportCheckedOnlyCheck_ = new QCheckBox(QStringLiteral("只导出已确认标注 checked=true"), inspector);
    exportDetCheck_->setChecked(true);
    exportRecCheck_->setChecked(true);
    exportClsCheck_->setChecked(true);
    exportTextlineClsCheck_->setChecked(true);
    exportTableClsCheck_->setChecked(false);
    exportCocoCheck_->setChecked(true);
    exportCheckedOnlyCheck_->setChecked(true);
    auto* exportButton = primaryButton(QStringLiteral("开始导出"), inspector);
    exportProjectButton_ = exportButton;
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportProject);

    auto* exportTab = new QWidget(tabs);
    auto* exportLayout = new QVBoxLayout(exportTab);
    exportLayout->setContentsMargins(10, 10, 10, 10);
    exportLayout->setSpacing(8);
    exportLayout->addWidget(sectionLabel(QStringLiteral("导出设置"), exportTab));
    exportLayout->addWidget(exportDetCheck_);
    exportLayout->addWidget(exportRecCheck_);
    exportLayout->addWidget(exportClsCheck_);
    exportLayout->addWidget(exportTextlineClsCheck_);
    exportLayout->addWidget(exportTableClsCheck_);
    exportLayout->addWidget(exportCocoCheck_);
    auto* exportHint = mutedLabel(QStringLiteral("文档矫正 UVDoc 需要成对矫正标注，当前项目格式暂不导出该数据集。导出 PP-OCR Rec 时会自动生成 Rec Crop；checked-only 开启时，未确认预标注不会进入训练集。"), exportTab);
    exportHint->setWordWrap(true);
    exportLayout->addWidget(exportHint);
    exportLayout->addSpacing(12);
    exportLayout->addWidget(exportCheckedOnlyCheck_);
    auto* exportDirRow = new QHBoxLayout();
    exportOutputDirEdit_ = new QLineEdit(exportTab);
    exportOutputDirEdit_->setPlaceholderText(QStringLiteral("留空使用当前项目 exports；外部目录会创建带时间戳的子目录"));
    auto* browseExportDirButton = new QPushButton(QStringLiteral("选择目录"), exportTab);
    connect(browseExportDirButton, &QPushButton::clicked, this, &MainWindow::browseExportOutputDir);
    exportDirRow->addWidget(exportOutputDirEdit_, 1);
    exportDirRow->addWidget(browseExportDirButton);
    exportLayout->addLayout(exportDirRow);
    exportProgressLabel_ = mutedLabel(QStringLiteral("导出状态：空闲"), exportTab);
    exportProgressBar_ = new QProgressBar(exportTab);
    exportProgressBar_->setRange(0, 100);
    exportProgressBar_->setValue(0);
    exportProgressBar_->setTextVisible(true);
    exportLayout->addWidget(exportProgressLabel_);
    exportLayout->addWidget(exportProgressBar_);
    exportLayout->addWidget(exportButton);
    exportLayout->addStretch(1);

    auto* validationTab = new QWidget(tabs);
    auto* validationLayout = new QVBoxLayout(validationTab);
    validationLayout->setContentsMargins(10, 10, 10, 10);
    validationLayout->setSpacing(8);
    validationLayout->addWidget(sectionLabel(QStringLiteral("数据校验结果"), validationTab));
    auto* validateButton = primaryButton(QStringLiteral("重新校验"), validationTab);
    connect(validateButton, &QPushButton::clicked, this, &MainWindow::validateProject);
    labelValidationSummaryLabel_ = mutedLabel(QStringLiteral("尚未运行校验"), validationTab);
    labelValidationIssueTable_ = tableWidget(5, {
            QStringLiteral("状态"),
            QStringLiteral("任务"),
            QStringLiteral("文件"),
            QStringLiteral("框ID"),
            QStringLiteral("问题描述"),
        }, validationTab);
    labelValidationIssueTable_->setObjectName("LabelValidationTable");
    validationLayout->addWidget(validateButton);
    validationLayout->addWidget(labelValidationIssueTable_, 1);
    validationLayout->addWidget(labelValidationSummaryLabel_);

    tabs->addTab(regionTab, QStringLiteral("属性"));
    tabs->addTab(classTab, QStringLiteral("分类"));
    tabs->addTab(modelTab, QStringLiteral("模型"));
    tabs->addTab(exportTab, QStringLiteral("导出"));
    tabs->addTab(validationTab, QStringLiteral("校验"));
    inspectorLayout->addWidget(tabs, 1);
    inspectorScroll->setWidget(inspector);

    splitter->addWidget(left);
    splitter->addWidget(center);
    splitter->addWidget(inspectorScroll);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({210, 700, 320});
    connect(pageList_, &QListWidget::currentRowChanged, this, &MainWindow::loadSelectedPage);

    layout->addWidget(header, 0);
    layout->addWidget(splitter, 1);

    updateSelectedRegionPanel();
    updateAnnotationHistoryActions();
    refreshLabelModelChoices();
    return page;
}

QWidget* MainWindow::buildLabelPageLegacy() {
    auto* page = new QWidget(this);
    page->setObjectName("LabelRoot");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto* header = new QWidget(page);
    header->setObjectName("HeaderPanel");
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 10, 14, 10);
    headerLayout->setSpacing(10);
    auto* titleBox = new QVBoxLayout();
    titleBox->setSpacing(3);
    auto* title = new QLabel(QStringLiteral("标注工作台"), header);
    title->setObjectName("PageTitle");
    labelWorkspaceSummary_ = new QLabel(QStringLiteral("未打开项目"), header);
    labelWorkspaceSummary_->setObjectName("Muted");
    titleBox->addWidget(title);
    titleBox->addWidget(labelWorkspaceSummary_);
    auto* headerActions = new QHBoxLayout();
    auto* headerImportButton = new QPushButton(QStringLiteral("导入图片"), header);
    auto* headerImportPdfButton = new QPushButton(QStringLiteral("导入PDF"), header);
    auto* createTrainingButton = new QPushButton(QStringLiteral("+ 创建训练任务"), header);
    auto* headerPrelabelButton = new QPushButton(QStringLiteral("预标注"), header);
    auto* headerValidateButton = new QPushButton(QStringLiteral("校验"), header);
    auto* headerExportButton = new QPushButton(QStringLiteral("导出"), header);
    createTrainingButton->setObjectName("Primary");
    headerPrelabelButton->setObjectName("Primary");
    connect(headerImportButton, &QPushButton::clicked, this, &MainWindow::importImagesDialog);
    connect(headerImportPdfButton, &QPushButton::clicked, this, &MainWindow::importPdfsDialog);
    connect(createTrainingButton, &QPushButton::clicked, this, [this]() { stack_->setCurrentIndex(2); });
    connect(headerPrelabelButton, &QPushButton::clicked, this, &MainWindow::prelabelSelectedPage);
    connect(headerValidateButton, &QPushButton::clicked, this, &MainWindow::validateProject);
    connect(headerExportButton, &QPushButton::clicked, this, &MainWindow::exportProject);
    headerActions->addWidget(headerImportButton);
    headerActions->addWidget(headerImportPdfButton);
    headerActions->addSpacing(8);
    headerActions->addWidget(createTrainingButton);
    headerActions->addWidget(headerPrelabelButton);
    headerActions->addWidget(headerValidateButton);
    headerActions->addWidget(headerExportButton);
    headerActions->addStretch(1);
    headerLayout->addLayout(titleBox, 1);
    headerLayout->addLayout(headerActions, 0);

    auto* splitter = new QSplitter(Qt::Horizontal, page);
    auto* left = new QWidget(splitter);
    left->setObjectName("Panel");
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(12, 12, 12, 12);
    leftLayout->setSpacing(10);
    pageSearchEdit_ = new QLineEdit(left);
    pageSearchEdit_->setPlaceholderText(QStringLiteral("搜索页面 / 文件名 / 状态"));
    pageSplitFilterCombo_ = new QComboBox(left);
    pageSplitFilterCombo_->addItem(QStringLiteral("全部集合"), QString());
    pageSplitFilterCombo_->addItem(QStringLiteral("训练集"), QStringLiteral("train"));
    pageSplitFilterCombo_->addItem(QStringLiteral("验证集"), QStringLiteral("val"));
    pageStatusFilterCombo_ = new QComboBox(left);
    pageStatusFilterCombo_->addItem(QStringLiteral("全部状态"), QString());
    pageStatusFilterCombo_->addItem(QStringLiteral("未标注"), QStringLiteral("unlabeled"));
    pageStatusFilterCombo_->addItem(QStringLiteral("已标注"), QStringLiteral("labeled"));
    pageStatusFilterCombo_->addItem(QStringLiteral("已校验"), QStringLiteral("checked"));
    auto* filterRow = new QHBoxLayout();
    filterRow->addWidget(pageSplitFilterCombo_);
    filterRow->addWidget(pageStatusFilterCombo_);
    pageList_ = new QListWidget(left);
    pageList_->setIconSize(QSize(72, 96));
    pageMetaLabel_ = new QLabel(QStringLiteral("请选择页面"), left);
    pageMetaLabel_->setObjectName("Muted");
    pageSplitCombo_ = new QComboBox(left);
    pageSplitCombo_->addItem(QStringLiteral("训练集"), QStringLiteral("train"));
    pageSplitCombo_->addItem(QStringLiteral("验证集"), QStringLiteral("val"));
    pageStatusCombo_ = new QComboBox(left);
    pageStatusCombo_->addItem(QStringLiteral("未标注"), QStringLiteral("unlabeled"));
    pageStatusCombo_->addItem(QStringLiteral("已标注"), QStringLiteral("labeled"));
    pageStatusCombo_->addItem(QStringLiteral("已校验"), QStringLiteral("checked"));
    auto* metadataRow = new QHBoxLayout();
    metadataRow->addWidget(pageSplitCombo_);
    metadataRow->addWidget(pageStatusCombo_);
    connect(pageSearchEdit_, &QLineEdit::textChanged, this, &MainWindow::filterPages);
    connect(pageSplitFilterCombo_, &QComboBox::currentTextChanged, this, &MainWindow::filterPages);
    connect(pageStatusFilterCombo_, &QComboBox::currentTextChanged, this, &MainWindow::filterPages);
    connect(pageSplitCombo_, &QComboBox::currentTextChanged, this, &MainWindow::savePageMetadata);
    connect(pageStatusCombo_, &QComboBox::currentTextChanged, this, &MainWindow::savePageMetadata);
    auto* pageHeader = new QHBoxLayout();
    auto* pageTitle = sectionLabel(QStringLiteral("项目文件"), left);
    pageCountLabel_ = new QLabel(QStringLiteral("0 页"), left);
    pageCountLabel_->setObjectName("Muted");
    pageHeader->addWidget(pageTitle);
    pageHeader->addStretch(1);
    pageHeader->addWidget(pageCountLabel_);
    leftLayout->addLayout(pageHeader);
    leftLayout->addWidget(pageMetaLabel_);
    leftLayout->addWidget(pageList_, 1);

    auto* center = new QWidget(splitter);
    center->setObjectName("CanvasPanel");
    auto* centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(8);

    auto* canvasHeader = new QWidget(center);
    canvasHeader->setObjectName("CanvasHeader");
    auto* canvasHeaderLayout = new QHBoxLayout(canvasHeader);
    canvasHeaderLayout->setContentsMargins(12, 10, 12, 10);
    canvasHeaderLayout->setSpacing(8);
    auto* fileTitleBox = new QVBoxLayout();
    fileTitleBox->setSpacing(2);
    fileTitleLabel_ = sectionLabel(QStringLiteral("未选择图片"), canvasHeader);
    fileMetaLabel_ = new QLabel(QStringLiteral("选择左侧图片开始标注"), canvasHeader);
    fileMetaLabel_->setObjectName("Muted");
    fileTitleBox->addWidget(fileTitleLabel_);
    fileTitleBox->addWidget(fileMetaLabel_);
    canvasHeaderLayout->addLayout(fileTitleBox, 1);

    selectToolButton_ = new QPushButton(QStringLiteral("选择"), canvasHeader);
    drawOcrToolButton_ = new QPushButton(QStringLiteral("OCR框"), canvasHeader);
    drawLayoutToolButton_ = new QPushButton(QStringLiteral("版面框"), canvasHeader);
    panToolButton_ = new QPushButton(QStringLiteral("平移"), canvasHeader);
    for (auto* button : {selectToolButton_, drawOcrToolButton_, drawLayoutToolButton_}) {
        button->setObjectName("ModeButton");
        button->setCheckable(true);
        canvasHeaderLayout->addWidget(button);
    }
    panToolButton_->setObjectName("ModeButton");
    panToolButton_->setCheckable(true);
    selectToolButton_->setChecked(true);
    connect(selectToolButton_, &QPushButton::clicked, this, [this]() { setCanvasToolMode("select"); });
    connect(drawOcrToolButton_, &QPushButton::clicked, this, [this]() { setCanvasToolMode("drawOcr"); });
    connect(drawLayoutToolButton_, &QPushButton::clicked, this, [this]() { setCanvasToolMode("drawLayout"); });
    connect(panToolButton_, &QPushButton::clicked, this, [this]() { setCanvasToolMode("pan"); });
    auto* fitButton = new QPushButton(QStringLiteral("适应"), canvasHeader);
    auto* actualButton = new QPushButton(QStringLiteral("100%"), canvasHeader);
    connect(fitButton, &QPushButton::clicked, this, [this]() {
        if (auto* root = canvasWidget_ ? canvasWidget_->rootObject() : nullptr) {
            QMetaObject::invokeMethod(root, "resetView");
        }
    });
    connect(actualButton, &QPushButton::clicked, this, [this]() {
        if (auto* root = canvasWidget_ ? canvasWidget_->rootObject() : nullptr) {
            root->setProperty("zoomScale", 1.0);
            root->setProperty("panX", 0.0);
            root->setProperty("panY", 0.0);
        }
    });
    canvasHeaderLayout->addWidget(fitButton);
    canvasHeaderLayout->addWidget(actualButton);

    canvasWidget_ = new QQuickWidget(center);
    canvasWidget_->setObjectName("AnnotationCanvas");
    canvasWidget_->setResizeMode(QQuickWidget::SizeRootObjectToView);
    canvasWidget_->setSource(QUrl("qrc:/qml/AnnotationCanvas.qml"));
    connectCanvasSignals();

    auto* canvasFooter = new QHBoxLayout();
    canvasModeLabel_ = new QLabel(QStringLiteral("当前模式：选择"), center);
    canvasModeLabel_->setObjectName("Muted");
    auto* canvasHint = new QLabel(QStringLiteral("滚轮缩放 | 拖动画布 | OCR/版面框模式下拖拽画框"), center);
    canvasHint->setObjectName("Muted");
    canvasFooter->addWidget(canvasModeLabel_);
    canvasFooter->addStretch(1);
    canvasFooter->addWidget(canvasHint);
    centerLayout->addWidget(canvasHeader);
    centerLayout->addWidget(canvasWidget_, 1);
    centerLayout->addLayout(canvasFooter);

    auto* inspectorScroll = new QScrollArea(splitter);
    inspectorScroll->setWidgetResizable(true);
    inspectorScroll->setFrameShape(QFrame::NoFrame);
    inspectorScroll->setMinimumWidth(260);
    inspectorScroll->setMaximumWidth(500);
    auto* inspector = new QWidget(inspectorScroll);
    inspector->setObjectName("InspectorPanel");
    auto* inspectorLayout = new QVBoxLayout(inspector);
    inspectorLayout->setContentsMargins(10, 10, 10, 10);
    inspectorLayout->setSpacing(8);
    undoAnnotationButton_ = new QPushButton(QStringLiteral("撤销"), inspector);
    redoAnnotationButton_ = new QPushButton(QStringLiteral("重做"), inspector);
    connect(undoAnnotationButton_, &QPushButton::clicked, this, &MainWindow::undoAnnotationEdit);
    connect(redoAnnotationButton_, &QPushButton::clicked, this, &MainWindow::redoAnnotationEdit);
    auto* historyLayout = new QHBoxLayout();
    historyLayout->addWidget(undoAnnotationButton_);
    historyLayout->addWidget(redoAnnotationButton_);
    auto* undoShortcut = new QShortcut(QKeySequence::Undo, page);
    auto* redoShortcut = new QShortcut(QKeySequence::Redo, page);
    connect(undoShortcut, &QShortcut::activated, this, &MainWindow::undoAnnotationEdit);
    connect(redoShortcut, &QShortcut::activated, this, &MainWindow::redoAnnotationEdit);

    selectedRegionLabel_ = new QLabel(QStringLiteral("未选择区域"), inspector);
    selectedRegionLabel_->setObjectName("Muted");
    regionTextEdit_ = new QPlainTextEdit(inspector);
    regionTextEdit_->setPlaceholderText(QStringLiteral("OCR 文本"));
    regionTextEdit_->setMinimumHeight(180);
    layoutLabelCombo_ = new QComboBox(inspector);
    regionReadingOrderSpin_ = new QSpinBox(inspector);
    regionReadingOrderSpin_->setRange(0, 999999);
    regionReadingOrderSpin_->setPrefix(QStringLiteral("顺序 "));
    regionCheckedCheck_ = new QCheckBox(QStringLiteral("已检查"), inspector);
    regionIgnoreCheck_ = new QCheckBox(QStringLiteral("忽略识别"), inspector);
    saveRegionButton_ = new QPushButton(QStringLiteral("保存区域"), inspector);
    deleteRegionButton_ = new QPushButton(QStringLiteral("删除区域"), inspector);
    auto* generateCropsButton = new QPushButton(QStringLiteral("生成 Rec 裁剪"), inspector);
    auto* confirmCurrentButton = new QPushButton(QStringLiteral("批量确认当前页"), inspector);
    auto* confirmAllButton = new QPushButton(QStringLiteral("批量确认全项目"), inspector);
    auto* clearPageButton = new QPushButton(QStringLiteral("清空当前页"), inspector);
    auto* clearAllButton = new QPushButton(QStringLiteral("清空全部标注"), inspector);
    clearPageButton->setObjectName("Danger");
    clearAllButton->setObjectName("Danger");
    docOrientationCombo_ = new QComboBox(inspector);
    textlineOrientationCombo_ = new QComboBox(inspector);
    tableClassificationCombo_ = new QComboBox(inspector);
    exportDetCheck_ = new QCheckBox(QStringLiteral("导出 Det"), inspector);
    exportRecCheck_ = new QCheckBox(QStringLiteral("导出 Rec"), inspector);
    exportClsCheck_ = new QCheckBox(QStringLiteral("导出文档方向"), inspector);
    exportTextlineClsCheck_ = new QCheckBox(QStringLiteral("导出文本行方向"), inspector);
    exportTableClsCheck_ = new QCheckBox(QStringLiteral("导出表格分类"), inspector);
    exportCocoCheck_ = new QCheckBox(QStringLiteral("导出版面 COCO"), inspector);
    exportCheckedOnlyCheck_ = new QCheckBox(QStringLiteral("仅已检查区域"), inspector);
    exportDetCheck_->setChecked(true);
    exportRecCheck_->setChecked(true);
    exportClsCheck_->setChecked(true);
    exportTextlineClsCheck_->setChecked(true);
    exportTableClsCheck_->setChecked(false);
    exportCocoCheck_->setChecked(true);
    exportCheckedOnlyCheck_->setChecked(true);
    connect(saveRegionButton_, &QPushButton::clicked, this, &MainWindow::saveSelectedRegionText);
    connect(deleteRegionButton_, &QPushButton::clicked, this, &MainWindow::deleteSelectedRegion);
    connect(generateCropsButton, &QPushButton::clicked, this, &MainWindow::generateCurrentRecCrops);
    connect(confirmCurrentButton, &QPushButton::clicked, this, &MainWindow::confirmCurrentPageAnnotations);
    connect(confirmAllButton, &QPushButton::clicked, this, &MainWindow::confirmAllProjectAnnotations);
    connect(clearPageButton, &QPushButton::clicked, this, &MainWindow::clearCurrentPageAnnotations);
    connect(clearAllButton, &QPushButton::clicked, this, &MainWindow::clearAllAnnotations);
    connect(docOrientationCombo_, &QComboBox::currentTextChanged, this, &MainWindow::saveImageLabels);
    connect(textlineOrientationCombo_, &QComboBox::currentTextChanged, this, &MainWindow::saveImageLabels);
    connect(tableClassificationCombo_, &QComboBox::currentTextChanged, this, &MainWindow::saveImageLabels);

    auto* importButton = new QPushButton(QStringLiteral("导入图片/PDF"), inspector);
    auto* prelabelButton = new QPushButton(QStringLiteral("预标注当前页 OCR"), inspector);
    auto* prelabelAllButton = new QPushButton(QStringLiteral("预标注全部 OCR"), inspector);
    auto* prelabelClsButton = new QPushButton(QStringLiteral("预标注当前页方向"), inspector);
    auto* prelabelAllClsButton = new QPushButton(QStringLiteral("预标注全部方向"), inspector);
    auto* prelabelTableButton = new QPushButton(QStringLiteral("预标注当前页表格"), inspector);
    auto* prelabelAllTableButton = new QPushButton(QStringLiteral("预标注全部表格"), inspector);
    auto* prelabelLayoutButton = new QPushButton(QStringLiteral("预标注当前页版面"), inspector);
    auto* prelabelAllLayoutButton = new QPushButton(QStringLiteral("预标注全部版面"), inspector);
    auto* validateButton = new QPushButton(QStringLiteral("校验项目"), inspector);
    auto* exportButton = new QPushButton(QStringLiteral("导出数据集"), inspector);
    exportProjectButton_ = exportButton;
    connect(importButton, &QPushButton::clicked, this, &MainWindow::importImagesDialog);
    connect(prelabelButton, &QPushButton::clicked, this, &MainWindow::prelabelSelectedPage);
    connect(prelabelAllButton, &QPushButton::clicked, this, &MainWindow::prelabelAllPages);
    connect(prelabelClsButton, &QPushButton::clicked, this, &MainWindow::prelabelSelectedPageClassification);
    connect(prelabelAllClsButton, &QPushButton::clicked, this, &MainWindow::prelabelAllPagesClassification);
    connect(prelabelTableButton, &QPushButton::clicked, this, &MainWindow::prelabelSelectedPageTableClassification);
    connect(prelabelAllTableButton, &QPushButton::clicked, this, &MainWindow::prelabelAllPagesTableClassification);
    connect(prelabelLayoutButton, &QPushButton::clicked, this, &MainWindow::prelabelSelectedPageLayout);
    connect(prelabelAllLayoutButton, &QPushButton::clicked, this, &MainWindow::prelabelAllPagesLayout);
    connect(validateButton, &QPushButton::clicked, this, &MainWindow::validateProject);
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportProject);
    auto* tabs = new QTabWidget(inspector);

    auto* regionTab = new QWidget(tabs);
    auto* regionLayout = new QVBoxLayout(regionTab);
    auto* regionForm = new QFormLayout();
    regionForm->addRow(QStringLiteral("文本内容"), regionTextEdit_);
    regionForm->addRow(QString(), regionIgnoreCheck_);
    regionForm->addRow(QString(), regionCheckedCheck_);
    regionForm->addRow(QStringLiteral("阅读顺序"), regionReadingOrderSpin_);
    regionForm->addRow(QStringLiteral("版面类别"), layoutLabelCombo_);
    saveRegionButton_->setObjectName("Primary");
    deleteRegionButton_->setObjectName("Danger");
    regionLayout->addWidget(sectionLabel(QStringLiteral("文本框 / 版面框属性"), regionTab));
    regionLayout->addWidget(selectedRegionLabel_);
    regionLayout->addLayout(regionForm);
    regionLayout->addLayout(historyLayout);
    regionLayout->addWidget(saveRegionButton_);
    regionLayout->addWidget(generateCropsButton);
    regionLayout->addWidget(confirmCurrentButton);
    regionLayout->addWidget(confirmAllButton);
    regionLayout->addWidget(deleteRegionButton_);
    regionLayout->addWidget(clearPageButton);
    regionLayout->addWidget(clearAllButton);
    regionLayout->addStretch(1);

    auto* classTab = new QWidget(tabs);
    auto* classLayout = new QVBoxLayout(classTab);
    auto* classForm = new QFormLayout();
    classForm->addRow(QStringLiteral("文档方向"), docOrientationCombo_);
    auto* docHint = new QLabel(QStringLiteral("文档方向：整张图要不要旋转(以顺时针角度为准)"), classTab);
    docHint->setObjectName("Muted");
    classForm->addRow(QString(), docHint);
    classForm->addRow(QStringLiteral("文本行方向"), textlineOrientationCombo_);
    auto* textlineHint = new QLabel(QStringLiteral("文本行方向：文字行是不是上下颠倒，只管 0/180"), classTab);
    textlineHint->setObjectName("Muted");
    classForm->addRow(QString(), textlineHint);
    classForm->addRow(QStringLiteral("表格类型"), tableClassificationCombo_);
    auto* applyClassButton = new QPushButton(QStringLiteral("应用分类标签"), classTab);
    applyClassButton->setObjectName("Primary");
    connect(applyClassButton, &QPushButton::clicked, this, &MainWindow::saveImageLabels);
    classLayout->addWidget(sectionLabel(QStringLiteral("分类标注"), classTab));
    classLayout->addLayout(classForm);
    classLayout->addWidget(applyClassButton);
    classLayout->addStretch(1);

    auto* modelTab = new QWidget(tabs);
    auto* modelLayout = new QVBoxLayout(modelTab);
    modelLayout->addWidget(sectionLabel(QStringLiteral("本地模型管理"), modelTab));
    modelLayout->addWidget(importButton);
    modelLayout->addWidget(prelabelButton);
    modelLayout->addWidget(prelabelAllButton);
    modelLayout->addWidget(prelabelClsButton);
    modelLayout->addWidget(prelabelAllClsButton);
    modelLayout->addWidget(prelabelTableButton);
    modelLayout->addWidget(prelabelAllTableButton);
    modelLayout->addWidget(prelabelLayoutButton);
    modelLayout->addWidget(prelabelAllLayoutButton);
    auto* modelStatus = new QLabel(QStringLiteral("状态：使用预测页当前模型配置"), modelTab);
    modelStatus->setObjectName("Muted");
    modelLayout->addWidget(modelStatus);
    modelLayout->addStretch(1);

    auto* exportTab = new QWidget(tabs);
    auto* exportLayout = new QVBoxLayout(exportTab);
    exportCheckedOnlyCheck_->setText(QStringLiteral("只导出已确认标注 checked=true"));
    auto* exportHint = new QLabel(QStringLiteral("导出 PP-OCR Rec 时会自动生成 Rec Crop；勾选 checked-only 时，未确认框不会进入训练集。"), exportTab);
    exportHint->setObjectName("Muted");
    exportHint->setWordWrap(true);
    exportButton->setObjectName("Primary");
    exportLayout->addWidget(sectionLabel(QStringLiteral("导出设置"), exportTab));
    exportLayout->addWidget(exportDetCheck_);
    exportLayout->addWidget(exportRecCheck_);
    exportLayout->addWidget(exportClsCheck_);
    exportLayout->addWidget(exportTextlineClsCheck_);
    exportLayout->addWidget(exportTableClsCheck_);
    exportLayout->addWidget(exportCocoCheck_);
    exportLayout->addWidget(exportHint);
    exportLayout->addSpacing(16);
    exportLayout->addWidget(exportCheckedOnlyCheck_);
    exportProgressLabel_ = new QLabel(QStringLiteral("导出状态：空闲"), exportTab);
    exportProgressLabel_->setObjectName("Muted");
    exportProgressBar_ = new QProgressBar(exportTab);
    exportProgressBar_->setRange(0, 100);
    exportProgressBar_->setValue(0);
    exportProgressBar_->setTextVisible(true);
    exportLayout->addWidget(exportProgressLabel_);
    exportLayout->addWidget(exportProgressBar_);
    exportLayout->addWidget(exportButton);
    exportLayout->addStretch(1);

    auto* validationTab = new QWidget(tabs);
    auto* validationLayout = new QVBoxLayout(validationTab);
    validateButton->setObjectName("Primary");
    validationLayout->addWidget(sectionLabel(QStringLiteral("数据校验结果"), validationTab));
    validationLayout->addWidget(validateButton);
    auto* validationHint = new QLabel(QStringLiteral("校验结果会同步显示在概况页的“校验结果”表格中。"), validationTab);
    validationHint->setObjectName("Muted");
    validationHint->setWordWrap(true);
    validationLayout->addWidget(validationHint);
    validationLayout->addStretch(1);

    tabs->addTab(regionTab, QStringLiteral("属性"));
    tabs->addTab(classTab, QStringLiteral("分类"));
    tabs->addTab(modelTab, QStringLiteral("模型"));
    tabs->addTab(exportTab, QStringLiteral("导出"));
    tabs->addTab(validationTab, QStringLiteral("校验"));

    inspectorLayout->addWidget(sectionLabel(QStringLiteral("属性检查器"), inspector));
    inspectorLayout->addWidget(tabs, 1);
    inspectorScroll->setWidget(inspector);
    updateSelectedRegionPanel();
    updateAnnotationHistoryActions();

    splitter->addWidget(left);
    splitter->addWidget(center);
    splitter->addWidget(inspectorScroll);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({240, 820, 260});
    connect(pageList_, &QListWidget::currentRowChanged, this, &MainWindow::loadSelectedPage);
    layout->addWidget(header, 0);
    layout->addWidget(splitter, 1);
    return page;
}

QWidget* MainWindow::buildTrainingPage() {
    auto* page = new QWidget(this);
    page->setObjectName("TrainingRoot");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(10);

    trainingTaskCombo_ = new QComboBox(page);
    for (const auto& task : TrainingService::trainingTasks()) {
        trainingTaskCombo_->addItem(task.title, task.key);
    }
    trainingDeviceCombo_ = new QComboBox(page);
    trainingDeviceCombo_->setEditable(true);
    trainingDeviceCombo_->addItems({"cpu", "gpu:0"});
    trainingEpochsSpin_ = new QSpinBox(page);
    trainingEpochsSpin_->setRange(1, 100000);
    trainingBatchSpin_ = new QSpinBox(page);
    trainingBatchSpin_->setRange(1, 4096);
    trainingLearningRateSpin_ = new QDoubleSpinBox(page);
    trainingLearningRateSpin_->setDecimals(8);
    trainingLearningRateSpin_->setRange(0.0, 100.0);
    trainingLearningRateSpin_->setSingleStep(0.001);
    connect(trainingTaskCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::trainingTaskChanged);

    auto* screens = new QStackedWidget(page);
    trainingScreens_ = screens;

    auto* header = workbenchCard(page, QStringLiteral("HeaderPanel"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 10, 14, 10);
    headerLayout->setSpacing(12);
    auto* titleBox = new QVBoxLayout();
    titleBox->setSpacing(3);
    auto* title = new QLabel(QStringLiteral("训练任务"), header);
    title->setObjectName("PageTitle");
    auto* subtitle = mutedLabel(QStringLiteral("查看所有训练任务，点击卡片进入详情并查看每次训练记录"), header);
    titleBox->addWidget(title);
    titleBox->addWidget(subtitle);
    trainingHeaderProjectLabel_ = mutedLabel(QStringLiteral("当前项目：未打开"), header);
    trainingHeaderStatusLabel_ = new QLabel(QStringLiteral("状态：未启动"), header);
    trainingHeaderStatusLabel_->setObjectName("StatusOk");
    auto* versionManageButton = new QPushButton(QStringLiteral("模型版本"), header);
    auto* newTaskButton = primaryButton(QStringLiteral("进入训练配置"), header);
    auto* backToListButton = new QPushButton(QStringLiteral("任务总览"), header);
    connect(versionManageButton, &QPushButton::clicked, this, [screens]() { screens->setCurrentIndex(2); });
    connect(newTaskButton, &QPushButton::clicked, this, [screens]() { screens->setCurrentIndex(1); });
    connect(backToListButton, &QPushButton::clicked, this, [screens]() { screens->setCurrentIndex(0); });
    headerLayout->addLayout(titleBox, 1);
    headerLayout->addWidget(trainingHeaderProjectLabel_, 1);
    headerLayout->addWidget(trainingHeaderStatusLabel_);
    headerLayout->addWidget(versionManageButton);
    headerLayout->addWidget(newTaskButton);
    headerLayout->addWidget(backToListButton);

    auto* listScreen = new QWidget(screens);
    auto* listLayout = new QVBoxLayout(listScreen);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(10);
    auto* filterRow = new QHBoxLayout();
    trainingTaskSearchEdit_ = new QLineEdit(listScreen);
    trainingTaskSearchEdit_->setPlaceholderText(QStringLiteral("搜索任务名称、数据集或备注"));
    trainingTaskTypeFilter_ = new QComboBox(listScreen);
    trainingTaskTypeFilter_->addItem(QStringLiteral("任务类型：全部"), QString());
    trainingTaskTypeFilter_->addItem(QStringLiteral("Det"), QStringLiteral("det"));
    trainingTaskTypeFilter_->addItem(QStringLiteral("Rec"), QStringLiteral("rec"));
    trainingTaskTypeFilter_->addItem(QStringLiteral("Cls"), QStringLiteral("cls"));
    trainingTaskTypeFilter_->addItem(QStringLiteral("Layout"), QStringLiteral("layout"));
    trainingTaskTypeFilter_->addItem(QStringLiteral("UVDoc"), QStringLiteral("uvdoc"));
    trainingTaskStatusFilter_ = new QComboBox(listScreen);
    trainingTaskStatusFilter_->addItem(QStringLiteral("状态：全部"), QString());
    trainingTaskStatusFilter_->addItem(QStringLiteral("未开始"), QStringLiteral("idle"));
    trainingTaskStatusFilter_->addItem(QStringLiteral("进行中"), QStringLiteral("running"));
    trainingTaskStatusFilter_->addItem(QStringLiteral("已完成"), QStringLiteral("success"));
    trainingTaskStatusFilter_->addItem(QStringLiteral("失败"), QStringLiteral("failed"));
    trainingTaskStatusFilter_->addItem(QStringLiteral("已停止"), QStringLiteral("stopped"));
    trainingTaskStatusFilter_->addItem(QStringLiteral("暂不支持"), QStringLiteral("unsupported"));
    trainingTaskSortCombo_ = new QComboBox(listScreen);
    trainingTaskSortCombo_->addItem(QStringLiteral("排序：最近训练时间"), QStringLiteral("recent"));
    trainingTaskSortCombo_->addItem(QStringLiteral("排序：任务类型"), QStringLiteral("kind"));
    trainingTaskSortCombo_->addItem(QStringLiteral("排序：名称"), QStringLiteral("name"));
    auto* refreshButton = new QPushButton(QStringLiteral("刷新"), listScreen);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshTrainingVersions);
    connect(trainingTaskSearchEdit_, &QLineEdit::textChanged, this, &MainWindow::refreshTrainingTaskOverview);
    connect(trainingTaskTypeFilter_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::refreshTrainingTaskOverview);
    connect(trainingTaskStatusFilter_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::refreshTrainingTaskOverview);
    connect(trainingTaskSortCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::refreshTrainingTaskOverview);
    filterRow->addWidget(trainingTaskSearchEdit_, 2);
    filterRow->addWidget(trainingTaskTypeFilter_);
    filterRow->addWidget(trainingTaskStatusFilter_);
    filterRow->addStretch(1);
    filterRow->addWidget(trainingTaskSortCombo_);
    filterRow->addWidget(refreshButton);
    listLayout->addLayout(filterRow);

    auto* summaryGrid = new QGridLayout();
    summaryGrid->setSpacing(10);
    auto addSummary = [listScreen, summaryGrid](int column, const QString& title, const QString& value, const QString& subtitle, const QColor& color) {
        auto* card = workbenchCard(listScreen, QStringLiteral("MetricPanel"));
        auto* cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(12, 10, 12, 10);
        auto* icon = new QLabel(QStringLiteral("□"), card);
        icon->setObjectName("OverviewMetricIcon");
        icon->setStyleSheet(QStringLiteral("background:%1;").arg(color.name()));
        auto* textBox = new QVBoxLayout();
        textBox->addWidget(mutedLabel(title, card));
        auto* valueLabel = new QLabel(value, card);
        valueLabel->setObjectName("MetricValue");
        textBox->addWidget(valueLabel);
        textBox->addWidget(mutedLabel(subtitle, card));
        cardLayout->addWidget(icon);
        cardLayout->addLayout(textBox, 1);
        summaryGrid->addWidget(card, 0, column);
        return valueLabel;
    };
    trainingTotalTasksMetric_ = addSummary(0, QStringLiteral("任务总数"), QString::number(TrainingService::trainingTasks().size()), QStringLiteral("所有训练任务"), QColor("#2563eb"));
    trainingRunningTasksMetric_ = addSummary(1, QStringLiteral("进行中"), QStringLiteral("0"), QStringLiteral("正在训练"), QColor("#14786d"));
    trainingDoneTasksMetric_ = addSummary(2, QStringLiteral("已完成"), QStringLiteral("0"), QStringLiteral("已有成功版本"), QColor("#46d39a"));
    trainingFailedTasksMetric_ = addSummary(3, QStringLiteral("失败任务"), QStringLiteral("0"), QStringLiteral("最近一次失败"), QColor("#b0444d"));
    trainingTodayTasksMetric_ = addSummary(4, QStringLiteral("今日新增"), QStringLiteral("0"), QStringLiteral("今天有输出"), QColor("#d79b35"));
    listLayout->addLayout(summaryGrid);

    auto* taskScroll = new QScrollArea(listScreen);
    taskScroll->setWidgetResizable(true);
    taskScroll->setFrameShape(QFrame::NoFrame);
    trainingTaskHost_ = new QWidget(taskScroll);
    trainingTaskGrid_ = new QGridLayout(trainingTaskHost_);
    trainingTaskGrid_->setContentsMargins(0, 0, 0, 0);
    trainingTaskGrid_->setSpacing(10);
    trainingTaskHost_->installEventFilter(this);
    taskScroll->setWidget(trainingTaskHost_);
    listLayout->addWidget(taskScroll, 1);

    auto* detailScreen = new QWidget(screens);
    auto* detailLayout = new QHBoxLayout(detailScreen);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->setSpacing(12);
    auto* leftScroll = new QScrollArea(detailScreen);
    leftScroll->setWidgetResizable(true);
    leftScroll->setFrameShape(QFrame::NoFrame);
    auto* left = new QWidget(leftScroll);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(10);

    auto* versionPanel = workbenchCard(left, QStringLiteral("TrainingCard"));
    auto* versionLayout = new QHBoxLayout(versionPanel);
    versionLayout->setContentsMargins(12, 10, 12, 10);
    auto* versionIntro = new QVBoxLayout();
    versionIntro->addWidget(sectionLabel(QStringLiteral("训练版本"), versionPanel));
    versionIntro->addWidget(mutedLabel(QStringLiteral("每次训练都会生成独立版本，便于追溯与对比。"), versionPanel));
    auto* newVersionButton = primaryButton(QStringLiteral("开始新训练"), versionPanel);
    newVersionButton->setToolTip(QStringLiteral("用当前参数启动训练，并自动生成一个新的模型版本。"));
    connect(newVersionButton, &QPushButton::clicked, this, &MainWindow::startTraining);
    versionIntro->addWidget(newVersionButton);
    trainingVersionList_ = new QListWidget(page);
    trainingVersionList_->setObjectName("TrainingVersionList");
    trainingVersionList_->setFlow(QListView::LeftToRight);
    trainingVersionList_->setWrapping(false);
    trainingVersionList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    trainingVersionList_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    trainingVersionList_->setMinimumHeight(112);
    connect(trainingVersionList_, &QListWidget::currentRowChanged, this, &MainWindow::updateSelectedTrainingVersionDetails);
    versionLayout->addLayout(versionIntro);
    versionLayout->addWidget(trainingVersionList_, 1);

    auto* selectedVersionPanel = workbenchCard(left, QStringLiteral("TrainingCard"));
    auto* selectedLayout = new QHBoxLayout(selectedVersionPanel);
    selectedLayout->setContentsMargins(12, 12, 12, 12);
    trainingVersionSummary_ = new QLabel(QStringLiteral("当前版本\n-"), selectedVersionPanel);
    trainingVersionSummary_->setObjectName("TrainingVersionSummary");
    auto* compareButton = new QPushButton(QStringLiteral("对比版本"), selectedVersionPanel);
    auto* setCurrentButton = primaryButton(QStringLiteral("设为当前版本"), selectedVersionPanel);
    auto* openVersionButton = new QPushButton(QStringLiteral("打开目录"), selectedVersionPanel);
    auto* deleteVersionButton = dangerButton(QStringLiteral("删除版本"), selectedVersionPanel);
    connect(compareButton, &QPushButton::clicked, this, &MainWindow::compareSelectedTrainingVersions);
    connect(setCurrentButton, &QPushButton::clicked, this, &MainWindow::setSelectedTrainingVersionCurrent);
    connect(openVersionButton, &QPushButton::clicked, this, &MainWindow::openSelectedTrainingVersionDir);
    connect(deleteVersionButton, &QPushButton::clicked, this, &MainWindow::deleteSelectedTrainingVersion);
    selectedLayout->addWidget(trainingVersionSummary_, 1);
    selectedLayout->addWidget(compareButton);
    selectedLayout->addWidget(setCurrentButton);
    selectedLayout->addWidget(openVersionButton);
    selectedLayout->addWidget(deleteVersionButton);

    auto* progressPanel = workbenchCard(left, QStringLiteral("TrainingCard"));
    auto* progressLayout = new QVBoxLayout(progressPanel);
    progressLayout->setContentsMargins(12, 10, 12, 10);
    progressLayout->addWidget(sectionLabel(QStringLiteral("训练进度"), progressPanel));
    trainingProgressMetaLabel_ = mutedLabel(QStringLiteral("任务名称：-        开始时间：-        当前轮数：0 / 20 (0%)        预计剩余时间：-        状态：未启动"), progressPanel);
    trainingProgressBar_ = new QProgressBar(progressPanel);
    trainingProgressBar_->setRange(0, 100);
    trainingProgressBar_->setValue(0);
    trainingProgressBar_->setTextVisible(true);
    progressLayout->addWidget(trainingProgressMetaLabel_);
    progressLayout->addWidget(trainingProgressBar_);

    auto* chartPanel = workbenchCard(left, QStringLiteral("TrainingCard"));
    auto* chartLayout = new QVBoxLayout(chartPanel);
    chartLayout->setContentsMargins(12, 10, 12, 10);
    chartLayout->addWidget(sectionLabel(QStringLiteral("训练指标曲线"), chartPanel));
    auto* chartRow = new QHBoxLayout();
    trainingLossChart_ = new TrainingPlaceholderChart(QStringLiteral("Loss 曲线"), chartPanel);
    trainingAccuracyChart_ = new TrainingPlaceholderChart(QStringLiteral("准确率 / 核心指标"), chartPanel);
    trainingLrChart_ = new TrainingPlaceholderChart(QStringLiteral("学习率 (LR)"), chartPanel);
    chartRow->addWidget(trainingLossChart_, 1);
    chartRow->addWidget(trainingAccuracyChart_, 1);
    chartRow->addWidget(trainingLrChart_, 1);
    chartLayout->addLayout(chartRow, 1);

    auto* reportPanel = workbenchCard(left, QStringLiteral("TrainingCard"));
    auto* reportLayout = new QVBoxLayout(reportPanel);
    reportLayout->setContentsMargins(12, 10, 12, 10);
    auto* reportHeader = new QHBoxLayout();
    reportHeader->addWidget(sectionLabel(QStringLiteral("训练指标报表"), reportPanel));
    reportHeader->addStretch(1);
    auto* exportMetricsButton = new QPushButton(QStringLiteral("导出报表 CSV"), reportPanel);
    auto* clearMetricsButton = new QPushButton(QStringLiteral("清空报表"), reportPanel);
    connect(exportMetricsButton, &QPushButton::clicked, this, &MainWindow::exportTrainingMetricsCsv);
    connect(clearMetricsButton, &QPushButton::clicked, this, &MainWindow::clearTrainingMetrics);
    reportHeader->addWidget(exportMetricsButton);
    reportHeader->addWidget(clearMetricsButton);
    trainingMetricSummaryLabel_ = mutedLabel(QStringLiteral("指标：0 行 | 最新 score -- | 最新 loss --"), reportPanel);
    trainingMetricsTable_ = tableWidget(9, {
        QStringLiteral("时间"),
        QStringLiteral("epoch"),
        QStringLiteral("step"),
        QStringLiteral("loss"),
        QStringLiteral("lr"),
        QStringLiteral("acc"),
        QStringLiteral("score"),
        QStringLiteral("P/R"),
        QStringLiteral("原始日志"),
    }, reportPanel);
    trainingPreview_ = new QPlainTextEdit(reportPanel);
    trainingPreview_->setReadOnly(true);
    trainingPreview_->setMinimumHeight(130);
    reportLayout->addLayout(reportHeader);
    reportLayout->addWidget(trainingMetricSummaryLabel_);
    reportLayout->addWidget(trainingMetricsTable_, 1);
    reportLayout->addWidget(mutedLabel(QStringLiteral("训练日志"), reportPanel));
    reportLayout->addWidget(trainingPreview_);

    auto* detailTabs = new QTabWidget(left);
    detailTabs->setDocumentMode(true);
    auto* overviewTab = new QWidget(detailTabs);
    auto* overviewLayout = new QVBoxLayout(overviewTab);
    overviewLayout->setContentsMargins(0, 0, 0, 0);
    overviewLayout->setSpacing(10);
    overviewLayout->addWidget(versionPanel);
    overviewLayout->addWidget(selectedVersionPanel);
    overviewLayout->addWidget(progressPanel);
    overviewLayout->addWidget(chartPanel, 1);
    overviewLayout->addStretch(1);
    auto* reportTab = new QWidget(detailTabs);
    auto* reportTabLayout = new QVBoxLayout(reportTab);
    reportTabLayout->setContentsMargins(0, 0, 0, 0);
    reportTabLayout->addWidget(reportPanel, 1);
    detailTabs->addTab(overviewTab, QStringLiteral("概览"));
    detailTabs->addTab(reportTab, QStringLiteral("指标 / 日志"));
    leftLayout->addWidget(detailTabs, 1);
    leftScroll->setWidget(left);

    auto* rightScroll = new QScrollArea(detailScreen);
    rightScroll->setWidgetResizable(true);
    rightScroll->setFrameShape(QFrame::NoFrame);
    rightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rightScroll->setMinimumWidth(330);
    rightScroll->setMaximumWidth(500);
    auto* rightPanel = workbenchCard(rightScroll, QStringLiteral("InspectorPanel"));
    rightPanel->setMinimumWidth(310);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(10, 10, 10, 10);
    auto* configTabs = new QTabWidget(rightPanel);
    auto* configTab = new QWidget(configTabs);
    auto* configLayout = new QVBoxLayout(configTab);
    configLayout->addWidget(sectionLabel(QStringLiteral("训练配置"), configTab));
    auto* setupHint = new QLabel(QStringLiteral("推荐流程：检查数据集 → 预览命令 → 开始训练并生成版本。每次训练都会保留独立版本。"), configTab);
    setupHint->setObjectName("TrainingSetupHint");
    setupHint->setWordWrap(true);
    configLayout->addWidget(setupHint);
    auto* basicTierTitle = new QLabel(QStringLiteral("基础参数"), configTab);
    basicTierTitle->setObjectName("ParameterTierTitle");
    configLayout->addWidget(basicTierTitle);
    auto* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(8);
    trainingTaskCombo_->setToolTip(QStringLiteral("选择要训练的模型模板和对应数据集类型。"));
    trainingDeviceCombo_->setToolTip(QStringLiteral("cpu 更稳但慢；gpu:0 速度更快，需要本机 Paddle GPU 环境可用。"));
    trainingEpochsSpin_->setToolTip(QStringLiteral("完整遍历训练集的轮数。小数据集可先用较小轮数做烟测。"));
    trainingBatchSpin_->setToolTip(QStringLiteral("每批训练样本数。显存不足时请降低这个值，例如 4 或 8。"));
    trainingLearningRateSpin_->setToolTip(QStringLiteral("学习率。除非你确定模型需要调参，一般使用任务默认值。"));
    form->addRow(QStringLiteral("模型模板"), trainingTaskCombo_);
    form->addRow(QStringLiteral("训练设备"), trainingDeviceCombo_);
    form->addRow(QStringLiteral("训练轮数"), trainingEpochsSpin_);
    form->addRow(QStringLiteral("批大小"), trainingBatchSpin_);
    form->addRow(QStringLiteral("学习率"), trainingLearningRateSpin_);
    trainingNumClassesSpin_ = new QSpinBox(configTab);
    auto* numClassesSpin = trainingNumClassesSpin_;
    numClassesSpin->setRange(0, 99999);
    trainingWarmupSpin_ = new QSpinBox(configTab);
    auto* warmupSpin = trainingWarmupSpin_;
    warmupSpin->setRange(0, 999999);
    numClassesSpin->setToolTip(QStringLiteral("分类任务的类别数。检测/识别任务通常保持 0，使用模型默认配置。"));
    warmupSpin->setToolTip(QStringLiteral("学习率预热步数。保持默认即可。"));
    form->addRow(QStringLiteral("类别数"), numClassesSpin);
    form->addRow(QStringLiteral("预热步数"), warmupSpin);
    auto* advancedTierTitle = new QLabel(QStringLiteral("高级参数"), configTab);
    advancedTierTitle->setObjectName("ParameterTierTitle");
    form->insertRow(4, advancedTierTitle);
    configLayout->addLayout(form);
    auto* resumeRow = new QHBoxLayout();
    trainingResumePathEdit_ = new QLineEdit(configTab);
    auto* resumeEdit = trainingResumePathEdit_;
    auto* resumeBrowse = new QPushButton(QStringLiteral("浏览"), configTab);
    connect(resumeBrowse, &QPushButton::clicked, this, &MainWindow::browseTrainingResumePath);
    resumeRow->addWidget(resumeEdit, 1);
    resumeRow->addWidget(resumeBrowse);
    configLayout->addWidget(sectionLabel(QStringLiteral("高级参数"), configTab));
    configLayout->addWidget(mutedLabel(QStringLiteral("resume_path（断点续训权重）"), configTab));
    configLayout->addLayout(resumeRow);
    auto* expertTierTitle = new QLabel(QStringLiteral("专家模式"), configTab);
    expertTierTitle->setObjectName("ParameterTierTitle");
    configLayout->addWidget(expertTierTitle);
    auto* commandPreviewButton = new QPushButton(QStringLiteral("预览命令"), configTab);
    auto* checkDatasetButton = new QPushButton(QStringLiteral("检查数据集"), configTab);
    auto* copyCommandButton = new QPushButton(QStringLiteral("复制命令"), configTab);
    commandPreviewButton->setToolTip(QStringLiteral("查看即将执行的 PaddleX 命令、数据集和输出目录。"));
    checkDatasetButton->setToolTip(QStringLiteral("训练前检查数据集、配置、预训练权重和环境路径。"));
    copyCommandButton->setToolTip(QStringLiteral("复制当前训练命令，方便在终端复现。"));
    connect(commandPreviewButton, &QPushButton::clicked, this, &MainWindow::previewTrainingCommand);
    connect(checkDatasetButton, &QPushButton::clicked, this, &MainWindow::checkTrainingSetup);
    connect(copyCommandButton, &QPushButton::clicked, this, &MainWindow::copyTrainingCommand);
    auto* commandRow = new QHBoxLayout();
    commandRow->addWidget(commandPreviewButton);
    commandRow->addWidget(checkDatasetButton);
    commandRow->addWidget(copyCommandButton);
    configLayout->addLayout(commandRow);
    trainingExpertPreview_ = new QPlainTextEdit(configTab);
    trainingExpertPreview_->setObjectName("ExpertPreview");
    trainingExpertPreview_->setReadOnly(true);
    trainingExpertPreview_->setMinimumHeight(132);
    trainingExpertPreview_->setPlaceholderText(QStringLiteral("预检报告、PaddleX 命令和 resolved config 预览会显示在这里。"));
    configLayout->addWidget(trainingExpertPreview_);
    startTrainingButton_ = primaryButton(QStringLiteral("开始训练并生成版本"), configTab);
    startTrainingButton_->setToolTip(QStringLiteral("启动训练，并在当前项目下创建一个独立训练版本。"));
    stopTrainingButton_ = dangerButton(QStringLiteral("停止"), configTab);
    connect(startTrainingButton_, &QPushButton::clicked, this, &MainWindow::startTraining);
    connect(stopTrainingButton_, &QPushButton::clicked, this, &MainWindow::stopTraining);
    auto* runRow = new QHBoxLayout();
    runRow->addWidget(startTrainingButton_, 1);
    runRow->addWidget(stopTrainingButton_);
    configLayout->addLayout(runRow);
    configLayout->addStretch(1);

    auto* exportTab = new QWidget(configTabs);
    auto* exportLayout = new QVBoxLayout(exportTab);
    exportLayout->addWidget(sectionLabel(QStringLiteral("模型导出"), exportTab));
    auto* exportWeightRow = new QHBoxLayout();
    trainingExportWeightEdit_ = new QLineEdit(exportTab);
    trainingExportWeightEdit_->setPlaceholderText(QStringLiteral("自动使用选中版本的 best 权重"));
    exportWeightRow->addWidget(trainingExportWeightEdit_, 1);
    auto* exportWeightBrowse = new QPushButton(QStringLiteral("浏览"), exportTab);
    connect(exportWeightBrowse, &QPushButton::clicked, this, &MainWindow::browseTrainingExportWeight);
    exportWeightRow->addWidget(exportWeightBrowse);
    auto* exportDirRow = new QHBoxLayout();
    trainingExportDirEdit_ = new QLineEdit(exportTab);
    trainingExportDirEdit_->setPlaceholderText(QStringLiteral("自动导出到 project/model_exports/task/version"));
    exportDirRow->addWidget(trainingExportDirEdit_, 1);
    auto* exportDirBrowse = new QPushButton(QStringLiteral("浏览"), exportTab);
    connect(exportDirBrowse, &QPushButton::clicked, this, &MainWindow::browseTrainingExportDir);
    exportDirRow->addWidget(exportDirBrowse);
    exportLayout->addWidget(mutedLabel(QStringLiteral("导出权重"), exportTab));
    exportLayout->addLayout(exportWeightRow);
    exportLayout->addWidget(mutedLabel(QStringLiteral("导出目录"), exportTab));
    exportLayout->addLayout(exportDirRow);
    auto* exportModelButton = primaryButton(QStringLiteral("导出选中版本模型"), exportTab);
    connect(exportModelButton, &QPushButton::clicked, this, &MainWindow::exportSelectedTrainingModel);
    exportLayout->addWidget(exportModelButton);
    exportLayout->addStretch(1);
    configTabs->addTab(configTab, QStringLiteral("训练配置"));
    configTabs->addTab(exportTab, QStringLiteral("模型导出"));
    trainingVersionDetail_ = new QPlainTextEdit(rightPanel);
    trainingVersionDetail_->setReadOnly(true);
    trainingVersionDetail_->setMaximumHeight(150);
    trainingVersionDetail_->setPlainText(QStringLiteral("未选择版本"));
    rightLayout->addWidget(configTabs, 1);
    rightLayout->addWidget(sectionLabel(QStringLiteral("版本对比"), rightPanel));
    rightLayout->addWidget(trainingVersionDetail_);
    rightScroll->setWidget(rightPanel);

    detailLayout->addWidget(leftScroll, 1);
    detailLayout->addWidget(rightScroll);

    auto* versionScreen = new QWidget(screens);
    auto* versionScreenLayout = new QVBoxLayout(versionScreen);
    versionScreenLayout->setContentsMargins(0, 0, 0, 0);
    auto* versionHeader = new QHBoxLayout();
    versionHeader->addWidget(sectionLabel(QStringLiteral("模型版本"), versionScreen));
    versionHeader->addStretch(1);
    auto* refreshVersionsButton = new QPushButton(QStringLiteral("刷新"), versionScreen);
    auto* setVersionButton = primaryButton(QStringLiteral("设为当前"), versionScreen);
    auto* openDirButton = new QPushButton(QStringLiteral("打开目录"), versionScreen);
    auto* deleteButton = dangerButton(QStringLiteral("删除版本"), versionScreen);
    connect(refreshVersionsButton, &QPushButton::clicked, this, &MainWindow::refreshTrainingVersions);
    connect(setVersionButton, &QPushButton::clicked, this, &MainWindow::setSelectedTrainingVersionCurrent);
    connect(openDirButton, &QPushButton::clicked, this, &MainWindow::openSelectedTrainingVersionDir);
    connect(deleteButton, &QPushButton::clicked, this, &MainWindow::deleteSelectedTrainingVersion);
    versionHeader->addWidget(refreshVersionsButton);
    versionHeader->addWidget(setVersionButton);
    versionHeader->addWidget(openDirButton);
    versionHeader->addWidget(deleteButton);
    auto* versionInfo = mutedLabel(QStringLiteral("每次训练都会生成独立版本目录；当前版本用于默认导出和预测，best 版本由主要指标自动维护。"), versionScreen);
    versionInfo->setWordWrap(true);
    trainingVersionManagerTable_ = tableWidget(10, {
        QStringLiteral("模型模板"),
        QStringLiteral("版本"),
        QStringLiteral("状态"),
        QStringLiteral("当前"),
        QStringLiteral("Best"),
        QStringLiteral("开始时间"),
        QStringLiteral("结束时间"),
        QStringLiteral("主要指标"),
        QStringLiteral("样本数"),
        QStringLiteral("目录"),
    }, versionScreen);
    connect(trainingVersionManagerTable_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (!trainingVersionManagerTable_) {
            return;
        }
        const auto* taskItem = trainingVersionManagerTable_->item(row, 0);
        if (!taskItem) {
            return;
        }
        selectTrainingTaskAndShowDetail(taskItem->data(Qt::UserRole).toString());
    });
    versionScreenLayout->addLayout(versionHeader);
    versionScreenLayout->addWidget(versionInfo);
    versionScreenLayout->addWidget(trainingVersionManagerTable_, 1);

    screens->addWidget(listScreen);
    screens->addWidget(detailScreen);
    screens->addWidget(versionScreen);
    connect(screens, &QStackedWidget::currentChanged, this, [this](int index) {
        if (index == 0) {
            queueTrainingTaskOverviewRefresh();
        }
    });
    layout->addWidget(header);
    layout->addWidget(screens, 1);
    applyTrainingTaskDefaults();
    setTrainingRunning(false);
    refreshTrainingVersions();
    return page;
}

QWidget* MainWindow::buildPredictionPage() {
    auto* page = new QWidget(this);
    page->setObjectName("PredictionRoot");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(10);

    predictInputEdit_ = new QLineEdit(page);
    predictOutputEdit_ = new QLineEdit(page);
    predictOutputEdit_->setText(defaultPredictionOutputDir());
    predictDetModelCombo_ = new QComboBox(page);
    predictRecModelCombo_ = new QComboBox(page);
    predictClsTaskCombo_ = new QComboBox(page);
    predictClsTaskCombo_->addItem(QStringLiteral("文档方向"), "doc_orientation");
    predictClsTaskCombo_->addItem(QStringLiteral("文本行方向"), "textline_orientation");
    predictClsTaskCombo_->addItem(QStringLiteral("表格分类"), "table_classification");
    predictClsModelCombo_ = new QComboBox(page);
    predictLayoutModelCombo_ = new QComboBox(page);
    predictDeviceCombo_ = new QComboBox(page);
    predictDeviceCombo_->setEditable(true);
    predictDeviceCombo_->addItems({"cpu", "gpu:0"});
    predictScoreThresholdSpin_ = new QDoubleSpinBox(page);
    predictScoreThresholdSpin_->setRange(0.0, 1.0);
    predictScoreThresholdSpin_->setDecimals(2);
    predictScoreThresholdSpin_->setSingleStep(0.05);
    predictScoreThresholdSpin_->setValue(0.50);
    predictSaveJsonCheck_ = new QCheckBox(QStringLiteral("保存 JSON"), page);
    predictSaveJsonCheck_->setChecked(true);
    predictSaveVisualCheck_ = new QCheckBox(QStringLiteral("保存可视化"), page);
    predictSaveVisualCheck_->setChecked(true);

    auto* header = workbenchCard(page, QStringLiteral("HeaderPanel"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 10, 14, 10);
    auto* titleBox = new QVBoxLayout();
    titleBox->setSpacing(3);
    auto* title = new QLabel(QStringLiteral("预测工作台"), header);
    title->setObjectName("PredictionTitle");
    titleBox->addWidget(title);
    titleBox->addWidget(mutedLabel(QStringLiteral("预测 / Det 检测 | PP-OCRv5 server"), header));
    auto* projectText = mutedLabel(QStringLiteral("当前项目：未打开"), header);
    auto* statusText = new QLabel(QStringLiteral("状态：未启动"), header);
    statusText->setObjectName("StatusOk");
    headerLayout->addLayout(titleBox, 1);
    headerLayout->addWidget(projectText, 1);
    headerLayout->addWidget(statusText);

    auto* modelStrip = workbenchCard(page, QStringLiteral("WorkbenchPanel"));
    modelStrip->setMaximumHeight(172);
    auto* stripLayout = new QVBoxLayout(modelStrip);
    stripLayout->setContentsMargins(12, 9, 12, 9);
    stripLayout->setSpacing(8);
    auto* stripHeader = new QHBoxLayout();
    stripHeader->addWidget(sectionLabel(QStringLiteral("已完成模型选择"), modelStrip));
    stripHeader->addWidget(mutedLabel(QStringLiteral("仅展示训练完成且可用于推理的模型"), modelStrip));
    stripHeader->addStretch(1);
    predictionModelSearchEdit_ = new QLineEdit(modelStrip);
    auto* modelSearch = predictionModelSearchEdit_;
    modelSearch->setPlaceholderText(QStringLiteral("搜索模型名称"));
    predictionModelKindFilter_ = new QComboBox(modelStrip);
    auto* modelType = predictionModelKindFilter_;
    modelType->addItems({QStringLiteral("模型类型：全部"), QStringLiteral("Det"), QStringLiteral("Rec"), QStringLiteral("Cls"), QStringLiteral("Layout")});
    auto* refreshModelStripButton = new QPushButton(QStringLiteral("刷新"), modelStrip);
    connect(refreshModelStripButton, &QPushButton::clicked, this, &MainWindow::refreshPredictionModels);
    for (int i = 0; i < predictionModelKindFilter_->count(); ++i) {
        const QString text = predictionModelKindFilter_->itemText(i).toLower();
        if (text.contains(QStringLiteral("det"))) {
            predictionModelKindFilter_->setItemData(i, QStringLiteral("det"));
        } else if (text.contains(QStringLiteral("rec"))) {
            predictionModelKindFilter_->setItemData(i, QStringLiteral("rec"));
        } else if (text.contains(QStringLiteral("cls"))) {
            predictionModelKindFilter_->setItemData(i, QStringLiteral("cls"));
        } else if (text.contains(QStringLiteral("layout"))) {
            predictionModelKindFilter_->setItemData(i, QStringLiteral("layout"));
        } else {
            predictionModelKindFilter_->setItemData(i, QString());
        }
    }
    connect(predictionModelSearchEdit_, &QLineEdit::textChanged, this, &MainWindow::refreshPredictionModelCards);
    connect(predictionModelKindFilter_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::refreshPredictionModelCards);
    stripHeader->addWidget(predictionModelSearchEdit_);
    stripHeader->addWidget(predictionModelKindFilter_);
    stripHeader->addWidget(refreshModelStripButton);

    predictionModelHost_ = new QWidget(modelStrip);
    predictionModelHost_->setObjectName("PredictionModelHost");
    predictionModelLayout_ = new QHBoxLayout(predictionModelHost_);
    predictionModelLayout_->setContentsMargins(0, 0, 0, 0);
    predictionModelLayout_->setSpacing(8);
    auto* modelRow = predictionModelLayout_;
    auto addModelTile = [modelStrip, modelRow](const QString& name, const QString& badge, const QString& dataset, const QString& path) {
        auto* tile = workbenchCard(modelStrip, QStringLiteral("PredictionModelTile"));
        tile->setMinimumWidth(220);
        auto* tileLayout = new QVBoxLayout(tile);
        tileLayout->setContentsMargins(12, 10, 12, 10);
        auto* top = new QHBoxLayout();
        auto* nameLabel = new QLabel(name, tile);
        nameLabel->setObjectName("TrainingTaskTitle");
        auto* badgeLabel = new QLabel(badge, tile);
        badgeLabel->setObjectName("TrainingBadge");
        top->addWidget(nameLabel, 1);
        top->addWidget(badgeLabel);
        tileLayout->addLayout(top);
        tileLayout->addWidget(mutedLabel(QStringLiteral("数据集：%1").arg(dataset), tile));
        tileLayout->addWidget(mutedLabel(QStringLiteral("模型目录：%1").arg(path), tile));
        auto* status = new QLabel(QStringLiteral("已完成    inference"), tile);
        status->setObjectName("TrainingStatusDone");
        tileLayout->addWidget(status);
        modelRow->addWidget(tile);
    };
    addModelTile(QStringLiteral("rec_v5_server"), QStringLiteral("Rec"), QStringLiteral("ppocr_rec"), QStringLiteral("training\\rec_v5_server"));
    addModelTile(QStringLiteral("det_v5_server"), QStringLiteral("Det"), QStringLiteral("ppocr_det"), QStringLiteral("training\\det_v5_server"));
    modelRow->addStretch(1);
    auto* modelScroll = new QScrollArea(modelStrip);
    modelScroll->setObjectName("PredictionModelScroll");
    modelScroll->setWidgetResizable(false);
    modelScroll->setFrameShape(QFrame::NoFrame);
    modelScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    modelScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    modelScroll->setMinimumHeight(108);
    modelScroll->setWidget(predictionModelHost_);
    stripLayout->addLayout(stripHeader);
    stripLayout->addWidget(modelScroll);

    auto* inputRow = new QHBoxLayout();
    auto* imageButton = new QPushButton(QStringLiteral("选择图片"), page);
    auto* dirButton = new QPushButton(QStringLiteral("选择目录"), page);
    auto* pasteButton = new QPushButton(QStringLiteral("粘贴图片"), page);
    auto* clearInputButton = new QPushButton(QStringLiteral("清空"), page);
    connect(imageButton, &QPushButton::clicked, this, &MainWindow::browsePredictInputFile);
    connect(dirButton, &QPushButton::clicked, this, &MainWindow::browsePredictInputDir);
    connect(pasteButton, &QPushButton::clicked, this, &MainWindow::pastePredictImageFromClipboard);
    connect(clearInputButton, &QPushButton::clicked, predictInputEdit_, &QLineEdit::clear);
    connect(clearInputButton, &QPushButton::clicked, this, [this]() { updatePredictionImagePreview(QString()); });
    connect(predictInputEdit_, &QLineEdit::editingFinished, this, [this]() {
        updatePredictionImagePreview(predictInputEdit_ ? predictInputEdit_->text().trimmed() : QString());
    });
    inputRow->addWidget(imageButton);
    inputRow->addWidget(dirButton);
    inputRow->addWidget(pasteButton);
    inputRow->addWidget(clearInputButton);
    predictInputEdit_->setPlaceholderText(QStringLiteral("选择图片文件或图片目录"));
    inputRow->addWidget(predictInputEdit_, 1);

    auto* outputRow = new QHBoxLayout();
    auto* outputButton = new QPushButton(QStringLiteral("浏览"), page);
    auto* openOutputButton = new QPushButton(QStringLiteral("打开"), page);
    connect(outputButton, &QPushButton::clicked, this, &MainWindow::browsePredictOutputDir);
    connect(openOutputButton, &QPushButton::clicked, this, &MainWindow::openPredictOutputDir);
    outputRow->addWidget(predictOutputEdit_, 1);
    outputRow->addWidget(outputButton);
    outputRow->addWidget(openOutputButton);

    auto* previewCard = workbenchCard(page, QStringLiteral("WorkbenchPanel"));
    auto* previewLayout = new QVBoxLayout(previewCard);
    previewLayout->setContentsMargins(12, 10, 12, 12);
    auto* previewHeader = new QHBoxLayout();
    previewHeader->addWidget(sectionLabel(QStringLiteral("图片预览与结果"), previewCard));
    previewHeader->addStretch(1);
    previewHeader->addWidget(new QLabel(QStringLiteral("缩放"), previewCard));
    previewHeader->addWidget(new QLabel(QStringLiteral("100%"), previewCard));
    predictionImagePathLabel_ = mutedLabel(QStringLiteral("未选择测试图片"), previewCard);
    predictionImagePathLabel_->setWordWrap(true);
    predictionImagePreview_ = new QLabel(QStringLiteral("未选择测试图片"), previewCard);
    predictionImagePreview_->setObjectName("ImagePreview");
    predictionImagePreview_->setAlignment(Qt::AlignCenter);
    predictionImagePreview_->setMinimumSize(360, 240);
    auto* previewFrame = workbenchCard(previewCard, QStringLiteral("PredictionPreviewFrame"));
    auto* previewFrameLayout = new QVBoxLayout(previewFrame);
    previewFrameLayout->setContentsMargins(0, 0, 0, 0);
    previewFrameLayout->addWidget(predictionImagePreview_, 1);

    predictionResultTable_ = tableWidget(4, {
        QStringLiteral("序号"),
        QStringLiteral("文本"),
        QStringLiteral("置信度"),
        QStringLiteral("坐标"),
    }, previewCard);
    predictionResultTable_->setHorizontalHeaderLabels({
        QStringLiteral("Kind"),
        QStringLiteral("Label"),
        QStringLiteral("Score"),
        QStringLiteral("Source"),
    });
    predictionPreview_ = new QPlainTextEdit(previewCard);
    predictionPreview_->setReadOnly(true);
    auto* structuredText = new QPlainTextEdit(QStringLiteral("结构化结果"), previewCard);
    structuredText->setReadOnly(true);
    predictionStructuredText_ = structuredText;
    auto* resultTabs = new QTabWidget(previewCard);
    resultTabs->addTab(predictionResultTable_, QStringLiteral("推理输出摘要"));
    resultTabs->addTab(structuredText, QStringLiteral("结构化结果"));
    resultTabs->addTab(predictionPreview_, QStringLiteral("运行日志"));

    predictionResultTotalLabel_ = new QLabel(QStringLiteral("结果行：0"), page);
    predictionResultConfidenceLabel_ = new QLabel(QStringLiteral("平均分：--"), page);
    predictionElapsedLabel_ = new QLabel(QStringLiteral("耗时：--"), page);
    predictionResultTotalLabel_->setObjectName("Muted");
    predictionResultConfidenceLabel_->setObjectName("Muted");
    predictionElapsedLabel_->setObjectName("Muted");
    auto* stats = new QHBoxLayout();
    stats->addWidget(predictionResultTotalLabel_);
    stats->addWidget(predictionResultConfidenceLabel_);
    stats->addWidget(predictionElapsedLabel_);
    stats->addStretch(1);
    previewLayout->addLayout(previewHeader);
    previewLayout->addLayout(inputRow);
    previewLayout->addWidget(predictionImagePathLabel_);
    previewLayout->addWidget(previewFrame, 1);
    previewLayout->addWidget(resultTabs, 1);
    previewLayout->addLayout(stats);

    auto* configPanel = workbenchCard(page, QStringLiteral("InspectorPanel"));
    configPanel->setMinimumWidth(430);
    configPanel->setMaximumWidth(620);
    auto* configLayout = new QVBoxLayout(configPanel);
    configLayout->setContentsMargins(10, 10, 10, 10);
    configLayout->addWidget(sectionLabel(QStringLiteral("预测配置"), configPanel));
    auto* workflowPanel = workbenchCard(configPanel, QStringLiteral("PredictionWorkflowPanel"));
    auto* workflowLayout = new QGridLayout(workflowPanel);
    workflowLayout->setContentsMargins(8, 8, 8, 8);
    workflowLayout->setHorizontalSpacing(6);
    workflowLayout->setVerticalSpacing(6);
    const QStringList workflowSteps{
        QStringLiteral("1 选择模型"),
        QStringLiteral("2 选择输入"),
        QStringLiteral("3 运行预测"),
        QStringLiteral("4 看可视化"),
        QStringLiteral("5 看 JSON"),
        QStringLiteral("6 错误样本"),
        QStringLiteral("7 对比版本"),
        QStringLiteral("8 导出结果"),
    };
    for (int i = 0; i < workflowSteps.size(); ++i) {
        auto* step = new QLabel(workflowSteps.at(i), workflowPanel);
        step->setObjectName(i < 3 ? "WorkflowStepActive" : "WorkflowStep");
        workflowLayout->addWidget(step, i / 2, i % 2);
    }
    configLayout->addWidget(workflowPanel);
    auto* activeModel = workbenchCard(configPanel, QStringLiteral("PredictionModelTile"));
    auto* activeModelLayout = new QHBoxLayout(activeModel);
    auto* modelIcon = new QLabel(QStringLiteral("≡"), activeModel);
    modelIcon->setObjectName("OverviewMetricIcon");
    modelIcon->setStyleSheet(QStringLiteral("background:#14786d;"));
    auto* modelText = new QVBoxLayout();
    predictionActiveModelTitleLabel_ = new QLabel(QStringLiteral("Det / Rec / Cls / Layout"), activeModel);
    predictionActiveModelTitleLabel_->setObjectName("TrainingTaskTitle");
    predictionActiveModelPathLabel_ = mutedLabel(QStringLiteral("model path: -"), activeModel);
    predictionActiveModelPathLabel_->setWordWrap(true);
    modelText->addWidget(predictionActiveModelTitleLabel_);
    modelText->addWidget(predictionActiveModelPathLabel_);
    modelText->addWidget(mutedLabel(QStringLiteral("模型路径：training\\det_v5_server\\inference"), activeModel));
    activeModelLayout->addWidget(modelIcon);
    if (modelText->count() > 2) {
        if (auto* legacyPathLabel = modelText->itemAt(2)->widget()) {
            legacyPathLabel->setVisible(false);
        }
    }
    activeModelLayout->addLayout(modelText, 1);
    configLayout->addWidget(activeModel);

    predictModeCombo_ = new QComboBox(configPanel);
    predictModeCombo_->addItem(QStringLiteral("OCR 文字识别"), QStringLiteral("ocr"));
    predictModeCombo_->addItem(QStringLiteral("分类预测"), QStringLiteral("classification"));
    predictModeCombo_->addItem(QStringLiteral("版面分析"), QStringLiteral("layout"));
    connect(predictModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updatePredictionModeButtons);
    predictBatchSpin_ = new QSpinBox(configPanel);
    auto* batchSpin = predictBatchSpin_;
    batchSpin->setRange(1, 4096);
    batchSpin->setValue(1);
    predictRunModeCombo_ = new QComboBox(configPanel);
    auto* runModeCombo = predictRunModeCombo_;
    runModeCombo->addItem(QStringLiteral("paddle"));
    const QString predictorLimitTip = QStringLiteral("Current built-in C++ predictors process the selected file or directory directly and do not consume PaddleX batch/run_mode/extra args yet.");
    batchSpin->setEnabled(false);
    batchSpin->setToolTip(predictorLimitTip);
    runModeCombo->setEnabled(false);
    runModeCombo->setToolTip(predictorLimitTip);
    batchSpin->hide();
    runModeCombo->hide();
    auto* configForm = new QFormLayout();
    configForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    configForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
    configForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    configForm->setHorizontalSpacing(10);
    configForm->setVerticalSpacing(8);
    configForm->addRow(QStringLiteral("预测模式"), predictModeCombo_);
    configForm->addRow(QStringLiteral("Det 模型"), predictDetModelCombo_);
    configForm->addRow(QStringLiteral("Rec 模型"), predictRecModelCombo_);
    configForm->addRow(QStringLiteral("分类任务"), predictClsTaskCombo_);
    configForm->addRow(QStringLiteral("分类模型"), predictClsModelCombo_);
    configForm->addRow(QStringLiteral("版面模型"), predictLayoutModelCombo_);
    configForm->addRow(QStringLiteral("推理设备"), predictDeviceCombo_);
    configForm->addRow(QStringLiteral("Score 阈值"), predictScoreThresholdSpin_);
    configForm->addRow(QStringLiteral("推理输出"), outputRow);
    configLayout->addLayout(configForm);
    auto setFormRowVisible = [configForm](QWidget* field, bool visible) {
        if (!field) {
            return;
        }
        field->setVisible(visible);
        if (auto* label = configForm->labelForField(field)) {
            label->setVisible(visible);
        }
    };
    auto applyPredictionModeForm = [this, setFormRowVisible]() {
        const QString mode = predictModeCombo_ ? predictModeCombo_->currentData().toString() : QStringLiteral("ocr");
        const bool ocrMode = mode == QStringLiteral("ocr");
        const bool classificationMode = mode == QStringLiteral("classification");
        const bool layoutMode = mode == QStringLiteral("layout");
        setFormRowVisible(predictDetModelCombo_, ocrMode);
        setFormRowVisible(predictRecModelCombo_, ocrMode);
        setFormRowVisible(predictClsTaskCombo_, classificationMode);
        setFormRowVisible(predictClsModelCombo_, classificationMode);
        setFormRowVisible(predictLayoutModelCombo_, layoutMode);
    };
    connect(predictModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, applyPredictionModeForm);
    applyPredictionModeForm();
    auto* outputOptions = new QHBoxLayout();
    outputOptions->addWidget(predictSaveVisualCheck_);
    outputOptions->addWidget(predictSaveJsonCheck_);
    outputOptions->addStretch(1);
    configLayout->addLayout(outputOptions);
    auto* extraHintLabel = mutedLabel(QStringLiteral("图像长边（可选）"), configPanel);
    extraHintLabel->hide();
    configLayout->addWidget(extraHintLabel);
    predictExtraOptionsEdit_ = new QPlainTextEdit(QStringLiteral("use_dilation true"), configPanel);
    auto* extraEdit = predictExtraOptionsEdit_;
    extraEdit->setMinimumHeight(120);
    extraEdit->setPlainText(predictorLimitTip);
    extraEdit->setReadOnly(true);
    extraEdit->setEnabled(false);
    extraEdit->setToolTip(predictorLimitTip);
    extraEdit->hide();
    configLayout->addWidget(extraEdit);

    auto* smallActions = new QGridLayout();
    auto* refreshModelsButton = new QPushButton(QStringLiteral("刷新模型"), configPanel);
    auto* modelLibraryButton = new QPushButton(QStringLiteral("模型库"), configPanel);
    auto* viewResultButton = new QPushButton(QStringLiteral("查看结果"), configPanel);
    auto* exportResultsButton = new QPushButton(QStringLiteral("导出结果"), configPanel);
    auto* errorSampleButton = new QPushButton(QStringLiteral("加入错误样本集"), configPanel);
    auto* compareVersionButton = new QPushButton(QStringLiteral("与上一版本对比"), configPanel);
    auto* smokeButton = new QPushButton(QStringLiteral("Smoke 测试"), configPanel);
    connect(refreshModelsButton, &QPushButton::clicked, this, &MainWindow::refreshPredictionModels);
    connect(modelLibraryButton, &QPushButton::clicked, this, [this]() { stack_->setCurrentIndex(5); });
    connect(viewResultButton, &QPushButton::clicked, this, &MainWindow::openPredictOutputDir);
    connect(exportResultsButton, &QPushButton::clicked, this, &MainWindow::exportPredictionResults);
    connect(errorSampleButton, &QPushButton::clicked, this, &MainWindow::addPredictionInputToErrorSamples);
    connect(compareVersionButton, &QPushButton::clicked, this, &MainWindow::comparePredictionWithPreviousVersion);
    connect(smokeButton, &QPushButton::clicked, this, &MainWindow::runInferenceSmoke);
    smallActions->addWidget(refreshModelsButton, 0, 0);
    smallActions->addWidget(modelLibraryButton, 0, 1);
    smallActions->addWidget(viewResultButton, 1, 0);
    smallActions->addWidget(exportResultsButton, 1, 1);
    smallActions->addWidget(errorSampleButton, 2, 0);
    smallActions->addWidget(compareVersionButton, 2, 1);
    smallActions->addWidget(smokeButton, 3, 0, 1, 2);
    configLayout->addLayout(smallActions);

    startPredictionButton_ = primaryButton(QStringLiteral("▷ 开始预测"), configPanel);
    startClsPredictionButton_ = primaryButton(QStringLiteral("开始分类预测"), configPanel);
    startLayoutPredictionButton_ = primaryButton(QStringLiteral("开始版面预测"), configPanel);
    stopPredictionButton_ = dangerButton(QStringLiteral("停止"), configPanel);
    startClsPredictionButton_->setVisible(false);
    startLayoutPredictionButton_->setVisible(false);
    connect(startPredictionButton_, &QPushButton::clicked, this, &MainWindow::startSelectedPredictionMode);
    connect(startClsPredictionButton_, &QPushButton::clicked, this, &MainWindow::startClassificationPrediction);
    connect(startLayoutPredictionButton_, &QPushButton::clicked, this, &MainWindow::startLayoutPrediction);
    connect(stopPredictionButton_, &QPushButton::clicked, this, &MainWindow::stopPrediction);
    connect(predictClsTaskCombo_, &QComboBox::currentTextChanged, this, &MainWindow::refreshPredictionModels);
    connect(predictDetModelCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updatePredictionActiveModelSummary);
    connect(predictRecModelCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updatePredictionActiveModelSummary);
    connect(predictClsModelCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updatePredictionActiveModelSummary);
    connect(predictLayoutModelCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updatePredictionActiveModelSummary);
    predictionStatusLabel_ = mutedLabel(QStringLiteral("空闲"), configPanel);
    predictionStatusLabel_->setObjectName("TrainingStatusIdle");
    configLayout->addWidget(predictionStatusLabel_);
    configLayout->addWidget(stopPredictionButton_);
    configLayout->addWidget(startPredictionButton_);
    configLayout->addWidget(startClsPredictionButton_);
    configLayout->addWidget(startLayoutPredictionButton_);
    configLayout->addStretch(1);

    auto* body = new QSplitter(Qt::Horizontal, page);
    body->addWidget(previewCard);
    body->addWidget(configPanel);
    body->setStretchFactor(0, 1);
    body->setSizes({900, 450});
    layout->addWidget(header);
    modelStrip->hide();
    layout->addWidget(body, 1);
    refreshPredictionModels();
    setPredictionRunning(false);
    updatePredictionModeButtons();
    return page;
}

QWidget* MainWindow::buildDatasetPage() {
    auto* page = new QWidget(this);
    page->setObjectName("DatasetRoot");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(10);

    auto* header = workbenchCard(page, QStringLiteral("HeaderPanel"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 10, 14, 10);
    headerLayout->setSpacing(10);
    auto* titleBox = new QVBoxLayout();
    titleBox->setSpacing(3);
    auto* title = new QLabel(QStringLiteral("数据集"), header);
    title->setObjectName("OverviewTitle");
    titleBox->addWidget(title);
    titleBox->addWidget(mutedLabel(QStringLiteral("导入 → 标注 → 校验 → 导出 → 训练"), header));
    headerLayout->addLayout(titleBox, 1);
    auto* importImagesButton = primaryButton(QStringLiteral("导入图片"), header);
    auto* importPdfsButton = new QPushButton(QStringLiteral("导入 PDF"), header);
    auto* validateButton = new QPushButton(QStringLiteral("校验"), header);
    auto* exportButton = new QPushButton(QStringLiteral("导出"), header);
    auto* splitButton = new QPushButton(QStringLiteral("按比例划分"), header);
    auto* manualSplitButton = new QPushButton(QStringLiteral("手动调整"), header);
    auto* splitReportButton = new QPushButton(QStringLiteral("导出 split 报告"), header);
    auto* labelButton = new QPushButton(QStringLiteral("去标注"), header);
    connect(importImagesButton, &QPushButton::clicked, this, &MainWindow::importImagesDialog);
    connect(importPdfsButton, &QPushButton::clicked, this, &MainWindow::importPdfsDialog);
    connect(validateButton, &QPushButton::clicked, this, &MainWindow::validateProject);
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportProject);
    connect(splitButton, &QPushButton::clicked, this, &MainWindow::reassignDatasetSplits);
    connect(manualSplitButton, &QPushButton::clicked, this, [this]() {
        if (stack_) {
            stack_->setCurrentIndex(1);
        }
        statusBar()->showMessage(QStringLiteral("请在页面元数据里手动调整 split"), 4000);
    });
    connect(splitReportButton, &QPushButton::clicked, this, &MainWindow::exportDatasetSplitReport);
    connect(labelButton, &QPushButton::clicked, this, [this]() { stack_->setCurrentIndex(1); });
    headerLayout->addWidget(importImagesButton);
    headerLayout->addWidget(importPdfsButton);
    headerLayout->addWidget(validateButton);
    headerLayout->addWidget(exportButton);
    headerLayout->addWidget(splitButton);
    headerLayout->addWidget(manualSplitButton);
    headerLayout->addWidget(splitReportButton);
    headerLayout->addWidget(labelButton);

    auto* summaryPanel = workbenchCard(page, QStringLiteral("WorkbenchPanel"));
    auto* summaryLayout = new QHBoxLayout(summaryPanel);
    summaryLayout->setContentsMargins(12, 10, 12, 10);
    datasetSummaryLabel_ = new QLabel(QStringLiteral("未打开项目"), summaryPanel);
    datasetSummaryLabel_->setObjectName("SectionTitle");
    datasetSummaryLabel_->setWordWrap(true);
    datasetNextStepLabel_ = new QLabel(QStringLiteral("下一步：打开或新建项目"), summaryPanel);
    datasetNextStepLabel_->setObjectName("StatusWarn");
    datasetNextStepLabel_->setWordWrap(true);
    summaryLayout->addWidget(datasetSummaryLabel_, 1);
    summaryLayout->addWidget(datasetNextStepLabel_);

    auto* tables = new QSplitter(Qt::Vertical, page);
    auto* overviewPanel = workbenchCard(tables, QStringLiteral("WorkbenchPanel"));
    auto* overviewLayout = new QVBoxLayout(overviewPanel);
    overviewLayout->setContentsMargins(12, 10, 12, 12);
    overviewLayout->addWidget(sectionLabel(QStringLiteral("任务闭环"), overviewPanel));
    datasetOverviewTable_ = tableWidget(4, {
        QStringLiteral("环节"),
        QStringLiteral("状态"),
        QStringLiteral("数量 / 路径"),
        QStringLiteral("下一步"),
    }, overviewPanel);
    datasetOverviewTable_->setMinimumHeight(210);
    overviewLayout->addWidget(datasetOverviewTable_, 1);

    auto* pathPanel = workbenchCard(tables, QStringLiteral("WorkbenchPanel"));
    auto* pathLayout = new QVBoxLayout(pathPanel);
    pathLayout->setContentsMargins(12, 10, 12, 12);
    pathLayout->addWidget(sectionLabel(QStringLiteral("项目目录"), pathPanel));
    datasetPathTable_ = tableWidget(3, {
        QStringLiteral("目录"),
        QStringLiteral("状态"),
        QStringLiteral("路径"),
    }, pathPanel);
    datasetPathTable_->setMinimumHeight(180);
    pathLayout->addWidget(datasetPathTable_, 1);

    tables->addWidget(overviewPanel);
    tables->addWidget(pathPanel);
    tables->setSizes({320, 260});
    layout->addWidget(header);
    layout->addWidget(summaryPanel);
    layout->addWidget(tables, 1);
    refreshDatasetPage();
    return page;
}

QWidget* MainWindow::buildModelLibraryPage() {
    auto* page = new QWidget(this);
    page->setObjectName("ModelLibraryRoot");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(10);

    auto* header = workbenchCard(page, QStringLiteral("HeaderPanel"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 10, 14, 10);
    headerLayout->setSpacing(10);
    auto* titleBox = new QVBoxLayout();
    titleBox->setSpacing(3);
    auto* title = new QLabel(QStringLiteral("模型库"), header);
    title->setObjectName("OverviewTitle");
    titleBox->addWidget(title);
    titleBox->addWidget(mutedLabel(QStringLiteral("版本、指标、当前模型、最佳模型"), header));
    headerLayout->addLayout(titleBox, 1);
    auto* refreshButton = new QPushButton(QStringLiteral("刷新"), header);
    auto* trainButton = primaryButton(QStringLiteral("去训练"), header);
    auto* predictButton = new QPushButton(QStringLiteral("去预测"), header);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshModelLibraryPage);
    connect(trainButton, &QPushButton::clicked, this, [this]() { stack_->setCurrentIndex(2); });
    connect(predictButton, &QPushButton::clicked, this, [this]() { stack_->setCurrentIndex(3); });
    headerLayout->addWidget(refreshButton);
    headerLayout->addWidget(trainButton);
    headerLayout->addWidget(predictButton);

    auto* summaryPanel = workbenchCard(page, QStringLiteral("WorkbenchPanel"));
    auto* summaryLayout = new QHBoxLayout(summaryPanel);
    summaryLayout->setContentsMargins(12, 10, 12, 10);
    modelLibrarySummaryLabel_ = new QLabel(QStringLiteral("未打开项目"), summaryPanel);
    modelLibrarySummaryLabel_->setObjectName("SectionTitle");
    modelLibrarySummaryLabel_->setWordWrap(true);
    summaryLayout->addWidget(modelLibrarySummaryLabel_, 1);

    auto* tablePanel = workbenchCard(page, QStringLiteral("WorkbenchPanel"));
    auto* tableLayout = new QVBoxLayout(tablePanel);
    tableLayout->setContentsMargins(12, 10, 12, 12);
    tableLayout->addWidget(sectionLabel(QStringLiteral("训练版本"), tablePanel));
    modelLibraryTable_ = tableWidget(8, {
        QStringLiteral("任务"),
        QStringLiteral("版本"),
        QStringLiteral("状态"),
        QStringLiteral("主指标"),
        QStringLiteral("当前"),
        QStringLiteral("最佳"),
        QStringLiteral("模型目录"),
        QStringLiteral("完成时间"),
    }, tablePanel);
    modelLibraryTable_->setMinimumHeight(360);
    modelLibraryTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(modelLibraryTable_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        const auto* item = modelLibraryTable_ ? modelLibraryTable_->item(row, 0) : nullptr;
        const QString taskKey = item ? item->data(Qt::UserRole).toString() : QString();
        if (!taskKey.isEmpty()) {
            selectTrainingTaskAndShowDetail(taskKey);
            stack_->setCurrentIndex(2);
        }
    });
    tableLayout->addWidget(modelLibraryTable_, 1);

    layout->addWidget(header);
    layout->addWidget(summaryPanel);
    layout->addWidget(tablePanel, 1);
    refreshModelLibraryPage();
    return page;
}

QWidget* MainWindow::buildLogsPage() {
    auto* page = new QWidget(this);
    page->setObjectName("LogsRoot");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(10);

    if (!logEdit_) {
        logEdit_ = new QPlainTextEdit(page);
        logEdit_->setReadOnly(true);
        logEdit_->hide();
    }

    auto* header = workbenchCard(page, QStringLiteral("HeaderPanel"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 10, 14, 10);
    headerLayout->setSpacing(10);
    auto* titleBox = new QVBoxLayout();
    titleBox->setSpacing(3);
    auto* title = new QLabel(QStringLiteral("日志"), header);
    title->setObjectName("OverviewTitle");
    titleBox->addWidget(title);
    titleBox->addWidget(mutedLabel(QStringLiteral("会话、校验、导出、训练、预测"), header));
    headerLayout->addLayout(titleBox, 1);
    logsSourceCombo_ = new QComboBox(header);
    logsSourceCombo_->addItem(QStringLiteral("会话日志"), QStringLiteral("session"));
    logsSourceCombo_->addItem(QStringLiteral("校验日志"), QStringLiteral("validate"));
    logsSourceCombo_->addItem(QStringLiteral("导出日志"), QStringLiteral("export"));
    logsSourceCombo_->addItem(QStringLiteral("训练记录 JSON"), QStringLiteral("runs"));
    logsSourceCombo_->addItem(QStringLiteral("当前训练输出"), QStringLiteral("training"));
    logsSourceCombo_->addItem(QStringLiteral("当前预测输出"), QStringLiteral("prediction"));
    auto* refreshButton = new QPushButton(QStringLiteral("刷新"), header);
    auto* openButton = new QPushButton(QStringLiteral("打开文件"), header);
    connect(logsSourceCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::refreshLogsPage);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshLogsPage);
    connect(openButton, &QPushButton::clicked, this, [this]() {
        QString path;
        const QString source = comboStoredValue(logsSourceCombo_);
        if (source == QStringLiteral("validate")) {
            path = lastValidationLogPath_;
            if (path.isEmpty() && context_) {
                path = QDir(context_->logRoot()).filePath(QStringLiteral("validate.log"));
            }
        } else if (source == QStringLiteral("export") && context_) {
            path = QDir(context_->logRoot()).filePath(QStringLiteral("export.log"));
        } else if (source == QStringLiteral("runs") && context_) {
            path = context_->path(QStringLiteral("training/runs.json"));
        }
        if (path.isEmpty() || !QFileInfo::exists(path)) {
            QMessageBox::information(this, QStringLiteral("日志"), QStringLiteral("当前来源没有可打开的日志文件。"));
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    headerLayout->addWidget(logsSourceCombo_);
    headerLayout->addWidget(refreshButton);
    headerLayout->addWidget(openButton);

    auto* summaryPanel = workbenchCard(page, QStringLiteral("WorkbenchPanel"));
    auto* summaryLayout = new QHBoxLayout(summaryPanel);
    summaryLayout->setContentsMargins(12, 10, 12, 10);
    logsSummaryLabel_ = new QLabel(QStringLiteral("未打开项目"), summaryPanel);
    logsSummaryLabel_->setObjectName("SectionTitle");
    logsSummaryLabel_->setWordWrap(true);
    summaryLayout->addWidget(logsSummaryLabel_, 1);

    logsViewer_ = new QPlainTextEdit(page);
    logsViewer_->setReadOnly(true);
    logsViewer_->setMinimumHeight(420);
    logsViewer_->setPlainText(QStringLiteral("暂无日志。"));
    layout->addWidget(header);
    layout->addWidget(summaryPanel);
    layout->addWidget(logsViewer_, 1);
    refreshLogsPage();
    return page;
}

QWidget* MainWindow::buildSettingsPage() {
    auto* page = new QWidget(this);
    page->setObjectName("SettingsRoot");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(10);

    auto* header = workbenchCard(page, QStringLiteral("HeaderPanel"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 10, 14, 10);
    headerLayout->setSpacing(10);
    auto* titleBox = new QVBoxLayout();
    titleBox->setSpacing(3);
    auto* title = new QLabel(QStringLiteral("配置"), header);
    title->setObjectName("OverviewTitle");
    titleBox->addWidget(title);
    titleBox->addWidget(mutedLabel(QStringLiteral("项目路径、依赖、模型、运行环境"), header));
    headerLayout->addLayout(titleBox, 1);
    auto* refreshButton = new QPushButton(QStringLiteral("刷新"), header);
    auto* reportButton = primaryButton(QStringLiteral("环境报告"), header);
    auto* openRootButton = new QPushButton(QStringLiteral("打开目录"), header);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshSettingsPage);
    connect(reportButton, &QPushButton::clicked, this, &MainWindow::showEnvironmentReport);
    connect(openRootButton, &QPushButton::clicked, this, [this]() {
        const QString path = context_ ? context_->root.absolutePath() : baseDir_;
        if (!path.isEmpty() && QFileInfo::exists(path)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        }
    });
    headerLayout->addWidget(refreshButton);
    headerLayout->addWidget(reportButton);
    headerLayout->addWidget(openRootButton);

    auto* summaryPanel = workbenchCard(page, QStringLiteral("WorkbenchPanel"));
    auto* summaryLayout = new QHBoxLayout(summaryPanel);
    summaryLayout->setContentsMargins(12, 10, 12, 10);
    settingsSummaryLabel_ = new QLabel(QStringLiteral("未打开项目"), summaryPanel);
    settingsSummaryLabel_->setObjectName("SectionTitle");
    settingsSummaryLabel_->setWordWrap(true);
    summaryLayout->addWidget(settingsSummaryLabel_, 1);

    auto* body = new QSplitter(Qt::Horizontal, page);
    auto* pathPanel = workbenchCard(body, QStringLiteral("WorkbenchPanel"));
    auto* pathLayout = new QVBoxLayout(pathPanel);
    pathLayout->setContentsMargins(12, 10, 12, 12);
    pathLayout->addWidget(sectionLabel(QStringLiteral("路径状态"), pathPanel));
    settingsPathTable_ = tableWidget(3, {
        QStringLiteral("项目 / 依赖"),
        QStringLiteral("状态"),
        QStringLiteral("路径"),
    }, pathPanel);
    settingsPathTable_->setMinimumWidth(460);
    pathLayout->addWidget(settingsPathTable_, 1);

    settingsReportView_ = new QPlainTextEdit(body);
    settingsReportView_->setReadOnly(true);
    settingsReportView_->setMinimumWidth(420);
    body->addWidget(pathPanel);
    body->addWidget(settingsReportView_);
    body->setSizes({560, 520});

    layout->addWidget(header);
    layout->addWidget(summaryPanel);
    layout->addWidget(body, 1);
    refreshSettingsPage();
    return page;
}

}  // namespace ppocr
