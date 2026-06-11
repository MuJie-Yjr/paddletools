#include "application/EnvironmentService.h"

#include "core/EnvironmentReport.h"

namespace ppocr {

QString EnvironmentService::defaultBaseDir() {
    return EnvironmentReport::defaultBaseDir();
}

QString EnvironmentService::defaultPaddlexPython() {
    return EnvironmentReport::defaultPaddlexPython();
}

QString EnvironmentService::resolvedExecutable(const QString& executable) {
    return EnvironmentReport::resolvedExecutable(executable);
}

QJsonObject EnvironmentService::pathStatus(const QString& path) {
    return EnvironmentReport::pathStatus(path);
}

QJsonObject EnvironmentService::executableStatus(const QString& executable) {
    return EnvironmentReport::executableStatus(executable);
}

QJsonObject EnvironmentService::build(const EnvironmentReportRequest& request) {
    EnvironmentReportOptions options;
    options.baseDir = request.baseDir;
    options.pythonExe = request.pythonExe;
    options.applicationDir = request.applicationDir;
    return EnvironmentReport::build(options);
}

bool EnvironmentService::writeJson(const QString& path, const QJsonObject& report) {
    return EnvironmentReport::writeJson(path, report);
}

}  // namespace ppocr
