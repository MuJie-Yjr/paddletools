#include "app/MainWindow.h"

#include <QApplication>
#include <QByteArray>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStringList>
#include <QTextStream>

namespace {

void applyDefaultUiScale() {
    if (!qEnvironmentVariableIsSet("QT_SCALE_FACTOR_ROUNDING_POLICY")) {
        qputenv("QT_SCALE_FACTOR_ROUNDING_POLICY", "PassThrough");
    }
    if (qEnvironmentVariableIsSet("QT_SCALE_FACTOR")) {
        return;
    }

    QByteArray scale = qgetenv("PPOCR_UI_SCALE").trimmed();
    if (scale.isEmpty()) {
        scale = "0.75";
    }
    qputenv("QT_SCALE_FACTOR", scale);
}

QString absoluteDir(const QString& path) {
    if (path.isEmpty()) {
        return {};
    }
    const QFileInfo info(path);
    return info.exists() ? info.absoluteFilePath() : QDir(path).absolutePath();
}

bool hasDir(const QString& baseDir, const QString& relativePath) {
    return QFileInfo(QDir(baseDir).filePath(relativePath)).isDir();
}

bool looksLikePpocrBaseDir(const QString& baseDir) {
    if (baseDir.isEmpty() || !QFileInfo(baseDir).isDir()) {
        return false;
    }
    if (hasDir(baseDir, QStringLiteral("PaddleOCR")) && hasDir(baseDir, QStringLiteral("PaddleX"))) {
        return true;
    }
    if (hasDir(baseDir, QStringLiteral("model/infer"))) {
        return true;
    }
    return QFileInfo(QDir(baseDir).filePath(QStringLiteral("run_labeler.py"))).isFile();
}

QString resolveBaseDir() {
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates;
    const QString envBaseDir = QString::fromLocal8Bit(qgetenv("PPOCR_BASE_DIR")).trimmed();
    if (!envBaseDir.isEmpty()) {
        candidates.append(envBaseDir);
    }
    candidates.append(QDir::currentPath());
    candidates.append(appDir);
    candidates.append(QDir(appDir).absoluteFilePath(QStringLiteral("../..")));
    if (appDir.contains(QStringLiteral("build_vs2026"))) {
        candidates.append(QDir(appDir).absoluteFilePath(QStringLiteral("../../")));
    }
#ifdef PPOCR_PROJECT_ROOT
    candidates.append(QString::fromUtf8(PPOCR_PROJECT_ROOT));
#endif

    QSet<QString> seen;
    for (const auto& candidate : candidates) {
        const QString absolute = QFileInfo(absoluteDir(candidate)).absoluteFilePath();
        if (seen.contains(absolute)) {
            continue;
        }
        seen.insert(absolute);
        if (looksLikePpocrBaseDir(absolute)) {
            return absolute;
        }
    }
    return QFileInfo(appDir).absoluteFilePath();
}

}  // namespace

int main(int argc, char** argv) {
    applyDefaultUiScale();

    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("ppocr_workbench");

    QFile styleFile(":/resources/qss/dark.qss");
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    }

    const QStringList rawArguments = QCoreApplication::arguments();
    if (rawArguments.contains("--help") || rawArguments.contains("-h") || rawArguments.contains("-?")) {
        QTextStream(stdout)
            << "Usage: ppocr_workbench.exe [options]\n\n"
            << "Options:\n"
            << "  -?, -h, --help        Displays help on commandline options.\n"
            << "  --project <dir>       Open a PPOCR project directory.\n"
            << "  --last                Open the most recent PPOCR project.\n";
        return 0;
    }

    QCommandLineParser parser;
    QCommandLineOption projectOption("project", "Open a PPOCR project directory.", "dir");
    QCommandLineOption lastOption("last", "Open the most recent PPOCR project.");
    parser.addOption(projectOption);
    parser.addOption(lastOption);
    if (!parser.parse(rawArguments)) {
        QTextStream(stderr) << parser.errorText() << '\n';
        return 1;
    }

    const QString baseDir = resolveBaseDir();

    ppocr::MainWindow window(QFileInfo(baseDir).absoluteFilePath());
    if (parser.isSet(projectOption)) {
        window.openProjectPath(parser.value(projectOption));
    } else if (parser.isSet(lastOption)) {
        window.openLastProject();
    }
    window.show();
    return app.exec();
}
