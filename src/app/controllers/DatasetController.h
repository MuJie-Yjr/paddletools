#pragma once

#include "application/DatasetService.h"

#include <QJsonObject>
#include <QObject>
#include <QString>

namespace ppocr {

class DatasetController : public QObject {
    Q_OBJECT

public:
    explicit DatasetController(QObject* parent = nullptr);

    static QJsonObject defaultSplitConfig();
    static QList<PageInfo> listPages(const ProjectContext& context);
    static void saveProject(const ProjectContext& context);
    static SplitSummary splitSummary(const ProjectContext& context);
    static int reassignSplitsByOrder(ProjectContext& context, double trainRatio, double valRatio, double testRatio = 0.0);
    static int reassignSplitsRandom(ProjectContext& context, double trainRatio, double valRatio, double testRatio = 0.0, int seed = 42);
    static void exportSplitReport(const ProjectContext& context, const QString& path);
};

}  // namespace ppocr
