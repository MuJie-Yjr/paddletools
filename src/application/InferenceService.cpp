#include "application/InferenceService.h"

namespace ppocr {

QString InferenceService::projectRoot() {
    return PaddleOcrEngine::projectRoot();
}

QString InferenceService::sdkRoot() {
    return PaddleInferenceRuntime::sdkRoot();
}

QString InferenceService::sdkVersion() {
    return PaddleInferenceRuntime::version();
}

bool InferenceService::sdkAvailable(QString* reason) {
    return PaddleInferenceRuntime::sdkAvailable(reason);
}

bool InferenceService::gpuSupported() {
    return PaddleInferenceRuntime::gpuSupported();
}

QString InferenceService::resolveModelDir(const QString& modelDir) {
    return PaddleInferenceRuntime::resolveModelDir(modelDir);
}

bool InferenceService::modelDirLooksUsable(const QString& modelDir, QString* reason) {
    return PaddleInferenceRuntime::modelDirLooksUsable(modelDir, reason);
}

bool InferenceService::layoutModelDirLooksUsable(const QString& modelDir, QString* reason) {
    return PaddleDocLayoutEngine::modelDirLooksUsable(modelDir, reason);
}

QJsonObject InferenceService::smokeReport(const QString& modelDir) {
    return PaddleInferenceRuntime::smokeReport(modelDir);
}

InferenceService::OcrConfig InferenceService::defaultOcrConfig(const QString& baseDir) {
    return PaddleOcrEngine::defaultConfig(baseDir);
}

InferenceService::ClassificationConfig InferenceService::defaultClassificationConfig(
    const QString& task,
    const QString& baseDir) {
    return PaddleClsEngine::defaultConfig(task, baseDir);
}

InferenceService::LayoutConfig InferenceService::defaultLayoutConfig(const QString& baseDir) {
    return PaddleDocLayoutEngine::defaultConfig(baseDir);
}

QJsonObject InferenceService::ocrConfigReport(const OcrConfig& config) {
    return PaddleOcrEngine::configReport(config);
}

QJsonObject InferenceService::classificationConfigReport(const ClassificationConfig& config) {
    return PaddleClsEngine::configReport(config);
}

QJsonObject InferenceService::layoutConfigReport(const LayoutConfig& config) {
    return PaddleDocLayoutEngine::configReport(config);
}

QJsonObject InferenceService::predictOcrImage(
    const QString& imagePath,
    const QString& outputDir,
    const OcrConfig& config,
    bool saveVisualization) {
    return PaddleOcrEngine::predictImage(imagePath, outputDir, config, saveVisualization);
}

QList<QJsonObject> InferenceService::predictOcrImages(
    const QList<ImageJob>& imageJobs,
    const OcrConfig& config,
    bool saveVisualization,
    const ProgressCallback& progress) {
    return PaddleOcrEngine::predictImages(imageJobs, config, saveVisualization, progress);
}

QJsonObject InferenceService::predictClassificationImage(
    const QString& imagePath,
    const QString& outputDir,
    const ClassificationConfig& config,
    bool saveVisualization) {
    return PaddleClsEngine::predictImage(imagePath, outputDir, config, saveVisualization);
}

QList<QJsonObject> InferenceService::predictClassificationImages(
    const QList<ImageJob>& imageJobs,
    const ClassificationConfig& config,
    bool saveVisualization,
    const ProgressCallback& progress) {
    return PaddleClsEngine::predictImages(imageJobs, config, saveVisualization, progress);
}

QJsonObject InferenceService::predictLayoutImage(
    const QString& imagePath,
    const QString& outputDir,
    const LayoutConfig& config,
    bool saveVisualization) {
    return PaddleDocLayoutEngine::predictImage(imagePath, outputDir, config, saveVisualization);
}

QList<QJsonObject> InferenceService::predictLayoutImages(
    const QList<ImageJob>& imageJobs,
    const LayoutConfig& config,
    bool saveVisualization,
    const ProgressCallback& progress) {
    return PaddleDocLayoutEngine::predictImages(imageJobs, config, saveVisualization, progress);
}

QJsonObject InferenceService::predictLayoutPath(
    const QString& inputPath,
    const QString& outputDir,
    const LayoutConfig& config,
    bool saveVisualization) {
    return PaddleDocLayoutEngine::predictPath(inputPath, outputDir, config, saveVisualization);
}

}  // namespace ppocr
