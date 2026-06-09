#include "paddle/PaddleDocLayoutEngine.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStringList>
#include <QTextStream>

namespace {

QString defaultImagePath() {
    return QDir(ppocr::PaddleDocLayoutEngine::projectRoot()).filePath(
        QStringLiteral("PaddleOCR/deploy/lite/imgs/lite_demo.png"));
}

QString defaultOutputDir() {
    return QDir(ppocr::PaddleDocLayoutEngine::projectRoot()).filePath(
        QStringLiteral("build_vs2026/layout_output"));
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ppocr_layout_predict"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Run local C++ PP-DocLayout prediction through Paddle Inference."));
    const QCommandLineOption helpOpt = parser.addHelpOption();
    const QCommandLineOption baseDirOpt(QStringLiteral("base-dir"), QStringLiteral("PPOCR workbench base directory."), QStringLiteral("dir"), ppocr::PaddleDocLayoutEngine::projectRoot());
    const QCommandLineOption inputOpt(QStringLiteral("input"), QStringLiteral("Input image or directory."), QStringLiteral("path"), defaultImagePath());
    const QCommandLineOption outputOpt(QStringLiteral("output"), QStringLiteral("Output directory."), QStringLiteral("dir"), defaultOutputDir());
    const QCommandLineOption modelDirOpt(QStringLiteral("model-dir"), QStringLiteral("Layout inference model directory."), QStringLiteral("dir"));
    const QCommandLineOption modelNameOpt(QStringLiteral("model-name"), QStringLiteral("Layout model name."), QStringLiteral("name"));
    const QCommandLineOption deviceOpt(QStringLiteral("device"), QStringLiteral("Inference device."), QStringLiteral("cpu|gpu"));
    const QCommandLineOption cpuThreadsOpt(QStringLiteral("cpu-threads"), QStringLiteral("CPU thread count for PaddleX layout prediction."), QStringLiteral("n"));
    const QCommandLineOption thresholdOpt(QStringLiteral("threshold"), QStringLiteral("Minimum layout score threshold."), QStringLiteral("value"));
    const QCommandLineOption noVisualOpt(QStringLiteral("no-visual"), QStringLiteral("Do not write visualization images."));
    parser.addOptions({
        baseDirOpt,
        inputOpt,
        outputOpt,
        modelDirOpt,
        modelNameOpt,
        deviceOpt,
        cpuThreadsOpt,
        thresholdOpt,
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

    const QString baseDir = parser.value(baseDirOpt);
    ppocr::PaddleDocLayoutModelConfig config = ppocr::PaddleDocLayoutEngine::defaultConfig(baseDir);
    if (parser.isSet(modelDirOpt)) {
        config.modelDir = parser.value(modelDirOpt);
    }
    if (parser.isSet(modelNameOpt)) {
        config.modelName = parser.value(modelNameOpt);
    }
    if (parser.isSet(deviceOpt)) {
        config.device = parser.value(deviceOpt);
    }
    if (parser.isSet(cpuThreadsOpt)) {
        bool ok = false;
        const int parsedThreads = parser.value(cpuThreadsOpt).toInt(&ok);
        if (!ok || parsedThreads < 1) {
            QTextStream(stderr) << "Invalid --cpu-threads value: " << parser.value(cpuThreadsOpt) << '\n';
            return 1;
        }
        config.cpuThreads = parsedThreads;
    }
    if (parser.isSet(thresholdOpt)) {
        bool ok = false;
        const double parsedThreshold = parser.value(thresholdOpt).toDouble(&ok);
        if (!ok) {
            QTextStream(stderr) << "Invalid --threshold value: " << parser.value(thresholdOpt) << '\n';
            return 1;
        }
        config.threshold = parsedThreshold;
    }

    const QString inputPath = parser.value(inputOpt);
    const QString outputDir = parser.value(outputOpt);
    const bool saveVisualization = !parser.isSet(noVisualOpt);
    const QJsonObject summary = ppocr::PaddleDocLayoutEngine::predictPath(inputPath, outputDir, config, saveVisualization);
    QTextStream(stdout) << QJsonDocument(summary).toJson(QJsonDocument::Indented);
    return summary.value(QStringLiteral("ok")).toBool() ? 0 : 2;
}
