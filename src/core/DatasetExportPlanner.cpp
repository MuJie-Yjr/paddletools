#include "core/DatasetExportPlanner.h"

#include "core/Exporter.h"

#include <QDir>

namespace ppocr {

DatasetExportPlan DatasetExportPlanner::plan(
    const ProjectContext& context,
    const TrainingTaskSpec& task,
    const TrainingOptions& options) {
    DatasetExportPlan plan;
    plan.taskKey = task.key;
    plan.taskTitle = task.title;
    plan.taskKind = task.kind;
    plan.exportTask = task.exportTask;
    plan.datasetName = task.datasetName;
    plan.expectedDir = Exporter::datasetOutputDir(context, task.datasetName);
    plan.checkedOnly = options.checkedOnly;
    plan.requireValidation = options.requireValidation;
    return plan;
}

QJsonObject DatasetExportPlanner::toJson(const DatasetExportPlan& plan) {
    return QJsonObject{
        {QStringLiteral("task_key"), plan.taskKey},
        {QStringLiteral("task_title"), plan.taskTitle},
        {QStringLiteral("task_kind"), toString(plan.taskKind)},
        {QStringLiteral("export_task"), plan.exportTask},
        {QStringLiteral("dataset_name"), plan.datasetName},
        {QStringLiteral("expected_dir"), QDir::toNativeSeparators(plan.expectedDir)},
        {QStringLiteral("checked_only"), plan.checkedOnly},
        {QStringLiteral("require_validation"), plan.requireValidation},
    };
}

}  // namespace ppocr
