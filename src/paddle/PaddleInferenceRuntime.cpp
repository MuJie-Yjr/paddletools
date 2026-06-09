#include "paddle/PaddleInferenceRuntime.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QTextStream>
#include <paddle_inference_api.h>

namespace ppocr {
namespace {

bool hasInferenceFiles(const QString& modelDir) {
    const QFileInfo dir(modelDir);
    if (!dir.exists() || !dir.isDir()) {
        return false;
    }
    const bool hasParams = QFileInfo::exists(dir.filePath() + "/inference.pdiparams");
    const bool hasModel = QFileInfo::exists(dir.filePath() + "/inference.json")
        || QFileInfo::exists(dir.filePath() + "/inference.pdmodel")
        || QFileInfo::exists(dir.filePath() + "/model.json");
    return hasParams && hasModel;
}

QString absolutePath(const QString& path) {
    return QFileInfo(path).absoluteFilePath();
}

bool hasFullSdkLayout(const QString& root) {
    return QFileInfo::exists(root + "/paddle/include/paddle_inference_api.h")
        && QFileInfo::exists(root + "/paddle/lib/paddle_inference.lib")
        && QFileInfo::exists(root + "/paddle/lib/paddle_inference.dll");
}

bool hasAppLocalRuntime(const QString& dir) {
    return QFileInfo::exists(QDir(dir).filePath(QStringLiteral("paddle_inference.dll")))
        && QFileInfo::exists(QDir(dir).filePath(QStringLiteral("common.dll")));
}

QString inferenceModelFile(const QString& modelDir) {
    const QDir dir(modelDir);
    const QStringList candidates{
        QStringLiteral("inference.json"),
        QStringLiteral("inference.pdmodel"),
        QStringLiteral("model.json"),
    };
    for (const auto& candidate : candidates) {
        const QString path = dir.filePath(candidate);
        if (QFileInfo::exists(path)) {
            return path;
        }
    }
    return {};
}

QJsonArray stringArray(const std::vector<std::string>& values) {
    QJsonArray out;
    for (const auto& value : values) {
        out.append(QString::fromStdString(value));
    }
    return out;
}

QJsonArray shapeArray(const std::vector<int64_t>& values) {
    QJsonArray out;
    for (const auto value : values) {
        out.append(static_cast<qint64>(value));
    }
    return out;
}

QJsonObject tensorShapesObject(const std::map<std::string, std::vector<int64_t>>& shapes) {
    QJsonObject out;
    for (const auto& item : shapes) {
        out.insert(QString::fromStdString(item.first), shapeArray(item.second));
    }
    return out;
}

QJsonObject predictorMetadata(const QString& resolvedModelDir) {
    QJsonObject metadata;
    const QString modelFile = inferenceModelFile(resolvedModelDir);
    const QString paramsFile = QDir(resolvedModelDir).filePath(QStringLiteral("inference.pdiparams"));
    metadata.insert(QStringLiteral("model_file"), modelFile);
    metadata.insert(QStringLiteral("params_file"), paramsFile);
    if (modelFile.isEmpty() || !QFileInfo::exists(paramsFile)) {
        metadata.insert(QStringLiteral("loaded"), false);
        metadata.insert(QStringLiteral("load_error"), QStringLiteral("missing model or params file"));
        return metadata;
    }

    try {
        paddle_infer::Config config;
        config.SetModel(
            QDir::toNativeSeparators(modelFile).toLocal8Bit().constData(),
            QDir::toNativeSeparators(paramsFile).toLocal8Bit().constData());
        config.DisableGpu();
        config.DisableGlogInfo();
        config.DisableMKLDNN();
        config.SetCpuMathLibraryNumThreads(1);
        auto predictor = paddle_infer::CreatePredictor(config);
        metadata.insert(QStringLiteral("loaded"), true);
        metadata.insert(QStringLiteral("input_names"), stringArray(predictor->GetInputNames()));
        metadata.insert(QStringLiteral("output_names"), stringArray(predictor->GetOutputNames()));
        metadata.insert(QStringLiteral("input_shapes"), tensorShapesObject(predictor->GetInputTensorShape()));
        metadata.insert(QStringLiteral("output_shapes"), tensorShapesObject(predictor->GetOutputTensorShape()));
    } catch (const std::exception& exc) {
        metadata.insert(QStringLiteral("loaded"), false);
        metadata.insert(QStringLiteral("load_error"), QString::fromUtf8(exc.what()));
    }
    return metadata;
}

}  // namespace

QString PaddleInferenceRuntime::sdkRoot() {
    const QString configured = QString::fromLocal8Bit(qgetenv("PPOCR_PADDLE_INFERENCE_ROOT")).trimmed();
    if (!configured.isEmpty()) {
        return absolutePath(configured);
    }
#ifdef PADDLE_LIB_ROOT
    const QString buildRoot = QString::fromUtf8(PADDLE_LIB_ROOT);
    if (QFileInfo::exists(buildRoot)) {
        return absolutePath(buildRoot);
    }
#else
    const QString buildRoot;
#endif
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString appLocalSdk = QDir(appDir).filePath(QStringLiteral("third_party/paddle_inference"));
    if (QFileInfo::exists(appLocalSdk)) {
        return absolutePath(appLocalSdk);
    }
    if (hasAppLocalRuntime(appDir)) {
        return absolutePath(appDir);
    }
#ifdef PADDLE_LIB_ROOT
    return absolutePath(buildRoot);
#else
    return absolutePath(appLocalSdk);
#endif
}

QString PaddleInferenceRuntime::version() {
    return QString::fromStdString(paddle_infer::GetVersion());
}

bool PaddleInferenceRuntime::sdkAvailable(QString* reason) {
    const QString root = sdkRoot();
    if (hasFullSdkLayout(root) || hasAppLocalRuntime(root)) {
        return true;
    }
    const QString appDir = QCoreApplication::applicationDirPath();
    if (root != appDir && hasAppLocalRuntime(appDir)) {
        return true;
    }
    if (reason) {
        *reason = QString("missing Paddle Inference SDK files or app-local runtime DLLs under %1").arg(root);
    }
    return false;
}

bool PaddleInferenceRuntime::gpuSupported() {
    const QStringList versionFiles{
        QDir(sdkRoot()).filePath(QStringLiteral("version.txt")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("version.txt")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("third_party/paddle_inference/version.txt")),
    };
    for (const QString& versionFile : versionFiles) {
        QFile file(versionFile);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        QTextStream stream(&file);
        while (!stream.atEnd()) {
            const QString line = stream.readLine().trimmed();
            if (line.startsWith(QStringLiteral("WITH_GPU"), Qt::CaseInsensitive)) {
                return line.contains(QStringLiteral("ON"), Qt::CaseInsensitive)
                    || line.contains(QStringLiteral("TRUE"), Qt::CaseInsensitive)
                    || line.endsWith(QStringLiteral("1"));
            }
        }
    }
#ifdef WITH_GPU
    return true;
#else
    return false;
#endif
}

QString PaddleInferenceRuntime::resolveModelDir(const QString& modelDir) {
    const QFileInfo dirInfo(modelDir);
    if (!dirInfo.exists() || !dirInfo.isDir()) {
        return modelDir;
    }
    const QString absolute = dirInfo.absoluteFilePath();
    if (hasInferenceFiles(absolute)) {
        return absolute;
    }

    const QDir dir(absolute);
    const QString sameNameChild = dir.filePath(dirInfo.fileName());
    if (hasInferenceFiles(sameNameChild)) {
        return QFileInfo(sameNameChild).absoluteFilePath();
    }

    const QFileInfoList children = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& child : children) {
        if (hasInferenceFiles(child.absoluteFilePath())) {
            return child.absoluteFilePath();
        }
    }
    return absolute;
}

bool PaddleInferenceRuntime::modelDirLooksUsable(const QString& modelDir, QString* reason) {
    const QString resolvedDir = resolveModelDir(modelDir);
    const QFileInfo dir(resolvedDir);
    if (!dir.exists() || !dir.isDir()) {
        if (reason) {
            *reason = QString("model directory does not exist: %1").arg(modelDir);
        }
        return false;
    }
    if (!hasInferenceFiles(resolvedDir)) {
        if (reason) {
            *reason = QString("missing inference model files in %1").arg(resolvedDir);
        }
        return false;
    }
    return true;
}

QJsonObject PaddleInferenceRuntime::smokeReport(const QString& modelDir) {
    QString sdkReason;
    QString modelReason;
    const bool sdkOk = sdkAvailable(&sdkReason);
    const bool modelOk = modelDirLooksUsable(modelDir, &modelReason);
    QJsonObject report{
        {"sdk_root", sdkRoot()},
        {"sdk_available", sdkOk},
        {"sdk_error", sdkReason},
        {"paddle_version", sdkOk ? version() : QString()},
        {"model_dir", modelDir},
        {"resolved_model_dir", resolveModelDir(modelDir)},
        {"model_available", modelOk},
        {"model_error", modelReason},
        {"ready_for_cpp_infer_integration", sdkOk && modelOk},
    };
    if (sdkOk && modelOk) {
        report.insert(QStringLiteral("predictor"), predictorMetadata(resolveModelDir(modelDir)));
    }
    return report;
}

}  // namespace ppocr
