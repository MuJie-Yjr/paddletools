#pragma once

#include "core/ProjectTypes.h"
#include "core/TrainingPreflight.h"
#include "core/TrainingTasks.h"

#include <QJsonObject>
#include <QString>

namespace ppocr {

struct TrainingRunStart {
    bool ok = false;
    QStringList errors;
    TrainingTaskSpec task;
    TrainingPreflightResult preflight;
    QString runId;
    QString versionId;
    QString versionDir;
    QString device;
    QJsonObject report;
};

struct TrainingRunResult {
    bool ok = false;
    RunStatus status = RunStatus::Pending;
    int exitCode = 0;
    QString errorSummary;
    QString logText;
    QJsonObject metrics;
    QJsonObject finishedVersion;
    QJsonObject report;
};

class TrainingRunner {
public:
    static TrainingRunStart prepare(
        const QString& baseDir,
        const ProjectContext& context,
        const QString& taskKey,
        const TrainingOptions& options,
        const QString& versionId = QString());

    static TrainingRunResult finish(
        const ProjectContext& context,
        const TrainingRunStart& start,
        RunStatus status,
        int exitCode,
        const QString& errorSummary,
        const QString& logText);

    static TrainingRunResult runBlocking(
        const QString& baseDir,
        const ProjectContext& context,
        const QString& taskKey,
        const TrainingOptions& options,
        int timeoutSeconds = 0,
        bool echoOutput = false,
        const QString& versionId = QString());

    static QJsonObject parseMetrics(const QString& logText);
};

}  // namespace ppocr
