#pragma once

#include "domain/PredictionJob.h"

#include <QString>

namespace ppocr {

class PredictionService {
public:
    static bool validateRequest(const PredictionRunRequest& request, QString* errorMessage = nullptr);
};

}  // namespace ppocr
