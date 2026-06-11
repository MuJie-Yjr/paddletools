#pragma once

#include "domain/Project.h"

#include <QString>
#include <QStringList>

namespace ppocr {

class ProjectService {
public:
    explicit ProjectService(QString baseDir);

    ProjectContext openProject(const QString& projectDir) const;
    ProjectContext createProject(const QString& projectDir, const QString& name) const;
    QStringList recentProjects() const;
    void rememberProject(const QString& projectDir) const;

private:
    QString recentProjectsPath() const;
    void saveRecentProjects(const QStringList& projects) const;

    QString baseDir_;
};

}  // namespace ppocr
