#pragma once

#include "domain/Page.h"
#include "domain/Project.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

namespace ppocr {

class AnnotationController : public QObject {
    Q_OBJECT

public:
    explicit AnnotationController(QObject* parent = nullptr);

    const QJsonObject& currentAnnotation() const;
    bool hasCurrentAnnotation() const;
    void setCurrentAnnotation(const QJsonObject& annotation);
    void clearCurrentAnnotation();

    QString selectedRegionId() const;
    void setSelectedRegionId(const QString& regionId);
    void clearSelectedRegion();
    QJsonObject selectedRegion() const;
    QJsonObject findRegion(const QString& regionId) const;

    bool canUndo() const;
    bool canRedo() const;
    void pushUndoState();
    bool undo();
    bool redo();

    bool replaceCurrentAnnotation(const QJsonObject& annotation, bool pushUndo = true);
    bool clearAnnotation();
    bool addOcrRegion(const QJsonArray& points);
    bool addLayoutRegion(const QJsonArray& points, const QString& label);
    bool moveRegion(const QString& regionId, const QJsonArray& points);
    bool updateSelectedRegion(const QJsonObject& updates);
    bool setImageLabels(const QString& docOrientation, const QString& textlineOrientation, const QString& tableClassification);
    bool deleteSelectedRegion();
    QList<PageInfo> listPages(const ProjectContext& context) const;
    QJsonObject readAnnotation(const QString& annotationPath) const;
    void writeAnnotation(const QString& annotationPath, const QJsonObject& annotation) const;
    static QList<PageInfo> listProjectPages(const ProjectContext& context);
    static QJsonObject readAnnotationFile(const QString& annotationPath);
    static void writeAnnotationFile(const QString& annotationPath, const QJsonObject& annotation);

private:
    void trimUndoStack();
    void clearSelectionIfMissing();

    QJsonObject currentAnnotation_;
    QList<QJsonObject> undoStack_;
    QList<QJsonObject> redoStack_;
    QString selectedRegionId_;
};

}  // namespace ppocr
