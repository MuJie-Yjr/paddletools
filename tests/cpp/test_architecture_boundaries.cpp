#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

std::string readFile(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

bool isSourceFile(const fs::path& path) {
    const std::string extension = path.extension().string();
    return extension == ".h" || extension == ".hpp" || extension == ".cpp" || extension == ".cc";
}

std::vector<fs::path> filesUnder(const fs::path& root) {
    std::vector<fs::path> files;
    if (!fs::exists(root)) {
        return files;
    }
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() && isSourceFile(entry.path())) {
            files.push_back(entry.path());
        }
    }
    return files;
}

bool fail(const fs::path& path, const std::string& message) {
    std::cerr << path.string() << ": " << message << '\n';
    return false;
}

bool checkDomain(const fs::path& sourceRoot) {
    bool ok = true;
    const std::vector<std::string> forbidden{
        "#include \"app/",
        "#include \"core/",
        "#include \"paddle/",
        "<QWidget",
        "<QMainWindow",
        "<QPushButton",
        "<QLabel",
        "<QQuickWidget",
    };
    for (const auto& path : filesUnder(sourceRoot / "domain")) {
        const std::string text = readFile(path);
        for (const auto& needle : forbidden) {
            if (contains(text, needle)) {
                ok = fail(path, "domain layer must not depend on UI/core/paddle widgets: " + needle) && ok;
            }
        }
    }
    return ok;
}

bool checkNoAppDependency(const fs::path& root, const std::string& layerName) {
    bool ok = true;
    for (const auto& path : filesUnder(root)) {
        if (contains(readFile(path), "#include \"app/")) {
            ok = fail(path, layerName + " must not include app layer") && ok;
        }
    }
    return ok;
}

bool checkControllerBoundary(const fs::path& sourceRoot) {
    bool ok = true;
    for (const auto& path : filesUnder(sourceRoot / "app" / "controllers")) {
        const std::string text = readFile(path);
        if (contains(text, "#include \"core/")) {
            ok = fail(path, "app controllers must go through application services instead of core") && ok;
        }
        if (contains(text, "#include \"paddle/")) {
            ok = fail(path, "app controllers must go through application services instead of paddle") && ok;
        }
    }
    return ok;
}

bool checkAppBoundary(const fs::path& sourceRoot) {
    bool ok = true;
    const std::vector<std::string> forbidden{
        "#include \"core/",
        "#include \"paddle/",
        "ProjectRepository::",
        "RuntimePaths::",
        "TrainingRunStore::",
        "TrainingPreflight::",
        "AnnotationOps::",
        "CropGenerator::",
        "OcrPrelabeler::",
        "ClassificationPrelabeler::",
        "LayoutPrelabeler::",
        "EnvironmentReport::",
        "PaddleInferenceRuntime::",
        "PaddleOcrEngine::",
        "PaddleClsEngine::",
        "PaddleDocLayoutEngine::",
        "PaddleOcrModelConfig",
        "PaddleClsModelConfig",
        "PaddleDocLayoutModelConfig",
    };
    for (const auto& path : filesUnder(sourceRoot / "app")) {
        const std::string text = readFile(path);
        for (const auto& needle : forbidden) {
            if (contains(text, needle)) {
                ok = fail(path, "app layer must use application services instead of core/paddle directly: " + needle) && ok;
            }
        }
    }
    return ok;
}

bool checkMainWindowSplit(const fs::path& sourceRoot) {
    bool ok = true;
    const fs::path mainWindowCpp = sourceRoot / "app" / "MainWindow.cpp";
    const std::string text = readFile(mainWindowCpp);
    const std::vector<std::string> forbiddenDefinitions{
        "void MainWindow::buildTraining",
        "void MainWindow::startTraining",
        "void MainWindow::startPrediction",
        "void MainWindow::prelabel",
        "void MainWindow::exportProject",
        "void MainWindow::validateProject",
    };
    for (const auto& needle : forbiddenDefinitions) {
        if (contains(text, needle)) {
            ok = fail(mainWindowCpp, "MainWindow.cpp must stay focused on shell/navigation, not feature workflows: " + needle) && ok;
        }
    }
    return ok;
}

}  // namespace

int main() {
#ifndef PPOCR_PROJECT_ROOT
    std::cerr << "PPOCR_PROJECT_ROOT is not defined\n";
    return 1;
#endif
    const fs::path sourceRoot = fs::path(PPOCR_PROJECT_ROOT) / "src";
    bool ok = true;
    ok = checkDomain(sourceRoot) && ok;
    ok = checkNoAppDependency(sourceRoot / "application", "application") && ok;
    ok = checkNoAppDependency(sourceRoot / "core", "core") && ok;
    ok = checkNoAppDependency(sourceRoot / "paddle", "paddle") && ok;
    ok = checkControllerBoundary(sourceRoot) && ok;
    ok = checkAppBoundary(sourceRoot) && ok;
    ok = checkMainWindowSplit(sourceRoot) && ok;
    return ok ? 0 : 1;
}
