#pragma once

#include "core/ProjectTypes.h"
#include "core/TrainingOptions.h"
#include "domain/TrainingTask.h"

#include <QJsonObject>
#include <QString>

namespace ppocr {

struct DatasetExportPlan {
    QString taskKey;
    QString taskTitle;
    TrainingTaskKind taskKind = TrainingTaskKind::Unknown;
    QString exportTask;
    QString datasetName;
    QString expectedDir;
    bool checkedOnly = true;
    bool requireValidation = true;
};

class DatasetExportPlanner {
public:
    static DatasetExportPlan plan(
        const ProjectContext& context,
        const TrainingTaskSpec& task,
        const TrainingOptions& options);

    static QJsonObject toJson(const DatasetExportPlan& plan);
};

}  // namespace ppocr
