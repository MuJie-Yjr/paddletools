#include "paddle/PaddleProcess.h"

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>

namespace ppocr {

static QString quote(const QString& value) {
    if (!value.contains(' ') && !value.contains('\t')) {
        return value;
    }
    QString escaped = value;
    escaped.replace("\"", "\\\"");
    return '"' + escaped + '"';
}

QString PaddleCommand::displayText() const {
    QStringList parts;
    parts.append(quote(program));
    for (const auto& arg : arguments) {
        parts.append(quote(arg));
    }
    return parts.join(' ');
}

PaddleCommand PaddleProcess::trainingCommand(
    const QString& baseDir,
    const QString& pythonExe,
    const QString& configPath,
    const QStringList& overrides) {
    PaddleCommand command;
    command.program = pythonExe;
    command.workingDirectory = QDir(baseDir).filePath("PaddleX");
    command.environment = paddleEnvironment(baseDir);
    command.arguments = {"main.py", "-c", configPath};
    for (const auto& item : overrides) {
        command.arguments.append("-o");
        command.arguments.append(item);
    }
    return command;
}

QProcessEnvironment PaddleProcess::paddleEnvironment(const QString& baseDir) {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONIOENCODING", "utf-8");
    env.insert("PYTHONUTF8", "1");
    const QString paddleOcr = QDir(baseDir).filePath("PaddleOCR");
    env.insert("PADDLE_PDX_PADDLEOCR_PATH", paddleOcr);
    const QString paddleClas = QDir(baseDir).filePath("PaddleClas");
    if (QFileInfo(paddleClas).isDir()) {
        env.insert("PADDLE_PDX_PADDLECLAS_PATH", paddleClas);
    }
    QStringList pythonPath = {QDir(baseDir).filePath("PaddleX"), paddleOcr};
    const QString effectivePaddleClas = env.value("PADDLE_PDX_PADDLECLAS_PATH");
    if (!effectivePaddleClas.isEmpty()) {
        const QString paddleClasShim = QDir(baseDir).filePath("PaddleX/paddlex_local_shims");
        if (QFileInfo(QDir(paddleClasShim).filePath("paddleclas/__init__.py")).isFile()) {
            pythonPath.append(paddleClasShim);
        }
        pythonPath.append(effectivePaddleClas);
    }
    const QString current = env.value("PYTHONPATH");
    if (!current.isEmpty()) {
        pythonPath.append(current);
    }
    env.insert("PYTHONPATH", pythonPath.join(QDir::listSeparator()));
    return env;
}

}  // namespace ppocr
