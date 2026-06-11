#pragma once

#include "domain/ProjectTypes.h"

#include <QString>
#include <QStringList>

namespace ppocr {

struct PredictionRunRequest {
    QString program;
    QStringList arguments;
    QString workingDirectory;
};

struct PredictionJob {
    PredictionRunRequest request;
    PredictionStatus status = PredictionStatus::Pending;
};

}  // namespace ppocr
