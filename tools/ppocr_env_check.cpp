#include "core/EnvironmentReport.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ppocr_env_check"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Check the C++ PPOCR Workbench runtime, dependency, model, and training bridge environment."));
    parser.addHelpOption();
    const QCommandLineOption baseOpt(QStringLiteral("base-dir"), QStringLiteral("PPOCR workbench base directory."), QStringLiteral("dir"), ppocr::EnvironmentReport::defaultBaseDir());
    const QCommandLineOption pythonOpt(QStringLiteral("python"), QStringLiteral("Python executable used only for the PaddleX training bridge."), QStringLiteral("exe"), ppocr::EnvironmentReport::defaultPaddlexPython());
    const QCommandLineOption reportOpt(QStringLiteral("report"), QStringLiteral("Write the JSON report to a file."), QStringLiteral("path"));
    parser.addOptions({baseOpt, pythonOpt, reportOpt});
    parser.process(app);

    ppocr::EnvironmentReportOptions options;
    options.baseDir = QFileInfo(parser.value(baseOpt)).absoluteFilePath();
    options.pythonExe = parser.value(pythonOpt);
    const QJsonObject report = ppocr::EnvironmentReport::build(options);
    if (!ppocr::EnvironmentReport::writeJson(parser.value(reportOpt), report)) {
        QTextStream(stderr) << "failed to write environment report: " << parser.value(reportOpt) << '\n';
        return 3;
    }
    QTextStream(stdout) << QJsonDocument(report).toJson(QJsonDocument::Indented);
    return report.value(QStringLiteral("ok")).toBool() ? 0 : 2;
}
