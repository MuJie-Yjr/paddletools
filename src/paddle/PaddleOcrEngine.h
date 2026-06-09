#pragma once

#include <QList>
#include <QJsonObject>
#include <QPair>
#include <QString>
#include <functional>

namespace ppocr {

struct PaddleOcrModelConfig {
    QString textDetectionModelName = QStringLiteral("PP-OCRv5_server_det");
    QString textDetectionModelDir;
    QString textRecognitionModelName = QStringLiteral("PP-OCRv5_server_rec");
    QString textRecognitionModelDir;
    QString device = QStringLiteral("cpu");
    QString precision = QStringLiteral("fp32");
    QString lang = QStringLiteral("ch");
    QString ocrVersion = QStringLiteral("PP-OCRv5");
    bool useDocOrientationClassify = false;
    bool useDocUnwarping = false;
    bool useTextlineOrientation = false;
    bool enableMkldnn = true;
    int mkldnnCacheCapacity = 10;
    int cpuThreads = 8;
    int threadNum = 1;
    double scoreThreshold = 0.0;
};

class PaddleOcrEngine {
public:
    static QString projectRoot();
    static PaddleOcrModelConfig defaultConfig(const QString& baseDir = QString());
    static QJsonObject configReport(const PaddleOcrModelConfig& config);
    static QJsonObject predictImage(
        const QString& imagePath,
        const QString& outputDir = QString(),
        const PaddleOcrModelConfig& config = defaultConfig(),
        bool saveVisualization = true);
    static QList<QJsonObject> predictImages(
        const QList<QPair<QString, QString>>& imageJobs,
        const PaddleOcrModelConfig& config = defaultConfig(),
        bool saveVisualization = false,
        const std::function<void(int current, int total, const QString& imagePath)>& progress = {});
};

}  // namespace ppocr
