#include "app/MainWindowInternal.h"
namespace ppocr {

MainWindow::MainWindow(QString baseDir, QWidget* parent)
    : QMainWindow(parent), baseDir_(std::move(baseDir)) {
    annotationController_ = new AnnotationController(this);
    datasetController_ = new DatasetController(this);
    predictionController_ = new PredictionController(this);
    projectController_ = new ProjectController(baseDir_, this);
    trainingController_ = new TrainingController(baseDir_, this);

    connect(trainingController_, &TrainingController::trainingPrepared, this, &MainWindow::handleTrainingPrepared);
    connect(trainingController_, &TrainingController::logTextReady, this, &MainWindow::appendTrainingText);
    connect(trainingController_, &TrainingController::metricsTextReady, this, &MainWindow::appendTrainingMetricsFromText);
    connect(trainingController_, &TrainingController::runningChanged, this, &MainWindow::setTrainingRunning);
    connect(trainingController_, &TrainingController::trainingFinished, this, &MainWindow::handleTrainingCompleted);

    connect(predictionController_, &PredictionController::outputTextReady, this, &MainWindow::appendPredictionText);
    connect(predictionController_, &PredictionController::runningChanged, this, &MainWindow::setPredictionRunning);
    connect(predictionController_, &PredictionController::predictionFinished, this, &MainWindow::handlePredictionFinished);
    connect(predictionController_, &PredictionController::predictionStartFailed, this, &MainWindow::handlePredictionStartFailed);

    setWindowTitle(QStringLiteral("PP-OCR 标注工具"));
    const QRect available = initialAvailableGeometry();
    buildUi();
    resize(initialWindowSizeForScreen(available));
    move(available.center() - rect().center());
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == trainingTaskHost_
            && (event->type() == QEvent::Resize
                || event->type() == QEvent::Show
                || event->type() == QEvent::LayoutRequest)) {
        const int columns = trainingTaskOverviewColumnsForWidth(trainingTaskHost_->width());
        if (event->type() == QEvent::Show || columns != trainingTaskOverviewColumns_) {
            queueTrainingTaskOverviewRefresh();
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::openProjectPath(const QString& projectDir) {
    try {
        setProject(projectController_->openProject(projectDir));
    } catch (const std::exception& exc) {
        QMessageBox::critical(this, QStringLiteral("打开项目失败"), QString::fromUtf8(exc.what()));
    }
}

void MainWindow::openLastProject() {
    const QStringList projects = projectController_ ? projectController_->recentProjects() : QStringList{};
    if (!projects.isEmpty()) {
        openProjectPath(projects.first());
    }
}

void MainWindow::buildUi() {
    auto* toolbar = new QToolBar(QStringLiteral("Main"), this);
    toolbar->setMovable(false);
    auto* title = new QLabel(QStringLiteral("PP-OCR 工作台"), toolbar);
    title->setObjectName("Title");
    title->setMinimumWidth(130);
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    toolbar->addWidget(title);
    addToolBar(toolbar);

    auto* host = new QWidget(this);
    auto* root = new QHBoxLayout(host);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    stack_ = new WorkbenchStackedWidget(host);
    stack_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    stack_->setMinimumSize(0, 0);
    stack_->addWidget(buildOverviewPage());
    stack_->addWidget(buildLabelPage());
    stack_->addWidget(buildTrainingPage());
    stack_->addWidget(buildPredictionPage());
    stack_->addWidget(buildDatasetPage());
    stack_->addWidget(buildModelLibraryPage());
    stack_->addWidget(buildLogsPage());
    stack_->addWidget(buildSettingsPage());

    root->addWidget(buildRail());
    root->addWidget(stack_, 1);
    setCentralWidget(host);
    statusBar()->showMessage(QStringLiteral("当前工作区：概况"));
    connect(stack_, &QStackedWidget::currentChanged, this, [this](int index) {
        const QStringList names{
            QStringLiteral("概况"),
            QStringLiteral("标注"),
            QStringLiteral("训练"),
            QStringLiteral("预测"),
            QStringLiteral("数据集"),
            QStringLiteral("模型库"),
            QStringLiteral("日志"),
            QStringLiteral("配置"),
        };
        statusBar()->showMessage(QStringLiteral("当前工作区：%1").arg(index >= 0 && index < names.size() ? names.at(index) : QString()));
        if (index == 4) {
            refreshDatasetPage();
        } else if (index == 5) {
            refreshModelLibraryPage();
        } else if (index == 6) {
            refreshLogsPage();
        } else if (index == 7) {
            refreshSettingsPage();
        } else if (index == 2) {
            queueTrainingTaskOverviewRefresh();
        }
    });
    connect(stack_, &QStackedWidget::currentChanged, this, [this](int index) {
        if (!overviewResourceTimer_) {
            return;
        }
        if (index == 0) {
            refreshOverviewResources();
            overviewResourceTimer_->start();
        } else {
            overviewResourceTimer_->stop();
        }
    });
    if (overviewResourceTimer_) {
        overviewResourceTimer_->start();
    }
}

QWidget* MainWindow::buildRail() {
    auto* rail = new QWidget(this);
    rail->setObjectName("WorkspaceRail");
    rail->setMinimumWidth(190);
    rail->setMaximumWidth(210);
    rail->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto* layout = new QVBoxLayout(rail);
    layout->setContentsMargins(14, 18, 14, 14);
    layout->setSpacing(10);
    auto* railTitle = new QLabel(QStringLiteral("PP-OCR 工作台"), rail);
    railTitle->setObjectName("RailTitle");
    layout->addWidget(railTitle);
    layout->addSpacing(14);
    layout->addWidget(railButton(QStringLiteral("概况"), 0));
    layout->addWidget(railButton(QStringLiteral("数据"), 4));
    layout->addWidget(railButton(QStringLiteral("标注"), 1));
    layout->addWidget(railButton(QStringLiteral("训练"), 2));
    layout->addWidget(railButton(QStringLiteral("模型"), 5));
    layout->addWidget(railButton(QStringLiteral("预测"), 3));
    layout->addWidget(railButton(QStringLiteral("日志"), 6));
    layout->addWidget(railButton(QStringLiteral("配置"), 7));
    layout->addStretch(1);
    return rail;
}

QPushButton* MainWindow::railButton(const QString& text, int pageIndex) {
    const QMap<int, QString> icons{
        {0, QStringLiteral("⌂")},
        {4, QStringLiteral("◎")},
        {1, QStringLiteral("✎")},
        {2, QStringLiteral("▥")},
        {5, QStringLiteral("▧")},
        {3, QStringLiteral("⌖")},
        {6, QStringLiteral("□")},
        {7, QStringLiteral("⚙")},
    };
    auto* button = new QPushButton(QStringLiteral("%1   %2").arg(icons.value(pageIndex, QStringLiteral("•")), text), this);
    button->setObjectName("WorkspaceNav");
    button->setCheckable(true);
    button->setChecked(stack_ && stack_->currentIndex() == pageIndex);
    button->setMinimumHeight(58);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(button, &QPushButton::clicked, this, [this, pageIndex]() { stack_->setCurrentIndex(pageIndex); });
    connect(stack_, &QStackedWidget::currentChanged, button, [button, pageIndex](int current) {
        button->setChecked(current == pageIndex);
    });
    return button;
}


}  // namespace ppocr
