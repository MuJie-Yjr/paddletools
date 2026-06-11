#include "application/AnnotationService.h"

#include "core/AnnotationOps.h"
#include "core/ClassificationPrelabeler.h"
#include "core/CropGenerator.h"
#include "core/LayoutPrelabeler.h"
#include "core/OcrPrelabeler.h"
#include "core/ProjectRepository.h"

#include <algorithm>
#include <cmath>

namespace ppocr {
namespace {

struct JsonPoint {
    double x = 0.0;
    double y = 0.0;
};

QList<JsonPoint> jsonPoints(const QJsonArray& points) {
    QList<JsonPoint> output;
    for (const auto& value : points) {
        const QJsonArray point = value.toArray();
        if (point.size() < 2) {
            continue;
        }
        const double x = point.at(0).toDouble();
        const double y = point.at(1).toDouble();
        if (!std::isfinite(x) || !std::isfinite(y)) {
            continue;
        }
        output.append({x, y});
    }
    return output;
}

double cross(const JsonPoint& a, const JsonPoint& b, const JsonPoint& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool rangesOverlap(double a1, double a2, double b1, double b2) {
    const double minA = std::min(a1, a2);
    const double maxA = std::max(a1, a2);
    const double minB = std::min(b1, b2);
    const double maxB = std::max(b1, b2);
    return std::max(minA, minB) <= std::min(maxA, maxB) + 1e-6;
}

bool segmentsIntersect(const JsonPoint& a, const JsonPoint& b, const JsonPoint& c, const JsonPoint& d) {
    const double c1 = cross(a, b, c);
    const double c2 = cross(a, b, d);
    const double c3 = cross(c, d, a);
    const double c4 = cross(c, d, b);
    if (std::abs(c1) < 1e-6 && std::abs(c2) < 1e-6 && std::abs(c3) < 1e-6 && std::abs(c4) < 1e-6) {
        return rangesOverlap(a.x, b.x, c.x, d.x) && rangesOverlap(a.y, b.y, c.y, d.y);
    }
    return c1 * c2 <= 0.0 && c3 * c4 <= 0.0;
}

}  // namespace

QJsonObject AnnotationService::findRegion(const QJsonObject& annotation, const QString& regionId) {
    if (regionId.isEmpty()) {
        return {};
    }
    for (const auto& value : annotation.value(QStringLiteral("regions")).toArray()) {
        const QJsonObject region = value.toObject();
        if (region.value(QStringLiteral("id")).toString() == regionId) {
            return region;
        }
    }
    return {};
}

QJsonObject AnnotationService::clearAnnotation(const QJsonObject& annotation) {
    return AnnotationOps::clearAnnotation(annotation);
}

QJsonObject AnnotationService::addOcrRegion(const QJsonObject& annotation, const QJsonArray& points) {
    return AnnotationOps::addOcrRegion(
        annotation,
        points,
        QString(),
        AnnotationSource::Manual,
        true);
}

QJsonObject AnnotationService::addLayoutRegion(const QJsonObject& annotation, const QJsonArray& points, const QString& label) {
    return AnnotationOps::addLayoutRegion(
        annotation,
        points,
        label.isEmpty() ? QStringLiteral("text") : label,
        AnnotationSource::Manual,
        true);
}

QJsonObject AnnotationService::moveRegion(const QJsonObject& annotation, const QString& regionId, const QJsonArray& points) {
    const QJsonObject region = findRegion(annotation, regionId);
    if (region.isEmpty() || points.size() < 2) {
        return annotation;
    }

    QJsonObject updates;
    if (region.value(QStringLiteral("type")).toString() == toString(RegionType::Layout)) {
        const QRectF rect = boundingRect(points);
        updates.insert(QStringLiteral("bbox"), QJsonArray{rect.x(), rect.y(), rect.width(), rect.height()});
        updates.insert(QStringLiteral("points"), points);
        updates.insert(QStringLiteral("shape"), points.size() == 4 ? QStringLiteral("quad") : QStringLiteral("polygon"));
    } else {
        updates.insert(QStringLiteral("points"), points);
        updates.insert(QStringLiteral("shape"), points.size() == 4 ? QStringLiteral("quad") : QStringLiteral("polygon"));
    }
    return AnnotationOps::updateRegion(annotation, regionId, updates);
}

QJsonObject AnnotationService::updateRegion(const QJsonObject& annotation, const QString& regionId, const QJsonObject& updates) {
    return AnnotationOps::updateRegion(annotation, regionId, updates);
}

QJsonObject AnnotationService::setImageLabels(
    const QJsonObject& annotation,
    const QString& docOrientation,
    const QString& textlineOrientation,
    const QString& tableClassification) {
    QJsonObject updated = AnnotationOps::setImageLabel(annotation, QStringLiteral("doc_orientation"), docOrientation);
    updated = AnnotationOps::setImageLabel(updated, QStringLiteral("textline_orientation"), textlineOrientation);
    updated = AnnotationOps::setImageLabel(updated, QStringLiteral("table_classification"), tableClassification);
    return updated;
}

QJsonObject AnnotationService::deleteRegion(const QJsonObject& annotation, const QString& regionId) {
    return AnnotationOps::deleteRegion(annotation, regionId);
}

QJsonObject AnnotationService::applyOcrResult(const QJsonObject& annotation, const QJsonObject& report) {
    return OcrPrelabeler::applyOcrResult(annotation, report);
}

int AnnotationService::autoOcrRegionCount(const QJsonObject& annotation) {
    return OcrPrelabeler::autoOcrRegionCount(annotation);
}

QString AnnotationService::normalizedClassificationLabel(const QString& task, const QJsonObject& report) {
    return ClassificationPrelabeler::normalizedLabel(task, report);
}

QJsonObject AnnotationService::applyClassificationResult(
    const QJsonObject& annotation,
    const QString& task,
    const QJsonObject& report) {
    return ClassificationPrelabeler::applyClassificationResult(annotation, task, report);
}

QJsonObject AnnotationService::applyLayoutResult(const QJsonObject& annotation, const QJsonObject& report) {
    return LayoutPrelabeler::applyLayoutResult(annotation, report);
}

int AnnotationService::autoLayoutRegionCount(const QJsonObject& annotation) {
    return LayoutPrelabeler::autoLayoutRegionCount(annotation);
}

QJsonObject AnnotationService::generateRecCrops(const ProjectContext& context, const QJsonObject& annotation) {
    return CropGenerator::generateRecCrops(context, annotation);
}

QList<PageInfo> AnnotationService::listPages(const ProjectContext& context) {
    return ProjectRepository::listPages(context);
}

QJsonObject AnnotationService::readAnnotation(const QString& annotationPath) {
    return ProjectRepository::readAnnotation(annotationPath);
}

void AnnotationService::writeAnnotation(const QString& annotationPath, const QJsonObject& annotation) {
    ProjectRepository::writeAnnotation(annotationPath, annotation);
}

bool AnnotationService::isValidRegionPoints(const QJsonArray& points, QString* reason) {
    const QList<JsonPoint> parsed = jsonPoints(points);
    if (parsed.size() < 4) {
        if (reason) {
            *reason = QStringLiteral("region needs at least 4 points");
        }
        return false;
    }

    double area = 0.0;
    for (int i = 0; i < parsed.size(); ++i) {
        const JsonPoint& a = parsed.at(i);
        const JsonPoint& b = parsed.at((i + 1) % parsed.size());
        if (std::hypot(b.x - a.x, b.y - a.y) < 1.0) {
            if (reason) {
                *reason = QStringLiteral("region has an edge shorter than 1 px");
            }
            return false;
        }
        area += a.x * b.y - b.x * a.y;
    }
    if (std::abs(area) < 8.0) {
        if (reason) {
            *reason = QStringLiteral("region area is too small");
        }
        return false;
    }

    for (int i = 0; i < parsed.size(); ++i) {
        const int i2 = (i + 1) % parsed.size();
        for (int j = i + 1; j < parsed.size(); ++j) {
            const int j2 = (j + 1) % parsed.size();
            if (i == j || i2 == j || j2 == i) {
                continue;
            }
            if (i == 0 && j2 == 0) {
                continue;
            }
            if (segmentsIntersect(parsed.at(i), parsed.at(i2), parsed.at(j), parsed.at(j2))) {
                if (reason) {
                    *reason = QStringLiteral("region polygon is self-intersecting");
                }
                return false;
            }
        }
    }
    return true;
}

QRectF AnnotationService::boundingRect(const QJsonArray& points) {
    double minX = 1e12;
    double minY = 1e12;
    double maxX = -1e12;
    double maxY = -1e12;
    for (const auto& value : points) {
        const auto point = value.toArray();
        if (point.size() < 2) {
            continue;
        }
        minX = std::min(minX, point.at(0).toDouble());
        minY = std::min(minY, point.at(1).toDouble());
        maxX = std::max(maxX, point.at(0).toDouble());
        maxY = std::max(maxY, point.at(1).toDouble());
    }
    return QRectF(minX, minY, std::max(1.0, maxX - minX), std::max(1.0, maxY - minY));
}

}  // namespace ppocr
