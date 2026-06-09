#include "paddle/PaddleOcrEngine.h"

#include "paddle/PaddleInferenceRuntime.h"

#include "src/api/pipelines/ocr.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QList>
#include <QStringList>
#include <QTemporaryDir>
#include <QVector>

#include <memory>
#include <stdexcept>
#include <vector>

namespace ppocr {
namespace {

std::string nativePathString(const QString& path) {
    const QByteArray bytes = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath()).toLocal8Bit();
    return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
}

bool isAsciiPath(const QString& path) {
    for (const QChar ch : QDir::toNativeSeparators(path)) {
        if (ch.unicode() > 0x7f) {
            return false;
        }
    }
    return true;
}

QString sdkTempTemplate(const QString& prefix) {
    QString root = QCoreApplication::applicationDirPath();
    if (!isAsciiPath(root)) {
        root = QDir::tempPath();
    }
    return QDir(root).filePath(prefix + QStringLiteral("_XXXXXX"));
}

QString ensureOutputDir(const QString& requestedOutputDir) {
    QString outputDir = requestedOutputDir;
    if (outputDir.isEmpty()) {
        outputDir = QDir::temp().filePath(
            QStringLiteral("ppocr_workbench_ocr_%1")
                .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz"))));
    }
    QDir dir(outputDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        throw std::runtime_error(QStringLiteral("failed to create output directory: %1").arg(outputDir).toStdString());
    }
    return QFileInfo(outputDir).absoluteFilePath();
}

QString resultJsonPathFor(const QString& outputDir, const QString& imagePath) {
    return QDir(outputDir).filePath(QFileInfo(imagePath).completeBaseName() + QStringLiteral("_res.json"));
}

QString findResultJson(const QString& outputDir, const QString& imagePath) {
    const QFileInfo imageInfo(imagePath);
    const QString expected = QDir(outputDir).filePath(imageInfo.completeBaseName() + QStringLiteral("_res.json"));
    if (QFileInfo::exists(expected)) {
        return expected;
    }

    const QFileInfoList files = QDir(outputDir).entryInfoList(
        QStringList{QStringLiteral("*_res.json"), QStringLiteral("*.json")},
        QDir::Files,
        QDir::Time);
    return files.isEmpty() ? QString() : files.first().absoluteFilePath();
}

QJsonObject errorReport(const QString& error, const QString& imagePath, const QString& outputDir, const PaddleOcrModelConfig& config) {
    QJsonObject report = PaddleOcrEngine::configReport(config);
    report.insert(QStringLiteral("ok"), false);
    report.insert(QStringLiteral("error"), error);
    report.insert(QStringLiteral("input_path"), imagePath);
    report.insert(QStringLiteral("output_dir"), outputDir);
    return report;
}

QJsonArray filteredArray(const QJsonArray& values, const QList<int>& keep) {
    QJsonArray filtered;
    for (int index : keep) {
        if (index >= 0 && index < values.size()) {
            filtered.append(values.at(index));
        }
    }
    return filtered;
}

QJsonObject filterOcrScores(QJsonObject report, double threshold) {
    if (threshold <= 0.0) {
        return report;
    }
    const QJsonArray scores = report.value(QStringLiteral("rec_scores")).toArray();
    if (scores.isEmpty()) {
        return report;
    }
    QList<int> keep;
    for (int i = 0; i < scores.size(); ++i) {
        if (scores.at(i).toDouble(-1.0) >= threshold) {
            keep.append(i);
        }
    }
    const QStringList parallelKeys{
        QStringLiteral("rec_scores"),
        QStringLiteral("rec_texts"),
        QStringLiteral("rec_polys"),
        QStringLiteral("dt_polys"),
        QStringLiteral("textline_orientation_angles"),
    };
    for (const auto& key : parallelKeys) {
        const QJsonArray values = report.value(key).toArray();
        if (values.size() == scores.size()) {
            report.insert(key, filteredArray(values, keep));
        }
    }
    report.insert(QStringLiteral("score_threshold"), threshold);
    report.insert(QStringLiteral("filtered_count"), scores.size() - keep.size());
    return report;
}

void rewriteJsonObject(const QString& path, const QJsonObject& object) {
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    }
}

void copyVisualizationArtifacts(const QString& sourceDir, const QString& targetDir) {
    const QFileInfoList files = QDir(sourceDir).entryInfoList(QDir::Files, QDir::Name);
    for (const QFileInfo& fileInfo : files) {
        if (fileInfo.suffix().compare(QStringLiteral("json"), Qt::CaseInsensitive) == 0) {
            continue;
        }
        const QString target = QDir(targetDir).filePath(fileInfo.fileName());
        if (QFileInfo::exists(target)) {
            QFile::remove(target);
        }
        QFile::copy(fileInfo.absoluteFilePath(), target);
    }
}

void validateConfig(const QString& imagePath, const PaddleOcrModelConfig& config) {
    if (!QFileInfo::exists(imagePath)) {
        throw std::runtime_error(QStringLiteral("image does not exist: %1").arg(imagePath).toStdString());
    }

    QString reason;
    if (!PaddleInferenceRuntime::sdkAvailable(&reason)) {
        throw std::runtime_error(reason.toStdString());
    }
    if (!PaddleInferenceRuntime::modelDirLooksUsable(config.textDetectionModelDir, &reason)) {
        throw std::runtime_error(QStringLiteral("text detection model is not usable: %1").arg(reason).toStdString());
    }
    if (!PaddleInferenceRuntime::modelDirLooksUsable(config.textRecognitionModelDir, &reason)) {
        throw std::runtime_error(QStringLiteral("text recognition model is not usable: %1").arg(reason).toStdString());
    }
}

QJsonObject readPredictionReport(
    const QString& sdkImagePath,
    const QString& sdkOutputDir,
    const QString& originalImagePath,
    const QString& originalOutputDir,
    const PaddleOcrModelConfig& originalConfig,
    const PaddleOcrModelConfig& resolvedConfig) {
    const QString resultJsonPath = findResultJson(sdkOutputDir, sdkImagePath);
    if (resultJsonPath.isEmpty()) {
        return errorReport(QStringLiteral("prediction finished but no result JSON was written"), originalImagePath, originalOutputDir, originalConfig);
    }

    QFile file(resultJsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return errorReport(QStringLiteral("failed to read result JSON: %1").arg(resultJsonPath), originalImagePath, originalOutputDir, originalConfig);
    }
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return errorReport(QStringLiteral("invalid result JSON: %1").arg(parseError.errorString()), originalImagePath, originalOutputDir, originalConfig);
    }

    const QString finalJsonPath = resultJsonPathFor(originalOutputDir, originalImagePath);
    QJsonObject report = filterOcrScores(document.object(), resolvedConfig.scoreThreshold);
    report.insert(QStringLiteral("ok"), true);
    report.insert(QStringLiteral("engine"), QStringLiteral("PaddleOCR/deploy/cpp_infer"));
    report.insert(QStringLiteral("input_path"), QFileInfo(originalImagePath).absoluteFilePath());
    report.insert(QStringLiteral("result_json"), finalJsonPath);
    report.insert(QStringLiteral("output_dir"), originalOutputDir);
    report.insert(QStringLiteral("sdk_root"), PaddleInferenceRuntime::sdkRoot());
    report.insert(QStringLiteral("paddle_version"), PaddleInferenceRuntime::version());
    report.insert(QStringLiteral("text_detection_model_dir"), resolvedConfig.textDetectionModelDir);
    report.insert(QStringLiteral("text_recognition_model_dir"), resolvedConfig.textRecognitionModelDir);
    rewriteJsonObject(finalJsonPath, report);
    return report;
}

struct SdkPathJob {
    QString originalImagePath;
    QString originalOutputDir;
    QString sdkImagePath;
    QString sdkOutputDir;
    std::unique_ptr<QTemporaryDir> tempDir;

    bool staged() const {
        return tempDir != nullptr;
    }
};

std::unique_ptr<SdkPathJob> prepareSdkPathJob(
    const QString& imagePath,
    const QString& outputDir,
    int index,
    const QString& prefix) {
    auto job = std::make_unique<SdkPathJob>();
    job->originalImagePath = QFileInfo(imagePath).absoluteFilePath();
    job->originalOutputDir = outputDir;
    job->sdkImagePath = job->originalImagePath;
    job->sdkOutputDir = outputDir;
    if (isAsciiPath(job->originalImagePath) && isAsciiPath(job->originalOutputDir)) {
        return job;
    }

    job->tempDir = std::make_unique<QTemporaryDir>(sdkTempTemplate(prefix));
    if (!job->tempDir->isValid()) {
        throw std::runtime_error(QStringLiteral("failed to create SDK staging directory").toStdString());
    }
    QDir tempRoot(job->tempDir->path());
    if (!tempRoot.mkpath(QStringLiteral("input")) || !tempRoot.mkpath(QStringLiteral("output"))) {
        throw std::runtime_error(QStringLiteral("failed to create SDK staging subdirectories").toStdString());
    }
    const QString suffix = QFileInfo(imagePath).suffix();
    const QString stagedName = suffix.isEmpty()
        ? QStringLiteral("input_%1").arg(index)
        : QStringLiteral("input_%1.%2").arg(index).arg(suffix);
    job->sdkImagePath = tempRoot.filePath(QStringLiteral("input/%1").arg(stagedName));
    job->sdkOutputDir = tempRoot.filePath(QStringLiteral("output"));
    if (!QFile::copy(job->originalImagePath, job->sdkImagePath)) {
        throw std::runtime_error(QStringLiteral("failed to copy image into SDK staging path: %1").arg(job->originalImagePath).toStdString());
    }
    return job;
}

PaddleOCRParams toPaddleParams(const PaddleOcrModelConfig& config) {
    PaddleOCRParams params;
    params.text_detection_model_name = config.textDetectionModelName.toStdString();
    params.text_detection_model_dir = nativePathString(config.textDetectionModelDir);
    params.text_recognition_model_name = config.textRecognitionModelName.toStdString();
    params.text_recognition_model_dir = nativePathString(config.textRecognitionModelDir);
    params.use_doc_orientation_classify = config.useDocOrientationClassify;
    params.use_doc_unwarping = config.useDocUnwarping;
    params.use_textline_orientation = config.useTextlineOrientation;
    params.device = config.device.toStdString();
    params.precision = config.precision.toStdString();
    params.lang = config.lang.toStdString();
    params.ocr_version = config.ocrVersion.toStdString();
    params.enable_mkldnn = config.enableMkldnn;
    params.mkldnn_cache_capacity = config.mkldnnCacheCapacity;
    params.cpu_threads = config.cpuThreads;
    params.thread_num = config.threadNum;
    return params;
}

}  // namespace

QString PaddleOcrEngine::projectRoot() {
#ifdef PPOCR_PROJECT_ROOT
    return QString::fromUtf8(PPOCR_PROJECT_ROOT);
#else
    return QDir(QCoreApplication::applicationDirPath()).absolutePath();
#endif
}

PaddleOcrModelConfig PaddleOcrEngine::defaultConfig(const QString& baseDir) {
    const QDir root(baseDir.isEmpty() ? projectRoot() : baseDir);
    const QDir inferRoot(root.filePath(QStringLiteral("model/infer")));

    PaddleOcrModelConfig config;
    config.textDetectionModelDir = inferRoot.filePath(
        QStringLiteral("PP-OCRv5_server_det_infer/PP-OCRv5_server_det_infer"));
    config.textRecognitionModelDir = inferRoot.filePath(
        QStringLiteral("PP-OCRv5_server_rec_infer/PP-OCRv5_server_rec_infer"));
    return config;
}

QJsonObject PaddleOcrEngine::configReport(const PaddleOcrModelConfig& config) {
    return {
        {QStringLiteral("engine"), QStringLiteral("PaddleOCR/deploy/cpp_infer")},
        {QStringLiteral("sdk_root"), PaddleInferenceRuntime::sdkRoot()},
        {QStringLiteral("paddle_version"), PaddleInferenceRuntime::version()},
        {QStringLiteral("text_detection_model_name"), config.textDetectionModelName},
        {QStringLiteral("text_detection_model_dir"), config.textDetectionModelDir},
        {QStringLiteral("text_recognition_model_name"), config.textRecognitionModelName},
        {QStringLiteral("text_recognition_model_dir"), config.textRecognitionModelDir},
        {QStringLiteral("device"), config.device},
        {QStringLiteral("precision"), config.precision},
        {QStringLiteral("lang"), config.lang},
        {QStringLiteral("ocr_version"), config.ocrVersion},
        {QStringLiteral("use_doc_orientation_classify"), config.useDocOrientationClassify},
        {QStringLiteral("use_doc_unwarping"), config.useDocUnwarping},
        {QStringLiteral("use_textline_orientation"), config.useTextlineOrientation},
        {QStringLiteral("enable_mkldnn"), config.enableMkldnn},
        {QStringLiteral("cpu_threads"), config.cpuThreads},
        {QStringLiteral("score_threshold"), config.scoreThreshold},
    };
}

QJsonObject PaddleOcrEngine::predictImage(
    const QString& imagePath,
    const QString& outputDir,
    const PaddleOcrModelConfig& config,
    bool saveVisualization) {
    const auto reports = predictImages({qMakePair(imagePath, outputDir)}, config, saveVisualization);
    return reports.isEmpty()
        ? errorReport(QStringLiteral("prediction finished but no report was produced"), imagePath, outputDir, config)
        : reports.first();
}

QList<QJsonObject> PaddleOcrEngine::predictImages(
    const QList<QPair<QString, QString>>& imageJobs,
    const PaddleOcrModelConfig& config,
    bool saveVisualization,
    const std::function<void(int current, int total, const QString& imagePath)>& progress) {
    QList<QJsonObject> reports;
    reports.reserve(imageJobs.size());
    std::vector<std::unique_ptr<SdkPathJob>> sdkJobs(static_cast<size_t>(imageJobs.size()));
    QVector<int> activeIndexes;
    activeIndexes.reserve(imageJobs.size());

    PaddleOcrModelConfig resolvedConfig = config;
    resolvedConfig.textDetectionModelDir = PaddleInferenceRuntime::resolveModelDir(config.textDetectionModelDir);
    resolvedConfig.textRecognitionModelDir = PaddleInferenceRuntime::resolveModelDir(config.textRecognitionModelDir);

    for (int i = 0; i < imageJobs.size(); ++i) {
        reports.append(QJsonObject{});
        const QString imagePath = imageJobs.at(i).first;
        try {
            const QString outputDir = ensureOutputDir(imageJobs.at(i).second);
            validateConfig(imagePath, resolvedConfig);
            sdkJobs[static_cast<size_t>(i)] = prepareSdkPathJob(imagePath, outputDir, i, QStringLiteral("ppocr_ocr_sdk"));
            activeIndexes.append(i);
        } catch (const std::exception& exc) {
            const QString outputDir = sdkJobs[static_cast<size_t>(i)]
                ? sdkJobs[static_cast<size_t>(i)]->originalOutputDir
                : imageJobs.at(i).second;
            reports[i] = errorReport(QString::fromUtf8(exc.what()), imagePath, outputDir, resolvedConfig);
        }
    }

    if (activeIndexes.isEmpty()) {
        return reports;
    }

    try {
        PaddleOCR ocr(toPaddleParams(resolvedConfig));
        for (int pos = 0; pos < activeIndexes.size(); ++pos) {
            const int jobIndex = activeIndexes.at(pos);
            const SdkPathJob* job = sdkJobs[static_cast<size_t>(jobIndex)].get();
            const QString imagePath = job->originalImagePath;
            if (progress) {
                progress(pos + 1, activeIndexes.size(), imagePath);
            }
            try {
                auto outputs = ocr.Predict(nativePathString(job->sdkImagePath));
                if (outputs.empty()) {
                    reports[jobIndex] = errorReport(
                        QStringLiteral("prediction finished but produced no output"),
                        imagePath,
                        job->originalOutputDir,
                        resolvedConfig);
                    continue;
                }
                for (auto& output : outputs) {
                    if (saveVisualization) {
                        output->SaveToImg(nativePathString(job->sdkOutputDir));
                    }
                    output->SaveToJson(nativePathString(job->sdkOutputDir));
                }
                if (saveVisualization && job->staged()) {
                    copyVisualizationArtifacts(job->sdkOutputDir, job->originalOutputDir);
                }
                reports[jobIndex] = readPredictionReport(
                    job->sdkImagePath,
                    job->sdkOutputDir,
                    imagePath,
                    job->originalOutputDir,
                    config,
                    resolvedConfig);
            } catch (const std::exception& exc) {
                reports[jobIndex] = errorReport(QString::fromUtf8(exc.what()), imagePath, job->originalOutputDir, resolvedConfig);
            }
        }
    } catch (const std::exception& exc) {
        for (const int jobIndex : activeIndexes) {
            reports[jobIndex] = errorReport(
                QString::fromUtf8(exc.what()),
                sdkJobs[static_cast<size_t>(jobIndex)] ? sdkJobs[static_cast<size_t>(jobIndex)]->originalImagePath : imageJobs.at(jobIndex).first,
                sdkJobs[static_cast<size_t>(jobIndex)] ? sdkJobs[static_cast<size_t>(jobIndex)]->originalOutputDir : imageJobs.at(jobIndex).second,
                resolvedConfig);
        }
    }
    return reports;
}

}  // namespace ppocr
