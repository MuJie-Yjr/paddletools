#include "app/controllers/DatasetController.h"

namespace ppocr {

DatasetController::DatasetController(QObject* parent)
    : QObject(parent) {}

QJsonObject DatasetController::defaultSplitConfig() {
    return DatasetService::defaultSplitConfig();
}

QList<PageInfo> DatasetController::listPages(const ProjectContext& context) {
    return DatasetService::listPages(context);
}

void DatasetController::saveProject(const ProjectContext& context) {
    DatasetService::saveProject(context);
}

SplitSummary DatasetController::splitSummary(const ProjectContext& context) {
    return DatasetService::splitSummary(context);
}

int DatasetController::reassignSplitsByOrder(ProjectContext& context, double trainRatio, double valRatio, double testRatio) {
    return DatasetService::reassignSplitsByOrder(context, trainRatio, valRatio, testRatio);
}

int DatasetController::reassignSplitsRandom(ProjectContext& context, double trainRatio, double valRatio, double testRatio, int seed) {
    return DatasetService::reassignSplitsRandom(context, trainRatio, valRatio, testRatio, seed);
}

void DatasetController::exportSplitReport(const ProjectContext& context, const QString& path) {
    DatasetService::exportSplitReport(context, path);
}

}  // namespace ppocr
