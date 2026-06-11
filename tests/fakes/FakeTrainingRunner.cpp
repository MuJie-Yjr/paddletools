#include "FakeTrainingRunner.h"

namespace ppocr::tests {
namespace {

QString defaultSuccessLog(const TrainingTaskSpec& task) {
    if (task.kind == TrainingTaskKind::Layout) {
        return QStringLiteral("epoch: 1 loss: 0.1 mAP: 0.9\n");
    }
    if (task.kind == TrainingTaskKind::OcrDet) {
        return QStringLiteral("epoch: 1 loss: 0.1 hmean: 0.9 precision: 0.9 recall: 0.9\n");
    }
    return QStringLiteral("epoch: 1 loss: 0.1 acc: 0.9\n");
}

}  // namespace

TrainingRunResult FakeTrainingRunner::simulateSuccess(
    const QString& baseDir,
    const ProjectContext& context,
    const QString& taskKey,
    const TrainingOptions& options,
    const QString& logText,
    const QString& versionId) {
    const TrainingRunStart start = TrainingRunner::prepare(baseDir, context, taskKey, options, versionId);
    if (!start.ok) {
        TrainingRunResult result;
        result.status = RunStatus::Failed;
        result.exitCode = -1;
        result.errorSummary = start.errors.join(QStringLiteral("; "));
        result.report = start.report;
        result.report.insert(QStringLiteral("status"), QStringLiteral("preflight_failed"));
        return result;
    }
    const QString effectiveLog = logText.isEmpty() ? defaultSuccessLog(start.task) : logText;
    return TrainingRunner::finish(context, start, RunStatus::Finished, 0, QString(), effectiveLog);
}

}  // namespace ppocr::tests
