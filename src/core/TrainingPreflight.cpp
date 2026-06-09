#include "core/TrainingPreflight.h"

#include "core/Exporter.h"
#include "core/RuntimePaths.h"
#include "core/TrainingRunStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>

namespace ppocr {
namespace {

QString effectivePython(const TrainingOptions& options) {
    return options.pythonExe.trimmed().isEmpty() ? QStringLiteral("python") : options.pythonExe.trimmed();
}

QString effectiveDevice(const TrainingOptions& options) {
    return options.device.trimmed().isEmpty() ? QStringLiteral("cpu") : options.device.trimmed();
}

bool hasPaddleClasTrainingScripts(const QString& path) {
    return !path.trimmed().isEmpty()
        && QFileInfo(QDir(path).filePath(QStringLiteral("tools/train.py"))).isFile();
}

QString localPretrainedWeightPath(const QString& baseDir, const TrainingTaskSpec& task) {
    const QString modelName = QFileInfo(task.configRel).baseName();
    if (modelName.isEmpty()) {
        return {};
    }
    const QString path = QDir(baseDir).filePath(QStringLiteral("model/train/%1_pretrained.pdparams").arg(modelName));
    return QFileInfo(path).isFile() ? QFileInfo(path).absoluteFilePath() : QString();
}

int nonEmptyTextLineCount(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0;
    }
    int count = 0;
    while (!file.atEnd()) {
        if (!file.readLine().trimmed().isEmpty()) {
            ++count;
        }
    }
    return count;
}

int cocoAnnotationCount(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return 0;
    }
    return doc.object().value(QStringLiteral("annotations")).toArray().size();
}

}  // namespace

PaddleCommand TrainingPreflight::buildCommand(
    const QString& baseDir,
    const ProjectContext* context,
    const QString& taskKey,
    const QString& outputDir,
    const TrainingOptions& options) {
    const auto task = trainingTaskByKey(taskKey);
    const QString datasetDir = context
        ? QDir(context->exportRoot()).filePath(task.datasetName)
        : QDir(baseDir).filePath(QStringLiteral("exports/%1").arg(task.datasetName));
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
    if (task.kind == QStringLiteral("layout") && context) {
        const int labelCount = context->config.value(QStringLiteral("label_sets"))
                                   .toObject()
                                   .value(QStringLiteral("layout"))
                                   .toArray()
                                   .size();
        if (labelCount > 0) {
            numClasses = labelCount;
        }
    }
    if ((task.kind == QStringLiteral("cls") || task.kind == QStringLiteral("layout")) && numClasses > 0) {
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

TrainingPreflightResult TrainingPreflight::run(
    const QString& baseDir,
    const ProjectContext& context,
    const QString& taskKey,
    const TrainingOptions& options,
    const QString& previewVersionId) {
    const auto task = trainingTaskByKey(taskKey);
    TrainingPreflightResult result;
    result.previewVersionId = previewVersionId.isEmpty() ? TrainingRunStore::newVersionId(task.key) : previewVersionId;
    result.outputDir = TrainingRunStore::versionDir(context, task.key, result.previewVersionId);
    result.command = buildCommand(baseDir, &context, task.key, result.outputDir, options);

    QJsonObject report{
        {QStringLiteral("ok"), false},
        {QStringLiteral("task_key"), task.key},
        {QStringLiteral("task_title"), task.title},
        {QStringLiteral("task_kind"), task.kind},
        {QStringLiteral("export_task"), task.exportTask},
        {QStringLiteral("dataset_name"), task.datasetName},
        {QStringLiteral("train_supported"), task.trainSupported},
        {QStringLiteral("note"), task.note},
        {QStringLiteral("base_dir"), QDir::toNativeSeparators(baseDir)},
        {QStringLiteral("project_dir"), QDir::toNativeSeparators(context.root.absolutePath())},
        {QStringLiteral("preview_version_id"), result.previewVersionId},
        {QStringLiteral("preview_output_dir"), QDir::toNativeSeparators(result.outputDir)},
        {QStringLiteral("python"), pathStatus(result.command.program)},
        {QStringLiteral("working_directory"), pathStatus(result.command.workingDirectory)},
        {QStringLiteral("paddlex_main"), pathStatus(QDir(result.command.workingDirectory).filePath(QStringLiteral("main.py")))},
        {QStringLiteral("config"), pathStatus(QDir(baseDir).filePath(task.configRel))},
        {QStringLiteral("local_pretrained_weight"), pathStatus(localPretrainedWeightPath(baseDir, task))},
        {QStringLiteral("command"), result.command.displayText()},
        {QStringLiteral("arguments"), QJsonArray::fromStringList(result.command.arguments)},
        {QStringLiteral("environment"), QJsonObject{
            {QStringLiteral("PYTHONIOENCODING"), result.command.environment.value(QStringLiteral("PYTHONIOENCODING"))},
            {QStringLiteral("PYTHONUTF8"), result.command.environment.value(QStringLiteral("PYTHONUTF8"))},
            {QStringLiteral("PADDLE_PDX_PADDLEOCR_PATH"), QDir::toNativeSeparators(result.command.environment.value(QStringLiteral("PADDLE_PDX_PADDLEOCR_PATH")))},
            {QStringLiteral("PADDLE_PDX_PADDLECLAS_PATH"), QDir::toNativeSeparators(result.command.environment.value(QStringLiteral("PADDLE_PDX_PADDLECLAS_PATH")))},
            {QStringLiteral("PYTHONPATH"), result.command.environment.value(QStringLiteral("PYTHONPATH"))},
        }},
    };

    if (!QFileInfo(result.command.program).exists() && result.command.program != QStringLiteral("python")) {
        result.errors.append(QStringLiteral("Python executable does not exist: %1").arg(result.command.program));
    }
    if (!QFileInfo(result.command.workingDirectory).isDir()) {
        result.errors.append(QStringLiteral("PaddleX working directory does not exist: %1").arg(result.command.workingDirectory));
    }
    if (!QFileInfo(QDir(result.command.workingDirectory).filePath(QStringLiteral("main.py"))).isFile()) {
        result.errors.append(QStringLiteral("PaddleX main.py does not exist under: %1").arg(result.command.workingDirectory));
    }
    if (!QFileInfo(QDir(baseDir).filePath(task.configRel)).isFile()) {
        result.errors.append(QStringLiteral("Training config does not exist: %1").arg(QDir(baseDir).filePath(task.configRel)));
    }
    if (!task.trainSupported) {
        const QString note = task.note.isEmpty()
            ? QStringLiteral("Training is not supported for task: %1").arg(task.key)
            : task.note;
        result.errors.append(note);
    }
    if (task.kind == QStringLiteral("cls")) {
        const QString paddleClasRoot = result.command.environment.value(QStringLiteral("PADDLE_PDX_PADDLECLAS_PATH"));
        if (!hasPaddleClasTrainingScripts(paddleClasRoot)) {
            result.errors.append(QStringLiteral(
                "PaddleClas training code is required for Cls training. model/train contains pretrained .pdparams weights and is already used when matching files exist, but PADDLE_PDX_PADDLECLAS_PATH must point to a PaddleClas checkout that contains tools/train.py. You can also place PaddleClas under the workbench root: %1")
                .arg(QDir(baseDir).filePath(QStringLiteral("PaddleClas"))));
        }
    }

    if (task.trainSupported) {
        try {
            const auto outputs = Exporter::exportSelected(context, {task.exportTask}, options.checkedOnly, options.requireValidation);
            result.datasetDir = outputs.value(task.exportTask);
            const QJsonObject sampleCounts = exportedSampleCounts(result.datasetDir);
            result.trainSampleCount = sampleCounts.value(QStringLiteral("train")).toInt();
            result.valSampleCount = sampleCounts.value(QStringLiteral("val")).toInt();
            result.sampleCount = sampleCounts.value(QStringLiteral("total")).toInt();
            report.insert(QStringLiteral("dataset_dir"), QDir::toNativeSeparators(result.datasetDir));
            report.insert(QStringLiteral("dataset"), pathStatus(result.datasetDir));
            report.insert(QStringLiteral("sample_count"), result.sampleCount);
            report.insert(QStringLiteral("sample_counts"), sampleCounts);
            if (result.sampleCount <= 0) {
                result.errors.append(QStringLiteral("Exported dataset has no samples: %1").arg(result.datasetDir));
            } else {
                if (result.trainSampleCount <= 0) {
                    result.errors.append(QStringLiteral("Exported dataset has no train samples: %1").arg(result.datasetDir));
                }
                if (result.valSampleCount <= 0) {
                    result.errors.append(QStringLiteral(
                        "Exported dataset has no val samples: %1. Mark at least one checked sample/page as val before training so PaddleX can evaluate and select a best model.")
                        .arg(result.datasetDir));
                }
            }
        } catch (const std::exception& exc) {
            result.errors.append(QStringLiteral("Dataset export failed: %1").arg(QString::fromUtf8(exc.what())));
        }
    } else {
        report.insert(QStringLiteral("dataset_dir"), QString());
        report.insert(QStringLiteral("sample_count"), 0);
    }

    report.insert(QStringLiteral("errors"), QJsonArray::fromStringList(result.errors));
    result.ok = result.errors.isEmpty();
    report.insert(QStringLiteral("ok"), result.ok);
    result.report = report;
    return result;
}

int TrainingPreflight::exportedSampleCount(const QString& datasetDir) {
    return exportedSampleCounts(datasetDir).value(QStringLiteral("total")).toInt();
}

QJsonObject TrainingPreflight::exportedSampleCounts(const QString& datasetDir) {
    const QDir dir(datasetDir);
    int train = nonEmptyTextLineCount(dir.filePath(QStringLiteral("train.txt")));
    int val = nonEmptyTextLineCount(dir.filePath(QStringLiteral("val.txt")));
    if (train + val <= 0) {
        train = cocoAnnotationCount(dir.filePath(QStringLiteral("annotations/instance_train.json")));
        val = cocoAnnotationCount(dir.filePath(QStringLiteral("annotations/instance_val.json")));
    }
    return QJsonObject{
        {QStringLiteral("train"), train},
        {QStringLiteral("val"), val},
        {QStringLiteral("total"), train + val},
    };
}

QJsonObject TrainingPreflight::pathStatus(const QString& path) {
    return RuntimePaths::pathStatus(path);
}

}  // namespace ppocr
