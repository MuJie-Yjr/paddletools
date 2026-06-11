#pragma once

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

namespace ppocr {

struct ProcessCommand {
    QString program;
    QStringList arguments;
    QString workingDirectory;
    QProcessEnvironment environment;
};

}  // namespace ppocr
