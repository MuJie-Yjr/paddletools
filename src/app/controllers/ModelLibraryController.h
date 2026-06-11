#pragma once

#include "application/ModelRegistryService.h"

namespace ppocr {

using ModelLibraryRow = ModelRegistryRow;
using ModelLibrarySummary = ModelRegistrySummary;

class ModelLibraryController {
public:
    static ModelLibrarySummary summarize(const ProjectContext& context);
    static QJsonObject loadManifest(const ProjectContext& context, const QString& taskKey);
    static QJsonArray loadRuns(const ProjectContext& context);
    static QJsonObject latestRun(const ProjectContext& context, const QString& taskKey);
    static QJsonObject findVersion(const ProjectContext& context, const QString& taskKey, const QString& versionId);
    static QJsonObject setCurrentVersion(const ProjectContext& context, const QString& taskKey, const QString& versionId);
    static void deleteVersion(
        const ProjectContext& context,
        const QString& taskKey,
        const QString& versionId,
        bool deleteFiles = true,
        bool allowDeleteBest = false);
    static QString newVersionId(const QString& taskKey);
    static QString versionDir(const ProjectContext& context, const QString& taskKey, const QString& versionId);
    static QString resolvedInferenceDir(const TrainingTaskSpec& task, const QJsonObject& version);
    static QJsonObject compareVersions(
        const ProjectContext& context,
        const TrainingTaskSpec& task,
        const QString& selectedVersionId);

};

}  // namespace ppocr
