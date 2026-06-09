#pragma once

#include "ProjectTypes.h"

#include <QJsonObject>

namespace ppocr {

class CropGenerator {
public:
    static QJsonObject generateRecCrops(const ProjectContext& context, const QJsonObject& annotation);
};

}  // namespace ppocr
