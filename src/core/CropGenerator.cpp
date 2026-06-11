#include "core/CropGenerator.h"

#include "core/JsonUtils.h"
#include "core/ProjectTypes.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <algorithm>
#include <cmath>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace ppocr {

static cv::Mat readMat(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray bytes = file.readAll();
    const std::vector<uchar> data(bytes.begin(), bytes.end());
    return cv::imdecode(data, cv::IMREAD_COLOR);
}

static bool writeMat(const QString& path, const cv::Mat& image) {
    QFileInfo info(path);
    QDir().mkpath(info.absolutePath());
    std::vector<uchar> encoded;
    if (!cv::imencode(".jpg", image, encoded)) {
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(reinterpret_cast<const char*>(encoded.data()), static_cast<qint64>(encoded.size()));
    return true;
}

static std::vector<cv::Point2f> orderPoints(const QJsonArray& rawPoints) {
    std::vector<cv::Point2f> points;
    for (const auto& value : rawPoints) {
        const auto point = value.toArray();
        if (point.size() >= 2) {
            points.emplace_back(static_cast<float>(point.at(0).toDouble()), static_cast<float>(point.at(1).toDouble()));
        }
    }
    if (points.size() < 4) {
        return {};
    }
    points.resize(4);
    std::sort(points.begin(), points.end(), [](const cv::Point2f& a, const cv::Point2f& b) {
        return (a.x + a.y) < (b.x + b.y);
    });
    cv::Point2f tl = points.front();
    cv::Point2f br = points.back();
    cv::Point2f p1 = points[1];
    cv::Point2f p2 = points[2];
    cv::Point2f tr = (p1.x - p1.y > p2.x - p2.y) ? p1 : p2;
    cv::Point2f bl = (p1.x - p1.y > p2.x - p2.y) ? p2 : p1;
    return {tl, tr, br, bl};
}

static cv::Mat cropQuad(const cv::Mat& image, const QJsonArray& rawPoints) {
    const auto points = orderPoints(rawPoints);
    if (points.size() != 4 || image.empty()) {
        return {};
    }
    const double widthA = cv::norm(points[2] - points[3]);
    const double widthB = cv::norm(points[1] - points[0]);
    const double heightA = cv::norm(points[1] - points[2]);
    const double heightB = cv::norm(points[0] - points[3]);
    const int width = std::max(1, static_cast<int>(std::round(std::max(widthA, widthB))));
    const int height = std::max(1, static_cast<int>(std::round(std::max(heightA, heightB))));
    std::vector<cv::Point2f> dst = {
        {0.0f, 0.0f},
        {static_cast<float>(width - 1), 0.0f},
        {static_cast<float>(width - 1), static_cast<float>(height - 1)},
        {0.0f, static_cast<float>(height - 1)},
    };
    const cv::Mat transform = cv::getPerspectiveTransform(points, dst);
    cv::Mat warped;
    cv::warpPerspective(image, warped, transform, cv::Size(width, height), cv::INTER_CUBIC);
    return warped;
}

QJsonObject CropGenerator::generateRecCrops(const ProjectContext& context, const QJsonObject& annotation) {
    QJsonObject output = annotation;
    const QString imagePath = context.path(annotation.value("image_path").toString());
    const cv::Mat image = readMat(imagePath);
    QJsonArray crops;
    for (const auto& value : annotation.value("regions").toArray()) {
        const auto region = value.toObject();
        if (region.value("type").toString() != "ocr_text" || region.value("ignore").toBool()) {
            continue;
        }
        const QString regionId = region.value("id").toString();
        cv::Mat crop = cropQuad(image, region.value("points").toArray());
        if (crop.empty()) {
            continue;
        }
        const auto split = pageSplitFromString(annotation.value("split").toString("train")).value_or(PageSplit::Unassigned);
        const QString subdir = split == PageSplit::Train ? "rec_train" : "rec_val";
        const QString fileName = QString("%1_%2.jpg").arg(annotation.value("asset_id").toString(), regionId);
        const QString relative = QString("crops/%1/%2").arg(subdir, fileName);
        const QString absolute = context.path(relative);
        if (!writeMat(absolute, crop)) {
            continue;
        }
        crops.append(QJsonObject{
            {"region_id", regionId},
            {"crop_path", relative},
            {"text", region.value("text").toString()},
            {"checked", region.value("checked").toBool(false)},
        });
    }
    output["rec_crops"] = crops;
    return output;
}

}  // namespace ppocr
