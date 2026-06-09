#include "core/AssetImporter.h"

#include "core/Constants.h"
#include "core/JsonUtils.h"
#include "core/ProjectRepository.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QImageReader>
#include <QPainter>
#include <QPdfDocument>
#include <stdexcept>

namespace ppocr {

QStringList AssetImporter::importPaths(ProjectContext& context, const QStringList& paths, int pdfDpi) {
    QStringList imported;
    for (const auto& path : paths) {
        const QString suffix = QFileInfo(path).suffix().prepend('.').toLower();
        if (imageExtensions().contains(suffix)) {
            imported.append(importImage(context, path));
        } else if (pdfExtensions().contains(suffix)) {
            imported.append(importPdf(context, path, pdfDpi));
        } else {
            throw std::runtime_error(QString("unsupported file type: %1").arg(path).toStdString());
        }
    }
    return imported;
}

QString AssetImporter::importImage(ProjectContext& context, const QString& sourcePath) {
    QFileInfo sourceInfo(sourcePath);
    const QString assetId = ProjectRepository::nextAssetId(context);
    QDir().mkpath(context.rawRoot());
    const QString rawTarget = QDir(context.rawRoot()).filePath(sourceInfo.fileName());
    if (sourceInfo.absoluteFilePath() != QFileInfo(rawTarget).absoluteFilePath()) {
        QFile::copy(sourceInfo.absoluteFilePath(), rawTarget);
    }

    QImageReader reader(sourceInfo.absoluteFilePath());
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull()) {
        throw std::runtime_error(QString("cannot read image: %1").arg(sourcePath).toStdString());
    }
    const QString pagePath = savePageImage(context, assetId, image);
    createThumb(pagePath, QDir(context.thumbRoot()).filePath(assetId + ".jpg"));
    createAnnotationForPage(context, assetId, pagePath);
    return pagePath;
}

QStringList AssetImporter::importPdf(ProjectContext& context, const QString& sourcePath, int dpi) {
    QFileInfo sourceInfo(sourcePath);
    QDir().mkpath(context.rawRoot());
    const QString rawTarget = QDir(context.rawRoot()).filePath(sourceInfo.fileName());
    if (sourceInfo.absoluteFilePath() != QFileInfo(rawTarget).absoluteFilePath()) {
        QFile::copy(sourceInfo.absoluteFilePath(), rawTarget);
    }

    QPdfDocument document;
    const auto status = document.load(sourceInfo.absoluteFilePath());
    if (status != QPdfDocument::Error::None) {
        throw std::runtime_error(QString("cannot load pdf: %1").arg(sourcePath).toStdString());
    }

    QStringList pages;
    const double scale = dpi / 72.0;
    for (int index = 0; index < document.pageCount(); ++index) {
        const QSizeF pointSize = document.pagePointSize(index);
        const QSize pixelSize(
            std::max(1, qRound(pointSize.width() * scale)),
            std::max(1, qRound(pointSize.height() * scale)));
        QImage image = document.render(index, pixelSize);
        if (image.isNull()) {
            continue;
        }
        const QString assetId = ProjectRepository::nextAssetId(context);
        const QString pagePath = savePageImage(context, assetId, image);
        createThumb(pagePath, QDir(context.thumbRoot()).filePath(assetId + ".jpg"));
        createAnnotationForPage(context, assetId, pagePath);
        pages.append(pagePath);
    }
    return pages;
}

QString AssetImporter::savePageImage(ProjectContext& context, const QString& assetId, const QImage& image) {
    QDir().mkpath(context.imageRoot());
    const QString pagePath = QDir(context.imageRoot()).filePath(assetId + ".jpg");
    image.convertToFormat(QImage::Format_RGB888).save(pagePath, "JPG", 94);
    return pagePath;
}

void AssetImporter::createThumb(const QString& pagePath, const QString& thumbPath) {
    QDir().mkpath(QFileInfo(thumbPath).absolutePath());
    QImage image(pagePath);
    image = image.scaled(QSize(180, 240), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    image.convertToFormat(QImage::Format_RGB888).save(thumbPath, "JPG", 85);
}

void AssetImporter::createAnnotationForPage(ProjectContext& context, const QString& assetId, const QString& pagePath) {
    QImageReader reader(pagePath);
    const QSize size = reader.size();
    bool ok = false;
    const int pageIndex = assetId.section('_', -1).toInt(&ok);
    const int index = ok ? pageIndex : 1;
    const QString split = (index % 5 == 0) ? "val" : "train";
    const QJsonObject annotation = ProjectRepository::defaultAnnotation(
        assetId,
        normalizedRelativePath(context.root, pagePath),
        size.width(),
        size.height(),
        index,
        split);
    ProjectRepository::writeAnnotation(QDir(context.annotationRoot()).filePath(assetId + ".json"), annotation);
}

}  // namespace ppocr
