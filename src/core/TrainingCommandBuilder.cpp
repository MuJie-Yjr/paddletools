#include "core/TrainingCommandBuilder.h"

#include "core/Exporter.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>

namespace ppocr {
namespace {

QString effectivePython(const TrainingOptions& options) {
    return options.pythonExe.trimmed().isEmpty() ? QStringLiteral("python") : options.pythonExe.trimmed();
}

QString effectiveDevice(const TrainingOptions& options) {
    return options.device.trimmed().isEmpty() ? QStringLiteral("cpu") : options.device.trimmed();
}

}  // namespace

PaddleCommand TrainingCommandBuilder::build(
    const QString& baseDir,
    const ProjectContext* context,
    const TrainingTaskSpec& task,
    const QString& outputDir,
    const TrainingOptions& options) {
    const QString datasetDir = context
        ? Exporter::datasetOutputDir(*context, task.datasetName)
        : QDir(baseDir).filePath(QStringLiteral("exports/%1/current").arg(task.datasetName));
    const int epochs = options.epochs > 0 ? options.epochs : task.epochs;
    const int batchSize = options.batchSize > 0 ? options.batchSize : task.batchSize;
    const double learningRate = options.learningRate > 0.0 ? options.learningRate : task.learningRate;

    QStringList overrides = {
        QStringLiteral("Global.mode=train"),
        QStringLiteral("Global.device=%1").arg(effectiveDevice(options)),
        QStringLiteral("Global.output=%1").arg(outputDir),
        QStringLiteral("Global.dataset_dir=%1").arg(datasetDir),
        QStringLiteral("Train.epochs_iters=%1").arg(epochs),
        QStringLiteral("Train.batch_size=%1").arg(batchSize),
        QStringLiteral("Train.learning_rate=%1").arg(QString::number(learningRate, 'g', 8)),
    };
    int numClasses = options.numClasses > 0 ? options.numClasses : task.numClasses;
    if (task.kind == TrainingTaskKind::Layout && context) {
        const int labelCount = context->config.value(QStringLiteral("label_sets"))
                                   .toObject()
                                   .value(QStringLiteral("layout"))
                                   .toArray()
                                   .size();
        if (labelCount > 0) {
            numClasses = labelCount;
        }
    }
    if ((isClassificationTrainingTaskKind(task.kind) || task.kind == TrainingTaskKind::Layout) && numClasses > 0) {
        overrides.append(QStringLiteral("Train.num_classes=%1").arg(numClasses));
    }
    const int warmupSteps = options.warmupSteps > 0 ? options.warmupSteps : task.warmupSteps;
    if (warmupSteps > 0) {
        overrides.append(QStringLiteral("Train.warmup_steps=%1").arg(warmupSteps));
    }
    const QString pretrainedWeight = localPretrainedWeightPath(baseDir, task);
    if (!pretrainedWeight.isEmpty()) {
        overrides.append(QStringLiteral("Train.pretrain_weight_path=%1").arg(pretrainedWeight));
    }
    const QString resumePath = options.resumePath.trimmed();
    if (!resumePath.isEmpty()) {
        overrides.append(QStringLiteral("Global.resume_path=%1").arg(resumePath));
    }
    return PaddleProcess::trainingCommand(
        baseDir,
        effectivePython(options),
        QDir(baseDir).filePath(task.configRel),
        overrides);
}

QString TrainingCommandBuilder::localPretrainedWeightPath(const QString& baseDir, const TrainingTaskSpec& task) {
    const QString modelName = QFileInfo(task.configRel).baseName();
    if (modelName.isEmpty()) {
        return {};
    }
    const QString path = QDir(baseDir).filePath(QStringLiteral("model/train/%1_pretrained.pdparams").arg(modelName));
    return QFileInfo(path).isFile() ? QFileInfo(path).absoluteFilePath() : QString();
}

}  // namespace ppocr
