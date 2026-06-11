#pragma once

#include "domain/ModelVersion.h"
#include "domain/Project.h"
#include "domain/TrainingTask.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>

namespace ppocr {

struct ModelRegistryRow {
    QString taskKey;
    QString taskTitle;
    QString versionId;
    QString status;
    QString metric;
    QString current;
    QString best;
    QString modelDir;
    QString finished;
};

struct ModelRegistrySummary {
    int trainableTasks = 0;
    int versionCount = 0;
    int usableModels = 0;
    int currentCount = 0;
    int bestCount = 0;
    QList<ModelRegistryRow> rows;
};

class ModelRegistryService {
public:
    static ModelRegistrySummary summarize(const ProjectContext& context);
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

private:
    static QString statusText(const QString& status);
    static QString metricText(const QJsonObject& version);
    static QJsonObject summarizeVersion(const TrainingTaskSpec& task, const QJsonObject& version);
    static QDateTime objectTime(const QJsonObject& object);
};

}  // namespace ppocr
