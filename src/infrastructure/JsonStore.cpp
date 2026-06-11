#include "infrastructure/JsonStore.h"

#include "infrastructure/FileSystem.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace ppocr {

QJsonArray JsonStore::readArray(const QString& path) {
    bool ok = false;
    const QByteArray data = FileSystem::readAll(path, &ok);
    if (!ok) {
        return {};
    }
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray()) {
        return {};
    }
    return doc.array();
}

QJsonObject JsonStore::readObject(const QString& path) {
    bool ok = false;
    const QByteArray data = FileSystem::readAll(path, &ok);
    if (!ok) {
        return {};
    }
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return {};
    }
    return doc.object();
}

bool JsonStore::writeArray(const QString& path, const QJsonArray& array) {
    return FileSystem::writeAll(path, QJsonDocument(array).toJson(QJsonDocument::Indented));
}

bool JsonStore::writeObject(const QString& path, const QJsonObject& object) {
    return FileSystem::writeAll(path, QJsonDocument(object).toJson(QJsonDocument::Indented));
}

}  // namespace ppocr
