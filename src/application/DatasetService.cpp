#include "application/DatasetService.h"

#include "core/AssetImporter.h"
#include "core/ProjectRepository.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>

namespace ppocr {

QJsonObject DatasetService::defaultSplitConfig() {
    return QJsonObject{
        {QStringLiteral("strategy"), QStringLiteral("page_index_modulo")},
        {QStringLiteral("train_ratio"), 0.8},
        {QStringLiteral("val_ratio"), 0.2},
        {QStringLiteral("test_ratio"), 0.0},
        {QStringLiteral("seed"), 42},
        {QStringLiteral("description"), QStringLiteral("Imported pages use page_index % 5 == 0 for val; all other pages are train.")},
    };
}

QList<PageInfo> DatasetService::listPages(const ProjectContext& context) {
    return ProjectRepository::listPages(context);
}

QStringList DatasetService::importAssets(ProjectContext& context, const QStringList& paths) {
    return AssetImporter::importPaths(context, paths);
}

void DatasetService::saveProject(const ProjectContext& context) {
    ProjectRepository::saveProject(context);
}

SplitSummary DatasetService::splitSummary(const ProjectContext& context) {
    const QList<PageInfo> pages = listPages(context);
    const QJsonObject splits = context.config.value(QStringLiteral("splits")).toObject(defaultSplitConfig());
    SplitSummary summary;
    summary.total = pages.size();
    summary.strategy = splits.value(QStringLiteral("strategy")).toString(QStringLiteral("page_index_modulo"));
    summary.trainRatio = splits.value(QStringLiteral("train_ratio")).toDouble(0.8);
    summary.valRatio = splits.value(QStringLiteral("val_ratio")).toDouble(0.2);
    summary.testRatio = splits.value(QStringLiteral("test_ratio")).toDouble(0.0);
    for (const auto& page : pages) {
        if (page.split == PageSplit::Train) {
            ++summary.train;
        } else if (page.split == PageSplit::Val) {
            ++summary.val;
        } else if (page.split == PageSplit::Test) {
            ++summary.test;
        } else {
            ++summary.unassigned;
        }
    }
    summary.strategyText = splitStrategyText(splits, pagesLookLikePageModulo(pages));
    return summary;
}

int DatasetService::reassignSplitsByOrder(ProjectContext& context, double trainRatio, double valRatio, double testRatio) {
    QList<PageInfo> pages = ProjectRepository::listPages(context);
    std::sort(pages.begin(), pages.end(), [](const PageInfo& left, const PageInfo& right) {
        if (left.pageIndex != right.pageIndex) {
            return left.pageIndex < right.pageIndex;
        }
        return left.assetId < right.assetId;
    });
    return applySplitAssignments(context, pages, trainRatio, valRatio, testRatio, QStringLiteral("ratio_by_order"), 42);
}

int DatasetService::reassignSplitsRandom(ProjectContext& context, double trainRatio, double valRatio, double testRatio, int seed) {
    QList<PageInfo> pages = ProjectRepository::listPages(context);
    std::sort(pages.begin(), pages.end(), [](const PageInfo& left, const PageInfo& right) {
        return left.assetId < right.assetId;
    });
    std::mt19937 generator(static_cast<std::mt19937::result_type>(seed));
    std::shuffle(pages.begin(), pages.end(), generator);
    return applySplitAssignments(context, pages, trainRatio, valRatio, testRatio, QStringLiteral("ratio_random"), seed);
}

int DatasetService::applySplitAssignments(
    ProjectContext& context,
    const QList<PageInfo>& pages,
    double trainRatio,
    double valRatio,
    double testRatio,
    const QString& strategy,
    int seed) {
    const double train = std::max(0.0, trainRatio);
    const double val = std::max(0.0, valRatio);
    const double test = std::max(0.0, testRatio);
    const double totalRatio = train + val + test;
    if (pages.isEmpty() || totalRatio <= 0.0) {
        return 0;
    }

    const int total = pages.size();
    int trainCount = static_cast<int>(std::round(total * train / totalRatio));
    int valCount = static_cast<int>(std::round(total * val / totalRatio));
    trainCount = qBound(0, trainCount, total);
    valCount = qBound(0, valCount, total - trainCount);
    if (test <= 0.0 && trainCount + valCount < total) {
        valCount = total - trainCount;
    }

    int changed = 0;
    for (int i = 0; i < pages.size(); ++i) {
        PageSplit split = PageSplit::Test;
        if (i < trainCount) {
            split = PageSplit::Train;
        } else if (i < trainCount + valCount) {
            split = PageSplit::Val;
        }

        QJsonObject annotation = ProjectRepository::readAnnotation(pages.at(i).annotationPath);
        const auto currentSplit = pageSplitFromString(annotation.value(QStringLiteral("split")).toString(QStringLiteral("train")))
            .value_or(PageSplit::Unassigned);
        if (currentSplit == split) {
            continue;
        }
        annotation.insert(QStringLiteral("split"), toString(split));
        ProjectRepository::writeAnnotation(pages.at(i).annotationPath, annotation);
        ++changed;
    }

    QJsonObject splits{
        {QStringLiteral("strategy"), strategy},
        {QStringLiteral("train_ratio"), train / totalRatio},
        {QStringLiteral("val_ratio"), val / totalRatio},
        {QStringLiteral("test_ratio"), test / totalRatio},
        {QStringLiteral("seed"), seed},
        {QStringLiteral("updated_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
    };
    context.config.insert(QStringLiteral("splits"), splits);
    ProjectRepository::saveProject(context);
    return changed;
}

void DatasetService::exportSplitReport(const ProjectContext& context, const QString& path) {
    QJsonArray pageArray;
    for (const auto& page : ProjectRepository::listPages(context)) {
        pageArray.append(QJsonObject{
            {QStringLiteral("asset_id"), page.assetId},
            {QStringLiteral("page_index"), page.pageIndex},
            {QStringLiteral("split"), toString(page.split)},
            {QStringLiteral("status"), toString(page.status)},
            {QStringLiteral("image_path"), page.relativeImagePath},
        });
    }

    const SplitSummary summary = splitSummary(context);
    const QJsonObject report{
        {QStringLiteral("project_dir"), context.root.absolutePath()},
        {QStringLiteral("generated_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("splits"), context.config.value(QStringLiteral("splits")).toObject(defaultSplitConfig())},
        {QStringLiteral("summary"), QJsonObject{
            {QStringLiteral("total"), summary.total},
            {QStringLiteral("train"), summary.train},
            {QStringLiteral("val"), summary.val},
            {QStringLiteral("test"), summary.test},
            {QStringLiteral("unassigned"), summary.unassigned},
            {QStringLiteral("strategy_text"), summary.strategyText},
        }},
        {QStringLiteral("pages"), pageArray},
    };

    QFileInfo info(path);
    QDir().mkpath(info.absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        throw std::runtime_error(QStringLiteral("failed to write split report: %1").arg(path).toStdString());
    }
    file.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
}

QString DatasetService::splitStrategyText(const QJsonObject& splits, bool looksLikePageModulo) {
    const QString strategy = splits.value(QStringLiteral("strategy")).toString(QStringLiteral("page_index_modulo"));
    if (strategy == QStringLiteral("page_index_modulo")) {
        return QStringLiteral("按页号 4:1 固定划分（page_index % 5 == 0 为 val）");
    }
    if (strategy == QStringLiteral("ratio_by_order")) {
        return QStringLiteral("按文件顺序比例划分（train %1% / val %2% / test %3%）")
            .arg(std::round(splits.value(QStringLiteral("train_ratio")).toDouble(0.8) * 100.0))
            .arg(std::round(splits.value(QStringLiteral("val_ratio")).toDouble(0.2) * 100.0))
            .arg(std::round(splits.value(QStringLiteral("test_ratio")).toDouble(0.0) * 100.0));
    }
    if (strategy == QStringLiteral("ratio_random")) {
        return QStringLiteral("固定比例随机划分（train %1% / val %2% / test %3%，seed %4）")
            .arg(std::round(splits.value(QStringLiteral("train_ratio")).toDouble(0.8) * 100.0))
            .arg(std::round(splits.value(QStringLiteral("val_ratio")).toDouble(0.2) * 100.0))
            .arg(std::round(splits.value(QStringLiteral("test_ratio")).toDouble(0.0) * 100.0))
            .arg(splits.value(QStringLiteral("seed")).toInt(42));
    }
    if (strategy == QStringLiteral("manual")) {
        return looksLikePageModulo
            ? QStringLiteral("历史项目：当前页面看起来仍是按页号 4:1 固定划分")
            : QStringLiteral("手动划分");
    }
    return strategy;
}

bool DatasetService::pagesLookLikePageModulo(const QList<PageInfo>& pages) {
    if (pages.isEmpty()) {
        return false;
    }
    for (const auto& page : pages) {
        const PageSplit expected = page.pageIndex % 5 == 0 ? PageSplit::Val : PageSplit::Train;
        if (page.split != expected) {
            return false;
        }
    }
    return true;
}

}  // namespace ppocr
