#include "application/ExportService.h"

#include "core/Exporter.h"
#include "core/Validator.h"

namespace ppocr {

QMap<QString, QString> ExportService::exportSelected(
    const ProjectContext& context,
    const QSet<QString>& tasks,
    bool checkedOnly,
    bool requireValidation,
    const ProgressCallback& progress,
    const Exporter::ExportOptions& options) {
    return Exporter::exportSelected(context, tasks, checkedOnly, requireValidation, progress, options);
}

QString ExportService::datasetOutputDir(const ProjectContext& context, const QString& outputName) {
    return Exporter::datasetOutputDir(context, outputName);
}

QList<ValidationIssue> ExportService::validateProject(const ProjectContext& context) {
    return Validator::validateProject(context);
}

QString ExportService::writeValidationLog(const ProjectContext& context, const QList<ValidationIssue>& issues) {
    return Validator::writeValidationLog(context, issues);
}

}  // namespace ppocr
