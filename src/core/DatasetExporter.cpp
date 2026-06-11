#include "core/DatasetExporter.h"

#include "core/Exporter.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <exception>

namespace ppocr {
namespace {

int nonEmptyTextLineCount(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0;
    }
    int count = 0;
    while (!file.atEnd()) {
        if (!file.readLine().trimmed().isEmpty()) {
            ++count;
        }
    }
    return count;
}

int cocoAnnotationCount(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return 0;
    }
    return doc.object().value(QStringLiteral("annotations")).toArray().size();
}

}  // namespace

DatasetExportResult DatasetExporter::exportDataset(
    const ProjectContext& context,
    const DatasetExportPlan& plan) {
    DatasetExportResult result;
    try {
        const auto outputs = Exporter::exportSelected(
            context,
            {plan.exportTask},
            plan.checkedOnly,
            plan.requireValidation);
        result.datasetDir = outputs.value(plan.exportTask, plan.expectedDir);
        result.sampleCounts = exportedSampleCounts(result.datasetDir);
        result.trainSampleCount = result.sampleCounts.value(QStringLiteral("train")).toInt();
        result.valSampleCount = result.sampleCounts.value(QStringLiteral("val")).toInt();
        result.sampleCount = result.sampleCounts.value(QStringLiteral("total")).toInt();
        result.ok = true;
    } catch (const std::exception& exc) {
        result.error = QString::fromUtf8(exc.what());
    }

    result.report = QJsonObject{
        {QStringLiteral("ok"), result.ok},
        {QStringLiteral("plan"), DatasetExportPlanner::toJson(plan)},
        {QStringLiteral("dataset_dir"), QDir::toNativeSeparators(result.datasetDir)},
        {QStringLiteral("sample_count"), result.sampleCount},
        {QStringLiteral("sample_counts"), result.sampleCounts},
        {QStringLiteral("error"), result.error},
    };
    return result;
}

int DatasetExporter::exportedSampleCount(const QString& datasetDir) {
    return exportedSampleCounts(datasetDir).value(QStringLiteral("total")).toInt();
}

QJsonObject DatasetExporter::exportedSampleCounts(const QString& datasetDir) {
    const QDir dir(datasetDir);
    int train = nonEmptyTextLineCount(dir.filePath(QStringLiteral("train.txt")));
    int val = nonEmptyTextLineCount(dir.filePath(QStringLiteral("val.txt")));
    if (train + val <= 0) {
        train = cocoAnnotationCount(dir.filePath(QStringLiteral("annotations/instance_train.json")));
        val = cocoAnnotationCount(dir.filePath(QStringLiteral("annotations/instance_val.json")));
    }
    return QJsonObject{
        {QStringLiteral("train"), train},
        {QStringLiteral("val"), val},
        {QStringLiteral("total"), train + val},
    };
}

}  // namespace ppocr
