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
#include "core/TrainingRunStore.h"
#include "core/TrainingTasks.h"
#include "core/Validator.h"
#include "paddle/PaddleClsEngine.h"
#include "paddle/PaddleDocLayoutEngine.h"
#include "paddle/PaddleProcess.h"
#include "tests/fakes/FakeTrainingRunner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPageSize>
#include <QPdfWriter>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTextStream>
#include <QStringList>
#include <cstdlib>

static void require(bool condition, const char* message, int line) {
    if (condition) {
        return;
    }
    QTextStream(stderr) << "test_core_workflow failed at line " << line << ": " << message << '\n';
    std::exit(1);
}

#define REQUIRE(condition, message) require((condition), (message), __LINE__)

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QTemporaryDir temp;
    REQUIRE(temp.isValid(), "temporary directory is valid");
    qputenv("PPOCR_BASE_DIR", temp.path().toUtf8());
    qputenv("PPOCR_PADDLEX_PYTHON", (temp.path() + "/configured_python.exe").toUtf8());
    REQUIRE(
        QFileInfo(ppocr::RuntimePaths::defaultBaseDir()).absoluteFilePath() == QFileInfo(temp.path()).absoluteFilePath(),
        "runtime base dir honors PPOCR_BASE_DIR");
    REQUIRE(
        ppocr::RuntimePaths::defaultPaddlexPython().endsWith("configured_python.exe"),
        "runtime PaddleX Python honors PPOCR_PADDLEX_PYTHON");
    const QJsonObject tempStatus = ppocr::RuntimePaths::pathStatus(temp.path());
    REQUIRE(tempStatus.value("exists").toBool() && tempStatus.value("is_dir").toBool(), "runtime path status recognizes directories");
    const QString directTableModelDir = temp.path() + "/direct_model_base/model/infer/PP-LCNet_x1_0_table_cls_infer";
    QDir().mkpath(directTableModelDir);
    QFile directTableModel(QDir(directTableModelDir).filePath("inference.json"));
    REQUIRE(directTableModel.open(QIODevice::WriteOnly | QIODevice::Truncate), "direct table model json placeholder is created");
    directTableModel.close();
    QFile directTableParams(QDir(directTableModelDir).filePath("inference.pdiparams"));
    REQUIRE(directTableParams.open(QIODevice::WriteOnly | QIODevice::Truncate), "direct table model params placeholder is created");
    directTableParams.close();
    REQUIRE(
        QFileInfo(ppocr::PaddleClsEngine::defaultConfig("table_classification", temp.path() + "/direct_model_base").modelDir).absoluteFilePath()
            == QFileInfo(directTableModelDir).absoluteFilePath(),
        "classification default config accepts directly extracted official inference models");

    const QString source = temp.path() + "/source.png";
    QImage image(320, 220, QImage::Format_RGB888);
    image.fill(Qt::white);
    REQUIRE(image.save(source), "source image is saved");

    auto context = ppocr::ProjectRepository::createProject(temp.path() + "/project", "demo");
    for (const QString& rel : {
             QStringLiteral("cache/ocr_prelabel"),
             QStringLiteral("cache/cls_prelabel"),
             QStringLiteral("cache/table_cls_prelabel"),
             QStringLiteral("cache/layout_prelabel"),
             QStringLiteral("cache/prediction_staging"),
             QStringLiteral("cache/prediction_clipboard"),
             QStringLiteral("predictions"),
         }) {
        REQUIRE(QFileInfo(context.path(rel)).isDir(), "new projects create prediction and prelabel runtime directories");
    }
    const auto imported = ppocr::AssetImporter::importPaths(context, {source});
    REQUIRE(imported.size() == 1, "one image is imported");

    auto pages = ppocr::ProjectRepository::listPages(context);
    REQUIRE(pages.size() == 1, "one page exists");
    const auto page = pages.first();
    REQUIRE(QFileInfo::exists(QDir(context.thumbRoot()).filePath(page.assetId + ".jpg")), "thumbnail exists for imported page");
    auto annotation = ppocr::ProjectRepository::readAnnotation(page.annotationPath);
    const QJsonObject fakeOcrResult{
        {"engine", "test_cpp_ocr"},
        {"result_json", "cache/ocr_prelabel/page_000001_res.json"},
        {"rec_polys", QJsonArray{
            QJsonArray{QJsonArray{10, 12}, QJsonArray{90, 12}, QJsonArray{90, 36}, QJsonArray{10, 36}},
            QJsonArray{QJsonArray{12, 48}, QJsonArray{120, 48}, QJsonArray{120, 76}, QJsonArray{12, 76}},
        }},
        {"rec_texts", QJsonArray{"HELLO", "2026"}},
        {"rec_scores", QJsonArray{0.91, 0.82}},
        {"doc_preprocessor_res", QJsonObject{{"angle", 0}}},
        {"textline_orientation_angles", QJsonArray{0, 0, 1}},
    };
    auto prelabelled = ppocr::OcrPrelabeler::applyOcrResult(annotation, fakeOcrResult);
    REQUIRE(ppocr::OcrPrelabeler::autoOcrRegionCount(prelabelled) == 2, "two auto OCR regions are written");
    REQUIRE(prelabelled.value("regions").toArray().first().toObject().value("text").toString() == "HELLO", "first OCR text is kept");
    REQUIRE(!prelabelled.value("regions").toArray().first().toObject().value("checked").toBool(), "auto OCR region is unchecked");
    REQUIRE(prelabelled.value("regions").toArray().first().toObject().value("confidence").toDouble() > 0.9, "confidence is copied");
    REQUIRE(prelabelled.value("image_labels").toArray().first().toObject().value("label").toString() == "0", "doc orientation is copied");
    prelabelled = ppocr::ClassificationPrelabeler::applyClassificationResult(
        prelabelled,
        "doc_orientation",
        QJsonObject{
            {"engine", "test_cpp_cls"},
            {"result_json", "cache/cls_prelabel/page_000001_doc_res.json"},
            {"label_names", QJsonArray{"180"}},
            {"scores", QJsonArray{0.98}},
        });
    prelabelled = ppocr::ClassificationPrelabeler::applyClassificationResult(
        prelabelled,
        "textline_orientation",
        QJsonObject{
            {"engine", "test_cpp_cls"},
            {"result_json", "cache/cls_prelabel/page_000001_textline_res.json"},
            {"label_names", QJsonArray{"180_degree"}},
            {"class_ids", QJsonArray{1}},
            {"scores", QJsonArray{0.87}},
        });
    const QJsonArray labelsAfterCls = prelabelled.value("image_labels").toArray();
    bool hasDoc180 = false;
    bool hasTextline180 = false;
    for (const auto& value : labelsAfterCls) {
        const QJsonObject label = value.toObject();
        hasDoc180 = hasDoc180 || (label.value("task").toString() == "doc_orientation" && label.value("label").toString() == "180");
        hasTextline180 = hasTextline180 || (label.value("task").toString() == "textline_orientation" && label.value("label").toString() == "180");
    }
    REQUIRE(hasDoc180, "document orientation classification is applied");
    REQUIRE(hasTextline180, "textline orientation class id is normalized");
    REQUIRE(
        prelabelled.value("prelabel").toObject().value("classification").toObject().contains("doc_orientation"),
        "classification prelabel metadata is written");
    prelabelled = ppocr::LayoutPrelabeler::applyLayoutResult(
        prelabelled,
        QJsonObject{
            {"engine", "test_paddlex_layout"},
            {"result_json", "cache/layout_prelabel/page_000001_layout.json"},
            {"boxes", QJsonArray{
                QJsonObject{
                    {"cls_id", 2},
                    {"label", "text"},
                    {"score", 0.93},
                    {"coordinate", QJsonArray{8, 10, 180, 64}},
                },
                QJsonObject{
                    {"cls_id", 4},
                    {"label", "table"},
                    {"score", 0.77},
                    {"coordinate", QJsonArray{20, 80, 210, 160}},
                },
            }},
        });
    REQUIRE(ppocr::LayoutPrelabeler::autoLayoutRegionCount(prelabelled) == 2, "two auto layout regions are written");
    REQUIRE(
        prelabelled.value("regions").toArray().last().toObject().value("label").toString() == "table",
        "layout label is copied");
    REQUIRE(
        prelabelled.value("regions").toArray().last().toObject().value("checked").toBool() == false,
        "auto layout region is unchecked");
    REQUIRE(
        prelabelled.value("prelabel").toObject().value("layout").toObject().value("region_count").toInt() == 2,
        "layout prelabel metadata is written");
    ppocr::ProjectRepository::writeAnnotation(page.annotationPath, prelabelled);

    const QString pdfSource = temp.path() + "/source.pdf";
    QPdfWriter pdfWriter(pdfSource);
    pdfWriter.setPageSize(QPageSize(QPageSize::A6));
    pdfWriter.setResolution(72);
    QPainter pdfPainter(&pdfWriter);
    pdfPainter.fillRect(QRectF(0, 0, 260, 180), Qt::white);
    pdfPainter.setPen(Qt::black);
    pdfPainter.drawText(QRectF(20, 20, 220, 80), "PDF import smoke");
    pdfPainter.end();
    REQUIRE(QFileInfo::exists(pdfSource), "test PDF is created");
    const auto importedPdfPages = ppocr::AssetImporter::importPaths(context, {pdfSource});
    REQUIRE(importedPdfPages.size() == 1, "one PDF page is imported");
    REQUIRE(QFileInfo::exists(importedPdfPages.first()), "PDF rendered page image exists");
    pages = ppocr::ProjectRepository::listPages(context);
    REQUIRE(pages.size() == 2, "image and PDF page both exist");

    auto valAnnotation = ppocr::ProjectRepository::readAnnotation(pages.last().annotationPath);
    valAnnotation["split"] = "val";
    valAnnotation = ppocr::AnnotationOps::addOcrRegion(
        valAnnotation,
        QJsonArray{
            QJsonArray{18, 18},
            QJsonArray{150, 18},
            QJsonArray{150, 52},
            QJsonArray{18, 52},
        },
        "VAL2026",
        ppocr::AnnotationSource::Manual,
        true);
    valAnnotation = ppocr::AnnotationOps::addLayoutRegion(valAnnotation, QRectF(16, 70, 150, 48), "text", ppocr::AnnotationSource::Manual, true);
    valAnnotation = ppocr::AnnotationOps::setImageLabel(valAnnotation, "doc_orientation", "0");
    valAnnotation = ppocr::AnnotationOps::setImageLabel(valAnnotation, "textline_orientation", "0");
    valAnnotation = ppocr::AnnotationOps::setImageLabel(valAnnotation, "table_classification", "wired_table");
    ppocr::ProjectRepository::writeAnnotation(pages.last().annotationPath, valAnnotation);

    annotation = ppocr::ProjectRepository::readAnnotation(page.annotationPath);
    annotation = ppocr::AnnotationOps::addOcrRegion(
        annotation,
        QJsonArray{
            QJsonArray{20, 20},
            QJsonArray{160, 20},
            QJsonArray{160, 60},
            QJsonArray{20, 60},
        },
        "A20260525",
        ppocr::AnnotationSource::Manual,
        true);
    const QString movedRegionId = annotation.value("regions").toArray().last().toObject().value("id").toString();
    annotation = ppocr::AnnotationOps::updateRegion(
        annotation,
        movedRegionId,
        QJsonObject{{"points", QJsonArray{
            QJsonArray{24, 26},
            QJsonArray{164, 26},
            QJsonArray{164, 66},
            QJsonArray{24, 66},
        }}});
    REQUIRE(
        annotation.value("regions").toArray().last().toObject().value("points").toArray().first().toArray().first().toDouble() == 24,
        "moved OCR region point is saved");
    annotation = ppocr::AnnotationOps::addLayoutRegion(annotation, QRectF(15, 80, 180, 60), "text");
    annotation = ppocr::AnnotationOps::setImageLabel(annotation, "doc_orientation", "0");
    annotation = ppocr::AnnotationOps::setImageLabel(annotation, "textline_orientation", "0");
    annotation = ppocr::AnnotationOps::setImageLabel(annotation, "table_classification", "wired_table");
    ppocr::ProjectRepository::writeAnnotation(page.annotationPath, annotation);

    const auto issues = ppocr::Validator::validateProject(context);
    for (const auto& issue : issues) {
        REQUIRE(issue.severity != "error", qPrintable(QString("unexpected validation error: %1").arg(issue.message)));
    }

    const auto outputs = ppocr::Exporter::exportSelected(
        context,
        {"det", "rec", "cls", "textline_cls", "table_cls", "coco"},
        true,
        true);
    const auto readText = [](const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString();
        }
        return QString::fromUtf8(file.readAll());
    };
    const auto readJson = [](const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return QJsonObject{};
        }
        return QJsonDocument::fromJson(file.readAll()).object();
    };
    const QString detTrainPath = QDir(outputs["det"]).filePath("train.txt");
    const QString recTrainPath = QDir(outputs["rec"]).filePath("train.txt");
    REQUIRE(QFileInfo(outputs["det"]).fileName() == "current", "project exports write det dataset into current directory");
    REQUIRE(QFileInfo(QDir(context.exportRoot()).filePath("ppocr_det/history")).isDir(), "project exports create per-dataset history directory");
    REQUIRE(QFileInfo::exists(detTrainPath), "det train.txt exists");
    REQUIRE(QFileInfo::exists(recTrainPath), "rec train.txt exists");
    REQUIRE(QFileInfo::exists(QDir(outputs["rec"]).filePath("dict.txt")), "rec dict.txt exists");
    REQUIRE(QFileInfo::exists(QDir(outputs["coco"]).filePath("annotations/instance_train.json")), "coco train json exists");
    const QString detTrain = readText(detTrainPath);
    REQUIRE(detTrain.contains("\"transcription\":\"A20260525\""), "det export preserves OCR transcription");
    REQUIRE(detTrain.contains("\"points\""), "det export writes OCR polygon points");
    const QString recTrain = readText(recTrainPath);
    REQUIRE(recTrain.contains("A20260525"), "rec export preserves crop text");
    const QString recDict = readText(QDir(outputs["rec"]).filePath("dict.txt"));
    REQUIRE(recDict.contains("A") && recDict.contains("0") && recDict.contains("5"), "rec dict is generated from crop text");
    const QJsonObject cocoExportTrain = readJson(QDir(outputs["coco"]).filePath("annotations/instance_train.json"));
    REQUIRE(!cocoExportTrain.value("images").toArray().isEmpty(), "coco export writes images");
    REQUIRE(!cocoExportTrain.value("annotations").toArray().isEmpty(), "coco export writes annotations");
    REQUIRE(!cocoExportTrain.value("categories").toArray().isEmpty(), "coco export writes categories");

    const QString bridgeBase = temp.path() + "/bridge base";
    QDir().mkpath(QDir(bridgeBase).filePath("PaddleX"));
    QDir().mkpath(QDir(bridgeBase).filePath("PaddleOCR"));
    const QString bridgePython = temp.path() + "/Python With Space/python.exe";
    const QString bridgeConfig = QDir(bridgeBase).filePath("PaddleX/config.yaml");
    const auto bridgeCommand = ppocr::PaddleProcess::trainingCommand(
        bridgeBase,
        bridgePython,
        bridgeConfig,
        {"Global.mode=train", "Global.device=cpu", "Train.epochs_iters=3"});
    REQUIRE(bridgeCommand.program == bridgePython, "PaddleX bridge keeps configured Python executable");
    REQUIRE(
        QFileInfo(bridgeCommand.workingDirectory).absoluteFilePath() == QFileInfo(QDir(bridgeBase).filePath("PaddleX")).absoluteFilePath(),
        "PaddleX bridge working directory is PaddleX");
    REQUIRE(bridgeCommand.arguments.size() == 9, "PaddleX bridge writes main.py -c and three overrides");
    REQUIRE(bridgeCommand.arguments.at(0) == "main.py", "PaddleX bridge entry point is main.py");
    REQUIRE(bridgeCommand.arguments.at(1) == "-c", "PaddleX bridge passes config switch");
    REQUIRE(bridgeCommand.arguments.at(2) == bridgeConfig, "PaddleX bridge passes config path");
    REQUIRE(bridgeCommand.arguments.at(3) == "-o" && bridgeCommand.arguments.at(4) == "Global.mode=train", "PaddleX bridge first override is preserved");
    REQUIRE(bridgeCommand.arguments.at(8) == "Train.epochs_iters=3", "PaddleX bridge final override is preserved");
    REQUIRE(
        bridgeCommand.environment.value("PADDLE_PDX_PADDLEOCR_PATH") == QDir(bridgeBase).filePath("PaddleOCR"),
        "PaddleX bridge points PaddleX at local PaddleOCR source");
    const QString pythonPath = bridgeCommand.environment.value("PYTHONPATH");
    REQUIRE(pythonPath.startsWith(QDir(bridgeBase).filePath("PaddleX") + QDir::listSeparator()), "PaddleX bridge PYTHONPATH starts with PaddleX");
    REQUIRE(pythonPath.contains(QDir(bridgeBase).filePath("PaddleOCR")), "PaddleX bridge PYTHONPATH includes PaddleOCR");
    REQUIRE(bridgeCommand.displayText().startsWith('"' + bridgePython + '"'), "PaddleX bridge display quotes Python paths with spaces");

    QStringList trainingTaskKeys;
    for (const auto& task : ppocr::trainingTasks()) {
        trainingTaskKeys.append(task.key);
    }
    for (const auto& key : {
             QStringLiteral("det_v5_mobile"),
             QStringLiteral("det_v4_server"),
             QStringLiteral("rec_v4_mobile_en"),
             QStringLiteral("textline_ori_x025"),
             QStringLiteral("layout_plus_l"),
             QStringLiteral("layout_s"),
             QStringLiteral("layout_block"),
             QStringLiteral("uvdoc"),
         }) {
        REQUIRE(trainingTaskKeys.contains(key), qPrintable(QString("training task is registered: %1").arg(key)));
    }
    REQUIRE(ppocr::trainingTaskByKey("layout_doc_s").key == "layout_s", "legacy layout_doc_s task key maps to layout_s");
    REQUIRE(!ppocr::trainingTaskByKey("uvdoc").trainSupported, "UVDoc is listed as a non-trainable compatibility task");

    const auto textlineCommand = ppocr::TrainingPreflight::buildCommand(
        bridgeBase,
        &context,
        "textline_ori_x025",
        temp.path() + "/textline_out",
        ppocr::TrainingOptions{});
    REQUIRE(textlineCommand.arguments.contains("Train.num_classes=2"), "classification training command includes num_classes");
    REQUIRE(textlineCommand.arguments.contains("Train.warmup_steps=100"), "classification training command includes warmup steps");
    const auto layoutCommand = ppocr::TrainingPreflight::buildCommand(
        bridgeBase,
        &context,
        "layout_s",
        temp.path() + "/layout_out",
        ppocr::TrainingOptions{});
    REQUIRE(layoutCommand.arguments.contains("Train.num_classes=6"), "layout training command uses project label count");
    const QJsonObject layoutBackendReport = ppocr::PaddleDocLayoutEngine::configReport(ppocr::PaddleDocLayoutEngine::defaultConfig(bridgeBase));
    REQUIRE(
        layoutBackendReport.value("backend").toString() == "paddle_inference_direct_cpp",
        "layout backend is explicitly reported as direct C++ Paddle Inference");
    REQUIRE(
        layoutBackendReport.value("engine").toString().contains("direct_cpp"),
        "layout report names the direct C++ engine");

    const QString bridgeMain = QDir(bridgeBase).filePath("PaddleX/main.py");
    QFile bridgeMainFile(bridgeMain);
    REQUIRE(bridgeMainFile.open(QIODevice::WriteOnly | QIODevice::Text), "mock PaddleX main.py is created");
    bridgeMainFile.write("# mock paddlex entry\n");
    bridgeMainFile.close();
    const QString detConfig = QDir(bridgeBase).filePath("PaddleX/paddlex/configs/modules/text_detection/PP-OCRv5_server_det.yaml");
    QDir().mkpath(QFileInfo(detConfig).absolutePath());
    QFile detConfigFile(detConfig);
    REQUIRE(detConfigFile.open(QIODevice::WriteOnly | QIODevice::Text), "mock det config is created");
    detConfigFile.write("Global:\n  mode: train\n");
    detConfigFile.close();
    const QString recConfig = QDir(bridgeBase).filePath("PaddleX/paddlex/configs/modules/text_recognition/PP-OCRv5_server_rec.yaml");
    QDir().mkpath(QFileInfo(recConfig).absolutePath());
    QFile recConfigFile(recConfig);
    REQUIRE(recConfigFile.open(QIODevice::WriteOnly | QIODevice::Text), "mock rec config is created");
    recConfigFile.write("Global:\n  mode: train\n");
    recConfigFile.close();
    ppocr::TrainingOptions preflightOptions;
    preflightOptions.pythonExe = "python";
    preflightOptions.device = "cpu";
    preflightOptions.epochs = 2;
    preflightOptions.batchSize = 1;
    preflightOptions.learningRate = 0.01;
    const auto preflight = ppocr::TrainingPreflight::run(
        bridgeBase,
        context,
        "det_v5_server",
        preflightOptions,
        "preflight_v1");
    REQUIRE(preflight.ok, qPrintable(QString("training preflight passes: %1").arg(preflight.errors.join("; "))));
    REQUIRE(preflight.sampleCount > 0, "training preflight counts exported samples");
    REQUIRE(QFileInfo::exists(QDir(preflight.datasetDir).filePath("train.txt")), "training preflight exports det train.txt");
    REQUIRE(preflight.command.arguments.contains("Train.epochs_iters=2"), "training preflight applies epoch override");
    REQUIRE(preflight.report.value("ok").toBool(), "training preflight report is ok");

    const auto simulatedRec = ppocr::tests::FakeTrainingRunner::simulateSuccess(
        bridgeBase,
        context,
        "rec_v5_server",
        preflightOptions,
        "epoch: 1 loss: 0.1 acc: 0.88\n",
        "rec_runner_v1");
    REQUIRE(simulatedRec.ok, "training runner simulated rec run succeeds");
    REQUIRE(simulatedRec.metrics.value("acc").toDouble() > 0.87, "training runner parses simulated metrics");
    const QString simulatedRecDir = ppocr::TrainingRunStore::versionDir(context, "rec_v5_server", "rec_runner_v1");
    REQUIRE(QFileInfo::exists(QDir(simulatedRecDir).filePath("config.input.json")), "training version writes input config snapshot");
    REQUIRE(QFileInfo::exists(QDir(simulatedRecDir).filePath("config.resolved.yaml")), "training version writes resolved config preview");
    REQUIRE(QFileInfo::exists(QDir(simulatedRecDir).filePath("dataset_snapshot.json")), "training version writes dataset snapshot");
    REQUIRE(QFileInfo::exists(QDir(simulatedRecDir).filePath("command.json")), "training version writes command json");
    REQUIRE(QFileInfo::exists(QDir(simulatedRecDir).filePath("environment.json")), "training version writes environment json");
    REQUIRE(QFileInfo::exists(QDir(simulatedRecDir).filePath("train.log")), "training version writes train log");
    REQUIRE(QFileInfo::exists(QDir(simulatedRecDir).filePath("metrics.jsonl")), "training version writes metrics jsonl");
    const QJsonObject simulatedResult = readJson(QDir(simulatedRecDir).filePath("result.json"));
    REQUIRE(simulatedResult.value("metrics").toObject().value("acc").toDouble() > 0.87, "training result json records metrics");
    auto recManifest = ppocr::TrainingRunStore::loadVersionManifest(context, "rec_v5_server");
    REQUIRE(recManifest.value("current_version_id").toString() == "rec_runner_v1", "training runner promotes current rec version");
    REQUIRE(recManifest.value("best_version_id").toString() == "rec_runner_v1", "training runner promotes best rec version");
    REQUIRE(ppocr::TrainingRunStore::latestRun(context, "rec_v5_server").value("status").toString() == "success", "training runner finishes run record");

    annotation = ppocr::ProjectRepository::readAnnotation(page.annotationPath);
    annotation = ppocr::AnnotationOps::addLayoutRegion(annotation, QRectF(40, 150, 80, 50), "figure");
    ppocr::ProjectRepository::writeAnnotation(page.annotationPath, annotation);
    const auto unknownLabelOutputs = ppocr::Exporter::exportSelected(context, {"coco"}, true, false);
    const auto archivedCocoExports = QDir(QDir(context.exportRoot()).filePath("coco_layout/history"))
        .entryList({QStringLiteral("*")}, QDir::Dirs | QDir::NoDotAndDotDot);
    REQUIRE(!archivedCocoExports.isEmpty(), "re-export archives previous coco export instead of deleting it");
    QFile cocoTrainFile(QDir(unknownLabelOutputs["coco"]).filePath("annotations/instance_train.json"));
    REQUIRE(cocoTrainFile.open(QIODevice::ReadOnly), "coco train json can be read");
    const QJsonObject cocoTrain = QJsonDocument::fromJson(cocoTrainFile.readAll()).object();
    bool hasFigureCategory = false;
    bool hasZeroCategory = false;
    for (const auto& value : cocoTrain.value("categories").toArray()) {
        const QJsonObject category = value.toObject();
        hasFigureCategory = hasFigureCategory || category.value("name").toString() == "figure";
    }
    for (const auto& value : cocoTrain.value("annotations").toArray()) {
        hasZeroCategory = hasZeroCategory || value.toObject().value("category_id").toInt() == 0;
    }
    REQUIRE(hasFigureCategory, "coco export adds unknown layout category");
    REQUIRE(!hasZeroCategory, "coco export never writes category 0");

    const QString externalRoot = temp.filePath(QStringLiteral("external_dataset"));
    QDir().mkpath(externalRoot);
    QFile sentinel(QDir(externalRoot).filePath(QStringLiteral("keep.txt")));
    REQUIRE(sentinel.open(QIODevice::WriteOnly | QIODevice::Text), "external export sentinel can be created");
    sentinel.write("keep");
    sentinel.close();
    ppocr::Exporter::ExportOptions externalOptions;
    externalOptions.outputRoot = externalRoot;
    externalOptions.timestampedTaskDirs = true;
    const auto externalOutputs = ppocr::Exporter::exportSelected(context, {"det"}, true, false, {}, externalOptions);
    REQUIRE(QFileInfo::exists(QDir(externalRoot).filePath(QStringLiteral("keep.txt"))), "external export does not remove user directory contents");
    REQUIRE(QFileInfo(externalOutputs["det"]).absolutePath() == QFileInfo(externalRoot).absoluteFilePath(), "external export writes a task timestamp subdirectory");
    REQUIRE(QFileInfo(externalOutputs["det"]).fileName().startsWith(QStringLiteral("ppocr_det_")), "external export task directory is timestamped");

    const QString runId = ppocr::TrainingRunStore::startRun(context, "det_v5_server", "Det V5 Server", "python main.py");
    ppocr::TrainingRunStore::finishRun(context, runId, ppocr::RunStatus::Finished, 0);
    const auto latest = ppocr::TrainingRunStore::latestRun(context, "det_v5_server");
    REQUIRE(latest.value("status").toString() == "success", "latest training run is success");
    QFile runsFile(context.path("training/runs.json"));
    REQUIRE(runsFile.open(QIODevice::ReadOnly), "runs.json can be read");
    REQUIRE(QJsonDocument::fromJson(runsFile.readAll()).isArray(), "runs.json stays Python-compatible array root");

    const QString v1 = "v1";
    const QString v1Dir = ppocr::TrainingRunStore::versionDir(context, "det_v5_server", v1);
    QDir().mkpath(v1Dir);
    const QString v1Run = ppocr::TrainingRunStore::startRun(
        context,
        "det_v5_server",
        "Det V5 Server",
        "python main.py",
        outputs["det"],
        v1Dir,
        "cpu",
        3,
        v1,
        v1Dir);
    ppocr::TrainingRunStore::startVersion(
        context,
        "det_v5_server",
        "Det V5 Server",
        ppocr::TrainingTaskKind::OcrDet,
        v1,
        "python main.py",
        v1Run,
        outputs["det"],
        v1Dir,
        "cpu",
        3);
    ppocr::TrainingRunStore::finishRun(context, v1Run, ppocr::RunStatus::Finished, 0, "", QJsonObject{{"hmean", 0.5}});
    ppocr::TrainingRunStore::finishVersion(context, "det_v5_server", ppocr::TrainingTaskKind::OcrDet, v1, ppocr::RunStatus::Finished, 0, "", QJsonObject{{"hmean", 0.5}});
    auto manifest = ppocr::TrainingRunStore::loadVersionManifest(context, "det_v5_server");
    REQUIRE(manifest.value("current_version_id").toString() == v1, "first successful version becomes current");
    REQUIRE(manifest.value("best_version_id").toString() == v1, "first successful version becomes best");

    const QString v2 = "v2";
    const QString v2Dir = ppocr::TrainingRunStore::versionDir(context, "det_v5_server", v2);
    QDir().mkpath(v2Dir);
    const QString v2Run = ppocr::TrainingRunStore::startRun(
        context,
        "det_v5_server",
        "Det V5 Server",
        "python main.py",
        outputs["det"],
        v2Dir,
        "cpu",
        3,
        v2,
        v2Dir);
    ppocr::TrainingRunStore::startVersion(
        context,
        "det_v5_server",
        "Det V5 Server",
        ppocr::TrainingTaskKind::OcrDet,
        v2,
        "python main.py",
        v2Run,
        outputs["det"],
        v2Dir,
        "cpu",
        3);
    const QString v2InferDir = QDir(v2Dir).filePath("best_accuracy/inference");
    QDir().mkpath(v2InferDir);
    QFile v2Params(QDir(v2InferDir).filePath("inference.pdiparams"));
    QFile v2Model(QDir(v2InferDir).filePath("inference.json"));
    REQUIRE(v2Params.open(QIODevice::WriteOnly), "mock inference params file is created");
    REQUIRE(v2Model.open(QIODevice::WriteOnly), "mock inference model file is created");
    v2Params.close();
    v2Model.close();
    ppocr::TrainingRunStore::finishRun(context, v2Run, ppocr::RunStatus::Finished, 0, "", QJsonObject{{"hmean", 0.7}});
    const QJsonObject finishedV2 = ppocr::TrainingRunStore::finishVersion(context, "det_v5_server", ppocr::TrainingTaskKind::OcrDet, v2, ppocr::RunStatus::Finished, 0, "", QJsonObject{{"hmean", 0.7}});
    REQUIRE(
        QFileInfo(finishedV2.value("inference_model_dir").toString()).absoluteFilePath() == QFileInfo(v2InferDir).absoluteFilePath(),
        "finished version records inference model directory");
    manifest = ppocr::TrainingRunStore::loadVersionManifest(context, "det_v5_server");
    REQUIRE(manifest.value("current_version_id").toString() == v2, "better det version becomes current");
    REQUIRE(manifest.value("best_version_id").toString() == v2, "better det version becomes best");
    ppocr::TrainingRunStore::setCurrentVersion(context, "det_v5_server", v1);
    manifest = ppocr::TrainingRunStore::loadVersionManifest(context, "det_v5_server");
    REQUIRE(manifest.value("current_version_id").toString() == v1, "successful version can be manually set current");
    REQUIRE(manifest.value("best_version_id").toString() == v2, "manual current change keeps best version");
    bool deleteCurrentFailed = false;
    try {
        ppocr::TrainingRunStore::deleteVersion(context, "det_v5_server", v1);
    } catch (const std::exception&) {
        deleteCurrentFailed = true;
    }
    REQUIRE(deleteCurrentFailed, "current version cannot be deleted");
    bool deleteBestFailed = false;
    try {
        ppocr::TrainingRunStore::deleteVersion(context, "det_v5_server", v2);
    } catch (const std::exception&) {
        deleteBestFailed = true;
    }
    REQUIRE(deleteBestFailed, "best version deletion requires confirmation");
    ppocr::TrainingRunStore::deleteVersion(context, "det_v5_server", v2, true, true);
    REQUIRE(!QFileInfo::exists(v2Dir), "deleted version directory is removed");
    manifest = ppocr::TrainingRunStore::loadVersionManifest(context, "det_v5_server");
    REQUIRE(manifest.value("current_version_id").toString() == v1, "deleting best keeps current version");
    REQUIRE(manifest.value("best_version_id").toString() == v1, "deleting best promotes remaining best");
    return 0;
}
