#include "application/ProjectService.h"

#include "core/ProjectRepository.h"
#include "infrastructure/JsonStore.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <utility>

namespace ppocr {

ProjectService::ProjectService(QString baseDir)
    : baseDir_(std::move(baseDir)) {}

ProjectContext ProjectService::openProject(const QString& projectDir) const {
    return ProjectRepository::openProject(projectDir);
}

ProjectContext ProjectService::createProject(const QString& projectDir, const QString& name) const {
    return ProjectRepository::createProject(projectDir, name);
}

QStringList ProjectService::recentProjects() const {
    QStringList projects;
    for (const auto& value : JsonStore::readArray(recentProjectsPath())) {
        const QString path = value.toString();
        if (!path.isEmpty() && QFileInfo::exists(QDir(path).filePath(QStringLiteral("project.json")))) {
            projects.append(QFileInfo(path).absoluteFilePath());
        }
    }
    projects.removeDuplicates();
    return projects;
}

void ProjectService::rememberProject(const QString& projectDir) const {
    if (projectDir.isEmpty()) {
        return;
    }

    QStringList projects = recentProjects();
    const QString normalized = QFileInfo(projectDir).absoluteFilePath();
    projects.removeAll(normalized);
    projects.prepend(normalized);
    while (projects.size() > 20) {
        projects.removeLast();
    }
    saveRecentProjects(projects);
}

QString ProjectService::recentProjectsPath() const {
    return QDir(baseDir_).filePath(QStringLiteral("recent_projects.json"));
}

void ProjectService::saveRecentProjects(const QStringList& projects) const {
    QJsonArray array;
    for (const auto& project : projects) {
        array.append(project);
    }
    JsonStore::writeArray(recentProjectsPath(), array);
}

}  // namespace ppocr
