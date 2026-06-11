#include "application/TrainingService.h"

#include "core/RuntimePaths.h"
#include "core/TrainingRunStore.h"
#include "core/TrainingTasks.h"

#include <utility>

namespace ppocr {

TrainingService::TrainingService(QString baseDir)
    : baseDir_(std::move(baseDir)) {}

PaddleCommand TrainingService::buildCommand(
    const ProjectContext* context,
    const QString& taskKey,
    const QString& outputDir,
    const TrainingOptions& options) const {
    return TrainingPreflight::buildCommand(baseDir_, context, taskKey, outputDir, options);
}

TrainingPreflightResult TrainingService::runPreflight(
    const ProjectContext& context,
    const QString& taskKey,
    const TrainingOptions& options,
    const QString& previewVersionId) const {
    return TrainingPreflight::run(baseDir_, context, taskKey, options, previewVersionId);
}

TrainingRunStart TrainingService::prepare(
    const ProjectContext& context,
    const QString& taskKey,
    const TrainingOptions& options) const {
    return TrainingRunner::prepare(baseDir_, context, taskKey, options);
}

TrainingRunResult TrainingService::finish(
    const ProjectContext& context,
    const TrainingRunStart& start,
    RunStatus status,
    int exitCode,
    const QString& errorSummary,
    const QString& logText) const {
    return TrainingRunner::finish(context, start, status, exitCode, errorSummary, logText);
}

QJsonObject TrainingService::parseMetrics(const QString& logText) const {
    return TrainingRunner::parseMetrics(logText);
}

QList<TrainingTaskSpec> TrainingService::trainingTasks() {
    return ppocr::trainingTasks();
}

TrainingTaskSpec TrainingService::trainingTaskByKey(const QString& taskKey) {
    return ppocr::trainingTaskByKey(taskKey);
}

QString TrainingService::defaultBaseDir() {
    return RuntimePaths::defaultBaseDir();
}

QString TrainingService::defaultPaddlexPython() {
    return RuntimePaths::defaultPaddlexPython();
}

QJsonObject TrainingService::loadVersionManifest(const ProjectContext& context, const QString& taskKey) {
    return TrainingRunStore::loadVersionManifest(context, taskKey);
}

double TrainingService::mainMetricValue(const QJsonObject& metrics, TrainingTaskKind taskKind, bool* ok) {
    return TrainingRunStore::mainMetricValue(metrics, taskKind, ok);
}

double TrainingService::mainMetricValue(const QJsonObject& metrics, const QString& taskKind, bool* ok) {
    return TrainingRunStore::mainMetricValue(metrics, taskKind, ok);
}

}  // namespace ppocr
