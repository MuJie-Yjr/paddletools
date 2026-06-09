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
#include <QStringList>
#include <QVector>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <paddle_inference_api.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace ppocr {
namespace {

struct LayoutModelMeta {
    QString modelName = QStringLiteral("PP-DocLayout-S");
    QStringList labels;
    int targetWidth = 480;
    int targetHeight = 480;
    double drawThreshold = 0.5;
};

struct LayoutBox {
    int classId = -1;
    QString label;
    double score = 0.0;
    double left = 0.0;
    double top = 0.0;
    double right = 0.0;
    double bottom = 0.0;
    int order = 0;
};

std::string nativePathString(const QString& path) {
    const QByteArray bytes = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath()).toLocal8Bit();
    return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
}

bool useGpuDevice(const QString& device) {
    return device.trimmed().startsWith(QStringLiteral("gpu"), Qt::CaseInsensitive);
}

int deviceIdFromString(const QString& device) {
    const QStringList parts = device.split(QLatin1Char(':'));
    if (parts.size() < 2) {
        return 0;
    }
    bool ok = false;
    const int id = parts.at(1).toInt(&ok);
    return ok && id >= 0 ? id : 0;
}

QString ensureOutputDir(const QString& requestedOutputDir) {
    QString outputDir = requestedOutputDir;
    if (outputDir.isEmpty()) {
        outputDir = QDir::temp().filePath(
            QStringLiteral("ppocr_workbench_layout_%1")
                .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz"))));
    }
    QDir dir(outputDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        throw std::runtime_error(QStringLiteral("failed to create output directory: %1").arg(outputDir).toStdString());
    }
    return QFileInfo(outputDir).absoluteFilePath();
}

QString inferenceModelFile(const QString& resolvedModelDir) {
    const QDir dir(resolvedModelDir);
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

QString inferenceConfigFile(const QString& resolvedModelDir) {
    const QString path = QDir(resolvedModelDir).filePath(QStringLiteral("inference.yml"));
    return QFileInfo::exists(path) ? path : QString();
}

QString completeBaseName(const QString& path) {
    return QFileInfo(path).completeBaseName();
}

QStringList collectImages(const QString& inputPath) {
    const QFileInfo info(inputPath);
    if (info.isFile()) {
        return {info.absoluteFilePath()};
    }
    if (!info.isDir()) {
        return {};
    }
    const QStringList filters{
        QStringLiteral("*.jpg"),
        QStringLiteral("*.jpeg"),
        QStringLiteral("*.png"),
        QStringLiteral("*.bmp"),
        QStringLiteral("*.tif"),
        QStringLiteral("*.tiff"),
    };
    QStringList images;
    const QFileInfoList entries = QDir(info.absoluteFilePath()).entryInfoList(filters, QDir::Files, QDir::Name);
    for (const auto& entry : entries) {
        images.append(entry.absoluteFilePath());
    }
    return images;
}

QString labelForId(const QStringList& labels, int classId) {
    if (classId >= 0 && classId < labels.size()) {
        return labels.at(classId);
    }
    return QStringLiteral("layout_%1").arg(classId);
}

QJsonArray coordinateArray(const LayoutBox& box) {
    return {
        std::round(box.left),
        std::round(box.top),
        std::round(box.right),
        std::round(box.bottom),
    };
}

QJsonObject boxObject(const LayoutBox& box) {
    return {
        {QStringLiteral("cls_id"), box.classId},
        {QStringLiteral("label"), box.label},
        {QStringLiteral("score"), box.score},
        {QStringLiteral("coordinate"), coordinateArray(box)},
        {QStringLiteral("order"), box.order},
    };
}

QJsonArray boxesArray(const std::vector<LayoutBox>& boxes) {
    QJsonArray values;
    for (const auto& box : boxes) {
        values.append(boxObject(box));
    }
    return values;
}

QJsonObject errorReport(
    const QString& error,
    const QString& imagePath,
    const QString& outputDir,
    const PaddleDocLayoutModelConfig& config) {
    QJsonObject report = PaddleDocLayoutEngine::configReport(config);
    report.insert(QStringLiteral("ok"), false);
    report.insert(QStringLiteral("error"), error);
    report.insert(QStringLiteral("input_path"), imagePath);
    report.insert(QStringLiteral("output_dir"), outputDir);
    return report;
}

LayoutModelMeta loadModelMeta(const QString& resolvedModelDir, const PaddleDocLayoutModelConfig& config) {
    LayoutModelMeta meta;
    meta.modelName = config.modelName;
    meta.drawThreshold = config.threshold;
    meta.labels = {
        QStringLiteral("paragraph_title"),
        QStringLiteral("image"),
        QStringLiteral("text"),
        QStringLiteral("number"),
        QStringLiteral("abstract"),
        QStringLiteral("content"),
        QStringLiteral("figure_title"),
        QStringLiteral("formula"),
        QStringLiteral("table"),
        QStringLiteral("table_title"),
        QStringLiteral("reference"),
        QStringLiteral("doc_title"),
        QStringLiteral("footnote"),
        QStringLiteral("header"),
        QStringLiteral("algorithm"),
        QStringLiteral("footer"),
        QStringLiteral("seal"),
        QStringLiteral("chart_title"),
        QStringLiteral("chart"),
        QStringLiteral("formula_number"),
        QStringLiteral("header_image"),
        QStringLiteral("footer_image"),
        QStringLiteral("aside_text"),
    };

    const QString configPath = inferenceConfigFile(resolvedModelDir);
    if (configPath.isEmpty()) {
        return meta;
    }
    const YAML::Node root = YAML::LoadFile(nativePathString(configPath));
    const YAML::Node global = root["Global"];
    if (global && global["model_name"]) {
        meta.modelName = QString::fromStdString(global["model_name"].as<std::string>());
    }
    if (root["draw_threshold"]) {
        meta.drawThreshold = root["draw_threshold"].as<double>();
    }
    const YAML::Node preprocess = root["Preprocess"];
    if (preprocess && preprocess.IsSequence()) {
        for (const auto& step : preprocess) {
            if (!step["type"] || step["type"].as<std::string>() != "Resize" || !step["target_size"]) {
                continue;
            }
            const auto target = step["target_size"];
            if (target.IsSequence() && target.size() >= 2) {
                meta.targetHeight = target[0].as<int>();
                meta.targetWidth = target[1].as<int>();
            }
        }
    }
    const YAML::Node labelList = root["label_list"];
    if (labelList && labelList.IsSequence()) {
        meta.labels.clear();
        for (const auto& label : labelList) {
            meta.labels.append(QString::fromStdString(label.as<std::string>()));
        }
    }
    return meta;
}

void validateConfig(const QString& imagePath, const PaddleDocLayoutModelConfig& config) {
    if (!QFileInfo::exists(imagePath)) {
        throw std::runtime_error(QStringLiteral("image does not exist: %1").arg(imagePath).toStdString());
    }
    if (useGpuDevice(config.device) && !PaddleInferenceRuntime::gpuSupported()) {
        throw std::runtime_error(QStringLiteral("PP-DocLayout GPU inference requires a Paddle Inference SDK built with WITH_GPU=ON.").toStdString());
    }
    QString reason;
    if (!PaddleInferenceRuntime::sdkAvailable(&reason)) {
        throw std::runtime_error(reason.toStdString());
    }
    if (!PaddleDocLayoutEngine::modelDirLooksUsable(config.modelDir, &reason)) {
        throw std::runtime_error(QStringLiteral("layout model is not usable: %1").arg(reason).toStdString());
    }
}

std::shared_ptr<paddle_infer::Predictor> createPredictor(const QString& resolvedModelDir, const PaddleDocLayoutModelConfig& config) {
    const QString modelFile = inferenceModelFile(resolvedModelDir);
    const QString paramsFile = QDir(resolvedModelDir).filePath(QStringLiteral("inference.pdiparams"));
    if (modelFile.isEmpty() || !QFileInfo::exists(paramsFile)) {
        throw std::runtime_error(QStringLiteral("missing inference model files in %1").arg(resolvedModelDir).toStdString());
    }

    paddle_infer::Config paddleConfig;
    paddleConfig.SetModel(nativePathString(modelFile), nativePathString(paramsFile));
    if (useGpuDevice(config.device)) {
        paddleConfig.DisableMKLDNN();
        paddleConfig.EnableUseGpu(
            100,
            deviceIdFromString(config.device),
            paddle_infer::PrecisionType::kFloat32);
        paddleConfig.EnableNewIR(true);
        paddleConfig.EnableNewExecutor();
        paddleConfig.SetOptimizationLevel(3);
    } else {
        paddleConfig.DisableGpu();
        if (config.enableMkldnn) {
        paddleConfig.EnableMKLDNN();
        paddleConfig.SetMkldnnCacheCapacity(config.mkldnnCacheCapacity);
        } else {
            paddleConfig.DisableMKLDNN();
        }
        paddleConfig.SetCpuMathLibraryNumThreads(config.cpuThreads);
    }
    paddleConfig.DisableGlogInfo();
    return paddle_infer::CreatePredictor(paddleConfig);
}

std::vector<float> preprocessImage(const cv::Mat& image, int targetWidth, int targetHeight) {
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(targetWidth, targetHeight), 0.0, 0.0, cv::INTER_CUBIC);

    const float mean[3] = {0.485f, 0.456f, 0.406f};
    const float stdv[3] = {0.229f, 0.224f, 0.225f};
    std::vector<float> input(static_cast<size_t>(3 * targetHeight * targetWidth));
    for (int y = 0; y < targetHeight; ++y) {
        const auto* row = resized.ptr<cv::Vec3b>(y);
        for (int x = 0; x < targetWidth; ++x) {
            const cv::Vec3b pixel = row[x];
            for (int c = 0; c < 3; ++c) {
                const float value = static_cast<float>(pixel[c]) / 255.0f;
                input[static_cast<size_t>(c * targetHeight * targetWidth + y * targetWidth + x)] =
                    (value - mean[c]) / stdv[c];
            }
        }
    }
    return input;
}

std::vector<LayoutBox> decodeBoxes(
    const std::vector<float>& raw,
    const std::vector<int>& shape,
    const QStringList& labels,
    int imageWidth,
    int imageHeight,
    double threshold) {
    if (shape.size() < 2 || shape.back() < 6) {
        throw std::runtime_error(QStringLiteral("unexpected PP-DocLayout output shape").toStdString());
    }
    const int rowWidth = shape.back();
    const int rowCount = static_cast<int>(raw.size() / rowWidth);
    std::vector<LayoutBox> boxes;
    for (int row = 0; row < rowCount; ++row) {
        const float* values = raw.data() + static_cast<size_t>(row * rowWidth);
        const int classId = static_cast<int>(std::round(values[0]));
        const double score = values[1];
        if (classId < 0 || score <= threshold) {
            continue;
        }
        const double left = std::clamp<double>(std::round(values[2]), 0.0, imageWidth);
        const double top = std::clamp<double>(std::round(values[3]), 0.0, imageHeight);
        const double right = std::clamp<double>(std::round(values[4]), 0.0, imageWidth);
        const double bottom = std::clamp<double>(std::round(values[5]), 0.0, imageHeight);
        if (right <= left || bottom <= top) {
            continue;
        }
        LayoutBox box;
        box.classId = classId;
        box.label = labelForId(labels, classId);
        box.score = score;
        box.left = left;
        box.top = top;
        box.right = right;
        box.bottom = bottom;
        box.order = static_cast<int>(boxes.size()) + 1;
        boxes.push_back(box);
    }
    return boxes;
}

std::vector<LayoutBox> runPredictor(
    paddle_infer::Predictor* predictor,
    const cv::Mat& image,
    const LayoutModelMeta& meta,
    double threshold) {
    const auto inputNames = predictor->GetInputNames();
    if (std::find(inputNames.begin(), inputNames.end(), "image") == inputNames.end()
        || std::find(inputNames.begin(), inputNames.end(), "scale_factor") == inputNames.end()) {
        throw std::runtime_error(QStringLiteral("PP-DocLayout model must expose image and scale_factor inputs").toStdString());
    }

    std::vector<float> imageTensor = preprocessImage(image, meta.targetWidth, meta.targetHeight);
    std::vector<float> scaleTensor{
        static_cast<float>(meta.targetHeight) / static_cast<float>(image.rows),
        static_cast<float>(meta.targetWidth) / static_cast<float>(image.cols),
    };

    auto imageHandle = predictor->GetInputHandle("image");
    imageHandle->Reshape({1, 3, meta.targetHeight, meta.targetWidth});
    imageHandle->CopyFromCpu(imageTensor.data());

    auto scaleHandle = predictor->GetInputHandle("scale_factor");
    scaleHandle->Reshape({1, 2});
    scaleHandle->CopyFromCpu(scaleTensor.data());

    if (!predictor->Run()) {
        throw std::runtime_error(QStringLiteral("Paddle Inference Run() returned false for PP-DocLayout").toStdString());
    }

    const auto outputNames = predictor->GetOutputNames();
    for (const auto& name : outputNames) {
        auto output = predictor->GetOutputHandle(name);
        const std::vector<int> shape = output->shape();
        if (shape.empty() || shape.back() != 6) {
            continue;
        }
        size_t total = 1;
        for (const int dim : shape) {
            if (dim > 0) {
                total *= static_cast<size_t>(dim);
            }
        }
        std::vector<float> values(total);
        output->CopyToCpu(values.data());
        return decodeBoxes(values, shape, meta.labels, image.cols, image.rows, threshold);
    }
    throw std::runtime_error(QStringLiteral("PP-DocLayout did not produce a [N, 6] boxes output").toStdString());
}

void saveVisualization(const QString& path, const cv::Mat& sourceImage, const std::vector<LayoutBox>& boxes) {
    cv::Mat visual = sourceImage.clone();
    const std::vector<cv::Scalar> colors{
        {92, 190, 255},
        {118, 178, 61},
        {223, 215, 115},
        {255, 167, 106},
        {206, 118, 50},
        {180, 120, 220},
    };
    for (const auto& box : boxes) {
        const cv::Scalar color = colors[static_cast<size_t>(std::max(0, box.classId)) % colors.size()];
        cv::rectangle(
            visual,
            cv::Rect(
                cv::Point(static_cast<int>(box.left), static_cast<int>(box.top)),
                cv::Point(static_cast<int>(box.right), static_cast<int>(box.bottom))),
            color,
            2);
        const QString label = QStringLiteral("%1 %.2f").arg(box.label).arg(box.score);
        cv::putText(
            visual,
            label.toStdString(),
            cv::Point(static_cast<int>(box.left), std::max(14, static_cast<int>(box.top) - 5)),
            cv::FONT_HERSHEY_SIMPLEX,
            0.45,
            color,
            1,
            cv::LINE_AA);
    }
    cv::imwrite(nativePathString(path), visual);
}

void writeJson(const QString& path, const QJsonObject& object) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        throw std::runtime_error(QStringLiteral("failed to write result JSON: %1").arg(path).toStdString());
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
}

QJsonObject reportForImage(
    const QString& imagePath,
    const QString& outputDir,
    const QString& resultJsonPath,
    const QString& visualPath,
    const PaddleDocLayoutModelConfig& config,
    const LayoutModelMeta& meta,
    const std::vector<LayoutBox>& boxes,
    bool visualizationSaved) {
    QJsonObject report = PaddleDocLayoutEngine::configReport(config);
    const QJsonArray boxValues = boxesArray(boxes);
    report.insert(QStringLiteral("ok"), true);
    report.insert(QStringLiteral("engine"), QStringLiteral("PaddleInference/PP-DocLayout/direct_cpp"));
    report.insert(QStringLiteral("backend"), QStringLiteral("paddle_inference_direct_cpp"));
    report.insert(QStringLiteral("input_path"), imagePath);
    report.insert(QStringLiteral("output_dir"), outputDir);
    report.insert(QStringLiteral("result_json"), resultJsonPath);
    report.insert(QStringLiteral("visual_path"), visualizationSaved ? visualPath : QString());
    report.insert(QStringLiteral("model_name"), meta.modelName);
    report.insert(QStringLiteral("threshold"), config.threshold);
    report.insert(QStringLiteral("box_count"), static_cast<int>(boxes.size()));
    report.insert(QStringLiteral("boxes"), boxValues);
    report.insert(QStringLiteral("res"), QJsonObject{
        {QStringLiteral("input_path"), imagePath},
        {QStringLiteral("model_settings"), QJsonObject{
            {QStringLiteral("use_doc_preprocessor"), false},
            {QStringLiteral("use_layout_detection"), true},
        }},
        {QStringLiteral("layout_det_res"), QJsonObject{
            {QStringLiteral("input_path"), imagePath},
            {QStringLiteral("boxes"), boxValues},
        }},
        {QStringLiteral("boxes"), boxValues},
    });
    return report;
}

}  // namespace

QString PaddleDocLayoutEngine::projectRoot() {
    return PaddleOcrEngine::projectRoot();
}

PaddleDocLayoutModelConfig PaddleDocLayoutEngine::defaultConfig(const QString& baseDir) {
    const QDir root(baseDir.isEmpty() ? projectRoot() : baseDir);
    const QDir inferRoot(root.filePath(QStringLiteral("model/infer")));
    PaddleDocLayoutModelConfig config;
    config.modelName = QStringLiteral("PP-DocLayout-S");
    config.modelDir = inferRoot.filePath(QStringLiteral("PP-DocLayout-S_infer/PP-DocLayout-S_infer"));
    return config;
}

bool PaddleDocLayoutEngine::modelDirLooksUsable(const QString& modelDir, QString* reason) {
    if (!PaddleInferenceRuntime::modelDirLooksUsable(modelDir, reason)) {
        return false;
    }
    const QString resolved = PaddleInferenceRuntime::resolveModelDir(modelDir);
    if (!QFileInfo::exists(QDir(resolved).filePath(QStringLiteral("inference.yml")))) {
        if (reason) {
            *reason = QStringLiteral("missing inference.yml in %1").arg(resolved);
        }
        return false;
    }
    return true;
}

QJsonObject PaddleDocLayoutEngine::configReport(const PaddleDocLayoutModelConfig& config) {
    return {
        {QStringLiteral("engine"), QStringLiteral("PaddleInference/PP-DocLayout/direct_cpp")},
        {QStringLiteral("backend"), QStringLiteral("paddle_inference_direct_cpp")},
        {QStringLiteral("sdk_root"), PaddleInferenceRuntime::sdkRoot()},
        {QStringLiteral("paddle_version"), PaddleInferenceRuntime::version()},
        {QStringLiteral("model_name"), config.modelName},
        {QStringLiteral("model_dir"), config.modelDir},
        {QStringLiteral("device"), config.device},
        {QStringLiteral("precision"), config.precision},
        {QStringLiteral("enable_mkldnn"), config.enableMkldnn},
        {QStringLiteral("cpu_threads"), config.cpuThreads},
        {QStringLiteral("threshold"), config.threshold},
    };
}

QJsonObject PaddleDocLayoutEngine::predictImage(
    const QString& imagePath,
    const QString& outputDir,
    const PaddleDocLayoutModelConfig& config,
    bool shouldSaveVisualization) {
    const auto reports = predictImages({qMakePair(imagePath, outputDir)}, config, shouldSaveVisualization);
    return reports.isEmpty()
        ? errorReport(QStringLiteral("layout prediction finished but no report was produced"), imagePath, outputDir, config)
        : reports.first();
}

QList<QJsonObject> PaddleDocLayoutEngine::predictImages(
    const QList<QPair<QString, QString>>& imageJobs,
    const PaddleDocLayoutModelConfig& config,
    bool shouldSaveVisualization,
    const std::function<void(int current, int total, const QString& imagePath)>& progress) {
    QList<QJsonObject> reports;
    reports.reserve(imageJobs.size());
    QVector<QString> outputDirs(imageJobs.size());
    QVector<int> activeIndexes;
    activeIndexes.reserve(imageJobs.size());

    PaddleDocLayoutModelConfig resolvedConfig = config;
    resolvedConfig.modelDir = PaddleInferenceRuntime::resolveModelDir(config.modelDir);
    for (int i = 0; i < imageJobs.size(); ++i) {
        reports.append(QJsonObject{});
        const QString imagePath = imageJobs.at(i).first;
        try {
            outputDirs[i] = ensureOutputDir(imageJobs.at(i).second);
            validateConfig(imagePath, resolvedConfig);
            activeIndexes.append(i);
        } catch (const std::exception& exc) {
            reports[i] = errorReport(QString::fromUtf8(exc.what()), imagePath, outputDirs.value(i), resolvedConfig);
        }
    }

    if (activeIndexes.isEmpty()) {
        return reports;
    }

    try {
        const LayoutModelMeta meta = loadModelMeta(resolvedConfig.modelDir, resolvedConfig);
        const double threshold = std::isfinite(resolvedConfig.threshold) ? resolvedConfig.threshold : meta.drawThreshold;
        auto predictor = createPredictor(resolvedConfig.modelDir, resolvedConfig);

        for (int pos = 0; pos < activeIndexes.size(); ++pos) {
            const int jobIndex = activeIndexes.at(pos);
            const QString imagePath = imageJobs.at(jobIndex).first;
            if (progress) {
                progress(pos + 1, activeIndexes.size(), imagePath);
            }
            try {
                cv::Mat image = cv::imread(nativePathString(imagePath), cv::IMREAD_COLOR);
                if (image.empty()) {
                    throw std::runtime_error(QStringLiteral("cannot read image: %1").arg(imagePath).toStdString());
                }

                const std::vector<LayoutBox> boxes = runPredictor(predictor.get(), image, meta, threshold);
                const QString resultJsonPath = QDir(outputDirs.value(jobIndex)).filePath(completeBaseName(imagePath) + QStringLiteral("_res.json"));
                const QString visualPath = QDir(outputDirs.value(jobIndex)).filePath(completeBaseName(imagePath) + QStringLiteral("_layout.jpg"));
                if (shouldSaveVisualization) {
                    saveVisualization(visualPath, image, boxes);
                }
                QJsonObject report = reportForImage(
                    QFileInfo(imagePath).absoluteFilePath(),
                    outputDirs.value(jobIndex),
                    resultJsonPath,
                    visualPath,
                    resolvedConfig,
                    meta,
                    boxes,
                    shouldSaveVisualization);
                report.insert(QStringLiteral("threshold"), threshold);
                writeJson(resultJsonPath, report);
                reports[jobIndex] = report;
            } catch (const std::exception& exc) {
                reports[jobIndex] = errorReport(QString::fromUtf8(exc.what()), imagePath, outputDirs.value(jobIndex), resolvedConfig);
            }
        }
    } catch (const std::exception& exc) {
        for (const int jobIndex : activeIndexes) {
            reports[jobIndex] = errorReport(
                QString::fromUtf8(exc.what()),
                imageJobs.at(jobIndex).first,
                outputDirs.value(jobIndex),
                resolvedConfig);
        }
    }
    return reports;
}

QJsonObject PaddleDocLayoutEngine::predictPath(
    const QString& inputPath,
    const QString& outputDir,
    const PaddleDocLayoutModelConfig& config,
    bool saveVisualization) {
    const QStringList images = collectImages(inputPath);
    QList<QPair<QString, QString>> jobs;
    jobs.reserve(images.size());
    for (const auto& imagePath : images) {
        jobs.append(qMakePair(imagePath, outputDir));
    }
    const QList<QJsonObject> reports = predictImages(jobs, config, saveVisualization);
    QJsonArray results;
    int okCount = 0;
    for (const auto& report : reports) {
        if (report.value(QStringLiteral("ok")).toBool()) {
            ++okCount;
        }
        results.append(report);
    }
    return {
        {QStringLiteral("ok"), !images.isEmpty() && okCount == images.size()},
        {QStringLiteral("engine"), QStringLiteral("PaddleInference/PP-DocLayout/direct_cpp")},
        {QStringLiteral("backend"), QStringLiteral("paddle_inference_direct_cpp")},
        {QStringLiteral("input_path"), inputPath},
        {QStringLiteral("output_dir"), outputDir},
        {QStringLiteral("total"), images.size()},
        {QStringLiteral("ok_count"), okCount},
        {QStringLiteral("failed_count"), images.size() - okCount},
        {QStringLiteral("config"), configReport(config)},
        {QStringLiteral("results"), results},
    };
}

}  // namespace ppocr
