#include "core/TrainingRunner.h"

#include "core/TrainingRunStore.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
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
    RunStatus status,
    int exitCode,
    const QString& errorSummary,
    const QJsonObject& metrics,
    const QJsonObject& finishedVersion) {
    const QString statusText = toString(status);
    return {
        {QStringLiteral("ok"), status == RunStatus::Finished},
        {QStringLiteral("status"), statusText},
        {QStringLiteral("exit_code"), exitCode},
        {QStringLiteral("error_summary"), errorSummary},
        {QStringLiteral("task_key"), start.task.key},
        {QStringLiteral("task_title"), start.task.title},
        {QStringLiteral("task_kind"), toString(start.task.kind)},
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

void writeBytes(const QString& path, const QByteArray& bytes, QIODevice::OpenMode mode = QIODevice::WriteOnly | QIODevice::Truncate) {
    if (path.isEmpty()) {
        return;
    }
    QFileInfo info(path);
    QDir().mkpath(info.absolutePath());
    QFile file(path);
    if (file.open(mode)) {
        file.write(bytes);
    }
}

void writeJsonObject(const QString& path, const QJsonObject& object) {
    writeBytes(path, QJsonDocument(object).toJson(QJsonDocument::Indented));
}

QJsonObject trainingOptionsReport(const TrainingOptions& options) {
    return QJsonObject{
        {QStringLiteral("python"), options.pythonExe},
        {QStringLiteral("device"), options.device},
        {QStringLiteral("epochs"), options.epochs},
        {QStringLiteral("batch_size"), options.batchSize},
        {QStringLiteral("learning_rate"), options.learningRate},
        {QStringLiteral("num_classes"), options.numClasses},
        {QStringLiteral("warmup_steps"), options.warmupSteps},
        {QStringLiteral("resume_path"), options.resumePath},
        {QStringLiteral("checked_only"), options.checkedOnly},
        {QStringLiteral("require_validation"), options.requireValidation},
    };
}

void writePrepareArtifacts(
    const QString& baseDir,
    const ProjectContext& context,
    const TrainingRunStart& start,
    const TrainingOptions& options) {
    const QDir versionDir(start.versionDir);
    writeJsonObject(versionDir.filePath(QStringLiteral("config.input.json")), QJsonObject{
        {QStringLiteral("base_dir"), QDir::toNativeSeparators(baseDir)},
        {QStringLiteral("project_dir"), QDir::toNativeSeparators(context.root.absolutePath())},
        {QStringLiteral("task_key"), start.task.key},
        {QStringLiteral("task_title"), start.task.title},
        {QStringLiteral("task_kind"), toString(start.task.kind)},
        {QStringLiteral("config_rel"), start.task.configRel},
        {QStringLiteral("best_weight_rel"), start.task.bestWeightRel},
        {QStringLiteral("infer_dir_rel"), start.task.inferDirRel},
        {QStringLiteral("options"), trainingOptionsReport(options)},
    });
    writeJsonObject(versionDir.filePath(QStringLiteral("dataset_snapshot.json")), QJsonObject{
        {QStringLiteral("dataset_dir"), QDir::toNativeSeparators(start.preflight.datasetDir)},
        {QStringLiteral("sample_count"), start.preflight.sampleCount},
        {QStringLiteral("sample_counts"), QJsonObject{
            {QStringLiteral("train"), start.preflight.trainSampleCount},
            {QStringLiteral("val"), start.preflight.valSampleCount},
            {QStringLiteral("total"), start.preflight.sampleCount},
        }},
        {QStringLiteral("export"), start.preflight.report.value(QStringLiteral("export")).toObject()},
    });
    writeJsonObject(versionDir.filePath(QStringLiteral("command.json")), commandReport(start.preflight.command));
    writeJsonObject(versionDir.filePath(QStringLiteral("environment.json")), QJsonObject{
        {QStringLiteral("device"), start.device},
        {QStringLiteral("process_environment"), commandReport(start.preflight.command).value(QStringLiteral("environment")).toObject()},
        {QStringLiteral("working_directory"), QDir::toNativeSeparators(start.preflight.command.workingDirectory)},
    });

    QString resolvedConfig;
    QTextStream stream(&resolvedConfig);
    stream << "# Generated command preview for PPOCR Workbench\n";
    stream << "task_key: " << start.task.key << '\n';
    stream << "config: " << QDir::toNativeSeparators(QDir(baseDir).filePath(start.task.configRel)) << '\n';
    stream << "command: " << start.preflight.command.displayText() << '\n';
    stream << "overrides:\n";
    for (const auto& argument : start.preflight.command.arguments) {
        if (argument.contains('=')) {
            stream << "  - " << argument << '\n';
        }
    }
    writeBytes(versionDir.filePath(QStringLiteral("config.resolved.yaml")), resolvedConfig.toUtf8());
}

QString normalizedMetricKey(const QString& key) {
    const QString normalized = key.trimmed().toLower();
    if (normalized == QStringLiteral("map")) {
        return QStringLiteral("mAP");
    }
    if (normalized == QStringLiteral("hmean")
        || normalized == QStringLiteral("precision")
        || normalized == QStringLiteral("recall")
        || normalized == QStringLiteral("acc")
        || normalized == QStringLiteral("accuracy")
        || normalized == QStringLiteral("score")
        || normalized == QStringLiteral("loss")
        || normalized == QStringLiteral("train_loss")
        || normalized == QStringLiteral("val_loss")
        || normalized == QStringLiteral("lr")
        || normalized == QStringLiteral("epoch")) {
        return normalized;
    }
    return {};
}

bool numericMetricValue(const QJsonValue& value, double* number) {
    if (value.isDouble()) {
        *number = value.toDouble();
        return true;
    }
    if (value.isString()) {
        bool ok = false;
        const double parsed = value.toString().toDouble(&ok);
        if (ok) {
            *number = parsed;
            return true;
        }
    }
    return false;
}

void collectMetricsFromJsonValue(const QJsonValue& value, QJsonObject* metrics);

void collectMetricsFromJsonObject(const QJsonObject& object, QJsonObject* metrics) {
    for (auto it = object.begin(); it != object.end(); ++it) {
        const QString key = normalizedMetricKey(it.key());
        double number = 0.0;
        if (!key.isEmpty() && numericMetricValue(it.value(), &number)) {
            metrics->insert(key, number);
            continue;
        }
        collectMetricsFromJsonValue(it.value(), metrics);
    }
}

void collectMetricsFromJsonValue(const QJsonValue& value, QJsonObject* metrics) {
    if (value.isObject()) {
        collectMetricsFromJsonObject(value.toObject(), metrics);
    } else if (value.isArray()) {
        for (const auto& item : value.toArray()) {
            collectMetricsFromJsonValue(item, metrics);
        }
    }
}

QJsonObject metricsFromJsonDocument(const QJsonDocument& doc) {
    QJsonObject metrics;
    if (doc.isObject()) {
        collectMetricsFromJsonObject(doc.object(), &metrics);
    } else if (doc.isArray()) {
        for (const auto& item : doc.array()) {
            collectMetricsFromJsonValue(item, &metrics);
        }
    }
    return metrics;
}

QJsonObject metricsFromJsonFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        return {};
    }
    return metricsFromJsonDocument(doc);
}

QJsonObject metricsFromJsonlFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    QJsonObject metrics;
    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        QJsonParseError error;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &error);
        if (error.error == QJsonParseError::NoError) {
            const QJsonObject lineMetrics = metricsFromJsonDocument(doc);
            for (auto it = lineMetrics.begin(); it != lineMetrics.end(); ++it) {
                metrics.insert(it.key(), it.value());
            }
        }
    }
    return metrics;
}

QJsonObject metricsFromCsvFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    QString headerLine;
    QString lastLine;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (headerLine.isEmpty()) {
            headerLine = line;
        } else {
            lastLine = line;
        }
    }
    if (headerLine.isEmpty() || lastLine.isEmpty()) {
        return {};
    }
    const QStringList headers = headerLine.split(',');
    const QStringList values = lastLine.split(',');
    QJsonObject metrics;
    const int count = qMin(headers.size(), values.size());
    for (int i = 0; i < count; ++i) {
        const QString key = normalizedMetricKey(headers.at(i));
        if (key.isEmpty()) {
            continue;
        }
        bool ok = false;
        const double number = values.at(i).trimmed().toDouble(&ok);
        if (ok) {
            metrics.insert(key, number);
        }
    }
    return metrics;
}

QStringList structuredMetricFiles(const QString& versionDir) {
    QStringList paths;
    const QDir dir(versionDir);
    for (const auto& name : {
             QStringLiteral("metrics.jsonl"),
             QStringLiteral("result.json"),
             QStringLiteral("metrics.json"),
             QStringLiteral("train_result.json"),
             QStringLiteral("metric.csv"),
             QStringLiteral("metrics.csv"),
         }) {
        const QString path = dir.filePath(name);
        if (QFileInfo(path).isFile()) {
            paths.append(path);
        }
    }

    QSet<QString> seen;
    for (const auto& path : paths) {
        seen.insert(path);
    }
    QDirIterator it(
        versionDir,
        {QStringLiteral("*.jsonl"), QStringLiteral("*.json"), QStringLiteral("*.csv"), QStringLiteral("*.yaml"), QStringLiteral("*.yml")},
        QDir::Files,
        QDirIterator::Subdirectories);
    int scanned = 0;
    while (it.hasNext() && scanned < 128) {
        const QString path = it.next();
        ++scanned;
        if (seen.contains(path) || QFileInfo(path).size() > 5 * 1024 * 1024) {
            continue;
        }
        seen.insert(path);
        paths.append(path);
    }
    return paths;
}

QJsonObject parseMetricsFromArtifacts(const QString& versionDir, const QString& logText) {
    if (!versionDir.isEmpty() && QFileInfo(versionDir).isDir()) {
        for (const auto& path : structuredMetricFiles(versionDir)) {
            const QString suffix = QFileInfo(path).suffix().toLower();
            QJsonObject metrics;
            if (suffix == QStringLiteral("jsonl")) {
                metrics = metricsFromJsonlFile(path);
            } else if (suffix == QStringLiteral("json")) {
                metrics = metricsFromJsonFile(path);
            } else if (suffix == QStringLiteral("csv")) {
                metrics = metricsFromCsvFile(path);
            } else if (suffix == QStringLiteral("yaml") || suffix == QStringLiteral("yml")) {
                QFile file(path);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    metrics = TrainingRunner::parseMetrics(QString::fromUtf8(file.readAll()));
                }
            }
            if (!metrics.isEmpty()) {
                return metrics;
            }
        }
    }
    return TrainingRunner::parseMetrics(logText);
}

void writeFinishArtifacts(const TrainingRunStart& start, const TrainingRunResult& result) {
    const QDir versionDir(start.versionDir);
    writeBytes(versionDir.filePath(QStringLiteral("train.log")), result.logText.toUtf8());
    if (!result.metrics.isEmpty()) {
        QJsonObject line = result.metrics;
        line.insert(QStringLiteral("source"), QStringLiteral("runner"));
        writeBytes(
            versionDir.filePath(QStringLiteral("metrics.jsonl")),
            QJsonDocument(line).toJson(QJsonDocument::Compact) + QByteArray("\n"),
            QIODevice::WriteOnly | QIODevice::Append);
    }
    writeJsonObject(versionDir.filePath(QStringLiteral("result.json")), result.report);
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
    writePrepareArtifacts(baseDir, context, start, options);
    start.ok = true;
    start.report = {
        {QStringLiteral("ok"), true},
        {QStringLiteral("status"), QStringLiteral("prepared")},
        {QStringLiteral("task_key"), start.task.key},
        {QStringLiteral("task_title"), start.task.title},
        {QStringLiteral("task_kind"), toString(start.task.kind)},
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
    RunStatus status,
    int exitCode,
    const QString& errorSummary,
    const QString& logText) {
    TrainingRunResult result;
    result.status = status;
    result.exitCode = exitCode;
    result.errorSummary = errorSummary;
    result.logText = logText;
    result.metrics = parseMetricsFromArtifacts(start.versionDir, logText);
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
    result.ok = status == RunStatus::Finished;
    result.report = runSummaryReport(start, status, exitCode, errorSummary, result.metrics, result.finishedVersion);
    writeFinishArtifacts(start, result);
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
        result.status = RunStatus::Failed;
        result.exitCode = -1;
        result.errorSummary = start.errors.join(QStringLiteral("; "));
        result.report = start.report;
        result.report.insert(QStringLiteral("status"), QStringLiteral("preflight_failed"));
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
        return finish(context, start, RunStatus::Failed, -1, error, logText);
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
            RunStatus::Failed,
            -2,
            QStringLiteral("training timed out after %1 seconds").arg(timeoutSeconds),
            logText);
    }
    const bool success = process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    return finish(
        context,
        start,
        success ? RunStatus::Finished : RunStatus::Failed,
        process.exitCode(),
        success ? QString() : QStringLiteral("PaddleX exited with code %1").arg(process.exitCode()),
        logText);
}

QJsonObject TrainingRunner::parseMetrics(const QString& logText) {
    QJsonObject metrics;
    const QRegularExpression pattern(
        QStringLiteral("[\\\"']?\\b(epoch|hmean|precision|recall|acc|accuracy|score|loss|train_loss|val_loss|lr|mAP|map)\\b[\\\"']?\\s*[:=]\\s*([-+]?\\d+(?:\\.\\d+)?(?:[eE][-+]?\\d+)?)"),
        QRegularExpression::CaseInsensitiveOption);
    auto matches = pattern.globalMatch(logText);
    while (matches.hasNext()) {
        const auto match = matches.next();
        const QString key = normalizedMetricKey(match.captured(1));
        bool ok = false;
        const double value = match.captured(2).toDouble(&ok);
        if (ok && !key.isEmpty()) {
            metrics.insert(key, value);
        }
    }
    return metrics;
}

}  // namespace ppocr
