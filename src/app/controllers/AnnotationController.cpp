#include "app/controllers/AnnotationController.h"

#include "application/AnnotationService.h"

namespace ppocr {

AnnotationController::AnnotationController(QObject* parent)
    : QObject(parent) {}

const QJsonObject& AnnotationController::currentAnnotation() const {
    return currentAnnotation_;
}

bool AnnotationController::hasCurrentAnnotation() const {
    return !currentAnnotation_.isEmpty();
}

void AnnotationController::setCurrentAnnotation(const QJsonObject& annotation) {
    currentAnnotation_ = annotation;
    undoStack_.clear();
    redoStack_.clear();
    selectedRegionId_.clear();
}

void AnnotationController::clearCurrentAnnotation() {
    currentAnnotation_ = {};
    undoStack_.clear();
    redoStack_.clear();
    selectedRegionId_.clear();
}

QString AnnotationController::selectedRegionId() const {
    return selectedRegionId_;
}

void AnnotationController::setSelectedRegionId(const QString& regionId) {
    selectedRegionId_ = regionId;
}

void AnnotationController::clearSelectedRegion() {
    selectedRegionId_.clear();
}

QJsonObject AnnotationController::selectedRegion() const {
    return findRegion(selectedRegionId_);
}

QJsonObject AnnotationController::findRegion(const QString& regionId) const {
    return AnnotationService::findRegion(currentAnnotation_, regionId);
}

bool AnnotationController::canUndo() const {
    return !undoStack_.isEmpty();
}

bool AnnotationController::canRedo() const {
    return !redoStack_.isEmpty();
}

void AnnotationController::pushUndoState() {
    if (currentAnnotation_.isEmpty()) {
        return;
    }
    if (!undoStack_.isEmpty() && undoStack_.last() == currentAnnotation_) {
        return;
    }
    undoStack_.append(currentAnnotation_);
    trimUndoStack();
    redoStack_.clear();
}

bool AnnotationController::undo() {
    if (undoStack_.isEmpty() || currentAnnotation_.isEmpty()) {
        return false;
    }
    redoStack_.append(currentAnnotation_);
    currentAnnotation_ = undoStack_.takeLast();
    clearSelectionIfMissing();
    return true;
}

bool AnnotationController::redo() {
    if (redoStack_.isEmpty() || currentAnnotation_.isEmpty()) {
        return false;
    }
    undoStack_.append(currentAnnotation_);
    currentAnnotation_ = redoStack_.takeLast();
    clearSelectionIfMissing();
    return true;
}

bool AnnotationController::replaceCurrentAnnotation(const QJsonObject& annotation, bool pushUndo) {
    if (annotation == currentAnnotation_) {
        return false;
    }
    if (pushUndo) {
        pushUndoState();
    }
    currentAnnotation_ = annotation;
    clearSelectionIfMissing();
    return true;
}

bool AnnotationController::clearAnnotation() {
    return replaceCurrentAnnotation(AnnotationService::clearAnnotation(currentAnnotation_));
}

bool AnnotationController::addOcrRegion(const QJsonArray& points) {
    if (points.size() != 4 || !AnnotationService::isValidRegionPoints(points)) {
        return false;
    }
    const QJsonObject updated = AnnotationService::addOcrRegion(currentAnnotation_, points);
    if (!replaceCurrentAnnotation(updated)) {
        return false;
    }
    const QJsonArray regions = currentAnnotation_.value(QStringLiteral("regions")).toArray();
    selectedRegionId_ = regions.isEmpty() ? QString() : regions.last().toObject().value(QStringLiteral("id")).toString();
    return true;
}

bool AnnotationController::addLayoutRegion(const QJsonArray& points, const QString& label) {
    if (points.size() != 4 || !AnnotationService::isValidRegionPoints(points)) {
        return false;
    }
    const QJsonObject updated = AnnotationService::addLayoutRegion(currentAnnotation_, points, label);
    if (!replaceCurrentAnnotation(updated)) {
        return false;
    }
    const QJsonArray regions = currentAnnotation_.value(QStringLiteral("regions")).toArray();
    selectedRegionId_ = regions.isEmpty() ? QString() : regions.last().toObject().value(QStringLiteral("id")).toString();
    return true;
}

bool AnnotationController::moveRegion(const QString& regionId, const QJsonArray& points) {
    const QJsonObject region = findRegion(regionId);
    if (region.isEmpty() || !AnnotationService::isValidRegionPoints(points)) {
        return false;
    }
    selectedRegionId_ = regionId;
    return replaceCurrentAnnotation(AnnotationService::moveRegion(currentAnnotation_, regionId, points));
}

bool AnnotationController::updateSelectedRegion(const QJsonObject& updates) {
    if (selectedRegionId_.isEmpty() || findRegion(selectedRegionId_).isEmpty()) {
        return false;
    }
    return replaceCurrentAnnotation(AnnotationService::updateRegion(currentAnnotation_, selectedRegionId_, updates));
}

bool AnnotationController::setImageLabels(
    const QString& docOrientation,
    const QString& textlineOrientation,
    const QString& tableClassification) {
    return replaceCurrentAnnotation(AnnotationService::setImageLabels(
        currentAnnotation_,
        docOrientation,
        textlineOrientation,
        tableClassification));
}

bool AnnotationController::deleteSelectedRegion() {
    if (selectedRegionId_.isEmpty() || findRegion(selectedRegionId_).isEmpty()) {
        return false;
    }
    const QJsonObject updated = AnnotationService::deleteRegion(currentAnnotation_, selectedRegionId_);
    selectedRegionId_.clear();
    return replaceCurrentAnnotation(updated);
}

QList<PageInfo> AnnotationController::listPages(const ProjectContext& context) const {
    return AnnotationService::listPages(context);
}

QJsonObject AnnotationController::readAnnotation(const QString& annotationPath) const {
    return AnnotationService::readAnnotation(annotationPath);
}

void AnnotationController::writeAnnotation(const QString& annotationPath, const QJsonObject& annotation) const {
    AnnotationService::writeAnnotation(annotationPath, annotation);
}

QList<PageInfo> AnnotationController::listProjectPages(const ProjectContext& context) {
    return AnnotationService::listPages(context);
}

QJsonObject AnnotationController::readAnnotationFile(const QString& annotationPath) {
    return AnnotationService::readAnnotation(annotationPath);
}

void AnnotationController::writeAnnotationFile(const QString& annotationPath, const QJsonObject& annotation) {
    AnnotationService::writeAnnotation(annotationPath, annotation);
}

void AnnotationController::trimUndoStack() {
    while (undoStack_.size() > 80) {
        undoStack_.removeFirst();
    }
}

void AnnotationController::clearSelectionIfMissing() {
    if (!selectedRegionId_.isEmpty() && findRegion(selectedRegionId_).isEmpty()) {
        selectedRegionId_.clear();
    }
}

}  // namespace ppocr
