#include "app/controllers/ModelLibraryController.h"

namespace ppocr {

ModelLibrarySummary ModelLibraryController::summarize(const ProjectContext& context) {
    return ModelRegistryService::summarize(context);
}

QJsonObject ModelLibraryController::loadManifest(const ProjectContext& context, const QString& taskKey) {
    return ModelRegistryService::loadManifest(context, taskKey);
}

QJsonArray ModelLibraryController::loadRuns(const ProjectContext& context) {
    return ModelRegistryService::loadRuns(context);
}

QJsonObject ModelLibraryController::latestRun(const ProjectContext& context, const QString& taskKey) {
    return ModelRegistryService::latestRun(context, taskKey);
}

QJsonObject ModelLibraryController::findVersion(const ProjectContext& context, const QString& taskKey, const QString& versionId) {
    return ModelRegistryService::findVersion(context, taskKey, versionId);
}

QJsonObject ModelLibraryController::setCurrentVersion(const ProjectContext& context, const QString& taskKey, const QString& versionId) {
    return ModelRegistryService::setCurrentVersion(context, taskKey, versionId);
}

void ModelLibraryController::deleteVersion(
    const ProjectContext& context,
    const QString& taskKey,
    const QString& versionId,
    bool deleteFiles,
    bool allowDeleteBest) {
    ModelRegistryService::deleteVersion(context, taskKey, versionId, deleteFiles, allowDeleteBest);
}

QString ModelLibraryController::newVersionId(const QString& taskKey) {
    return ModelRegistryService::newVersionId(taskKey);
}

QString ModelLibraryController::versionDir(const ProjectContext& context, const QString& taskKey, const QString& versionId) {
    return ModelRegistryService::versionDir(context, taskKey, versionId);
}

QString ModelLibraryController::resolvedInferenceDir(const TrainingTaskSpec& task, const QJsonObject& version) {
    return ModelRegistryService::resolvedInferenceDir(task, version);
}

QJsonObject ModelLibraryController::compareVersions(
    const ProjectContext& context,
    const TrainingTaskSpec& task,
    const QString& selectedVersionId) {
    return ModelRegistryService::compareVersions(context, task, selectedVersionId);
}

}  // namespace ppocr
