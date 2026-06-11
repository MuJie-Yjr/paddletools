#pragma once

#include "core/TrainingPreflight.h"
#include "core/TrainingRunner.h"

#include <QList>
#include <QJsonObject>
#include <QString>

namespace ppocr {

class TrainingService {
public:
    explicit TrainingService(QString baseDir);

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

    TrainingRunStart prepare(
        const ProjectContext& context,
        const QString& taskKey,
        const TrainingOptions& options) const;

    TrainingRunResult finish(
        const ProjectContext& context,
        const TrainingRunStart& start,
        RunStatus status,
        int exitCode,
        const QString& errorSummary,
        const QString& logText) const;

    QJsonObject parseMetrics(const QString& logText) const;

    static QList<TrainingTaskSpec> trainingTasks();
    static TrainingTaskSpec trainingTaskByKey(const QString& taskKey);
    static QString defaultBaseDir();
    static QString defaultPaddlexPython();
    static QJsonObject loadVersionManifest(const ProjectContext& context, const QString& taskKey);
    static double mainMetricValue(const QJsonObject& metrics, TrainingTaskKind taskKind, bool* ok = nullptr);
    static double mainMetricValue(const QJsonObject& metrics, const QString& taskKind, bool* ok = nullptr);

private:
    QString baseDir_;
};

}  // namespace ppocr
