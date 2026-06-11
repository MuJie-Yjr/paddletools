#include "infrastructure/EnvironmentDetector.h"

#include "infrastructure/FileSystem.h"

namespace ppocr {

EnvironmentPathStatus EnvironmentDetector::checkPath(const QString& path) {
    return EnvironmentPathStatus{path, FileSystem::exists(path)};
}

}  // namespace ppocr
