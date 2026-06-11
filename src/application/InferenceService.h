#pragma once

#include "paddle/PaddleClsEngine.h"
#include "paddle/PaddleDocLayoutEngine.h"
#include "paddle/PaddleInferenceRuntime.h"
#include "paddle/PaddleOcrEngine.h"

#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QString>
#include <functional>

namespace ppocr {

class InferenceService {
public:
    using OcrConfig = PaddleOcrModelConfig;
    using ClassificationConfig = PaddleClsModelConfig;
    using LayoutConfig = PaddleDocLayoutModelConfig;
    using ImageJob = QPair<QString, QString>;
    using ProgressCallback = std::function<void(int current, int total, const QString& imagePath)>;

    static QString projectRoot();
    static QString sdkRoot();
    static QString sdkVersion();
    static bool sdkAvailable(QString* reason = nullptr);
    static bool gpuSupported();
    static QString resolveModelDir(const QString& modelDir);
    static bool modelDirLooksUsable(const QString& modelDir, QString* reason = nullptr);
    static bool layoutModelDirLooksUsable(const QString& modelDir, QString* reason = nullptr);
    static QJsonObject smokeReport(const QString& modelDir);

    static OcrConfig defaultOcrConfig(const QString& baseDir = QString());
    static ClassificationConfig defaultClassificationConfig(const QString& task, const QString& baseDir = QString());
    static LayoutConfig defaultLayoutConfig(const QString& baseDir = QString());

    static QJsonObject ocrConfigReport(const OcrConfig& config);
    static QJsonObject classificationConfigReport(const ClassificationConfig& config);
    static QJsonObject layoutConfigReport(const LayoutConfig& config);

    static QJsonObject predictOcrImage(
        const QString& imagePath,
        const QString& outputDir,
        const OcrConfig& config,
        bool saveVisualization = true);
    static QList<QJsonObject> predictOcrImages(
        const QList<ImageJob>& imageJobs,
        const OcrConfig& config,
        bool saveVisualization = false,
        const ProgressCallback& progress = {});

    static QJsonObject predictClassificationImage(
        const QString& imagePath,
        const QString& outputDir,
        const ClassificationConfig& config,
        bool saveVisualization = true);
    static QList<QJsonObject> predictClassificationImages(
        const QList<ImageJob>& imageJobs,
        const ClassificationConfig& config,
        bool saveVisualization = false,
        const ProgressCallback& progress = {});

    static QJsonObject predictLayoutImage(
        const QString& imagePath,
        const QString& outputDir,
        const LayoutConfig& config,
        bool saveVisualization = true);
    static QList<QJsonObject> predictLayoutImages(
        const QList<ImageJob>& imageJobs,
        const LayoutConfig& config,
        bool saveVisualization = false,
        const ProgressCallback& progress = {});
    static QJsonObject predictLayoutPath(
        const QString& inputPath,
        const QString& outputDir,
        const LayoutConfig& config,
        bool saveVisualization = true);
};

}  // namespace ppocr
