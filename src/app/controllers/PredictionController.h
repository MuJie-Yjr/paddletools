#pragma once

#include "domain/PredictionJob.h"

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QString>

namespace ppocr {

class PredictionController : public QObject {
    Q_OBJECT

public:
    explicit PredictionController(QObject* parent = nullptr);
    ~PredictionController() override;

    bool isRunning() const;
    bool startPrediction(const PredictionRunRequest& request, QString* errorMessage = nullptr);

public slots:
    void stopPrediction();

signals:
    void outputTextReady(const QString& text);
    void runningChanged(bool running);
    void predictionFinished(bool processOk, int exitCode, qint64 elapsedMs);
    void predictionStartFailed(const QString& errorMessage);

private slots:
    void handleOutput();
    void handleFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleError(QProcess::ProcessError error);

private:
    void cleanupProcess();

    QProcess* process_ = nullptr;
    QElapsedTimer timer_;
};

}  // namespace ppocr
