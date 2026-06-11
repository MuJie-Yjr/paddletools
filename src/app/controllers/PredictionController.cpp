#include "app/controllers/PredictionController.h"

#include "application/PredictionService.h"

namespace ppocr {

PredictionController::PredictionController(QObject* parent)
    : QObject(parent) {}

PredictionController::~PredictionController() = default;

bool PredictionController::isRunning() const {
    return process_ != nullptr;
}

bool PredictionController::startPrediction(const PredictionRunRequest& request, QString* errorMessage) {
    if (process_) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("A prediction process is already running.");
        }
        return false;
    }
    if (!PredictionService::validateRequest(request, errorMessage)) {
        return false;
    }

    process_ = new QProcess(this);
    process_->setProgram(request.program);
    process_->setArguments(request.arguments);
    process_->setWorkingDirectory(request.workingDirectory);
    process_->setProcessChannelMode(QProcess::MergedChannels);
    connect(process_, &QProcess::readyReadStandardOutput, this, &PredictionController::handleOutput);
    connect(process_, &QProcess::finished, this, &PredictionController::handleFinished);
    connect(process_, &QProcess::errorOccurred, this, &PredictionController::handleError);

    timer_.restart();
    emit runningChanged(true);
    process_->start();
    return true;
}

void PredictionController::stopPrediction() {
    if (!process_) {
        return;
    }
    emit outputTextReady(QStringLiteral("\nStopping prediction...\n"));
    process_->terminate();
    if (!process_->waitForFinished(3000)) {
        process_->kill();
    }
}

void PredictionController::handleOutput() {
    auto* source = qobject_cast<QProcess*>(sender());
    if (!source) {
        source = process_;
    }
    if (!source) {
        return;
    }
    emit outputTextReady(QString::fromUtf8(source->readAllStandardOutput()));
}

void PredictionController::handleFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    handleOutput();
    const bool ok = exitStatus == QProcess::NormalExit && exitCode == 0;
    const qint64 elapsedMs = timer_.isValid() ? timer_.elapsed() : -1;
    cleanupProcess();
    emit predictionFinished(ok, exitCode, elapsedMs);
    emit runningChanged(false);
}

void PredictionController::handleError(QProcess::ProcessError error) {
    if (!process_ || error != QProcess::FailedToStart) {
        return;
    }
    const QString errorMessage = process_->errorString();
    cleanupProcess();
    emit predictionStartFailed(errorMessage);
    emit runningChanged(false);
}

void PredictionController::cleanupProcess() {
    if (!process_) {
        return;
    }
    process_->deleteLater();
    process_ = nullptr;
}

}  // namespace ppocr
