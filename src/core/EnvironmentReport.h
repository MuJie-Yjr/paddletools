#pragma once

#include <QJsonObject>
#include <QString>

namespace ppocr {

struct EnvironmentReportOptions {
    QString baseDir;
    QString pythonExe;
    QString applicationDir;
};

class EnvironmentReport {
public:
    static QString defaultBaseDir();
    static QString defaultPaddlexPython();
    static QString resolvedExecutable(const QString& executable);
    static QJsonObject pathStatus(const QString& path);
    static QJsonObject executableStatus(const QString& executable);
    static QJsonObject build(const EnvironmentReportOptions& options = {});
    static bool writeJson(const QString& path, const QJsonObject& report);
};

}  // namespace ppocr
