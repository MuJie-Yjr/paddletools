#pragma once

#include "ProjectTypes.h"

#include <functional>
#include <QList>
#include <QMap>
#include <QSet>

namespace ppocr {

class Exporter {
public:
    using ProgressCallback = std::function<void(int current, int total, const QString& message)>;

    struct ExportOptions {
        QString outputRoot;
        bool timestampedTaskDirs = false;
    };

    static QMap<QString, QString> exportSelected(
        const ProjectContext& context,
        const QSet<QString>& tasks = {"det", "rec", "cls", "textline_cls", "coco"},
        bool checkedOnly = true,
        bool requireValidation = true,
        const ProgressCallback& progress = {},
        const ExportOptions& options = ExportOptions{});

    static QString datasetOutputDir(const ProjectContext& context, const QString& outputName);

private:
    static QString exportDet(
        const ProjectContext& context,
        const QList<PageInfo>& pages,
        const QString& exportRoot,
        const QString& exportStamp,
        bool timestampedTaskDirs,
        bool checkedOnly,
        int& completed,
        int total,
        const ProgressCallback& progress);
    static QString exportRec(
        const ProjectContext& context,
        const QList<PageInfo>& pages,
        const QString& exportRoot,
        const QString& exportStamp,
        bool timestampedTaskDirs,
        bool checkedOnly,
        int& completed,
        int total,
        const ProgressCallback& progress);
    static QString exportImageClassification(
        const ProjectContext& context,
        const QList<PageInfo>& pages,
        const QString& exportRoot,
        const QString& exportStamp,
        bool timestampedTaskDirs,
        const QString& outputName,
        const QString& task,
        const QString& labelSetKey,
        const QString& fallbackLabelSetKey,
        int& completed,
        int total,
        const ProgressCallback& progress);
    static QString exportCocoLayout(
        const ProjectContext& context,
        const QList<PageInfo>& pages,
        const QString& exportRoot,
        const QString& exportStamp,
        bool timestampedTaskDirs,
        bool checkedOnly,
        int& completed,
        int total,
        const ProgressCallback& progress);
    static QString imageLabel(const QJsonObject& annotation, const QString& task);
    static void resetDir(const QString& path);
    static void writeExportLog(const ProjectContext& context, const QMap<QString, QString>& outputs);
};

}  // namespace ppocr
