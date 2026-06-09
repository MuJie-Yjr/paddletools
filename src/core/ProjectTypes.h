#pragma once

#include <QDir>
#include <QJsonObject>
#include <QString>

namespace ppocr {

struct PageInfo {
    QString assetId;
    QString imagePath;
    QString relativeImagePath;
    int width = 0;
    int height = 0;
    int pageIndex = 0;
    QString split = "train";
    QString status = "unlabeled";
    QString annotationPath;
};

struct ProjectContext {
    QDir root;
    QJsonObject config;

    QString path(const QString& relativePath) const {
        return root.filePath(relativePath);
    }

    QString imageRoot() const {
        return path(config.value("image_root").toString("assets/pages"));
    }

    QString rawRoot() const {
        return path("assets/raw");
    }

    QString thumbRoot() const {
        return path("assets/thumbs");
    }

    QString annotationRoot() const {
        return path(config.value("annotation_root").toString("annotations"));
    }

    QString cropRoot() const {
        return path(config.value("crop_root").toString("crops"));
    }

    QString exportRoot() const {
        return path(config.value("export_root").toString("exports"));
    }

    QString logRoot() const {
        return path("logs");
    }
};

struct ValidationIssue {
    QString severity;
    QString task;
    QString assetId;
    QString message;
    QString location;
    QString regionId;
};

}  // namespace ppocr
