#pragma once

#include <QString>

namespace ppocr {

struct SplitSummary {
    int total = 0;
    int train = 0;
    int val = 0;
    int test = 0;
    int unassigned = 0;
    QString strategy;
    QString strategyText;
    double trainRatio = 0.8;
    double valRatio = 0.2;
    double testRatio = 0.0;
};

}  // namespace ppocr
