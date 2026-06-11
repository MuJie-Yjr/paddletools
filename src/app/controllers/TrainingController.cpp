#include "app/controllers/TrainingController.h"

#include <QProcess>
#include <stdexcept>
#include <utility>

namespace ppocr {

TrainingController::TrainingController(QString baseDir, QObject* parent)
    : QObject(parent), baseDir_(baseDir), service_(std::move(baseDir)) {}

TrainingController::~TrainingController() = default;

bool TrainingController::isRunning() const {
    return process_ != nullptr;
}

PaddleCommand TrainingController::buildCommand(
    const ProjectContext* context,
    const QString& taskKey,
    const QString& outputDir,
    const TrainingOptions& options) const {
    return service_.buildCommand(context, taskKey, outputDir, options);
}

TrainingPreflightResult TrainingController::runPreflight(
    const ProjectContext& context,
    const QString& taskKey,
    const TrainingOptions& options,
    const QString& previewVersionId) const {
    return service_.runPreflight(context, taskKey, options, previewVersionId);
}

QJsonObject TrainingController::parseMetrics(const QString& logText) const {
    return service_.parseMetrics(logText);
}

bool TrainingController::startTraining(
    const ProjectContext& context,
    const QString& taskKey,
    const TrainingOptions& options,
    QString* errorMessage) {
    if (process_) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("A training process is already running.");
        }
        return false;
    }

    try {
        activeContext_ = context;
        activeStart_ = std::make_unique<TrainingRunStart>(
            service_.prepare(activeContext_, taskKey, options));
        if (!activeStart_->ok) {
            throw std::runtime_error(activeStart_->errors.join(QStringLiteral("; ")).toStdString());
        }

        const auto command = activeStart_->preflight.command;
        logBuffer_.clear();
        stopRequested_ = false;
        finalized_ = false;
        process_ = new QProcess(this);
        process_->setProgram(command.program);
        process_->setArguments(command.arguments);
        process_->setWorkingDirectory(command.workingDirectory);
        process_->setProcessEnvironment(command.environment);
        process_->setProcessChannelMode(QProcess::SeparateChannels);
        connect(process_, &QProcess::readyReadStandardOutput, this, &TrainingController::handleOutput);
        connect(process_, &QProcess::readyReadStandardError, this, &TrainingController::handleOutput);
        connect(process_, &QProcess::finished, this, &TrainingController::handleFinished);
        connect(process_, &QProcess::errorOccurred, this, &TrainingController::handleError);

        emit trainingPrepared(
            activeStart_->task.key,
            toString(activeStart_->task.kind),
            activeStart_->runId,
            activeStart_->versionId,
            activeStart_->preflight.datasetDir,
            activeStart_->versionDir,
            command.displayText());
        emit runningChanged(true);
        process_->start();
        return true;
    } catch (const std::exception& exc) {
        cleanupProcess();
        activeStart_.reset();
        logBuffer_.clear();
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(exc.what());
        }
        emit runningChanged(false);
        return false;
    }
}

void TrainingController::stopTraining() {
    if (!process_) {
        return;
    }
    stopRequested_ = true;
    emit logTextReady(QStringLiteral("\nStopping training...\n"));
    process_->terminate();
    if (!process_->waitForFinished(3000)) {
        process_->kill();
    }
}

void TrainingController::handleOutput() {
    auto* source = qobject_cast<QProcess*>(sender());
    if (!source) {
        source = process_;
    }
    if (!source) {
        return;
    }
    const QString stdoutText = QString::fromUtf8(source->readAllStandardOutput());
    const QString stderrText = QString::fromUtf8(source->readAllStandardError());
    for (const auto& text : {stdoutText, stderrText}) {
        if (text.isEmpty()) {
            continue;
        }
        logBuffer_.append(text);
        emit logTextReady(text);
        emit metricsTextReady(text);
    }
}

void TrainingController::handleFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    handleOutput();
    const bool ok = exitStatus == QProcess::NormalExit && exitCode == 0;
    const RunStatus status = stopRequested_ ? RunStatus::Cancelled : (ok ? RunStatus::Finished : RunStatus::Failed);
    const QString errorSummary = status == RunStatus::Finished
        ? QString()
        : (stopRequested_ ? QStringLiteral("user stopped training") : QStringLiteral("PaddleX exited with code %1").arg(exitCode));
    finalize(status, exitCode, errorSummary);
}

void TrainingController::handleError(QProcess::ProcessError error) {
    if (!process_ || error != QProcess::FailedToStart) {
        return;
    }
    const QString errorSummary = process_->errorString();
    emit logTextReady(QStringLiteral("\nFailed to start PaddleX: %1\n").arg(errorSummary));
    finalize(RunStatus::Failed, -1, errorSummary);
}

void TrainingController::finalize(RunStatus status, int exitCode, const QString& errorSummary) {
    if (finalized_) {
        return;
    }
    finalized_ = true;

    TrainingRunResult result;
    QString versionId;
    if (activeStart_) {
        versionId = activeStart_->versionId;
        result = service_.finish(activeContext_, *activeStart_, status, exitCode, errorSummary, logBuffer_);
    }

    cleanupProcess();
    activeStart_.reset();
    stopRequested_ = false;
    emit trainingFinished(toString(status), exitCode, errorSummary, result.metrics, result.finishedVersion, versionId);
    emit runningChanged(false);
}

void TrainingController::cleanupProcess() {
    if (!process_) {
        return;
    }
    process_->deleteLater();
    process_ = nullptr;
}

}  // namespace ppocr
