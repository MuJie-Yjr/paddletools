#pragma once

#include <QString>

namespace ppocr {

class Logger {
public:
    static bool appendLine(const QString& path, const QString& line);
};

}  // namespace ppocr
