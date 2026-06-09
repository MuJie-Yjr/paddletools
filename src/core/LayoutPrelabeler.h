#pragma once

#include <QJsonObject>

namespace ppocr {

class LayoutPrelabeler {
public:
    static QJsonObject applyLayoutResult(const QJsonObject& annotation, const QJsonObject& layoutResult);
    static int autoLayoutRegionCount(const QJsonObject& annotation);
};

}  // namespace ppocr
