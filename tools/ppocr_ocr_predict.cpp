#include "paddle/PaddleOcrEngine.h"

#include <QCoreApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QTextStream>

namespace {

QString defaultImagePath() {
    return QDir(ppocr::PaddleOcrEngine::projectRoot()).filePath(
        QStringLiteral("PaddleOCR/deploy/lite/imgs/lite_demo.png"));
}

QString defaultOutputDir() {
    return QDir(ppocr::PaddleOcrEngine::projectRoot()).filePath(
        QStringLiteral("build_vs2026/ocr_output"));
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
    QCoreApplication::setApplicationName(QStringLiteral("ppocr_ocr_predict"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Run local C++ PaddleOCR text detection and recognition."));
    const QCommandLineOption helpOpt = parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("image_path_or_dir"), QStringLiteral("Input image or directory."), QStringLiteral("[image_path_or_dir]"));
    parser.addPositionalArgument(QStringLiteral("output_dir"), QStringLiteral("Directory for JSON and visual outputs."), QStringLiteral("[output_dir]"));

    ppocr::PaddleOcrModelConfig config = ppocr::PaddleOcrEngine::defaultConfig();
    const QCommandLineOption inputOpt(QStringLiteral("input"), QStringLiteral("Input image or directory."), QStringLiteral("path"));
    const QCommandLineOption outputOpt(QStringLiteral("output"), QStringLiteral("Output directory."), QStringLiteral("dir"));
    const QCommandLineOption detModelDirOpt(QStringLiteral("det-model-dir"), QStringLiteral("Text detection inference model directory."), QStringLiteral("dir"), config.textDetectionModelDir);
    const QCommandLineOption recModelDirOpt(QStringLiteral("rec-model-dir"), QStringLiteral("Text recognition inference model directory."), QStringLiteral("dir"), config.textRecognitionModelDir);
    const QCommandLineOption detModelNameOpt(QStringLiteral("det-model-name"), QStringLiteral("Text detection model name."), QStringLiteral("name"), config.textDetectionModelName);
    const QCommandLineOption recModelNameOpt(QStringLiteral("rec-model-name"), QStringLiteral("Text recognition model name."), QStringLiteral("name"), config.textRecognitionModelName);
    const QCommandLineOption deviceOpt(QStringLiteral("device"), QStringLiteral("Inference device."), QStringLiteral("cpu|gpu"), config.device);
    const QCommandLineOption scoreOpt(QStringLiteral("score-threshold"), QStringLiteral("Minimum OCR score for exported regions."), QStringLiteral("value"), QString::number(config.scoreThreshold));
    const QCommandLineOption noVisualOpt(QStringLiteral("no-visual"), QStringLiteral("Do not write visualization images."));
    parser.addOptions({
        inputOpt,
        outputOpt,
        detModelDirOpt,
        recModelDirOpt,
        detModelNameOpt,
        recModelNameOpt,
        deviceOpt,
        scoreOpt,
        noVisualOpt,
    });

    const QStringList args = app.arguments();
    if (!parser.parse(args)) {
        QTextStream(stderr) << parser.errorText() << '\n';
        return 1;
    }
    if (parser.isSet(helpOpt)) {
        QTextStream(stdout) << parser.helpText()
                            << "\nDefaults:\n"
                            << "  image_path: " << defaultImagePath() << "\n"
                            << "  output_dir: " << defaultOutputDir() << "\n";
        return 0;
    }

    const QStringList positional = parser.positionalArguments();
    const QString positionalInput = positional.size() > 0 ? positional.at(0) : defaultImagePath();
    const QString positionalOutput = positional.size() > 1 ? positional.at(1) : defaultOutputDir();
    const QString inputPath = parser.isSet(inputOpt) ? parser.value(inputOpt) : positionalInput;
    const QString outputDir = parser.isSet(outputOpt) ? parser.value(outputOpt) : positionalOutput;
    config.textDetectionModelDir = parser.value(detModelDirOpt);
    config.textRecognitionModelDir = parser.value(recModelDirOpt);
    config.textDetectionModelName = parser.value(detModelNameOpt);
    config.textRecognitionModelName = parser.value(recModelNameOpt);
    config.device = parser.value(deviceOpt);

    bool scoreOk = false;
    const double parsedScore = parser.value(scoreOpt).toDouble(&scoreOk);
    if (!scoreOk) {
        QTextStream(stderr) << "Invalid --score-threshold value: " << parser.value(scoreOpt) << '\n';
        return 1;
    }
    config.scoreThreshold = parsedScore;
    const bool saveVisualization = !parser.isSet(noVisualOpt);

    const QStringList images = collectImages(inputPath);
    QJsonArray results;
    int okCount = 0;
    for (const auto& imagePath : images) {
        const auto report = ppocr::PaddleOcrEngine::predictImage(imagePath, outputDir, config, saveVisualization);
        if (report.value(QStringLiteral("ok")).toBool()) {
            ++okCount;
        }
        results.append(report);
    }

    const QJsonObject summary{
        {QStringLiteral("ok"), !images.isEmpty() && okCount == images.size()},
        {QStringLiteral("input_path"), inputPath},
        {QStringLiteral("output_dir"), outputDir},
        {QStringLiteral("total"), images.size()},
        {QStringLiteral("ok_count"), okCount},
        {QStringLiteral("failed_count"), images.size() - okCount},
        {QStringLiteral("config"), ppocr::PaddleOcrEngine::configReport(config)},
        {QStringLiteral("results"), results},
    };
    QTextStream(stdout) << QJsonDocument(summary).toJson(QJsonDocument::Indented);
    return summary.value(QStringLiteral("ok")).toBool() ? 0 : 2;
}
