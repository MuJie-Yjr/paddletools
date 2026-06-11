#include "app/controllers/ProjectController.h"

#include <utility>

namespace ppocr {

ProjectController::ProjectController(QString baseDir, QObject* parent)
    : QObject(parent), service_(std::move(baseDir)) {}

ProjectContext ProjectController::openProject(const QString& projectDir) const {
    return service_.openProject(projectDir);
}

ProjectContext ProjectController::createProject(const QString& projectDir, const QString& name) const {
    return service_.createProject(projectDir, name);
}

QStringList ProjectController::recentProjects() const {
    return service_.recentProjects();
}

void ProjectController::rememberProject(const QString& projectDir) const {
    service_.rememberProject(projectDir);
}

}  // namespace ppocr
