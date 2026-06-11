#pragma once

#include "domain/ProjectTypes.h"

#include <QString>

namespace ppocr {

struct TrainingTaskSpec {
    QString key;
    QString title;
    TrainingTaskKind kind = TrainingTaskKind::Unknown;
    QString exportTask;
    QString datasetName;
    QString configRel;
    QString bestWeightRel = "best_accuracy/best_accuracy.pdparams";
    QString inferDirRel = "best_accuracy/inference";
    int epochs = 20;
    int batchSize = 8;
    double learningRate = 0.001;
    int numClasses = 0;
    int warmupSteps = 0;
    bool trainSupported = true;
    QString note;
};

struct TrainingTask {
    QString key;
    QString title;
    TrainingTaskKind kind = TrainingTaskKind::Unknown;
};

}  // namespace ppocr
