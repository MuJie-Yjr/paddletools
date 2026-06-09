#include "core/TrainingRunner.h"

#include "core/TrainingRunStore.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>
#include <stdexcept>

namespace ppocr {
namespace {

QString effectiveDevice(const TrainingOptions& options) {
    return options.device.trimmed().isEmpty() ? QStringLiteral("cpu") : options.device.trimmed();
}

QJsonObject commandReport(const PaddleCommand& command) {
    return {
        {QStringLiteral("program"), command.program},
        {QStringLiteral("arguments"), QJsonArray::fromStringList(command.arguments)},
        {QStringLiteral("working_directory"), command.workingDirectory},
        {QStringLiteral("display"), command.displayText()},
        {QStringLiteral("environment"), QJsonObject{
            {QStringLiteral("PYTHONIOENCODING"), command.environment.value(QStringLiteral("PYTHONIOENCODING"))},
            {QStringLiteral("PYTHONUTF8"), command.environment.value(QStringLiteral("PYTHONUTF8"))},
            {QStringLiteral("PADDLE_PDX_PADDLEOCR_PATH"), command.environment.value(QStringLiteral("PADDLE_PDX_PADDLEOCR_PATH"))},
            {QStringLiteral("PADDLE_PDX_PADDLECLAS_PATH"), command.environment.value(QStringLiteral("PADDLE_PDX_PADDLECLAS_PATH"))},
            {QStringLiteral("PYTHONPATH"), command.environment.value(QStringLiteral("PYTHONPATH"))},
        }},
    };
}

QString defaultSuccessLog(const TrainingTaskSpec& task) {
    if (task.kind == QStringLiteral("layout")) {
        return QStringLiteral("epoch: 1 loss: 0.1 mAP: 0.9\n");
    }
    if (task.kind == QStringLiteral("det")) {
        return QStringLiteral("epoch: 1 loss: 0.1 hmean: 0.9 precision: 0.9 recall: 0.9\n");
    }
    return QStringLiteral("epoch: 1 loss: 0.1 acc: 0.9\n");
}

void appendProcessOutput(QProcess* process, QString* buffer, bool echoOutput) {
    const QString stdoutText = QString::fromUtf8(process->readAllStandardOutput());
    const QString stderrText = QString::fromUtf8(process->readAllStandardError());
    if (!stdoutText.isEmpty()) {
        buffer->append(stdoutText);
        if (echoOutput) {
            QTextStream(stdout) << stdoutText;
        }
    }
    if (!stderrText.isEmpty()) {
        buffer->append(stderrText);
        if (echoOutput) {
            QTextStream(stderr) << stderrText;
        }
    }
}

QJsonObject runSummaryReport(
    const TrainingRunStart& start,
    const QString& status,
    int exitCode,
    const QString& errorSummary,
    const QJsonObject& metrics,
    const QJsonObject& finishedVersion) {
    return {
        {QStringLiteral("ok"), status == QStringLiteral("success")},
        {QStringLiteral("status"), status},
        {QStringLiteral("exit_code"), exitCode},
        {QStringLiteral("error_summary"), errorSummary},
        {QStringLiteral("task_key"), start.task.key},
        {QStringLiteral("task_title"), start.task.title},
        {QStringLiteral("task_kind"), start.task.kind},
        {QStringLiteral("run_id"), start.runId},
        {QStringLiteral("version_id"), start.versionId},
        {QStringLiteral("version_dir"), QDir::toNativeSeparators(start.versionDir)},
        {QStringLiteral("dataset_dir"), QDir::toNativeSeparators(start.preflight.datasetDir)},
        {QStringLiteral("sample_count"), start.preflight.sampleCount},
        {QStringLiteral("command"), commandReport(start.preflight.command)},
        {QStringLiteral("metrics"), metrics},
        {QStringLiteral("finished_version"), finishedVersion},
    };
}

}  // namespace

TrainingRunStart TrainingRunner::prepare(
    const QString& baseDir,
    const ProjectContext& context,
    const QString& taskKey,
    const TrainingOptions& options,
    const QString& versionId) {
    TrainingRunStart start;
    start.task = trainingTaskByKey(taskKey);
    start.device = effectiveDevice(options);
    start.versionId = versionId.isEmpty() ? TrainingRunStore::newVersionId(start.task.key) : versionId;
    start.preflight = TrainingPreflight::run(baseDir, context, start.task.key, options, start.versionId);
    start.versionDir = start.preflight.outputDir;
    start.errors = start.preflight.errors;
    if (!start.preflight.ok) {
        start.report = start.preflight.report;
        start.ok = false;
        return start;
    }
    if (!QDir().mkpath(start.versionDir)) {
        start.errors.append(QStringLiteral("Cannot create training output directory: %1").arg(start.versionDir));
        start.report = start.preflight.report;
        start.report.insert(QStringLiteral("ok"), false);
        start.report.insert(QStringLiteral("errors"), QJsonArray::fromStringList(start.errors));
        start.ok = false;
        return start;
    }

    start.runId = TrainingRunStore::startRun(
        context,
        start.task.key,
        start.task.title,
        start.preflight.command.displayText(),
        start.preflight.datasetDir,
        start.versionDir,
        start.device,
        start.preflight.sampleCount,
        start.versionId,
        start.versionDir);
    TrainingRunStore::startVersion(
        context,
        start.task.key,
        start.task.title,
        start.task.kind,
        start.versionId,
        start.preflight.command.displayText(),
        start.runId,
        start.preflight.datasetDir,
        start.versionDir,
        start.device,
        start.preflight.sampleCount);
    start.ok = true;
    start.report = {
        {QStringLiteral("ok"), true},
        {QStringLiteral("status"), QStringLiteral("prepared")},
        {QStringLiteral("task_key"), start.task.key},
        {QStringLiteral("task_title"), start.task.title},
        {QStringLiteral("task_kind"), start.task.kind},
        {QStringLiteral("run_id"), start.runId},
        {QStringLiteral("version_id"), start.versionId},
        {QStringLiteral("version_dir"), QDir::toNativeSeparators(start.versionDir)},
        {QStringLiteral("dataset_dir"), QDir::toNativeSeparators(start.preflight.datasetDir)},
        {QStringLiteral("sample_count"), start.preflight.sampleCount},
        {QStringLiteral("command"), commandReport(start.preflight.command)},
        {QStringLiteral("preflight"), start.preflight.report},
    };
    return start;
}

TrainingRunResult TrainingRunner::finish(
    const ProjectContext& context,
    const TrainingRunStart& start,
    const QString& status,
    int exitCode,
    const QString& errorSummary,
    const QString& logText) {
    TrainingRunResult result;
    result.status = status;
    result.exitCode = exitCode;
    result.errorSummary = errorSummary;
    result.logText = logText;
    result.metrics = parseMetrics(logText);
    if (!start.runId.isEmpty()) {
        TrainingRunStore::finishRun(context, start.runId, status, exitCode, errorSummary, result.metrics);
    }
    if (!start.versionId.isEmpty()) {
        result.finishedVersion = TrainingRunStore::finishVersion(
            context,
            start.task.key,
            start.task.kind,
            start.versionId,
            status,
            exitCode,
            errorSummary,
            result.metrics,
            start.task.bestWeightRel,
            start.task.inferDirRel);
    }
    result.ok = status == QStringLiteral("success");
    result.report = runSummaryReport(start, status, exitCode, errorSummary, result.metrics, result.finishedVersion);
    return result;
}

TrainingRunResult TrainingRunner::runBlocking(
    const QString& baseDir,
    const ProjectContext& context,
    const QString& taskKey,
    const TrainingOptions& options,
    int timeoutSeconds,
    bool echoOutput,
    const QString& versionId) {
    const TrainingRunStart start = prepare(baseDir, context, taskKey, options, versionId);
    if (!start.ok) {
        TrainingRunResult result;
        result.status = QStringLiteral("preflight_failed");
        result.exitCode = -1;
        result.errorSummary = start.errors.join(QStringLiteral("; "));
        result.report = start.report;
        result.report.insert(QStringLiteral("status"), result.status);
        return result;
    }

    QProcess process;
    process.setProgram(start.preflight.command.program);
    process.setArguments(start.preflight.command.arguments);
    process.setWorkingDirectory(start.preflight.command.workingDirectory);
    process.setProcessEnvironment(start.preflight.command.environment);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    QString logText;
    process.start();
    if (!process.waitForStarted(30000)) {
        const QString error = process.errorString();
        return finish(context, start, QStringLiteral("failed"), -1, error, logText);
    }

    const QDateTime deadline = timeoutSeconds > 0
        ? QDateTime::currentDateTimeUtc().addSecs(timeoutSeconds)
        : QDateTime();
    bool timedOut = false;
    while (true) {
        if (process.waitForFinished(1000)) {
            appendProcessOutput(&process, &logText, echoOutput);
            break;
        }
        appendProcessOutput(&process, &logText, echoOutput);
        if (timeoutSeconds > 0 && QDateTime::currentDateTimeUtc() >= deadline) {
            timedOut = true;
            process.terminate();
            if (!process.waitForFinished(3000)) {
                process.kill();
                process.waitForFinished(3000);
            }
            appendProcessOutput(&process, &logText, echoOutput);
            break;
        }
    }

    if (timedOut) {
        return finish(
            context,
            start,
            QStringLiteral("failed"),
            -2,
            QStringLiteral("training timed out after %1 seconds").arg(timeoutSeconds),
            logText);
    }
    const bool success = process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    return finish(
        context,
        start,
        success ? QStringLiteral("success") : QStringLiteral("failed"),
        process.exitCode(),
        success ? QString() : QStringLiteral("PaddleX exited with code %1").arg(process.exitCode()),
        logText);
}

TrainingRunResult TrainingRunner::simulateSuccess(
    const QString& baseDir,
    const ProjectContext& context,
    const QString& taskKey,
    const TrainingOptions& options,
    const QString& logText,
    const QString& versionId) {
    const TrainingRunStart start = prepare(baseDir, context, taskKey, options, versionId);
    if (!start.ok) {
        TrainingRunResult result;
        result.status = QStringLiteral("preflight_failed");
        result.exitCode = -1;
        result.errorSummary = start.errors.join(QStringLiteral("; "));
        result.report = start.report;
        result.report.insert(QStringLiteral("status"), result.status);
        return result;
    }
    const QString effectiveLog = logText.isEmpty() ? defaultSuccessLog(start.task) : logText;
    return finish(context, start, QStringLiteral("success"), 0, QString(), effectiveLog);
}

QJsonObject TrainingRunner::parseMetrics(const QString& logText) {
    QJsonObject metrics;
    const QRegularExpression pattern(
        QStringLiteral("[\\\"']?\\b(hmean|precision|recall|acc|accuracy|score|loss|train_loss|val_loss|lr|mAP|map)\\b[\\\"']?\\s*[:=]\\s*([-+]?\\d+(?:\\.\\d+)?(?:[eE][-+]?\\d+)?)"),
        QRegularExpression::CaseInsensitiveOption);
    auto matches = pattern.globalMatch(logText);
    while (matches.hasNext()) {
        const auto match = matches.next();
        QString key = match.captured(1);
        if (key.compare(QStringLiteral("mAP"), Qt::CaseInsensitive) != 0) {
            key = key.toLower();
        } else {
            key = QStringLiteral("mAP");
        }
        bool ok = false;
        const double value = match.captured(2).toDouble(&ok);
        if (ok) {
            metrics.insert(key, value);
        }
    }
    return metrics;
}

}  // namespace ppocr
