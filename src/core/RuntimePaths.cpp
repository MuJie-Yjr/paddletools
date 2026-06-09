#include "core/RuntimePaths.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace ppocr {

QString RuntimePaths::defaultBaseDir() {
    const QString configured = qEnvironmentVariable("PPOCR_BASE_DIR").trimmed();
    if (!configured.isEmpty()) {
        return configured;
    }
#ifdef PPOCR_PROJECT_ROOT
    return QStringLiteral(PPOCR_PROJECT_ROOT);
#else
    return QDir::currentPath();
#endif
}

QString RuntimePaths::defaultPaddlexPython() {
    const QString configured = qEnvironmentVariable("PPOCR_PADDLEX_PYTHON").trimmed();
    if (!configured.isEmpty()) {
        return configured;
    }
    const QString preferred = QStringLiteral("D:/IDE/anconda/envs/paddleX-py312/python.exe");
    return QFileInfo::exists(preferred) ? preferred : QStringLiteral("python");
}

QString RuntimePaths::resolvedExecutable(const QString& executable) {
    const QFileInfo info(executable);
    if (info.isAbsolute() || executable.contains('/') || executable.contains('\\')) {
        return info.exists() ? info.absoluteFilePath() : QString();
    }
    return QStandardPaths::findExecutable(executable);
}

QJsonObject RuntimePaths::pathStatus(const QString& path) {
    const QFileInfo info(path);
    const QString absolute = info.absoluteFilePath();
    return {
        {QStringLiteral("path"), QDir::toNativeSeparators(path)},
        {QStringLiteral("absolute"), QDir::toNativeSeparators(absolute)},
        {QStringLiteral("absolute_path"), QDir::toNativeSeparators(absolute)},
        {QStringLiteral("exists"), info.exists()},
        {QStringLiteral("is_file"), info.isFile()},
        {QStringLiteral("is_dir"), info.isDir()},
    };
}

QJsonObject RuntimePaths::executableStatus(const QString& executable) {
    QJsonObject report = pathStatus(executable);
    const QString resolved = resolvedExecutable(executable);
    report.insert(QStringLiteral("resolved_executable"), QDir::toNativeSeparators(resolved));
    report.insert(QStringLiteral("executable_found"), !resolved.isEmpty());
    return report;
}

}  // namespace ppocr
