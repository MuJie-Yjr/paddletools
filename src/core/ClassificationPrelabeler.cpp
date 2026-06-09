#include "core/ClassificationPrelabeler.h"

#include "core/AnnotationOps.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QRegularExpression>

namespace ppocr {
namespace {

QString rawLabel(const QJsonObject& result) {
    const QJsonArray labelNames = result.value(QStringLiteral("label_names")).toArray();
    if (!labelNames.isEmpty()) {
        return labelNames.first().toString().trimmed();
    }
    const QJsonArray classIds = result.value(QStringLiteral("class_ids")).toArray();
    if (!classIds.isEmpty()) {
        return QString::number(classIds.first().toInt());
    }
    return {};
}

QString firstIntegerToken(const QString& value) {
    const QRegularExpression re(QStringLiteral("-?\\d+"));
    const QRegularExpressionMatch match = re.match(value);
    return match.hasMatch() ? match.captured(0) : QString();
}

QString normalizeDocOrientation(const QString& value) {
    bool ok = false;
    const int angle = firstIntegerToken(value).toInt(&ok);
    if (ok && (angle == 0 || angle == 90 || angle == 180 || angle == 270)) {
        return QString::number(angle);
    }
    return {};
}

QString normalizeTextlineOrientation(const QString& value) {
    bool ok = false;
    const int angle = firstIntegerToken(value).toInt(&ok);
    if (!ok) {
        return {};
    }
    if (angle == 0 || angle == 360) {
        return QStringLiteral("0");
    }
    if (angle == 1 || angle == 180) {
        return QStringLiteral("180");
    }
    return {};
}

QString labelKeyForTask(const QString& task) {
    if (task == QStringLiteral("doc_orientation")) {
        return QStringLiteral("doc_orientation");
    }
    if (task == QStringLiteral("textline_orientation")) {
        return QStringLiteral("textline_orientation");
    }
    if (task == QStringLiteral("table_classification")) {
        return QStringLiteral("table_classification");
    }
    return {};
}

}  // namespace

QString ClassificationPrelabeler::normalizedLabel(const QString& task, const QJsonObject& classificationResult) {
    const QString label = rawLabel(classificationResult);
    if (task == QStringLiteral("doc_orientation")) {
        return normalizeDocOrientation(label);
    }
    if (task == QStringLiteral("textline_orientation")) {
        return normalizeTextlineOrientation(label);
    }
    if (task == QStringLiteral("table_classification")) {
        return label;
    }
    return {};
}

QJsonObject ClassificationPrelabeler::applyClassificationResult(
    const QJsonObject& annotation,
    const QString& task,
    const QJsonObject& classificationResult) {
    const QString labelKey = labelKeyForTask(task);
    const QString label = normalizedLabel(task, classificationResult);
    if (labelKey.isEmpty() || label.isEmpty()) {
        return annotation;
    }

    QJsonObject output = AnnotationOps::setImageLabel(annotation, labelKey, label);
    QJsonObject prelabel = output.value(QStringLiteral("prelabel")).toObject();
    QJsonObject classification = prelabel.value(QStringLiteral("classification")).toObject();
    classification.insert(task, QJsonObject{
        {QStringLiteral("engine"), classificationResult.value(QStringLiteral("engine")).toString(QStringLiteral("PaddleOCR/deploy/cpp_infer/classification"))},
        {QStringLiteral("result_json"), classificationResult.value(QStringLiteral("result_json")).toString()},
        {QStringLiteral("label"), label},
        {QStringLiteral("score"), classificationResult.value(QStringLiteral("scores")).toArray().isEmpty()
                ? QJsonValue()
                : classificationResult.value(QStringLiteral("scores")).toArray().first()},
    });
    prelabel.insert(QStringLiteral("classification"), classification);
    output.insert(QStringLiteral("prelabel"), prelabel);
    return output;
}

}  // namespace ppocr
