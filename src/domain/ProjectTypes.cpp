#include "domain/ProjectTypes.h"

namespace ppocr {

namespace {

template <typename Enum>
std::optional<Enum> optionalIf(bool condition, Enum value) {
    if (condition) {
        return value;
    }
    return std::nullopt;
}

}  // namespace

QString toString(PageSplit split) {
    switch (split) {
    case PageSplit::Train:
        return QStringLiteral("train");
    case PageSplit::Val:
        return QStringLiteral("val");
    case PageSplit::Test:
        return QStringLiteral("test");
    case PageSplit::Unassigned:
        return QStringLiteral("unassigned");
    }
    return QStringLiteral("unassigned");
}

std::optional<PageSplit> pageSplitFromString(const QString& value) {
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("train")) {
        return PageSplit::Train;
    }
    if (normalized == QStringLiteral("val")) {
        return PageSplit::Val;
    }
    if (normalized == QStringLiteral("test")) {
        return PageSplit::Test;
    }
    if (normalized == QStringLiteral("unassigned")) {
        return PageSplit::Unassigned;
    }
    return std::nullopt;
}

QString toString(PageStatus status) {
    switch (status) {
    case PageStatus::Imported:
        return QStringLiteral("imported");
    case PageStatus::Prelabeled:
        return QStringLiteral("prelabeled");
    case PageStatus::Labeled:
        return QStringLiteral("labeled");
    case PageStatus::Checked:
        return QStringLiteral("checked");
    case PageStatus::Exported:
        return QStringLiteral("exported");
    case PageStatus::Error:
        return QStringLiteral("error");
    case PageStatus::Unlabeled:
        return QStringLiteral("unlabeled");
    }
    return QStringLiteral("unlabeled");
}

std::optional<PageStatus> pageStatusFromString(const QString& value) {
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("imported")) {
        return PageStatus::Imported;
    }
    if (normalized == QStringLiteral("prelabeled")) {
        return PageStatus::Prelabeled;
    }
    if (normalized == QStringLiteral("labeled")) {
        return PageStatus::Labeled;
    }
    if (normalized == QStringLiteral("checked") || normalized == QStringLiteral("validated")) {
        return PageStatus::Checked;
    }
    if (normalized == QStringLiteral("exported")) {
        return PageStatus::Exported;
    }
    if (normalized == QStringLiteral("error")) {
        return PageStatus::Error;
    }
    if (normalized == QStringLiteral("unlabeled") || normalized.isEmpty()) {
        return PageStatus::Unlabeled;
    }
    return std::nullopt;
}

QString toString(RegionType type) {
    switch (type) {
    case RegionType::OcrText:
        return QStringLiteral("ocr_text");
    case RegionType::Layout:
        return QStringLiteral("layout");
    }
    return QStringLiteral("ocr_text");
}

std::optional<RegionType> regionTypeFromString(const QString& value) {
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("ocr_text") || normalized == QStringLiteral("ocr")) {
        return RegionType::OcrText;
    }
    if (normalized == QStringLiteral("layout")) {
        return RegionType::Layout;
    }
    return std::nullopt;
}

QString toString(AnnotationSource source) {
    switch (source) {
    case AnnotationSource::Manual:
        return QStringLiteral("manual");
    case AnnotationSource::OcrPrelabel:
        return QStringLiteral("ocr_prelabel");
    case AnnotationSource::LayoutPrelabel:
        return QStringLiteral("layout_prelabel");
    case AnnotationSource::Imported:
        return QStringLiteral("imported");
    }
    return QStringLiteral("manual");
}

std::optional<AnnotationSource> annotationSourceFromString(const QString& value) {
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("manual")) {
        return AnnotationSource::Manual;
    }
    if (normalized == QStringLiteral("ocr_prelabel") || normalized == QStringLiteral("auto")) {
        return AnnotationSource::OcrPrelabel;
    }
    if (normalized == QStringLiteral("layout_prelabel")) {
        return AnnotationSource::LayoutPrelabel;
    }
    if (normalized == QStringLiteral("imported")) {
        return AnnotationSource::Imported;
    }
    return std::nullopt;
}

QString toString(TrainingTaskKind kind) {
    switch (kind) {
    case TrainingTaskKind::OcrDet:
        return QStringLiteral("ocr_det");
    case TrainingTaskKind::OcrRec:
        return QStringLiteral("ocr_rec");
    case TrainingTaskKind::DocCls:
        return QStringLiteral("doc_cls");
    case TrainingTaskKind::TextlineCls:
        return QStringLiteral("textline_cls");
    case TrainingTaskKind::TableCls:
        return QStringLiteral("table_cls");
    case TrainingTaskKind::Layout:
        return QStringLiteral("layout");
    case TrainingTaskKind::Unknown:
        return QStringLiteral("uvdoc");
    }
    return QStringLiteral("uvdoc");
}

std::optional<TrainingTaskKind> trainingTaskKindFromString(const QString& value) {
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("det") || normalized == QStringLiteral("ocr_det")) {
        return TrainingTaskKind::OcrDet;
    }
    if (normalized == QStringLiteral("rec") || normalized == QStringLiteral("ocr_rec")) {
        return TrainingTaskKind::OcrRec;
    }
    if (normalized == QStringLiteral("cls") || normalized == QStringLiteral("doc_cls")) {
        return TrainingTaskKind::DocCls;
    }
    if (normalized == QStringLiteral("doc_orientation")) {
        return TrainingTaskKind::DocCls;
    }
    if (normalized == QStringLiteral("textline_cls")) {
        return TrainingTaskKind::TextlineCls;
    }
    if (normalized == QStringLiteral("table_cls") || normalized == QStringLiteral("table_classification")) {
        return TrainingTaskKind::TableCls;
    }
    if (normalized == QStringLiteral("layout")) {
        return TrainingTaskKind::Layout;
    }
    return optionalIf(normalized == QStringLiteral("unknown") || normalized == QStringLiteral("uvdoc"), TrainingTaskKind::Unknown);
}

QString trainingTaskKindGroupKey(TrainingTaskKind kind) {
    switch (kind) {
    case TrainingTaskKind::OcrDet:
        return QStringLiteral("det");
    case TrainingTaskKind::OcrRec:
        return QStringLiteral("rec");
    case TrainingTaskKind::DocCls:
    case TrainingTaskKind::TextlineCls:
    case TrainingTaskKind::TableCls:
        return QStringLiteral("cls");
    case TrainingTaskKind::Layout:
        return QStringLiteral("layout");
    case TrainingTaskKind::Unknown:
        return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

bool isClassificationTrainingTaskKind(TrainingTaskKind kind) {
    return kind == TrainingTaskKind::DocCls
        || kind == TrainingTaskKind::TextlineCls
        || kind == TrainingTaskKind::TableCls;
}

QString toString(RunStatus status) {
    switch (status) {
    case RunStatus::Pending:
        return QStringLiteral("pending");
    case RunStatus::Running:
        return QStringLiteral("running");
    case RunStatus::Finished:
        return QStringLiteral("success");
    case RunStatus::Failed:
        return QStringLiteral("failed");
    case RunStatus::Cancelled:
        return QStringLiteral("stopped");
    }
    return QStringLiteral("pending");
}

std::optional<RunStatus> runStatusFromString(const QString& value) {
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("pending")) {
        return RunStatus::Pending;
    }
    if (normalized == QStringLiteral("running")) {
        return RunStatus::Running;
    }
    if (normalized == QStringLiteral("success") || normalized == QStringLiteral("finished")) {
        return RunStatus::Finished;
    }
    if (normalized == QStringLiteral("failed")) {
        return RunStatus::Failed;
    }
    if (normalized == QStringLiteral("stopped") || normalized == QStringLiteral("cancelled")) {
        return RunStatus::Cancelled;
    }
    return std::nullopt;
}

QString toString(PredictionStatus status) {
    switch (status) {
    case PredictionStatus::Pending:
        return QStringLiteral("pending");
    case PredictionStatus::Running:
        return QStringLiteral("running");
    case PredictionStatus::Finished:
        return QStringLiteral("finished");
    case PredictionStatus::Failed:
        return QStringLiteral("failed");
    }
    return QStringLiteral("pending");
}

std::optional<PredictionStatus> predictionStatusFromString(const QString& value) {
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("pending")) {
        return PredictionStatus::Pending;
    }
    if (normalized == QStringLiteral("running")) {
        return PredictionStatus::Running;
    }
    if (normalized == QStringLiteral("finished") || normalized == QStringLiteral("success")) {
        return PredictionStatus::Finished;
    }
    if (normalized == QStringLiteral("failed")) {
        return PredictionStatus::Failed;
    }
    return std::nullopt;
}

}  // namespace ppocr
