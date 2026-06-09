#include "core/LayoutPrelabeler.h"

#include "core/AnnotationOps.h"

#include <QJsonArray>
#include <QRectF>

namespace ppocr {
namespace {

QJsonObject resultPayload(const QJsonObject& layoutResult) {
    const QJsonObject res = layoutResult.value(QStringLiteral("res")).toObject();
    return res.isEmpty() ? layoutResult : res;
}

QJsonArray resultBoxes(const QJsonObject& layoutResult) {
    return resultPayload(layoutResult).value(QStringLiteral("boxes")).toArray();
}

QJsonObject removeAutoLayoutRegions(QJsonObject annotation) {
    QJsonArray kept;
    for (const auto& value : annotation.value(QStringLiteral("regions")).toArray()) {
        const QJsonObject region = value.toObject();
        const bool isAutoLayout =
            region.value(QStringLiteral("type")).toString() == QStringLiteral("layout")
            && region.value(QStringLiteral("source")).toString() == QStringLiteral("auto");
        if (!isAutoLayout) {
            kept.append(region);
        }
    }
    annotation.insert(QStringLiteral("regions"), kept);
    return annotation;
}

QRectF boxRect(const QJsonObject& box, bool* ok) {
    *ok = false;
    const QJsonArray coordinate = box.value(QStringLiteral("coordinate")).toArray();
    if (coordinate.size() == 4) {
        const double left = coordinate.at(0).toDouble();
        const double top = coordinate.at(1).toDouble();
        const double right = coordinate.at(2).toDouble();
        const double bottom = coordinate.at(3).toDouble();
        if (right > left && bottom > top) {
            *ok = true;
            return QRectF(left, top, right - left, bottom - top);
        }
    }

    const QJsonArray bbox = box.value(QStringLiteral("bbox")).toArray();
    if (bbox.size() == 4) {
        const double x = bbox.at(0).toDouble();
        const double y = bbox.at(1).toDouble();
        const double width = bbox.at(2).toDouble();
        const double height = bbox.at(3).toDouble();
        if (width > 0 && height > 0) {
            *ok = true;
            return QRectF(x, y, width, height);
        }
    }

    return {};
}

QJsonObject withLastLayoutMetadata(QJsonObject annotation, const QJsonObject& box) {
    QJsonArray regions = annotation.value(QStringLiteral("regions")).toArray();
    if (regions.isEmpty()) {
        return annotation;
    }
    QJsonObject region = regions.last().toObject();
    if (box.contains(QStringLiteral("score"))) {
        region.insert(QStringLiteral("confidence"), box.value(QStringLiteral("score")));
    }
    if (box.contains(QStringLiteral("cls_id"))) {
        region.insert(QStringLiteral("class_id"), box.value(QStringLiteral("cls_id")));
    }
    if (box.contains(QStringLiteral("order"))) {
        region.insert(QStringLiteral("reading_order"), box.value(QStringLiteral("order")));
    }
    regions.replace(regions.size() - 1, region);
    annotation.insert(QStringLiteral("regions"), regions);
    return annotation;
}

}  // namespace

QJsonObject LayoutPrelabeler::applyLayoutResult(const QJsonObject& annotation, const QJsonObject& layoutResult) {
    QJsonObject output = removeAutoLayoutRegions(annotation);
    const QJsonArray boxes = resultBoxes(layoutResult);
    for (const auto& value : boxes) {
        const QJsonObject box = value.toObject();
        bool ok = false;
        const QRectF rect = boxRect(box, &ok);
        if (!ok) {
            continue;
        }
        const QString label = box.value(QStringLiteral("label")).toString(QStringLiteral("text"));
        output = AnnotationOps::addLayoutRegion(output, rect, label.isEmpty() ? QStringLiteral("text") : label, QStringLiteral("auto"), false);
        output = withLastLayoutMetadata(output, box);
    }

    QJsonObject prelabel = output.value(QStringLiteral("prelabel")).toObject();
    prelabel.insert(QStringLiteral("layout"), QJsonObject{
        {QStringLiteral("engine"), layoutResult.value(QStringLiteral("engine")).toString(QStringLiteral("PaddleX/layout_analysis"))},
        {QStringLiteral("result_json"), layoutResult.value(QStringLiteral("result_json")).toString()},
        {QStringLiteral("region_count"), autoLayoutRegionCount(output)},
    });
    output.insert(QStringLiteral("prelabel"), prelabel);
    if (output.value(QStringLiteral("regions")).toArray().isEmpty()) {
        output.insert(QStringLiteral("status"), QStringLiteral("unlabeled"));
    }
    return output;
}

int LayoutPrelabeler::autoLayoutRegionCount(const QJsonObject& annotation) {
    int count = 0;
    for (const auto& value : annotation.value(QStringLiteral("regions")).toArray()) {
        const QJsonObject region = value.toObject();
        if (region.value(QStringLiteral("type")).toString() == QStringLiteral("layout")
            && region.value(QStringLiteral("source")).toString() == QStringLiteral("auto")) {
            ++count;
        }
    }
    return count;
}

}  // namespace ppocr
