#pragma once

#include "core/ProjectTypes.h"
#include "core/TrainingRunner.h"

#include <QComboBox>
#include <QCheckBox>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QQuickWidget>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVariant>
#include <optional>

class QGridLayout;
class QHBoxLayout;
class QThread;
class QTimer;

namespace ppocr {

struct PaddleCommand;
class DashboardDonutChart;
class DashboardLineChart;
class DashboardSparklineCard;
class TrainingPlaceholderChart;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QString baseDir, QWidget* parent = nullptr);
    void openProjectPath(const QString& projectDir);
    void openLastProject();

private:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void buildUi();
    QWidget* buildRail();
    QWidget* buildOverviewPage();
    QWidget* buildLabelPage();
    QWidget* buildLabelPageLegacy();
    QWidget* buildTrainingPage();
    QWidget* buildPredictionPage();
    QWidget* buildDatasetPage();
    QWidget* buildModelLibraryPage();
    QWidget* buildLogsPage();
    QWidget* buildSettingsPage();
    QPushButton* railButton(const QString& text, int pageIndex);
    void setProject(const ProjectContext& context);
    void refreshPages();
    void refreshOverviewStats();
    void refreshOverviewResources();
    void refreshDatasetPage();
    void refreshModelLibraryPage();
    void refreshLogsPage();
    void refreshSettingsPage();
    void applyPageFilters();
    void loadSelectedPage();
    bool pageMatchesFilters(const PageInfo& page) const;
    void updatePageMetadataPanel();
    void appendLog(const QString& text);
    void refreshValidationTable(const QList<ValidationIssue>& issues, const QString& logPath);
    QStringList loadRecentProjects() const;
    void saveRecentProjects(const QStringList& projects) const;
    void rememberProject(const QString& projectDir);
    void refreshRecentProjects();
    void connectCanvasSignals();
    void setCanvasToolMode(const QString& mode);
    void refreshCanvasAnnotation();
    void persistCurrentAnnotation();
    void pushAnnotationUndoState();
    void updateAnnotationHistoryActions();
    void updateSelectedRegionPanel();
    void refreshImageLabelPanel();
    void fillComboFromLabelSet(QComboBox* combo, const QString& key);
    QString imageLabelValue(const QString& task) const;
    void setComboToValue(QComboBox* combo, const QString& value);
    void refreshLabelModelChoices();
    void loadLabelModelConfig();
    void saveLabelModelConfig();
    void startOneClickPrelabelAll();
    void startPrelabelCurrentPageAsync(
        bool runOcr,
        bool runOrientation,
        bool runTableClassification,
        bool runLayout,
        const QString& title);
    void startPrelabelAllAsync(
        bool runOcr,
        bool runOrientation,
        bool runTableClassification,
        bool runLayout,
        const QString& title);
    void setComboToPath(QComboBox* combo, const QString& path);
    QString labelSelectedModelDir(QComboBox* labelCombo, QComboBox* fallbackCombo = nullptr) const;
    QString labelPrelabelDevice() const;
    double prelabelScoreThreshold() const;
    QStringList labelModelMissingReasons() const;
    QString currentTrainingTaskKey() const;
    TrainingOptions trainingOptionsFromUi(const TrainingTaskSpec& task) const;
    PaddleCommand buildTrainingCommand(const QString& taskKey, const QString& outputDir) const;
    QString paddlexPythonPath() const;
    void appendTrainingText(const QString& text);
    void appendTrainingMetricsFromText(const QString& text);
    void refreshTrainingMetricCharts();
    void setTrainingRunning(bool running);
    QJsonObject parseTrainingMetrics(const QString& logText) const;
    void refreshTrainingVersions();
    void refreshTrainingTaskOverview();
    void queueTrainingTaskOverviewRefresh();
    void refreshTrainingVersionManagerTable();
    void selectTrainingTaskAndShowDetail(const QString& taskKey);
    void compareSelectedTrainingVersions();
    void browseTrainingResumePath();
    void browseTrainingExportWeight();
    void browseTrainingExportDir();
    void exportSelectedTrainingModel();
    void applyTrainingTaskDefaults();
    QJsonObject selectedTrainingVersionObject() const;
    QString selectedTrainingVersionId() const;
    QString selectedTrainingVersionDir() const;
    void finalizeTrainingRun(const QString& status, int exitCode, const QString& errorSummary);
    void refreshPredictionModels();
    void refreshPredictionModelCards();
    void selectPredictionModel(const QString& kind, const QString& modelDir);
    void updatePredictionActiveModelSummary();
    void updatePredictionModeButtons();
    QString defaultPredictionOutputDir() const;
    QString selectedPredictionModelDir(QComboBox* combo) const;
    void appendPredictionText(const QString& text);
    void setPredictionRunning(bool running);
    bool preparePredictionRun(const QString& kind, QString* inputPath, QString* publishDir, QString* stagingDir);
    QPair<int, int> publishPredictionOutputs(const QString& stagingDir, const QString& publishDir) const;
    void summarizePredictionOutputs(const QString& stagingDir, const QString& publishDir, const QPair<int, int>& published, qint64 elapsedMs);
    QString firstPredictionPreviewImage(const QString& path) const;
    void updatePredictionImagePreview(const QString& path);

private slots:
    void newProjectDialog();
    void openProjectDialog();
    void openRecentProjectFromCombo();
    void filterPages();
    void savePageMetadata();
    void importImagesDialog();
    void importPdfsDialog();
    void validateProject();
    void exportProject();
    void showEnvironmentReport();
    void openValidationLog();
    void checkTrainingSetup();
    void previewTrainingCommand();
    void copyTrainingCommand();
    void exportTrainingMetricsCsv();
    void clearTrainingMetrics();
    void trainingTaskChanged();
    void updateSelectedTrainingVersionDetails();
    void setSelectedTrainingVersionCurrent();
    void deleteSelectedTrainingVersion();
    void openSelectedTrainingVersionDir();
    void startTraining();
    void stopTraining();
    void handleTrainingOutput();
    void handleTrainingFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleTrainingError(QProcess::ProcessError error);
    void runInferenceSmoke();
    void browsePredictInputFile();
    void browsePredictInputDir();
    void pastePredictImageFromClipboard();
    void browsePredictOutputDir();
    void openPredictOutputDir();
    void startPrediction();
    void startSelectedPredictionMode();
    void startClassificationPrediction();
    void startLayoutPrediction();
    void stopPrediction();
    void handlePredictionOutput();
    void handlePredictionFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handlePredictionError(QProcess::ProcessError error);
    void prelabelSelectedPage();
    void prelabelSelectedPageClassification();
    void prelabelSelectedPageLayout();
    void prelabelSelectedPageTableClassification();
    void prelabelAllPages();
    void prelabelAllPagesClassification();
    void prelabelAllPagesLayout();
    void prelabelAllPagesTableClassification();
    void undoAnnotationEdit();
    void redoAnnotationEdit();
    void clearCurrentPageAnnotations();
    void clearAllAnnotations();
    void generateCurrentRecCrops();
    void addOcrRegionFromCanvas(const QVariant& points);
    void addLayoutRegionFromCanvas(const QVariant& points);
    void selectRegionFromCanvas(const QString& regionId);
    void moveRegionFromCanvas(const QString& regionId, const QVariant& points);
    void saveSelectedRegionText();
    void saveImageLabels();
    void deleteSelectedRegion();

private:
    QString baseDir_;
    std::optional<ProjectContext> context_;
    QList<PageInfo> allPages_;
    QList<PageInfo> pages_;
    QJsonObject currentAnnotation_;
    QList<QJsonObject> annotationUndoStack_;
    QList<QJsonObject> annotationRedoStack_;
    QString selectedRegionId_;
    bool updatingRegionPanel_ = false;
    QString trainingLogBuffer_;
    QString activeTrainingRunId_;
    QString activeTrainingTaskKey_;
    QString activeTrainingTaskKind_;
    QString activeTrainingVersionId_;
    QString activeTrainingVersionDir_;
    TrainingRunStart activeTrainingStart_;
    bool trainingStopRequested_ = false;
    bool trainingFinalized_ = false;
    QString predictionLogBuffer_;
    QString activePredictionStagingDir_;
    QString activePredictionPublishDir_;
    QString lastValidationLogPath_;
    bool activePredictionPublishJson_ = true;
    bool activePredictionPublishVisual_ = true;
    QElapsedTimer predictionTimer_;

    QStackedWidget* stack_ = nullptr;
    QLabel* projectLabel_ = nullptr;
    QComboBox* overviewTrendRangeCombo_ = nullptr;
    DashboardLineChart* overviewTrendChart_ = nullptr;
    DashboardDonutChart* overviewDonutChart_ = nullptr;
    DashboardSparklineCard* overviewGpuCard_ = nullptr;
    DashboardSparklineCard* overviewCpuCard_ = nullptr;
    DashboardSparklineCard* overviewMemoryCard_ = nullptr;
    DashboardSparklineCard* overviewDiskCard_ = nullptr;
    QTimer* overviewResourceTimer_ = nullptr;
    QTableWidget* overviewRecentTasksTable_ = nullptr;
    QTableWidget* overviewDatasetTable_ = nullptr;
    QLabel* overviewPagesMetric_ = nullptr;
    QLabel* overviewLabeledMetric_ = nullptr;
    QLabel* overviewSplitMetric_ = nullptr;
    QLabel* overviewRegionsMetric_ = nullptr;
    QLabel* overviewExportsMetric_ = nullptr;
    QLabel* overviewVersionsMetric_ = nullptr;
    QLabel* validationSummaryLabel_ = nullptr;
    QTableWidget* validationIssueTable_ = nullptr;
    QLabel* datasetSummaryLabel_ = nullptr;
    QTableWidget* datasetOverviewTable_ = nullptr;
    QTableWidget* datasetPathTable_ = nullptr;
    QLabel* datasetNextStepLabel_ = nullptr;
    QLabel* modelLibrarySummaryLabel_ = nullptr;
    QTableWidget* modelLibraryTable_ = nullptr;
    QLabel* logsSummaryLabel_ = nullptr;
    QComboBox* logsSourceCombo_ = nullptr;
    QPlainTextEdit* logsViewer_ = nullptr;
    QLabel* settingsSummaryLabel_ = nullptr;
    QTableWidget* settingsPathTable_ = nullptr;
    QPlainTextEdit* settingsReportView_ = nullptr;
    QLabel* labelValidationSummaryLabel_ = nullptr;
    QTableWidget* labelValidationIssueTable_ = nullptr;
    QComboBox* recentProjectsCombo_ = nullptr;
    QLabel* labelWorkspaceSummary_ = nullptr;
    QLabel* pageCountLabel_ = nullptr;
    QLabel* fileTitleLabel_ = nullptr;
    QLabel* fileMetaLabel_ = nullptr;
    QLabel* canvasModeLabel_ = nullptr;
    QLabel* pageMetaLabel_ = nullptr;
    QListWidget* pageList_ = nullptr;
    QLineEdit* pageSearchEdit_ = nullptr;
    QComboBox* pageSplitFilterCombo_ = nullptr;
    QComboBox* pageStatusFilterCombo_ = nullptr;
    QComboBox* pageSplitCombo_ = nullptr;
    QComboBox* pageStatusCombo_ = nullptr;
    QQuickWidget* canvasWidget_ = nullptr;
    QPushButton* selectToolButton_ = nullptr;
    QPushButton* drawOcrToolButton_ = nullptr;
    QPushButton* drawLayoutToolButton_ = nullptr;
    QPushButton* panToolButton_ = nullptr;
    QPushButton* undoAnnotationButton_ = nullptr;
    QPushButton* redoAnnotationButton_ = nullptr;
    QLabel* selectedRegionLabel_ = nullptr;
    QPlainTextEdit* regionTextEdit_ = nullptr;
    QComboBox* layoutLabelCombo_ = nullptr;
    QSpinBox* regionReadingOrderSpin_ = nullptr;
    QCheckBox* regionCheckedCheck_ = nullptr;
    QCheckBox* regionIgnoreCheck_ = nullptr;
    QCheckBox* exportDetCheck_ = nullptr;
    QCheckBox* exportRecCheck_ = nullptr;
    QCheckBox* exportClsCheck_ = nullptr;
    QCheckBox* exportTextlineClsCheck_ = nullptr;
    QCheckBox* exportTableClsCheck_ = nullptr;
    QCheckBox* exportCocoCheck_ = nullptr;
    QCheckBox* exportCheckedOnlyCheck_ = nullptr;
    QLabel* exportProgressLabel_ = nullptr;
    QProgressBar* exportProgressBar_ = nullptr;
    QPushButton* exportProjectButton_ = nullptr;
    QComboBox* docOrientationCombo_ = nullptr;
    QComboBox* textlineOrientationCombo_ = nullptr;
    QComboBox* tableClassificationCombo_ = nullptr;
    QComboBox* labelDetModelCombo_ = nullptr;
    QComboBox* labelRecModelCombo_ = nullptr;
    QComboBox* labelTextlineClsModelCombo_ = nullptr;
    QComboBox* labelDocOrientationModelCombo_ = nullptr;
    QComboBox* labelDocUnwarpingModelCombo_ = nullptr;
    QComboBox* labelLayoutModelCombo_ = nullptr;
    QCheckBox* labelUseGpuCheck_ = nullptr;
    QCheckBox* labelEnableTextlineClsCheck_ = nullptr;
    QCheckBox* labelEnableDocOrientationCheck_ = nullptr;
    QCheckBox* labelEnableDocUnwarpingCheck_ = nullptr;
    QDoubleSpinBox* prelabelScoreThresholdSpin_ = nullptr;
    QLabel* labelModelStatus_ = nullptr;
    QPushButton* saveRegionButton_ = nullptr;
    QPushButton* deleteRegionButton_ = nullptr;
    QComboBox* trainingTaskCombo_ = nullptr;
    QStackedWidget* trainingScreens_ = nullptr;
    QLineEdit* trainingTaskSearchEdit_ = nullptr;
    QComboBox* trainingTaskTypeFilter_ = nullptr;
    QComboBox* trainingTaskStatusFilter_ = nullptr;
    QComboBox* trainingTaskSortCombo_ = nullptr;
    QWidget* trainingTaskHost_ = nullptr;
    QGridLayout* trainingTaskGrid_ = nullptr;
    int trainingTaskOverviewColumns_ = 0;
    bool trainingTaskOverviewRefreshQueued_ = false;
    QLabel* trainingTotalTasksMetric_ = nullptr;
    QLabel* trainingRunningTasksMetric_ = nullptr;
    QLabel* trainingDoneTasksMetric_ = nullptr;
    QLabel* trainingFailedTasksMetric_ = nullptr;
    QLabel* trainingTodayTasksMetric_ = nullptr;
    QComboBox* trainingDeviceCombo_ = nullptr;
    QSpinBox* trainingEpochsSpin_ = nullptr;
    QSpinBox* trainingBatchSpin_ = nullptr;
    QDoubleSpinBox* trainingLearningRateSpin_ = nullptr;
    QSpinBox* trainingNumClassesSpin_ = nullptr;
    QSpinBox* trainingWarmupSpin_ = nullptr;
    QLineEdit* trainingResumePathEdit_ = nullptr;
    QLineEdit* trainingExportWeightEdit_ = nullptr;
    QLineEdit* trainingExportDirEdit_ = nullptr;
    QLabel* trainingHeaderProjectLabel_ = nullptr;
    QLabel* trainingHeaderStatusLabel_ = nullptr;
    QLabel* trainingDetailTitleLabel_ = nullptr;
    QLabel* trainingDetailSubtitleLabel_ = nullptr;
    QLabel* trainingProgressMetaLabel_ = nullptr;
    QProgressBar* trainingProgressBar_ = nullptr;
    TrainingPlaceholderChart* trainingLossChart_ = nullptr;
    TrainingPlaceholderChart* trainingAccuracyChart_ = nullptr;
    TrainingPlaceholderChart* trainingLrChart_ = nullptr;
    QLabel* trainingVersionSummary_ = nullptr;
    QListWidget* trainingVersionList_ = nullptr;
    QLabel* trainingMetricSummaryLabel_ = nullptr;
    QTableWidget* trainingMetricsTable_ = nullptr;
    QPlainTextEdit* trainingVersionDetail_ = nullptr;
    QTableWidget* trainingVersionManagerTable_ = nullptr;
    QPushButton* startTrainingButton_ = nullptr;
    QPushButton* stopTrainingButton_ = nullptr;
    QProcess* trainingProcess_ = nullptr;
    QLineEdit* predictInputEdit_ = nullptr;
    QLineEdit* predictOutputEdit_ = nullptr;
    QComboBox* predictDetModelCombo_ = nullptr;
    QComboBox* predictRecModelCombo_ = nullptr;
    QComboBox* predictClsTaskCombo_ = nullptr;
    QComboBox* predictClsModelCombo_ = nullptr;
    QComboBox* predictLayoutModelCombo_ = nullptr;
    QComboBox* predictModeCombo_ = nullptr;
    QLineEdit* predictionModelSearchEdit_ = nullptr;
    QComboBox* predictionModelKindFilter_ = nullptr;
    QWidget* predictionModelHost_ = nullptr;
    QHBoxLayout* predictionModelLayout_ = nullptr;
    QLabel* predictionActiveModelTitleLabel_ = nullptr;
    QLabel* predictionActiveModelPathLabel_ = nullptr;
    QComboBox* predictDeviceCombo_ = nullptr;
    QSpinBox* predictBatchSpin_ = nullptr;
    QComboBox* predictRunModeCombo_ = nullptr;
    QPlainTextEdit* predictExtraOptionsEdit_ = nullptr;
    QDoubleSpinBox* predictScoreThresholdSpin_ = nullptr;
    QCheckBox* predictSaveJsonCheck_ = nullptr;
    QCheckBox* predictSaveVisualCheck_ = nullptr;
    QLabel* predictionStatusLabel_ = nullptr;
    QLabel* predictionResultTotalLabel_ = nullptr;
    QLabel* predictionResultConfidenceLabel_ = nullptr;
    QLabel* predictionElapsedLabel_ = nullptr;
    QLabel* predictionImagePreview_ = nullptr;
    QLabel* predictionImagePathLabel_ = nullptr;
    QTableWidget* predictionResultTable_ = nullptr;
    QPushButton* startPredictionButton_ = nullptr;
    QPushButton* startClsPredictionButton_ = nullptr;
    QPushButton* startLayoutPredictionButton_ = nullptr;
    QPushButton* stopPredictionButton_ = nullptr;
    QProcess* predictionProcess_ = nullptr;
    QThread* activePrelabelThread_ = nullptr;
    QThread* activeExportThread_ = nullptr;
    bool prelabelAllRunning_ = false;
    bool exportRunning_ = false;
    QPlainTextEdit* logEdit_ = nullptr;
    QPlainTextEdit* trainingPreview_ = nullptr;
    QPlainTextEdit* predictionPreview_ = nullptr;
    QPlainTextEdit* predictionStructuredText_ = nullptr;
    bool updatingPageMetadata_ = false;
};

}  // namespace ppocr
