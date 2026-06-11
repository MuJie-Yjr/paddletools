#pragma once

#include "core/DatasetExportPlanner.h"

#include <QJsonObject>
#include <QString>

namespace ppocr {

struct DatasetExportResult {
    bool ok = false;
    QString error;
    QString datasetDir;
    int sampleCount = 0;
    int trainSampleCount = 0;
    int valSampleCount = 0;
    QJsonObject sampleCounts;
    QJsonObject report;
};

class DatasetExporter {
public:
    static DatasetExportResult exportDataset(
        const ProjectContext& context,
        const DatasetExportPlan& plan);

    static int exportedSampleCount(const QString& datasetDir);
    static QJsonObject exportedSampleCounts(const QString& datasetDir);
};

}  // namespace ppocr
