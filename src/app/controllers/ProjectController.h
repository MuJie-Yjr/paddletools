#pragma once

#include "application/ProjectService.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace ppocr {

class ProjectController : public QObject {
    Q_OBJECT

public:
    explicit ProjectController(QString baseDir, QObject* parent = nullptr);

    ProjectContext openProject(const QString& projectDir) const;
    ProjectContext createProject(const QString& projectDir, const QString& name) const;
    QStringList recentProjects() const;
    void rememberProject(const QString& projectDir) const;

private:
    ProjectService service_;
};

}  // namespace ppocr
