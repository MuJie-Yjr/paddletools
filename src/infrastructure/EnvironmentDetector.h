#pragma once

#include <QString>

namespace ppocr {

struct EnvironmentPathStatus {
    QString path;
    bool exists = false;
};

class EnvironmentDetector {
public:
    static EnvironmentPathStatus checkPath(const QString& path);
};

}  // namespace ppocr
