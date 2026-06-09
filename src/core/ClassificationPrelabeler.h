#pragma once

#include <QJsonObject>
#include <QString>

namespace ppocr {

class ClassificationPrelabeler {
public:
    static QJsonObject applyClassificationResult(
        const QJsonObject& annotation,
        const QString& task,
        const QJsonObject& classificationResult);
    static QString normalizedLabel(const QString& task, const QJsonObject& classificationResult);
};

}  // namespace ppocr
