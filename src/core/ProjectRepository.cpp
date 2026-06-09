#include "core/ProjectRepository.h"

#include "core/Constants.h"
#include "core/JsonUtils.h"

#include <QDateTime>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <algorithm>
#include <stdexcept>

namespace ppocr {

QStringList ProjectRepository::projectDirs() {
    return {
        "assets/raw",
        "assets/pages",
        "assets/thumbs",
        "annotations",
        "crops/rec_train",
        "crops/rec_val",
        "crops/preview",
        "exports/ppocr_det",
        "exports/ppocr_rec",
        "exports/ppocr_cls",
        "exports/ppocr_textline_cls",
        "exports/table_classification",
        "exports/coco_layout",
        "training",
        "cache/ocr_prelabel",
        "cache/cls_prelabel",
        "cache/table_cls_prelabel",
        "cache/layout_prelabel",
        "cache/prediction_staging",
        "cache/prediction_clipboard",
        "predictions",
        "logs",
    };
}

ProjectContext ProjectRepository::createProject(const QString& projectDir, const QString& projectName) {
    ProjectContext context;
    context.root = QDir(QFileInfo(projectDir).absoluteFilePath());
    for (const auto& rel : projectDirs()) {
        QDir().mkpath(context.path(rel));
    }

    context.config = {
        {"project_name", projectName.isEmpty() ? context.root.dirName() : projectName},
        {"version", kProjectVersion},
        {"offline_mode", true},
        {"created_at", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")},
        {"image_root", "assets/pages"},
        {"annotation_root", "annotations"},
        {"crop_root", "crops"},
        {"export_root", "exports"},
        {"label_sets", defaultLabelSets()},
        {"splits", QJsonObject{{"train_ratio", 0.8}, {"val_ratio", 0.2}, {"strategy", "manual"}}},
        {"local_models", QJsonObject{
            {"det_model_dir", ""},
            {"rec_model_dir", ""},
            {"cls_model_dir", ""},
            {"doc_orientation_model_dir", ""},
            {"doc_unwarping_model_dir", ""},
            {"layout_model_dir", ""},
            {"use_gpu", false},
            {"enable_cls", false},
            {"enable_doc_orientation", false},
            {"enable_doc_unwarping", false},
        }},
        {"ui_state", QJsonObject{}},
    };
    saveProject(context);
    return context;
}

ProjectContext ProjectRepository::openProject(const QString& projectDir) {
    ProjectContext context;
    context.root = QDir(QFileInfo(projectDir).absoluteFilePath());
    const QString configPath = context.path("project.json");
    if (!QFileInfo::exists(configPath)) {
        throw std::runtime_error(QString("project.json not found: %1").arg(configPath).toStdString());
    }
    QString error;
    if (!readJsonObject(configPath, &context.config, &error)) {
        throw std::runtime_error(QString("cannot read project.json: %1").arg(error).toStdString());
    }
    for (const auto& rel : projectDirs()) {
        QDir().mkpath(context.path(rel));
    }
    return context;
}

void ProjectRepository::saveProject(const ProjectContext& context) {
    writeJsonObject(context.path("project.json"), context.config);
}

QList<PageInfo> ProjectRepository::listPages(const ProjectContext& context) {
    QList<PageInfo> pages;
    QDir imageDir(context.imageRoot());
    const auto entries = imageDir.entryInfoList({"page_*.jpg"}, QDir::Files, QDir::Name);
    for (const auto& imageInfo : entries) {
        const QString assetId = imageInfo.completeBaseName();
        const QString annotationPath = QDir(context.annotationRoot()).filePath(assetId + ".json");
        QJsonObject annotation;
        int width = 0;
        int height = 0;
        QString split = "train";
        QString status = "unlabeled";
        int pageIndex = pages.size() + 1;
        if (QFileInfo::exists(annotationPath)) {
            annotation = readAnnotation(annotationPath);
            width = annotation.value("width").toInt();
            height = annotation.value("height").toInt();
            split = annotation.value("split").toString("train");
            status = annotation.value("status").toString("unlabeled");
            pageIndex = annotation.value("page_index").toInt(pageIndex);
        } else {
            const QSize size = imageSize(imageInfo.absoluteFilePath());
            width = size.width();
            height = size.height();
            annotation = defaultAnnotation(
                assetId,
                normalizedRelativePath(context.root, imageInfo.absoluteFilePath()),
                width,
                height,
                pageIndex,
                split);
            writeAnnotation(annotationPath, annotation);
        }
        pages.append(PageInfo{
            assetId,
            imageInfo.absoluteFilePath(),
            normalizedRelativePath(context.root, imageInfo.absoluteFilePath()),
            width,
            height,
            pageIndex,
            split,
            status,
            annotationPath,
        });
    }
    return pages;
}

QString ProjectRepository::nextAssetId(const ProjectContext& context) {
    int maxIndex = 0;
    QDir imageDir(context.imageRoot());
    const auto entries = imageDir.entryInfoList({"page_*.jpg"}, QDir::Files, QDir::Name);
    for (const auto& entry : entries) {
        const auto parts = entry.completeBaseName().split('_');
        bool ok = false;
        const int index = parts.isEmpty() ? 0 : parts.last().toInt(&ok);
        if (ok) {
            maxIndex = std::max(maxIndex, index);
        }
    }
    return QString("page_%1").arg(maxIndex + 1, 6, 10, QLatin1Char('0'));
}

QSize ProjectRepository::imageSize(const QString& path) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    return reader.size();
}

QJsonObject ProjectRepository::readAnnotation(const QString& path) {
    QJsonObject object;
    QString error;
    if (!readJsonObject(path, &object, &error)) {
        throw std::runtime_error(QString("cannot read annotation %1: %2").arg(path, error).toStdString());
    }
    return object;
}

void ProjectRepository::writeAnnotation(const QString& path, const QJsonObject& annotation) {
    writeJsonObject(path, annotation);
}

QJsonObject ProjectRepository::defaultAnnotation(
    const QString& assetId,
    const QString& relativeImagePath,
    int width,
    int height,
    int pageIndex,
    const QString& split) {
    return {
        {"asset_id", assetId},
        {"image_path", QString(relativeImagePath).replace("\\", "/")},
        {"width", width},
        {"height", height},
        {"page_index", pageIndex},
        {"split", split},
        {"status", "unlabeled"},
        {"image_labels", QJsonArray{
            QJsonObject{{"task", "doc_orientation"}, {"label", ""}},
            QJsonObject{{"task", "textline_orientation"}, {"label", ""}},
            QJsonObject{{"task", "table_classification"}, {"label", ""}},
        }},
        {"regions", QJsonArray{}},
        {"rec_crops", QJsonArray{}},
        {"validation", QJsonObject{{"passed", false}, {"errors", QJsonArray{}}, {"warnings", QJsonArray{}}}},
    };
}

}  // namespace ppocr
