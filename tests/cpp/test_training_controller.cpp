#include "app/controllers/TrainingController.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>
#include <cstdlib>

static void require(bool condition, const char* message, int line) {
    if (condition) {
        return;
    }
    QTextStream(stderr) << "test_training_controller failed at line " << line << ": " << message << '\n';
    std::exit(1);
}

#define REQUIRE(condition, message) require((condition), (message), __LINE__)

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    REQUIRE(temp.isValid(), "temporary directory is valid");

    ppocr::TrainingController controller(temp.path());
    REQUIRE(!controller.isRunning(), "controller starts idle");

    ppocr::TrainingOptions options;
    options.pythonExe = "python";
    const auto command = controller.buildCommand(
        nullptr,
        QStringLiteral("det_v5_server"),
        QFileInfo(temp.filePath(QStringLiteral("out"))).absoluteFilePath(),
        options);
    REQUIRE(!command.program.isEmpty(), "controller can build a training command");
    REQUIRE(!command.arguments.isEmpty(), "training command has arguments");

    const QJsonObject metrics = controller.parseMetrics(QStringLiteral("loss: 1.25 hmean=0.62 precision: 0.7 recall: 0.55"));
    REQUIRE(metrics.value(QStringLiteral("loss")).toDouble() == 1.25, "controller delegates metric parsing to TrainingRunner");
    REQUIRE(metrics.value(QStringLiteral("hmean")).toDouble() == 0.62, "metric parser returns hmean");
    return 0;
}
