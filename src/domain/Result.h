#pragma once

#include "domain/Error.h"

namespace ppocr {

struct OperationStatus {
    bool ok = true;
    DomainError error;

    static OperationStatus success() {
        return {};
    }

    static OperationStatus failure(const QString& code, const QString& message) {
        OperationStatus status;
        status.ok = false;
        status.error.code = code;
        status.error.message = message;
        return status;
    }
};

}  // namespace ppocr
