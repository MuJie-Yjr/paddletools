#include "infrastructure/FileSystem.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace ppocr {

bool FileSystem::exists(const QString& path) {
    return QFileInfo::exists(path);
}

bool FileSystem::ensureDirectory(const QString& path) {
    return QDir().mkpath(path);
}

QByteArray FileSystem::readAll(const QString& path, bool* ok) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (ok) {
            *ok = false;
        }
        return {};
    }
    if (ok) {
        *ok = true;
    }
    return file.readAll();
}

bool FileSystem::writeAll(const QString& path, const QByteArray& data) {
    const QFileInfo info(path);
    if (!info.absolutePath().isEmpty()) {
        QDir().mkpath(info.absolutePath());
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(data) == data.size();
}

QString FileSystem::absolutePath(const QString& path) {
    return QFileInfo(path).absoluteFilePath();
}

}  // namespace ppocr
