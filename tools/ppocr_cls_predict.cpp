#include "paddle/PaddleClsEngine.h"
#include "paddle/PaddleOcrEngine.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>
#include <QTextStream>

namespace {

QString defaultImagePath() {
    return QDir(ppocr::PaddleOcrEngine::projectRoot()).filePath(
        QStringLiteral("PaddleOCR/deploy/lite/imgs/lite_demo.png"));
}

QString defaultOutputDir() {
    return QDir(ppocr::PaddleOcrEngine::projectRoot()).filePath(
        QStringLiteral("build_vs2026/cls_output"));
}

QStringList collectImages(const QString& inputPath) {
    const QFileInfo info(inputPath);
    if (info.isFile()) {
        return {info.absoluteFilePath()};
    }
    if (!info.isDir()) {
        return {};
    }
    const QStringList filters{
        QStringLiteral("*.jpg"),
        QStringLiteral("*.jpeg"),
        QStringLiteral("*.png"),
        QStringLiteral("*.bmp"),
        QStringLiteral("*.tif"),
        QStringLiteral("*.tiff"),
    };
    QStringList images;
    const QFileInfoList entries = QDir(info.absoluteFilePath()).entryInfoList(filters, QDir::Files, QDir::Name);
    for (const auto& entry : entries) {
        images.append(entry.absoluteFilePath());
    }
    return images;
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ppocr_cls_predict"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Run local C++ PaddleOCR image classification."));
    const QCommandLineOption helpOpt = parser.addHelpOption();
    const QCommandLineOption taskOpt(
        QStringLiteral("task"),
        QStringLiteral("Classification task: doc_orientation, textline_orientation, or table_classification."),
        QStringLiteral("task"),
        QStringLiteral("doc_orientation"));
    const QCommandLineOption inputOpt(QStringLiteral("input"), QStringLiteral("Input image or directory."), QStringLiteral("path"), defaultImagePath());
    const QCommandLineOption outputOpt(QStringLiteral("output"), QStringLiteral("Output directory."), QStringLiteral("dir"), defaultOutputDir());
    const QCommandLineOption modelDirOpt(QStringLiteral("model-dir"), QStringLiteral("Classification inference model directory."), QStringLiteral("dir"));
    const QCommandLineOption modelNameOpt(QStringLiteral("model-name"), QStringLiteral("Classification model name."), QStringLiteral("name"));
    const QCommandLineOption deviceOpt(QStringLiteral("device"), QStringLiteral("Inference device."), QStringLiteral("cpu|gpu"), QStringLiteral("cpu"));
    const QCommandLineOption noVisualOpt(QStringLiteral("no-visual"), QStringLiteral("Do not write visualization images."));
    parser.addOptions({
        taskOpt,
        inputOpt,
        outputOpt,
        modelDirOpt,
        modelNameOpt,
        deviceOpt,
        noVisualOpt,
    });

    if (!parser.parse(app.arguments())) {
        QTextStream(stderr) << parser.errorText() << '\n';
        return 1;
    }
    if (parser.isSet(helpOpt)) {
        QTextStream(stdout) << parser.helpText();
        return 0;
    }

    const QString task = parser.value(taskOpt);
    const QStringList validTasks{
        QStringLiteral("doc_orientation"),
        QStringLiteral("textline_orientation"),
        QStringLiteral("table_classification"),
    };
    if (!validTasks.contains(task)) {
        QTextStream(stderr) << "Invalid --task value: " << task << '\n';
        return 1;
    }

    const QString inputPath = parser.value(inputOpt);
    const QString outputDir = parser.value(outputOpt);
    ppocr::PaddleClsModelConfig config = ppocr::PaddleClsEngine::defaultConfig(task);
    if (parser.isSet(modelDirOpt)) {
        config.modelDir = parser.value(modelDirOpt);
    }
    if (parser.isSet(modelNameOpt)) {
        config.modelName = parser.value(modelNameOpt);
    }
    config.device = parser.value(deviceOpt);
    const bool saveVisualization = !parser.isSet(noVisualOpt);

    const QStringList images = collectImages(inputPath);
    QJsonArray results;
    int okCount = 0;
    for (const auto& imagePath : images) {
        const auto report = ppocr::PaddleClsEngine::predictImage(imagePath, outputDir, config, saveVisualization);
        if (report.value(QStringLiteral("ok")).toBool()) {
            ++okCount;
        }
        results.append(report);
    }

    const QJsonObject summary{
        {QStringLiteral("ok"), !images.isEmpty() && okCount == images.size()},
        {QStringLiteral("task"), config.task},
        {QStringLiteral("input_path"), inputPath},
        {QStringLiteral("output_dir"), outputDir},
        {QStringLiteral("total"), images.size()},
        {QStringLiteral("ok_count"), okCount},
        {QStringLiteral("failed_count"), images.size() - okCount},
        {QStringLiteral("config"), ppocr::PaddleClsEngine::configReport(config)},
        {QStringLiteral("results"), results},
    };
    QTextStream(stdout) << QJsonDocument(summary).toJson(QJsonDocument::Indented);
    return summary.value(QStringLiteral("ok")).toBool() ? 0 : 2;
}
