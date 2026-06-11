#pragma once

#include <QDir>
#include <QJsonObject>
#include <QString>
#include <optional>

namespace ppocr {

enum class PageSplit {
    Train,
    Val,
    Test,
    Unassigned,
};

enum class PageStatus {
    Imported,
    Prelabeled,
    Labeled,
    Checked,
    Exported,
    Error,
    Unlabeled,
};

enum class RegionType {
    OcrText,
    Layout,
};

enum class AnnotationSource {
    Manual,
    OcrPrelabel,
    LayoutPrelabel,
    Imported,
};

enum class TrainingTaskKind {
    OcrDet,
    OcrRec,
    DocCls,
    TextlineCls,
    TableCls,
    Layout,
    Unknown,
};

enum class RunStatus {
    Pending,
    Running,
    Finished,
    Failed,
    Cancelled,
};

enum class PredictionStatus {
    Pending,
    Running,
    Finished,
    Failed,
};

QString toString(PageSplit split);
std::optional<PageSplit> pageSplitFromString(const QString& value);

QString toString(PageStatus status);
std::optional<PageStatus> pageStatusFromString(const QString& value);

QString toString(RegionType type);
std::optional<RegionType> regionTypeFromString(const QString& value);

QString toString(AnnotationSource source);
std::optional<AnnotationSource> annotationSourceFromString(const QString& value);

QString toString(TrainingTaskKind kind);
std::optional<TrainingTaskKind> trainingTaskKindFromString(const QString& value);
QString trainingTaskKindGroupKey(TrainingTaskKind kind);
bool isClassificationTrainingTaskKind(TrainingTaskKind kind);

QString toString(RunStatus status);
std::optional<RunStatus> runStatusFromString(const QString& value);

QString toString(PredictionStatus status);
std::optional<PredictionStatus> predictionStatusFromString(const QString& value);

struct PageInfo {
    QString assetId;
    QString imagePath;
    QString relativeImagePath;
    int width = 0;
    int height = 0;
    int pageIndex = 0;
    PageSplit split = PageSplit::Train;
    PageStatus status = PageStatus::Unlabeled;
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
