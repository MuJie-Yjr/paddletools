#pragma once

#include <QByteArray>
#include <QString>

namespace ppocr {

class FileSystem {
public:
    static bool exists(const QString& path);
    static bool ensureDirectory(const QString& path);
    static QByteArray readAll(const QString& path, bool* ok = nullptr);
    static bool writeAll(const QString& path, const QByteArray& data);
    static QString absolutePath(const QString& path);
};

}  // namespace ppocr
