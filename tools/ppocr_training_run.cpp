#include "core/ProjectRepository.h"
#include "core/RuntimePaths.h"
#include "core/TrainingPreflight.h"
#include "core/TrainingRunner.h"
#include "core/TrainingTasks.h"
#include "tests/fakes/FakeTrainingRunner.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <exception>

namespace {

bool taskExists(const QString& key) {
    for (const auto& task : ppocr::trainingTasks()) {
        if (task.key == key) {
            return true;
        }
    }
    return false;
}

QJsonArray taskArray() {
    QJsonArray tasks;
    for (const auto& task : ppocr::trainingTasks()) {
        tasks.append(QJsonObject{
            {QStringLiteral("key"), task.key},
            {QStringLiteral("title"), task.title},
            {QStringLiteral("kind"), ppocr::toString(task.kind)},
            {QStringLiteral("export_task"), task.exportTask},
            {QStringLiteral("dataset_name"), task.datasetName},
            {QStringLiteral("config"), task.configRel},
            {QStringLiteral("epochs"), task.epochs},
            {QStringLiteral("batch_size"), task.batchSize},
            {QStringLiteral("learning_rate"), task.learningRate},
            {QStringLiteral("num_classes"), task.numClasses},
            {QStringLiteral("warmup_steps"), task.warmupSteps},
            {QStringLiteral("train_supported"), task.trainSupported},
            {QStringLiteral("note"), task.note},
        });
    }
    return tasks;
}

bool parsePositiveInt(const QString& text, const QString& name, int* value, QJsonObject* error) {
    if (text.isEmpty()) {
        return true;
    }
    bool ok = false;
    const int parsed = text.toInt(&ok);
    if (ok && parsed > 0) {
        *value = parsed;
        return true;
    }
    error->insert(QStringLiteral("ok"), false);
    error->insert(QStringLiteral("error"), QStringLiteral("%1 must be a positive integer").arg(name));
    return false;
}

bool parseNonNegativeInt(const QString& text, const QString& name, int* value, QJsonObject* error) {
    if (text.isEmpty()) {
        return true;
    }
    bool ok = false;
    const int parsed = text.toInt(&ok);
    if (ok && parsed >= 0) {
        *value = parsed;
        return true;
    }
    error->insert(QStringLiteral("ok"), false);
    error->insert(QStringLiteral("error"), QStringLiteral("%1 must be a non-negative integer").arg(name));
    return false;
}

bool parsePositiveDouble(const QString& text, const QString& name, double* value, QJsonObject* error) {
    if (text.isEmpty()) {
        return true;
    }
    bool ok = false;
    const double parsed = text.toDouble(&ok);
    if (ok && parsed > 0.0) {
        *value = parsed;
        return true;
    }
    error->insert(QStringLiteral("ok"), false);
    error->insert(QStringLiteral("error"), QStringLiteral("%1 must be a positive number").arg(name));
    return false;
}

void writeOptionalFile(const QString& path, const QByteArray& bytes) {
    if (path.isEmpty()) {
        return;
    }
    QFileInfo info(path);
    QDir().mkpath(info.absolutePath());
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(bytes);
    }
}

int writeJsonAndExit(const QJsonObject& object, int code, const QString& reportPath = QString()) {
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Indented);
    writeOptionalFile(reportPath, json);
    QTextStream(stdout) << QString::fromUtf8(json);
    return code;
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ppocr_training_run"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Run or simulate a PaddleX training task for a PPOCR workbench project."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption listTasksOpt(QStringLiteral("list-tasks"), QStringLiteral("Print supported training task specs as JSON."));
    const QCommandLineOption baseOpt(QStringLiteral("base-dir"), QStringLiteral("PPOCR paddletools root directory."), QStringLiteral("dir"));
    const QCommandLineOption projectOpt(QStringLiteral("project"), QStringLiteral("PPOCR project directory."), QStringLiteral("dir"));
    const QCommandLineOption taskOpt(QStringLiteral("task"), QStringLiteral("Training task key."), QStringLiteral("key"), QStringLiteral("det_v5_server"));
    const QCommandLineOption pythonOpt(QStringLiteral("python"), QStringLiteral("Python executable for PaddleX."), QStringLiteral("exe"));
    const QCommandLineOption deviceOpt(QStringLiteral("device"), QStringLiteral("Training device override."), QStringLiteral("device"), QStringLiteral("cpu"));
    const QCommandLineOption epochsOpt(QStringLiteral("epochs"), QStringLiteral("Training epochs/iters override."), QStringLiteral("n"));
    const QCommandLineOption batchOpt(QStringLiteral("batch-size"), QStringLiteral("Training batch size override."), QStringLiteral("n"));
    const QCommandLineOption lrOpt(QStringLiteral("learning-rate"), QStringLiteral("Training learning rate override."), QStringLiteral("value"));
    const QCommandLineOption allSamplesOpt(QStringLiteral("all-samples"), QStringLiteral("Export all samples instead of checked samples only."));
    const QCommandLineOption noValidationOpt(QStringLiteral("no-validation"), QStringLiteral("Skip validation before export."));
    const QCommandLineOption dryRunOpt(QStringLiteral("dry-run"), QStringLiteral("Run preflight and command generation without creating a training version."));
    const QCommandLineOption simulateOpt(QStringLiteral("simulate-success"), QStringLiteral("Create and finish a successful training version without launching PaddleX."));
    const QCommandLineOption timeoutOpt(QStringLiteral("timeout-seconds"), QStringLiteral("Kill real training after this many seconds; 0 disables timeout."), QStringLiteral("n"), QStringLiteral("0"));
    const QCommandLineOption versionOpt(QStringLiteral("version-id"), QStringLiteral("Use an explicit training version id."), QStringLiteral("id"));
    const QCommandLineOption echoOpt(QStringLiteral("echo-output"), QStringLiteral("Echo PaddleX output while training runs."));
    const QCommandLineOption reportOpt(QStringLiteral("report"), QStringLiteral("Write JSON report to this file."), QStringLiteral("file"));
    const QCommandLineOption logOpt(QStringLiteral("log"), QStringLiteral("Write captured training log to this file."), QStringLiteral("file"));

    parser.addOptions({
        listTasksOpt,
        baseOpt,
        projectOpt,
        taskOpt,
        pythonOpt,
        deviceOpt,
        epochsOpt,
        batchOpt,
        lrOpt,
        allSamplesOpt,
        noValidationOpt,
        dryRunOpt,
        simulateOpt,
        timeoutOpt,
        versionOpt,
        echoOpt,
        reportOpt,
        logOpt,
    });
    parser.process(app);

    const QString reportPath = parser.value(reportOpt);
    if (parser.isSet(listTasksOpt)) {
        return writeJsonAndExit(QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("tasks"), taskArray()}}, 0, reportPath);
    }

    const QString projectDir = parser.value(projectOpt);
    if (projectDir.isEmpty()) {
        return writeJsonAndExit(QJsonObject{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("--project <dir> is required unless --list-tasks is used")},
            {QStringLiteral("tasks"), taskArray()},
        }, 1, reportPath);
    }

    const QString taskKey = parser.value(taskOpt);
    if (!taskExists(taskKey)) {
        return writeJsonAndExit(QJsonObject{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("Unknown training task: %1").arg(taskKey)},
            {QStringLiteral("tasks"), taskArray()},
        }, 1, reportPath);
    }

    ppocr::TrainingOptions options;
    options.pythonExe = parser.value(pythonOpt).isEmpty() ? ppocr::RuntimePaths::defaultPaddlexPython() : parser.value(pythonOpt);
    options.device = parser.value(deviceOpt);
    options.checkedOnly = !parser.isSet(allSamplesOpt);
    options.requireValidation = !parser.isSet(noValidationOpt);

    int timeoutSeconds = 0;
    QJsonObject parseError;
    if (!parsePositiveInt(parser.value(epochsOpt), QStringLiteral("--epochs"), &options.epochs, &parseError)
        || !parsePositiveInt(parser.value(batchOpt), QStringLiteral("--batch-size"), &options.batchSize, &parseError)
        || !parsePositiveDouble(parser.value(lrOpt), QStringLiteral("--learning-rate"), &options.learningRate, &parseError)
        || !parseNonNegativeInt(parser.value(timeoutOpt), QStringLiteral("--timeout-seconds"), &timeoutSeconds, &parseError)) {
        return writeJsonAndExit(parseError, 1, reportPath);
    }

    const QString baseDir = QFileInfo(parser.value(baseOpt).isEmpty() ? ppocr::RuntimePaths::defaultBaseDir() : parser.value(baseOpt)).absoluteFilePath();
    const QString versionId = parser.value(versionOpt);
    try {
        const auto context = ppocr::ProjectRepository::openProject(projectDir);
        if (parser.isSet(dryRunOpt)) {
            auto preflight = ppocr::TrainingPreflight::run(baseDir, context, taskKey, options, versionId);
            preflight.report.insert(QStringLiteral("status"), QStringLiteral("dry_run"));
            return writeJsonAndExit(preflight.report, preflight.ok ? 0 : 2, reportPath);
        }

        ppocr::TrainingRunResult result = parser.isSet(simulateOpt)
            ? ppocr::tests::FakeTrainingRunner::simulateSuccess(baseDir, context, taskKey, options, QString(), versionId)
            : ppocr::TrainingRunner::runBlocking(baseDir, context, taskKey, options, timeoutSeconds, parser.isSet(echoOpt), versionId);
        writeOptionalFile(parser.value(logOpt), result.logText.toUtf8());
        return writeJsonAndExit(result.report, result.ok ? 0 : 2, reportPath);
    } catch (const std::exception& exc) {
        return writeJsonAndExit(QJsonObject{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QString::fromUtf8(exc.what())},
            {QStringLiteral("project_dir"), QDir::toNativeSeparators(projectDir)},
            {QStringLiteral("base_dir"), QDir::toNativeSeparators(baseDir)},
        }, 1, reportPath);
    }
}
