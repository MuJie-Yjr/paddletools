#pragma once

#include "core/TrainingRunner.h"

namespace ppocr::tests {

class FakeTrainingRunner {
public:
    static TrainingRunResult simulateSuccess(
        const QString& baseDir,
        const ProjectContext& context,
        const QString& taskKey,
        const TrainingOptions& options,
        const QString& logText = QString(),
        const QString& versionId = QString());
};

}  // namespace ppocr::tests
