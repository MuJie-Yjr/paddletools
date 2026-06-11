#include "application/PredictionService.h"

#include <QFileInfo>

namespace ppocr {

bool PredictionService::validateRequest(const PredictionRunRequest& request, QString* errorMessage) {
    if (request.program.trimmed().isEmpty() || !QFileInfo::exists(request.program)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Prediction program does not exist: %1").arg(request.program);
        }
        return false;
    }
    return true;
}

}  // namespace ppocr
