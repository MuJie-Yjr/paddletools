#include "core/TrainingPreflight.h"

#include "core/DatasetExporter.h"
#include "core/DatasetExportPlanner.h"
#include "core/RuntimePaths.h"
#include "core/TrainingCommandBuilder.h"
#include "core/TrainingRunStore.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QStringList>

namespace ppocr {
namespace {

bool hasPaddleClasTrainingScripts(const QString& path) {
    return !path.trimmed().isEmpty()
        && QFileInfo(QDir(path).filePath(QStringLiteral("tools/train.py"))).isFile();
}

}  // namespace

PaddleCommand TrainingPreflight::buildCommand(
    const QString& baseDir,
    const ProjectContext* context,
    const QString& taskKey,
    const QString& outputDir,
    const TrainingOptions& options) {
    const auto task = trainingTaskByKey(taskKey);
    return TrainingCommandBuilder::build(baseDir, context, task, outputDir, options);
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
        {QStringLiteral("task_kind"), toString(task.kind)},
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
        {QStringLiteral("local_pretrained_weight"), pathStatus(TrainingCommandBuilder::localPretrainedWeightPath(baseDir, task))},
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
    if (isClassificationTrainingTaskKind(task.kind)) {
        const QString paddleClasRoot = result.command.environment.value(QStringLiteral("PADDLE_PDX_PADDLECLAS_PATH"));
        if (!hasPaddleClasTrainingScripts(paddleClasRoot)) {
            result.errors.append(QStringLiteral(
                "PaddleClas training code is required for Cls training. model/train contains pretrained .pdparams weights and is already used when matching files exist, but PADDLE_PDX_PADDLECLAS_PATH must point to a PaddleClas checkout that contains tools/train.py. You can also place PaddleClas under the workbench root: %1")
                .arg(QDir(baseDir).filePath(QStringLiteral("PaddleClas"))));
        }
    }

    if (task.trainSupported) {
        const DatasetExportPlan exportPlan = DatasetExportPlanner::plan(context, task, options);
        const DatasetExportResult exportResult = DatasetExporter::exportDataset(context, exportPlan);
        report.insert(QStringLiteral("export_plan"), DatasetExportPlanner::toJson(exportPlan));
        report.insert(QStringLiteral("export"), exportResult.report);
        result.datasetDir = exportResult.datasetDir;
        result.trainSampleCount = exportResult.trainSampleCount;
        result.valSampleCount = exportResult.valSampleCount;
        result.sampleCount = exportResult.sampleCount;
        report.insert(QStringLiteral("dataset_dir"), QDir::toNativeSeparators(result.datasetDir));
        report.insert(QStringLiteral("dataset"), pathStatus(result.datasetDir));
        report.insert(QStringLiteral("sample_count"), result.sampleCount);
        report.insert(QStringLiteral("sample_counts"), exportResult.sampleCounts);
        report.insert(QStringLiteral("checked_only"), options.checkedOnly);
        if (!exportResult.ok) {
            result.errors.append(QStringLiteral("Dataset export failed: %1").arg(exportResult.error));
        } else if (result.sampleCount <= 0) {
            if (options.checkedOnly) {
                const QStringList suggestions{
                    QStringLiteral("点击“批量确认当前页”，确认当前页已检查过的预标注区域。"),
                    QStringLiteral("点击“批量确认全项目”，在检查后把全项目区域标记为 checked=true。"),
                    QStringLiteral("如果你明确要使用未确认标注训练，可以关闭 checked-only 导出。"),
                };
                report.insert(QStringLiteral("zero_sample_reason"), QStringLiteral("checked_only_without_confirmed_annotations"));
                report.insert(QStringLiteral("suggestions"), QJsonArray::fromStringList(suggestions));
                result.errors.append(QStringLiteral(
                    "训练样本为 0。原因：当前 checked-only 导出只导出 checked=true 的标注，但从 %1 没有导出任何已确认样本；自动预标注结果默认 checked=false。处理方式：批量确认当前页、批量确认全项目，或在确认风险后关闭 checked-only 导出。")
                    .arg(result.datasetDir));
            } else {
                result.errors.append(QStringLiteral("Exported dataset has no samples: %1").arg(result.datasetDir));
            }
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
    return DatasetExporter::exportedSampleCount(datasetDir);
}

QJsonObject TrainingPreflight::exportedSampleCounts(const QString& datasetDir) {
    return DatasetExporter::exportedSampleCounts(datasetDir);
}

QJsonObject TrainingPreflight::pathStatus(const QString& path) {
    return RuntimePaths::pathStatus(path);
}

}  // namespace ppocr
