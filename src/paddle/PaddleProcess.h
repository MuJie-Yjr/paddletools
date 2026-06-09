#pragma once

#include <QProcess>
#include <QStringList>

namespace ppocr {

struct PaddleCommand {
    QString program;
    QStringList arguments;
    QString workingDirectory;
    QProcessEnvironment environment;

    QString displayText() const;
};

class PaddleProcess {
public:
    static PaddleCommand trainingCommand(
        const QString& baseDir,
        const QString& pythonExe,
        const QString& configPath,
        const QStringList& overrides);
    static QProcessEnvironment paddleEnvironment(const QString& baseDir);
};

}  // namespace ppocr
