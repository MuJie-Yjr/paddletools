#include "core/Validator.h"

#include "core/JsonUtils.h"
#include "core/ProjectRepository.h"

#include <QFileInfo>
#include <QDir>
#include <QImageReader>
#include <QJsonArray>
#include <QMap>
#include <QStringConverter>
#include <QTextStream>
#include <cmath>

namespace ppocr {

static double polygonArea(const QJsonArray& points) {
    if (points.size() < 3) {
        return 0.0;
    }
    double area = 0.0;
    for (int i = 0; i < points.size(); ++i) {
        const auto p1 = points.at(i).toArray();
        const auto p2 = points.at((i + 1) % points.size()).toArray();
        area += p1.at(0).toDouble() * p2.at(1).toDouble() - p2.at(0).toDouble() * p1.at(1).toDouble();
    }
    return std::abs(area) / 2.0;
}

static bool labelSetContains(const QJsonObject& labelSets, const QString& task, const QString& label) {
    for (const auto& value : labelSets.value(task).toArray()) {
        if (value.toString() == label) {
            return true;
        }
    }
    return false;
}

static void addIssue(QList<ValidationIssue>* issues, const QString& severity, const QString& task, const QString& assetId, const QString& message, const QString& regionId = QString()) {
    issues->append({severity, task, assetId, message, QString(), regionId});
}

QList<ValidationIssue> Validator::validateProject(const ProjectContext& context) {
    QList<ValidationIssue> issues;
    const auto pages = ProjectRepository::listPages(context);
    const QJsonObject labelSets = context.config.value("label_sets").toObject();
    QMap<QString, int> splitCounts;
    for (const auto& page : pages) {
        QJsonObject annotation = ProjectRepository::readAnnotation(page.annotationPath);
        auto pageIssues = validateAnnotation(context, annotation, labelSets);
        issues.append(pageIssues);
        QJsonArray errors;
        QJsonArray warnings;
        for (const auto& issue : pageIssues) {
            if (issue.severity == "error") {
                errors.append(issue.message);
            } else if (issue.severity == "warning") {
                warnings.append(issue.message);
            }
        }
        annotation["validation"] = QJsonObject{
            {"passed", errors.isEmpty()},
            {"errors", errors},
            {"warnings", warnings},
        };
        ProjectRepository::writeAnnotation(page.annotationPath, annotation);
        splitCounts[page.split] += 1;
    }
    if (!pages.isEmpty() && (splitCounts.value("train") == 0 || splitCounts.value("val") == 0)) {
        addIssue(&issues, "warning", "split", "", "train / val has an empty side");
    }
    return issues;
}

QList<ValidationIssue> Validator::validateAnnotation(
    const ProjectContext& context,
    const QJsonObject& annotation,
    const QJsonObject& labelSets) {
    QList<ValidationIssue> issues;
    const QString assetId = annotation.value("asset_id").toString();
    const QString imagePath = context.path(annotation.value("image_path").toString());
    if (!QFileInfo::exists(imagePath)) {
        addIssue(&issues, "error", "image", assetId, "image path does not exist");
        return issues;
    }
    QImageReader reader(imagePath);
    const QSize size = reader.size();
    if (!size.isValid()) {
        addIssue(&issues, "error", "image", assetId, "image cannot be read");
        return issues;
    }
    if (size.width() != annotation.value("width").toInt() || size.height() != annotation.value("height").toInt()) {
        addIssue(&issues, "warning", "image", assetId, "image size differs from annotation");
    }

    QMap<int, int> readingOrders;
    for (const auto& value : annotation.value("regions").toArray()) {
        const auto region = value.toObject();
        const QString type = region.value("type").toString();
        const QString rid = region.value("id").toString();
        if (type == "ocr_text") {
            const QJsonArray points = region.value("points").toArray();
            if (points.size() < 4) {
                addIssue(&issues, "error", "det", assetId, "OCR points must contain at least 4 points", rid);
                continue;
            }
            for (const auto& pointValue : points) {
                const auto point = pointValue.toArray();
                const double x = point.at(0).toDouble();
                const double y = point.at(1).toDouble();
                if (x < 0 || y < 0 || x > size.width() || y > size.height()) {
                    addIssue(&issues, "error", "det", assetId, "OCR points out of bounds", rid);
                    break;
                }
            }
            if (!region.value("ignore").toBool() && region.value("text").toString().trimmed().isEmpty()) {
                addIssue(&issues, "error", "det", assetId, "non-ignore OCR region has empty text", rid);
            }
            if (polygonArea(points) < 4.0) {
                addIssue(&issues, "warning", "det", assetId, "OCR region area is too small", rid);
            }
            if (!region.value("checked").toBool(false)) {
                addIssue(&issues, "warning", "det", assetId, "OCR region is not checked", rid);
            }
            const int order = region.value("reading_order").toInt();
            if (order > 0) {
                readingOrders[order] += 1;
            }
        } else if (type == "layout") {
            const auto bbox = region.value("bbox").toArray();
            if (bbox.size() != 4) {
                addIssue(&issues, "error", "coco", assetId, "layout bbox is invalid", rid);
                continue;
            }
            const double x = bbox.at(0).toDouble();
            const double y = bbox.at(1).toDouble();
            const double w = bbox.at(2).toDouble();
            const double h = bbox.at(3).toDouble();
            if (w <= 0 || h <= 0) {
                addIssue(&issues, "error", "coco", assetId, "layout bbox width/height must be positive", rid);
            }
            if (x < 0 || y < 0 || x + w > size.width() || y + h > size.height()) {
                addIssue(&issues, "error", "coco", assetId, "layout bbox out of bounds", rid);
            }
            if (!labelSetContains(labelSets, "layout", region.value("label").toString())) {
                addIssue(&issues, "error", "coco", assetId, "layout label is invalid", rid);
            }
        }
    }
    for (auto it = readingOrders.begin(); it != readingOrders.end(); ++it) {
        if (it.value() > 1) {
            addIssue(&issues, "error", "det", assetId, QString("duplicate reading_order: %1").arg(it.key()));
        }
    }
    for (const auto& value : annotation.value("image_labels").toArray()) {
        const auto label = value.toObject();
        const QString task = label.value("task").toString();
        const QString text = label.value("label").toString();
        if (!text.isEmpty() && labelSets.contains(task) && !labelSetContains(labelSets, task, text)) {
            addIssue(&issues, "error", "cls", assetId, QString("%1 label is invalid: %2").arg(task, text));
        }
    }
    if (issues.isEmpty()) {
        addIssue(&issues, "passed", "all", assetId, "validation passed");
    }
    return issues;
}

QString Validator::writeValidationLog(const ProjectContext& context, const QList<ValidationIssue>& issues) {
    const QString path = QDir(context.logRoot()).filePath("validate.log");
    QFileInfo info(path);
    QDir().mkpath(info.absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return path;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    for (const auto& issue : issues) {
        stream << issue.severity << '\t' << issue.task << '\t' << issue.assetId << '\t'
               << issue.regionId << '\t' << issue.message << '\n';
    }
    return path;
}

}  // namespace ppocr
