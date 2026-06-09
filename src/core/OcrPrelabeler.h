#pragma once

#include <QJsonObject>

namespace ppocr {

class OcrPrelabeler {
public:
    static QJsonObject applyOcrResult(const QJsonObject& annotation, const QJsonObject& ocrResult);
    static int autoOcrRegionCount(const QJsonObject& annotation);
};

}  // namespace ppocr
