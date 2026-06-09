#pragma once

#include "ProjectTypes.h"

#include <QJsonObject>
#include <QStringList>

namespace ppocr {

class ProjectRepository {
public:
    static ProjectContext createProject(const QString& projectDir, const QString& projectName = QString());
    static ProjectContext openProject(const QString& projectDir);
    static void saveProject(const ProjectContext& context);
    static QList<PageInfo> listPages(const ProjectContext& context);
    static QString nextAssetId(const ProjectContext& context);
    static QSize imageSize(const QString& path);
    static QJsonObject readAnnotation(const QString& path);
    static void writeAnnotation(const QString& path, const QJsonObject& annotation);
    static QJsonObject defaultAnnotation(
        const QString& assetId,
        const QString& relativeImagePath,
        int width,
        int height,
        int pageIndex,
        const QString& split = "train");

private:
    static QStringList projectDirs();
};

}  // namespace ppocr
