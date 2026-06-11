#include "core/AnnotationOps.h"
#include "core/AssetImporter.h"
#include "core/ClassificationPrelabeler.h"
#include "core/Exporter.h"
#include "core/LayoutPrelabeler.h"
#include "core/OcrPrelabeler.h"
#include "core/ProjectRepository.h"
#include "core/RuntimePaths.h"
#include "core/TrainingPreflight.h"
#include "core/TrainingRunner.h"
#include "core/TrainingTasks.h"
#include "core/Validator.h"
#include "paddle/PaddleOcrEngine.h"
#include "tests/fakes/FakeTrainingRunner.h"

#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QTextStream>
#include <exception>

namespace {

void writeJsonFile(const QString& path, const QJsonObject& object) {
    if (path.isEmpty()) {
        return;
    }
    QFileInfo info(path);
    QDir().mkpath(info.absolutePath());
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    }
}

int writeReport(const QJsonObject& report, const QString& reportPath) {
    writeJsonFile(reportPath, report);
    QTextStream(stdout) << QJsonDocument(report).toJson(QJsonDocument::Indented);
    return report.value(QStringLiteral("ok")).toBool() ? 0 : 2;
}

QString makeSourceImage(const QString& dir) {
    QDir().mkpath(dir);
    const QString imagePath = QDir(dir).filePath(QStringLiteral("workflow_source.jpg"));
    QImage image(360, 240, QImage::Format_RGB888);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::black, 2));
    painter.drawRect(QRect(24, 32, 250, 54));
    painter.setFont(QFont(QStringLiteral("Arial"), 22, QFont::Bold));
    painter.drawText(QRect(32, 38, 235, 44), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("WORKFLOW2026"));
    painter.setPen(QPen(QColor(80, 80, 80), 2));
    painter.drawRect(QRect(28, 110, 220, 70));
    painter.drawText(QRect(36, 120, 200, 40), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Layout Box"));
    painter.end();
    if (!image.save(imagePath)) {
        throw std::runtime_error(QStringLiteral("failed to create workflow source image: %1").arg(imagePath).toStdString());
    }
    return imagePath;
}

QString makeSourcePdf(const QString& dir) {
    QDir().mkpath(dir);
    const QString pdfPath = QDir(dir).filePath(QStringLiteral("workflow_source.pdf"));
    QPdfWriter writer(pdfPath);
    writer.setPageSize(QPageSize(QPageSize::A6));
    writer.setResolution(72);
    QPainter painter(&writer);
    painter.fillRect(QRectF(0, 0, 260, 180), Qt::white);
    painter.setPen(Qt::black);
    painter.setFont(QFont(QStringLiteral("Arial"), 16));
    painter.drawText(QRectF(20, 20, 220, 80), QStringLiteral("PDF workflow smoke"));
    painter.end();
    return pdfPath;
}

QJsonObject syntheticOcrResult() {
    QJsonArray poly;
    poly.append(QJsonArray{24, 32});
    poly.append(QJsonArray{274, 32});
    poly.append(QJsonArray{274, 86});
    poly.append(QJsonArray{24, 86});
    QJsonArray polys;
    polys.append(poly);
    return {
        {QStringLiteral("engine"), QStringLiteral("workflow_synthetic_ocr")},
        {QStringLiteral("result_json"), QStringLiteral("cache/ocr_prelabel/workflow_res.json")},
        {QStringLiteral("rec_polys"), polys},
        {QStringLiteral("rec_texts"), QJsonArray{QStringLiteral("WORKFLOW2026")}},
        {QStringLiteral("rec_scores"), QJsonArray{0.99}},
        {QStringLiteral("doc_preprocessor_res"), QJsonObject{{QStringLiteral("angle"), 0}}},
        {QStringLiteral("textline_orientation_angles"), QJsonArray{0}},
    };
}

QJsonObject syntheticLayoutResult() {
    return {
        {QStringLiteral("engine"), QStringLiteral("workflow_synthetic_layout")},
        {QStringLiteral("result_json"), QStringLiteral("cache/layout_prelabel/workflow_layout.json")},
        {QStringLiteral("boxes"), QJsonArray{
            QJsonObject{
                {QStringLiteral("cls_id"), 1},
                {QStringLiteral("label"), QStringLiteral("text")},
                {QStringLiteral("score"), 0.95},
                {QStringLiteral("coordinate"), QJsonArray{28, 110, 248, 180}},
            },
        }},
    };
}

QJsonObject syntheticClsResult(const QString& label) {
    return {
        {QStringLiteral("engine"), QStringLiteral("workflow_synthetic_cls")},
        {QStringLiteral("label_names"), QJsonArray{label}},
        {QStringLiteral("scores"), QJsonArray{0.97}},
    };
}

QJsonObject markRegionsChecked(QJsonObject annotation) {
    QJsonArray regions = annotation.value(QStringLiteral("regions")).toArray();
    for (int i = 0; i < regions.size(); ++i) {
        QJsonObject region = regions.at(i).toObject();
        region.insert(QStringLiteral("checked"), true);
        regions.replace(i, region);
    }
    annotation.insert(QStringLiteral("regions"), regions);
    if (!regions.isEmpty()) {
        annotation.insert(QStringLiteral("status"), QStringLiteral("labeled"));
    }
    return annotation;
}

int sampleCount(const QString& datasetDir) {
    return ppocr::TrainingPreflight::exportedSampleCount(datasetDir);
}

QJsonObject exportSummary(const QMap<QString, QString>& outputs) {
    QJsonObject summary;
    for (auto it = outputs.begin(); it != outputs.end(); ++it) {
        summary.insert(it.key(), QJsonObject{
            {QStringLiteral("dir"), QDir::toNativeSeparators(it.value())},
            {QStringLiteral("exists"), QFileInfo(it.value()).isDir()},
            {QStringLiteral("sample_count"), sampleCount(it.value())},
        });
    }
    return summary;
}

QJsonObject preflightAllTasks(
    const QString& baseDir,
    const ppocr::ProjectContext& context,
    const ppocr::TrainingOptions& options,
    QStringList* errors) {
    QJsonObject reports;
    for (const auto& task : ppocr::trainingTasks()) {
        if (!task.trainSupported) {
            reports.insert(task.key, QJsonObject{
                {QStringLiteral("ok"), true},
                {QStringLiteral("skipped"), true},
                {QStringLiteral("task_key"), task.key},
                {QStringLiteral("task_title"), task.title},
                {QStringLiteral("note"), task.note},
            });
            continue;
        }
        const auto preflight = ppocr::TrainingPreflight::run(baseDir, context, task.key, options);
        reports.insert(task.key, preflight.report);
        if (!preflight.ok) {
            errors->append(QStringLiteral("training preflight failed for %1: %2").arg(task.key, preflight.errors.join(QStringLiteral("; "))));
        }
    }
    return reports;
}

}  // namespace

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ppocr_workflow_smoke"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Run an end-to-end PPOCR workbench workflow smoke test."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption baseOpt(QStringLiteral("base-dir"), QStringLiteral("PPOCR paddletools root directory."), QStringLiteral("dir"));
    const QCommandLineOption projectOpt(QStringLiteral("project"), QStringLiteral("Project output directory. Existing directory is replaced."), QStringLiteral("dir"));
    const QCommandLineOption reportOpt(QStringLiteral("report"), QStringLiteral("Write JSON report to this file."), QStringLiteral("file"));
    const QCommandLineOption pythonOpt(QStringLiteral("python"), QStringLiteral("Python executable for PaddleX layout/training."), QStringLiteral("exe"));
    const QCommandLineOption withOcrOpt(QStringLiteral("with-ocr"), QStringLiteral("Also run real C++ OCR prediction and apply the result to the first page."));
    parser.addOptions({baseOpt, projectOpt, reportOpt, pythonOpt, withOcrOpt});
    parser.process(app);

    const QString baseDir = QFileInfo(parser.value(baseOpt).isEmpty() ? ppocr::RuntimePaths::defaultBaseDir() : parser.value(baseOpt)).absoluteFilePath();
    const QString projectDir = QFileInfo(parser.value(projectOpt).isEmpty()
            ? QDir(baseDir).filePath(QStringLiteral("build_vs2026/workflow_smoke_project"))
            : parser.value(projectOpt))
        .absoluteFilePath();
    const QString reportPath = parser.value(reportOpt);
    const bool withOcr = parser.isSet(withOcrOpt);
    QStringList errors;

    QJsonObject report{
        {QStringLiteral("ok"), false},
        {QStringLiteral("base_dir"), QDir::toNativeSeparators(baseDir)},
        {QStringLiteral("project_dir"), QDir::toNativeSeparators(projectDir)},
        {QStringLiteral("with_ocr"), withOcr},
        {QStringLiteral("started_at"), QDateTime::currentDateTime().toString(Qt::ISODate)},
    };

    try {
        QDir existing(projectDir);
        if (existing.exists() && !existing.removeRecursively()) {
            throw std::runtime_error(QStringLiteral("failed to clean project directory: %1").arg(projectDir).toStdString());
        }

        const QString inputDir = QDir(projectDir).filePath(QStringLiteral("_inputs"));
        const QString sourceImage = makeSourceImage(inputDir);
        const QString sourcePdf = makeSourcePdf(inputDir);
        auto context = ppocr::ProjectRepository::createProject(projectDir, QStringLiteral("workflow_smoke"));
        const auto imported = ppocr::AssetImporter::importPaths(context, {sourceImage, sourcePdf});
        report.insert(QStringLiteral("imported_count"), imported.size());

        auto pages = ppocr::ProjectRepository::listPages(context);
        if (pages.size() < 2) {
            errors.append(QStringLiteral("expected at least 2 imported pages, got %1").arg(pages.size()));
        }

        if (!pages.isEmpty()) {
            const auto page = pages.first();
            QJsonObject annotation = ppocr::ProjectRepository::readAnnotation(page.annotationPath);
            annotation = ppocr::OcrPrelabeler::applyOcrResult(annotation, syntheticOcrResult());
            annotation = ppocr::LayoutPrelabeler::applyLayoutResult(annotation, syntheticLayoutResult());
            annotation = ppocr::ClassificationPrelabeler::applyClassificationResult(annotation, QStringLiteral("doc_orientation"), syntheticClsResult(QStringLiteral("0")));
            annotation = ppocr::ClassificationPrelabeler::applyClassificationResult(annotation, QStringLiteral("textline_orientation"), syntheticClsResult(QStringLiteral("0")));
            annotation = ppocr::ClassificationPrelabeler::applyClassificationResult(annotation, QStringLiteral("table_classification"), syntheticClsResult(QStringLiteral("wired_table")));
            annotation = ppocr::AnnotationOps::addOcrRegion(
                annotation,
                QJsonArray{QJsonArray{26, 34}, QJsonArray{274, 34}, QJsonArray{274, 84}, QJsonArray{26, 84}},
                QStringLiteral("WORKFLOW2026"),
                ppocr::AnnotationSource::Manual,
                true);
            annotation = ppocr::AnnotationOps::addLayoutRegion(annotation, QRectF(28, 110, 220, 70), QStringLiteral("text"), ppocr::AnnotationSource::Manual, true);
            annotation = ppocr::AnnotationOps::setImageLabel(annotation, QStringLiteral("doc_orientation"), QStringLiteral("0"));
            annotation = ppocr::AnnotationOps::setImageLabel(annotation, QStringLiteral("textline_orientation"), QStringLiteral("0"));
            annotation = ppocr::AnnotationOps::setImageLabel(annotation, QStringLiteral("table_classification"), QStringLiteral("wired_table"));
            ppocr::ProjectRepository::writeAnnotation(page.annotationPath, annotation);
            report.insert(QStringLiteral("synthetic_auto_ocr_regions"), ppocr::OcrPrelabeler::autoOcrRegionCount(annotation));
            report.insert(QStringLiteral("synthetic_auto_layout_regions"), ppocr::LayoutPrelabeler::autoLayoutRegionCount(annotation));

            if (withOcr) {
                const QString ocrOut = context.path(QStringLiteral("predictions/workflow_ocr"));
                const auto ocrReport = ppocr::PaddleOcrEngine::predictImage(page.imagePath, ocrOut, ppocr::PaddleOcrEngine::defaultConfig(baseDir), false);
                report.insert(QStringLiteral("real_ocr_prediction"), ocrReport);
                if (!ocrReport.value(QStringLiteral("ok")).toBool()) {
                    errors.append(QStringLiteral("real OCR prediction failed"));
                } else {
                    QJsonObject updated = ppocr::OcrPrelabeler::applyOcrResult(
                        ppocr::ProjectRepository::readAnnotation(page.annotationPath),
                        ocrReport);
                    updated = ppocr::LayoutPrelabeler::applyLayoutResult(updated, syntheticLayoutResult());
                    updated = ppocr::ClassificationPrelabeler::applyClassificationResult(updated, QStringLiteral("doc_orientation"), syntheticClsResult(QStringLiteral("0")));
                    updated = ppocr::ClassificationPrelabeler::applyClassificationResult(updated, QStringLiteral("textline_orientation"), syntheticClsResult(QStringLiteral("0")));
                    updated = ppocr::ClassificationPrelabeler::applyClassificationResult(updated, QStringLiteral("table_classification"), syntheticClsResult(QStringLiteral("wired_table")));
                    updated = markRegionsChecked(updated);
                    ppocr::ProjectRepository::writeAnnotation(page.annotationPath, updated);
                    report.insert(QStringLiteral("real_ocr_auto_regions"), ppocr::OcrPrelabeler::autoOcrRegionCount(updated));
                }
            }
        }

        if (pages.size() > 1) {
            const auto valPage = pages.last();
            QJsonObject annotation = ppocr::ProjectRepository::readAnnotation(valPage.annotationPath);
            annotation.insert(QStringLiteral("split"), QStringLiteral("val"));
            annotation = ppocr::AnnotationOps::addOcrRegion(
                annotation,
                QJsonArray{QJsonArray{22, 24}, QJsonArray{190, 24}, QJsonArray{190, 58}, QJsonArray{22, 58}},
                QStringLiteral("VAL2026"),
                ppocr::AnnotationSource::Manual,
                true);
            annotation = ppocr::AnnotationOps::addLayoutRegion(annotation, QRectF(24, 82, 170, 52), QStringLiteral("text"), ppocr::AnnotationSource::Manual, true);
            annotation = ppocr::AnnotationOps::setImageLabel(annotation, QStringLiteral("doc_orientation"), QStringLiteral("0"));
            annotation = ppocr::AnnotationOps::setImageLabel(annotation, QStringLiteral("textline_orientation"), QStringLiteral("0"));
            annotation = ppocr::AnnotationOps::setImageLabel(annotation, QStringLiteral("table_classification"), QStringLiteral("wired_table"));
            ppocr::ProjectRepository::writeAnnotation(valPage.annotationPath, annotation);
        }

        pages = ppocr::ProjectRepository::listPages(context);
        report.insert(QStringLiteral("page_count"), pages.size());
        const auto issues = ppocr::Validator::validateProject(context);
        QJsonArray issueArray;
        int validationErrors = 0;
        for (const auto& issue : issues) {
            issueArray.append(QJsonObject{
                {QStringLiteral("severity"), issue.severity},
                {QStringLiteral("task"), issue.task},
                {QStringLiteral("asset_id"), issue.assetId},
                {QStringLiteral("message"), issue.message},
            });
            if (issue.severity == QStringLiteral("error")) {
                ++validationErrors;
            }
        }
        report.insert(QStringLiteral("validation_errors"), validationErrors);
        report.insert(QStringLiteral("validation_issues"), issueArray);
        if (validationErrors > 0) {
            errors.append(QStringLiteral("validation has %1 error(s)").arg(validationErrors));
        }

        const auto outputs = ppocr::Exporter::exportSelected(
            context,
            {QStringLiteral("det"), QStringLiteral("rec"), QStringLiteral("cls"), QStringLiteral("textline_cls"), QStringLiteral("table_cls"), QStringLiteral("coco")},
            true,
            true);
        report.insert(QStringLiteral("exports"), exportSummary(outputs));

        ppocr::TrainingOptions options;
        options.pythonExe = parser.value(pythonOpt).isEmpty() ? ppocr::RuntimePaths::defaultPaddlexPython() : parser.value(pythonOpt);
        options.device = QStringLiteral("cpu");
        options.epochs = 1;
        options.batchSize = 1;
        options.learningRate = 0.001;
        report.insert(QStringLiteral("training_preflight"), preflightAllTasks(baseDir, context, options, &errors));
        const auto simulated = ppocr::tests::FakeTrainingRunner::simulateSuccess(
            baseDir,
            context,
            QStringLiteral("det_v5_server"),
            options,
            QStringLiteral("epoch: 1 loss: 0.1 hmean: 0.91 precision: 0.92 recall: 0.9\n"),
            QStringLiteral("workflow_det_simulated"));
        report.insert(QStringLiteral("training_simulated"), simulated.report);
        if (!simulated.ok) {
            errors.append(QStringLiteral("training simulation failed"));
        }

        report.insert(QStringLiteral("project"), ppocr::RuntimePaths::pathStatus(context.path(QStringLiteral("project.json"))));
        report.insert(QStringLiteral("runs"), ppocr::RuntimePaths::pathStatus(context.path(QStringLiteral("training/runs.json"))));
        report.insert(QStringLiteral("det_versions"), ppocr::RuntimePaths::pathStatus(context.path(QStringLiteral("training/det_v5_server/versions.json"))));
    } catch (const std::exception& exc) {
        errors.append(QString::fromUtf8(exc.what()));
    }

    report.insert(QStringLiteral("errors"), QJsonArray::fromStringList(errors));
    report.insert(QStringLiteral("ok"), errors.isEmpty());
    report.insert(QStringLiteral("finished_at"), QDateTime::currentDateTime().toString(Qt::ISODate));
    return writeReport(report, reportPath);
}
