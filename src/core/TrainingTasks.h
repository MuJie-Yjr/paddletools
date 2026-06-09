#pragma once

#include <QList>
#include <QString>

namespace ppocr {

struct TrainingTaskSpec {
    QString key;
    QString title;
    QString kind;
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

QList<TrainingTaskSpec> trainingTasks();
TrainingTaskSpec trainingTaskByKey(const QString& key);

}  // namespace ppocr
