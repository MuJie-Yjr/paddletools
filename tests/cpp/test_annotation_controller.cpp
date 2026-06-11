#include "app/controllers/AnnotationController.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QTextStream>
#include <cstdlib>

static void require(bool condition, const char* message, int line) {
    if (condition) {
        return;
    }
    QTextStream(stderr) << "test_annotation_controller failed at line " << line << ": " << message << '\n';
    std::exit(1);
}

#define REQUIRE(condition, message) require((condition), (message), __LINE__)

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    ppocr::AnnotationController controller;
    controller.setCurrentAnnotation(QJsonObject{
        {QStringLiteral("asset_id"), QStringLiteral("page_000001")},
        {QStringLiteral("regions"), QJsonArray{}},
        {QStringLiteral("image_labels"), QJsonArray{}},
    });

    const QJsonArray points{
        QJsonArray{0, 0},
        QJsonArray{10, 0},
        QJsonArray{10, 8},
        QJsonArray{0, 8},
    };
    REQUIRE(controller.addOcrRegion(points), "OCR region is added");
    REQUIRE(controller.canUndo(), "add operation records undo state");
    REQUIRE(!controller.selectedRegionId().isEmpty(), "new region becomes selected");
    REQUIRE(controller.selectedRegion().value(QStringLiteral("type")).toString() == QStringLiteral("ocr_text"), "selected region is OCR");
    const QString ocrRegionId = controller.selectedRegionId();

    REQUIRE(controller.updateSelectedRegion(QJsonObject{{QStringLiteral("text"), QStringLiteral("hello")}}), "selected OCR region updates");
    REQUIRE(controller.selectedRegion().value(QStringLiteral("text")).toString() == QStringLiteral("hello"), "updated text is stored");

    REQUIRE(controller.undo(), "undo is available");
    REQUIRE(controller.selectedRegion().value(QStringLiteral("text")).toString().isEmpty(), "undo restores previous text");
    REQUIRE(controller.redo(), "redo is available");
    REQUIRE(controller.selectedRegion().value(QStringLiteral("text")).toString() == QStringLiteral("hello"), "redo reapplies text");

    REQUIRE(controller.setImageLabels(QStringLiteral("0"), QStringLiteral("180"), QStringLiteral("wired_table")), "image labels update");
    REQUIRE(controller.currentAnnotation().value(QStringLiteral("image_labels")).toArray().size() == 3, "three image labels are stored");

    const QJsonArray slantedQuad{
        QJsonArray{0, 0},
        QJsonArray{12, 2},
        QJsonArray{10, 10},
        QJsonArray{1, 8},
    };
    REQUIRE(controller.moveRegion(ocrRegionId, slantedQuad), "OCR quad keeps independent corner edits");
    REQUIRE(controller.findRegion(ocrRegionId).value(QStringLiteral("shape")).toString() == QStringLiteral("quad"), "OCR quad shape is stored");
    REQUIRE(controller.findRegion(ocrRegionId).value(QStringLiteral("points")).toArray().at(1).toArray().at(1).toDouble() == 2,
        "slanted OCR corner is preserved");

    const QJsonArray polygon{
        QJsonArray{0, 0},
        QJsonArray{12, 1},
        QJsonArray{14, 8},
        QJsonArray{8, 12},
        QJsonArray{1, 9},
    };
    REQUIRE(controller.moveRegion(ocrRegionId, polygon), "OCR polygon edit is accepted");
    REQUIRE(controller.findRegion(ocrRegionId).value(QStringLiteral("shape")).toString() == QStringLiteral("polygon"), "OCR polygon shape is stored");
    REQUIRE(controller.findRegion(ocrRegionId).value(QStringLiteral("points")).toArray().size() == 5, "OCR polygon points are stored");

    const QJsonArray bowTie{
        QJsonArray{0, 0},
        QJsonArray{12, 12},
        QJsonArray{0, 12},
        QJsonArray{12, 0},
    };
    REQUIRE(!controller.moveRegion(ocrRegionId, bowTie), "self-intersecting polygon is rejected");
    REQUIRE(controller.findRegion(ocrRegionId).value(QStringLiteral("points")).toArray().size() == 5, "rejected polygon does not replace points");

    REQUIRE(controller.addLayoutRegion(slantedQuad, QStringLiteral("table")), "layout region is added");
    const QString layoutRegionId = controller.selectedRegionId();
    const QJsonObject layoutRegion = controller.findRegion(layoutRegionId);
    REQUIRE(layoutRegion.value(QStringLiteral("points")).toArray().size() == 4, "layout region stores editable points");
    REQUIRE(layoutRegion.value(QStringLiteral("bbox")).toArray().size() == 4, "layout region keeps bbox compatibility");
    REQUIRE(layoutRegion.value(QStringLiteral("shape")).toString() == QStringLiteral("quad"), "layout quad shape is stored");

    REQUIRE(controller.deleteSelectedRegion(), "selected region is deleted");
    REQUIRE(controller.currentAnnotation().value(QStringLiteral("regions")).toArray().size() == 1, "only selected layout region is deleted");
    return 0;
}
