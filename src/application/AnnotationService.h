#pragma once

#include "domain/Page.h"
#include "domain/Project.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QRectF>
#include <QString>

namespace ppocr {

class AnnotationService {
public:
    static QJsonObject findRegion(const QJsonObject& annotation, const QString& regionId);
    static QJsonObject clearAnnotation(const QJsonObject& annotation);
    static QJsonObject addOcrRegion(const QJsonObject& annotation, const QJsonArray& points);
    static QJsonObject addLayoutRegion(const QJsonObject& annotation, const QJsonArray& points, const QString& label);
    static QJsonObject moveRegion(const QJsonObject& annotation, const QString& regionId, const QJsonArray& points);
    static QJsonObject updateRegion(const QJsonObject& annotation, const QString& regionId, const QJsonObject& updates);
    static QJsonObject setImageLabels(
        const QJsonObject& annotation,
        const QString& docOrientation,
        const QString& textlineOrientation,
        const QString& tableClassification);
    static QJsonObject deleteRegion(const QJsonObject& annotation, const QString& regionId);
    static QJsonObject applyOcrResult(const QJsonObject& annotation, const QJsonObject& report);
    static int autoOcrRegionCount(const QJsonObject& annotation);
    static QString normalizedClassificationLabel(const QString& task, const QJsonObject& report);
    static QJsonObject applyClassificationResult(const QJsonObject& annotation, const QString& task, const QJsonObject& report);
    static QJsonObject applyLayoutResult(const QJsonObject& annotation, const QJsonObject& report);
    static int autoLayoutRegionCount(const QJsonObject& annotation);
    static QJsonObject generateRecCrops(const ProjectContext& context, const QJsonObject& annotation);

    static QList<PageInfo> listPages(const ProjectContext& context);
    static QJsonObject readAnnotation(const QString& annotationPath);
    static void writeAnnotation(const QString& annotationPath, const QJsonObject& annotation);
    static bool isValidRegionPoints(const QJsonArray& points, QString* reason = nullptr);

private:
    static QRectF boundingRect(const QJsonArray& points);
};

}  // namespace ppocr
