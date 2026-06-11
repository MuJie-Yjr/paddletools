#pragma once

#include "domain/Dataset.h"
#include "domain/Page.h"
#include "domain/Project.h"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace ppocr {

class DatasetService {
public:
    static QJsonObject defaultSplitConfig();
    static QList<PageInfo> listPages(const ProjectContext& context);
    static QStringList importAssets(ProjectContext& context, const QStringList& paths);
    static void saveProject(const ProjectContext& context);
    static SplitSummary splitSummary(const ProjectContext& context);
    static int reassignSplitsByOrder(ProjectContext& context, double trainRatio, double valRatio, double testRatio = 0.0);
    static int reassignSplitsRandom(ProjectContext& context, double trainRatio, double valRatio, double testRatio = 0.0, int seed = 42);
    static void exportSplitReport(const ProjectContext& context, const QString& path);

private:
    static int applySplitAssignments(
        ProjectContext& context,
        const QList<PageInfo>& pages,
        double trainRatio,
        double valRatio,
        double testRatio,
        const QString& strategy,
        int seed);
    static QString splitStrategyText(const QJsonObject& splits, bool looksLikePageModulo);
    static bool pagesLookLikePageModulo(const QList<PageInfo>& pages);
};

}  // namespace ppocr
