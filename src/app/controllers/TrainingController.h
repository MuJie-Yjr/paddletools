#pragma once

#include "application/TrainingService.h"

#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QString>
#include <memory>

namespace ppocr {

struct TrainingRunStart;

class TrainingController : public QObject {
    Q_OBJECT

public:
    explicit TrainingController(QString baseDir, QObject* parent = nullptr);
    ~TrainingController() override;

    bool isRunning() const;

    PaddleCommand buildCommand(
        const ProjectContext* context,
        const QString& taskKey,
        const QString& outputDir,
        const TrainingOptions& options) const;

    TrainingPreflightResult runPreflight(
        const ProjectContext& context,
        const QString& taskKey,
        const TrainingOptions& options,
        const QString& previewVersionId = QString()) const;

    QJsonObject parseMetrics(const QString& logText) const;

    bool startTraining(
        const ProjectContext& context,
        const QString& taskKey,
        const TrainingOptions& options,
        QString* errorMessage = nullptr);

public slots:
    void stopTraining();

signals:
    void trainingPrepared(
        const QString& taskKey,
        const QString& taskKind,
        const QString& runId,
        const QString& versionId,
        const QString& datasetDir,
        const QString& versionDir,
        const QString& commandText);
    void logTextReady(const QString& text);
    void metricsTextReady(const QString& text);
    void runningChanged(bool running);
    void trainingFinished(
        const QString& status,
        int exitCode,
        const QString& errorSummary,
        const QJsonObject& metrics,
        const QJsonObject& finishedVersion,
        const QString& versionId);

private slots:
    void handleOutput();
    void handleFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleError(QProcess::ProcessError error);

private:
    void finalize(RunStatus status, int exitCode, const QString& errorSummary);
    void cleanupProcess();

    QString baseDir_;
    TrainingService service_;
    ProjectContext activeContext_;
    std::unique_ptr<TrainingRunStart> activeStart_;
    QProcess* process_ = nullptr;
    QString logBuffer_;
    bool stopRequested_ = false;
    bool finalized_ = false;
};

}  // namespace ppocr
