#pragma once

#include <QJsonObject>
#include <QString>

namespace ppocr {

struct EnvironmentReportRequest {
    QString baseDir;
    QString pythonExe;
    QString applicationDir;
};

class EnvironmentService {
public:
    static QString defaultBaseDir();
    static QString defaultPaddlexPython();
    static QString resolvedExecutable(const QString& executable);
    static QJsonObject pathStatus(const QString& path);
    static QJsonObject executableStatus(const QString& executable);
    static QJsonObject build(const EnvironmentReportRequest& request = {});
    static bool writeJson(const QString& path, const QJsonObject& report);
};

}  // namespace ppocr
