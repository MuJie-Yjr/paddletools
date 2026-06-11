#include "core/Exporter.h"

#include "core/CropGenerator.h"
#include "core/JsonUtils.h"
#include "core/ProjectRepository.h"
#include "core/Validator.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStringConverter>
#include <QTextStream>
#include <stdexcept>

namespace ppocr {

namespace {

PageSplit annotationSplit(const QJsonObject& annotation) {
    const QJsonValue value = annotation.value(QStringLiteral("split"));
    if (value.isUndefined() || value.toString().isEmpty()) {
        return PageSplit::Train;
    }
    return pageSplitFromString(value.toString()).value_or(PageSplit::Unassigned);
}

bool dirHasEntries(const QString& path) {
    QDir dir(path);
    return dir.exists() && !dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty();
}

QString uniqueHistoryPath(const QString& historyRoot, const QString& stamp, const QString& nameSuffix = {}) {
    QDir().mkpath(historyRoot);
    const QString baseName = nameSuffix.isEmpty() ? stamp : QStringLiteral("%1_%2").arg(stamp, nameSuffix);
    QString archivePath = QDir(historyRoot).filePath(baseName);
    int counter = 1;
    while (QFileInfo::exists(archivePath)) {
        archivePath = QDir(historyRoot).filePath(QStringLiteral("%1_%2").arg(baseName).arg(counter++));
    }
    return archivePath;
}

void archiveCurrentDir(const QString& container, const QString& stamp) {
    const QString current = QDir(container).filePath(QStringLiteral("current"));
    if (!dirHasEntries(current)) {
        return;
    }

    const QString historyRoot = QDir(container).filePath(QStringLiteral("history"));
    const QString archivePath = uniqueHistoryPath(historyRoot, stamp);
    QDir root;
    if (!root.rename(current, archivePath)) {
        throw std::runtime_error(QStringLiteral("failed to archive existing export: %1 -> %2")
            .arg(current, archivePath)
            .toStdString());
    }
}

void archiveLegacyFlatEntries(const QString& container, const QString& stamp) {
    QDir dir(container);
    if (!dir.exists()) {
        return;
    }

    const QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);
    QFileInfoList legacyEntries;
    for (const QFileInfo& entry : entries) {
        const QString name = entry.fileName();
        if (name == QStringLiteral("current") || name == QStringLiteral("history")) {
            continue;
        }
        legacyEntries.append(entry);
    }
    if (legacyEntries.isEmpty()) {
        return;
    }

    const QString archivePath = uniqueHistoryPath(QDir(container).filePath(QStringLiteral("history")), stamp, QStringLiteral("legacy"));
    QDir().mkpath(archivePath);
    QDir root;
    for (const QFileInfo& entry : legacyEntries) {
        const QString target = QDir(archivePath).filePath(entry.fileName());
        if (!root.rename(entry.absoluteFilePath(), target)) {
            throw std::runtime_error(QStringLiteral("failed to archive legacy export entry: %1 -> %2")
                .arg(entry.absoluteFilePath(), target)
                .toStdString());
        }
    }
}

QString prepareOutputDir(
    const QString& exportRoot,
    const QString& outputName,
    const QString& stamp,
    bool timestampedTaskDirs) {
    if (timestampedTaskDirs) {
        QString path = QDir(exportRoot).filePath(QStringLiteral("%1_%2").arg(outputName, stamp));
        int suffix = 1;
        while (QFileInfo::exists(path)) {
            path = QDir(exportRoot).filePath(QStringLiteral("%1_%2_%3").arg(outputName, stamp).arg(suffix++));
        }
        QDir().mkpath(path);
        return path;
    }

    const QString container = QDir(exportRoot).filePath(outputName);
    QDir().mkpath(container);
    archiveCurrentDir(container, stamp);
    archiveLegacyFlatEntries(container, stamp);
    const QString current = QDir(container).filePath(QStringLiteral("current"));
    QDir().mkpath(current);
    QDir().mkpath(QDir(container).filePath(QStringLiteral("history")));
    return current;
}

void reportProgress(
    const Exporter::ProgressCallback& progress,
    int& completed,
    int total,
    const QString& message) {
    ++completed;
    if (progress) {
        progress(completed, total, message);
    }
}

}  // namespace

QMap<QString, QString> Exporter::exportSelected(
    const ProjectContext& context,
    const QSet<QString>& tasks,
    bool checkedOnly,
    bool requireValidation,
    const ProgressCallback& progress,
    const ExportOptions& options) {
    const QList<PageInfo> pages = ProjectRepository::listPages(context);
    const QString exportRoot = options.outputRoot.trimmed().isEmpty()
        ? context.exportRoot()
        : QFileInfo(options.outputRoot).absoluteFilePath();
    const QString exportStamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    QDir().mkpath(exportRoot);
    int total = requireValidation ? 1 : 0;
    for (const auto& task : tasks) {
        if (task == QStringLiteral("det")
            || task == QStringLiteral("rec")
            || task == QStringLiteral("cls")
            || task == QStringLiteral("textline_cls")
            || task == QStringLiteral("table_cls")
            || task == QStringLiteral("coco")) {
            total += pages.size();
        }
    }
    if (total <= 0) {
        total = 1;
    }
    int completed = 0;
    if (progress) {
        progress(completed, total, QStringLiteral("准备导出"));
    }
    if (requireValidation) {
        const auto issues = Validator::validateProject(context);
        Validator::writeValidationLog(context, issues);
        int errors = 0;
        for (const auto& issue : issues) {
            if (issue.severity == "error") {
                ++errors;
            }
        }
        if (errors > 0) {
            throw std::runtime_error(QString("validation has %1 errors").arg(errors).toStdString());
        }
        reportProgress(progress, completed, total, QStringLiteral("校验完成"));
    }
    QMap<QString, QString> outputs;
    if (tasks.contains("det")) {
        outputs["det"] = exportDet(context, pages, exportRoot, exportStamp, options.timestampedTaskDirs, checkedOnly, completed, total, progress);
    }
    if (tasks.contains("rec")) {
        outputs["rec"] = exportRec(context, pages, exportRoot, exportStamp, options.timestampedTaskDirs, checkedOnly, completed, total, progress);
    }
    if (tasks.contains("cls")) {
        outputs["cls"] = exportImageClassification(
            context,
            pages,
            exportRoot,
            exportStamp,
            options.timestampedTaskDirs,
            "ppocr_cls",
            "doc_orientation",
            "doc_orientation",
            QString(),
            completed,
            total,
            progress);
    }
    if (tasks.contains("textline_cls")) {
        outputs["textline_cls"] = exportImageClassification(
            context,
            pages,
            exportRoot,
            exportStamp,
            options.timestampedTaskDirs,
            "ppocr_textline_cls",
            "textline_orientation",
            "textline_orientation",
            QString(),
            completed,
            total,
            progress);
    }
    if (tasks.contains("table_cls")) {
        outputs["table_cls"] = exportImageClassification(
            context,
            pages,
            exportRoot,
            exportStamp,
            options.timestampedTaskDirs,
            "table_classification",
            "table_classification",
            "table_classification",
            "table_type",
            completed,
            total,
            progress);
    }
    if (tasks.contains("coco")) {
        outputs["coco"] = exportCocoLayout(context, pages, exportRoot, exportStamp, options.timestampedTaskDirs, checkedOnly, completed, total, progress);
    }
    writeExportLog(context, outputs);
    if (progress) {
        progress(total, total, QStringLiteral("导出完成"));
    }
    return outputs;
}

QString Exporter::datasetOutputDir(const ProjectContext& context, const QString& outputName) {
    return QDir(QDir(context.exportRoot()).filePath(outputName)).filePath(QStringLiteral("current"));
}

QString Exporter::exportDet(
    const ProjectContext& context,
    const QList<PageInfo>& pages,
    const QString& exportRoot,
    const QString& exportStamp,
    bool timestampedTaskDirs,
    bool checkedOnly,
    int& completed,
    int total,
    const ProgressCallback& progress) {
    const QString output = prepareOutputDir(exportRoot, QStringLiteral("ppocr_det"), exportStamp, timestampedTaskDirs);
    const QString imagesDir = QDir(output).filePath("images");
    resetDir(imagesDir);
    QStringList trainLines;
    QStringList valLines;
    for (const auto& page : pages) {
        QJsonObject annotation = ProjectRepository::readAnnotation(page.annotationPath);
        const QString relImage = "images/" + QFileInfo(page.imagePath).fileName();
        QFile::copy(page.imagePath, QDir(imagesDir).filePath(QFileInfo(page.imagePath).fileName()));
        QJsonArray entries;
        for (const auto& value : annotation.value("regions").toArray()) {
            const auto region = value.toObject();
            if (region.value("type").toString() != "ocr_text") {
                continue;
            }
            if (checkedOnly && !region.value("checked").toBool()) {
                continue;
            }
            entries.append(QJsonObject{
                {"transcription", region.value("ignore").toBool() ? "###" : region.value("text").toString()},
                {"points", region.value("points").toArray()},
            });
        }
        if (!entries.isEmpty()) {
            const QString line = relImage + "\t" + QString::fromUtf8(QJsonDocument(entries).toJson(QJsonDocument::Compact));
            (annotationSplit(annotation) == PageSplit::Train ? trainLines : valLines).append(line);
        }
        reportProgress(progress, completed, total, QStringLiteral("导出 Det：%1").arg(QFileInfo(page.imagePath).fileName()));
    }
    writeTextLines(QDir(output).filePath("train.txt"), trainLines);
    writeTextLines(QDir(output).filePath("val.txt"), valLines);
    return output;
}

QString Exporter::exportRec(
    const ProjectContext& context,
    const QList<PageInfo>& pages,
    const QString& exportRoot,
    const QString& exportStamp,
    bool timestampedTaskDirs,
    bool checkedOnly,
    int& completed,
    int total,
    const ProgressCallback& progress) {
    const QString output = prepareOutputDir(exportRoot, QStringLiteral("ppocr_rec"), exportStamp, timestampedTaskDirs);
    const QString trainDir = QDir(output).filePath("train");
    const QString testDir = QDir(output).filePath("test");
    resetDir(trainDir);
    resetDir(testDir);
    QStringList trainLines;
    QStringList testLines;
    QSet<QChar> charset;
    for (const auto& page : pages) {
        QJsonObject annotation = CropGenerator::generateRecCrops(context, ProjectRepository::readAnnotation(page.annotationPath));
        ProjectRepository::writeAnnotation(page.annotationPath, annotation);
        const bool train = annotationSplit(annotation) == PageSplit::Train;
        const QString targetDir = train ? trainDir : testDir;
        const QString prefix = train ? "train" : "test";
        for (const auto& value : annotation.value("rec_crops").toArray()) {
            const auto crop = value.toObject();
            if (checkedOnly && !crop.value("checked").toBool()) {
                continue;
            }
            const QString source = context.path(crop.value("crop_path").toString());
            if (!QFileInfo::exists(source)) {
                continue;
            }
            const QString name = QFileInfo(source).fileName();
            QFile::copy(source, QDir(targetDir).filePath(name));
            const QString text = crop.value("text").toString();
            for (const auto ch : text) {
                if (!ch.isSpace()) {
                    charset.insert(ch);
                }
            }
            (train ? trainLines : testLines).append(QString("%1/%2\t%3").arg(prefix, name, text));
        }
        reportProgress(progress, completed, total, QStringLiteral("导出 Rec：%1").arg(QFileInfo(page.imagePath).fileName()));
    }
    QStringList dict;
    for (const auto ch : charset) {
        dict.append(QString(ch));
    }
    dict.sort();
    writeTextLines(QDir(output).filePath("rec_gt_train.txt"), trainLines);
    writeTextLines(QDir(output).filePath("rec_gt_test.txt"), testLines);
    writeTextLines(QDir(output).filePath("train.txt"), trainLines);
    writeTextLines(QDir(output).filePath("val.txt"), testLines);
    writeTextLines(QDir(output).filePath("dict.txt"), dict);
    return output;
}

QString Exporter::exportImageClassification(
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
    const ProgressCallback& progress) {
    const QString output = prepareOutputDir(exportRoot, outputName, exportStamp, timestampedTaskDirs);
    const QString imagesDir = QDir(output).filePath("images");
    resetDir(imagesDir);
    const QJsonObject labelSets = context.config.value("label_sets").toObject();
    QJsonArray labels = labelSets.value(labelSetKey).toArray();
    if (labels.isEmpty() && !fallbackLabelSetKey.isEmpty()) {
        labels = labelSets.value(fallbackLabelSetKey).toArray();
    }
    QMap<QString, int> labelToId;
    QStringList labelLines;
    for (int i = 0; i < labels.size(); ++i) {
        const QString label = labels.at(i).toString();
        labelToId[label] = i;
        labelLines.append(QString("%1 %2").arg(i).arg(label));
    }
    QStringList trainLines;
    QStringList valLines;
    for (const auto& page : pages) {
        const QJsonObject annotation = ProjectRepository::readAnnotation(page.annotationPath);
        const QString label = imageLabel(annotation, task);
        if (!label.isEmpty() && labelToId.contains(label)) {
            const QString relImage = "images/" + QFileInfo(page.imagePath).fileName();
            QFile::copy(page.imagePath, QDir(imagesDir).filePath(QFileInfo(page.imagePath).fileName()));
            const QString line = QString("%1 %2").arg(relImage).arg(labelToId.value(label));
            (annotationSplit(annotation) == PageSplit::Train ? trainLines : valLines).append(line);
        }
        reportProgress(progress, completed, total, QStringLiteral("导出分类：%1").arg(QFileInfo(page.imagePath).fileName()));
    }
    writeTextLines(QDir(output).filePath("label.txt"), labelLines);
    writeTextLines(QDir(output).filePath("train.txt"), trainLines);
    writeTextLines(QDir(output).filePath("val.txt"), valLines);
    return output;
}

QString Exporter::exportCocoLayout(
    const ProjectContext& context,
    const QList<PageInfo>& pages,
    const QString& exportRoot,
    const QString& exportStamp,
    bool timestampedTaskDirs,
    bool checkedOnly,
    int& completed,
    int total,
    const ProgressCallback& progress) {
    const QString output = prepareOutputDir(exportRoot, QStringLiteral("coco_layout"), exportStamp, timestampedTaskDirs);
    const QString imagesDir = QDir(output).filePath("images");
    const QString annotationsDir = QDir(output).filePath("annotations");
    resetDir(imagesDir);
    QDir().mkpath(annotationsDir);
    const QJsonArray labels = context.config.value("label_sets").toObject().value("layout").toArray();
    QJsonArray categories;
    QMap<QString, int> categoryIds;
    for (int i = 0; i < labels.size(); ++i) {
        const QString label = labels.at(i).toString();
        const int id = i + 1;
        categoryIds[label] = id;
        categories.append(QJsonObject{{"id", id}, {"name", label}});
    }
    auto ensureCategoryId = [&categoryIds, &categories](const QString& label) {
        if (label.isEmpty()) {
            return 0;
        }
        if (categoryIds.contains(label)) {
            return categoryIds.value(label);
        }
        const int id = categories.size() + 1;
        categoryIds[label] = id;
        categories.append(QJsonObject{{"id", id}, {"name", label}});
        return id;
    };
    QJsonObject train{{"images", QJsonArray{}}, {"annotations", QJsonArray{}}};
    QJsonObject val = train;
    int imageId = 1;
    int annotationId = 1;
    for (const auto& page : pages) {
        const QJsonObject annotation = ProjectRepository::readAnnotation(page.annotationPath);
        QFile::copy(page.imagePath, QDir(imagesDir).filePath(QFileInfo(page.imagePath).fileName()));
        QJsonObject& bucket = annotationSplit(annotation) == PageSplit::Train ? train : val;
        QJsonArray images = bucket.value("images").toArray();
        images.append(QJsonObject{
            {"id", imageId},
            {"file_name", QFileInfo(page.imagePath).fileName()},
            {"width", page.width},
            {"height", page.height},
        });
        bucket["images"] = images;
        QJsonArray annotations = bucket.value("annotations").toArray();
        for (const auto& value : annotation.value("regions").toArray()) {
            const auto region = value.toObject();
            if (region.value("type").toString() != "layout") {
                continue;
            }
            if (checkedOnly && !region.value("checked").toBool(false)) {
                continue;
            }
            const QJsonArray bbox = region.value("bbox").toArray();
            if (bbox.size() != 4) {
                continue;
            }
            const int categoryId = ensureCategoryId(region.value("label").toString());
            if (categoryId <= 0) {
                continue;
            }
            annotations.append(QJsonObject{
                {"id", annotationId++},
                {"image_id", imageId},
                {"category_id", categoryId},
                {"bbox", bbox},
                {"area", bbox.at(2).toDouble() * bbox.at(3).toDouble()},
                {"iscrowd", 0},
            });
        }
        bucket["annotations"] = annotations;
        ++imageId;
        reportProgress(progress, completed, total, QStringLiteral("导出 COCO：%1").arg(QFileInfo(page.imagePath).fileName()));
    }
    train["categories"] = categories;
    val["categories"] = categories;
    writeJsonObject(QDir(annotationsDir).filePath("instance_train.json"), train);
    writeJsonObject(QDir(annotationsDir).filePath("instance_val.json"), val);
    return output;
}

QString Exporter::imageLabel(const QJsonObject& annotation, const QString& task) {
    for (const auto& value : annotation.value("image_labels").toArray()) {
        const auto item = value.toObject();
        if (item.value("task").toString() == task) {
            return item.value("label").toString();
        }
    }
    return {};
}

void Exporter::resetDir(const QString& path) {
    QDir dir(path);
    if (dir.exists()) {
        dir.removeRecursively();
    }
    QDir().mkpath(path);
}

void Exporter::writeExportLog(const ProjectContext& context, const QMap<QString, QString>& outputs) {
    const QString path = QDir(context.logRoot()).filePath("export.log");
    QFileInfo info(path);
    QDir().mkpath(info.absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    for (auto it = outputs.begin(); it != outputs.end(); ++it) {
        stream << it.key() << '\t' << it.value() << '\n';
    }
}

}  // namespace ppocr
