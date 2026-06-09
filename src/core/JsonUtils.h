#pragma once

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringConverter>
#include <QTextStream>

namespace ppocr {

inline bool readJsonObject(const QString& path, QJsonObject* object, QString* error = nullptr) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) {
            *error = parseError.errorString();
        }
        return false;
    }
    *object = document.object();
    return true;
}

inline bool writeJsonObject(const QString& path, const QJsonObject& object, QString* error = nullptr) {
    QFileInfo info(path);
    QDir().mkpath(info.absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return true;
}

inline QString normalizedRelativePath(const QDir& root, const QString& absolutePath) {
    return root.relativeFilePath(QFileInfo(absolutePath).absoluteFilePath()).replace("\\", "/");
}

inline void writeTextLines(const QString& path, const QStringList& lines) {
    QFileInfo info(path);
    QDir().mkpath(info.absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    for (const auto& line : lines) {
        stream << line << '\n';
    }
}

}  // namespace ppocr
