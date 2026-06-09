#include "core/EnvironmentReport.h"

#include "core/RuntimePaths.h"
#include "paddle/PaddleClsEngine.h"
#include "paddle/PaddleDocLayoutEngine.h"
#include "paddle/PaddleInferenceRuntime.h"
#include "paddle/PaddleOcrEngine.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLibraryInfo>
#include <QStringList>

#include <opencv2/core/version.hpp>

namespace ppocr {
namespace {

void addFailure(QJsonArray* failures, const QString& key, bool condition, const QString& message) {
    if (condition || !failures) {
        return;
    }
    failures->append(QJsonObject{
        {QStringLiteral("key"), key},
        {QStringLiteral("message"), message},
    });
}

QJsonObject modelStatus(const QString& modelDir) {
    QString reason;
    const bool ok = PaddleInferenceRuntime::modelDirLooksUsable(modelDir, &reason);
    return {
        {QStringLiteral("ok"), ok},
        {QStringLiteral("usable"), ok},
        {QStringLiteral("error"), reason},
        {QStringLiteral("model_dir"), QDir::toNativeSeparators(modelDir)},
        {QStringLiteral("resolved_model_dir"), QDir::toNativeSeparators(PaddleInferenceRuntime::resolveModelDir(modelDir))},
    };
}

QString runtimeFile(const QString& appDir, const QStringList& patterns) {
    QDir dir(appDir);
    for (const QString& pattern : patterns) {
        const QStringList matches = dir.entryList(QStringList{pattern}, QDir::Files, QDir::Name);
        if (!matches.isEmpty()) {
            return dir.filePath(matches.first());
        }
    }
    return patterns.isEmpty() ? appDir : dir.filePath(patterns.first());
}

QJsonObject layoutModelStatus(const QString& modelDir) {
    QString reason;
    const bool ok = PaddleDocLayoutEngine::modelDirLooksUsable(modelDir, &reason);
    return {
        {QStringLiteral("ok"), ok},
        {QStringLiteral("usable"), ok},
        {QStringLiteral("error"), reason},
        {QStringLiteral("model_dir"), QDir::toNativeSeparators(modelDir)},
        {QStringLiteral("resolved_model_dir"), QDir::toNativeSeparators(PaddleInferenceRuntime::resolveModelDir(modelDir))},
    };
}

QJsonObject runtimeFilesReport(const QString& appDir, QJsonArray* failures) {
    const QJsonObject files{
        {QStringLiteral("qt_core"), EnvironmentReport::pathStatus(QDir(appDir).filePath(QStringLiteral("Qt6Core.dll")))},
        {QStringLiteral("qt_gui"), EnvironmentReport::pathStatus(QDir(appDir).filePath(QStringLiteral("Qt6Gui.dll")))},
        {QStringLiteral("qt_widgets"), EnvironmentReport::pathStatus(QDir(appDir).filePath(QStringLiteral("Qt6Widgets.dll")))},
        {QStringLiteral("qt_pdf"), EnvironmentReport::pathStatus(QDir(appDir).filePath(QStringLiteral("Qt6Pdf.dll")))},
        {QStringLiteral("qt_quick"), EnvironmentReport::pathStatus(QDir(appDir).filePath(QStringLiteral("Qt6Quick.dll")))},
        {QStringLiteral("qt_platform_windows"), EnvironmentReport::pathStatus(QDir(appDir).filePath(QStringLiteral("platforms/qwindows.dll")))},
        {QStringLiteral("opencv_core"), EnvironmentReport::pathStatus(runtimeFile(appDir, {QStringLiteral("opencv_core*.dll")}))},
        {QStringLiteral("opencv_imgproc"), EnvironmentReport::pathStatus(runtimeFile(appDir, {QStringLiteral("opencv_imgproc*.dll")}))},
        {QStringLiteral("opencv_imgcodecs"), EnvironmentReport::pathStatus(runtimeFile(appDir, {QStringLiteral("opencv_imgcodecs*.dll")}))},
        {QStringLiteral("paddle_inference"), EnvironmentReport::pathStatus(QDir(appDir).filePath(QStringLiteral("paddle_inference.dll")))},
        {QStringLiteral("paddle_common"), EnvironmentReport::pathStatus(QDir(appDir).filePath(QStringLiteral("common.dll")))},
        {QStringLiteral("mklml"), EnvironmentReport::pathStatus(QDir(appDir).filePath(QStringLiteral("mklml.dll")))},
        {QStringLiteral("mkldnn"), EnvironmentReport::pathStatus(QDir(appDir).filePath(QStringLiteral("mkldnn.dll")))},
        {QStringLiteral("libiomp5md"), EnvironmentReport::pathStatus(QDir(appDir).filePath(QStringLiteral("libiomp5md.dll")))},
    };
    for (const auto& key : files.keys()) {
        addFailure(
            failures,
            QStringLiteral("runtime_files.%1").arg(key),
            files.value(key).toObject().value(QStringLiteral("exists")).toBool(),
            QStringLiteral("missing runtime file: %1").arg(key));
    }
    return files;
}

QJsonObject dependencyRootsReport(const QString& baseDir, const QString& pythonExe, QJsonArray* failures) {
    const QJsonObject roots{
        {QStringLiteral("PaddleOCR"), EnvironmentReport::pathStatus(QDir(baseDir).filePath(QStringLiteral("PaddleOCR")))},
        {QStringLiteral("PaddleX"), EnvironmentReport::pathStatus(QDir(baseDir).filePath(QStringLiteral("PaddleX")))},
        {QStringLiteral("model_infer"), EnvironmentReport::pathStatus(QDir(baseDir).filePath(QStringLiteral("model/infer")))},
        {QStringLiteral("third_party_paddle_inference"), EnvironmentReport::pathStatus(QDir(baseDir).filePath(QStringLiteral("third_party/paddle_inference")))},
        {QStringLiteral("paddlex_python"), EnvironmentReport::executableStatus(pythonExe)},
        {QStringLiteral("paddlex_main"), EnvironmentReport::pathStatus(QDir(baseDir).filePath(QStringLiteral("PaddleX/main.py")))},
    };
    for (const QString& key : {QStringLiteral("PaddleOCR"), QStringLiteral("PaddleX"), QStringLiteral("model_infer")}) {
        addFailure(
            failures,
            QStringLiteral("dependency_roots.%1").arg(key),
            roots.value(key).toObject().value(QStringLiteral("exists")).toBool(),
            QStringLiteral("missing dependency root: %1").arg(key));
    }
    addFailure(
        failures,
        QStringLiteral("dependency_roots.paddlex_python"),
        roots.value(QStringLiteral("paddlex_python")).toObject().value(QStringLiteral("executable_found")).toBool(),
        QStringLiteral("PaddleX Python executable was not found"));
    addFailure(
        failures,
        QStringLiteral("dependency_roots.paddlex_main"),
        roots.value(QStringLiteral("paddlex_main")).toObject().value(QStringLiteral("exists")).toBool(),
        QStringLiteral("PaddleX/main.py was not found"));
    return roots;
}

QJsonObject modelStatusReport(const QString& baseDir, QJsonArray* failures) {
    const auto ocrConfig = PaddleOcrEngine::defaultConfig(baseDir);
    const auto docClsConfig = PaddleClsEngine::defaultConfig(QStringLiteral("doc_orientation"), baseDir);
    const auto textlineClsConfig = PaddleClsEngine::defaultConfig(QStringLiteral("textline_orientation"), baseDir);
    const auto tableClsConfig = PaddleClsEngine::defaultConfig(QStringLiteral("table_classification"), baseDir);
    const auto layoutConfig = PaddleDocLayoutEngine::defaultConfig(baseDir);
    const QJsonObject models{
        {QStringLiteral("det"), modelStatus(ocrConfig.textDetectionModelDir)},
        {QStringLiteral("rec"), modelStatus(ocrConfig.textRecognitionModelDir)},
        {QStringLiteral("doc_orientation"), modelStatus(docClsConfig.modelDir)},
        {QStringLiteral("textline_orientation"), modelStatus(textlineClsConfig.modelDir)},
        {QStringLiteral("table_classification"), modelStatus(tableClsConfig.modelDir)},
        {QStringLiteral("layout"), layoutModelStatus(layoutConfig.modelDir)},
    };
    for (const QString& key : {QStringLiteral("det"), QStringLiteral("rec"), QStringLiteral("doc_orientation"), QStringLiteral("textline_orientation"), QStringLiteral("layout")}) {
        const QJsonObject status = models.value(key).toObject();
        addFailure(
            failures,
            QStringLiteral("model_status.%1").arg(key),
            status.value(QStringLiteral("ok")).toBool(),
            QStringLiteral("model is not usable: %1: %2").arg(key, status.value(QStringLiteral("error")).toString()));
    }
    return models;
}

QString normalizedBaseDir(const QString& baseDir) {
    return QFileInfo(baseDir.isEmpty() ? EnvironmentReport::defaultBaseDir() : baseDir).absoluteFilePath();
}

QString normalizedApplicationDir(const QString& applicationDir) {
    if (!applicationDir.isEmpty()) {
        return QFileInfo(applicationDir).absoluteFilePath();
    }
    return QCoreApplication::applicationDirPath();
}

}  // namespace

QString EnvironmentReport::defaultBaseDir() {
    return RuntimePaths::defaultBaseDir();
}

QString EnvironmentReport::defaultPaddlexPython() {
    return RuntimePaths::defaultPaddlexPython();
}

QString EnvironmentReport::resolvedExecutable(const QString& executable) {
    return RuntimePaths::resolvedExecutable(executable);
}

QJsonObject EnvironmentReport::pathStatus(const QString& path) {
    return RuntimePaths::pathStatus(path);
}

QJsonObject EnvironmentReport::executableStatus(const QString& executable) {
    return RuntimePaths::executableStatus(executable);
}

QJsonObject EnvironmentReport::build(const EnvironmentReportOptions& options) {
    QJsonArray failures;
    const QString baseDir = normalizedBaseDir(options.baseDir);
    const QString appDir = normalizedApplicationDir(options.applicationDir);
    const QString pythonExe = options.pythonExe.isEmpty() ? defaultPaddlexPython() : options.pythonExe;
    const auto ocrConfig = PaddleOcrEngine::defaultConfig(baseDir);
    const auto docClsConfig = PaddleClsEngine::defaultConfig(QStringLiteral("doc_orientation"), baseDir);
    const auto textlineClsConfig = PaddleClsEngine::defaultConfig(QStringLiteral("textline_orientation"), baseDir);
    const auto tableClsConfig = PaddleClsEngine::defaultConfig(QStringLiteral("table_classification"), baseDir);
    const auto layoutConfig = PaddleDocLayoutEngine::defaultConfig(baseDir);
    QString sdkReason;
    const bool sdkOk = PaddleInferenceRuntime::sdkAvailable(&sdkReason);
    addFailure(&failures, QStringLiteral("paddle_sdk"), sdkOk, sdkReason);

    QJsonObject report{
        {QStringLiteral("ok"), false},
        {QStringLiteral("checked_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("base_dir"), QDir::toNativeSeparators(baseDir)},
        {QStringLiteral("application_dir"), QDir::toNativeSeparators(appDir)},
        {QStringLiteral("paddlex_python"), QDir::toNativeSeparators(pythonExe)},
        {QStringLiteral("qt_version"), qVersion()},
        {QStringLiteral("qt_prefix_path"), QDir::toNativeSeparators(QLibraryInfo::path(QLibraryInfo::PrefixPath))},
        {QStringLiteral("opencv_version"), QStringLiteral(CV_VERSION)},
        {QStringLiteral("paddle_version"), PaddleInferenceRuntime::version()},
        {QStringLiteral("paddle_sdk_root"), QDir::toNativeSeparators(PaddleInferenceRuntime::sdkRoot())},
        {QStringLiteral("paddle_sdk_available"), sdkOk},
        {QStringLiteral("paddle_sdk_error"), sdkReason},
        {QStringLiteral("runtime_files"), runtimeFilesReport(appDir, &failures)},
        {QStringLiteral("dependency_roots"), dependencyRootsReport(baseDir, pythonExe, &failures)},
        {QStringLiteral("model_status"), modelStatusReport(baseDir, &failures)},
        {QStringLiteral("paddle_sdk_smoke"), PaddleInferenceRuntime::smokeReport(ocrConfig.textDetectionModelDir)},
        {QStringLiteral("ocr_config"), PaddleOcrEngine::configReport(ocrConfig)},
        {QStringLiteral("doc_cls_config"), PaddleClsEngine::configReport(docClsConfig)},
        {QStringLiteral("textline_cls_config"), PaddleClsEngine::configReport(textlineClsConfig)},
        {QStringLiteral("table_cls_config"), PaddleClsEngine::configReport(tableClsConfig)},
        {QStringLiteral("layout_config"), PaddleDocLayoutEngine::configReport(layoutConfig)},
    };

    const QJsonObject modelStatus = report.value(QStringLiteral("model_status")).toObject();
    report.insert(QStringLiteral("det_model_ok"), modelStatus.value(QStringLiteral("det")).toObject().value(QStringLiteral("ok")).toBool());
    report.insert(QStringLiteral("det_model_error"), modelStatus.value(QStringLiteral("det")).toObject().value(QStringLiteral("error")).toString());
    report.insert(QStringLiteral("rec_model_ok"), modelStatus.value(QStringLiteral("rec")).toObject().value(QStringLiteral("ok")).toBool());
    report.insert(QStringLiteral("rec_model_error"), modelStatus.value(QStringLiteral("rec")).toObject().value(QStringLiteral("error")).toString());
    report.insert(QStringLiteral("layout_model_ok"), modelStatus.value(QStringLiteral("layout")).toObject().value(QStringLiteral("ok")).toBool());
    report.insert(QStringLiteral("layout_model_error"), modelStatus.value(QStringLiteral("layout")).toObject().value(QStringLiteral("error")).toString());
    report.insert(QStringLiteral("failures"), failures);
    report.insert(QStringLiteral("ok"), failures.isEmpty());
    return report;
}

bool EnvironmentReport::writeJson(const QString& path, const QJsonObject& report) {
    if (path.isEmpty()) {
        return true;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
    return true;
}

}  // namespace ppocr
