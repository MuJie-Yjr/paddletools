#pragma once

#include "ProjectTypes.h"

#include <QJsonArray>
#include <QJsonObject>

namespace ppocr {

class TrainingRunStore {
public:
    static QJsonArray loadRuns(const ProjectContext& context);
    static QString newVersionId(const QString& taskKey);
    static QString versionDir(const ProjectContext& context, const QString& taskKey, const QString& versionId);
    static QJsonObject loadVersionManifest(const ProjectContext& context, const QString& taskKey);
    static QString startRun(
        const ProjectContext& context,
        const QString& taskKey,
        const QString& taskTitle,
        const QString& command,
        const QString& datasetDir = QString(),
        const QString& outputDir = QString(),
        const QString& device = QString(),
        int sampleCount = 0,
        const QString& versionId = QString(),
        const QString& versionDir = QString());
    static void finishRun(
        const ProjectContext& context,
        const QString& runId,
        RunStatus status,
        int exitCode = 0,
        const QString& errorSummary = QString(),
        const QJsonObject& metrics = QJsonObject());
    static QJsonObject latestRun(const ProjectContext& context, const QString& taskKey);
    static void startVersion(
        const ProjectContext& context,
        const QString& taskKey,
        const QString& taskTitle,
        TrainingTaskKind taskKind,
        const QString& versionId,
        const QString& command,
        const QString& runId = QString(),
        const QString& datasetDir = QString(),
        const QString& outputDir = QString(),
        const QString& device = QString(),
        int sampleCount = 0,
        const QString& note = QString());
    static QJsonObject finishVersion(
        const ProjectContext& context,
        const QString& taskKey,
        TrainingTaskKind taskKind,
        const QString& versionId,
        RunStatus status,
        int exitCode = 0,
        const QString& errorSummary = QString(),
        const QJsonObject& metrics = QJsonObject(),
        const QString& bestWeightRel = "best_accuracy/best_accuracy.pdparams",
        const QString& inferDirRel = "best_accuracy/inference");
    static QJsonObject setCurrentVersion(const ProjectContext& context, const QString& taskKey, const QString& versionId);
    static void deleteVersion(
        const ProjectContext& context,
        const QString& taskKey,
        const QString& versionId,
        bool deleteFiles = true,
        bool allowDeleteBest = false);
    static double mainMetricValue(const QJsonObject& metrics, TrainingTaskKind taskKind, bool* ok = nullptr);
    static double mainMetricValue(const QJsonObject& metrics, const QString& taskKind, bool* ok = nullptr);

private:
    static QString runsPath(const ProjectContext& context);
    static QString versionsPath(const ProjectContext& context, const QString& taskKey);
    static void writeRuns(const ProjectContext& context, const QJsonArray& runs);
    static void writeVersionManifest(const ProjectContext& context, const QString& taskKey, QJsonObject manifest);
    static QString nowText();
};

}  // namespace ppocr
