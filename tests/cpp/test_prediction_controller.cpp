#include "app/controllers/PredictionController.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTextStream>
#include <cstdlib>

static void require(bool condition, const char* message, int line) {
    if (condition) {
        return;
    }
    QTextStream(stderr) << "test_prediction_controller failed at line " << line << ": " << message << '\n';
    std::exit(1);
}

#define REQUIRE(condition, message) require((condition), (message), __LINE__)

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    REQUIRE(temp.isValid(), "temporary directory is valid");

    ppocr::PredictionController controller;
    REQUIRE(!controller.isRunning(), "controller starts idle");

    ppocr::PredictionRunRequest request;
    request.program = temp.filePath(QStringLiteral("missing_predictor.exe"));
    request.workingDirectory = temp.path();
    QString errorMessage;
    REQUIRE(!controller.startPrediction(request, &errorMessage), "missing program is rejected before spawning");
    REQUIRE(errorMessage.contains(QStringLiteral("does not exist")), "missing program explains the problem");
    REQUIRE(!controller.isRunning(), "controller remains idle after rejected start");
    return 0;
}
