#pragma once

#include <QJsonObject>
#include <QString>

namespace ppocr {

class RuntimePaths {
public:
    static QString defaultBaseDir();
    static QString defaultPaddlexPython();
    static QString resolvedExecutable(const QString& executable);
    static QJsonObject pathStatus(const QString& path);
    static QJsonObject executableStatus(const QString& executable);
};

}  // namespace ppocr
