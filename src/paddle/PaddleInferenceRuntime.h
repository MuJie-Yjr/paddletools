#pragma once

#include <QJsonObject>
#include <QString>

namespace ppocr {

class PaddleInferenceRuntime {
public:
    static QString sdkRoot();
    static QString version();
    static bool sdkAvailable(QString* reason = nullptr);
    static bool gpuSupported();
    static QString resolveModelDir(const QString& modelDir);
    static bool modelDirLooksUsable(const QString& modelDir, QString* reason = nullptr);
    static QJsonObject smokeReport(const QString& modelDir);
};

}  // namespace ppocr
