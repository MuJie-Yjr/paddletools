#include "core/OcrPrelabeler.h"

#include "core/AnnotationOps.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QMap>
#include <QVariant>

namespace ppocr {
namespace {

QJsonArray resultPolys(const QJsonObject& ocrResult) {
    const QJsonArray recPolys = ocrResult.value("rec_polys").toArray();
    if (!recPolys.isEmpty()) {
        return recPolys;
    }
    return ocrResult.value("dt_polys").toArray();
}

QString normalizeDocAngle(const QJsonValue& value) {
    bool ok = false;
    const int angle = value.toVariant().toInt(&ok);
    if (!ok) {
        return {};
    }
    if (angle == 0 || angle == 90 || angle == 180 || angle == 270) {
        return QString::number(angle);
    }
    return {};
}

QString normalizeTextlineAngle(const QJsonValue& value) {
    bool ok = false;
    const int angle = value.toVariant().toInt(&ok);
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

QString mostCommon(const QStringList& values) {
    QMap<QString, int> counts;
    for (const auto& value : values) {
        if (!value.isEmpty()) {
            counts[value] += 1;
        }
    }
    QString best;
    int bestCount = 0;
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        if (it.value() > bestCount) {
            best = it.key();
            bestCount = it.value();
        }
    }
    return best;
}

QJsonObject withRegionConfidence(QJsonObject annotation, double confidence, bool hasConfidence) {
    if (!hasConfidence) {
        return annotation;
    }
    QJsonArray regions = annotation.value("regions").toArray();
    if (regions.isEmpty()) {
        return annotation;
    }
    QJsonObject region = regions.last().toObject();
    region["confidence"] = confidence;
    regions.replace(regions.size() - 1, region);
    annotation["regions"] = regions;
    return annotation;
}

}  // namespace

QJsonObject OcrPrelabeler::applyOcrResult(const QJsonObject& annotation, const QJsonObject& ocrResult) {
    QJsonObject output = AnnotationOps::clearAnnotation(annotation);
    const QJsonArray polys = resultPolys(ocrResult);
    const QJsonArray texts = ocrResult.value("rec_texts").toArray();
    const QJsonArray scores = ocrResult.value("rec_scores").toArray();

    for (int i = 0; i < polys.size(); ++i) {
        const QJsonArray points = polys.at(i).toArray();
        const QString text = i < texts.size() ? texts.at(i).toString() : QString();
        const bool hasConfidence = i < scores.size() && scores.at(i).isDouble();
        const double confidence = hasConfidence ? scores.at(i).toDouble() : 0.0;
        output = AnnotationOps::addOcrRegion(output, points, text, AnnotationSource::OcrPrelabel, false);
        output = withRegionConfidence(output, confidence, hasConfidence);
    }

    const QJsonObject doc = ocrResult.value("doc_preprocessor_res").toObject();
    const QString docAngle = normalizeDocAngle(doc.value("angle"));
    if (!docAngle.isEmpty()) {
        output = AnnotationOps::setImageLabel(output, QStringLiteral("doc_orientation"), docAngle);
    }

    QStringList textlineAngles;
    for (const auto& value : ocrResult.value("textline_orientation_angles").toArray()) {
        const QString angle = normalizeTextlineAngle(value);
        if (!angle.isEmpty()) {
            textlineAngles.append(angle);
        }
    }
    const QString textlineAngle = mostCommon(textlineAngles);
    if (!textlineAngle.isEmpty()) {
        output = AnnotationOps::setImageLabel(output, QStringLiteral("textline_orientation"), textlineAngle);
    }

    if (polys.isEmpty()) {
        output["status"] = toString(PageStatus::Unlabeled);
    }
    output["prelabel"] = QJsonObject{
        {"engine", ocrResult.value("engine").toString("PaddleOCR/deploy/cpp_infer")},
        {"result_json", ocrResult.value("result_json").toString()},
        {"region_count", autoOcrRegionCount(output)},
    };
    return output;
}

int OcrPrelabeler::autoOcrRegionCount(const QJsonObject& annotation) {
    int count = 0;
    for (const auto& value : annotation.value("regions").toArray()) {
        const QJsonObject region = value.toObject();
        const auto type = regionTypeFromString(region.value("type").toString()).value_or(RegionType::OcrText);
        const QString sourceText = region.value("source").toString();
        const auto source = annotationSourceFromString(sourceText).value_or(AnnotationSource::Manual);
        if (type == RegionType::OcrText && (source == AnnotationSource::OcrPrelabel || sourceText == QStringLiteral("auto"))) {
            ++count;
        }
    }
    return count;
}

}  // namespace ppocr
