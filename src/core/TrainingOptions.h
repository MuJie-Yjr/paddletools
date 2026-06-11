#pragma once

#include <QString>

namespace ppocr {

struct TrainingOptions {
    QString pythonExe = "python";
    QString device = "cpu";
    int epochs = 0;
    int batchSize = 0;
    double learningRate = 0.0;
    int numClasses = 0;
    int warmupSteps = 0;
    QString resumePath;
    bool checkedOnly = true;
    bool requireValidation = true;
};

}  // namespace ppocr
