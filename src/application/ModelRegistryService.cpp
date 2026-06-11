#include "application/ModelRegistryService.h"

#include "core/TrainingTasks.h"
#include "core/TrainingRunStore.h"
#include "paddle/PaddleInferenceRuntime.h"

#include <QDir>
#include <QJsonArray>
#include <algorithm>

namespace ppocr {

namespace {

QDateTime parseStoreDateTime(const QString& text) {
    if (text.isEmpty()) {
        return {};
    }
    const QString clipped = text.left(19);
    QDateTime value = QDateTime::fromString(clipped, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!value.isValid()) {
        value = QDateTime::fromString(clipped, QStringLiteral("yyyy-MM-ddTHH:mm:ss"));
    }
    return value;
}

}  // namespace

ModelRegistrySummary ModelRegistryService::summarize(const ProjectContext& context) {
    struct RawRow {
        TrainingTaskSpec task;
        QJsonObject manifest;
        QJsonObject version;
        QDateTime time;
    };

    QList<RawRow> rawRows;
    ModelRegistrySummary summary;
    for (const auto& task : trainingTasks()) {
        if (task.trainSupported) {
            ++summary.trainableTasks;
        }
        const QJsonObject manifest = TrainingRunStore::loadVersionManifest(context, task.key);
        const QJsonArray versions = manifest.value(QStringLiteral("versions")).toArray();
        if (versions.isEmpty()) {
            rawRows.append(RawRow{task, manifest, QJsonObject{}, QDateTime{}});
            continue;
        }
        for (const auto& value : versions) {
            const QJsonObject version = value.toObject();
            rawRows.append(RawRow{task, manifest, version, objectTime(version)});
        }
    }

    std::sort(rawRows.begin(), rawRows.end(), [](const RawRow& left, const RawRow& right) {
        if (left.version.isEmpty() != right.version.isEmpty()) {
            return !left.version.isEmpty();
        }
        if (left.time.isValid() != right.time.isValid()) {
            return left.time.isValid();
        }
        if (left.time.isValid() && left.time != right.time) {
            return left.time > right.time;
        }
        return left.task.key < right.task.key;
    });

    for (const auto& row : rawRows) {
        ModelRegistryRow output;
        output.taskKey = row.task.key;
        output.taskTitle = row.task.title;
        output.versionId = QStringLiteral("-");
        output.status = row.task.trainSupported ? QStringLiteral("not trained") : QStringLiteral("unsupported");
        output.metric = QStringLiteral("-");
        output.current = QStringLiteral("-");
        output.best = QStringLiteral("-");
        output.modelDir = row.task.datasetName;
        output.finished = QStringLiteral("-");

        if (!row.version.isEmpty()) {
            ++summary.versionCount;
            output.versionId = row.version.value(QStringLiteral("version_id")).toString(QStringLiteral("-"));
            output.status = statusText(row.version.value(QStringLiteral("status")).toString());
            output.metric = metricText(row.version);
            const bool isCurrent = row.version.value(QStringLiteral("is_current")).toBool()
                || row.manifest.value(QStringLiteral("current_version_id")).toString() == output.versionId;
            const bool isBest = row.version.value(QStringLiteral("is_best")).toBool()
                || row.manifest.value(QStringLiteral("best_version_id")).toString() == output.versionId;
            if (isCurrent) {
                output.current = QStringLiteral("current");
                ++summary.currentCount;
            }
            if (isBest) {
                output.best = QStringLiteral("best");
                ++summary.bestCount;
            }

            QString reason;
            const QString inferenceDir = resolvedInferenceDir(row.task, row.version);
            if (!inferenceDir.isEmpty()) {
                output.modelDir = inferenceDir;
                if (PaddleInferenceRuntime::modelDirLooksUsable(inferenceDir, &reason)) {
                    ++summary.usableModels;
                }
            } else {
                output.modelDir = row.version.value(QStringLiteral("best_weight_path")).toString(
                    row.version.value(QStringLiteral("version_dir")).toString(
                        row.version.value(QStringLiteral("output_dir")).toString(row.task.datasetName)));
            }
            output.finished = row.version.value(QStringLiteral("finished_at")).toString(
                row.version.value(QStringLiteral("started_at")).toString(QStringLiteral("-")));
        }

        summary.rows.append(output);
    }
    return summary;
}

QJsonObject ModelRegistryService::loadManifest(const ProjectContext& context, const QString& taskKey) {
    return TrainingRunStore::loadVersionManifest(context, taskKey);
}

QJsonArray ModelRegistryService::loadRuns(const ProjectContext& context) {
    return TrainingRunStore::loadRuns(context);
}

QJsonObject ModelRegistryService::latestRun(const ProjectContext& context, const QString& taskKey) {
    return TrainingRunStore::latestRun(context, taskKey);
}

QJsonObject ModelRegistryService::findVersion(const ProjectContext& context, const QString& taskKey, const QString& versionId) {
    if (versionId.isEmpty()) {
        return {};
    }
    const QJsonObject manifest = loadManifest(context, taskKey);
    for (const auto& value : manifest.value(QStringLiteral("versions")).toArray()) {
        const QJsonObject version = value.toObject();
        if (version.value(QStringLiteral("version_id")).toString() == versionId) {
            return version;
        }
    }
    return {};
}

QJsonObject ModelRegistryService::setCurrentVersion(
    const ProjectContext& context,
    const QString& taskKey,
    const QString& versionId) {
    return TrainingRunStore::setCurrentVersion(context, taskKey, versionId);
}

void ModelRegistryService::deleteVersion(
    const ProjectContext& context,
    const QString& taskKey,
    const QString& versionId,
    bool deleteFiles,
    bool allowDeleteBest) {
    TrainingRunStore::deleteVersion(context, taskKey, versionId, deleteFiles, allowDeleteBest);
}

QString ModelRegistryService::newVersionId(const QString& taskKey) {
    return TrainingRunStore::newVersionId(taskKey);
}

QString ModelRegistryService::versionDir(const ProjectContext& context, const QString& taskKey, const QString& versionId) {
    return TrainingRunStore::versionDir(context, taskKey, versionId);
}

QJsonObject ModelRegistryService::compareVersions(
    const ProjectContext& context,
    const TrainingTaskSpec& task,
    const QString& selectedVersionId) {
    const QJsonObject manifest = loadManifest(context, task.key);
    auto findInManifest = [&manifest](const QString& versionId) {
        if (versionId.isEmpty()) {
            return QJsonObject{};
        }
        for (const auto& value : manifest.value(QStringLiteral("versions")).toArray()) {
            const QJsonObject version = value.toObject();
            if (version.value(QStringLiteral("version_id")).toString() == versionId) {
                return version;
            }
        }
        return QJsonObject{};
    };
    return QJsonObject{
        {QStringLiteral("task_key"), task.key},
        {QStringLiteral("selected"), summarizeVersion(task, findInManifest(selectedVersionId))},
        {QStringLiteral("current"), summarizeVersion(task, findInManifest(manifest.value(QStringLiteral("current_version_id")).toString()))},
        {QStringLiteral("best"), summarizeVersion(task, findInManifest(manifest.value(QStringLiteral("best_version_id")).toString()))},
    };
}

QString ModelRegistryService::statusText(const QString& status) {
    if (status == QStringLiteral("running")) {
        return QStringLiteral("running");
    }
    if (status == QStringLiteral("success")) {
        return QStringLiteral("success");
    }
    if (status == QStringLiteral("failed")) {
        return QStringLiteral("failed");
    }
    if (status == QStringLiteral("stopped")) {
        return QStringLiteral("stopped");
    }
    if (status.isEmpty()) {
        return QStringLiteral("not started");
    }
    return status;
}

QString ModelRegistryService::metricText(const QJsonObject& version) {
    bool ok = false;
    const double metric = TrainingRunStore::mainMetricValue(
        version.value(QStringLiteral("metrics")).toObject(),
        version.value(QStringLiteral("task_kind")).toString(),
        &ok);
    return ok ? QString::number(metric, 'f', 4) : QStringLiteral("-");
}

QString ModelRegistryService::resolvedInferenceDir(const TrainingTaskSpec& task, const QJsonObject& version) {
    const QString explicitDir = version.value(QStringLiteral("inference_model_dir")).toString();
    QString reason;
    if (!explicitDir.isEmpty() && PaddleInferenceRuntime::modelDirLooksUsable(explicitDir, &reason)) {
        return explicitDir;
    }
    const QString root = version.value(QStringLiteral("version_dir")).toString(version.value(QStringLiteral("output_dir")).toString());
    if (root.isEmpty()) {
        return {};
    }
    const QString preferred = QDir(root).filePath(task.inferDirRel);
    if (PaddleInferenceRuntime::modelDirLooksUsable(preferred, &reason)) {
        return preferred;
    }
    return {};
}

QJsonObject ModelRegistryService::summarizeVersion(const TrainingTaskSpec& task, const QJsonObject& version) {
    if (version.isEmpty()) {
        return {};
    }
    QString reason;
    const QString inferenceDir = resolvedInferenceDir(task, version);
    return QJsonObject{
        {QStringLiteral("version_id"), version.value(QStringLiteral("version_id")).toString()},
        {QStringLiteral("status"), version.value(QStringLiteral("status")).toString()},
        {QStringLiteral("started_at"), version.value(QStringLiteral("started_at")).toString()},
        {QStringLiteral("finished_at"), version.value(QStringLiteral("finished_at")).toString()},
        {QStringLiteral("sample_count"), version.value(QStringLiteral("sample_count")).toInt()},
        {QStringLiteral("metric"), metricText(version)},
        {QStringLiteral("best_weight_path"), version.value(QStringLiteral("best_weight_path")).toString()},
        {QStringLiteral("inference_model_dir"), inferenceDir},
        {QStringLiteral("inference_usable"), !inferenceDir.isEmpty() && PaddleInferenceRuntime::modelDirLooksUsable(inferenceDir, &reason)},
        {QStringLiteral("inference_error"), reason},
    };
}

QDateTime ModelRegistryService::objectTime(const QJsonObject& object) {
    QDateTime value = parseStoreDateTime(object.value(QStringLiteral("finished_at")).toString());
    if (!value.isValid()) {
        value = parseStoreDateTime(object.value(QStringLiteral("started_at")).toString());
    }
    return value;
}

}  // namespace ppocr
