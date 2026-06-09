#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QRectF>

namespace ppocr {

class AnnotationOps {
public:
    static QString nextRegionId(const QJsonObject& annotation, const QString& prefix);
    static QJsonObject addOcrRegion(
        const QJsonObject& annotation,
        const QJsonArray& points,
        const QString& text = QString(),
        const QString& source = "manual",
        bool checked = true);
    static QJsonObject addLayoutRegion(
        const QJsonObject& annotation,
        const QRectF& bbox,
        const QString& label,
        const QString& source = "manual",
        bool checked = true);
    static QJsonObject setImageLabel(const QJsonObject& annotation, const QString& task, const QString& label);
    static QJsonObject updateRegion(const QJsonObject& annotation, const QString& regionId, const QJsonObject& updates);
    static QJsonObject deleteRegion(const QJsonObject& annotation, const QString& regionId);
    static QJsonObject clearAnnotation(const QJsonObject& annotation);
};

QJsonArray rectToPoints(const QRectF& rect);

}  // namespace ppocr
