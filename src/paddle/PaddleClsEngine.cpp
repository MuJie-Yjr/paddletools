#include "paddle/PaddleClsEngine.h"

#include "paddle/PaddleInferenceRuntime.h"
#include "paddle/PaddleOcrEngine.h"

#include "src/api/models/doc_img_orientation_classification.h"
#include "src/api/models/textline_orientation_classification.h"
#include "src/modules/image_classification/predictor.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
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
            QStringLiteral("ppocr_workbench_cls_%1")
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

QJsonObject errorReport(const QString& error, const QString& imagePath, const QString& outputDir, const PaddleClsModelConfig& config) {
    QJsonObject report = PaddleClsEngine::configReport(config);
    report.insert(QStringLiteral("ok"), false);
    report.insert(QStringLiteral("error"), error);
    report.insert(QStringLiteral("input_path"), imagePath);
    report.insert(QStringLiteral("output_dir"), outputDir);
    return report;
}

void validateConfig(const QString& imagePath, const PaddleClsModelConfig& config) {
    if (!QFileInfo::exists(imagePath)) {
        throw std::runtime_error(QStringLiteral("image does not exist: %1").arg(imagePath).toStdString());
    }
    QString reason;
    if (!PaddleInferenceRuntime::sdkAvailable(&reason)) {
        throw std::runtime_error(reason.toStdString());
    }
    if (!PaddleInferenceRuntime::modelDirLooksUsable(config.modelDir, &reason)) {
        throw std::runtime_error(QStringLiteral("classification model is not usable: %1").arg(reason).toStdString());
    }
}

QString defaultModelDir(const QDir& inferRoot, const QString& folderName) {
    const QString parentDir = inferRoot.filePath(folderName);
    const QString nestedDir = QDir(parentDir).filePath(folderName);
    QString reason;
    if (PaddleInferenceRuntime::modelDirLooksUsable(nestedDir, &reason)) {
        return nestedDir;
    }
    if (PaddleInferenceRuntime::modelDirLooksUsable(parentDir, &reason)) {
        return parentDir;
    }
    return nestedDir;
}

TextLineOrientationClassificationParams toTextlineParams(const PaddleClsModelConfig& config) {
    TextLineOrientationClassificationParams params;
    params.model_name = config.modelName.toStdString();
    params.model_dir = nativePathString(config.modelDir);
    params.device = config.device.toStdString();
    params.precision = config.precision.toStdString();
    params.enable_mkldnn = config.enableMkldnn;
    params.mkldnn_cache_capacity = config.mkldnnCacheCapacity;
    params.cpu_threads = config.cpuThreads;
    return params;
}

DocImgOrientationClassificationParams toDocParams(const PaddleClsModelConfig& config) {
    DocImgOrientationClassificationParams params;
    params.model_name = config.modelName.toStdString();
    params.model_dir = nativePathString(config.modelDir);
    params.device = config.device.toStdString();
    params.precision = config.precision.toStdString();
    params.enable_mkldnn = config.enableMkldnn;
    params.mkldnn_cache_capacity = config.mkldnnCacheCapacity;
    params.cpu_threads = config.cpuThreads;
    return params;
}

ClasPredictorParams toClasParams(const PaddleClsModelConfig& config) {
    ClasPredictorParams params;
    if (!config.modelName.isEmpty()) {
        params.model_name = config.modelName.toStdString();
    }
    params.model_dir = nativePathString(config.modelDir);
    params.device = config.device.toStdString();
    params.precision = config.precision.toStdString();
    params.enable_mkldnn = config.enableMkldnn;
    params.mkldnn_cache_capacity = config.mkldnnCacheCapacity;
    params.cpu_threads = config.cpuThreads;
    return params;
}

QJsonObject readPredictionReport(
    const QString& sdkImagePath,
    const QString& sdkOutputDir,
    const QString& originalImagePath,
    const QString& originalOutputDir,
    const PaddleClsModelConfig& originalConfig,
    const PaddleClsModelConfig& resolvedConfig) {
    const QString resultJsonPath = findResultJson(sdkOutputDir, sdkImagePath);
    if (resultJsonPath.isEmpty()) {
        return errorReport(QStringLiteral("classification finished but no result JSON was written"), originalImagePath, originalOutputDir, originalConfig);
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
    QJsonObject report = document.object();
    report.insert(QStringLiteral("ok"), true);
    report.insert(QStringLiteral("input_path"), QFileInfo(originalImagePath).absoluteFilePath());
    report.insert(QStringLiteral("result_json"), finalJsonPath);
    report.insert(QStringLiteral("output_dir"), originalOutputDir);
    report.insert(QStringLiteral("task"), resolvedConfig.task);
    report.insert(QStringLiteral("engine"), QStringLiteral("PaddleOCR/deploy/cpp_infer/classification"));
    report.insert(QStringLiteral("model_dir"), resolvedConfig.modelDir);
    QFile out(finalJsonPath);
    if (out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        out.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
    }
    return report;
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

template <typename Classifier>
void runClassificationJobs(
    Classifier& classifier,
    const QList<QPair<QString, QString>>& imageJobs,
    const QVector<int>& activeIndexes,
    const std::vector<std::unique_ptr<SdkPathJob>>& sdkJobs,
    const PaddleClsModelConfig& originalConfig,
    const PaddleClsModelConfig& resolvedConfig,
    bool saveVisualization,
    const std::function<void(int current, int total, const QString& imagePath)>& progress,
    QList<QJsonObject>* reports) {
    for (int pos = 0; pos < activeIndexes.size(); ++pos) {
        const int jobIndex = activeIndexes.at(pos);
        const SdkPathJob* job = sdkJobs.at(jobIndex).get();
        const QString imagePath = job->originalImagePath;
        if (progress) {
            progress(pos + 1, activeIndexes.size(), imagePath);
        }
        try {
            auto outputs = classifier.Predict(nativePathString(job->sdkImagePath));
            if (outputs.empty()) {
                (*reports)[jobIndex] = errorReport(
                    QStringLiteral("classification finished but produced no output"),
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
            (*reports)[jobIndex] = readPredictionReport(
                job->sdkImagePath,
                job->sdkOutputDir,
                imagePath,
                job->originalOutputDir,
                originalConfig,
                resolvedConfig);
        } catch (const std::exception& exc) {
            (*reports)[jobIndex] = errorReport(QString::fromUtf8(exc.what()), imagePath, job->originalOutputDir, resolvedConfig);
        }
    }
}

}  // namespace

PaddleClsModelConfig PaddleClsEngine::defaultConfig(const QString& task, const QString& baseDir) {
    const QDir root(baseDir.isEmpty() ? PaddleOcrEngine::projectRoot() : baseDir);
    const QDir inferRoot(root.filePath(QStringLiteral("model/infer")));

    PaddleClsModelConfig config;
    config.task = task;
    if (task == QStringLiteral("textline_orientation")) {
        config.modelName = QStringLiteral("PP-LCNet_x1_0_textline_ori");
        config.modelDir = defaultModelDir(inferRoot, QStringLiteral("PP-LCNet_x1_0_textline_ori_infer"));
    } else if (task == QStringLiteral("table_classification")) {
        config.modelName = QStringLiteral("PP-LCNet_x1_0_table_cls");
        config.modelDir = defaultModelDir(inferRoot, QStringLiteral("PP-LCNet_x1_0_table_cls_infer"));
    } else {
        config.task = QStringLiteral("doc_orientation");
        config.modelName = QStringLiteral("PP-LCNet_x1_0_doc_ori");
        config.modelDir = defaultModelDir(inferRoot, QStringLiteral("PP-LCNet_x1_0_doc_ori_infer"));
    }
    return config;
}

QJsonObject PaddleClsEngine::configReport(const PaddleClsModelConfig& config) {
    return {
        {QStringLiteral("engine"), QStringLiteral("PaddleOCR/deploy/cpp_infer/classification")},
        {QStringLiteral("sdk_root"), PaddleInferenceRuntime::sdkRoot()},
        {QStringLiteral("paddle_version"), PaddleInferenceRuntime::version()},
        {QStringLiteral("task"), config.task},
        {QStringLiteral("model_name"), config.modelName},
        {QStringLiteral("model_dir"), config.modelDir},
        {QStringLiteral("device"), config.device},
        {QStringLiteral("precision"), config.precision},
        {QStringLiteral("enable_mkldnn"), config.enableMkldnn},
        {QStringLiteral("cpu_threads"), config.cpuThreads},
    };
}

QJsonObject PaddleClsEngine::predictImage(
    const QString& imagePath,
    const QString& outputDir,
    const PaddleClsModelConfig& config,
    bool saveVisualization) {
    const auto reports = predictImages({qMakePair(imagePath, outputDir)}, config, saveVisualization);
    return reports.isEmpty()
        ? errorReport(QStringLiteral("classification finished but no report was produced"), imagePath, outputDir, config)
        : reports.first();
}

QList<QJsonObject> PaddleClsEngine::predictImages(
    const QList<QPair<QString, QString>>& imageJobs,
    const PaddleClsModelConfig& config,
    bool saveVisualization,
    const std::function<void(int current, int total, const QString& imagePath)>& progress) {
    QList<QJsonObject> reports;
    reports.reserve(imageJobs.size());
    std::vector<std::unique_ptr<SdkPathJob>> sdkJobs(static_cast<size_t>(imageJobs.size()));
    QVector<int> activeIndexes;
    activeIndexes.reserve(imageJobs.size());

    PaddleClsModelConfig resolvedConfig = config;
    resolvedConfig.modelDir = PaddleInferenceRuntime::resolveModelDir(config.modelDir);
    for (int i = 0; i < imageJobs.size(); ++i) {
        reports.append(QJsonObject{});
        const QString imagePath = imageJobs.at(i).first;
        try {
            const QString outputDir = ensureOutputDir(imageJobs.at(i).second);
            validateConfig(imagePath, resolvedConfig);
            sdkJobs[static_cast<size_t>(i)] = prepareSdkPathJob(imagePath, outputDir, i, QStringLiteral("ppocr_cls_sdk"));
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
        if (resolvedConfig.task == QStringLiteral("textline_orientation")) {
            TextLineOrientationClassification classifier(toTextlineParams(resolvedConfig));
            runClassificationJobs(
                classifier,
                imageJobs,
                activeIndexes,
                sdkJobs,
                config,
                resolvedConfig,
                saveVisualization,
                progress,
                &reports);
        } else if (resolvedConfig.task == QStringLiteral("doc_orientation")) {
            DocImgOrientationClassification classifier(toDocParams(resolvedConfig));
            runClassificationJobs(
                classifier,
                imageJobs,
                activeIndexes,
                sdkJobs,
                config,
                resolvedConfig,
                saveVisualization,
                progress,
                &reports);
        } else {
            ClasPredictor classifier(toClasParams(resolvedConfig));
            runClassificationJobs(
                classifier,
                imageJobs,
                activeIndexes,
                sdkJobs,
                config,
                resolvedConfig,
                saveVisualization,
                progress,
                &reports);
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
