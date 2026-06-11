#pragma once

#include "domain/ProjectTypes.h"

#include <QJsonObject>
#include <QString>

namespace ppocr {

struct Annotation {
    QString assetId;
    QJsonObject json;
};

}  // namespace ppocr
