#include "core/TrainingRunStore.h"

#include "core/JsonUtils.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QSet>
#include <QUuid>
#include <stdexcept>

namespace ppocr {
namespace {

QJsonObject emptyVersionManifest(const QString& taskKey) {
    return QJsonObject{
        {"task_key", taskKey},
        {"current_version_id", ""},
        {"best_version_id", ""},
        {"versions", QJsonArray{}},
    };
}

QString sortKey(const QJsonObject& item) {
    const QString finished = item.value("finished_at").toString();
    if (!finished.isEmpty()) {
        return finished;
    }
    return item.value("started_at").toString();
}

int findVersionIndex(const QJsonArray& versions, const QString& versionId) {
    if (versionId.isEmpty()) {
        return -1;
    }
    for (int i = 0; i < versions.size(); ++i) {
        if (versions.at(i).toObject().value("version_id").toString() == versionId) {
            return i;
        }
    }
    return -1;
}

void syncVersionFlags(QJsonObject* manifest) {
    const QString currentId = manifest->value("current_version_id").toString();
    const QString bestId = manifest->value("best_version_id").toString();
    QJsonArray versions = manifest->value("versions").toArray();
    for (int i = 0; i < versions.size(); ++i) {
        QJsonObject version = versions.at(i).toObject();
        const QString versionId = version.value("version_id").toString();
        version["is_current"] = !currentId.isEmpty() && versionId == currentId;
        version["is_best"] = !bestId.isEmpty() && versionId == bestId;
        versions.replace(i, version);
    }
    (*manifest)["versions"] = versions;
}

bool hasPaddleModelFiles(const QString& dirPath) {
    if (dirPath.isEmpty()) {
        return false;
    }
    const QDir dir(dirPath);
    return QFileInfo::exists(dir.filePath("inference.pdiparams"))
        && (QFileInfo::exists(dir.filePath("inference.json")) || QFileInfo::exists(dir.filePath("inference.pdmodel")));
}

QString findLatestInferenceDir(const QString& rootPath) {
    if (rootPath.isEmpty()) {
        return {};
    }
    QFileInfo rootInfo(rootPath);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        return {};
    }
    QString best;
    QDateTime bestTime;
    if (hasPaddleModelFiles(rootInfo.absoluteFilePath())) {
        best = rootInfo.absoluteFilePath();
        bestTime = rootInfo.lastModified();
    }
    QDirIterator it(rootInfo.absoluteFilePath(), QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (QFileInfo(path).fileName() != "inference" || !hasPaddleModelFiles(path)) {
            continue;
        }
        const QDateTime modified = QFileInfo(path).lastModified();
        if (best.isEmpty() || modified > bestTime) {
            best = QFileInfo(path).absoluteFilePath();
            bestTime = modified;
        }
    }
    return best;
}

QString findLatestWeightFile(const QString& rootPath) {
    if (rootPath.isEmpty()) {
        return {};
    }
    QFileInfo rootInfo(rootPath);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        return {};
    }
    QString best;
    QDateTime bestTime;
    QDirIterator it(rootInfo.absoluteFilePath(), {"*.pdparams"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QFileInfo info(path);
        if (best.isEmpty() || info.lastModified() > bestTime) {
            best = info.absoluteFilePath();
            bestTime = info.lastModified();
        }
    }
    return best;
}

int durationSeconds(const QString& startedAt, const QString& finishedAt) {
    const auto started = QDateTime::fromString(startedAt, "yyyy-MM-dd HH:mm:ss");
    const auto finished = QDateTime::fromString(finishedAt, "yyyy-MM-dd HH:mm:ss");
    if (!started.isValid() || !finished.isValid()) {
        return 0;
    }
    return qMax<qint64>(0, started.secsTo(finished));
}

QString bestRemainingVersionId(const QJsonObject& manifest) {
    QString bestId;
    double bestScore = 0.0;
    bool hasBest = false;
    for (const auto& value : manifest.value("versions").toArray()) {
        const QJsonObject version = value.toObject();
        const auto status = runStatusFromString(version.value("status").toString()).value_or(RunStatus::Pending);
        if (status != RunStatus::Finished) {
            continue;
        }
        bool ok = false;
        const double score = TrainingRunStore::mainMetricValue(
            version.value("metrics").toObject(),
            version.value("task_kind").toString(),
            &ok);
        if (ok && (!hasBest || score > bestScore)) {
            bestScore = score;
            bestId = version.value("version_id").toString();
            hasBest = true;
        }
    }
    return bestId;
}

bool isInsideDirectory(const QString& childPath, const QString& parentPath) {
    const QDir parent(QFileInfo(parentPath).absoluteFilePath());
    const QString relative = parent.relativeFilePath(QFileInfo(childPath).absoluteFilePath());
    return !relative.isEmpty()
        && relative != "."
        && !relative.startsWith("..")
        && !QFileInfo(relative).isAbsolute();
}

}  // namespace

QString TrainingRunStore::runsPath(const ProjectContext& context) {
    return context.path("training/runs.json");
}

QString TrainingRunStore::versionsPath(const ProjectContext& context, const QString& taskKey) {
    return context.path(QString("training/%1/versions.json").arg(taskKey));
}

QJsonArray TrainingRunStore::loadRuns(const ProjectContext& context) {
    QFile file(runsPath(context));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        return {};
    }
    if (doc.isArray()) {
        return doc.array();
    }
    if (doc.isObject()) {
        return doc.object().value("runs").toArray();
    }
    return {};
}

QString TrainingRunStore::newVersionId(const QString& taskKey) {
    return QString("%1_%2_%3")
        .arg(taskKey, QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"), QUuid::createUuid().toString(QUuid::Id128).left(8));
}

QString TrainingRunStore::versionDir(const ProjectContext& context, const QString& taskKey, const QString& versionId) {
    return context.path(QString("training/%1/versions/%2").arg(taskKey, versionId));
}

QJsonObject TrainingRunStore::loadVersionManifest(const ProjectContext& context, const QString& taskKey) {
    QJsonObject manifest = emptyVersionManifest(taskKey);
    QJsonObject loaded;
    if (readJsonObject(versionsPath(context, taskKey), &loaded)) {
        manifest["task_key"] = loaded.value("task_key").toString(taskKey);
        manifest["current_version_id"] = loaded.value("current_version_id").toString();
        manifest["best_version_id"] = loaded.value("best_version_id").toString();
        manifest["versions"] = loaded.value("versions").toArray();
    }
    syncVersionFlags(&manifest);
    return manifest;
}

QString TrainingRunStore::startRun(
    const ProjectContext& context,
    const QString& taskKey,
    const QString& taskTitle,
    const QString& command,
    const QString& datasetDir,
    const QString& outputDir,
    const QString& device,
    int sampleCount,
    const QString& versionId,
    const QString& versionDir) {
    QJsonArray runs = loadRuns(context);
    const QString runId = newVersionId(taskKey);
    runs.append(QJsonObject{
        {"run_id", runId},
        {"task_key", taskKey},
        {"task_title", taskTitle.isEmpty() ? taskKey : taskTitle},
        {"status", toString(RunStatus::Running)},
        {"started_at", nowText()},
        {"finished_at", ""},
        {"duration_seconds", 0},
        {"command", command},
        {"dataset_dir", datasetDir},
        {"output_dir", outputDir},
        {"device", device},
        {"sample_count", sampleCount},
        {"exit_code", QJsonValue::Null},
        {"error_summary", ""},
        {"metrics", QJsonObject{}},
        {"version_id", versionId},
        {"version_dir", versionDir},
        {"is_current", false},
        {"is_best", false},
    });
    writeRuns(context, runs);
    return runId;
}

void TrainingRunStore::finishRun(
    const ProjectContext& context,
    const QString& runId,
    RunStatus status,
    int exitCode,
    const QString& errorSummary,
    const QJsonObject& metrics) {
    QJsonArray runs = loadRuns(context);
    bool found = false;
    const QString finishedAt = nowText();
    const QString statusText = toString(status);
    for (int i = 0; i < runs.size(); ++i) {
        QJsonObject run = runs.at(i).toObject();
        if (run.value("run_id").toString() != runId) {
            continue;
        }
        run["status"] = statusText;
        run["finished_at"] = finishedAt;
        run["duration_seconds"] = durationSeconds(run.value("started_at").toString(), finishedAt);
        run["exit_code"] = exitCode;
        run["error_summary"] = errorSummary;
        run["metrics"] = metrics;
        runs.replace(i, run);
        found = true;
        break;
    }
    if (!found) {
        runs.append(QJsonObject{
            {"run_id", runId},
            {"task_key", ""},
            {"task_title", ""},
            {"status", statusText},
            {"started_at", ""},
            {"finished_at", finishedAt},
            {"duration_seconds", 0},
            {"command", ""},
            {"dataset_dir", ""},
            {"output_dir", ""},
            {"device", ""},
            {"sample_count", 0},
            {"exit_code", exitCode},
            {"error_summary", errorSummary},
            {"metrics", metrics},
            {"version_id", ""},
            {"version_dir", ""},
            {"is_current", false},
            {"is_best", false},
        });
    }
    writeRuns(context, runs);
}

QJsonObject TrainingRunStore::latestRun(const ProjectContext& context, const QString& taskKey) {
    QJsonObject latest;
    QString latestKey;
    int latestIndex = -1;
    const QJsonArray runs = loadRuns(context);
    for (int i = 0; i < runs.size(); ++i) {
        const auto value = runs.at(i);
        const auto run = value.toObject();
        if (run.value("task_key").toString() != taskKey) {
            continue;
        }
        const QString key = sortKey(run);
        if (latestIndex < 0 || key > latestKey || (key == latestKey && i > latestIndex)) {
            latest = run;
            latestKey = key;
            latestIndex = i;
        }
    }
    return latest;
}

void TrainingRunStore::startVersion(
    const ProjectContext& context,
    const QString& taskKey,
    const QString& taskTitle,
    TrainingTaskKind taskKind,
    const QString& versionId,
    const QString& command,
    const QString& runId,
    const QString& datasetDir,
    const QString& outputDir,
    const QString& device,
    int sampleCount,
    const QString& note) {
    QJsonObject manifest = loadVersionManifest(context, taskKey);
    QJsonArray versions = manifest.value("versions").toArray();
    const int existing = findVersionIndex(versions, versionId);
    const QString now = nowText();
    const QJsonObject payload{
        {"version_id", versionId},
        {"version_label", ""},
        {"task_key", taskKey},
        {"task_title", taskTitle.isEmpty() ? taskKey : taskTitle},
        {"task_kind", toString(taskKind)},
        {"status", toString(RunStatus::Running)},
        {"started_at", now},
        {"finished_at", ""},
        {"dataset_dir", datasetDir},
        {"output_dir", outputDir},
        {"version_dir", outputDir},
        {"device", device},
        {"command", command},
        {"exit_code", QJsonValue::Null},
        {"error_summary", ""},
        {"metrics", QJsonObject{}},
        {"sample_count", sampleCount},
        {"note", note},
        {"best_weight_path", ""},
        {"inference_model_dir", ""},
        {"run_id", runId},
        {"is_current", false},
        {"is_best", false},
    };
    if (existing >= 0) {
        versions.replace(existing, payload);
    } else {
        versions.append(payload);
    }
    manifest["versions"] = versions;
    writeVersionManifest(context, taskKey, manifest);
}

QJsonObject TrainingRunStore::finishVersion(
    const ProjectContext& context,
    const QString& taskKey,
    TrainingTaskKind taskKind,
    const QString& versionId,
    RunStatus status,
    int exitCode,
    const QString& errorSummary,
    const QJsonObject& metrics,
    const QString& bestWeightRel,
    const QString& inferDirRel) {
    QJsonObject manifest = loadVersionManifest(context, taskKey);
    QJsonArray versions = manifest.value("versions").toArray();
    const int index = findVersionIndex(versions, versionId);
    if (index < 0) {
        return {};
    }

    QJsonObject version = versions.at(index).toObject();
    const QString outputDir = version.value("version_dir").toString(version.value("output_dir").toString());
    const QString finishedAt = nowText();
    QString bestWeightPath;
    QString inferenceModelDir;
    if (!outputDir.isEmpty()) {
        const QString preferredWeight = QFileInfo(QDir(outputDir).filePath(bestWeightRel)).absoluteFilePath();
        bestWeightPath = QFileInfo::exists(preferredWeight) ? preferredWeight : findLatestWeightFile(outputDir);
        const QString preferred = QFileInfo(QDir(outputDir).filePath(inferDirRel)).absoluteFilePath();
        inferenceModelDir = hasPaddleModelFiles(preferred) ? preferred : findLatestInferenceDir(outputDir);
    }

    const QString statusText = toString(status);
    version["status"] = statusText;
    version["task_kind"] = toString(taskKind);
    version["finished_at"] = finishedAt;
    version["exit_code"] = exitCode;
    version["error_summary"] = errorSummary;
    version["metrics"] = metrics;
    version["best_weight_path"] = bestWeightPath;
    version["inference_model_dir"] = inferenceModelDir;
    versions.replace(index, version);
    manifest["versions"] = versions;

    if (status == RunStatus::Finished) {
        if (manifest.value("current_version_id").toString().isEmpty()) {
            manifest["current_version_id"] = versionId;
        }
        const QString bestId = manifest.value("best_version_id").toString();
        if (bestId.isEmpty()) {
            manifest["best_version_id"] = versionId;
        } else {
            versions = manifest.value("versions").toArray();
            const int bestIndex = findVersionIndex(versions, bestId);
            bool candidateOk = false;
            bool bestOk = false;
            const double candidateScore = mainMetricValue(metrics, taskKind, &candidateOk);
            const double bestScore = bestIndex >= 0
                ? mainMetricValue(versions.at(bestIndex).toObject().value("metrics").toObject(), taskKind, &bestOk)
                : 0.0;
            if (candidateOk && (!bestOk || candidateScore > bestScore)) {
                manifest["current_version_id"] = versionId;
                manifest["best_version_id"] = versionId;
            }
        }
    }

    writeVersionManifest(context, taskKey, manifest);
    const QJsonObject updatedManifest = loadVersionManifest(context, taskKey);
    const QJsonArray updatedVersions = updatedManifest.value("versions").toArray();
    const int updatedIndex = findVersionIndex(updatedVersions, versionId);
    return updatedIndex >= 0 ? updatedVersions.at(updatedIndex).toObject() : QJsonObject{};
}

QJsonObject TrainingRunStore::setCurrentVersion(const ProjectContext& context, const QString& taskKey, const QString& versionId) {
    QJsonObject manifest = loadVersionManifest(context, taskKey);
    QJsonArray versions = manifest.value("versions").toArray();
    const int index = findVersionIndex(versions, versionId);
    if (index < 0) {
        throw std::runtime_error(QString("unknown training version: %1").arg(versionId).toStdString());
    }
    QJsonObject version = versions.at(index).toObject();
    const auto status = runStatusFromString(version.value("status").toString()).value_or(RunStatus::Pending);
    if (status != RunStatus::Finished) {
        throw std::runtime_error("only successful versions can be set current");
    }
    manifest["current_version_id"] = versionId;
    writeVersionManifest(context, taskKey, manifest);
    return loadVersionManifest(context, taskKey).value("versions").toArray().at(index).toObject();
}

void TrainingRunStore::deleteVersion(
    const ProjectContext& context,
    const QString& taskKey,
    const QString& versionId,
    bool deleteFiles,
    bool allowDeleteBest) {
    QJsonObject manifest = loadVersionManifest(context, taskKey);
    if (manifest.value("current_version_id").toString() == versionId) {
        throw std::runtime_error("current version cannot be deleted");
    }
    if (manifest.value("best_version_id").toString() == versionId && !allowDeleteBest) {
        throw std::runtime_error("best version deletion requires confirmation");
    }

    QJsonArray versions = manifest.value("versions").toArray();
    const int index = findVersionIndex(versions, versionId);
    if (index < 0) {
        throw std::runtime_error(QString("unknown training version: %1").arg(versionId).toStdString());
    }
    const QJsonObject version = versions.at(index).toObject();
    versions.removeAt(index);
    manifest["versions"] = versions;
    if (manifest.value("best_version_id").toString() == versionId) {
        manifest["best_version_id"] = bestRemainingVersionId(manifest);
    }
    writeVersionManifest(context, taskKey, manifest);

    if (!deleteFiles) {
        return;
    }
    const QString versionPath = version.value("version_dir").toString(version.value("output_dir").toString());
    if (versionPath.isEmpty()) {
        return;
    }
    QFileInfo versionInfo(versionPath);
    if (!versionInfo.exists() || !versionInfo.isDir()) {
        return;
    }
    const QString allowedRoot = context.path(QString("training/%1/versions").arg(taskKey));
    if (!isInsideDirectory(versionInfo.absoluteFilePath(), allowedRoot)) {
        throw std::runtime_error(QString("refusing to delete version directory outside versions root: %1").arg(versionInfo.absoluteFilePath()).toStdString());
    }
    if (!QDir(versionInfo.absoluteFilePath()).removeRecursively()) {
        throw std::runtime_error(QString("failed to delete version directory: %1").arg(versionInfo.absoluteFilePath()).toStdString());
    }
}

double TrainingRunStore::mainMetricValue(const QJsonObject& metrics, TrainingTaskKind taskKind, bool* ok) {
    QStringList keys;
    switch (taskKind) {
    case TrainingTaskKind::OcrDet:
        keys = {QStringLiteral("hmean"), QStringLiteral("score")};
        break;
    case TrainingTaskKind::OcrRec:
    case TrainingTaskKind::DocCls:
    case TrainingTaskKind::TextlineCls:
    case TrainingTaskKind::TableCls:
        keys = {QStringLiteral("acc"), QStringLiteral("accuracy"), QStringLiteral("score")};
        break;
    case TrainingTaskKind::Layout:
        keys = {QStringLiteral("mAP"), QStringLiteral("map"), QStringLiteral("score")};
        break;
    case TrainingTaskKind::Unknown:
        keys = {QStringLiteral("score")};
        break;
    }
    for (const auto& key : keys) {
        const QJsonValue value = metrics.value(key);
        if (value.isUndefined() || value.isNull()) {
            continue;
        }
        bool converted = false;
        double number = 0.0;
        if (value.isDouble()) {
            number = value.toDouble();
            converted = true;
        } else if (value.isString()) {
            number = value.toString().toDouble(&converted);
        }
        if (converted) {
            if (ok) {
                *ok = true;
            }
            return number;
        }
    }
    if (ok) {
        *ok = false;
    }
    return 0.0;
}

double TrainingRunStore::mainMetricValue(const QJsonObject& metrics, const QString& taskKind, bool* ok) {
    const auto parsed = trainingTaskKindFromString(taskKind).value_or(TrainingTaskKind::Unknown);
    return mainMetricValue(metrics, parsed, ok);
}

void TrainingRunStore::writeRuns(const ProjectContext& context, const QJsonArray& runs) {
    const QString path = runsPath(context);
    QFileInfo info(path);
    QDir().mkpath(info.absolutePath());
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(runs).toJson(QJsonDocument::Indented));
    }
}

void TrainingRunStore::writeVersionManifest(const ProjectContext& context, const QString& taskKey, QJsonObject manifest) {
    syncVersionFlags(&manifest);

    const QString currentId = manifest.value("current_version_id").toString();
    const QString bestId = manifest.value("best_version_id").toString();
    QJsonArray runs = loadRuns(context);
    bool changed = false;
    for (int i = 0; i < runs.size(); ++i) {
        QJsonObject run = runs.at(i).toObject();
        if (run.value("task_key").toString() != taskKey) {
            continue;
        }
        const QString runVersionId = run.value("version_id").toString();
        const bool isCurrent = !currentId.isEmpty() && runVersionId == currentId;
        const bool isBest = !bestId.isEmpty() && runVersionId == bestId;
        if (run.value("is_current").toBool() != isCurrent || run.value("is_best").toBool() != isBest) {
            run["is_current"] = isCurrent;
            run["is_best"] = isBest;
            runs.replace(i, run);
            changed = true;
        }
    }
    if (changed) {
        writeRuns(context, runs);
    }

    writeJsonObject(versionsPath(context, taskKey), manifest);
}

QString TrainingRunStore::nowText() {
    return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
}

}  // namespace ppocr
