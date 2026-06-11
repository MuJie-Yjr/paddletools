#pragma once

#include "core/ProjectTypes.h"
#include "core/TrainingOptions.h"
#include "domain/TrainingTask.h"
#include "paddle/PaddleProcess.h"

namespace ppocr {

class TrainingCommandBuilder {
public:
    static PaddleCommand build(
        const QString& baseDir,
        const ProjectContext* context,
        const TrainingTaskSpec& task,
        const QString& outputDir,
        const TrainingOptions& options);

    static QString localPretrainedWeightPath(const QString& baseDir, const TrainingTaskSpec& task);
};

}  // namespace ppocr
