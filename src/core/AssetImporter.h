#pragma once

#include "ProjectTypes.h"

#include <QStringList>

class QImage;

namespace ppocr {

class AssetImporter {
public:
    static QStringList importPaths(ProjectContext& context, const QStringList& paths, int pdfDpi = 200);
    static QString importImage(ProjectContext& context, const QString& sourcePath);
    static QStringList importPdf(ProjectContext& context, const QString& sourcePath, int dpi = 200);

private:
    static QString savePageImage(ProjectContext& context, const QString& assetId, const QImage& image);
    static void createThumb(const QString& pagePath, const QString& thumbPath);
    static void createAnnotationForPage(ProjectContext& context, const QString& assetId, const QString& pagePath);
};

}  // namespace ppocr
