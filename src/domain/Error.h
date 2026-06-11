#pragma once

#include <QString>
#include <QStringList>

namespace ppocr {

struct DomainError {
    QString code;
    QString message;
    QStringList details;
};

}  // namespace ppocr
