#pragma once

#include "domain/ProjectTypes.h"

#include <QJsonObject>
#include <QString>

namespace ppocr {

struct Region {
    QString id;
    RegionType type = RegionType::OcrText;
    AnnotationSource source = AnnotationSource::Manual;
    bool checked = false;
    QJsonObject payload;
};

}  // namespace ppocr
