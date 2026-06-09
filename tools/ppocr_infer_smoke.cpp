#include "paddle/PaddleInferenceRuntime.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QTextStream>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const QString modelDir = app.arguments().size() > 1
        ? app.arguments().at(1)
        : QStringLiteral("model/infer/PP-OCRv5_server_det_infer/PP-OCRv5_server_det_infer");
    const auto report = ppocr::PaddleInferenceRuntime::smokeReport(modelDir);
    QTextStream(stdout) << QJsonDocument(report).toJson(QJsonDocument::Indented);
    return report.value("ready_for_cpp_infer_integration").toBool() ? 0 : 2;
}
