#pragma once

#include <QList>
#include <QJsonObject>
#include <QPair>
#include <QString>
#include <functional>

namespace ppocr {

struct PaddleDocLayoutModelConfig {
    QString modelName = QStringLiteral("PP-DocLayout-S");
    QString modelDir;
    QString device = QStringLiteral("cpu");
    QString precision = QStringLiteral("fp32");
    bool enableMkldnn = true;
    int mkldnnCacheCapacity = 10;
    int cpuThreads = 8;
    double threshold = 0.5;
};

class PaddleDocLayoutEngine {
public:
    static QString projectRoot();
    static PaddleDocLayoutModelConfig defaultConfig(const QString& baseDir = QString());
    static bool modelDirLooksUsable(const QString& modelDir, QString* reason = nullptr);
    static QJsonObject configReport(const PaddleDocLayoutModelConfig& config);
    static QJsonObject predictImage(
        const QString& imagePath,
        const QString& outputDir,
        const PaddleDocLayoutModelConfig& config = defaultConfig(),
        bool saveVisualization = true);
    static QList<QJsonObject> predictImages(
        const QList<QPair<QString, QString>>& imageJobs,
        const PaddleDocLayoutModelConfig& config = defaultConfig(),
        bool saveVisualization = false,
        const std::function<void(int current, int total, const QString& imagePath)>& progress = {});
    static QJsonObject predictPath(
        const QString& inputPath,
        const QString& outputDir,
        const PaddleDocLayoutModelConfig& config = defaultConfig(),
        bool saveVisualization = true);
};

}  // namespace ppocr
