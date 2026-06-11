#include "app/controllers/DatasetController.h"

#include "core/AssetImporter.h"
#include "core/ProjectRepository.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QImage>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>
#include <cstdlib>

static void require(bool condition, const char* message, int line) {
    if (condition) {
        return;
    }
    QTextStream(stderr) << "test_dataset_controller failed at line " << line << ": " << message << '\n';
    std::exit(1);
}

#define REQUIRE(condition, message) require((condition), (message), __LINE__)

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    REQUIRE(temp.isValid(), "temporary directory is valid");
    REQUIRE(ppocr::toString(ppocr::PageSplit::Train) == QStringLiteral("train"), "PageSplit serializes to stable JSON text");
    REQUIRE(ppocr::pageSplitFromString(QStringLiteral("val")).value() == ppocr::PageSplit::Val, "PageSplit parses val");
    REQUIRE(!ppocr::pageSplitFromString(QStringLiteral("valid")).has_value(), "unknown split text is rejected");
    REQUIRE(ppocr::pageStatusFromString(QStringLiteral("validated")).value() == ppocr::PageStatus::Checked, "legacy validated page status parses as checked");
    REQUIRE(ppocr::regionTypeFromString(QStringLiteral("ocr_text")).value() == ppocr::RegionType::OcrText, "RegionType parses OCR text");
    REQUIRE(ppocr::annotationSourceFromString(QStringLiteral("ocr_prelabel")).value() == ppocr::AnnotationSource::OcrPrelabel, "AnnotationSource parses OCR prelabel");
    REQUIRE(ppocr::trainingTaskKindFromString(QStringLiteral("table_cls")).value() == ppocr::TrainingTaskKind::TableCls, "TrainingTaskKind parses table classification");
    REQUIRE(ppocr::runStatusFromString(QStringLiteral("success")).value() == ppocr::RunStatus::Finished, "legacy success run status parses as finished");
    REQUIRE(ppocr::predictionStatusFromString(QStringLiteral("success")).value() == ppocr::PredictionStatus::Finished, "legacy success prediction status parses as finished");

    auto context = ppocr::ProjectRepository::createProject(temp.filePath(QStringLiteral("project")), QStringLiteral("demo"));
    for (int i = 0; i < 10; ++i) {
        const QString path = temp.filePath(QStringLiteral("source_%1.png").arg(i));
        QImage image(120, 80, QImage::Format_RGB888);
        image.fill(Qt::white);
        REQUIRE(image.save(path), "source image is saved");
        ppocr::AssetImporter::importPaths(context, {path});
    }

    auto summary = ppocr::DatasetController::splitSummary(context);
    REQUIRE(summary.total == 10, "split summary counts pages");
    REQUIRE(summary.train == 8 && summary.val == 2, "default import split is page-index 4:1");
    REQUIRE(ppocr::ProjectRepository::listPages(context).first().split == ppocr::PageSplit::Train, "imported pages expose typed split");
    REQUIRE(ppocr::ProjectRepository::listPages(context).first().status == ppocr::PageStatus::Unlabeled, "imported pages expose typed status");

    const int changed = ppocr::DatasetController::reassignSplitsByOrder(context, 0.7, 0.3);
    REQUIRE(changed > 0, "ratio split changes at least one page");
    summary = ppocr::DatasetController::splitSummary(context);
    REQUIRE(summary.train == 7 && summary.val == 3, "ratio split updates train and val counts");
    REQUIRE(context.config.value(QStringLiteral("splits")).toObject().value(QStringLiteral("strategy")).toString() == QStringLiteral("ratio_by_order"),
        "ratio split strategy is persisted");

    const int randomChanged = ppocr::DatasetController::reassignSplitsRandom(context, 0.6, 0.4, 0.0, 123);
    REQUIRE(randomChanged >= 0, "random ratio split runs");
    summary = ppocr::DatasetController::splitSummary(context);
    REQUIRE(summary.train == 6 && summary.val == 4, "random ratio split updates train and val counts");
    REQUIRE(context.config.value(QStringLiteral("splits")).toObject().value(QStringLiteral("strategy")).toString() == QStringLiteral("ratio_random"),
        "random split strategy is persisted");
    REQUIRE(context.config.value(QStringLiteral("splits")).toObject().value(QStringLiteral("seed")).toInt() == 123,
        "random split seed is persisted");

    auto invalidAnnotation = ppocr::ProjectRepository::readAnnotation(ppocr::ProjectRepository::listPages(context).first().annotationPath);
    invalidAnnotation.insert(QStringLiteral("split"), QStringLiteral("valid"));
    ppocr::ProjectRepository::writeAnnotation(ppocr::ProjectRepository::listPages(context).first().annotationPath, invalidAnnotation);
    summary = ppocr::DatasetController::splitSummary(context);
    REQUIRE(summary.unassigned == 1, "invalid split text is surfaced as unassigned");

    const QString reportPath = temp.filePath(QStringLiteral("split_report.json"));
    ppocr::DatasetController::exportSplitReport(context, reportPath);
    REQUIRE(QFileInfo::exists(reportPath), "split report is written");
    return 0;
}
