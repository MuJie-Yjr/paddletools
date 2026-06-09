#pragma once

#include "core/ProjectTypes.h"
#include "core/TrainingTasks.h"
#include "paddle/PaddleProcess.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace ppocr {

struct TrainingOptions {
    QString pythonExe = "python";
    QString device = "cpu";
    int epochs = 0;
    int batchSize = 0;
    double learningRate = 0.0;
    int numClasses = 0;
    int warmupSteps = 0;
    QString resumePath;
    bool checkedOnly = true;
    bool requireValidation = true;
};

struct TrainingPreflightResult {
    bool ok = false;
    QStringList errors;
    QString previewVersionId;
    QString outputDir;
    QString datasetDir;
    int sampleCount = 0;
    int trainSampleCount = 0;
    int valSampleCount = 0;
    PaddleCommand command;
    QJsonObject report;
};

class TrainingPreflight {
public:
    static PaddleCommand buildCommand(
        const QString& baseDir,
        const ProjectContext* context,
        const QString& taskKey,
        const QString& outputDir,
        const TrainingOptions& options);

    static TrainingPreflightResult run(
        const QString& baseDir,
        const ProjectContext& context,
        const QString& taskKey,
        const TrainingOptions& options,
        const QString& previewVersionId = QString());

    static int exportedSampleCount(const QString& datasetDir);
    static QJsonObject exportedSampleCounts(const QString& datasetDir);
    static QJsonObject pathStatus(const QString& path);
};

}  // namespace ppocr
