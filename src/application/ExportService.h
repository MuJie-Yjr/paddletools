#pragma once

#include "core/Exporter.h"
#include "domain/Project.h"

#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <functional>

namespace ppocr {

class ExportService {
public:
    using ProgressCallback = std::function<void(int current, int total, const QString& message)>;

    static QMap<QString, QString> exportSelected(
        const ProjectContext& context,
        const QSet<QString>& tasks,
        bool checkedOnly,
        bool requireValidation,
        const ProgressCallback& progress = {},
        const Exporter::ExportOptions& options = Exporter::ExportOptions{});

    static QString datasetOutputDir(const ProjectContext& context, const QString& outputName);

    static QList<ValidationIssue> validateProject(const ProjectContext& context);
    static QString writeValidationLog(const ProjectContext& context, const QList<ValidationIssue>& issues);
};

}  // namespace ppocr
