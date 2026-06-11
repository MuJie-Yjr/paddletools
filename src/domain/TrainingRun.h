#pragma once

#include "domain/ProjectTypes.h"

#include <QJsonObject>
#include <QString>

namespace ppocr {

struct TrainingRun {
    QString runId;
    QString taskKey;
    RunStatus status = RunStatus::Pending;
    QJsonObject metrics;
};

}  // namespace ppocr
