#pragma once

#include "ProjectTypes.h"

namespace ppocr {

class Validator {
public:
    static QList<ValidationIssue> validateProject(const ProjectContext& context);
    static QList<ValidationIssue> validateAnnotation(
        const ProjectContext& context,
        const QJsonObject& annotation,
        const QJsonObject& labelSets);
    static QString writeValidationLog(const ProjectContext& context, const QList<ValidationIssue>& issues);
};

}  // namespace ppocr
