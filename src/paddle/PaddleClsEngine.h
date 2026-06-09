#pragma once

#include <QList>
#include <QJsonObject>
#include <QPair>
#include <QString>
#include <functional>

namespace ppocr {

struct PaddleClsModelConfig {
    QString task = QStringLiteral("doc_orientation");
    QString modelName = QStringLiteral("PP-LCNet_x1_0_doc_ori");
    QString modelDir;
    QString device = QStringLiteral("cpu");
    QString precision = QStringLiteral("fp32");
    bool enableMkldnn = true;
    int mkldnnCacheCapacity = 10;
    int cpuThreads = 8;
};

class PaddleClsEngine {
public:
    static PaddleClsModelConfig defaultConfig(const QString& task, const QString& baseDir = QString());
    static QJsonObject configReport(const PaddleClsModelConfig& config);
    static QJsonObject predictImage(
        const QString& imagePath,
        const QString& outputDir,
        const PaddleClsModelConfig& config,
        bool saveVisualization = true);
    static QList<QJsonObject> predictImages(
        const QList<QPair<QString, QString>>& imageJobs,
        const PaddleClsModelConfig& config,
        bool saveVisualization = false,
        const std::function<void(int current, int total, const QString& imagePath)>& progress = {});
};

}  // namespace ppocr
