#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace ppocr {

class JsonStore {
public:
    static QJsonArray readArray(const QString& path);
    static QJsonObject readObject(const QString& path);
    static bool writeArray(const QString& path, const QJsonArray& array);
    static bool writeObject(const QString& path, const QJsonObject& object);
};

}  // namespace ppocr
