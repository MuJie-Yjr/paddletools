#pragma once

#include "domain/ProjectTypes.h"

#include <QJsonObject>
#include <QString>

namespace ppocr {

struct ModelVersion {
    QString taskKey;
    QString versionId;
    RunStatus status = RunStatus::Pending;
    QString inferenceModelDir;
    QJsonObject metrics;
};

}  // namespace ppocr
