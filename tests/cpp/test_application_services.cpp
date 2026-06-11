#include "application/AnnotationService.h"
#include "application/DatasetService.h"
#include "application/PredictionService.h"
#include "application/ProjectService.h"
#include "application/TrainingService.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QTextStream>
#include <cstdlib>

static void require(bool condition, const char* message, int line) {
    if (condition) {
        return;
    }
    QTextStream(stderr) << "test_application_services failed at line " << line << ": " << message << '\n';
    std::exit(1);
}

#define REQUIRE(condition, message) require((condition), (message), __LINE__)

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    REQUIRE(temp.isValid(), "temporary directory is valid");

    ppocr::ProjectService projects(temp.path());
    auto context = projects.createProject(temp.filePath(QStringLiteral("project")), QStringLiteral("demo"));
    REQUIRE(QFileInfo::exists(context.root.filePath(QStringLiteral("project.json"))), "project service creates project");
    projects.rememberProject(context.root.absolutePath());
    REQUIRE(projects.recentProjects().contains(context.root.absolutePath()), "project service stores recent project");

    REQUIRE(ppocr::DatasetService::defaultSplitConfig().value(QStringLiteral("strategy")).toString() == QStringLiteral("page_index_modulo"),
        "dataset service exposes split defaults");
    REQUIRE(ppocr::DatasetService::splitSummary(context).total == 0, "dataset service summarizes empty project");

    QJsonObject annotation{
        {QStringLiteral("asset_id"), QStringLiteral("page_000001")},
        {QStringLiteral("regions"), QJsonArray{}},
    };
    const QJsonArray points{
        QJsonArray{0, 0},
        QJsonArray{20, 0},
        QJsonArray{20, 10},
        QJsonArray{0, 10},
    };
    annotation = ppocr::AnnotationService::addOcrRegion(annotation, points);
    REQUIRE(annotation.value(QStringLiteral("regions")).toArray().size() == 1, "annotation service adds OCR region");
    REQUIRE(ppocr::AnnotationService::isValidRegionPoints(points), "axis-aligned quad is valid");
    const QJsonArray invalidPoints{
        QJsonArray{0, 0},
        QJsonArray{20, 10},
        QJsonArray{0, 10},
        QJsonArray{20, 0},
    };
    REQUIRE(!ppocr::AnnotationService::isValidRegionPoints(invalidPoints), "self-intersecting quad is invalid");

    const QJsonArray layoutPoints{
        QJsonArray{2, 0},
        QJsonArray{22, 3},
        QJsonArray{20, 13},
        QJsonArray{0, 10},
    };
    annotation = ppocr::AnnotationService::addLayoutRegion(annotation, layoutPoints, QStringLiteral("table"));
    const QJsonObject layoutRegion = annotation.value(QStringLiteral("regions")).toArray().last().toObject();
    REQUIRE(layoutRegion.value(QStringLiteral("points")).toArray().size() == 4, "layout service stores editable points");
    REQUIRE(layoutRegion.value(QStringLiteral("bbox")).toArray().size() == 4, "layout service keeps bbox export compatibility");

    ppocr::PredictionRunRequest request;
    request.program = temp.filePath(QStringLiteral("missing_predictor.exe"));
    QString errorMessage;
    REQUIRE(!ppocr::PredictionService::validateRequest(request, &errorMessage), "prediction service rejects missing executable");
    REQUIRE(errorMessage.contains(QStringLiteral("does not exist")), "prediction validation returns useful message");

    ppocr::TrainingService training(temp.path());
    ppocr::TrainingOptions options;
    options.pythonExe = QStringLiteral("python");
    const auto command = training.buildCommand(nullptr, QStringLiteral("det_v5_server"), temp.filePath(QStringLiteral("out")), options);
    REQUIRE(!command.program.isEmpty(), "training service builds command");
    REQUIRE(training.parseMetrics(QStringLiteral("loss=1.5 hmean=0.75")).value(QStringLiteral("hmean")).toDouble() == 0.75,
        "training service parses metrics");

    return 0;
}
