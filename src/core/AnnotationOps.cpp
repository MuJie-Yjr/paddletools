#include "core/AnnotationOps.h"

#include <QJsonArray>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace ppocr {

static QJsonArray roundedPoints(const QJsonArray& points) {
    QJsonArray output;
    for (const auto& item : points) {
        const auto point = item.toArray();
        if (point.size() < 2) {
            continue;
        }
        output.append(QJsonArray{
            std::round(point.at(0).toDouble() * 100.0) / 100.0,
            std::round(point.at(1).toDouble() * 100.0) / 100.0,
        });
    }
    return output;
}

static int nextReadingOrder(const QJsonObject& annotation) {
    int maxOrder = 0;
    for (const auto& value : annotation.value("regions").toArray()) {
        const auto region = value.toObject();
        if (region.value("type").toString() == "ocr_text") {
            maxOrder = std::max(maxOrder, region.value("reading_order").toInt());
        }
    }
    return maxOrder + 1;
}

QString AnnotationOps::nextRegionId(const QJsonObject& annotation, const QString& prefix) {
    int maxIndex = 0;
    const QString marker = prefix + "_";
    for (const auto& value : annotation.value("regions").toArray()) {
        const QString id = value.toObject().value("id").toString();
        if (!id.startsWith(marker)) {
            continue;
        }
        bool ok = false;
        const int index = id.mid(marker.size()).toInt(&ok);
        if (ok) {
            maxIndex = std::max(maxIndex, index);
        }
    }
    return QString("%1_%2").arg(prefix).arg(maxIndex + 1, 6, 10, QLatin1Char('0'));
}

QJsonObject AnnotationOps::addOcrRegion(
    const QJsonObject& annotation,
    const QJsonArray& points,
    const QString& text,
    const QString& source,
    bool checked) {
    QJsonObject output = annotation;
    QJsonArray regions = output.value("regions").toArray();
    const QJsonArray normalized = roundedPoints(points);
    regions.append(QJsonObject{
        {"id", nextRegionId(output, "text")},
        {"type", "ocr_text"},
        {"shape", normalized.size() == 4 ? "quad" : "polygon"},
        {"points", normalized},
        {"text", text},
        {"ignore", false},
        {"reading_order", nextReadingOrder(output)},
        {"source", source},
        {"checked", checked},
        {"confidence", QJsonValue::Null},
    });
    output["regions"] = regions;
    output["status"] = "labeled";
    return output;
}

QJsonObject AnnotationOps::addLayoutRegion(
    const QJsonObject& annotation,
    const QRectF& bbox,
    const QString& label,
    const QString& source,
    bool checked) {
    QJsonObject output = annotation;
    QJsonArray regions = output.value("regions").toArray();
    regions.append(QJsonObject{
        {"id", nextRegionId(output, "layout")},
        {"type", "layout"},
        {"label", label},
        {"bbox", QJsonArray{bbox.x(), bbox.y(), bbox.width(), bbox.height()}},
        {"source", source},
        {"checked", checked},
    });
    output["regions"] = regions;
    output["status"] = "labeled";
    return output;
}

QJsonObject AnnotationOps::setImageLabel(const QJsonObject& annotation, const QString& task, const QString& label) {
    QJsonObject output = annotation;
    QJsonArray labels = output.value("image_labels").toArray();
    bool found = false;
    for (int i = 0; i < labels.size(); ++i) {
        QJsonObject item = labels.at(i).toObject();
        if (item.value("task").toString() == task) {
            item["label"] = label;
            labels.replace(i, item);
            found = true;
            break;
        }
    }
    if (!found) {
        labels.append(QJsonObject{{"task", task}, {"label", label}});
    }
    output["image_labels"] = labels;
    output["status"] = "labeled";
    return output;
}

QJsonObject AnnotationOps::updateRegion(const QJsonObject& annotation, const QString& regionId, const QJsonObject& updates) {
    QJsonObject output = annotation;
    QJsonArray regions = output.value("regions").toArray();
    for (int i = 0; i < regions.size(); ++i) {
        QJsonObject region = regions.at(i).toObject();
        if (region.value("id").toString() == regionId) {
            for (auto it = updates.begin(); it != updates.end(); ++it) {
                region[it.key()] = it.value();
            }
            regions.replace(i, region);
            break;
        }
    }
    output["regions"] = regions;
    output["status"] = "labeled";
    return output;
}

QJsonObject AnnotationOps::deleteRegion(const QJsonObject& annotation, const QString& regionId) {
    QJsonObject output = annotation;
    QJsonArray regions;
    for (const auto& value : output.value("regions").toArray()) {
        const auto region = value.toObject();
        if (region.value("id").toString() != regionId) {
            regions.append(region);
        }
    }
    QJsonArray crops;
    for (const auto& value : output.value("rec_crops").toArray()) {
        const auto crop = value.toObject();
        if (crop.value("region_id").toString() != regionId) {
            crops.append(crop);
        }
    }
    output["regions"] = regions;
    output["rec_crops"] = crops;
    return output;
}

QJsonObject AnnotationOps::clearAnnotation(const QJsonObject& annotation) {
    QJsonObject output = annotation;
    output["regions"] = QJsonArray{};
    output["rec_crops"] = QJsonArray{};
    QJsonArray labels = output.value("image_labels").toArray();
    const QSet<QString> known = {"doc_orientation", "textline_orientation", "table_classification"};
    QSet<QString> seen;
    for (int i = 0; i < labels.size(); ++i) {
        QJsonObject item = labels.at(i).toObject();
        const QString task = item.value("task").toString();
        seen.insert(task);
        if (known.contains(task)) {
            item["label"] = "";
            labels.replace(i, item);
        }
    }
    for (const auto& task : known) {
        if (!seen.contains(task)) {
            labels.append(QJsonObject{{"task", task}, {"label", ""}});
        }
    }
    output["image_labels"] = labels;
    output["status"] = "unlabeled";
    output["validation"] = QJsonObject{{"passed", false}, {"errors", QJsonArray{}}, {"warnings", QJsonArray{}}};
    return output;
}

QJsonArray rectToPoints(const QRectF& rect) {
    return QJsonArray{
        QJsonArray{rect.left(), rect.top()},
        QJsonArray{rect.right(), rect.top()},
        QJsonArray{rect.right(), rect.bottom()},
        QJsonArray{rect.left(), rect.bottom()},
    };
}

}  // namespace ppocr
