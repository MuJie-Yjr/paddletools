#include "app/MainWindow.h"

#include "core/AnnotationOps.h"
#include "core/AssetImporter.h"
#include "core/ClassificationPrelabeler.h"
#include "core/CropGenerator.h"
#include "core/EnvironmentReport.h"
#include "core/Exporter.h"
#include "core/LayoutPrelabeler.h"
#include "core/OcrPrelabeler.h"
#include "core/ProjectRepository.h"
#include "core/RuntimePaths.h"
#include "core/TrainingPreflight.h"
#include "core/TrainingRunner.h"
#include "core/TrainingRunStore.h"
#include "core/TrainingTasks.h"
#include "core/Validator.h"
#include "paddle/PaddleClsEngine.h"
#include "paddle/PaddleDocLayoutEngine.h"
#include "paddle/PaddleInferenceRuntime.h"
#include "paddle/PaddleOcrEngine.h"
#include "paddle/PaddleProcess.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QClipboard>
#include <QCoreApplication>
#include <QColor>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeySequence>
#include <QLayout>
#include <QListView>
#include <QMap>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QProgressBar>
#include <QPushButton>
#include <QQuickItem>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScreen>
#include <QSet>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSize>
#include <QSplitter>
#include <QStatusBar>
#include <QStorageInfo>
#include <QStyle>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextCursor>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace ppocr {

struct DashboardLineSeries {
    QString name;
    QVector<double> values;
    QColor color;
};

class DashboardLineChart : public QWidget {
public:
    explicit DashboardLineChart(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(250);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setData(const QStringList& labels, const QVector<DashboardLineSeries>& series) {
        labels_ = labels;
        series_ = series;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor("#151619"));

        const QRectF plot(56, 44, qMax(10, width() - 88), qMax(10, height() - 86));
        QPen gridPen(QColor("#2b2d33"), 1);
        gridPen.setStyle(Qt::DashLine);
        painter.setPen(gridPen);
        for (int i = 0; i < 5; ++i) {
            const qreal y = plot.top() + plot.height() * i / 4.0;
            painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        }
        const int labelCount = qMax(1, labels_.size());
        for (int i = 0; i < labelCount; ++i) {
            const qreal x = plot.left() + plot.width() * i / qMax(1, labelCount - 1);
            painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }

        double maximum = 1.0;
        for (const auto& series : series_) {
            for (double value : series.values) {
                maximum = qMax(maximum, value);
            }
        }

        painter.setPen(QColor("#9ca3af"));
        QFont font = painter.font();
        font.setPointSize(8);
        painter.setFont(font);
        painter.drawText(QRectF(8, plot.top() - 8, 36, 18), Qt::AlignRight | Qt::AlignVCenter, QString::number(static_cast<int>(maximum)));
        painter.drawText(QRectF(8, plot.bottom() - 9, 36, 18), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("0"));
        for (int i = 0; i < labels_.size(); ++i) {
            const qreal x = plot.left() + plot.width() * i / qMax(1, labels_.size() - 1);
            painter.drawText(QRectF(x - 32, plot.bottom() + 12, 64, 18), Qt::AlignCenter, labels_.at(i));
        }

        qreal legendX = plot.left() + 8;
        for (const auto& series : series_) {
            painter.setBrush(series.color);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(QPointF(legendX, 24), 4, 4);
            painter.setPen(QColor("#e7e9ee"));
            painter.drawText(QPointF(legendX + 12, 28), series.name);
            legendX += qMax<qreal>(86, painter.fontMetrics().horizontalAdvance(series.name) + 34);
        }

        for (const auto& series : series_) {
            if (series.values.isEmpty()) {
                continue;
            }
            QPainterPath path;
            QPainterPath fillPath;
            for (int i = 0; i < series.values.size(); ++i) {
                const qreal x = plot.left() + plot.width() * i / qMax(1, series.values.size() - 1);
                const qreal y = plot.bottom() - (series.values.at(i) / maximum) * plot.height();
                if (i == 0) {
                    path.moveTo(x, y);
                    fillPath.moveTo(x, plot.bottom());
                    fillPath.lineTo(x, y);
                } else {
                    path.lineTo(x, y);
                    fillPath.lineTo(x, y);
                }
            }
            fillPath.lineTo(plot.right(), plot.bottom());
            fillPath.closeSubpath();
            QColor fill = series.color;
            fill.setAlpha(70);
            painter.fillPath(fillPath, fill);
            painter.setPen(QPen(series.color, 2));
            painter.drawPath(path);
            painter.setBrush(series.color);
            painter.setPen(Qt::NoPen);
            for (int i = 0; i < series.values.size(); ++i) {
                const qreal x = plot.left() + plot.width() * i / qMax(1, series.values.size() - 1);
                const qreal y = plot.bottom() - (series.values.at(i) / maximum) * plot.height();
                painter.drawEllipse(QPointF(x, y), 3.4, 3.4);
            }
        }
    }

private:
    QStringList labels_;
    QVector<DashboardLineSeries> series_;
};

struct DashboardDonutSegment {
    QString name;
    int value = 0;
    QColor color;
};

class DashboardDonutChart : public QWidget {
public:
    explicit DashboardDonutChart(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(250);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setData(const QVector<DashboardDonutSegment>& segments) {
        segments_ = segments;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor("#151619"));

        int total = 0;
        for (const auto& segment : segments_) {
            total += segment.value;
        }
        total = qMax(1, total);

        const qreal side = qMin<qreal>(height() - 68, width() * 0.48);
        const QRectF arcRect(36, 54, qMax<qreal>(80, side), qMax<qreal>(80, side));
        const int penWidth = qMax(18, static_cast<int>(arcRect.width() * 0.16));
        int startAngle = 90 * 16;
        for (const auto& segment : segments_) {
            const int span = -static_cast<int>(360.0 * 16.0 * segment.value / total);
            painter.setPen(QPen(segment.color, penWidth, Qt::SolidLine, Qt::FlatCap));
            painter.drawArc(arcRect, startAngle, span);
            startAngle += span;
        }

        QFont font = painter.font();
        font.setPointSize(20);
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(QColor("#f4f6f8"));
        painter.drawText(arcRect, Qt::AlignCenter, QString::number(total));
        font.setPointSize(9);
        font.setBold(false);
        painter.setFont(font);
        painter.setPen(QColor("#9ca3af"));
        painter.drawText(arcRect.adjusted(0, 42, 0, 42), Qt::AlignCenter, QStringLiteral("总任务"));

        qreal legendX = arcRect.right() + 40;
        qreal legendY = arcRect.top() + 28;
        for (const auto& segment : segments_) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(segment.color);
            painter.drawEllipse(QPointF(legendX, legendY - 4), 5, 5);
            painter.setPen(QColor("#e7e9ee"));
            const double percent = 100.0 * segment.value / total;
            painter.drawText(QPointF(legendX + 18, legendY + 1), QStringLiteral("%1    %2 (%3%)")
                    .arg(segment.name)
                    .arg(segment.value)
                    .arg(percent, 0, 'f', 1));
            legendY += 34;
        }
    }

private:
    QVector<DashboardDonutSegment> segments_;
};

class DashboardSparklineCard : public QWidget {
public:
    DashboardSparklineCard(const QString& title, const QColor& color, QWidget* parent = nullptr)
        : QWidget(parent), title_(title), color_(color) {
        setObjectName("OverviewMiniMetric");
        setAttribute(Qt::WA_StyledBackground, true);
        setMinimumSize(110, 180);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setData(const QString& value, const QString& subtitle, const QVector<double>& values) {
        value_ = value;
        subtitle_ = subtitle;
        values_ = values;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor("#181b1e"));
        painter.setPen(QColor("#c8cdd6"));
        painter.drawText(14, 24, title_);
        QFont font = painter.font();
        font.setPointSize(18);
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(QColor("#ffffff"));
        painter.drawText(14, 54, value_.isEmpty() ? QStringLiteral("--") : value_);
        font.setPointSize(8);
        font.setBold(false);
        painter.setFont(font);
        painter.setPen(QColor("#9ca3af"));
        painter.drawText(QRectF(14, height() - 26, width() - 28, 18), Qt::AlignLeft | Qt::AlignVCenter, subtitle_);

        if (values_.isEmpty()) {
            return;
        }
        const QRectF plot(14, qMax(72, height() - 84), qMax(10, width() - 28), 38);
        double maximum = 1.0;
        for (double value : values_) {
            maximum = qMax(maximum, value);
        }
        QPainterPath path;
        for (int i = 0; i < values_.size(); ++i) {
            const qreal x = plot.left() + plot.width() * i / qMax(1, values_.size() - 1);
            const qreal y = plot.bottom() - values_.at(i) / maximum * plot.height();
            i == 0 ? path.moveTo(x, y) : path.lineTo(x, y);
        }
        painter.setPen(QPen(color_, 2));
        painter.drawPath(path);
    }

private:
    QString title_;
    QColor color_;
    QString value_ = QStringLiteral("--");
    QString subtitle_;
    QVector<double> values_;
};

class TrainingPlaceholderChart : public QWidget {
public:
    explicit TrainingPlaceholderChart(const QString& title, QWidget* parent = nullptr)
        : QWidget(parent), title_(title) {
        setObjectName("TrainingChart");
        setMinimumHeight(210);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setValues(const QVector<double>& values, const QString& latestText = QString()) {
        values_ = values;
        latestText_ = latestText;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor("#151619"));
        painter.setPen(QColor("#e7e9ee"));
        painter.drawText(16, 24, title_);
        const QRectF plot(48, 46, qMax(10, width() - 70), qMax(10, height() - 72));
        QPen gridPen(QColor("#2b2d33"), 1);
        gridPen.setStyle(Qt::DashLine);
        painter.setPen(gridPen);
        for (int i = 0; i < 5; ++i) {
            const qreal y = plot.top() + i * plot.height() / 4.0;
            const qreal x = plot.left() + i * plot.width() / 4.0;
            painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
            painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }
        if (values_.isEmpty()) {
            painter.setPen(QColor("#9ca3af"));
            painter.drawText(plot, Qt::AlignCenter, QStringLiteral("等待训练指标"));
            return;
        }

        double minimum = values_.first();
        double maximum = values_.first();
        for (double value : values_) {
            if (!std::isfinite(value)) {
                continue;
            }
            minimum = qMin(minimum, value);
            maximum = qMax(maximum, value);
        }
        if (qFuzzyCompare(minimum, maximum)) {
            const double padding = qMax(1.0, std::abs(maximum)) * 0.05;
            minimum -= padding;
            maximum += padding;
        }

        painter.setPen(QColor("#9ca3af"));
        QFont smallFont = painter.font();
        smallFont.setPointSize(8);
        painter.setFont(smallFont);
        painter.drawText(QRectF(2, plot.top() - 8, 42, 18), Qt::AlignRight | Qt::AlignVCenter, QString::number(maximum, 'g', 4));
        painter.drawText(QRectF(2, plot.bottom() - 9, 42, 18), Qt::AlignRight | Qt::AlignVCenter, QString::number(minimum, 'g', 4));
        if (!latestText_.isEmpty()) {
            painter.drawText(QRectF(plot.left(), 14, plot.width(), 18), Qt::AlignRight | Qt::AlignVCenter, latestText_);
        }

        QPainterPath path;
        for (int i = 0; i < values_.size(); ++i) {
            const double normalized = (values_.at(i) - minimum) / (maximum - minimum);
            const qreal x = plot.left() + plot.width() * i / qMax(1, values_.size() - 1);
            const qreal y = plot.bottom() - normalized * plot.height();
            if (i == 0) {
                path.moveTo(x, y);
            } else {
                path.lineTo(x, y);
            }
        }
        QColor accent("#46d39a");
        QPainterPath fillPath(path);
        fillPath.lineTo(plot.right(), plot.bottom());
        fillPath.lineTo(plot.left(), plot.bottom());
        fillPath.closeSubpath();
        QColor fill = accent;
        fill.setAlpha(45);
        painter.fillPath(fillPath, fill);
        painter.setPen(QPen(accent, 2));
        painter.drawPath(path);
        painter.setBrush(accent);
        painter.setPen(Qt::NoPen);
        const int step = qMax(1, values_.size() / 24);
        for (int i = 0; i < values_.size(); i += step) {
            const double normalized = (values_.at(i) - minimum) / (maximum - minimum);
            const qreal x = plot.left() + plot.width() * i / qMax(1, values_.size() - 1);
            const qreal y = plot.bottom() - normalized * plot.height();
            painter.drawEllipse(QPointF(x, y), 2.8, 2.8);
        }
        const int last = values_.size() - 1;
        const double normalized = (values_.at(last) - minimum) / (maximum - minimum);
        painter.drawEllipse(QPointF(
            plot.left() + plot.width() * last / qMax(1, values_.size() - 1),
            plot.bottom() - normalized * plot.height()), 4.2, 4.2);
        return;
        painter.setPen(QColor("#9ca3af"));
        painter.drawText(plot, Qt::AlignCenter, QStringLiteral("等待训练指标"));
    }

private:
    QString title_;
    QString latestText_;
    QVector<double> values_;
};

namespace {

class WorkbenchStackedWidget : public QStackedWidget {
public:
    using QStackedWidget::QStackedWidget;

    QSize sizeHint() const override {
        return QSize(1100, 700);
    }

    QSize minimumSizeHint() const override {
        return QSize(720, 480);
    }
};

int trainingTaskOverviewColumnsForWidth(int availableWidth) {
    return qBound(1, availableWidth > 0 ? availableWidth / 340 : 4, 4);
}

QString versionListText(const QJsonObject& version) {
    QStringList flags;
    if (version.value("is_current").toBool()) {
        flags.append("current");
    }
    if (version.value("is_best").toBool()) {
        flags.append("best");
    }
    const QString flagText = flags.isEmpty() ? QString() : QString(" [%1]").arg(flags.join(", "));
    const QString finished = version.value("finished_at").toString();
    const QString time = finished.isEmpty() ? version.value("started_at").toString() : finished;
    const QJsonObject metrics = version.value("metrics").toObject();
    bool hasMetric = false;
    const double metric = TrainingRunStore::mainMetricValue(metrics, version.value("task_kind").toString(), &hasMetric);
    const QString metricText = hasMetric ? QString(" | score %1").arg(metric, 0, 'f', 4) : QString();
    return QString("%1 | %2 | samples %3%4 | %5%6")
        .arg(version.value("status").toString("unknown"),
            version.value("version_id").toString(),
            QString::number(version.value("sample_count").toInt()),
            metricText,
            time,
            flagText);
}

QString inferenceDirFromVersion(const TrainingTaskSpec& task, const QJsonObject& version) {
    const QString explicitDir = version.value("inference_model_dir").toString();
    QString reason;
    if (!explicitDir.isEmpty() && PaddleInferenceRuntime::modelDirLooksUsable(explicitDir, &reason)) {
        return explicitDir;
    }
    const QString root = version.value("version_dir").toString(version.value("output_dir").toString());
    if (root.isEmpty()) {
        return {};
    }
    const QString preferred = QDir(root).filePath(task.inferDirRel);
    if (PaddleInferenceRuntime::modelDirLooksUsable(preferred, &reason)) {
        return preferred;
    }
    return {};
}

QList<TrainingTaskSpec> tasksForKind(const QString& kind) {
    QList<TrainingTaskSpec> result;
    for (const auto& task : trainingTasks()) {
        if (task.trainSupported && task.kind == kind) {
            result.append(task);
        }
    }
    return result;
}

QList<TrainingTaskSpec> classificationTasksForPrediction(const QString& predictionTask) {
    QList<TrainingTaskSpec> result;
    for (const auto& task : trainingTasks()) {
        if (!task.trainSupported || task.kind != QStringLiteral("cls")) {
            continue;
        }
        if (predictionTask == QStringLiteral("doc_orientation") && task.key.startsWith(QStringLiteral("doc_ori"))) {
            result.append(task);
        } else if (predictionTask == QStringLiteral("textline_orientation") && task.key.startsWith(QStringLiteral("textline_ori"))) {
            result.append(task);
        } else if (predictionTask == QStringLiteral("table_classification") && task.key.startsWith(QStringLiteral("table_cls"))) {
            result.append(task);
        }
    }
    return result;
}

struct LocalModelChoice {
    QString name;
    QString title;
    QString path;
    int order = 999;
};

QString localModelDisplayName(const QString& name) {
    if (name == QStringLiteral("PP-OCRv5_server_det_infer")) {
        return QStringLiteral("PP-OCRv5 server 检测");
    }
    if (name == QStringLiteral("PP-OCRv5_mobile_det_infer")) {
        return QStringLiteral("PP-OCRv5 mobile 检测");
    }
    if (name == QStringLiteral("PP-OCRv4_server_det_infer")) {
        return QStringLiteral("PP-OCRv4 server 检测");
    }
    if (name == QStringLiteral("PP-OCRv4_mobile_det_infer")) {
        return QStringLiteral("PP-OCRv4 mobile 检测");
    }
    if (name == QStringLiteral("PP-OCRv5_server_rec_infer")) {
        return QStringLiteral("PP-OCRv5 server 识别");
    }
    if (name == QStringLiteral("PP-OCRv5_mobile_rec_infer")) {
        return QStringLiteral("PP-OCRv5 mobile 识别");
    }
    if (name == QStringLiteral("PP-OCRv4_server_rec_doc_infer")) {
        return QStringLiteral("PP-OCRv4 server 文档识别");
    }
    if (name == QStringLiteral("PP-OCRv4_server_rec_infer")) {
        return QStringLiteral("PP-OCRv4 server 识别");
    }
    if (name == QStringLiteral("PP-OCRv4_mobile_rec_infer")) {
        return QStringLiteral("PP-OCRv4 mobile 识别");
    }
    if (name == QStringLiteral("en_PP-OCRv4_mobile_rec_infer")) {
        return QStringLiteral("PP-OCRv4 mobile 英文/数字识别");
    }
    if (name == QStringLiteral("PP-LCNet_x1_0_textline_ori_infer")) {
        return QStringLiteral("PP-LCNet x1.0 文本行方向");
    }
    if (name == QStringLiteral("PP-LCNet_x0_25_textline_ori_infer")) {
        return QStringLiteral("PP-LCNet x0.25 文本行方向");
    }
    if (name == QStringLiteral("PP-LCNet_x1_0_doc_ori_infer")) {
        return QStringLiteral("PP-LCNet x1.0 文档方向");
    }
    if (name == QStringLiteral("PP-LCNet_x1_0_table_cls_infer")) {
        return QStringLiteral("PP-LCNet x1.0 表格分类");
    }
    if (name == QStringLiteral("UVDoc_infer")) {
        return QStringLiteral("UVDoc 文档图像矫正");
    }
    if (name == QStringLiteral("PP-DocLayout_plus-L_infer")) {
        return QStringLiteral("PP-DocLayout plus-L 版面");
    }
    if (name == QStringLiteral("PP-DocLayout-L_infer")) {
        return QStringLiteral("PP-DocLayout-L 版面");
    }
    if (name == QStringLiteral("PP-DocLayout-M_infer")) {
        return QStringLiteral("PP-DocLayout-M 版面");
    }
    if (name == QStringLiteral("PP-DocLayout-S_infer")) {
        return QStringLiteral("PP-DocLayout-S 版面");
    }
    if (name == QStringLiteral("PP-DocBlockLayout_infer")) {
        return QStringLiteral("PP-DocBlockLayout 块级版面");
    }
    return name;
}

QStringList localModelOrder(const QString& category) {
    if (category == QStringLiteral("det")) {
        return {
            QStringLiteral("PP-OCRv5_server_det_infer"),
            QStringLiteral("PP-OCRv5_mobile_det_infer"),
            QStringLiteral("PP-OCRv4_server_det_infer"),
            QStringLiteral("PP-OCRv4_mobile_det_infer"),
        };
    }
    if (category == QStringLiteral("rec")) {
        return {
            QStringLiteral("PP-OCRv5_server_rec_infer"),
            QStringLiteral("PP-OCRv5_mobile_rec_infer"),
            QStringLiteral("PP-OCRv4_server_rec_doc_infer"),
            QStringLiteral("PP-OCRv4_server_rec_infer"),
            QStringLiteral("PP-OCRv4_mobile_rec_infer"),
            QStringLiteral("en_PP-OCRv4_mobile_rec_infer"),
        };
    }
    if (category == QStringLiteral("cls") || category == QStringLiteral("textline_orientation")) {
        return {
            QStringLiteral("PP-LCNet_x1_0_textline_ori_infer"),
            QStringLiteral("PP-LCNet_x0_25_textline_ori_infer"),
        };
    }
    if (category == QStringLiteral("doc_orientation")) {
        return {QStringLiteral("PP-LCNet_x1_0_doc_ori_infer")};
    }
    if (category == QStringLiteral("table_classification")) {
        return {QStringLiteral("PP-LCNet_x1_0_table_cls_infer")};
    }
    if (category == QStringLiteral("doc_unwarping")) {
        return {QStringLiteral("UVDoc_infer")};
    }
    if (category == QStringLiteral("layout")) {
        return {
            QStringLiteral("PP-DocLayout_plus-L_infer"),
            QStringLiteral("PP-DocLayout-L_infer"),
            QStringLiteral("PP-DocLayout-M_infer"),
            QStringLiteral("PP-DocLayout-S_infer"),
            QStringLiteral("PP-DocBlockLayout_infer"),
        };
    }
    return {};
}

QString inferModelCategory(const QString& name) {
    const QString lower = name.toLower();
    if (lower.contains(QStringLiteral("table_cls"))) {
        return QStringLiteral("table_classification");
    }
    if (lower.contains(QStringLiteral("textline_ori"))) {
        return QStringLiteral("textline_orientation");
    }
    if (lower.contains(QStringLiteral("doc_ori"))) {
        return QStringLiteral("doc_orientation");
    }
    if (lower == QStringLiteral("uvdoc_infer") || lower.contains(QStringLiteral("unwarping"))) {
        return QStringLiteral("doc_unwarping");
    }
    if (lower.contains(QStringLiteral("doclayout")) || lower.contains(QStringLiteral("docblocklayout"))) {
        return QStringLiteral("layout");
    }
    if (lower.endsWith(QStringLiteral("_det_infer")) || lower.contains(QStringLiteral("_det_"))) {
        return QStringLiteral("det");
    }
    if (lower.endsWith(QStringLiteral("_rec_infer")) || lower.contains(QStringLiteral("_rec_"))) {
        return QStringLiteral("rec");
    }
    return QStringLiteral("other");
}

QString inferredModelDir(const QFileInfo& outerDir) {
    const QDir dir(outerDir.absoluteFilePath());
    const QString nested = dir.filePath(outerDir.fileName());
    for (const QString& candidate : {nested, outerDir.absoluteFilePath()}) {
        const QDir candidateDir(candidate);
        if (QFileInfo::exists(candidateDir.filePath(QStringLiteral("inference.yml")))
            || QFileInfo::exists(candidateDir.filePath(QStringLiteral("inference.json")))
            || QFileInfo::exists(candidateDir.filePath(QStringLiteral("model.json")))) {
            return candidate;
        }
    }
    return PaddleInferenceRuntime::resolveModelDir(outerDir.absoluteFilePath());
}

QStringList localModelInferRoots(const QString& baseDir) {
    QStringList roots;
    auto addRoot = [&roots](const QString& path) {
        if (path.trimmed().isEmpty()) {
            return;
        }
        const QFileInfo info(path);
        if (!info.exists() || !info.isDir()) {
            return;
        }
        const QString absolute = QDir::cleanPath(info.absoluteFilePath());
        if (!roots.contains(absolute, Qt::CaseInsensitive)) {
            roots.append(absolute);
        }
    };

    addRoot(qEnvironmentVariable("PPOCR_MODEL_INFER_DIR"));
    addRoot(QDir(baseDir).filePath(QStringLiteral("model/infer")));
    addRoot(QDir(baseDir).filePath(QStringLiteral("../PaddleX-3.5.2/PaddleX-3.5.2/paddlelabel/model/infer")));
    const QString paddleLabelDir = qEnvironmentVariable("PPOCR_PADDLELABEL_DIR");
    if (!paddleLabelDir.isEmpty()) {
        addRoot(QDir(paddleLabelDir).filePath(QStringLiteral("model/infer")));
    }
    return roots;
}

QList<LocalModelChoice> localModelChoices(const QString& baseDir, const QString& category) {
    QList<LocalModelChoice> choices;
    if (category.isEmpty()) {
        return choices;
    }
    const QStringList order = localModelOrder(category);
    QSet<QString> seenNames;
    for (const QString& root : localModelInferRoots(baseDir)) {
        const QDir dir(root);
        const QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo& entry : entries) {
            const QString modelName = entry.fileName();
            const QString modelCategory = inferModelCategory(modelName);
            if (modelCategory != category
                && !(category == QStringLiteral("cls") && modelCategory == QStringLiteral("textline_orientation"))) {
                continue;
            }
            const QString seenKey = modelCategory + QChar('|') + modelName.toLower();
            if (seenNames.contains(seenKey)) {
                continue;
            }
            const QString modelDir = inferredModelDir(entry);
            QString reason;
            if (!PaddleInferenceRuntime::modelDirLooksUsable(modelDir, &reason)) {
                continue;
            }
            seenNames.insert(seenKey);
            const int orderIndex = order.indexOf(modelName);
            if (!order.isEmpty() && orderIndex < 0) {
                continue;
            }
            choices.append(LocalModelChoice{
                modelName,
                localModelDisplayName(modelName),
                modelDir,
                orderIndex < 0 ? 999 : orderIndex,
            });
        }
    }
    std::sort(choices.begin(), choices.end(), [](const LocalModelChoice& left, const LocalModelChoice& right) {
        if (left.order != right.order) {
            return left.order < right.order;
        }
        return left.name.toLower() < right.name.toLower();
    });
    return choices;
}

QString baseDirFromModelDir(const QString& modelDir) {
    if (modelDir.trimmed().isEmpty()) {
        return RuntimePaths::defaultBaseDir();
    }
    const QString resolved = PaddleInferenceRuntime::resolveModelDir(modelDir);
    QDir current(QFileInfo(resolved).isDir() ? QFileInfo(resolved).absoluteFilePath() : QFileInfo(resolved).absolutePath());
    for (int i = 0; i < 8; ++i) {
        if (current.dirName().compare(QStringLiteral("infer"), Qt::CaseInsensitive) == 0) {
            QDir parent = current;
            if (parent.cdUp() && parent.dirName().compare(QStringLiteral("model"), Qt::CaseInsensitive) == 0 && parent.cdUp()) {
                return parent.absolutePath();
            }
        }
        if (!current.cdUp()) {
            break;
        }
    }
    return RuntimePaths::defaultBaseDir();
}

QString localModelCategoryFromPath(const QString& modelDir) {
    const QString resolved = PaddleInferenceRuntime::resolveModelDir(modelDir);
    const QFileInfo info(resolved);
    QString category = inferModelCategory(info.fileName());
    if (category != QStringLiteral("other")) {
        return category;
    }
    category = inferModelCategory(info.dir().dirName());
    return category == QStringLiteral("other") ? QString() : category;
}

QString modelChoiceTitleFromPath(const QString& modelDir) {
    const QString resolved = PaddleInferenceRuntime::resolveModelDir(modelDir);
    const QFileInfo info(resolved);
    QString modelName = info.fileName();
    if (inferModelCategory(modelName) == QStringLiteral("other")) {
        modelName = info.dir().dirName();
    }
    return localModelDisplayName(modelName);
}

QString localModelChoiceText(const QString& title) {
    return QStringLiteral("%1 | 本地").arg(title);
}

void addLocalModelChoices(QComboBox* combo, const QString& category, const QString& baseDir) {
    if (!combo || category.isEmpty()) {
        return;
    }
    for (const LocalModelChoice& choice : localModelChoices(baseDir, category)) {
        if (combo->findData(choice.path) >= 0
            || combo->findData(PaddleInferenceRuntime::resolveModelDir(choice.path)) >= 0) {
            continue;
        }
        const int index = combo->count();
        combo->addItem(localModelChoiceText(choice.title), choice.path);
        combo->setItemData(index, QDir::toNativeSeparators(choice.path), Qt::ToolTipRole);
    }
}

void addPredictionModelChoices(
    QComboBox* combo,
    const ProjectContext* context,
    const QList<TrainingTaskSpec>& tasks,
    const QString& defaultModelDir,
    bool optional = false,
    const QString& optionalText = QStringLiteral("不启用")) {
    if (!combo) {
        return;
    }
    Q_UNUSED(context);
    Q_UNUSED(tasks);
    const QString previous = combo->currentData().toString();
    QSignalBlocker blocker(combo);
    combo->clear();
    if (optional) {
        combo->addItem(optionalText, QString());
    }
    if (!defaultModelDir.isEmpty()) {
        const QString modelDir = PaddleInferenceRuntime::resolveModelDir(defaultModelDir);
        const int index = combo->count();
        combo->addItem(localModelChoiceText(modelChoiceTitleFromPath(modelDir)), modelDir);
        combo->setItemData(index, QDir::toNativeSeparators(modelDir), Qt::ToolTipRole);
    }
    addLocalModelChoices(combo, localModelCategoryFromPath(defaultModelDir), baseDirFromModelDir(defaultModelDir));
    const int previousIndex = previous.isEmpty() ? -1 : combo->findData(previous);
    if (previousIndex >= 0) {
        combo->setCurrentIndex(previousIndex);
    }
}

void addStandaloneModelChoice(
    QComboBox* combo,
    const QString& defaultModelDir,
    bool optional = true,
    const QString& optionalText = QStringLiteral("不启用")) {
    if (!combo) {
        return;
    }
    const QString previous = combo->currentData().toString();
    QSignalBlocker blocker(combo);
    combo->clear();
    if (optional) {
        combo->addItem(optionalText, QString());
    }
    if (!defaultModelDir.isEmpty()) {
        const QString modelDir = PaddleInferenceRuntime::resolveModelDir(defaultModelDir);
        const int index = combo->count();
        combo->addItem(localModelChoiceText(modelChoiceTitleFromPath(modelDir)), modelDir);
        combo->setItemData(index, QDir::toNativeSeparators(modelDir), Qt::ToolTipRole);
    }
    addLocalModelChoices(combo, localModelCategoryFromPath(defaultModelDir), baseDirFromModelDir(defaultModelDir));
    const int previousIndex = previous.isEmpty() ? -1 : combo->findData(previous);
    if (previousIndex >= 0) {
        combo->setCurrentIndex(previousIndex);
    }
}

QString canonicalExistingOrCleanPath(const QString& path) {
    if (path.trimmed().isEmpty()) {
        return {};
    }
    const QFileInfo info(path);
    if (info.exists()) {
        return QDir::cleanPath(info.absoluteFilePath());
    }
    return QDir::cleanPath(path);
}

QSet<QString> equivalentModelPaths(const QString& path) {
    QSet<QString> paths;
    const QString canonical = canonicalExistingOrCleanPath(path);
    if (!canonical.isEmpty()) {
        paths.insert(canonical);
    }
    const QString resolved = canonicalExistingOrCleanPath(PaddleInferenceRuntime::resolveModelDir(path));
    if (!resolved.isEmpty()) {
        paths.insert(resolved);
    }
    return paths;
}

QString preferredModelDir(const ProjectContext* context, const TrainingTaskSpec& task, const QString& defaultModelDir) {
    if (!context) {
        return defaultModelDir;
    }
    const QJsonObject manifest = TrainingRunStore::loadVersionManifest(*context, task.key);
    const QJsonArray versions = manifest.value("versions").toArray();
    const QStringList preferredIds{
        manifest.value("current_version_id").toString(),
        manifest.value("best_version_id").toString(),
    };
    for (const QString& preferredId : preferredIds) {
        if (preferredId.isEmpty()) {
            continue;
        }
        for (const auto& value : versions) {
            const QJsonObject version = value.toObject();
            if (version.value("status").toString() == "success"
                && version.value("version_id").toString() == preferredId) {
                const QString modelDir = inferenceDirFromVersion(task, version);
                if (!modelDir.isEmpty()) {
                    return modelDir;
                }
            }
        }
    }
    for (int i = versions.size() - 1; i >= 0; --i) {
        const QJsonObject version = versions.at(i).toObject();
        if (version.value("status").toString() != "success") {
            continue;
        }
        const QString modelDir = inferenceDirFromVersion(task, version);
        if (!modelDir.isEmpty()) {
            return modelDir;
        }
    }
    return defaultModelDir;
}

QString preferredModelDir(const ProjectContext* context, const QList<TrainingTaskSpec>& tasks, const QString& defaultModelDir) {
    if (!context) {
        return defaultModelDir;
    }
    for (const auto& task : tasks) {
        const QString modelDir = preferredModelDir(context, task, QString());
        if (!modelDir.isEmpty()) {
            return modelDir;
        }
    }
    return defaultModelDir;
}

QString splitDisplayText(const QString& value);
QString statusDisplayText(const QString& value);

QString pageListText(const PageInfo& page) {
    const QString name = QFileInfo(page.imagePath).fileName().isEmpty()
        ? page.assetId
        : QFileInfo(page.imagePath).fileName();
    return QString("%1\n%2 x %3    %4    %5")
        .arg(name)
        .arg(page.width)
        .arg(page.height)
        .arg(page.status.isEmpty() ? QStringLiteral("unlabeled") : page.status)
        .arg(page.split.isEmpty() ? QStringLiteral("train") : page.split);
}

QJsonObject findRegion(const QJsonObject& annotation, const QString& regionId) {
    for (const auto& value : annotation.value("regions").toArray()) {
        const QJsonObject region = value.toObject();
        if (region.value("id").toString() == regionId) {
            return region;
        }
    }
    return {};
}

QJsonArray pointsFromVariant(const QVariant& value) {
    QJsonArray points;
    for (const auto& item : value.toList()) {
        const auto pair = item.toList();
        if (pair.size() < 2) {
            continue;
        }
        points.append(QJsonArray{
            std::round(pair.at(0).toDouble() * 100.0) / 100.0,
            std::round(pair.at(1).toDouble() * 100.0) / 100.0,
        });
    }
    return points;
}

struct PredictionRow {
    QString kind;
    QString label;
    QString scoreText;
    QString source;
    double score = 0.0;
    bool hasScore = false;
};

struct TrainingMetricRow {
    QString time;
    QString epoch;
    QString step;
    QString loss;
    QString lr;
    QString accuracy;
    QString score;
    QString precisionRecall;
    QString raw;
};

QString boolText(bool value) {
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

QString splitDisplayText(const QString& value) {
    if (value == QStringLiteral("train")) {
        return QStringLiteral("训练集");
    }
    if (value == QStringLiteral("val")) {
        return QStringLiteral("验证集");
    }
    return value.isEmpty() ? QStringLiteral("未设置") : value;
}

QString statusDisplayText(const QString& value) {
    if (value == QStringLiteral("unlabeled")) {
        return QStringLiteral("未标注");
    }
    if (value == QStringLiteral("labeled")) {
        return QStringLiteral("已标注");
    }
    if (value == QStringLiteral("validated")) {
        return QStringLiteral("已校验");
    }
    return value.isEmpty() ? QStringLiteral("未设置") : value;
}

QString comboStoredValue(const QComboBox* combo) {
    if (!combo) {
        return {};
    }
    const QVariant data = combo->currentData();
    if (data.isValid()) {
        return data.toString();
    }
    return combo->currentText();
}

QLabel* sectionLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName("SectionTitle");
    return label;
}

QLabel* mutedLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName("Muted");
    return label;
}

QWidget* workbenchCard(QWidget* parent, const QString& objectName = QStringLiteral("WorkbenchPanel")) {
    auto* card = new QWidget(parent);
    card->setObjectName(objectName);
    card->setAttribute(Qt::WA_StyledBackground, true);
    return card;
}

QPushButton* primaryButton(const QString& text, QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setObjectName("Primary");
    return button;
}

QPushButton* dangerButton(const QString& text, QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setObjectName("Danger");
    return button;
}

QTableWidget* tableWidget(int columns, const QStringList& headers, QWidget* parent) {
    auto* table = new QTableWidget(0, columns, parent);
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(false);
    return table;
}

QColor stateTextColor(const QString& text) {
    if (text.contains(QStringLiteral("正常"))
        || text.contains(QStringLiteral("存在"))
        || text.contains(QStringLiteral("可用"))
        || text.contains(QStringLiteral("已完成"))
        || text.contains(QStringLiteral("已导出"))
        || text.contains(QStringLiteral("通过"))) {
        return QColor(QStringLiteral("#46d39a"));
    }
    if (text.contains(QStringLiteral("失败"))
        || text.contains(QStringLiteral("缺失"))
        || text.contains(QStringLiteral("错误"))) {
        return QColor(QStringLiteral("#ff8b8b"));
    }
    if (text.contains(QStringLiteral("未"))
        || text.contains(QStringLiteral("待"))
        || text.contains(QStringLiteral("需要"))
        || text.contains(QStringLiteral("部分"))
        || text.contains(QStringLiteral("进行中"))) {
        return QColor(QStringLiteral("#f2c078"));
    }
    return {};
}

void appendTableRow(QTableWidget* table, const QStringList& values, int statusColumn = 1) {
    if (!table) {
        return;
    }
    const int row = table->rowCount();
    table->insertRow(row);
    for (int column = 0; column < table->columnCount(); ++column) {
        const QString text = column < values.size() ? values.at(column) : QString();
        auto* item = new QTableWidgetItem(text);
        item->setToolTip(text);
        if (column == statusColumn) {
            const QColor color = stateTextColor(text);
            if (color.isValid()) {
                item->setData(Qt::ForegroundRole, color);
            }
        }
        table->setItem(row, column, item);
    }
}

QString pathStateText(const QString& path) {
    return QFileInfo::exists(path) ? QStringLiteral("存在") : QStringLiteral("缺失");
}

QString nativePathText(const QString& path) {
    return path.isEmpty() ? QStringLiteral("-") : QDir::toNativeSeparators(path);
}

QString readTextPreview(const QString& path, qint64 maxBytes = 512 * 1024) {
    if (path.isEmpty()) {
        return QStringLiteral("未生成日志文件。");
    }
    QFile file(path);
    if (!file.exists()) {
        return QStringLiteral("文件不存在：%1").arg(nativePathText(path));
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QStringLiteral("无法读取：%1").arg(nativePathText(path));
    }
    QByteArray bytes = file.read(maxBytes);
    QString text = QString::fromUtf8(bytes);
    if (!file.atEnd()) {
        text.append(QStringLiteral("\n\n... 文件较大，仅显示前 %1 KB。").arg(maxBytes / 1024));
    }
    return text.isEmpty() ? QStringLiteral("文件为空：%1").arg(nativePathText(path)) : text;
}

int countFilesUnder(const QString& rootPath, const QStringList& filters = QStringList()) {
    if (!QFileInfo::exists(rootPath)) {
        return 0;
    }
    QDirIterator it(rootPath, filters, QDir::Files, QDirIterator::Subdirectories);
    int count = 0;
    while (it.hasNext()) {
        it.next();
        ++count;
    }
    return count;
}

QVector<double> resourceSparkValues(const std::optional<double>& value) {
    if (!value) {
        return {};
    }
    const double base = qMax(5.0, qMin(95.0, *value));
    return {
        qMax(1.0, qMin(100.0, base - 8.0)),
        qMax(1.0, qMin(100.0, base - 3.0)),
        qMax(1.0, qMin(100.0, base - 5.0)),
        qMax(1.0, qMin(100.0, base + 2.0)),
        qMax(1.0, qMin(100.0, base - 1.0)),
        qMax(1.0, qMin(100.0, base + 7.0)),
        qMax(1.0, qMin(100.0, base - 6.0)),
        qMax(1.0, qMin(100.0, base + 4.0)),
        qMax(1.0, qMin(100.0, base - 2.0)),
        qMax(1.0, qMin(100.0, base)),
    };
}

QString resourcePercentText(const std::optional<double>& value) {
    return value ? QStringLiteral("%1%").arg(*value, 0, 'f', 0) : QStringLiteral("--");
}

#ifdef Q_OS_WIN
quint64 fileTimeValue(const FILETIME& time) {
    ULARGE_INTEGER value;
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return value.QuadPart;
}
#endif

std::optional<double> currentCpuPercent() {
#ifdef Q_OS_WIN
    FILETIME idleTime;
    FILETIME kernelTime;
    FILETIME userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        return std::nullopt;
    }
    const quint64 idle = fileTimeValue(idleTime);
    const quint64 kernel = fileTimeValue(kernelTime);
    const quint64 user = fileTimeValue(userTime);
    static quint64 previousIdle = 0;
    static quint64 previousKernel = 0;
    static quint64 previousUser = 0;
    if (previousKernel == 0 && previousUser == 0) {
        previousIdle = idle;
        previousKernel = kernel;
        previousUser = user;
        return 0.0;
    }

    const quint64 idleDelta = idle - previousIdle;
    const quint64 kernelDelta = kernel - previousKernel;
    const quint64 userDelta = user - previousUser;
    previousIdle = idle;
    previousKernel = kernel;
    previousUser = user;

    const quint64 totalDelta = kernelDelta + userDelta;
    if (totalDelta == 0) {
        return 0.0;
    }
    return qMax(0.0, qMin(100.0, 100.0 * (1.0 - static_cast<double>(idleDelta) / static_cast<double>(totalDelta))));
#else
    return std::nullopt;
#endif
}

std::optional<double> currentMemoryPercent() {
#ifdef Q_OS_WIN
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx(&status)) {
        return std::nullopt;
    }
    return static_cast<double>(status.dwMemoryLoad);
#else
    return std::nullopt;
#endif
}

std::optional<double> currentGpuPercent() {
    QProcess process;
    process.start(QStringLiteral("nvidia-smi"), {
        QStringLiteral("--query-gpu=utilization.gpu"),
        QStringLiteral("--format=csv,noheader,nounits"),
    });
    if (!process.waitForStarted(500)) {
        return std::nullopt;
    }
    if (!process.waitForFinished(1500)) {
        process.kill();
        process.waitForFinished(300);
        return std::nullopt;
    }
    const QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    double total = 0.0;
    int count = 0;
    for (const auto& line : output.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts)) {
        bool ok = false;
        const double value = line.trimmed().toDouble(&ok);
        if (ok) {
            total += value;
            ++count;
        }
    }
    if (count == 0) {
        return std::nullopt;
    }
    return qMax(0.0, qMin(100.0, total / count));
}

struct DiskUsageSnapshot {
    std::optional<double> percent;
    QString subtitle;
};

DiskUsageSnapshot currentDiskUsage(const QString& root) {
    QStorageInfo storage(root);
    if (!storage.isValid() || storage.bytesTotal() <= 0) {
        return {std::nullopt, QStringLiteral("存储使用 无法读取")};
    }
    const double totalGb = static_cast<double>(storage.bytesTotal()) / (1024.0 * 1024.0 * 1024.0);
    const double usedGb = static_cast<double>(storage.bytesTotal() - storage.bytesAvailable()) / (1024.0 * 1024.0 * 1024.0);
    const double percent = 100.0 * usedGb / totalGb;
    return {
        qMax(0.0, qMin(100.0, percent)),
        QStringLiteral("存储使用 %1 / %2 GB").arg(usedGb, 0, 'f', 0).arg(totalGb, 0, 'f', 0),
    };
}

QString statusLabelText(const QString& status) {
    if (status == QStringLiteral("running")) {
        return QStringLiteral("进行中");
    }
    if (status == QStringLiteral("success")) {
        return QStringLiteral("已完成");
    }
    if (status == QStringLiteral("failed")) {
        return QStringLiteral("失败");
    }
    if (status == QStringLiteral("stopped")) {
        return QStringLiteral("已停止");
    }
    if (status.isEmpty()) {
        return QStringLiteral("未开始");
    }
    return status;
}

QString taskKindLabel(const QString& kind) {
    if (kind == QStringLiteral("det")) {
        return QStringLiteral("Det");
    }
    if (kind == QStringLiteral("rec")) {
        return QStringLiteral("Rec");
    }
    if (kind == QStringLiteral("cls")) {
        return QStringLiteral("Cls");
    }
    if (kind == QStringLiteral("layout")) {
        return QStringLiteral("Layout");
    }
    if (kind == QStringLiteral("uvdoc")) {
        return QStringLiteral("UVDoc");
    }
    return kind;
}

QString taskDatasetText(const TrainingTaskSpec& task) {
    return task.datasetName.isEmpty() ? QStringLiteral("-") : task.datasetName;
}

QString shortTaskTitle(const TrainingTaskSpec& task) {
    QString title = task.title;
    title.replace(QStringLiteral(" | "), QStringLiteral(" "));
    return title;
}

QDateTime parseStoreDateTime(const QString& text) {
    if (text.isEmpty()) {
        return {};
    }
    const QString clipped = text.left(19);
    QDateTime value = QDateTime::fromString(clipped, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!value.isValid()) {
        value = QDateTime::fromString(clipped, QStringLiteral("yyyy-MM-ddTHH:mm:ss"));
    }
    return value;
}

void clearLayoutItems(QLayout* layout) {
    if (!layout) {
        return;
    }
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        if (QLayout* childLayout = item->layout()) {
            clearLayoutItems(childLayout);
        }
        delete item;
    }
}

QDateTime storeObjectTime(const QJsonObject& object) {
    QDateTime value = parseStoreDateTime(object.value(QStringLiteral("finished_at")).toString());
    if (!value.isValid()) {
        value = parseStoreDateTime(object.value(QStringLiteral("started_at")).toString());
    }
    return value;
}

QJsonObject latestObjectByTime(const QJsonArray& items) {
    QJsonObject latest;
    QDateTime latestTime;
    for (const auto& value : items) {
        const QJsonObject object = value.toObject();
        const QDateTime time = storeObjectTime(object);
        if (latest.isEmpty() || (time.isValid() && (!latestTime.isValid() || time > latestTime))) {
            latest = object;
            latestTime = time;
        }
    }
    return latest;
}

QJsonObject latestSuccessfulVersion(const QJsonArray& versions) {
    QJsonArray successes;
    for (const auto& value : versions) {
        const QJsonObject version = value.toObject();
        if (version.value(QStringLiteral("status")).toString() == QStringLiteral("success")) {
            successes.append(version);
        }
    }
    return latestObjectByTime(successes);
}

QString taskOverviewStatusKey(
    const TrainingTaskSpec& task,
    const QJsonObject& manifest,
    const QJsonObject& latestRun) {
    if (!task.trainSupported) {
        return QStringLiteral("unsupported");
    }
    const QString runStatus = latestRun.value(QStringLiteral("status")).toString();
    if (runStatus == QStringLiteral("running")) {
        return QStringLiteral("running");
    }
    const QJsonObject latestSuccess = latestSuccessfulVersion(manifest.value(QStringLiteral("versions")).toArray());
    if (!latestSuccess.isEmpty()) {
        return QStringLiteral("success");
    }
    if (runStatus == QStringLiteral("failed")) {
        return QStringLiteral("failed");
    }
    if (runStatus == QStringLiteral("stopped")) {
        return QStringLiteral("stopped");
    }
    return QStringLiteral("idle");
}

QString trainingStatusObjectName(const QString& statusKey) {
    if (statusKey == QStringLiteral("success")) {
        return QStringLiteral("TrainingStatusDone");
    }
    if (statusKey == QStringLiteral("running") || statusKey == QStringLiteral("unsupported")) {
        return QStringLiteral("TrainingStatusWarn");
    }
    if (statusKey == QStringLiteral("failed")) {
        return QStringLiteral("TrainingStatusFailed");
    }
    return QStringLiteral("TrainingStatusIdle");
}

QString taskOverviewStatusLabel(const QString& statusKey) {
    if (statusKey == QStringLiteral("running")) {
        return QStringLiteral("进行中");
    }
    if (statusKey == QStringLiteral("success")) {
        return QStringLiteral("已完成");
    }
    if (statusKey == QStringLiteral("failed")) {
        return QStringLiteral("失败");
    }
    if (statusKey == QStringLiteral("stopped")) {
        return QStringLiteral("已停止");
    }
    if (statusKey == QStringLiteral("unsupported")) {
        return QStringLiteral("暂不支持训练");
    }
    return QStringLiteral("未开始");
}

QString versionMetricText(const TrainingTaskSpec& task, const QJsonObject& version) {
    bool ok = false;
    const double metric = TrainingRunStore::mainMetricValue(version.value(QStringLiteral("metrics")).toObject(), task.kind, &ok);
    return ok ? QString::number(metric, 'f', 4) : QStringLiteral("-");
}

QString shortTimeText(const QDateTime& time) {
    return time.isValid() ? time.toString(QStringLiteral("yyyy-MM-dd HH:mm")) : QStringLiteral("-");
}

QString selectedVersionTableData(QTableWidget* table, int role) {
    if (!table || table->currentRow() < 0) {
        return {};
    }
    const auto* item = table->item(table->currentRow(), 0);
    return item ? item->data(role).toString() : QString();
}

int datasetItemCount(const QString& rootPath) {
    const QDir root(rootPath);
    if (!root.exists()) {
        return 0;
    }
    int total = 0;
    for (const QString& name : {QStringLiteral("train.txt"), QStringLiteral("val.txt"), QStringLiteral("label.txt")}) {
        QFile file(root.filePath(name));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        while (!file.atEnd()) {
            if (!QString::fromUtf8(file.readLine()).trimmed().isEmpty()) {
                ++total;
            }
        }
    }
    if (total > 0) {
        return total;
    }
    const QDir images(root.filePath(QStringLiteral("images")));
    if (!images.exists()) {
        return 0;
    }
    const QStringList suffixes{
        QStringLiteral("*.jpg"),
        QStringLiteral("*.jpeg"),
        QStringLiteral("*.png"),
        QStringLiteral("*.bmp"),
        QStringLiteral("*.tif"),
        QStringLiteral("*.tiff"),
    };
    QDirIterator it(images.absolutePath(), suffixes, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        ++total;
    }
    return total;
}

QDateTime latestFileTime(const QString& rootPath) {
    QDateTime latest;
    if (!QFileInfo::exists(rootPath)) {
        return latest;
    }
    QDirIterator it(rootPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo info(it.next());
        if (!latest.isValid() || info.lastModified() > latest) {
            latest = info.lastModified();
        }
    }
    return latest;
}

int countUsableInferenceModels(const ProjectContext& context) {
    int count = 0;
    QDirIterator it(context.path(QStringLiteral("training")), QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QDir dir(it.next());
        QString reason;
        if (PaddleInferenceRuntime::modelDirLooksUsable(dir.absolutePath(), &reason)) {
            ++count;
        }
    }
    return count;
}

bool jsonObjectsEqual(const QJsonObject& lhs, const QJsonObject& rhs) {
    return QJsonDocument(lhs).toJson(QJsonDocument::Compact) == QJsonDocument(rhs).toJson(QJsonDocument::Compact);
}

QString scoreText(const QJsonValue& value, double* score = nullptr, bool* hasScore = nullptr) {
    bool ok = false;
    const double number = value.toVariant().toDouble(&ok);
    if (score) {
        *score = ok ? number : 0.0;
    }
    if (hasScore) {
        *hasScore = ok;
    }
    return ok ? QString::number(number, 'f', 4) : QStringLiteral("--");
}

QString firstRegexCapture(const QString& text, const QStringList& patterns) {
    for (const auto& patternText : patterns) {
        const QRegularExpression pattern(patternText, QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch match = pattern.match(text);
        if (match.hasMatch()) {
            return match.captured(1);
        }
    }
    return {};
}

QString metricValue(const QString& line, const QStringList& names) {
    for (const auto& name : names) {
        const QString escaped = QRegularExpression::escape(name);
        const QRegularExpression pattern(
            QStringLiteral("(?:^|[\\s,;{])['\\\"]?%1['\\\"]?\\s*[:=]\\s*([-+]?\\d+(?:\\.\\d+)?(?:[eE][-+]?\\d+)?)")
                .arg(escaped),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch match = pattern.match(line);
        if (match.hasMatch()) {
            return match.captured(1);
        }
    }
    return {};
}

bool lineLooksLikeTrainingMetric(const QString& line) {
    const QString lower = line.toLower();
    return lower.contains(QStringLiteral("loss"))
        || lower.contains(QStringLiteral("acc"))
        || lower.contains(QStringLiteral("hmean"))
        || lower.contains(QStringLiteral("precision"))
        || lower.contains(QStringLiteral("recall"))
        || lower.contains(QStringLiteral("epoch"))
        || lower.contains(QStringLiteral("iter"))
        || lower.contains(QStringLiteral("step"))
        || lower.contains(QStringLiteral(" map"))
        || lower.contains(QStringLiteral("map:"))
        || lower.contains(QStringLiteral("lr:"))
        || lower.contains(QStringLiteral("lr="));
}

TrainingMetricRow parseTrainingMetricRow(const QString& line) {
    const QString precision = metricValue(line, {QStringLiteral("precision"), QStringLiteral("prec")});
    const QString recall = metricValue(line, {QStringLiteral("recall"), QStringLiteral("rec")});
    const QString score = metricValue(line, {
        QStringLiteral("mAP@0.5"),
        QStringLiteral("mAP"),
        QStringLiteral("map"),
        QStringLiteral("hmean"),
        QStringLiteral("f1"),
        QStringLiteral("acc"),
        QStringLiteral("accuracy"),
        QStringLiteral("score"),
    });
    return {
        QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
        firstRegexCapture(line, {QStringLiteral("(?:epoch|Epoch)[^0-9]{0,12}(\\d+)")}),
        firstRegexCapture(line, {QStringLiteral("(?:iter|iters|step|Step)[^0-9]{0,12}(\\d+)")}),
        metricValue(line, {QStringLiteral("loss"), QStringLiteral("loss_total"), QStringLiteral("total_loss"), QStringLiteral("train_loss")}),
        metricValue(line, {QStringLiteral("lr"), QStringLiteral("learning_rate")}),
        metricValue(line, {QStringLiteral("acc"), QStringLiteral("accuracy"), QStringLiteral("norm_edit_dis")}),
        score,
        precision.isEmpty() && recall.isEmpty() ? QString() : QStringLiteral("%1/%2").arg(precision, recall),
        line.trimmed(),
    };
}

bool suffixMatches(const QString& path, const QStringList& suffixes) {
    const QString suffix = QStringLiteral(".") + QFileInfo(path).suffix().toLower();
    return suffixes.contains(suffix);
}

QStringList imageFileSuffixes() {
    return {
        QStringLiteral(".jpg"),
        QStringLiteral(".jpeg"),
        QStringLiteral(".png"),
        QStringLiteral(".bmp"),
        QStringLiteral(".tif"),
        QStringLiteral(".tiff"),
    };
}

QStringList filesWithSuffixes(const QString& root, const QStringList& suffixes) {
    QStringList files;
    if (!QFileInfo(root).isDir()) {
        return files;
    }
    QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (suffixMatches(path, suffixes)) {
            files.append(path);
        }
    }
    files.sort();
    return files;
}

void copyPreservingRelativePath(const QString& sourceRoot, const QString& targetRoot, const QString& sourcePath) {
    const QString relative = QDir(sourceRoot).relativeFilePath(sourcePath);
    const QString targetPath = QDir(targetRoot).filePath(relative);
    QDir().mkpath(QFileInfo(targetPath).absolutePath());
    QFile::remove(targetPath);
    if (!QFile::copy(sourcePath, targetPath)) {
        throw std::runtime_error(QStringLiteral("failed to copy %1 -> %2").arg(sourcePath, targetPath).toStdString());
    }
}

int copyDirectoryContents(const QString& sourceRoot, const QString& targetRoot) {
    if (!QFileInfo(sourceRoot).isDir()) {
        return 0;
    }
    QDir().mkpath(targetRoot);
    int copied = 0;
    QDirIterator it(sourceRoot, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        copyPreservingRelativePath(sourceRoot, targetRoot, it.next());
        ++copied;
    }
    return copied;
}

QJsonObject readPredictionJson(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }
    return document.object();
}

void collectPredictionRowsFromObject(const QJsonObject& object, const QString& source, QList<PredictionRow>* rows) {
    const QJsonObject res = object.value(QStringLiteral("res")).toObject();
    if (!res.isEmpty()) {
        collectPredictionRowsFromObject(res, source, rows);
    }

    const QJsonArray texts = object.value(QStringLiteral("rec_texts")).toArray();
    const QJsonArray scores = object.value(QStringLiteral("rec_scores")).toArray();
    for (int i = 0; i < texts.size(); ++i) {
        double score = 0.0;
        bool hasScore = false;
        rows->append(PredictionRow{
            QStringLiteral("OCR"),
            texts.at(i).toString(),
            scoreText(i < scores.size() ? scores.at(i) : QJsonValue(), &score, &hasScore),
            source,
            score,
            hasScore,
        });
    }

    const QJsonArray boxes = object.value(QStringLiteral("boxes")).toArray();
    for (const auto& value : boxes) {
        const QJsonObject box = value.toObject();
        double score = 0.0;
        bool hasScore = false;
        rows->append(PredictionRow{
            QStringLiteral("Layout"),
            box.value(QStringLiteral("label")).toString(QStringLiteral("box")),
            scoreText(box.value(QStringLiteral("score")), &score, &hasScore),
            source,
            score,
            hasScore,
        });
    }

    const QJsonArray labels = object.value(QStringLiteral("label_names")).toArray();
    const QJsonArray classIds = object.value(QStringLiteral("class_ids")).toArray();
    if (!labels.isEmpty() || !classIds.isEmpty()) {
        const QJsonArray clsScores = object.value(QStringLiteral("scores")).toArray();
        double score = 0.0;
        bool hasScore = false;
        rows->append(PredictionRow{
            QStringLiteral("Cls"),
            labels.isEmpty() ? QString::number(classIds.first().toInt()) : labels.first().toString(),
            scoreText(clsScores.isEmpty() ? QJsonValue() : clsScores.first(), &score, &hasScore),
            source,
            score,
            hasScore,
        });
    }

    if (texts.isEmpty() && boxes.isEmpty() && labels.isEmpty() && classIds.isEmpty()) {
        for (const auto& value : object.value(QStringLiteral("results")).toArray()) {
            collectPredictionRowsFromObject(value.toObject(), source, rows);
        }
    }
}

QList<PredictionRow> collectPredictionRows(const QString& root) {
    QList<PredictionRow> rows;
    for (const auto& path : filesWithSuffixes(root, {QStringLiteral(".json")})) {
        const QString name = QFileInfo(path).fileName();
        if (name.endsWith(QStringLiteral("_summary.json")) || name == QStringLiteral("layout_summary.json")) {
            continue;
        }
        collectPredictionRowsFromObject(readPredictionJson(path), name, &rows);
    }
    if (rows.isEmpty()) {
        for (const auto& path : filesWithSuffixes(root, {QStringLiteral(".json")})) {
            collectPredictionRowsFromObject(readPredictionJson(path), QFileInfo(path).fileName(), &rows);
        }
    }
    return rows;
}

QRect initialAvailableGeometry() {
    if (QScreen* screen = QGuiApplication::primaryScreen()) {
        return screen->availableGeometry();
    }
    return QRect(0, 0, 1440, 900);
}

QSize initialWindowSizeForScreen(const QRect& available) {
    const int width = qMin(1180, qMax(960, static_cast<int>(available.width() * 0.72)));
    const int height = qMin(740, qMax(620, static_cast<int>(available.height() * 0.76)));
    return QSize(qMin(width, available.width()), qMin(height, available.height()));
}

struct AsyncPrelabelClassificationTask {
    QString predictionTask;
    PaddleClsModelConfig config;
};

struct AsyncPrelabelStepSummary {
    QString name;
    int okCount = 0;
    int failedCount = 0;
    int regionCount = 0;
    QStringList details;
};

struct AsyncPrelabelJobResult {
    QString title;
    qint64 elapsedMs = 0;
    QList<AsyncPrelabelStepSummary> steps;
    QStringList logLines;
    QJsonObject preview;
};

struct AsyncExportJobResult {
    QMap<QString, QString> outputs;
    QString error;
    QList<ValidationIssue> validationIssues;
    QString validationLogPath;
    qint64 elapsedMs = 0;
};

QString prelabelSummaryText(const AsyncPrelabelJobResult& result) {
    QStringList lines;
    lines.append(QStringLiteral("耗时：%1 秒").arg(result.elapsedMs / 1000.0, 0, 'f', 1));
    for (const auto& step : result.steps) {
        QString line = QStringLiteral("%1：成功 %2，失败 %3")
            .arg(step.name)
            .arg(step.okCount)
            .arg(step.failedCount);
        if (step.regionCount > 0) {
            line += QStringLiteral("，区域 %1").arg(step.regionCount);
        }
        lines.append(line);
    }
    QStringList detailLines;
    for (const auto& step : result.steps) {
        if (!step.details.isEmpty()) {
            detailLines.append(QStringLiteral("[%1]").arg(step.name));
            detailLines.append(step.details.mid(0, 8));
        }
    }
    if (!detailLines.isEmpty()) {
        lines.append(QString());
        lines.append(QStringLiteral("失败详情："));
        lines.append(detailLines);
    }
    return lines.join(QLatin1Char('\n'));
}

int removeAnnotationCrops(const ProjectContext& context, const QJsonObject& annotation) {
    int removed = 0;
    for (const auto& value : annotation.value(QStringLiteral("rec_crops")).toArray()) {
        const QString relative = value.toObject().value(QStringLiteral("crop_path")).toString();
        if (relative.isEmpty()) {
            continue;
        }
        const QString path = context.path(relative);
        if (QFileInfo::exists(path) && QFile::remove(path)) {
            ++removed;
        }
    }
    return removed;
}

}  // namespace

MainWindow::MainWindow(QString baseDir, QWidget* parent)
    : QMainWindow(parent), baseDir_(std::move(baseDir)) {
    setWindowTitle(QStringLiteral("PP-OCR 标注工具"));
    const QRect available = initialAvailableGeometry();
    buildUi();
    resize(initialWindowSizeForScreen(available));
    move(available.center() - rect().center());
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == trainingTaskHost_
            && (event->type() == QEvent::Resize
                || event->type() == QEvent::Show
                || event->type() == QEvent::LayoutRequest)) {
        const int columns = trainingTaskOverviewColumnsForWidth(trainingTaskHost_->width());
        if (event->type() == QEvent::Show || columns != trainingTaskOverviewColumns_) {
            queueTrainingTaskOverviewRefresh();
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::openProjectPath(const QString& projectDir) {
    try {
        setProject(ProjectRepository::openProject(projectDir));
    } catch (const std::exception& exc) {
        QMessageBox::critical(this, QStringLiteral("打开项目失败"), QString::fromUtf8(exc.what()));
    }
}

void MainWindow::openLastProject() {
    const QStringList projects = loadRecentProjects();
    if (!projects.isEmpty()) {
        openProjectPath(projects.first());
    }
}

void MainWindow::buildUi() {
    auto* toolbar = new QToolBar(QStringLiteral("Main"), this);
    toolbar->setMovable(false);
    auto* title = new QLabel(QStringLiteral("PP-OCR 工作台"), toolbar);
    title->setObjectName("Title");
    title->setMinimumWidth(130);
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    toolbar->addWidget(title);
    addToolBar(toolbar);

    auto* host = new QWidget(this);
    auto* root = new QHBoxLayout(host);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    stack_ = new WorkbenchStackedWidget(host);
    stack_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    stack_->setMinimumSize(0, 0);
    stack_->addWidget(buildOverviewPage());
    stack_->addWidget(buildLabelPage());
    stack_->addWidget(buildTrainingPage());
    stack_->addWidget(buildPredictionPage());
    stack_->addWidget(buildDatasetPage());
    stack_->addWidget(buildModelLibraryPage());
    stack_->addWidget(buildLogsPage());
    stack_->addWidget(buildSettingsPage());

    root->addWidget(buildRail());
    root->addWidget(stack_, 1);
    setCentralWidget(host);
    statusBar()->showMessage(QStringLiteral("当前工作区：概况"));
    connect(stack_, &QStackedWidget::currentChanged, this, [this](int index) {
        const QStringList names{
            QStringLiteral("概况"),
            QStringLiteral("标注"),
            QStringLiteral("训练"),
            QStringLiteral("预测"),
            QStringLiteral("数据集"),
            QStringLiteral("模型库"),
            QStringLiteral("日志"),
            QStringLiteral("配置"),
        };
        statusBar()->showMessage(QStringLiteral("当前工作区：%1").arg(index >= 0 && index < names.size() ? names.at(index) : QString()));
        if (index == 4) {
            refreshDatasetPage();
        } else if (index == 5) {
            refreshModelLibraryPage();
        } else if (index == 6) {
            refreshLogsPage();
        } else if (index == 7) {
            refreshSettingsPage();
        } else if (index == 2) {
            queueTrainingTaskOverviewRefresh();
        }
    });
    connect(stack_, &QStackedWidget::currentChanged, this, [this](int index) {
        if (!overviewResourceTimer_) {
            return;
        }
        if (index == 0) {
            refreshOverviewResources();
            overviewResourceTimer_->start();
        } else {
            overviewResourceTimer_->stop();
        }
    });
    if (overviewResourceTimer_) {
        overviewResourceTimer_->start();
    }
}

QWidget* MainWindow::buildRail() {
    auto* rail = new QWidget(this);
    rail->setObjectName("WorkspaceRail");
    rail->setMinimumWidth(190);
    rail->setMaximumWidth(210);
    rail->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto* layout = new QVBoxLayout(rail);
    layout->setContentsMargins(14, 18, 14, 14);
    layout->setSpacing(10);
    auto* railTitle = new QLabel(QStringLiteral("PP-OCR 工作台"), rail);
    railTitle->setObjectName("RailTitle");
    layout->addWidget(railTitle);
    layout->addSpacing(14);
    layout->addWidget(railButton(QStringLiteral("概况"), 0));
    layout->addWidget(railButton(QStringLiteral("数据"), 4));
    layout->addWidget(railButton(QStringLiteral("标注"), 1));
    layout->addWidget(railButton(QStringLiteral("训练"), 2));
    layout->addWidget(railButton(QStringLiteral("模型"), 5));
    layout->addWidget(railButton(QStringLiteral("预测"), 3));
    layout->addWidget(railButton(QStringLiteral("日志"), 6));
    layout->addWidget(railButton(QStringLiteral("配置"), 7));
    layout->addStretch(1);
    return rail;
}

QPushButton* MainWindow::railButton(const QString& text, int pageIndex) {
    const QMap<int, QString> icons{
        {0, QStringLiteral("⌂")},
        {4, QStringLiteral("◎")},
        {1, QStringLiteral("✎")},
        {2, QStringLiteral("▥")},
        {5, QStringLiteral("▧")},
        {3, QStringLiteral("⌖")},
        {6, QStringLiteral("□")},
        {7, QStringLiteral("⚙")},
    };
    auto* button = new QPushButton(QStringLiteral("%1   %2").arg(icons.value(pageIndex, QStringLiteral("•")), text), this);
    button->setObjectName("WorkspaceNav");
    button->setCheckable(true);
    button->setChecked(stack_ && stack_->currentIndex() == pageIndex);
    button->setMinimumHeight(58);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(button, &QPushButton::clicked, this, [this, pageIndex]() { stack_->setCurrentIndex(pageIndex); });
    connect(stack_, &QStackedWidget::currentChanged, button, [button, pageIndex](int current) {
        button->setChecked(current == pageIndex);
    });
    return button;
}

QWidget* MainWindow::buildOverviewPage() {
    auto* page = new QWidget(this);
    page->setObjectName("OverviewRoot");
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(10, 8, 10, 10);
    outer->setSpacing(0);
    outer->setSizeConstraint(QLayout::SetNoConstraint);

    auto* scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget(scroll);
    content->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->setSizeConstraint(QLayout::SetNoConstraint);

    auto* header = new QWidget(content);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);
    headerLayout->setSizeConstraint(QLayout::SetNoConstraint);
    auto* titleBox = new QVBoxLayout();
    titleBox->setSpacing(4);
    auto* title = new QLabel(QStringLiteral("概况"), header);
    title->setObjectName("OverviewTitle");
    auto* overviewSubtitle = new QLabel(QStringLiteral("欢迎使用 PP-OCR 工作台，快速了解训练任务与模型状态"), header);
    overviewSubtitle->setObjectName("OverviewMuted");
    titleBox->addWidget(title);
    titleBox->addWidget(overviewSubtitle);
    headerLayout->addLayout(titleBox, 1);

    recentProjectsCombo_ = new QComboBox(header);
    recentProjectsCombo_->setMinimumWidth(240);
    recentProjectsCombo_->setMaximumWidth(420);
    recentProjectsCombo_->setMinimumContentsLength(16);
    recentProjectsCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    recentProjectsCombo_->setObjectName("OverviewProject");
    projectLabel_ = mutedLabel(QStringLiteral("● 状态：未打开"), header);
    projectLabel_->setObjectName("OverviewProjectStatus");
    auto* newButton = primaryButton(QStringLiteral("新建"), header);
    auto* openButton = new QPushButton(QStringLiteral("打开"), header);
    connect(newButton, &QPushButton::clicked, this, &MainWindow::newProjectDialog);
    connect(openButton, &QPushButton::clicked, this, &MainWindow::openProjectDialog);
    connect(recentProjectsCombo_, QOverload<int>::of(&QComboBox::activated), this, [this](int) {
        openRecentProjectFromCombo();
    });
    headerLayout->addWidget(recentProjectsCombo_);
    headerLayout->addWidget(projectLabel_);
    headerLayout->addWidget(newButton);
    headerLayout->addWidget(openButton);

    auto* metrics = new QGridLayout();
    metrics->setHorizontalSpacing(14);
    metrics->setVerticalSpacing(10);
    metrics->setSizeConstraint(QLayout::SetNoConstraint);
    auto addMetric = [content, metrics](int column, const QString& title, const QString& delta, const QString& icon, const QColor& color, QLabel** valueLabel) {
        auto* box = workbenchCard(content, QStringLiteral("OverviewCard"));
        box->setMinimumWidth(0);
        box->setMinimumHeight(92);
        auto* boxLayout = new QHBoxLayout(box);
        boxLayout->setContentsMargins(12, 12, 12, 12);
        boxLayout->setSpacing(10);
        auto* iconLabel = new QLabel(icon, box);
        iconLabel->setObjectName("OverviewMetricIcon");
        iconLabel->setFixedSize(44, 68);
        iconLabel->setStyleSheet(QStringLiteral("background:%1;").arg(color.name()));
        auto* textBox = new QVBoxLayout();
        textBox->setSpacing(3);
        auto* titleLabel = new QLabel(title, box);
        titleLabel->setObjectName("OverviewMuted");
        titleLabel->setWordWrap(true);
        titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        auto* value = new QLabel(QStringLiteral("0"), box);
        value->setObjectName("OverviewMetricValue");
        auto* deltaLabel = new QLabel(delta, box);
        deltaLabel->setObjectName("OverviewMuted");
        deltaLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        textBox->addWidget(titleLabel);
        textBox->addWidget(value);
        textBox->addWidget(deltaLabel);
        boxLayout->addWidget(iconLabel);
        boxLayout->addLayout(textBox, 1);
        metrics->addWidget(box, 0, column);
        *valueLabel = value;
    };
    addMetric(0, QStringLiteral("训练任务总数"), QStringLiteral("较昨日 ↑ 0"), QStringLiteral("↗"), QColor("#2563eb"), &overviewPagesMetric_);
    addMetric(1, QStringLiteral("进行中任务"), QStringLiteral("较昨日 ↑ 0"), QStringLiteral("▶"), QColor("#14786d"), &overviewLabeledMetric_);
    addMetric(2, QStringLiteral("已完成任务"), QStringLiteral("较昨日 ↑ 0"), QStringLiteral("✓"), QColor("#46d39a"), &overviewSplitMetric_);
    addMetric(3, QStringLiteral("失败任务"), QStringLiteral("较昨日 ↓ 0"), QStringLiteral("!"), QColor("#b0444d"), &overviewRegionsMetric_);
    addMetric(4, QStringLiteral("可用模型"), QStringLiteral("较昨日 ↑ 0"), QStringLiteral("□"), QColor("#7c3aed"), &overviewExportsMetric_);
    addMetric(5, QStringLiteral("数据集"), QStringLiteral("较昨日 ↑ 0"), QStringLiteral("≡"), QColor("#d79b35"), &overviewVersionsMetric_);
    for (int column = 0; column < 6; ++column) {
        metrics->setColumnStretch(column, 1);
    }

    auto* middle = new QGridLayout();
    middle->setSpacing(12);
    middle->setSizeConstraint(QLayout::SetNoConstraint);
    auto* trendCard = workbenchCard(content, QStringLiteral("OverviewCard"));
    trendCard->setMinimumHeight(320);
    auto* trendLayout = new QVBoxLayout(trendCard);
    trendLayout->setContentsMargins(12, 10, 12, 10);
    auto* trendHeader = new QHBoxLayout();
    trendHeader->addWidget(sectionLabel(QStringLiteral("训练任务趋势"), trendCard));
    trendHeader->addStretch(1);
    overviewTrendRangeCombo_ = new QComboBox(trendCard);
    overviewTrendRangeCombo_->addItems({QStringLiteral("近 7 天"), QStringLiteral("近 14 天"), QStringLiteral("近 30 天")});
    connect(overviewTrendRangeCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::refreshOverviewStats);
    trendHeader->addWidget(overviewTrendRangeCombo_);
    overviewTrendChart_ = new DashboardLineChart(trendCard);
    trendLayout->addLayout(trendHeader);
    trendLayout->addWidget(overviewTrendChart_, 1);

    auto* donutCard = workbenchCard(content, QStringLiteral("OverviewCard"));
    donutCard->setMinimumHeight(320);
    auto* donutLayout = new QVBoxLayout(donutCard);
    donutLayout->setContentsMargins(12, 10, 12, 10);
    donutLayout->addWidget(sectionLabel(QStringLiteral("任务状态分布"), donutCard));
    overviewDonutChart_ = new DashboardDonutChart(donutCard);
    donutLayout->addWidget(overviewDonutChart_, 1);

    auto* recentCard = workbenchCard(content, QStringLiteral("OverviewCard"));
    recentCard->setMinimumHeight(320);
    auto* recentLayout = new QVBoxLayout(recentCard);
    recentLayout->setContentsMargins(12, 10, 12, 10);
    auto* recentHeader = new QHBoxLayout();
    recentHeader->addWidget(sectionLabel(QStringLiteral("最近任务"), recentCard));
    recentHeader->addStretch(1);
    auto* allTrainingButton = new QPushButton(QStringLiteral("查看全部"), recentCard);
    connect(allTrainingButton, &QPushButton::clicked, this, [this]() { stack_->setCurrentIndex(2); });
    recentHeader->addWidget(allTrainingButton);
    overviewRecentTasksTable_ = tableWidget(3, {QStringLiteral("任务"), QStringLiteral("状态"), QStringLiteral("时间")}, recentCard);
    overviewRecentTasksTable_->setObjectName("OverviewTable");
    overviewRecentTasksTable_->setMinimumWidth(0);
    overviewRecentTasksTable_->setMinimumHeight(260);
    overviewRecentTasksTable_->verticalHeader()->setDefaultSectionSize(34);
    overviewRecentTasksTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    overviewRecentTasksTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    overviewRecentTasksTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    recentLayout->addLayout(recentHeader);
    recentLayout->addWidget(overviewRecentTasksTable_, 1);

    middle->addWidget(trendCard, 0, 0, 1, 2);
    middle->addWidget(donutCard, 0, 2);
    middle->addWidget(recentCard, 0, 3);
    middle->setColumnStretch(0, 2);
    middle->setColumnStretch(1, 2);
    middle->setColumnStretch(2, 2);
    middle->setColumnStretch(3, 2);

    auto* dataGrid = new QGridLayout();
    dataGrid->setSpacing(12);
    dataGrid->setSizeConstraint(QLayout::SetNoConstraint);
    auto* usageCard = workbenchCard(content, QStringLiteral("OverviewCard"));
    usageCard->setMinimumHeight(230);
    auto* usageLayout = new QVBoxLayout(usageCard);
    usageLayout->setContentsMargins(12, 10, 12, 10);
    usageLayout->addWidget(sectionLabel(QStringLiteral("训练使用情况"), usageCard));
    auto* usageCards = new QHBoxLayout();
    usageCards->setSpacing(8);
    usageCards->setSizeConstraint(QLayout::SetNoConstraint);
    overviewGpuCard_ = new DashboardSparklineCard(QStringLiteral("GPU 使用率"), QColor("#2563eb"), usageCard);
    overviewCpuCard_ = new DashboardSparklineCard(QStringLiteral("CPU 使用率"), QColor("#14786d"), usageCard);
    overviewMemoryCard_ = new DashboardSparklineCard(QStringLiteral("内存使用率"), QColor("#46d39a"), usageCard);
    overviewDiskCard_ = new DashboardSparklineCard(QStringLiteral("存储使用率"), QColor("#d79b35"), usageCard);
    for (auto* card : {overviewGpuCard_, overviewCpuCard_, overviewMemoryCard_, overviewDiskCard_}) {
        usageCards->addWidget(card, 1);
    }
    usageLayout->addLayout(usageCards, 1);

    auto* datasetCard = workbenchCard(content, QStringLiteral("OverviewCard"));
    datasetCard->setMinimumHeight(230);
    auto* datasetLayout = new QVBoxLayout(datasetCard);
    datasetLayout->setContentsMargins(12, 10, 12, 10);
    auto* datasetHeader = new QHBoxLayout();
    datasetHeader->addWidget(sectionLabel(QStringLiteral("数据集概览"), datasetCard));
    datasetHeader->addStretch(1);
    auto* allDatasetButton = new QPushButton(QStringLiteral("查看全部"), datasetCard);
    connect(allDatasetButton, &QPushButton::clicked, this, [this]() { stack_->setCurrentIndex(1); });
    datasetHeader->addWidget(allDatasetButton);
    overviewDatasetTable_ = tableWidget(4, {QStringLiteral("数据集名称"), QStringLiteral("类型"), QStringLiteral("数量"), QStringLiteral("修改时间")}, datasetCard);
    overviewDatasetTable_->setObjectName("OverviewTable");
    overviewDatasetTable_->setMinimumWidth(0);
    overviewDatasetTable_->setMinimumHeight(180);
    overviewDatasetTable_->verticalHeader()->setDefaultSectionSize(34);
    overviewDatasetTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    overviewDatasetTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    overviewDatasetTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    overviewDatasetTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    datasetLayout->addLayout(datasetHeader);
    datasetLayout->addWidget(overviewDatasetTable_, 1);

    dataGrid->addWidget(usageCard, 0, 0, 1, 2);
    dataGrid->addWidget(datasetCard, 0, 2);
    dataGrid->setColumnStretch(0, 2);
    dataGrid->setColumnStretch(1, 2);
    dataGrid->setColumnStretch(2, 3);

    overviewResourceTimer_ = new QTimer(page);
    overviewResourceTimer_->setInterval(5000);
    connect(overviewResourceTimer_, &QTimer::timeout, this, &MainWindow::refreshOverviewResources);

    auto* quickCard = workbenchCard(content, QStringLiteral("OverviewCard"));
    quickCard->setMinimumHeight(98);
    auto* quickLayout = new QVBoxLayout(quickCard);
    quickLayout->setContentsMargins(12, 10, 12, 12);
    quickLayout->addWidget(sectionLabel(QStringLiteral("快捷入口"), quickCard));
    auto* quickRow = new QHBoxLayout();
    quickRow->setSpacing(10);
    quickRow->setSizeConstraint(QLayout::SetNoConstraint);
    auto addQuick = [this, quickCard, quickRow](const QString& title, const QString& subtitle, const QColor& color, int pageIndex) {
        auto* item = workbenchCard(quickCard, QStringLiteral("OverviewQuick"));
        item->setMinimumWidth(0);
        item->setMinimumHeight(66);
        auto* itemLayout = new QHBoxLayout(item);
        itemLayout->setContentsMargins(12, 12, 12, 12);
        auto* icon = new QLabel(QStringLiteral("→"), item);
        icon->setObjectName("OverviewMetricIcon");
        icon->setFixedSize(44, 44);
        icon->setStyleSheet(QStringLiteral("background:%1;").arg(color.name()));
        auto* textBox = new QVBoxLayout();
        textBox->setSpacing(2);
        auto* titleLabel = new QLabel(title, item);
        titleLabel->setStyleSheet(QStringLiteral("font-weight:700;"));
        titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        auto* subtitleLabel = mutedLabel(subtitle, item);
        subtitleLabel->setWordWrap(true);
        subtitleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        textBox->addWidget(titleLabel);
        textBox->addWidget(subtitleLabel);
        auto* button = new QPushButton(QStringLiteral("→"), item);
        button->setObjectName("OverviewArrow");
        button->setMaximumWidth(44);
        connect(button, &QPushButton::clicked, this, [this, pageIndex]() { stack_->setCurrentIndex(pageIndex); });
        itemLayout->addWidget(icon);
        itemLayout->addLayout(textBox, 1);
        itemLayout->addWidget(button);
        quickRow->addWidget(item, 1);
    };
    addQuick(QStringLiteral("训练控制台"), QStringLiteral("查看模型训练状态与输出"), QColor("#2563eb"), 2);
    addQuick(QStringLiteral("模型配置"), QStringLiteral("配置训练参数与超参数设置"), QColor("#14786d"), 2);
    addQuick(QStringLiteral("数据集管理"), QStringLiteral("管理和查看所有数据集"), QColor("#4ebf8f"), 1);
    addQuick(QStringLiteral("训练日志"), QStringLiteral("查看任务训练过程日志与指标"), QColor("#4b86f5"), 2);
    addQuick(QStringLiteral("可视化分析"), QStringLiteral("查看训练指标对比与分析"), QColor("#37a08e"), 2);
    quickLayout->addLayout(quickRow);

    layout->addWidget(header);
    layout->addLayout(metrics);
    layout->addLayout(middle, 6);
    layout->addLayout(dataGrid, 4);
    layout->addWidget(quickCard);
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);
    refreshRecentProjects();
    refreshOverviewStats();
    return page;
}

QWidget* MainWindow::buildLabelPage() {
    auto* page = new QWidget(this);
    page->setObjectName("LabelRoot");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(10);

    auto* header = workbenchCard(page, QStringLiteral("HeaderPanel"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 10, 14, 10);
    headerLayout->setSpacing(10);

    auto* titleBox = new QVBoxLayout();
    titleBox->setSpacing(3);
    auto* title = new QLabel(QStringLiteral("标注工作台"), header);
    title->setObjectName("PageTitle");
    labelWorkspaceSummary_ = new QLabel(QStringLiteral("未打开项目"), header);
    labelWorkspaceSummary_->setObjectName("Muted");
    titleBox->addWidget(title);
    titleBox->addWidget(labelWorkspaceSummary_);

    auto* headerActions = new QHBoxLayout();
    headerActions->setSpacing(8);
    auto* headerImportButton = new QPushButton(QStringLiteral("导入图片"), header);
    auto* headerImportPdfButton = new QPushButton(QStringLiteral("导入PDF"), header);
    auto* createTrainingButton = primaryButton(QStringLiteral("+ 创建训练任务"), header);
    auto* headerPrelabelButton = primaryButton(QStringLiteral("预标注"), header);
    auto* headerValidateButton = new QPushButton(QStringLiteral("校验"), header);
    auto* headerExportButton = new QPushButton(QStringLiteral("导出"), header);
    connect(headerImportButton, &QPushButton::clicked, this, &MainWindow::importImagesDialog);
    connect(headerImportPdfButton, &QPushButton::clicked, this, &MainWindow::importPdfsDialog);
    connect(createTrainingButton, &QPushButton::clicked, this, [this]() { stack_->setCurrentIndex(2); });
    connect(headerPrelabelButton, &QPushButton::clicked, this, &MainWindow::prelabelSelectedPage);
    connect(headerValidateButton, &QPushButton::clicked, this, &MainWindow::validateProject);
    connect(headerExportButton, &QPushButton::clicked, this, &MainWindow::exportProject);
    headerActions->addWidget(headerImportButton);
    headerActions->addWidget(headerImportPdfButton);
    headerActions->addSpacing(8);
    headerActions->addWidget(createTrainingButton);
    headerActions->addWidget(headerPrelabelButton);
    headerActions->addWidget(headerValidateButton);
    headerActions->addWidget(headerExportButton);
    headerLayout->addLayout(titleBox, 1);
    headerLayout->addLayout(headerActions);

    auto* splitter = new QSplitter(Qt::Horizontal, page);
    splitter->setHandleWidth(6);

    auto* left = workbenchCard(splitter, QStringLiteral("Panel"));
    left->setMinimumWidth(200);
    left->setMaximumWidth(310);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(12, 12, 12, 12);
    leftLayout->setSpacing(8);

    pageSearchEdit_ = new QLineEdit(left);
    pageSearchEdit_->hide();
    pageSplitFilterCombo_ = new QComboBox(left);
    pageSplitFilterCombo_->addItem(QStringLiteral("全部集合"), QString());
    pageSplitFilterCombo_->addItem(QStringLiteral("训练集"), QStringLiteral("train"));
    pageSplitFilterCombo_->addItem(QStringLiteral("验证集"), QStringLiteral("val"));
    pageSplitFilterCombo_->hide();
    pageStatusFilterCombo_ = new QComboBox(left);
    pageStatusFilterCombo_->addItem(QStringLiteral("全部状态"), QString());
    pageStatusFilterCombo_->addItem(QStringLiteral("未标注"), QStringLiteral("unlabeled"));
    pageStatusFilterCombo_->addItem(QStringLiteral("已标注"), QStringLiteral("labeled"));
    pageStatusFilterCombo_->addItem(QStringLiteral("已校验"), QStringLiteral("validated"));
    pageStatusFilterCombo_->hide();
    pageSplitCombo_ = new QComboBox(left);
    pageSplitCombo_->addItem(QStringLiteral("训练集"), QStringLiteral("train"));
    pageSplitCombo_->addItem(QStringLiteral("验证集"), QStringLiteral("val"));
    pageSplitCombo_->hide();
    pageStatusCombo_ = new QComboBox(left);
    pageStatusCombo_->addItem(QStringLiteral("未标注"), QStringLiteral("unlabeled"));
    pageStatusCombo_->addItem(QStringLiteral("已标注"), QStringLiteral("labeled"));
    pageStatusCombo_->addItem(QStringLiteral("已校验"), QStringLiteral("validated"));
    pageStatusCombo_->hide();
    connect(pageSearchEdit_, &QLineEdit::textChanged, this, &MainWindow::filterPages);
    connect(pageSplitFilterCombo_, &QComboBox::currentTextChanged, this, &MainWindow::filterPages);
    connect(pageStatusFilterCombo_, &QComboBox::currentTextChanged, this, &MainWindow::filterPages);
    connect(pageSplitCombo_, &QComboBox::currentTextChanged, this, &MainWindow::savePageMetadata);
    connect(pageStatusCombo_, &QComboBox::currentTextChanged, this, &MainWindow::savePageMetadata);

    auto* pageHeader = new QHBoxLayout();
    pageHeader->setSpacing(6);
    pageHeader->addWidget(sectionLabel(QStringLiteral("项目文件"), left));
    pageHeader->addStretch(1);
    pageCountLabel_ = new QLabel(QStringLiteral("0 页"), left);
    pageCountLabel_->setObjectName("Muted");
    pageHeader->addWidget(pageCountLabel_);
    pageMetaLabel_ = new QLabel(QStringLiteral("未打开项目"), left);
    pageMetaLabel_->setObjectName("Muted");
    pageMetaLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pageList_ = new QListWidget(left);
    pageList_->setObjectName("ProjectPageList");
    pageList_->setIconSize(QSize(0, 0));
    pageList_->setUniformItemSizes(false);
    pageList_->setSpacing(0);
    leftLayout->addLayout(pageHeader);
    leftLayout->addWidget(pageMetaLabel_);
    leftLayout->addWidget(pageList_, 1);

    auto* center = new QWidget(splitter);
    center->setObjectName("CanvasPanel");
    auto* centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(8);

    auto* canvasHeader = workbenchCard(center, QStringLiteral("CanvasHeader"));
    auto* canvasHeaderLayout = new QHBoxLayout(canvasHeader);
    canvasHeaderLayout->setContentsMargins(12, 10, 12, 10);
    canvasHeaderLayout->setSpacing(8);
    auto* fileTitleBox = new QVBoxLayout();
    fileTitleBox->setSpacing(2);
    fileTitleLabel_ = sectionLabel(QStringLiteral("未选择图片"), canvasHeader);
    fileMetaLabel_ = new QLabel(QStringLiteral("选择左侧图片开始标注"), canvasHeader);
    fileMetaLabel_->setObjectName("Muted");
    fileTitleBox->addWidget(fileTitleLabel_);
    fileTitleBox->addWidget(fileMetaLabel_);
    canvasHeaderLayout->addLayout(fileTitleBox, 1);

    selectToolButton_ = new QPushButton(QStringLiteral("选择"), canvasHeader);
    drawOcrToolButton_ = new QPushButton(QStringLiteral("OCR框"), canvasHeader);
    drawLayoutToolButton_ = new QPushButton(QStringLiteral("版面框"), canvasHeader);
    panToolButton_ = new QPushButton(QStringLiteral("平移"), canvasHeader);
    panToolButton_->hide();
    for (auto* button : {selectToolButton_, drawOcrToolButton_, drawLayoutToolButton_}) {
        button->setObjectName("ModeButton");
        button->setCheckable(true);
        canvasHeaderLayout->addWidget(button);
    }
    selectToolButton_->setChecked(true);
    connect(selectToolButton_, &QPushButton::clicked, this, [this]() { setCanvasToolMode(QStringLiteral("select")); });
    connect(drawOcrToolButton_, &QPushButton::clicked, this, [this]() { setCanvasToolMode(QStringLiteral("drawOcr")); });
    connect(drawLayoutToolButton_, &QPushButton::clicked, this, [this]() { setCanvasToolMode(QStringLiteral("drawLayout")); });
    connect(panToolButton_, &QPushButton::clicked, this, [this]() { setCanvasToolMode(QStringLiteral("pan")); });

    auto* fitButton = new QPushButton(QStringLiteral("适应"), canvasHeader);
    auto* actualButton = new QPushButton(QStringLiteral("100%"), canvasHeader);
    connect(fitButton, &QPushButton::clicked, this, [this]() {
        if (auto* root = canvasWidget_ ? canvasWidget_->rootObject() : nullptr) {
            QMetaObject::invokeMethod(root, "resetView");
        }
    });
    connect(actualButton, &QPushButton::clicked, this, [this]() {
        if (auto* root = canvasWidget_ ? canvasWidget_->rootObject() : nullptr) {
            root->setProperty("zoomScale", 1.0);
            root->setProperty("panX", 0.0);
            root->setProperty("panY", 0.0);
        }
    });
    canvasHeaderLayout->addWidget(fitButton);
    canvasHeaderLayout->addWidget(actualButton);

    canvasWidget_ = new QQuickWidget(center);
    canvasWidget_->setObjectName("AnnotationCanvas");
    canvasWidget_->setResizeMode(QQuickWidget::SizeRootObjectToView);
    canvasWidget_->setSource(QUrl(QStringLiteral("qrc:/qml/AnnotationCanvas.qml")));
    connectCanvasSignals();

    auto* canvasFooter = new QHBoxLayout();
    canvasModeLabel_ = new QLabel(QStringLiteral("当前模式：选择"), center);
    canvasModeLabel_->setObjectName("Muted");
    auto* canvasHint = new QLabel(QStringLiteral("滚轮缩放 | 拖动画布 | OCR/版面框模式下拖拽画框"), center);
    canvasHint->setObjectName("Muted");
    canvasFooter->addWidget(canvasModeLabel_);
    canvasFooter->addStretch(1);
    canvasFooter->addWidget(canvasHint);
    centerLayout->addWidget(canvasHeader);
    centerLayout->addWidget(canvasWidget_, 1);
    centerLayout->addLayout(canvasFooter);

    auto* inspectorScroll = new QScrollArea(splitter);
    inspectorScroll->setWidgetResizable(true);
    inspectorScroll->setFrameShape(QFrame::NoFrame);
    inspectorScroll->setMinimumWidth(300);
    inspectorScroll->setMaximumWidth(560);
    auto* inspector = new QWidget(inspectorScroll);
    inspector->setObjectName("InspectorPanel");
    auto* inspectorLayout = new QVBoxLayout(inspector);
    inspectorLayout->setContentsMargins(10, 10, 10, 10);
    inspectorLayout->setSpacing(8);
    inspectorLayout->addWidget(sectionLabel(QStringLiteral("属性检查器"), inspector));

    auto* tabs = new QTabWidget(inspector);
    tabs->setObjectName("InspectorTabs");

    undoAnnotationButton_ = new QPushButton(QStringLiteral("撤销"), inspector);
    redoAnnotationButton_ = new QPushButton(QStringLiteral("重做"), inspector);
    connect(undoAnnotationButton_, &QPushButton::clicked, this, &MainWindow::undoAnnotationEdit);
    connect(redoAnnotationButton_, &QPushButton::clicked, this, &MainWindow::redoAnnotationEdit);
    auto* undoShortcut = new QShortcut(QKeySequence::Undo, page);
    auto* redoShortcut = new QShortcut(QKeySequence::Redo, page);
    connect(undoShortcut, &QShortcut::activated, this, &MainWindow::undoAnnotationEdit);
    connect(redoShortcut, &QShortcut::activated, this, &MainWindow::redoAnnotationEdit);

    selectedRegionLabel_ = new QLabel(QStringLiteral("ID: -"), inspector);
    selectedRegionLabel_->setObjectName("Muted");
    regionTextEdit_ = new QPlainTextEdit(inspector);
    regionTextEdit_->setPlaceholderText(QStringLiteral("OCR 文本内容"));
    regionTextEdit_->setMinimumHeight(160);
    layoutLabelCombo_ = new QComboBox(inspector);
    regionReadingOrderSpin_ = new QSpinBox(inspector);
    regionReadingOrderSpin_->setRange(0, 999999);
    regionCheckedCheck_ = new QCheckBox(QStringLiteral("checked（已校验）"), inspector);
    regionIgnoreCheck_ = new QCheckBox(QStringLiteral("ignore（忽略）"), inspector);
    saveRegionButton_ = primaryButton(QStringLiteral("更新当前框"), inspector);
    deleteRegionButton_ = dangerButton(QStringLiteral("删除当前框"), inspector);
    auto* generateCropsButton = new QPushButton(QStringLiteral("生成 Rec Crop"), inspector);
    auto* clearPageButton = dangerButton(QStringLiteral("清空当前页标注"), inspector);
    auto* clearAllButton = dangerButton(QStringLiteral("一键删除所有标注"), inspector);
    connect(saveRegionButton_, &QPushButton::clicked, this, &MainWindow::saveSelectedRegionText);
    connect(deleteRegionButton_, &QPushButton::clicked, this, &MainWindow::deleteSelectedRegion);
    connect(generateCropsButton, &QPushButton::clicked, this, &MainWindow::generateCurrentRecCrops);
    connect(clearPageButton, &QPushButton::clicked, this, &MainWindow::clearCurrentPageAnnotations);
    connect(clearAllButton, &QPushButton::clicked, this, &MainWindow::clearAllAnnotations);

    auto* regionTab = new QWidget(tabs);
    auto* regionLayout = new QVBoxLayout(regionTab);
    regionLayout->setContentsMargins(10, 10, 10, 10);
    regionLayout->setSpacing(8);
    regionLayout->addWidget(sectionLabel(QStringLiteral("文本框 / 版面框属性"), regionTab));
    regionLayout->addWidget(selectedRegionLabel_);
    auto* textRow = new QHBoxLayout();
    auto* textLabel = new QLabel(QStringLiteral("文本内容"), regionTab);
    textRow->addWidget(textLabel);
    textRow->addWidget(regionTextEdit_, 1);
    regionLayout->addLayout(textRow);
    regionLayout->addWidget(regionIgnoreCheck_);
    regionLayout->addWidget(regionCheckedCheck_);
    auto* orderRow = new QHBoxLayout();
    orderRow->addWidget(new QLabel(QStringLiteral("阅读顺序"), regionTab));
    orderRow->addWidget(regionReadingOrderSpin_, 1);
    regionLayout->addLayout(orderRow);
    auto* layoutLabelRow = new QHBoxLayout();
    layoutLabelRow->addWidget(new QLabel(QStringLiteral("版面类别"), regionTab));
    layoutLabelRow->addWidget(layoutLabelCombo_, 1);
    regionLayout->addLayout(layoutLabelRow);
    auto* historyLayout = new QHBoxLayout();
    historyLayout->addWidget(undoAnnotationButton_);
    historyLayout->addWidget(redoAnnotationButton_);
    regionLayout->addWidget(saveRegionButton_);
    regionLayout->addWidget(generateCropsButton);
    regionLayout->addWidget(deleteRegionButton_);
    regionLayout->addWidget(clearPageButton);
    regionLayout->addWidget(clearAllButton);
    regionLayout->addLayout(historyLayout);
    regionLayout->addStretch(1);

    docOrientationCombo_ = new QComboBox(inspector);
    textlineOrientationCombo_ = new QComboBox(inspector);
    tableClassificationCombo_ = new QComboBox(inspector);
    connect(docOrientationCombo_, &QComboBox::currentTextChanged, this, &MainWindow::saveImageLabels);
    connect(textlineOrientationCombo_, &QComboBox::currentTextChanged, this, &MainWindow::saveImageLabels);
    connect(tableClassificationCombo_, &QComboBox::currentTextChanged, this, &MainWindow::saveImageLabels);

    auto* classTab = new QWidget(tabs);
    auto* classLayout = new QVBoxLayout(classTab);
    classLayout->setContentsMargins(10, 10, 10, 10);
    classLayout->setSpacing(8);
    classLayout->addWidget(sectionLabel(QStringLiteral("分类标注"), classTab));
    auto addClassRow = [classLayout, classTab](const QString& labelText, QComboBox* combo, const QString& hintText) {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel(labelText, classTab));
        row->addWidget(combo, 1);
        classLayout->addLayout(row);
        if (!hintText.isEmpty()) {
            auto* hint = mutedLabel(hintText, classTab);
            hint->setWordWrap(true);
            hint->setContentsMargins(86, 0, 0, 0);
            classLayout->addWidget(hint);
        }
    };
    addClassRow(QStringLiteral("文档方向"), docOrientationCombo_, QStringLiteral("文档方向：整张图要不要旋转（以顺时针角度为准）"));
    addClassRow(QStringLiteral("文本行方向"), textlineOrientationCombo_, QStringLiteral("文本行方向：文字行是不是上下颠倒，只管 0/180"));
    addClassRow(QStringLiteral("表格类型"), tableClassificationCombo_, QString());
    auto* applyClassButton = primaryButton(QStringLiteral("应用分类标签"), classTab);
    connect(applyClassButton, &QPushButton::clicked, this, &MainWindow::saveImageLabels);
    classLayout->addWidget(applyClassButton);
    classLayout->addStretch(1);

    auto* modelTab = new QWidget(tabs);
    auto* modelLayout = new QVBoxLayout(modelTab);
    modelLayout->setContentsMargins(10, 10, 10, 10);
    modelLayout->setSpacing(8);
    modelLayout->addWidget(sectionLabel(QStringLiteral("本地模型管理"), modelTab));
    auto addModelRow = [modelLayout, modelTab](const QString& labelText, QComboBox* combo) {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel(labelText, modelTab));
        combo->setMinimumWidth(150);
        combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        row->addWidget(combo, 1);
        modelLayout->addLayout(row);
    };
    labelDetModelCombo_ = new QComboBox(modelTab);
    labelRecModelCombo_ = new QComboBox(modelTab);
    labelTextlineClsModelCombo_ = new QComboBox(modelTab);
    labelDocOrientationModelCombo_ = new QComboBox(modelTab);
    labelDocUnwarpingModelCombo_ = new QComboBox(modelTab);
    labelLayoutModelCombo_ = new QComboBox(modelTab);
    addModelRow(QStringLiteral("Det 检测模型"), labelDetModelCombo_);
    addModelRow(QStringLiteral("Rec 识别模型"), labelRecModelCombo_);
    addModelRow(QStringLiteral("Cls 方向模型"), labelTextlineClsModelCombo_);
    addModelRow(QStringLiteral("文档方向模型"), labelDocOrientationModelCombo_);
    addModelRow(QStringLiteral("文档矫正模型"), labelDocUnwarpingModelCombo_);
    addModelRow(QStringLiteral("Layout 版面模型"), labelLayoutModelCombo_);

    labelUseGpuCheck_ = new QCheckBox(QStringLiteral("使用 GPU"), modelTab);
    labelEnableTextlineClsCheck_ = new QCheckBox(QStringLiteral("启用文本行方向分类"), modelTab);
    labelEnableDocOrientationCheck_ = new QCheckBox(QStringLiteral("启用文档方向分类"), modelTab);
    labelEnableDocUnwarpingCheck_ = new QCheckBox(QStringLiteral("启用文档矫正"), modelTab);
    prelabelScoreThresholdSpin_ = new QDoubleSpinBox(modelTab);
    prelabelScoreThresholdSpin_->setRange(0.0, 1.0);
    prelabelScoreThresholdSpin_->setDecimals(2);
    prelabelScoreThresholdSpin_->setSingleStep(0.05);
    prelabelScoreThresholdSpin_->setValue(0.50);
    prelabelScoreThresholdSpin_->setToolTip(QStringLiteral("用于 OCR 识别分数和 Layout 版面框分数。降低阈值会保留更多低置信度结果，也可能增加误标。"));
    modelLayout->addWidget(labelUseGpuCheck_);
    modelLayout->addWidget(labelEnableTextlineClsCheck_);
    modelLayout->addWidget(labelEnableDocOrientationCheck_);
    modelLayout->addWidget(labelEnableDocUnwarpingCheck_);
    auto* thresholdRow = new QHBoxLayout();
    thresholdRow->addWidget(new QLabel(QStringLiteral("预标注置信度阈值"), modelTab));
    thresholdRow->addWidget(prelabelScoreThresholdSpin_);
    thresholdRow->addStretch(1);
    modelLayout->addLayout(thresholdRow);
    auto* modelHint = mutedLabel(QStringLiteral("提示：文档方向会单独写入分类标签；文档矫正不参与 OCR 框预标注，避免坐标偏移；预标注生成的框默认为 unchecked，确认后再导出训练集。"), modelTab);
    modelHint->setWordWrap(true);
    modelLayout->addWidget(modelHint);
    auto* checkModelButton = new QPushButton(QStringLiteral("检查模型"), modelTab);
    auto* prelabelCurrentButton = primaryButton(QStringLiteral("预标注当前页"), modelTab);
    auto* prelabelAllButton = new QPushButton(QStringLiteral("一键预标注全部图片"), modelTab);
    labelModelStatus_ = mutedLabel(QStringLiteral("状态：未配置"), modelTab);
    connect(checkModelButton, &QPushButton::clicked, this, [this]() {
        saveLabelModelConfig();
        const QStringList missing = labelModelMissingReasons();
        if (missing.isEmpty()) {
            if (labelModelStatus_) {
                labelModelStatus_->setText(QStringLiteral("状态：模型目录可用"));
            }
            statusBar()->showMessage(QStringLiteral("模型检查通过"), 3000);
        } else {
            if (labelModelStatus_) {
                labelModelStatus_->setText(QStringLiteral("状态：模型不完整"));
            }
            QMessageBox::warning(this, QStringLiteral("模型检查"), missing.join(QLatin1Char('\n')));
        }
    });
    connect(prelabelCurrentButton, &QPushButton::clicked, this, [this]() {
        saveLabelModelConfig();
        const bool runOrientation =
            (labelEnableTextlineClsCheck_ && labelEnableTextlineClsCheck_->isChecked())
            || (labelEnableDocOrientationCheck_ && labelEnableDocOrientationCheck_->isChecked());
        const bool runLayout = !selectedPredictionModelDir(labelLayoutModelCombo_).isEmpty();
        startPrelabelCurrentPageAsync(
            true,
            runOrientation,
            false,
            runLayout,
            QStringLiteral("预标注当前页"));
    });
    connect(prelabelAllButton, &QPushButton::clicked, this, [this]() {
        startOneClickPrelabelAll();
    });
    for (auto* combo : {labelDetModelCombo_, labelRecModelCombo_, labelTextlineClsModelCombo_,
                       labelDocOrientationModelCombo_, labelDocUnwarpingModelCombo_, labelLayoutModelCombo_}) {
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::saveLabelModelConfig);
    }
    for (auto* checkbox : {labelUseGpuCheck_, labelEnableTextlineClsCheck_, labelEnableDocOrientationCheck_, labelEnableDocUnwarpingCheck_}) {
        connect(checkbox, &QCheckBox::toggled, this, &MainWindow::saveLabelModelConfig);
    }
    connect(prelabelScoreThresholdSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::saveLabelModelConfig);
    modelLayout->addWidget(checkModelButton);
    modelLayout->addWidget(prelabelCurrentButton);
    modelLayout->addWidget(prelabelAllButton);
    modelLayout->addWidget(labelModelStatus_);
    modelLayout->addStretch(1);

    exportDetCheck_ = new QCheckBox(QStringLiteral("PP-OCR Det"), inspector);
    exportRecCheck_ = new QCheckBox(QStringLiteral("PP-OCR Rec"), inspector);
    exportClsCheck_ = new QCheckBox(QStringLiteral("文档方向 Cls"), inspector);
    exportTextlineClsCheck_ = new QCheckBox(QStringLiteral("文本行方向 Cls"), inspector);
    exportTableClsCheck_ = new QCheckBox(QStringLiteral("表格类型 Cls"), inspector);
    exportCocoCheck_ = new QCheckBox(QStringLiteral("Layout COCO Detection"), inspector);
    exportCheckedOnlyCheck_ = new QCheckBox(QStringLiteral("仅导出 checked=true"), inspector);
    exportDetCheck_->setChecked(true);
    exportRecCheck_->setChecked(true);
    exportClsCheck_->setChecked(true);
    exportTextlineClsCheck_->setChecked(true);
    exportTableClsCheck_->setChecked(false);
    exportCocoCheck_->setChecked(true);
    exportCheckedOnlyCheck_->setChecked(true);
    auto* exportButton = primaryButton(QStringLiteral("开始导出"), inspector);
    exportProjectButton_ = exportButton;
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportProject);

    auto* exportTab = new QWidget(tabs);
    auto* exportLayout = new QVBoxLayout(exportTab);
    exportLayout->setContentsMargins(10, 10, 10, 10);
    exportLayout->setSpacing(8);
    exportLayout->addWidget(sectionLabel(QStringLiteral("导出设置"), exportTab));
    exportLayout->addWidget(exportDetCheck_);
    exportLayout->addWidget(exportRecCheck_);
    exportLayout->addWidget(exportClsCheck_);
    exportLayout->addWidget(exportTextlineClsCheck_);
    exportLayout->addWidget(exportTableClsCheck_);
    exportLayout->addWidget(exportCocoCheck_);
    auto* exportHint = mutedLabel(QStringLiteral("文档矫正 UVDoc 需要成对矫正标注，当前项目格式暂不导出该数据集。导出 PP-OCR Rec 时会自动生成 Rec Crop；表格类型 Cls 是可选数据集，需要训练表格分类时再勾选。"), exportTab);
    exportHint->setWordWrap(true);
    exportLayout->addWidget(exportHint);
    exportLayout->addSpacing(12);
    exportLayout->addWidget(exportCheckedOnlyCheck_);
    exportProgressLabel_ = mutedLabel(QStringLiteral("导出状态：空闲"), exportTab);
    exportProgressBar_ = new QProgressBar(exportTab);
    exportProgressBar_->setRange(0, 100);
    exportProgressBar_->setValue(0);
    exportProgressBar_->setTextVisible(true);
    exportLayout->addWidget(exportProgressLabel_);
    exportLayout->addWidget(exportProgressBar_);
    exportLayout->addWidget(exportButton);
    exportLayout->addStretch(1);

    auto* validationTab = new QWidget(tabs);
    auto* validationLayout = new QVBoxLayout(validationTab);
    validationLayout->setContentsMargins(10, 10, 10, 10);
    validationLayout->setSpacing(8);
    validationLayout->addWidget(sectionLabel(QStringLiteral("数据校验结果"), validationTab));
    auto* validateButton = primaryButton(QStringLiteral("重新校验"), validationTab);
    connect(validateButton, &QPushButton::clicked, this, &MainWindow::validateProject);
    labelValidationSummaryLabel_ = mutedLabel(QStringLiteral("尚未运行校验"), validationTab);
    labelValidationIssueTable_ = tableWidget(5, {
            QStringLiteral("状态"),
            QStringLiteral("任务"),
            QStringLiteral("文件"),
            QStringLiteral("框ID"),
            QStringLiteral("问题描述"),
        }, validationTab);
    labelValidationIssueTable_->setObjectName("LabelValidationTable");
    validationLayout->addWidget(validateButton);
    validationLayout->addWidget(labelValidationIssueTable_, 1);
    validationLayout->addWidget(labelValidationSummaryLabel_);

    tabs->addTab(regionTab, QStringLiteral("属性"));
    tabs->addTab(classTab, QStringLiteral("分类"));
    tabs->addTab(modelTab, QStringLiteral("模型"));
    tabs->addTab(exportTab, QStringLiteral("导出"));
    tabs->addTab(validationTab, QStringLiteral("校验"));
    inspectorLayout->addWidget(tabs, 1);
    inspectorScroll->setWidget(inspector);

    splitter->addWidget(left);
    splitter->addWidget(center);
    splitter->addWidget(inspectorScroll);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({210, 700, 320});
    connect(pageList_, &QListWidget::currentRowChanged, this, &MainWindow::loadSelectedPage);

    layout->addWidget(header, 0);
    layout->addWidget(splitter, 1);

    updateSelectedRegionPanel();
    updateAnnotationHistoryActions();
    refreshLabelModelChoices();
    return page;
}

QWidget* MainWindow::buildLabelPageLegacy() {
    auto* page = new QWidget(this);
    page->setObjectName("LabelRoot");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto* header = new QWidget(page);
    header->setObjectName("HeaderPanel");
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 10, 14, 10);
    headerLayout->setSpacing(10);
    auto* titleBox = new QVBoxLayout();
    titleBox->setSpacing(3);
    auto* title = new QLabel(QStringLiteral("标注工作台"), header);
    title->setObjectName("PageTitle");
    labelWorkspaceSummary_ = new QLabel(QStringLiteral("未打开项目"), header);
    labelWorkspaceSummary_->setObjectName("Muted");
    titleBox->addWidget(title);
    titleBox->addWidget(labelWorkspaceSummary_);
    auto* headerActions = new QHBoxLayout();
    auto* headerImportButton = new QPushButton(QStringLiteral("导入图片"), header);
    auto* headerImportPdfButton = new QPushButton(QStringLiteral("导入PDF"), header);
    auto* createTrainingButton = new QPushButton(QStringLiteral("+ 创建训练任务"), header);
    auto* headerPrelabelButton = new QPushButton(QStringLiteral("预标注"), header);
    auto* headerValidateButton = new QPushButton(QStringLiteral("校验"), header);
    auto* headerExportButton = new QPushButton(QStringLiteral("导出"), header);
    createTrainingButton->setObjectName("Primary");
    headerPrelabelButton->setObjectName("Primary");
    connect(headerImportButton, &QPushButton::clicked, this, &MainWindow::importImagesDialog);
    connect(headerImportPdfButton, &QPushButton::clicked, this, &MainWindow::importPdfsDialog);
    connect(createTrainingButton, &QPushButton::clicked, this, [this]() { stack_->setCurrentIndex(2); });
    connect(headerPrelabelButton, &QPushButton::clicked, this, &MainWindow::prelabelSelectedPage);
    connect(headerValidateButton, &QPushButton::clicked, this, &MainWindow::validateProject);
    connect(headerExportButton, &QPushButton::clicked, this, &MainWindow::exportProject);
    headerActions->addWidget(headerImportButton);
    headerActions->addWidget(headerImportPdfButton);
    headerActions->addSpacing(8);
    headerActions->addWidget(createTrainingButton);
    headerActions->addWidget(headerPrelabelButton);
    headerActions->addWidget(headerValidateButton);
    headerActions->addWidget(headerExportButton);
    headerActions->addStretch(1);
    headerLayout->addLayout(titleBox, 1);
    headerLayout->addLayout(headerActions, 0);

    auto* splitter = new QSplitter(Qt::Horizontal, page);
    auto* left = new QWidget(splitter);
    left->setObjectName("Panel");
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(12, 12, 12, 12);
    leftLayout->setSpacing(10);
    pageSearchEdit_ = new QLineEdit(left);
    pageSearchEdit_->setPlaceholderText(QStringLiteral("搜索页面 / 文件名 / 状态"));
    pageSplitFilterCombo_ = new QComboBox(left);
    pageSplitFilterCombo_->addItem(QStringLiteral("全部集合"), QString());
    pageSplitFilterCombo_->addItem(QStringLiteral("训练集"), QStringLiteral("train"));
    pageSplitFilterCombo_->addItem(QStringLiteral("验证集"), QStringLiteral("val"));
    pageStatusFilterCombo_ = new QComboBox(left);
    pageStatusFilterCombo_->addItem(QStringLiteral("全部状态"), QString());
    pageStatusFilterCombo_->addItem(QStringLiteral("未标注"), QStringLiteral("unlabeled"));
    pageStatusFilterCombo_->addItem(QStringLiteral("已标注"), QStringLiteral("labeled"));
    pageStatusFilterCombo_->addItem(QStringLiteral("已校验"), QStringLiteral("validated"));
    auto* filterRow = new QHBoxLayout();
    filterRow->addWidget(pageSplitFilterCombo_);
    filterRow->addWidget(pageStatusFilterCombo_);
    pageList_ = new QListWidget(left);
    pageList_->setIconSize(QSize(72, 96));
    pageMetaLabel_ = new QLabel(QStringLiteral("请选择页面"), left);
    pageMetaLabel_->setObjectName("Muted");
    pageSplitCombo_ = new QComboBox(left);
    pageSplitCombo_->addItem(QStringLiteral("训练集"), QStringLiteral("train"));
    pageSplitCombo_->addItem(QStringLiteral("验证集"), QStringLiteral("val"));
    pageStatusCombo_ = new QComboBox(left);
    pageStatusCombo_->addItem(QStringLiteral("未标注"), QStringLiteral("unlabeled"));
    pageStatusCombo_->addItem(QStringLiteral("已标注"), QStringLiteral("labeled"));
    pageStatusCombo_->addItem(QStringLiteral("已校验"), QStringLiteral("validated"));
    auto* metadataRow = new QHBoxLayout();
    metadataRow->addWidget(pageSplitCombo_);
    metadataRow->addWidget(pageStatusCombo_);
    connect(pageSearchEdit_, &QLineEdit::textChanged, this, &MainWindow::filterPages);
    connect(pageSplitFilterCombo_, &QComboBox::currentTextChanged, this, &MainWindow::filterPages);
    connect(pageStatusFilterCombo_, &QComboBox::currentTextChanged, this, &MainWindow::filterPages);
    connect(pageSplitCombo_, &QComboBox::currentTextChanged, this, &MainWindow::savePageMetadata);
    connect(pageStatusCombo_, &QComboBox::currentTextChanged, this, &MainWindow::savePageMetadata);
    auto* pageHeader = new QHBoxLayout();
    auto* pageTitle = sectionLabel(QStringLiteral("项目文件"), left);
    pageCountLabel_ = new QLabel(QStringLiteral("0 页"), left);
    pageCountLabel_->setObjectName("Muted");
    pageHeader->addWidget(pageTitle);
    pageHeader->addStretch(1);
    pageHeader->addWidget(pageCountLabel_);
    leftLayout->addLayout(pageHeader);
    leftLayout->addWidget(pageMetaLabel_);
    leftLayout->addWidget(pageList_, 1);

    auto* center = new QWidget(splitter);
    center->setObjectName("CanvasPanel");
    auto* centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(8);

    auto* canvasHeader = new QWidget(center);
    canvasHeader->setObjectName("CanvasHeader");
    auto* canvasHeaderLayout = new QHBoxLayout(canvasHeader);
    canvasHeaderLayout->setContentsMargins(12, 10, 12, 10);
    canvasHeaderLayout->setSpacing(8);
    auto* fileTitleBox = new QVBoxLayout();
    fileTitleBox->setSpacing(2);
    fileTitleLabel_ = sectionLabel(QStringLiteral("未选择图片"), canvasHeader);
    fileMetaLabel_ = new QLabel(QStringLiteral("选择左侧图片开始标注"), canvasHeader);
    fileMetaLabel_->setObjectName("Muted");
    fileTitleBox->addWidget(fileTitleLabel_);
    fileTitleBox->addWidget(fileMetaLabel_);
    canvasHeaderLayout->addLayout(fileTitleBox, 1);

    selectToolButton_ = new QPushButton(QStringLiteral("选择"), canvasHeader);
    drawOcrToolButton_ = new QPushButton(QStringLiteral("OCR框"), canvasHeader);
    drawLayoutToolButton_ = new QPushButton(QStringLiteral("版面框"), canvasHeader);
    panToolButton_ = new QPushButton(QStringLiteral("平移"), canvasHeader);
    for (auto* button : {selectToolButton_, drawOcrToolButton_, drawLayoutToolButton_}) {
        button->setObjectName("ModeButton");
        button->setCheckable(true);
        canvasHeaderLayout->addWidget(button);
    }
    panToolButton_->setObjectName("ModeButton");
    panToolButton_->setCheckable(true);
    selectToolButton_->setChecked(true);
    connect(selectToolButton_, &QPushButton::clicked, this, [this]() { setCanvasToolMode("select"); });
    connect(drawOcrToolButton_, &QPushButton::clicked, this, [this]() { setCanvasToolMode("drawOcr"); });
    connect(drawLayoutToolButton_, &QPushButton::clicked, this, [this]() { setCanvasToolMode("drawLayout"); });
    connect(panToolButton_, &QPushButton::clicked, this, [this]() { setCanvasToolMode("pan"); });
    auto* fitButton = new QPushButton(QStringLiteral("适应"), canvasHeader);
    auto* actualButton = new QPushButton(QStringLiteral("100%"), canvasHeader);
    connect(fitButton, &QPushButton::clicked, this, [this]() {
        if (auto* root = canvasWidget_ ? canvasWidget_->rootObject() : nullptr) {
            QMetaObject::invokeMethod(root, "resetView");
        }
    });
    connect(actualButton, &QPushButton::clicked, this, [this]() {
        if (auto* root = canvasWidget_ ? canvasWidget_->rootObject() : nullptr) {
            root->setProperty("zoomScale", 1.0);
            root->setProperty("panX", 0.0);
            root->setProperty("panY", 0.0);
        }
    });
    canvasHeaderLayout->addWidget(fitButton);
    canvasHeaderLayout->addWidget(actualButton);

    canvasWidget_ = new QQuickWidget(center);
    canvasWidget_->setObjectName("AnnotationCanvas");
    canvasWidget_->setResizeMode(QQuickWidget::SizeRootObjectToView);
    canvasWidget_->setSource(QUrl("qrc:/qml/AnnotationCanvas.qml"));
    connectCanvasSignals();

    auto* canvasFooter = new QHBoxLayout();
    canvasModeLabel_ = new QLabel(QStringLiteral("当前模式：选择"), center);
    canvasModeLabel_->setObjectName("Muted");
    auto* canvasHint = new QLabel(QStringLiteral("滚轮缩放 | 拖动画布 | OCR/版面框模式下拖拽画框"), center);
    canvasHint->setObjectName("Muted");
    canvasFooter->addWidget(canvasModeLabel_);
    canvasFooter->addStretch(1);
    canvasFooter->addWidget(canvasHint);
    centerLayout->addWidget(canvasHeader);
    centerLayout->addWidget(canvasWidget_, 1);
    centerLayout->addLayout(canvasFooter);

    auto* inspectorScroll = new QScrollArea(splitter);
    inspectorScroll->setWidgetResizable(true);
    inspectorScroll->setFrameShape(QFrame::NoFrame);
    inspectorScroll->setMinimumWidth(260);
    inspectorScroll->setMaximumWidth(500);
    auto* inspector = new QWidget(inspectorScroll);
    inspector->setObjectName("InspectorPanel");
    auto* inspectorLayout = new QVBoxLayout(inspector);
    inspectorLayout->setContentsMargins(10, 10, 10, 10);
    inspectorLayout->setSpacing(8);
    undoAnnotationButton_ = new QPushButton(QStringLiteral("撤销"), inspector);
    redoAnnotationButton_ = new QPushButton(QStringLiteral("重做"), inspector);
    connect(undoAnnotationButton_, &QPushButton::clicked, this, &MainWindow::undoAnnotationEdit);
    connect(redoAnnotationButton_, &QPushButton::clicked, this, &MainWindow::redoAnnotationEdit);
    auto* historyLayout = new QHBoxLayout();
    historyLayout->addWidget(undoAnnotationButton_);
    historyLayout->addWidget(redoAnnotationButton_);
    auto* undoShortcut = new QShortcut(QKeySequence::Undo, page);
    auto* redoShortcut = new QShortcut(QKeySequence::Redo, page);
    connect(undoShortcut, &QShortcut::activated, this, &MainWindow::undoAnnotationEdit);
    connect(redoShortcut, &QShortcut::activated, this, &MainWindow::redoAnnotationEdit);

    selectedRegionLabel_ = new QLabel(QStringLiteral("未选择区域"), inspector);
    selectedRegionLabel_->setObjectName("Muted");
    regionTextEdit_ = new QPlainTextEdit(inspector);
    regionTextEdit_->setPlaceholderText(QStringLiteral("OCR 文本"));
    regionTextEdit_->setMinimumHeight(180);
    layoutLabelCombo_ = new QComboBox(inspector);
    regionReadingOrderSpin_ = new QSpinBox(inspector);
    regionReadingOrderSpin_->setRange(0, 999999);
    regionReadingOrderSpin_->setPrefix(QStringLiteral("顺序 "));
    regionCheckedCheck_ = new QCheckBox(QStringLiteral("已检查"), inspector);
    regionIgnoreCheck_ = new QCheckBox(QStringLiteral("忽略识别"), inspector);
    saveRegionButton_ = new QPushButton(QStringLiteral("保存区域"), inspector);
    deleteRegionButton_ = new QPushButton(QStringLiteral("删除区域"), inspector);
    auto* generateCropsButton = new QPushButton(QStringLiteral("生成 Rec 裁剪"), inspector);
    auto* clearPageButton = new QPushButton(QStringLiteral("清空当前页"), inspector);
    auto* clearAllButton = new QPushButton(QStringLiteral("清空全部标注"), inspector);
    clearPageButton->setObjectName("Danger");
    clearAllButton->setObjectName("Danger");
    docOrientationCombo_ = new QComboBox(inspector);
    textlineOrientationCombo_ = new QComboBox(inspector);
    tableClassificationCombo_ = new QComboBox(inspector);
    exportDetCheck_ = new QCheckBox(QStringLiteral("导出 Det"), inspector);
    exportRecCheck_ = new QCheckBox(QStringLiteral("导出 Rec"), inspector);
    exportClsCheck_ = new QCheckBox(QStringLiteral("导出文档方向"), inspector);
    exportTextlineClsCheck_ = new QCheckBox(QStringLiteral("导出文本行方向"), inspector);
    exportTableClsCheck_ = new QCheckBox(QStringLiteral("导出表格分类"), inspector);
    exportCocoCheck_ = new QCheckBox(QStringLiteral("导出版面 COCO"), inspector);
    exportCheckedOnlyCheck_ = new QCheckBox(QStringLiteral("仅已检查区域"), inspector);
    exportDetCheck_->setChecked(true);
    exportRecCheck_->setChecked(true);
    exportClsCheck_->setChecked(true);
    exportTextlineClsCheck_->setChecked(true);
    exportTableClsCheck_->setChecked(false);
    exportCocoCheck_->setChecked(true);
    exportCheckedOnlyCheck_->setChecked(true);
    connect(saveRegionButton_, &QPushButton::clicked, this, &MainWindow::saveSelectedRegionText);
    connect(deleteRegionButton_, &QPushButton::clicked, this, &MainWindow::deleteSelectedRegion);
    connect(generateCropsButton, &QPushButton::clicked, this, &MainWindow::generateCurrentRecCrops);
    connect(clearPageButton, &QPushButton::clicked, this, &MainWindow::clearCurrentPageAnnotations);
    connect(clearAllButton, &QPushButton::clicked, this, &MainWindow::clearAllAnnotations);
    connect(docOrientationCombo_, &QComboBox::currentTextChanged, this, &MainWindow::saveImageLabels);
    connect(textlineOrientationCombo_, &QComboBox::currentTextChanged, this, &MainWindow::saveImageLabels);
    connect(tableClassificationCombo_, &QComboBox::currentTextChanged, this, &MainWindow::saveImageLabels);

    auto* importButton = new QPushButton(QStringLiteral("导入图片/PDF"), inspector);
    auto* prelabelButton = new QPushButton(QStringLiteral("预标注当前页 OCR"), inspector);
    auto* prelabelAllButton = new QPushButton(QStringLiteral("预标注全部 OCR"), inspector);
    auto* prelabelClsButton = new QPushButton(QStringLiteral("预标注当前页方向"), inspector);
    auto* prelabelAllClsButton = new QPushButton(QStringLiteral("预标注全部方向"), inspector);
    auto* prelabelTableButton = new QPushButton(QStringLiteral("预标注当前页表格"), inspector);
    auto* prelabelAllTableButton = new QPushButton(QStringLiteral("预标注全部表格"), inspector);
    auto* prelabelLayoutButton = new QPushButton(QStringLiteral("预标注当前页版面"), inspector);
    auto* prelabelAllLayoutButton = new QPushButton(QStringLiteral("预标注全部版面"), inspector);
    auto* validateButton = new QPushButton(QStringLiteral("校验项目"), inspector);
    auto* exportButton = new QPushButton(QStringLiteral("导出数据集"), inspector);
    exportProjectButton_ = exportButton;
    connect(importButton, &QPushButton::clicked, this, &MainWindow::importImagesDialog);
    connect(prelabelButton, &QPushButton::clicked, this, &MainWindow::prelabelSelectedPage);
    connect(prelabelAllButton, &QPushButton::clicked, this, &MainWindow::prelabelAllPages);
    connect(prelabelClsButton, &QPushButton::clicked, this, &MainWindow::prelabelSelectedPageClassification);
    connect(prelabelAllClsButton, &QPushButton::clicked, this, &MainWindow::prelabelAllPagesClassification);
    connect(prelabelTableButton, &QPushButton::clicked, this, &MainWindow::prelabelSelectedPageTableClassification);
    connect(prelabelAllTableButton, &QPushButton::clicked, this, &MainWindow::prelabelAllPagesTableClassification);
    connect(prelabelLayoutButton, &QPushButton::clicked, this, &MainWindow::prelabelSelectedPageLayout);
    connect(prelabelAllLayoutButton, &QPushButton::clicked, this, &MainWindow::prelabelAllPagesLayout);
    connect(validateButton, &QPushButton::clicked, this, &MainWindow::validateProject);
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportProject);
    auto* tabs = new QTabWidget(inspector);

    auto* regionTab = new QWidget(tabs);
    auto* regionLayout = new QVBoxLayout(regionTab);
    auto* regionForm = new QFormLayout();
    regionForm->addRow(QStringLiteral("文本内容"), regionTextEdit_);
    regionForm->addRow(QString(), regionIgnoreCheck_);
    regionForm->addRow(QString(), regionCheckedCheck_);
    regionForm->addRow(QStringLiteral("阅读顺序"), regionReadingOrderSpin_);
    regionForm->addRow(QStringLiteral("版面类别"), layoutLabelCombo_);
    saveRegionButton_->setObjectName("Primary");
    deleteRegionButton_->setObjectName("Danger");
    regionLayout->addWidget(sectionLabel(QStringLiteral("文本框 / 版面框属性"), regionTab));
    regionLayout->addWidget(selectedRegionLabel_);
    regionLayout->addLayout(regionForm);
    regionLayout->addLayout(historyLayout);
    regionLayout->addWidget(saveRegionButton_);
    regionLayout->addWidget(generateCropsButton);
    regionLayout->addWidget(deleteRegionButton_);
    regionLayout->addWidget(clearPageButton);
    regionLayout->addWidget(clearAllButton);
    regionLayout->addStretch(1);

    auto* classTab = new QWidget(tabs);
    auto* classLayout = new QVBoxLayout(classTab);
    auto* classForm = new QFormLayout();
    classForm->addRow(QStringLiteral("文档方向"), docOrientationCombo_);
    auto* docHint = new QLabel(QStringLiteral("文档方向：整张图要不要旋转(以顺时针角度为准)"), classTab);
    docHint->setObjectName("Muted");
    classForm->addRow(QString(), docHint);
    classForm->addRow(QStringLiteral("文本行方向"), textlineOrientationCombo_);
    auto* textlineHint = new QLabel(QStringLiteral("文本行方向：文字行是不是上下颠倒，只管 0/180"), classTab);
    textlineHint->setObjectName("Muted");
    classForm->addRow(QString(), textlineHint);
    classForm->addRow(QStringLiteral("表格类型"), tableClassificationCombo_);
    auto* applyClassButton = new QPushButton(QStringLiteral("应用分类标签"), classTab);
    applyClassButton->setObjectName("Primary");
    connect(applyClassButton, &QPushButton::clicked, this, &MainWindow::saveImageLabels);
    classLayout->addWidget(sectionLabel(QStringLiteral("分类标注"), classTab));
    classLayout->addLayout(classForm);
    classLayout->addWidget(applyClassButton);
    classLayout->addStretch(1);

    auto* modelTab = new QWidget(tabs);
    auto* modelLayout = new QVBoxLayout(modelTab);
    modelLayout->addWidget(sectionLabel(QStringLiteral("本地模型管理"), modelTab));
    modelLayout->addWidget(importButton);
    modelLayout->addWidget(prelabelButton);
    modelLayout->addWidget(prelabelAllButton);
    modelLayout->addWidget(prelabelClsButton);
    modelLayout->addWidget(prelabelAllClsButton);
    modelLayout->addWidget(prelabelTableButton);
    modelLayout->addWidget(prelabelAllTableButton);
    modelLayout->addWidget(prelabelLayoutButton);
    modelLayout->addWidget(prelabelAllLayoutButton);
    auto* modelStatus = new QLabel(QStringLiteral("状态：使用预测页当前模型配置"), modelTab);
    modelStatus->setObjectName("Muted");
    modelLayout->addWidget(modelStatus);
    modelLayout->addStretch(1);

    auto* exportTab = new QWidget(tabs);
    auto* exportLayout = new QVBoxLayout(exportTab);
    auto* exportHint = new QLabel(QStringLiteral("导出 PP-OCR Rec 时会自动生成 Rec Crop；勾选“仅已检查区域”时，未确认框不会进入训练集。"), exportTab);
    exportHint->setObjectName("Muted");
    exportHint->setWordWrap(true);
    exportButton->setObjectName("Primary");
    exportLayout->addWidget(sectionLabel(QStringLiteral("导出设置"), exportTab));
    exportLayout->addWidget(exportDetCheck_);
    exportLayout->addWidget(exportRecCheck_);
    exportLayout->addWidget(exportClsCheck_);
    exportLayout->addWidget(exportTextlineClsCheck_);
    exportLayout->addWidget(exportTableClsCheck_);
    exportLayout->addWidget(exportCocoCheck_);
    exportLayout->addWidget(exportHint);
    exportLayout->addSpacing(16);
    exportLayout->addWidget(exportCheckedOnlyCheck_);
    exportProgressLabel_ = new QLabel(QStringLiteral("导出状态：空闲"), exportTab);
    exportProgressLabel_->setObjectName("Muted");
    exportProgressBar_ = new QProgressBar(exportTab);
    exportProgressBar_->setRange(0, 100);
    exportProgressBar_->setValue(0);
    exportProgressBar_->setTextVisible(true);
    exportLayout->addWidget(exportProgressLabel_);
    exportLayout->addWidget(exportProgressBar_);
    exportLayout->addWidget(exportButton);
    exportLayout->addStretch(1);

    auto* validationTab = new QWidget(tabs);
    auto* validationLayout = new QVBoxLayout(validationTab);
    validateButton->setObjectName("Primary");
    validationLayout->addWidget(sectionLabel(QStringLiteral("数据校验结果"), validationTab));
    validationLayout->addWidget(validateButton);
    auto* validationHint = new QLabel(QStringLiteral("校验结果会同步显示在概况页的“校验结果”表格中。"), validationTab);
    validationHint->setObjectName("Muted");
    validationHint->setWordWrap(true);
    validationLayout->addWidget(validationHint);
    validationLayout->addStretch(1);

    tabs->addTab(regionTab, QStringLiteral("属性"));
    tabs->addTab(classTab, QStringLiteral("分类"));
    tabs->addTab(modelTab, QStringLiteral("模型"));
    tabs->addTab(exportTab, QStringLiteral("导出"));
    tabs->addTab(validationTab, QStringLiteral("校验"));

    inspectorLayout->addWidget(sectionLabel(QStringLiteral("属性检查器"), inspector));
    inspectorLayout->addWidget(tabs, 1);
    inspectorScroll->setWidget(inspector);
    updateSelectedRegionPanel();
    updateAnnotationHistoryActions();

    splitter->addWidget(left);
    splitter->addWidget(center);
    splitter->addWidget(inspectorScroll);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({240, 820, 260});
    connect(pageList_, &QListWidget::currentRowChanged, this, &MainWindow::loadSelectedPage);
    layout->addWidget(header, 0);
    layout->addWidget(splitter, 1);
    return page;
}

QWidget* MainWindow::buildTrainingPage() {
    auto* page = new QWidget(this);
    page->setObjectName("TrainingRoot");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(10);

    trainingTaskCombo_ = new QComboBox(page);
    for (const auto& task : trainingTasks()) {
        trainingTaskCombo_->addItem(task.title, task.key);
    }
    trainingDeviceCombo_ = new QComboBox(page);
    trainingDeviceCombo_->setEditable(true);
    trainingDeviceCombo_->addItems({"cpu", "gpu:0"});
    trainingEpochsSpin_ = new QSpinBox(page);
    trainingEpochsSpin_->setRange(1, 100000);
    trainingBatchSpin_ = new QSpinBox(page);
    trainingBatchSpin_->setRange(1, 4096);
    trainingLearningRateSpin_ = new QDoubleSpinBox(page);
    trainingLearningRateSpin_->setDecimals(8);
    trainingLearningRateSpin_->setRange(0.0, 100.0);
    trainingLearningRateSpin_->setSingleStep(0.001);
    connect(trainingTaskCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::trainingTaskChanged);

    auto* screens = new QStackedWidget(page);
    trainingScreens_ = screens;

    auto* header = workbenchCard(page, QStringLiteral("HeaderPanel"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 10, 14, 10);
    headerLayout->setSpacing(12);
    auto* titleBox = new QVBoxLayout();
    titleBox->setSpacing(3);
    auto* title = new QLabel(QStringLiteral("训练任务"), header);
    title->setObjectName("PageTitle");
    auto* subtitle = mutedLabel(QStringLiteral("查看所有训练任务，点击卡片进入详情并查看每次训练记录"), header);
    titleBox->addWidget(title);
    titleBox->addWidget(subtitle);
    trainingHeaderProjectLabel_ = mutedLabel(QStringLiteral("当前项目：未打开"), header);
    trainingHeaderStatusLabel_ = new QLabel(QStringLiteral("状态：未启动"), header);
    trainingHeaderStatusLabel_->setObjectName("StatusOk");
    auto* versionManageButton = new QPushButton(QStringLiteral("模型版本"), header);
    auto* newTaskButton = primaryButton(QStringLiteral("进入训练配置"), header);
    auto* backToListButton = new QPushButton(QStringLiteral("任务总览"), header);
    connect(versionManageButton, &QPushButton::clicked, this, [screens]() { screens->setCurrentIndex(2); });
    connect(newTaskButton, &QPushButton::clicked, this, [screens]() { screens->setCurrentIndex(1); });
    connect(backToListButton, &QPushButton::clicked, this, [screens]() { screens->setCurrentIndex(0); });
    headerLayout->addLayout(titleBox, 1);
    headerLayout->addWidget(trainingHeaderProjectLabel_, 1);
    headerLayout->addWidget(trainingHeaderStatusLabel_);
    headerLayout->addWidget(versionManageButton);
    headerLayout->addWidget(newTaskButton);
    headerLayout->addWidget(backToListButton);

    auto* listScreen = new QWidget(screens);
    auto* listLayout = new QVBoxLayout(listScreen);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(10);
    auto* filterRow = new QHBoxLayout();
    trainingTaskSearchEdit_ = new QLineEdit(listScreen);
    trainingTaskSearchEdit_->setPlaceholderText(QStringLiteral("搜索任务名称、数据集或备注"));
    trainingTaskTypeFilter_ = new QComboBox(listScreen);
    trainingTaskTypeFilter_->addItem(QStringLiteral("任务类型：全部"), QString());
    trainingTaskTypeFilter_->addItem(QStringLiteral("Det"), QStringLiteral("det"));
    trainingTaskTypeFilter_->addItem(QStringLiteral("Rec"), QStringLiteral("rec"));
    trainingTaskTypeFilter_->addItem(QStringLiteral("Cls"), QStringLiteral("cls"));
    trainingTaskTypeFilter_->addItem(QStringLiteral("Layout"), QStringLiteral("layout"));
    trainingTaskTypeFilter_->addItem(QStringLiteral("UVDoc"), QStringLiteral("uvdoc"));
    trainingTaskStatusFilter_ = new QComboBox(listScreen);
    trainingTaskStatusFilter_->addItem(QStringLiteral("状态：全部"), QString());
    trainingTaskStatusFilter_->addItem(QStringLiteral("未开始"), QStringLiteral("idle"));
    trainingTaskStatusFilter_->addItem(QStringLiteral("进行中"), QStringLiteral("running"));
    trainingTaskStatusFilter_->addItem(QStringLiteral("已完成"), QStringLiteral("success"));
    trainingTaskStatusFilter_->addItem(QStringLiteral("失败"), QStringLiteral("failed"));
    trainingTaskStatusFilter_->addItem(QStringLiteral("已停止"), QStringLiteral("stopped"));
    trainingTaskStatusFilter_->addItem(QStringLiteral("暂不支持"), QStringLiteral("unsupported"));
    trainingTaskSortCombo_ = new QComboBox(listScreen);
    trainingTaskSortCombo_->addItem(QStringLiteral("排序：最近训练时间"), QStringLiteral("recent"));
    trainingTaskSortCombo_->addItem(QStringLiteral("排序：任务类型"), QStringLiteral("kind"));
    trainingTaskSortCombo_->addItem(QStringLiteral("排序：名称"), QStringLiteral("name"));
    auto* refreshButton = new QPushButton(QStringLiteral("刷新"), listScreen);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshTrainingVersions);
    connect(trainingTaskSearchEdit_, &QLineEdit::textChanged, this, &MainWindow::refreshTrainingTaskOverview);
    connect(trainingTaskTypeFilter_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::refreshTrainingTaskOverview);
    connect(trainingTaskStatusFilter_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::refreshTrainingTaskOverview);
    connect(trainingTaskSortCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::refreshTrainingTaskOverview);
    filterRow->addWidget(trainingTaskSearchEdit_, 2);
    filterRow->addWidget(trainingTaskTypeFilter_);
    filterRow->addWidget(trainingTaskStatusFilter_);
    filterRow->addStretch(1);
    filterRow->addWidget(trainingTaskSortCombo_);
    filterRow->addWidget(refreshButton);
    listLayout->addLayout(filterRow);

    auto* summaryGrid = new QGridLayout();
    summaryGrid->setSpacing(10);
    auto addSummary = [listScreen, summaryGrid](int column, const QString& title, const QString& value, const QString& subtitle, const QColor& color) {
        auto* card = workbenchCard(listScreen, QStringLiteral("MetricPanel"));
        auto* cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(12, 10, 12, 10);
        auto* icon = new QLabel(QStringLiteral("□"), card);
        icon->setObjectName("OverviewMetricIcon");
        icon->setStyleSheet(QStringLiteral("background:%1;").arg(color.name()));
        auto* textBox = new QVBoxLayout();
        textBox->addWidget(mutedLabel(title, card));
        auto* valueLabel = new QLabel(value, card);
        valueLabel->setObjectName("MetricValue");
        textBox->addWidget(valueLabel);
        textBox->addWidget(mutedLabel(subtitle, card));
        cardLayout->addWidget(icon);
        cardLayout->addLayout(textBox, 1);
        summaryGrid->addWidget(card, 0, column);
        return valueLabel;
    };
    trainingTotalTasksMetric_ = addSummary(0, QStringLiteral("任务总数"), QString::number(trainingTasks().size()), QStringLiteral("所有训练任务"), QColor("#2563eb"));
    trainingRunningTasksMetric_ = addSummary(1, QStringLiteral("进行中"), QStringLiteral("0"), QStringLiteral("正在训练"), QColor("#14786d"));
    trainingDoneTasksMetric_ = addSummary(2, QStringLiteral("已完成"), QStringLiteral("0"), QStringLiteral("已有成功版本"), QColor("#46d39a"));
    trainingFailedTasksMetric_ = addSummary(3, QStringLiteral("失败任务"), QStringLiteral("0"), QStringLiteral("最近一次失败"), QColor("#b0444d"));
    trainingTodayTasksMetric_ = addSummary(4, QStringLiteral("今日新增"), QStringLiteral("0"), QStringLiteral("今天有输出"), QColor("#d79b35"));
    listLayout->addLayout(summaryGrid);

    auto* taskScroll = new QScrollArea(listScreen);
    taskScroll->setWidgetResizable(true);
    taskScroll->setFrameShape(QFrame::NoFrame);
    trainingTaskHost_ = new QWidget(taskScroll);
    trainingTaskGrid_ = new QGridLayout(trainingTaskHost_);
    trainingTaskGrid_->setContentsMargins(0, 0, 0, 0);
    trainingTaskGrid_->setSpacing(10);
    trainingTaskHost_->installEventFilter(this);
    taskScroll->setWidget(trainingTaskHost_);
    listLayout->addWidget(taskScroll, 1);

    auto* detailScreen = new QWidget(screens);
    auto* detailLayout = new QHBoxLayout(detailScreen);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->setSpacing(12);
    auto* leftScroll = new QScrollArea(detailScreen);
    leftScroll->setWidgetResizable(true);
    leftScroll->setFrameShape(QFrame::NoFrame);
    auto* left = new QWidget(leftScroll);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(10);

    auto* versionPanel = workbenchCard(left, QStringLiteral("TrainingCard"));
    auto* versionLayout = new QHBoxLayout(versionPanel);
    versionLayout->setContentsMargins(12, 10, 12, 10);
    auto* versionIntro = new QVBoxLayout();
    versionIntro->addWidget(sectionLabel(QStringLiteral("训练版本"), versionPanel));
    versionIntro->addWidget(mutedLabel(QStringLiteral("每次训练都会生成独立版本，便于追溯与对比。"), versionPanel));
    auto* newVersionButton = primaryButton(QStringLiteral("开始新训练"), versionPanel);
    newVersionButton->setToolTip(QStringLiteral("用当前参数启动训练，并自动生成一个新的模型版本。"));
    connect(newVersionButton, &QPushButton::clicked, this, &MainWindow::startTraining);
    versionIntro->addWidget(newVersionButton);
    trainingVersionList_ = new QListWidget(page);
    trainingVersionList_->setObjectName("TrainingVersionList");
    trainingVersionList_->setFlow(QListView::LeftToRight);
    trainingVersionList_->setWrapping(false);
    trainingVersionList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    trainingVersionList_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    trainingVersionList_->setMinimumHeight(112);
    connect(trainingVersionList_, &QListWidget::currentRowChanged, this, &MainWindow::updateSelectedTrainingVersionDetails);
    versionLayout->addLayout(versionIntro);
    versionLayout->addWidget(trainingVersionList_, 1);

    auto* selectedVersionPanel = workbenchCard(left, QStringLiteral("TrainingCard"));
    auto* selectedLayout = new QHBoxLayout(selectedVersionPanel);
    selectedLayout->setContentsMargins(12, 12, 12, 12);
    trainingVersionSummary_ = new QLabel(QStringLiteral("当前版本\n-"), selectedVersionPanel);
    trainingVersionSummary_->setObjectName("TrainingVersionSummary");
    auto* compareButton = new QPushButton(QStringLiteral("对比版本"), selectedVersionPanel);
    auto* setCurrentButton = primaryButton(QStringLiteral("设为当前版本"), selectedVersionPanel);
    auto* openVersionButton = new QPushButton(QStringLiteral("打开目录"), selectedVersionPanel);
    auto* deleteVersionButton = dangerButton(QStringLiteral("删除版本"), selectedVersionPanel);
    connect(compareButton, &QPushButton::clicked, this, &MainWindow::compareSelectedTrainingVersions);
    connect(setCurrentButton, &QPushButton::clicked, this, &MainWindow::setSelectedTrainingVersionCurrent);
    connect(openVersionButton, &QPushButton::clicked, this, &MainWindow::openSelectedTrainingVersionDir);
    connect(deleteVersionButton, &QPushButton::clicked, this, &MainWindow::deleteSelectedTrainingVersion);
    selectedLayout->addWidget(trainingVersionSummary_, 1);
    selectedLayout->addWidget(compareButton);
    selectedLayout->addWidget(setCurrentButton);
    selectedLayout->addWidget(openVersionButton);
    selectedLayout->addWidget(deleteVersionButton);

    auto* progressPanel = workbenchCard(left, QStringLiteral("TrainingCard"));
    auto* progressLayout = new QVBoxLayout(progressPanel);
    progressLayout->setContentsMargins(12, 10, 12, 10);
    progressLayout->addWidget(sectionLabel(QStringLiteral("训练进度"), progressPanel));
    trainingProgressMetaLabel_ = mutedLabel(QStringLiteral("任务名称：-        开始时间：-        当前轮数：0 / 20 (0%)        预计剩余时间：-        状态：未启动"), progressPanel);
    trainingProgressBar_ = new QProgressBar(progressPanel);
    trainingProgressBar_->setRange(0, 100);
    trainingProgressBar_->setValue(0);
    trainingProgressBar_->setTextVisible(true);
    progressLayout->addWidget(trainingProgressMetaLabel_);
    progressLayout->addWidget(trainingProgressBar_);

    auto* chartPanel = workbenchCard(left, QStringLiteral("TrainingCard"));
    auto* chartLayout = new QVBoxLayout(chartPanel);
    chartLayout->setContentsMargins(12, 10, 12, 10);
    chartLayout->addWidget(sectionLabel(QStringLiteral("训练指标曲线"), chartPanel));
    auto* chartRow = new QHBoxLayout();
    trainingLossChart_ = new TrainingPlaceholderChart(QStringLiteral("Loss 曲线"), chartPanel);
    trainingAccuracyChart_ = new TrainingPlaceholderChart(QStringLiteral("准确率 / 核心指标"), chartPanel);
    trainingLrChart_ = new TrainingPlaceholderChart(QStringLiteral("学习率 (LR)"), chartPanel);
    chartRow->addWidget(trainingLossChart_, 1);
    chartRow->addWidget(trainingAccuracyChart_, 1);
    chartRow->addWidget(trainingLrChart_, 1);
    chartLayout->addLayout(chartRow, 1);

    auto* reportPanel = workbenchCard(left, QStringLiteral("TrainingCard"));
    auto* reportLayout = new QVBoxLayout(reportPanel);
    reportLayout->setContentsMargins(12, 10, 12, 10);
    auto* reportHeader = new QHBoxLayout();
    reportHeader->addWidget(sectionLabel(QStringLiteral("训练指标报表"), reportPanel));
    reportHeader->addStretch(1);
    auto* exportMetricsButton = new QPushButton(QStringLiteral("导出报表 CSV"), reportPanel);
    auto* clearMetricsButton = new QPushButton(QStringLiteral("清空报表"), reportPanel);
    connect(exportMetricsButton, &QPushButton::clicked, this, &MainWindow::exportTrainingMetricsCsv);
    connect(clearMetricsButton, &QPushButton::clicked, this, &MainWindow::clearTrainingMetrics);
    reportHeader->addWidget(exportMetricsButton);
    reportHeader->addWidget(clearMetricsButton);
    trainingMetricSummaryLabel_ = mutedLabel(QStringLiteral("指标：0 行 | 最新 score -- | 最新 loss --"), reportPanel);
    trainingMetricsTable_ = tableWidget(9, {
        QStringLiteral("时间"),
        QStringLiteral("epoch"),
        QStringLiteral("step"),
        QStringLiteral("loss"),
        QStringLiteral("lr"),
        QStringLiteral("acc"),
        QStringLiteral("score"),
        QStringLiteral("P/R"),
        QStringLiteral("原始日志"),
    }, reportPanel);
    trainingPreview_ = new QPlainTextEdit(reportPanel);
    trainingPreview_->setReadOnly(true);
    trainingPreview_->setMinimumHeight(130);
    reportLayout->addLayout(reportHeader);
    reportLayout->addWidget(trainingMetricSummaryLabel_);
    reportLayout->addWidget(trainingMetricsTable_, 1);
    reportLayout->addWidget(mutedLabel(QStringLiteral("训练日志"), reportPanel));
    reportLayout->addWidget(trainingPreview_);

    auto* detailTabs = new QTabWidget(left);
    detailTabs->setDocumentMode(true);
    auto* overviewTab = new QWidget(detailTabs);
    auto* overviewLayout = new QVBoxLayout(overviewTab);
    overviewLayout->setContentsMargins(0, 0, 0, 0);
    overviewLayout->setSpacing(10);
    overviewLayout->addWidget(versionPanel);
    overviewLayout->addWidget(selectedVersionPanel);
    overviewLayout->addWidget(progressPanel);
    overviewLayout->addWidget(chartPanel, 1);
    overviewLayout->addStretch(1);
    auto* reportTab = new QWidget(detailTabs);
    auto* reportTabLayout = new QVBoxLayout(reportTab);
    reportTabLayout->setContentsMargins(0, 0, 0, 0);
    reportTabLayout->addWidget(reportPanel, 1);
    detailTabs->addTab(overviewTab, QStringLiteral("概览"));
    detailTabs->addTab(reportTab, QStringLiteral("指标 / 日志"));
    leftLayout->addWidget(detailTabs, 1);
    leftScroll->setWidget(left);

    auto* rightScroll = new QScrollArea(detailScreen);
    rightScroll->setWidgetResizable(true);
    rightScroll->setFrameShape(QFrame::NoFrame);
    rightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rightScroll->setMinimumWidth(330);
    rightScroll->setMaximumWidth(500);
    auto* rightPanel = workbenchCard(rightScroll, QStringLiteral("InspectorPanel"));
    rightPanel->setMinimumWidth(310);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(10, 10, 10, 10);
    auto* configTabs = new QTabWidget(rightPanel);
    auto* configTab = new QWidget(configTabs);
    auto* configLayout = new QVBoxLayout(configTab);
    configLayout->addWidget(sectionLabel(QStringLiteral("训练配置"), configTab));
    auto* setupHint = new QLabel(QStringLiteral("推荐流程：检查数据集 → 预览命令 → 开始训练并生成版本。每次训练都会保留独立版本。"), configTab);
    setupHint->setObjectName("TrainingSetupHint");
    setupHint->setWordWrap(true);
    configLayout->addWidget(setupHint);
    auto* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(8);
    trainingTaskCombo_->setToolTip(QStringLiteral("选择要训练的模型模板和对应数据集类型。"));
    trainingDeviceCombo_->setToolTip(QStringLiteral("cpu 更稳但慢；gpu:0 速度更快，需要本机 Paddle GPU 环境可用。"));
    trainingEpochsSpin_->setToolTip(QStringLiteral("完整遍历训练集的轮数。小数据集可先用较小轮数做烟测。"));
    trainingBatchSpin_->setToolTip(QStringLiteral("每批训练样本数。显存不足时请降低这个值，例如 4 或 8。"));
    trainingLearningRateSpin_->setToolTip(QStringLiteral("学习率。除非你确定模型需要调参，一般使用任务默认值。"));
    form->addRow(QStringLiteral("模型模板"), trainingTaskCombo_);
    form->addRow(QStringLiteral("训练设备"), trainingDeviceCombo_);
    form->addRow(QStringLiteral("训练轮数"), trainingEpochsSpin_);
    form->addRow(QStringLiteral("批大小"), trainingBatchSpin_);
    form->addRow(QStringLiteral("学习率"), trainingLearningRateSpin_);
    trainingNumClassesSpin_ = new QSpinBox(configTab);
    auto* numClassesSpin = trainingNumClassesSpin_;
    numClassesSpin->setRange(0, 99999);
    trainingWarmupSpin_ = new QSpinBox(configTab);
    auto* warmupSpin = trainingWarmupSpin_;
    warmupSpin->setRange(0, 999999);
    numClassesSpin->setToolTip(QStringLiteral("分类任务的类别数。检测/识别任务通常保持 0，使用模型默认配置。"));
    warmupSpin->setToolTip(QStringLiteral("学习率预热步数。保持默认即可。"));
    form->addRow(QStringLiteral("类别数"), numClassesSpin);
    form->addRow(QStringLiteral("预热步数"), warmupSpin);
    configLayout->addLayout(form);
    auto* resumeRow = new QHBoxLayout();
    trainingResumePathEdit_ = new QLineEdit(configTab);
    auto* resumeEdit = trainingResumePathEdit_;
    auto* resumeBrowse = new QPushButton(QStringLiteral("浏览"), configTab);
    connect(resumeBrowse, &QPushButton::clicked, this, &MainWindow::browseTrainingResumePath);
    resumeRow->addWidget(resumeEdit, 1);
    resumeRow->addWidget(resumeBrowse);
    configLayout->addWidget(sectionLabel(QStringLiteral("高级参数"), configTab));
    configLayout->addWidget(mutedLabel(QStringLiteral("resume_path（断点续训权重）"), configTab));
    configLayout->addLayout(resumeRow);
    auto* commandPreviewButton = new QPushButton(QStringLiteral("预览命令"), configTab);
    auto* checkDatasetButton = new QPushButton(QStringLiteral("检查数据集"), configTab);
    auto* copyCommandButton = new QPushButton(QStringLiteral("复制命令"), configTab);
    commandPreviewButton->setToolTip(QStringLiteral("查看即将执行的 PaddleX 命令、数据集和输出目录。"));
    checkDatasetButton->setToolTip(QStringLiteral("训练前检查数据集、配置、预训练权重和环境路径。"));
    copyCommandButton->setToolTip(QStringLiteral("复制当前训练命令，方便在终端复现。"));
    connect(commandPreviewButton, &QPushButton::clicked, this, &MainWindow::previewTrainingCommand);
    connect(checkDatasetButton, &QPushButton::clicked, this, &MainWindow::checkTrainingSetup);
    connect(copyCommandButton, &QPushButton::clicked, this, &MainWindow::copyTrainingCommand);
    auto* commandRow = new QHBoxLayout();
    commandRow->addWidget(commandPreviewButton);
    commandRow->addWidget(checkDatasetButton);
    commandRow->addWidget(copyCommandButton);
    configLayout->addLayout(commandRow);
    startTrainingButton_ = primaryButton(QStringLiteral("开始训练并生成版本"), configTab);
    startTrainingButton_->setToolTip(QStringLiteral("启动训练，并在当前项目下创建一个独立训练版本。"));
    stopTrainingButton_ = dangerButton(QStringLiteral("停止"), configTab);
    connect(startTrainingButton_, &QPushButton::clicked, this, &MainWindow::startTraining);
    connect(stopTrainingButton_, &QPushButton::clicked, this, &MainWindow::stopTraining);
    auto* runRow = new QHBoxLayout();
    runRow->addWidget(startTrainingButton_, 1);
    runRow->addWidget(stopTrainingButton_);
    configLayout->addLayout(runRow);
    configLayout->addStretch(1);

    auto* exportTab = new QWidget(configTabs);
    auto* exportLayout = new QVBoxLayout(exportTab);
    exportLayout->addWidget(sectionLabel(QStringLiteral("模型导出"), exportTab));
    auto* exportWeightRow = new QHBoxLayout();
    trainingExportWeightEdit_ = new QLineEdit(exportTab);
    trainingExportWeightEdit_->setPlaceholderText(QStringLiteral("自动使用选中版本的 best 权重"));
    exportWeightRow->addWidget(trainingExportWeightEdit_, 1);
    auto* exportWeightBrowse = new QPushButton(QStringLiteral("浏览"), exportTab);
    connect(exportWeightBrowse, &QPushButton::clicked, this, &MainWindow::browseTrainingExportWeight);
    exportWeightRow->addWidget(exportWeightBrowse);
    auto* exportDirRow = new QHBoxLayout();
    trainingExportDirEdit_ = new QLineEdit(exportTab);
    trainingExportDirEdit_->setPlaceholderText(QStringLiteral("自动导出到 project/model_exports/task/version"));
    exportDirRow->addWidget(trainingExportDirEdit_, 1);
    auto* exportDirBrowse = new QPushButton(QStringLiteral("浏览"), exportTab);
    connect(exportDirBrowse, &QPushButton::clicked, this, &MainWindow::browseTrainingExportDir);
    exportDirRow->addWidget(exportDirBrowse);
    exportLayout->addWidget(mutedLabel(QStringLiteral("导出权重"), exportTab));
    exportLayout->addLayout(exportWeightRow);
    exportLayout->addWidget(mutedLabel(QStringLiteral("导出目录"), exportTab));
    exportLayout->addLayout(exportDirRow);
    auto* exportModelButton = primaryButton(QStringLiteral("导出选中版本模型"), exportTab);
    connect(exportModelButton, &QPushButton::clicked, this, &MainWindow::exportSelectedTrainingModel);
    exportLayout->addWidget(exportModelButton);
    exportLayout->addStretch(1);
    configTabs->addTab(configTab, QStringLiteral("训练配置"));
    configTabs->addTab(exportTab, QStringLiteral("模型导出"));
    trainingVersionDetail_ = new QPlainTextEdit(rightPanel);
    trainingVersionDetail_->setReadOnly(true);
    trainingVersionDetail_->setMaximumHeight(150);
    trainingVersionDetail_->setPlainText(QStringLiteral("未选择版本"));
    rightLayout->addWidget(configTabs, 1);
    rightLayout->addWidget(sectionLabel(QStringLiteral("版本对比"), rightPanel));
    rightLayout->addWidget(trainingVersionDetail_);
    rightScroll->setWidget(rightPanel);

    detailLayout->addWidget(leftScroll, 1);
    detailLayout->addWidget(rightScroll);

    auto* versionScreen = new QWidget(screens);
    auto* versionScreenLayout = new QVBoxLayout(versionScreen);
    versionScreenLayout->setContentsMargins(0, 0, 0, 0);
    auto* versionHeader = new QHBoxLayout();
    versionHeader->addWidget(sectionLabel(QStringLiteral("模型版本"), versionScreen));
    versionHeader->addStretch(1);
    auto* refreshVersionsButton = new QPushButton(QStringLiteral("刷新"), versionScreen);
    auto* setVersionButton = primaryButton(QStringLiteral("设为当前"), versionScreen);
    auto* openDirButton = new QPushButton(QStringLiteral("打开目录"), versionScreen);
    auto* deleteButton = dangerButton(QStringLiteral("删除版本"), versionScreen);
    connect(refreshVersionsButton, &QPushButton::clicked, this, &MainWindow::refreshTrainingVersions);
    connect(setVersionButton, &QPushButton::clicked, this, &MainWindow::setSelectedTrainingVersionCurrent);
    connect(openDirButton, &QPushButton::clicked, this, &MainWindow::openSelectedTrainingVersionDir);
    connect(deleteButton, &QPushButton::clicked, this, &MainWindow::deleteSelectedTrainingVersion);
    versionHeader->addWidget(refreshVersionsButton);
    versionHeader->addWidget(setVersionButton);
    versionHeader->addWidget(openDirButton);
    versionHeader->addWidget(deleteButton);
    auto* versionInfo = mutedLabel(QStringLiteral("每次训练都会生成独立版本目录；当前版本用于默认导出和预测，best 版本由主要指标自动维护。"), versionScreen);
    versionInfo->setWordWrap(true);
    trainingVersionManagerTable_ = tableWidget(10, {
        QStringLiteral("模型模板"),
        QStringLiteral("版本"),
        QStringLiteral("状态"),
        QStringLiteral("当前"),
        QStringLiteral("Best"),
        QStringLiteral("开始时间"),
        QStringLiteral("结束时间"),
        QStringLiteral("主要指标"),
        QStringLiteral("样本数"),
        QStringLiteral("目录"),
    }, versionScreen);
    connect(trainingVersionManagerTable_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (!trainingVersionManagerTable_) {
            return;
        }
        const auto* taskItem = trainingVersionManagerTable_->item(row, 0);
        if (!taskItem) {
            return;
        }
        selectTrainingTaskAndShowDetail(taskItem->data(Qt::UserRole).toString());
    });
    versionScreenLayout->addLayout(versionHeader);
    versionScreenLayout->addWidget(versionInfo);
    versionScreenLayout->addWidget(trainingVersionManagerTable_, 1);

    screens->addWidget(listScreen);
    screens->addWidget(detailScreen);
    screens->addWidget(versionScreen);
    connect(screens, &QStackedWidget::currentChanged, this, [this](int index) {
        if (index == 0) {
            queueTrainingTaskOverviewRefresh();
        }
    });
    layout->addWidget(header);
    layout->addWidget(screens, 1);
    applyTrainingTaskDefaults();
    setTrainingRunning(false);
    refreshTrainingVersions();
    return page;
}

QWidget* MainWindow::buildPredictionPage() {
    auto* page = new QWidget(this);
    page->setObjectName("PredictionRoot");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(10);

    predictInputEdit_ = new QLineEdit(page);
    predictOutputEdit_ = new QLineEdit(page);
    predictOutputEdit_->setText(defaultPredictionOutputDir());
    predictDetModelCombo_ = new QComboBox(page);
    predictRecModelCombo_ = new QComboBox(page);
    predictClsTaskCombo_ = new QComboBox(page);
    predictClsTaskCombo_->addItem(QStringLiteral("文档方向"), "doc_orientation");
    predictClsTaskCombo_->addItem(QStringLiteral("文本行方向"), "textline_orientation");
    predictClsTaskCombo_->addItem(QStringLiteral("表格分类"), "table_classification");
    predictClsModelCombo_ = new QComboBox(page);
    predictLayoutModelCombo_ = new QComboBox(page);
    predictDeviceCombo_ = new QComboBox(page);
    predictDeviceCombo_->setEditable(true);
    predictDeviceCombo_->addItems({"cpu", "gpu:0"});
    predictScoreThresholdSpin_ = new QDoubleSpinBox(page);
    predictScoreThresholdSpin_->setRange(0.0, 1.0);
    predictScoreThresholdSpin_->setDecimals(2);
    predictScoreThresholdSpin_->setSingleStep(0.05);
    predictScoreThresholdSpin_->setValue(0.50);
    predictSaveJsonCheck_ = new QCheckBox(QStringLiteral("保存 JSON"), page);
    predictSaveJsonCheck_->setChecked(true);
    predictSaveVisualCheck_ = new QCheckBox(QStringLiteral("保存可视化"), page);
    predictSaveVisualCheck_->setChecked(true);

    auto* header = workbenchCard(page, QStringLiteral("HeaderPanel"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 10, 14, 10);
    auto* titleBox = new QVBoxLayout();
    titleBox->setSpacing(3);
    auto* title = new QLabel(QStringLiteral("预测工作台"), header);
    title->setObjectName("PredictionTitle");
    titleBox->addWidget(title);
    titleBox->addWidget(mutedLabel(QStringLiteral("预测 / Det 检测 | PP-OCRv5 server"), header));
    auto* projectText = mutedLabel(QStringLiteral("当前项目：未打开"), header);
    auto* statusText = new QLabel(QStringLiteral("状态：未启动"), header);
    statusText->setObjectName("StatusOk");
    headerLayout->addLayout(titleBox, 1);
    headerLayout->addWidget(projectText, 1);
    headerLayout->addWidget(statusText);

    auto* modelStrip = workbenchCard(page, QStringLiteral("WorkbenchPanel"));
    modelStrip->setMaximumHeight(172);
    auto* stripLayout = new QVBoxLayout(modelStrip);
    stripLayout->setContentsMargins(12, 9, 12, 9);
    stripLayout->setSpacing(8);
    auto* stripHeader = new QHBoxLayout();
    stripHeader->addWidget(sectionLabel(QStringLiteral("已完成模型选择"), modelStrip));
    stripHeader->addWidget(mutedLabel(QStringLiteral("仅展示训练完成且可用于推理的模型"), modelStrip));
    stripHeader->addStretch(1);
    predictionModelSearchEdit_ = new QLineEdit(modelStrip);
    auto* modelSearch = predictionModelSearchEdit_;
    modelSearch->setPlaceholderText(QStringLiteral("搜索模型名称"));
    predictionModelKindFilter_ = new QComboBox(modelStrip);
    auto* modelType = predictionModelKindFilter_;
    modelType->addItems({QStringLiteral("模型类型：全部"), QStringLiteral("Det"), QStringLiteral("Rec"), QStringLiteral("Cls"), QStringLiteral("Layout")});
    auto* refreshModelStripButton = new QPushButton(QStringLiteral("刷新"), modelStrip);
    connect(refreshModelStripButton, &QPushButton::clicked, this, &MainWindow::refreshPredictionModels);
    for (int i = 0; i < predictionModelKindFilter_->count(); ++i) {
        const QString text = predictionModelKindFilter_->itemText(i).toLower();
        if (text.contains(QStringLiteral("det"))) {
            predictionModelKindFilter_->setItemData(i, QStringLiteral("det"));
        } else if (text.contains(QStringLiteral("rec"))) {
            predictionModelKindFilter_->setItemData(i, QStringLiteral("rec"));
        } else if (text.contains(QStringLiteral("cls"))) {
            predictionModelKindFilter_->setItemData(i, QStringLiteral("cls"));
        } else if (text.contains(QStringLiteral("layout"))) {
            predictionModelKindFilter_->setItemData(i, QStringLiteral("layout"));
        } else {
            predictionModelKindFilter_->setItemData(i, QString());
        }
    }
    connect(predictionModelSearchEdit_, &QLineEdit::textChanged, this, &MainWindow::refreshPredictionModelCards);
    connect(predictionModelKindFilter_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::refreshPredictionModelCards);
    stripHeader->addWidget(predictionModelSearchEdit_);
    stripHeader->addWidget(predictionModelKindFilter_);
    stripHeader->addWidget(refreshModelStripButton);

    predictionModelHost_ = new QWidget(modelStrip);
    predictionModelHost_->setObjectName("PredictionModelHost");
    predictionModelLayout_ = new QHBoxLayout(predictionModelHost_);
    predictionModelLayout_->setContentsMargins(0, 0, 0, 0);
    predictionModelLayout_->setSpacing(8);
    auto* modelRow = predictionModelLayout_;
    auto addModelTile = [modelStrip, modelRow](const QString& name, const QString& badge, const QString& dataset, const QString& path) {
        auto* tile = workbenchCard(modelStrip, QStringLiteral("PredictionModelTile"));
        tile->setMinimumWidth(220);
        auto* tileLayout = new QVBoxLayout(tile);
        tileLayout->setContentsMargins(12, 10, 12, 10);
        auto* top = new QHBoxLayout();
        auto* nameLabel = new QLabel(name, tile);
        nameLabel->setObjectName("TrainingTaskTitle");
        auto* badgeLabel = new QLabel(badge, tile);
        badgeLabel->setObjectName("TrainingBadge");
        top->addWidget(nameLabel, 1);
        top->addWidget(badgeLabel);
        tileLayout->addLayout(top);
        tileLayout->addWidget(mutedLabel(QStringLiteral("数据集：%1").arg(dataset), tile));
        tileLayout->addWidget(mutedLabel(QStringLiteral("模型目录：%1").arg(path), tile));
        auto* status = new QLabel(QStringLiteral("已完成    inference"), tile);
        status->setObjectName("TrainingStatusDone");
        tileLayout->addWidget(status);
        modelRow->addWidget(tile);
    };
    addModelTile(QStringLiteral("rec_v5_server"), QStringLiteral("Rec"), QStringLiteral("ppocr_rec"), QStringLiteral("training\\rec_v5_server"));
    addModelTile(QStringLiteral("det_v5_server"), QStringLiteral("Det"), QStringLiteral("ppocr_det"), QStringLiteral("training\\det_v5_server"));
    modelRow->addStretch(1);
    auto* modelScroll = new QScrollArea(modelStrip);
    modelScroll->setObjectName("PredictionModelScroll");
    modelScroll->setWidgetResizable(false);
    modelScroll->setFrameShape(QFrame::NoFrame);
    modelScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    modelScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    modelScroll->setMinimumHeight(108);
    modelScroll->setWidget(predictionModelHost_);
    stripLayout->addLayout(stripHeader);
    stripLayout->addWidget(modelScroll);

    auto* inputRow = new QHBoxLayout();
    auto* imageButton = new QPushButton(QStringLiteral("选择图片"), page);
    auto* dirButton = new QPushButton(QStringLiteral("选择目录"), page);
    auto* pasteButton = new QPushButton(QStringLiteral("粘贴图片"), page);
    auto* clearInputButton = new QPushButton(QStringLiteral("清空"), page);
    connect(imageButton, &QPushButton::clicked, this, &MainWindow::browsePredictInputFile);
    connect(dirButton, &QPushButton::clicked, this, &MainWindow::browsePredictInputDir);
    connect(pasteButton, &QPushButton::clicked, this, &MainWindow::pastePredictImageFromClipboard);
    connect(clearInputButton, &QPushButton::clicked, predictInputEdit_, &QLineEdit::clear);
    connect(clearInputButton, &QPushButton::clicked, this, [this]() { updatePredictionImagePreview(QString()); });
    connect(predictInputEdit_, &QLineEdit::editingFinished, this, [this]() {
        updatePredictionImagePreview(predictInputEdit_ ? predictInputEdit_->text().trimmed() : QString());
    });
    inputRow->addWidget(imageButton);
    inputRow->addWidget(dirButton);
    inputRow->addWidget(pasteButton);
    inputRow->addWidget(clearInputButton);
    predictInputEdit_->setPlaceholderText(QStringLiteral("选择图片文件或图片目录"));
    inputRow->addWidget(predictInputEdit_, 1);

    auto* outputRow = new QHBoxLayout();
    auto* outputButton = new QPushButton(QStringLiteral("浏览"), page);
    auto* openOutputButton = new QPushButton(QStringLiteral("打开"), page);
    connect(outputButton, &QPushButton::clicked, this, &MainWindow::browsePredictOutputDir);
    connect(openOutputButton, &QPushButton::clicked, this, &MainWindow::openPredictOutputDir);
    outputRow->addWidget(predictOutputEdit_, 1);
    outputRow->addWidget(outputButton);
    outputRow->addWidget(openOutputButton);

    auto* previewCard = workbenchCard(page, QStringLiteral("WorkbenchPanel"));
    auto* previewLayout = new QVBoxLayout(previewCard);
    previewLayout->setContentsMargins(12, 10, 12, 12);
    auto* previewHeader = new QHBoxLayout();
    previewHeader->addWidget(sectionLabel(QStringLiteral("图片预览与结果"), previewCard));
    previewHeader->addStretch(1);
    previewHeader->addWidget(new QLabel(QStringLiteral("缩放"), previewCard));
    previewHeader->addWidget(new QLabel(QStringLiteral("100%"), previewCard));
    predictionImagePathLabel_ = mutedLabel(QStringLiteral("未选择测试图片"), previewCard);
    predictionImagePathLabel_->setWordWrap(true);
    predictionImagePreview_ = new QLabel(QStringLiteral("未选择测试图片"), previewCard);
    predictionImagePreview_->setObjectName("ImagePreview");
    predictionImagePreview_->setAlignment(Qt::AlignCenter);
    predictionImagePreview_->setMinimumSize(360, 240);
    auto* previewFrame = workbenchCard(previewCard, QStringLiteral("PredictionPreviewFrame"));
    auto* previewFrameLayout = new QVBoxLayout(previewFrame);
    previewFrameLayout->setContentsMargins(0, 0, 0, 0);
    previewFrameLayout->addWidget(predictionImagePreview_, 1);

    predictionResultTable_ = tableWidget(4, {
        QStringLiteral("序号"),
        QStringLiteral("文本"),
        QStringLiteral("置信度"),
        QStringLiteral("坐标"),
    }, previewCard);
    predictionResultTable_->setHorizontalHeaderLabels({
        QStringLiteral("Kind"),
        QStringLiteral("Label"),
        QStringLiteral("Score"),
        QStringLiteral("Source"),
    });
    predictionPreview_ = new QPlainTextEdit(previewCard);
    predictionPreview_->setReadOnly(true);
    auto* structuredText = new QPlainTextEdit(QStringLiteral("结构化结果"), previewCard);
    structuredText->setReadOnly(true);
    predictionStructuredText_ = structuredText;
    auto* resultTabs = new QTabWidget(previewCard);
    resultTabs->addTab(predictionResultTable_, QStringLiteral("推理输出摘要"));
    resultTabs->addTab(structuredText, QStringLiteral("结构化结果"));
    resultTabs->addTab(predictionPreview_, QStringLiteral("运行日志"));

    predictionResultTotalLabel_ = new QLabel(QStringLiteral("结果行：0"), page);
    predictionResultConfidenceLabel_ = new QLabel(QStringLiteral("平均分：--"), page);
    predictionElapsedLabel_ = new QLabel(QStringLiteral("耗时：--"), page);
    predictionResultTotalLabel_->setObjectName("Muted");
    predictionResultConfidenceLabel_->setObjectName("Muted");
    predictionElapsedLabel_->setObjectName("Muted");
    auto* stats = new QHBoxLayout();
    stats->addWidget(predictionResultTotalLabel_);
    stats->addWidget(predictionResultConfidenceLabel_);
    stats->addWidget(predictionElapsedLabel_);
    stats->addStretch(1);
    previewLayout->addLayout(previewHeader);
    previewLayout->addLayout(inputRow);
    previewLayout->addWidget(predictionImagePathLabel_);
    previewLayout->addWidget(previewFrame, 1);
    previewLayout->addWidget(resultTabs, 1);
    previewLayout->addLayout(stats);

    auto* configPanel = workbenchCard(page, QStringLiteral("InspectorPanel"));
    configPanel->setMinimumWidth(430);
    configPanel->setMaximumWidth(620);
    auto* configLayout = new QVBoxLayout(configPanel);
    configLayout->setContentsMargins(10, 10, 10, 10);
    configLayout->addWidget(sectionLabel(QStringLiteral("预测配置"), configPanel));
    auto* activeModel = workbenchCard(configPanel, QStringLiteral("PredictionModelTile"));
    auto* activeModelLayout = new QHBoxLayout(activeModel);
    auto* modelIcon = new QLabel(QStringLiteral("≡"), activeModel);
    modelIcon->setObjectName("OverviewMetricIcon");
    modelIcon->setStyleSheet(QStringLiteral("background:#14786d;"));
    auto* modelText = new QVBoxLayout();
    predictionActiveModelTitleLabel_ = new QLabel(QStringLiteral("Det / Rec / Cls / Layout"), activeModel);
    predictionActiveModelTitleLabel_->setObjectName("TrainingTaskTitle");
    predictionActiveModelPathLabel_ = mutedLabel(QStringLiteral("model path: -"), activeModel);
    predictionActiveModelPathLabel_->setWordWrap(true);
    modelText->addWidget(predictionActiveModelTitleLabel_);
    modelText->addWidget(predictionActiveModelPathLabel_);
    modelText->addWidget(mutedLabel(QStringLiteral("模型路径：training\\det_v5_server\\inference"), activeModel));
    activeModelLayout->addWidget(modelIcon);
    if (modelText->count() > 2) {
        if (auto* legacyPathLabel = modelText->itemAt(2)->widget()) {
            legacyPathLabel->setVisible(false);
        }
    }
    activeModelLayout->addLayout(modelText, 1);
    configLayout->addWidget(activeModel);

    predictModeCombo_ = new QComboBox(configPanel);
    predictModeCombo_->addItem(QStringLiteral("OCR 文字识别"), QStringLiteral("ocr"));
    predictModeCombo_->addItem(QStringLiteral("分类预测"), QStringLiteral("classification"));
    predictModeCombo_->addItem(QStringLiteral("版面分析"), QStringLiteral("layout"));
    connect(predictModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updatePredictionModeButtons);
    predictBatchSpin_ = new QSpinBox(configPanel);
    auto* batchSpin = predictBatchSpin_;
    batchSpin->setRange(1, 4096);
    batchSpin->setValue(1);
    predictRunModeCombo_ = new QComboBox(configPanel);
    auto* runModeCombo = predictRunModeCombo_;
    runModeCombo->addItem(QStringLiteral("paddle"));
    const QString predictorLimitTip = QStringLiteral("Current built-in C++ predictors process the selected file or directory directly and do not consume PaddleX batch/run_mode/extra args yet.");
    batchSpin->setEnabled(false);
    batchSpin->setToolTip(predictorLimitTip);
    runModeCombo->setEnabled(false);
    runModeCombo->setToolTip(predictorLimitTip);
    batchSpin->hide();
    runModeCombo->hide();
    auto* configForm = new QFormLayout();
    configForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    configForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
    configForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    configForm->setHorizontalSpacing(10);
    configForm->setVerticalSpacing(8);
    configForm->addRow(QStringLiteral("预测模式"), predictModeCombo_);
    configForm->addRow(QStringLiteral("Det 模型"), predictDetModelCombo_);
    configForm->addRow(QStringLiteral("Rec 模型"), predictRecModelCombo_);
    configForm->addRow(QStringLiteral("分类任务"), predictClsTaskCombo_);
    configForm->addRow(QStringLiteral("分类模型"), predictClsModelCombo_);
    configForm->addRow(QStringLiteral("版面模型"), predictLayoutModelCombo_);
    configForm->addRow(QStringLiteral("推理设备"), predictDeviceCombo_);
    configForm->addRow(QStringLiteral("Score 阈值"), predictScoreThresholdSpin_);
    configForm->addRow(QStringLiteral("推理输出"), outputRow);
    configLayout->addLayout(configForm);
    auto setFormRowVisible = [configForm](QWidget* field, bool visible) {
        if (!field) {
            return;
        }
        field->setVisible(visible);
        if (auto* label = configForm->labelForField(field)) {
            label->setVisible(visible);
        }
    };
    auto applyPredictionModeForm = [this, setFormRowVisible]() {
        const QString mode = predictModeCombo_ ? predictModeCombo_->currentData().toString() : QStringLiteral("ocr");
        const bool ocrMode = mode == QStringLiteral("ocr");
        const bool classificationMode = mode == QStringLiteral("classification");
        const bool layoutMode = mode == QStringLiteral("layout");
        setFormRowVisible(predictDetModelCombo_, ocrMode);
        setFormRowVisible(predictRecModelCombo_, ocrMode);
        setFormRowVisible(predictClsTaskCombo_, classificationMode);
        setFormRowVisible(predictClsModelCombo_, classificationMode);
        setFormRowVisible(predictLayoutModelCombo_, layoutMode);
    };
    connect(predictModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, applyPredictionModeForm);
    applyPredictionModeForm();
    auto* outputOptions = new QHBoxLayout();
    outputOptions->addWidget(predictSaveVisualCheck_);
    outputOptions->addWidget(predictSaveJsonCheck_);
    outputOptions->addStretch(1);
    configLayout->addLayout(outputOptions);
    auto* extraHintLabel = mutedLabel(QStringLiteral("图像长边（可选）"), configPanel);
    extraHintLabel->hide();
    configLayout->addWidget(extraHintLabel);
    predictExtraOptionsEdit_ = new QPlainTextEdit(QStringLiteral("use_dilation true"), configPanel);
    auto* extraEdit = predictExtraOptionsEdit_;
    extraEdit->setMinimumHeight(120);
    extraEdit->setPlainText(predictorLimitTip);
    extraEdit->setReadOnly(true);
    extraEdit->setEnabled(false);
    extraEdit->setToolTip(predictorLimitTip);
    extraEdit->hide();
    configLayout->addWidget(extraEdit);

    auto* smallActions = new QGridLayout();
    auto* refreshModelsButton = new QPushButton(QStringLiteral("刷新模型"), configPanel);
    auto* modelLibraryButton = new QPushButton(QStringLiteral("模型库"), configPanel);
    auto* viewResultButton = new QPushButton(QStringLiteral("查看结果"), configPanel);
    auto* smokeButton = new QPushButton(QStringLiteral("Smoke 测试"), configPanel);
    connect(refreshModelsButton, &QPushButton::clicked, this, &MainWindow::refreshPredictionModels);
    connect(modelLibraryButton, &QPushButton::clicked, this, [this]() { stack_->setCurrentIndex(5); });
    connect(viewResultButton, &QPushButton::clicked, this, &MainWindow::openPredictOutputDir);
    connect(smokeButton, &QPushButton::clicked, this, &MainWindow::runInferenceSmoke);
    smallActions->addWidget(refreshModelsButton, 0, 0);
    smallActions->addWidget(modelLibraryButton, 0, 1);
    smallActions->addWidget(viewResultButton, 1, 0);
    smallActions->addWidget(smokeButton, 1, 1);
    configLayout->addLayout(smallActions);

    startPredictionButton_ = primaryButton(QStringLiteral("▷ 开始预测"), configPanel);
    startClsPredictionButton_ = primaryButton(QStringLiteral("开始分类预测"), configPanel);
    startLayoutPredictionButton_ = primaryButton(QStringLiteral("开始版面预测"), configPanel);
    stopPredictionButton_ = dangerButton(QStringLiteral("停止"), configPanel);
    startClsPredictionButton_->setVisible(false);
    startLayoutPredictionButton_->setVisible(false);
    connect(startPredictionButton_, &QPushButton::clicked, this, &MainWindow::startSelectedPredictionMode);
    connect(startClsPredictionButton_, &QPushButton::clicked, this, &MainWindow::startClassificationPrediction);
    connect(startLayoutPredictionButton_, &QPushButton::clicked, this, &MainWindow::startLayoutPrediction);
    connect(stopPredictionButton_, &QPushButton::clicked, this, &MainWindow::stopPrediction);
    connect(predictClsTaskCombo_, &QComboBox::currentTextChanged, this, &MainWindow::refreshPredictionModels);
    connect(predictDetModelCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updatePredictionActiveModelSummary);
    connect(predictRecModelCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updatePredictionActiveModelSummary);
    connect(predictClsModelCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updatePredictionActiveModelSummary);
    connect(predictLayoutModelCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updatePredictionActiveModelSummary);
    predictionStatusLabel_ = mutedLabel(QStringLiteral("空闲"), configPanel);
    predictionStatusLabel_->setObjectName("TrainingStatusIdle");
    configLayout->addWidget(predictionStatusLabel_);
    configLayout->addWidget(stopPredictionButton_);
    configLayout->addWidget(startPredictionButton_);
    configLayout->addWidget(startClsPredictionButton_);
    configLayout->addWidget(startLayoutPredictionButton_);
    configLayout->addStretch(1);

    auto* body = new QSplitter(Qt::Horizontal, page);
    body->addWidget(previewCard);
    body->addWidget(configPanel);
    body->setStretchFactor(0, 1);
    body->setSizes({900, 450});
    layout->addWidget(header);
    modelStrip->hide();
    layout->addWidget(body, 1);
    refreshPredictionModels();
    setPredictionRunning(false);
    updatePredictionModeButtons();
    return page;
}

QWidget* MainWindow::buildDatasetPage() {
    auto* page = new QWidget(this);
    page->setObjectName("DatasetRoot");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(10);

    auto* header = workbenchCard(page, QStringLiteral("HeaderPanel"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 10, 14, 10);
    headerLayout->setSpacing(10);
    auto* titleBox = new QVBoxLayout();
    titleBox->setSpacing(3);
    auto* title = new QLabel(QStringLiteral("数据集"), header);
    title->setObjectName("OverviewTitle");
    titleBox->addWidget(title);
    titleBox->addWidget(mutedLabel(QStringLiteral("导入 → 标注 → 校验 → 导出 → 训练"), header));
    headerLayout->addLayout(titleBox, 1);
    auto* importImagesButton = primaryButton(QStringLiteral("导入图片"), header);
    auto* importPdfsButton = new QPushButton(QStringLiteral("导入 PDF"), header);
    auto* validateButton = new QPushButton(QStringLiteral("校验"), header);
    auto* exportButton = new QPushButton(QStringLiteral("导出"), header);
    auto* labelButton = new QPushButton(QStringLiteral("去标注"), header);
    connect(importImagesButton, &QPushButton::clicked, this, &MainWindow::importImagesDialog);
    connect(importPdfsButton, &QPushButton::clicked, this, &MainWindow::importPdfsDialog);
    connect(validateButton, &QPushButton::clicked, this, &MainWindow::validateProject);
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportProject);
    connect(labelButton, &QPushButton::clicked, this, [this]() { stack_->setCurrentIndex(1); });
    headerLayout->addWidget(importImagesButton);
    headerLayout->addWidget(importPdfsButton);
    headerLayout->addWidget(validateButton);
    headerLayout->addWidget(exportButton);
    headerLayout->addWidget(labelButton);

    auto* summaryPanel = workbenchCard(page, QStringLiteral("WorkbenchPanel"));
    auto* summaryLayout = new QHBoxLayout(summaryPanel);
    summaryLayout->setContentsMargins(12, 10, 12, 10);
    datasetSummaryLabel_ = new QLabel(QStringLiteral("未打开项目"), summaryPanel);
    datasetSummaryLabel_->setObjectName("SectionTitle");
    datasetSummaryLabel_->setWordWrap(true);
    datasetNextStepLabel_ = new QLabel(QStringLiteral("下一步：打开或新建项目"), summaryPanel);
    datasetNextStepLabel_->setObjectName("StatusWarn");
    datasetNextStepLabel_->setWordWrap(true);
    summaryLayout->addWidget(datasetSummaryLabel_, 1);
    summaryLayout->addWidget(datasetNextStepLabel_);

    auto* tables = new QSplitter(Qt::Vertical, page);
    auto* overviewPanel = workbenchCard(tables, QStringLiteral("WorkbenchPanel"));
    auto* overviewLayout = new QVBoxLayout(overviewPanel);
    overviewLayout->setContentsMargins(12, 10, 12, 12);
    overviewLayout->addWidget(sectionLabel(QStringLiteral("任务闭环"), overviewPanel));
    datasetOverviewTable_ = tableWidget(4, {
        QStringLiteral("环节"),
        QStringLiteral("状态"),
        QStringLiteral("数量 / 路径"),
        QStringLiteral("下一步"),
    }, overviewPanel);
    datasetOverviewTable_->setMinimumHeight(210);
    overviewLayout->addWidget(datasetOverviewTable_, 1);

    auto* pathPanel = workbenchCard(tables, QStringLiteral("WorkbenchPanel"));
    auto* pathLayout = new QVBoxLayout(pathPanel);
    pathLayout->setContentsMargins(12, 10, 12, 12);
    pathLayout->addWidget(sectionLabel(QStringLiteral("项目目录"), pathPanel));
    datasetPathTable_ = tableWidget(3, {
        QStringLiteral("目录"),
        QStringLiteral("状态"),
        QStringLiteral("路径"),
    }, pathPanel);
    datasetPathTable_->setMinimumHeight(180);
    pathLayout->addWidget(datasetPathTable_, 1);

    tables->addWidget(overviewPanel);
    tables->addWidget(pathPanel);
    tables->setSizes({320, 260});
    layout->addWidget(header);
    layout->addWidget(summaryPanel);
    layout->addWidget(tables, 1);
    refreshDatasetPage();
    return page;
}

QWidget* MainWindow::buildModelLibraryPage() {
    auto* page = new QWidget(this);
    page->setObjectName("ModelLibraryRoot");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(10);

    auto* header = workbenchCard(page, QStringLiteral("HeaderPanel"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 10, 14, 10);
    headerLayout->setSpacing(10);
    auto* titleBox = new QVBoxLayout();
    titleBox->setSpacing(3);
    auto* title = new QLabel(QStringLiteral("模型库"), header);
    title->setObjectName("OverviewTitle");
    titleBox->addWidget(title);
    titleBox->addWidget(mutedLabel(QStringLiteral("版本、指标、当前模型、最佳模型"), header));
    headerLayout->addLayout(titleBox, 1);
    auto* refreshButton = new QPushButton(QStringLiteral("刷新"), header);
    auto* trainButton = primaryButton(QStringLiteral("去训练"), header);
    auto* predictButton = new QPushButton(QStringLiteral("去预测"), header);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshModelLibraryPage);
    connect(trainButton, &QPushButton::clicked, this, [this]() { stack_->setCurrentIndex(2); });
    connect(predictButton, &QPushButton::clicked, this, [this]() { stack_->setCurrentIndex(3); });
    headerLayout->addWidget(refreshButton);
    headerLayout->addWidget(trainButton);
    headerLayout->addWidget(predictButton);

    auto* summaryPanel = workbenchCard(page, QStringLiteral("WorkbenchPanel"));
    auto* summaryLayout = new QHBoxLayout(summaryPanel);
    summaryLayout->setContentsMargins(12, 10, 12, 10);
    modelLibrarySummaryLabel_ = new QLabel(QStringLiteral("未打开项目"), summaryPanel);
    modelLibrarySummaryLabel_->setObjectName("SectionTitle");
    modelLibrarySummaryLabel_->setWordWrap(true);
    summaryLayout->addWidget(modelLibrarySummaryLabel_, 1);

    auto* tablePanel = workbenchCard(page, QStringLiteral("WorkbenchPanel"));
    auto* tableLayout = new QVBoxLayout(tablePanel);
    tableLayout->setContentsMargins(12, 10, 12, 12);
    tableLayout->addWidget(sectionLabel(QStringLiteral("训练版本"), tablePanel));
    modelLibraryTable_ = tableWidget(8, {
        QStringLiteral("任务"),
        QStringLiteral("版本"),
        QStringLiteral("状态"),
        QStringLiteral("主指标"),
        QStringLiteral("当前"),
        QStringLiteral("最佳"),
        QStringLiteral("模型目录"),
        QStringLiteral("完成时间"),
    }, tablePanel);
    modelLibraryTable_->setMinimumHeight(360);
    modelLibraryTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(modelLibraryTable_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        const auto* item = modelLibraryTable_ ? modelLibraryTable_->item(row, 0) : nullptr;
        const QString taskKey = item ? item->data(Qt::UserRole).toString() : QString();
        if (!taskKey.isEmpty()) {
            selectTrainingTaskAndShowDetail(taskKey);
            stack_->setCurrentIndex(2);
        }
    });
    tableLayout->addWidget(modelLibraryTable_, 1);

    layout->addWidget(header);
    layout->addWidget(summaryPanel);
    layout->addWidget(tablePanel, 1);
    refreshModelLibraryPage();
    return page;
}

QWidget* MainWindow::buildLogsPage() {
    auto* page = new QWidget(this);
    page->setObjectName("LogsRoot");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(10);

    if (!logEdit_) {
        logEdit_ = new QPlainTextEdit(page);
        logEdit_->setReadOnly(true);
        logEdit_->hide();
    }

    auto* header = workbenchCard(page, QStringLiteral("HeaderPanel"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 10, 14, 10);
    headerLayout->setSpacing(10);
    auto* titleBox = new QVBoxLayout();
    titleBox->setSpacing(3);
    auto* title = new QLabel(QStringLiteral("日志"), header);
    title->setObjectName("OverviewTitle");
    titleBox->addWidget(title);
    titleBox->addWidget(mutedLabel(QStringLiteral("会话、校验、导出、训练、预测"), header));
    headerLayout->addLayout(titleBox, 1);
    logsSourceCombo_ = new QComboBox(header);
    logsSourceCombo_->addItem(QStringLiteral("会话日志"), QStringLiteral("session"));
    logsSourceCombo_->addItem(QStringLiteral("校验日志"), QStringLiteral("validate"));
    logsSourceCombo_->addItem(QStringLiteral("导出日志"), QStringLiteral("export"));
    logsSourceCombo_->addItem(QStringLiteral("训练记录 JSON"), QStringLiteral("runs"));
    logsSourceCombo_->addItem(QStringLiteral("当前训练输出"), QStringLiteral("training"));
    logsSourceCombo_->addItem(QStringLiteral("当前预测输出"), QStringLiteral("prediction"));
    auto* refreshButton = new QPushButton(QStringLiteral("刷新"), header);
    auto* openButton = new QPushButton(QStringLiteral("打开文件"), header);
    connect(logsSourceCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::refreshLogsPage);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshLogsPage);
    connect(openButton, &QPushButton::clicked, this, [this]() {
        QString path;
        const QString source = comboStoredValue(logsSourceCombo_);
        if (source == QStringLiteral("validate")) {
            path = lastValidationLogPath_;
            if (path.isEmpty() && context_) {
                path = QDir(context_->logRoot()).filePath(QStringLiteral("validate.log"));
            }
        } else if (source == QStringLiteral("export") && context_) {
            path = QDir(context_->logRoot()).filePath(QStringLiteral("export.log"));
        } else if (source == QStringLiteral("runs") && context_) {
            path = context_->path(QStringLiteral("training/runs.json"));
        }
        if (path.isEmpty() || !QFileInfo::exists(path)) {
            QMessageBox::information(this, QStringLiteral("日志"), QStringLiteral("当前来源没有可打开的日志文件。"));
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    headerLayout->addWidget(logsSourceCombo_);
    headerLayout->addWidget(refreshButton);
    headerLayout->addWidget(openButton);

    auto* summaryPanel = workbenchCard(page, QStringLiteral("WorkbenchPanel"));
    auto* summaryLayout = new QHBoxLayout(summaryPanel);
    summaryLayout->setContentsMargins(12, 10, 12, 10);
    logsSummaryLabel_ = new QLabel(QStringLiteral("未打开项目"), summaryPanel);
    logsSummaryLabel_->setObjectName("SectionTitle");
    logsSummaryLabel_->setWordWrap(true);
    summaryLayout->addWidget(logsSummaryLabel_, 1);

    logsViewer_ = new QPlainTextEdit(page);
    logsViewer_->setReadOnly(true);
    logsViewer_->setMinimumHeight(420);
    logsViewer_->setPlainText(QStringLiteral("暂无日志。"));
    layout->addWidget(header);
    layout->addWidget(summaryPanel);
    layout->addWidget(logsViewer_, 1);
    refreshLogsPage();
    return page;
}

QWidget* MainWindow::buildSettingsPage() {
    auto* page = new QWidget(this);
    page->setObjectName("SettingsRoot");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(10);

    auto* header = workbenchCard(page, QStringLiteral("HeaderPanel"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 10, 14, 10);
    headerLayout->setSpacing(10);
    auto* titleBox = new QVBoxLayout();
    titleBox->setSpacing(3);
    auto* title = new QLabel(QStringLiteral("配置"), header);
    title->setObjectName("OverviewTitle");
    titleBox->addWidget(title);
    titleBox->addWidget(mutedLabel(QStringLiteral("项目路径、依赖、模型、运行环境"), header));
    headerLayout->addLayout(titleBox, 1);
    auto* refreshButton = new QPushButton(QStringLiteral("刷新"), header);
    auto* reportButton = primaryButton(QStringLiteral("环境报告"), header);
    auto* openRootButton = new QPushButton(QStringLiteral("打开目录"), header);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshSettingsPage);
    connect(reportButton, &QPushButton::clicked, this, &MainWindow::showEnvironmentReport);
    connect(openRootButton, &QPushButton::clicked, this, [this]() {
        const QString path = context_ ? context_->root.absolutePath() : baseDir_;
        if (!path.isEmpty() && QFileInfo::exists(path)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        }
    });
    headerLayout->addWidget(refreshButton);
    headerLayout->addWidget(reportButton);
    headerLayout->addWidget(openRootButton);

    auto* summaryPanel = workbenchCard(page, QStringLiteral("WorkbenchPanel"));
    auto* summaryLayout = new QHBoxLayout(summaryPanel);
    summaryLayout->setContentsMargins(12, 10, 12, 10);
    settingsSummaryLabel_ = new QLabel(QStringLiteral("未打开项目"), summaryPanel);
    settingsSummaryLabel_->setObjectName("SectionTitle");
    settingsSummaryLabel_->setWordWrap(true);
    summaryLayout->addWidget(settingsSummaryLabel_, 1);

    auto* body = new QSplitter(Qt::Horizontal, page);
    auto* pathPanel = workbenchCard(body, QStringLiteral("WorkbenchPanel"));
    auto* pathLayout = new QVBoxLayout(pathPanel);
    pathLayout->setContentsMargins(12, 10, 12, 12);
    pathLayout->addWidget(sectionLabel(QStringLiteral("路径状态"), pathPanel));
    settingsPathTable_ = tableWidget(3, {
        QStringLiteral("项目 / 依赖"),
        QStringLiteral("状态"),
        QStringLiteral("路径"),
    }, pathPanel);
    settingsPathTable_->setMinimumWidth(460);
    pathLayout->addWidget(settingsPathTable_, 1);

    settingsReportView_ = new QPlainTextEdit(body);
    settingsReportView_->setReadOnly(true);
    settingsReportView_->setMinimumWidth(420);
    body->addWidget(pathPanel);
    body->addWidget(settingsReportView_);
    body->setSizes({560, 520});

    layout->addWidget(header);
    layout->addWidget(summaryPanel);
    layout->addWidget(body, 1);
    refreshSettingsPage();
    return page;
}

void MainWindow::setProject(const ProjectContext& context) {
    context_ = context;
    if (projectLabel_) {
        projectLabel_->setText(QStringLiteral("● 状态：正常"));
    }
    if (trainingHeaderProjectLabel_) {
        trainingHeaderProjectLabel_->setText(QStringLiteral("当前项目：%1").arg(context.root.absolutePath()));
    }
    if (trainingHeaderStatusLabel_) {
        trainingHeaderStatusLabel_->setText(QStringLiteral("状态：未启动"));
    }
    rememberProject(context.root.absolutePath());
    refreshPages();
    refreshTrainingVersions();
    if (predictOutputEdit_) {
        predictOutputEdit_->setText(defaultPredictionOutputDir());
    }
    refreshPredictionModels();
    refreshLabelModelChoices();
    loadLabelModelConfig();
    refreshDatasetPage();
    refreshModelLibraryPage();
    refreshLogsPage();
    refreshSettingsPage();
    appendLog(QStringLiteral("打开项目：") + context.root.absolutePath());
}

void MainWindow::refreshPages() {
    if (!pageList_) {
        return;
    }
    pageList_->clear();
    allPages_.clear();
    pages_.clear();
    if (!context_) {
        if (pageCountLabel_) {
            pageCountLabel_->setText(QStringLiteral("0 页"));
        }
        if (labelWorkspaceSummary_) {
            labelWorkspaceSummary_->setText(QStringLiteral("未打开项目"));
        }
        return;
    }
    allPages_ = ProjectRepository::listPages(*context_);
    applyPageFilters();
    if (pageCountLabel_) {
        pageCountLabel_->setText(QStringLiteral("%1 页").arg(allPages_.size()));
    }
    if (labelWorkspaceSummary_) {
        labelWorkspaceSummary_->setText(QStringLiteral("%1 | 共 %2 页 | 当前筛选 %3 页")
                .arg(context_->config.value("project_name").toString(QStringLiteral("未命名项目")))
                .arg(allPages_.size())
                .arg(pages_.size()));
    }
    int labeledCount = 0;
    for (const auto& page : allPages_) {
        if (page.status == QStringLiteral("labeled") || page.status == QStringLiteral("validated")) {
            ++labeledCount;
        }
    }
    if (labelWorkspaceSummary_) {
        labelWorkspaceSummary_->setText(QStringLiteral("%1 | %2 页 | 已标注 %3 页")
                .arg(QDir::toNativeSeparators(context_->root.absolutePath()))
                .arg(allPages_.size())
                .arg(labeledCount));
    }
    if (pageMetaLabel_) {
        pageMetaLabel_->setText(QDir::toNativeSeparators(context_->root.absolutePath()));
    }
    refreshOverviewStats();
    refreshDatasetPage();
}

void MainWindow::refreshDatasetPage() {
    if (!datasetSummaryLabel_ && !datasetOverviewTable_ && !datasetPathTable_) {
        return;
    }
    if (datasetOverviewTable_) {
        datasetOverviewTable_->setRowCount(0);
    }
    if (datasetPathTable_) {
        datasetPathTable_->setRowCount(0);
    }

    if (!context_) {
        if (datasetSummaryLabel_) {
            datasetSummaryLabel_->setText(QStringLiteral("未打开项目"));
        }
        if (datasetNextStepLabel_) {
            datasetNextStepLabel_->setText(QStringLiteral("下一步：新建或打开项目"));
            datasetNextStepLabel_->setObjectName("StatusWarn");
            datasetNextStepLabel_->style()->unpolish(datasetNextStepLabel_);
            datasetNextStepLabel_->style()->polish(datasetNextStepLabel_);
        }
        appendTableRow(datasetOverviewTable_, {
            QStringLiteral("素材导入"),
            QStringLiteral("未打开"),
            QStringLiteral("-"),
            QStringLiteral("打开项目"),
        });
        appendTableRow(datasetPathTable_, {
            QStringLiteral("工作台根目录"),
            pathStateText(baseDir_),
            nativePathText(baseDir_),
        });
        return;
    }

    const QList<PageInfo> pages = ProjectRepository::listPages(*context_);
    int trainPages = 0;
    int valPages = 0;
    int labeledPages = 0;
    int validatedPages = 0;
    int ocrRegions = 0;
    int layoutRegions = 0;
    int uncheckedRegions = 0;
    int recCrops = 0;
    int validationErrors = 0;
    int validationWarnings = 0;
    for (const auto& page : pages) {
        if (page.split == QStringLiteral("val")) {
            ++valPages;
        } else {
            ++trainPages;
        }
        if (page.status == QStringLiteral("labeled") || page.status == QStringLiteral("validated")) {
            ++labeledPages;
        }
        if (page.status == QStringLiteral("validated")) {
            ++validatedPages;
        }
        try {
            const QJsonObject annotation = ProjectRepository::readAnnotation(page.annotationPath);
            const QJsonArray regions = annotation.value(QStringLiteral("regions")).toArray();
            for (const auto& value : regions) {
                const QJsonObject region = value.toObject();
                if (region.value(QStringLiteral("type")).toString() == QStringLiteral("layout")) {
                    ++layoutRegions;
                } else {
                    ++ocrRegions;
                }
                if (!region.value(QStringLiteral("checked")).toBool(false)) {
                    ++uncheckedRegions;
                }
            }
            recCrops += annotation.value(QStringLiteral("rec_crops")).toArray().size();
            const QJsonObject validation = annotation.value(QStringLiteral("validation")).toObject();
            validationErrors += validation.value(QStringLiteral("errors")).toArray().size();
            validationWarnings += validation.value(QStringLiteral("warnings")).toArray().size();
        } catch (const std::exception&) {
            ++validationErrors;
        }
    }

    const QList<QPair<QString, QString>> exportSpecs{
        {QStringLiteral("ppocr_det"), QStringLiteral("Det")},
        {QStringLiteral("ppocr_rec"), QStringLiteral("Rec")},
        {QStringLiteral("ppocr_cls"), QStringLiteral("Doc Cls")},
        {QStringLiteral("ppocr_textline_cls"), QStringLiteral("Line Cls")},
        {QStringLiteral("table_classification"), QStringLiteral("Table Cls")},
        {QStringLiteral("coco_layout"), QStringLiteral("Layout")},
    };
    int exportedDatasets = 0;
    int exportedSamples = 0;
    QStringList exportDetails;
    for (const auto& spec : exportSpecs) {
        const QString root = QDir(context_->exportRoot()).filePath(spec.first);
        const int count = datasetItemCount(root);
        if (QFileInfo::exists(root) || count > 0) {
            ++exportedDatasets;
            exportedSamples += count;
            exportDetails.append(QStringLiteral("%1:%2").arg(spec.second).arg(count));
        }
    }

    int successVersions = 0;
    for (const auto& task : trainingTasks()) {
        if (!task.trainSupported) {
            continue;
        }
        const QJsonArray versions = TrainingRunStore::loadVersionManifest(*context_, task.key)
                .value(QStringLiteral("versions")).toArray();
        for (const auto& value : versions) {
            if (value.toObject().value(QStringLiteral("status")).toString() == QStringLiteral("success")) {
                ++successVersions;
            }
        }
    }

    QString nextStep;
    QString nextObject = QStringLiteral("StatusWarn");
    if (pages.isEmpty()) {
        nextStep = QStringLiteral("下一步：导入图片或 PDF");
    } else if (labeledPages < pages.size()) {
        nextStep = QStringLiteral("下一步：继续标注");
    } else if (lastValidationLogPath_.isEmpty() && validatedPages < pages.size()) {
        nextStep = QStringLiteral("下一步：运行校验");
    } else if (validationErrors > 0) {
        nextStep = QStringLiteral("下一步：修复校验错误");
    } else if (exportedDatasets == 0) {
        nextStep = QStringLiteral("下一步：导出训练集");
    } else {
        nextStep = QStringLiteral("下一步：开始训练或预测");
        nextObject = QStringLiteral("StatusOk");
    }

    if (datasetSummaryLabel_) {
        datasetSummaryLabel_->setText(QStringLiteral("%1 | 页面 %2 | 已标注 %3 | 区域 %4 | 导出 %5 个数据集")
                .arg(context_->config.value(QStringLiteral("project_name")).toString(QStringLiteral("未命名项目")))
                .arg(pages.size())
                .arg(labeledPages)
                .arg(ocrRegions + layoutRegions)
                .arg(exportedDatasets));
    }
    if (datasetNextStepLabel_) {
        datasetNextStepLabel_->setText(nextStep);
        datasetNextStepLabel_->setObjectName(nextObject.toUtf8().constData());
        datasetNextStepLabel_->style()->unpolish(datasetNextStepLabel_);
        datasetNextStepLabel_->style()->polish(datasetNextStepLabel_);
    }

    appendTableRow(datasetOverviewTable_, {
        QStringLiteral("素材导入"),
        pages.isEmpty() ? QStringLiteral("未导入") : QStringLiteral("正常"),
        QStringLiteral("页面 %1 | train %2 / val %3").arg(pages.size()).arg(trainPages).arg(valPages),
        pages.isEmpty() ? QStringLiteral("导入图片 / PDF") : QStringLiteral("进入标注"),
    });
    appendTableRow(datasetOverviewTable_, {
        QStringLiteral("标注进度"),
        labeledPages == pages.size() && !pages.isEmpty() ? QStringLiteral("已完成") : QStringLiteral("部分完成"),
        QStringLiteral("已标注 %1/%2 | OCR %3 | Layout %4 | unchecked %5 | crops %6")
            .arg(labeledPages)
            .arg(pages.size())
            .arg(ocrRegions)
            .arg(layoutRegions)
            .arg(uncheckedRegions)
            .arg(recCrops),
        labeledPages < pages.size() ? QStringLiteral("继续标注") : QStringLiteral("运行校验"),
    });
    appendTableRow(datasetOverviewTable_, {
        QStringLiteral("质量校验"),
        validationErrors > 0 ? QStringLiteral("有错误") : lastValidationLogPath_.isEmpty() ? QStringLiteral("未校验") : QStringLiteral("通过"),
        QStringLiteral("errors %1 | warnings %2 | validated %3")
            .arg(validationErrors)
            .arg(validationWarnings)
            .arg(validatedPages),
        validationErrors > 0 ? QStringLiteral("打开日志定位") : QStringLiteral("可导出"),
    });
    appendTableRow(datasetOverviewTable_, {
        QStringLiteral("训练导出"),
        exportedDatasets > 0 ? QStringLiteral("已导出") : QStringLiteral("未导出"),
        exportDetails.isEmpty()
            ? QStringLiteral("训练样本 0")
            : QStringLiteral("训练样本 %1 | %2").arg(exportedSamples).arg(exportDetails.join(QStringLiteral("  "))),
        exportedDatasets > 0 ? QStringLiteral("进入训练") : QStringLiteral("导出训练集"),
    });
    appendTableRow(datasetOverviewTable_, {
        QStringLiteral("模型训练"),
        successVersions > 0 ? QStringLiteral("已完成") : exportedDatasets > 0 ? QStringLiteral("可训练") : QStringLiteral("待导出"),
        QStringLiteral("成功版本 %1").arg(successVersions),
        successVersions > 0 ? QStringLiteral("进入模型库") : QStringLiteral("开始训练"),
    });

    const QList<QPair<QString, QString>> paths{
        {QStringLiteral("项目根目录"), context_->root.absolutePath()},
        {QStringLiteral("页面素材"), context_->imageRoot()},
        {QStringLiteral("标注 JSON"), context_->annotationRoot()},
        {QStringLiteral("识别裁剪"), context_->cropRoot()},
        {QStringLiteral("训练导出"), context_->exportRoot()},
        {QStringLiteral("训练版本"), context_->path(QStringLiteral("training"))},
        {QStringLiteral("预测结果"), context_->path(QStringLiteral("predictions"))},
        {QStringLiteral("日志"), context_->logRoot()},
    };
    for (const auto& row : paths) {
        appendTableRow(datasetPathTable_, {row.first, pathStateText(row.second), nativePathText(row.second)});
    }
    if (datasetOverviewTable_) {
        datasetOverviewTable_->resizeColumnsToContents();
    }
    if (datasetPathTable_) {
        datasetPathTable_->resizeColumnsToContents();
    }
}

void MainWindow::refreshModelLibraryPage() {
    if (!modelLibrarySummaryLabel_ && !modelLibraryTable_) {
        return;
    }
    if (modelLibraryTable_) {
        modelLibraryTable_->setRowCount(0);
    }
    if (!context_) {
        if (modelLibrarySummaryLabel_) {
            modelLibrarySummaryLabel_->setText(QStringLiteral("未打开项目"));
        }
        appendTableRow(modelLibraryTable_, {
            QStringLiteral("全部任务"),
            QStringLiteral("-"),
            QStringLiteral("未打开"),
            QStringLiteral("-"),
            QStringLiteral("-"),
            QStringLiteral("-"),
            QStringLiteral("-"),
            QStringLiteral("-"),
        }, 2);
        return;
    }

    struct ModelVersionRow {
        TrainingTaskSpec task;
        QJsonObject manifest;
        QJsonObject version;
        QDateTime time;
    };
    QList<ModelVersionRow> rows;
    int trainableTasks = 0;
    for (const auto& task : trainingTasks()) {
        if (task.trainSupported) {
            ++trainableTasks;
        }
        const QJsonObject manifest = TrainingRunStore::loadVersionManifest(*context_, task.key);
        const QJsonArray versions = manifest.value(QStringLiteral("versions")).toArray();
        if (versions.isEmpty()) {
            rows.append(ModelVersionRow{task, manifest, QJsonObject{}, QDateTime{}});
            continue;
        }
        for (const auto& value : versions) {
            const QJsonObject version = value.toObject();
            rows.append(ModelVersionRow{task, manifest, version, storeObjectTime(version)});
        }
    }
    std::sort(rows.begin(), rows.end(), [](const ModelVersionRow& left, const ModelVersionRow& right) {
        if (left.version.isEmpty() != right.version.isEmpty()) {
            return !left.version.isEmpty();
        }
        if (left.time.isValid() != right.time.isValid()) {
            return left.time.isValid();
        }
        if (left.time.isValid() && left.time != right.time) {
            return left.time > right.time;
        }
        return left.task.key < right.task.key;
    });

    int versionCount = 0;
    int usableModels = 0;
    int currentCount = 0;
    int bestCount = 0;
    for (const auto& row : rows) {
        QString versionId = QStringLiteral("-");
        QString status = row.task.trainSupported ? QStringLiteral("未训练") : QStringLiteral("暂不支持");
        QString metric = QStringLiteral("-");
        QString current = QStringLiteral("-");
        QString best = QStringLiteral("-");
        QString modelDir = row.task.datasetName;
        QString finished = QStringLiteral("-");
        if (!row.version.isEmpty()) {
            ++versionCount;
            versionId = row.version.value(QStringLiteral("version_id")).toString(QStringLiteral("-"));
            status = statusLabelText(row.version.value(QStringLiteral("status")).toString());
            metric = versionMetricText(row.task, row.version);
            const bool isCurrent = row.version.value(QStringLiteral("is_current")).toBool()
                    || row.manifest.value(QStringLiteral("current_version_id")).toString() == versionId;
            const bool isBest = row.version.value(QStringLiteral("is_best")).toBool()
                    || row.manifest.value(QStringLiteral("best_version_id")).toString() == versionId;
            if (isCurrent) {
                current = QStringLiteral("当前");
                ++currentCount;
            }
            if (isBest) {
                best = QStringLiteral("最佳");
                ++bestCount;
            }
            QString reason;
            const QString inferenceDir = inferenceDirFromVersion(row.task, row.version);
            if (!inferenceDir.isEmpty()) {
                modelDir = inferenceDir;
                if (PaddleInferenceRuntime::modelDirLooksUsable(inferenceDir, &reason)) {
                    ++usableModels;
                }
            } else {
                modelDir = row.version.value(QStringLiteral("best_weight_path")).toString(
                        row.version.value(QStringLiteral("version_dir")).toString(
                            row.version.value(QStringLiteral("output_dir")).toString(row.task.datasetName)));
            }
            finished = row.version.value(QStringLiteral("finished_at")).toString(
                    row.version.value(QStringLiteral("started_at")).toString(QStringLiteral("-")));
        }
        appendTableRow(modelLibraryTable_, {
            row.task.title,
            versionId,
            status,
            metric,
            current,
            best,
            nativePathText(modelDir),
            finished,
        }, 2);
        const int tableRow = modelLibraryTable_ ? modelLibraryTable_->rowCount() - 1 : -1;
        if (modelLibraryTable_ && tableRow >= 0) {
            for (int column = 0; column < modelLibraryTable_->columnCount(); ++column) {
                if (auto* item = modelLibraryTable_->item(tableRow, column)) {
                    item->setData(Qt::UserRole, row.task.key);
                    item->setData(Qt::UserRole + 1, versionId);
                }
            }
        }
    }
    if (modelLibrarySummaryLabel_) {
        modelLibrarySummaryLabel_->setText(QStringLiteral("训练任务 %1 | 版本 %2 | 可推理模型 %3 | 当前 %4 | 最佳 %5")
                .arg(trainableTasks)
                .arg(versionCount)
                .arg(usableModels)
                .arg(currentCount)
                .arg(bestCount));
    }
    if (modelLibraryTable_) {
        modelLibraryTable_->resizeColumnsToContents();
    }
}

void MainWindow::refreshLogsPage() {
    if (!logsViewer_) {
        return;
    }
    const QString source = comboStoredValue(logsSourceCombo_).isEmpty()
            ? QStringLiteral("session")
            : comboStoredValue(logsSourceCombo_);
    QString path;
    QString text;
    QString sourceLabel = logsSourceCombo_ ? logsSourceCombo_->currentText() : QStringLiteral("会话日志");
    if (source == QStringLiteral("session")) {
        text = logEdit_ ? logEdit_->toPlainText() : QString();
    } else if (source == QStringLiteral("validate")) {
        path = lastValidationLogPath_;
        if (path.isEmpty() && context_) {
            path = QDir(context_->logRoot()).filePath(QStringLiteral("validate.log"));
        }
        text = readTextPreview(path);
    } else if (source == QStringLiteral("export")) {
        if (context_) {
            path = QDir(context_->logRoot()).filePath(QStringLiteral("export.log"));
        }
        text = readTextPreview(path);
    } else if (source == QStringLiteral("runs")) {
        if (context_) {
            path = context_->path(QStringLiteral("training/runs.json"));
        }
        text = readTextPreview(path);
    } else if (source == QStringLiteral("training")) {
        text = trainingPreview_ ? trainingPreview_->toPlainText() : trainingLogBuffer_;
    } else if (source == QStringLiteral("prediction")) {
        text = predictionPreview_ ? predictionPreview_->toPlainText() : predictionLogBuffer_;
    }
    if (text.trimmed().isEmpty()) {
        text = QStringLiteral("暂无日志。");
    }
    logsViewer_->setPlainText(text);
    logsViewer_->moveCursor(QTextCursor::End);
    if (logsSummaryLabel_) {
        logsSummaryLabel_->setText(path.isEmpty()
                ? QStringLiteral("%1 | 内存输出").arg(sourceLabel)
                : QStringLiteral("%1 | %2").arg(sourceLabel, nativePathText(path)));
    }
}

void MainWindow::refreshSettingsPage() {
    if (!settingsSummaryLabel_ && !settingsPathTable_ && !settingsReportView_) {
        return;
    }
    if (settingsPathTable_) {
        settingsPathTable_->setRowCount(0);
    }

    int missing = 0;
    auto addPath = [this, &missing](const QString& name, const QString& path, bool executable = false) {
        QString displayPath = path;
        bool exists = QFileInfo::exists(path);
        if (executable) {
            const QJsonObject status = EnvironmentReport::executableStatus(path);
            displayPath = status.value(QStringLiteral("resolved_executable")).toString(path);
            exists = status.value(QStringLiteral("executable_found")).toBool(false);
        }
        if (!exists) {
            ++missing;
        }
        appendTableRow(settingsPathTable_, {
            name,
            exists ? QStringLiteral("存在") : QStringLiteral("缺失"),
            nativePathText(displayPath),
        });
    };

    addPath(QStringLiteral("工作台根目录"), baseDir_);
    addPath(QStringLiteral("PaddleOCR"), QDir(baseDir_).filePath(QStringLiteral("PaddleOCR")));
    addPath(QStringLiteral("PaddleX"), QDir(baseDir_).filePath(QStringLiteral("PaddleX")));
    addPath(QStringLiteral("PaddleX Python"), paddlexPythonPath(), true);
    addPath(QStringLiteral("推理模型"), QDir(baseDir_).filePath(QStringLiteral("model/infer")));
    addPath(QStringLiteral("CPU 推理 SDK"), QDir(baseDir_).filePath(QStringLiteral("third_party/paddle_inference")));
    addPath(QStringLiteral("GPU 推理 SDK"), QDir(baseDir_).filePath(QStringLiteral("third_party/paddle_inference_gpu")));
    addPath(QStringLiteral("运行目录"), QCoreApplication::applicationDirPath());
    if (context_) {
        addPath(QStringLiteral("当前项目"), context_->root.absolutePath());
        addPath(QStringLiteral("页面素材"), context_->imageRoot());
        addPath(QStringLiteral("标注 JSON"), context_->annotationRoot());
        addPath(QStringLiteral("训练导出"), context_->exportRoot());
        addPath(QStringLiteral("训练版本"), context_->path(QStringLiteral("training")));
        addPath(QStringLiteral("预测结果"), context_->path(QStringLiteral("predictions")));
        addPath(QStringLiteral("日志"), context_->logRoot());
    }

    if (settingsSummaryLabel_) {
        settingsSummaryLabel_->setText(QStringLiteral("%1 | %2")
                .arg(context_
                    ? context_->config.value(QStringLiteral("project_name")).toString(QStringLiteral("未命名项目"))
                    : QStringLiteral("未打开项目"))
                .arg(missing == 0
                    ? QStringLiteral("路径状态正常")
                    : QStringLiteral("%1 项缺失").arg(missing)));
    }
    if (settingsPathTable_) {
        settingsPathTable_->resizeColumnsToContents();
    }
    if (settingsReportView_) {
        if (stack_ && stack_->currentIndex() == 7) {
            EnvironmentReportOptions options;
            options.baseDir = baseDir_;
            options.pythonExe = paddlexPythonPath();
            options.applicationDir = QCoreApplication::applicationDirPath();
            const QJsonObject report = EnvironmentReport::build(options);
            settingsReportView_->setPlainText(QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented)));
        } else {
            QJsonObject quick{
                {QStringLiteral("base_dir"), nativePathText(baseDir_)},
                {QStringLiteral("project_dir"), context_ ? nativePathText(context_->root.absolutePath()) : QString()},
                {QStringLiteral("missing_paths"), missing},
            };
            settingsReportView_->setPlainText(QString::fromUtf8(QJsonDocument(quick).toJson(QJsonDocument::Indented)));
        }
    }
}

void MainWindow::refreshOverviewStats() {
    auto setMetric = [](QLabel* label, const QString& value) {
        if (label) {
            label->setText(value);
        }
    };

    QVector<QJsonObject> runs;
    if (context_) {
        for (const auto& value : TrainingRunStore::loadRuns(*context_)) {
            runs.append(value.toObject());
        }
    }

    int running = 0;
    int done = 0;
    int failed = 0;
    int stopped = 0;
    for (const auto& run : runs) {
        const QString status = run.value(QStringLiteral("status")).toString();
        if (status == QStringLiteral("running")) {
            ++running;
        } else if (status == QStringLiteral("success")) {
            ++done;
        } else if (status == QStringLiteral("failed")) {
            ++failed;
        } else if (status == QStringLiteral("stopped")) {
            ++stopped;
        }
    }

    QVector<QJsonObject> datasetRows;
    int datasetCount = 0;
    if (context_) {
        const QList<QPair<QString, QString>> specs{
            {QStringLiteral("ppocr_textline_cls"), QStringLiteral("Cls 文本行")},
            {QStringLiteral("ppocr_cls"), QStringLiteral("Cls 文档方向")},
            {QStringLiteral("ppocr_rec"), QStringLiteral("Rec 识别")},
            {QStringLiteral("ppocr_det"), QStringLiteral("Det 检测")},
            {QStringLiteral("table_classification"), QStringLiteral("Cls 表格")},
            {QStringLiteral("coco_layout"), QStringLiteral("Layout 版面")},
        };
        for (const auto& spec : specs) {
            const QString root = QDir(context_->exportRoot()).filePath(spec.first);
            const int count = datasetItemCount(root);
            const QDateTime latest = latestFileTime(root);
            if (QFileInfo::exists(root) || count > 0) {
                datasetRows.append(QJsonObject{
                    {QStringLiteral("name"), spec.first},
                    {QStringLiteral("kind"), spec.second},
                    {QStringLiteral("count"), count},
                    {QStringLiteral("time"), latest.isValid() ? latest.toString(QStringLiteral("yyyy-MM-dd HH:mm")) : QStringLiteral("-")},
                });
            }
        }
        datasetCount = datasetRows.size();
    }

    int availableModels = 0;
    if (context_) {
        availableModels = countUsableInferenceModels(*context_);
    } else {
        const QDir inferRoot(QDir(baseDir_).filePath(QStringLiteral("model/infer")));
        availableModels = inferRoot.exists() ? inferRoot.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot).size() : 0;
    }

    const int totalRuns = runs.isEmpty() ? trainingTasks().size() : runs.size();
    setMetric(overviewPagesMetric_, QString::number(totalRuns));
    setMetric(overviewLabeledMetric_, QString::number(running));
    setMetric(overviewSplitMetric_, QString::number(done));
    setMetric(overviewRegionsMetric_, QString::number(failed));
    setMetric(overviewExportsMetric_, QString::number(availableModels));
    setMetric(overviewVersionsMetric_, QString::number(datasetCount));

    if (overviewDonutChart_) {
        const int notStarted = qMax(0, totalRuns - running - done - failed - stopped);
        overviewDonutChart_->setData({
            {QStringLiteral("进行中"), running, QColor("#2563eb")},
            {QStringLiteral("已完成"), done, QColor("#46d39a")},
            {QStringLiteral("失败"), failed, QColor("#b0444d")},
            {QStringLiteral("已停止"), stopped, QColor("#d79b35")},
            {QStringLiteral("未开始"), notStarted, QColor("#3a3e47")},
        });
    }

    const int days = overviewTrendRangeCombo_ && overviewTrendRangeCombo_->currentIndex() == 1
        ? 14
        : overviewTrendRangeCombo_ && overviewTrendRangeCombo_->currentIndex() == 2 ? 30 : 7;
    QStringList labels;
    QVector<double> created(days, 0.0);
    QVector<double> completed(days, 0.0);
    QVector<double> failedSeries(days, 0.0);
    const QDate today = QDate::currentDate();
    for (int i = 0; i < days; ++i) {
        labels.append(today.addDays(i - days + 1).toString(QStringLiteral("MM-dd")));
    }
    for (const auto& run : runs) {
        const QDateTime started = parseStoreDateTime(run.value(QStringLiteral("started_at")).toString());
        if (started.isValid()) {
            const int offset = started.date().daysTo(today);
            if (offset >= 0 && offset < days) {
                created[days - offset - 1] += 1.0;
            }
        }
        QDateTime finished = parseStoreDateTime(run.value(QStringLiteral("finished_at")).toString());
        if (!finished.isValid()) {
            finished = started;
        }
        if (!finished.isValid()) {
            continue;
        }
        const int offset = finished.date().daysTo(today);
        if (offset < 0 || offset >= days) {
            continue;
        }
        const QString status = run.value(QStringLiteral("status")).toString();
        if (status == QStringLiteral("success")) {
            completed[days - offset - 1] += 1.0;
        } else if (status == QStringLiteral("failed")) {
            failedSeries[days - offset - 1] += 1.0;
        }
    }
    if (overviewTrendChart_) {
        overviewTrendChart_->setData(labels, {
            {QStringLiteral("创建任务"), created, QColor("#2563eb")},
            {QStringLiteral("完成任务"), completed, QColor("#46d39a")},
            {QStringLiteral("失败任务"), failedSeries, QColor("#b0444d")},
        });
    }

    if (overviewRecentTasksTable_) {
        std::sort(runs.begin(), runs.end(), [](const QJsonObject& left, const QJsonObject& right) {
            const QString leftTime = left.value(QStringLiteral("finished_at")).toString(left.value(QStringLiteral("started_at")).toString());
            const QString rightTime = right.value(QStringLiteral("finished_at")).toString(right.value(QStringLiteral("started_at")).toString());
            return leftTime > rightTime;
        });
        overviewRecentTasksTable_->setRowCount(0);
        const int rows = qMin(5, runs.size());
        for (int row = 0; row < rows; ++row) {
            const auto run = runs.at(row);
            overviewRecentTasksTable_->insertRow(row);
            const QString time = run.value(QStringLiteral("finished_at")).toString(run.value(QStringLiteral("started_at")).toString());
            const QStringList values{
                run.value(QStringLiteral("task_key")).toString(run.value(QStringLiteral("task_title")).toString()),
                statusLabelText(run.value(QStringLiteral("status")).toString()),
                time,
            };
            for (int column = 0; column < values.size(); ++column) {
                overviewRecentTasksTable_->setItem(row, column, new QTableWidgetItem(values.at(column)));
            }
        }
    }

    if (overviewDatasetTable_) {
        overviewDatasetTable_->setRowCount(0);
        for (int row = 0; row < datasetRows.size(); ++row) {
            overviewDatasetTable_->insertRow(row);
            const QJsonObject item = datasetRows.at(row);
            const QStringList values{
                item.value(QStringLiteral("name")).toString(),
                item.value(QStringLiteral("kind")).toString(),
                QString::number(item.value(QStringLiteral("count")).toInt()),
                item.value(QStringLiteral("time")).toString(),
            };
            for (int column = 0; column < values.size(); ++column) {
                overviewDatasetTable_->setItem(row, column, new QTableWidgetItem(values.at(column)));
            }
        }
    }

    refreshOverviewResources();
}

void MainWindow::refreshOverviewResources() {
    if (overviewGpuCard_) {
        const auto gpu = currentGpuPercent();
        overviewGpuCard_->setData(
            resourcePercentText(gpu),
            QStringLiteral("GPU 使用 需要 nvidia-smi"),
            resourceSparkValues(gpu));
    }
    if (overviewCpuCard_) {
        const auto cpu = currentCpuPercent();
        overviewCpuCard_->setData(
            resourcePercentText(cpu),
            QStringLiteral("核心数 自动检测"),
            resourceSparkValues(cpu));
    }
    if (overviewMemoryCard_) {
        const auto memory = currentMemoryPercent();
        overviewMemoryCard_->setData(
            resourcePercentText(memory),
            QStringLiteral("内存使用 自动检测"),
            resourceSparkValues(memory));
    }
    if (overviewDiskCard_) {
        const auto disk = currentDiskUsage(context_ ? context_->root.absolutePath() : baseDir_);
        overviewDiskCard_->setData(
            resourcePercentText(disk.percent),
            disk.subtitle,
            resourceSparkValues(disk.percent));
    }
}

bool MainWindow::pageMatchesFilters(const PageInfo& page) const {
    const QString query = pageSearchEdit_ ? pageSearchEdit_->text().trimmed().toLower() : QString();
    if (!query.isEmpty()) {
        const QString haystack = QString("%1 %2 %3 %4").arg(page.assetId, page.relativeImagePath, page.split, page.status).toLower();
        if (!haystack.contains(query)) {
            return false;
        }
    }
    const QString splitFilter = comboStoredValue(pageSplitFilterCombo_);
    if (!splitFilter.isEmpty() && page.split != splitFilter) {
        return false;
    }
    const QString statusFilter = comboStoredValue(pageStatusFilterCombo_);
    if (!statusFilter.isEmpty() && page.status != statusFilter) {
        return false;
    }
    return true;
}

void MainWindow::applyPageFilters() {
    if (!pageList_) {
        return;
    }
    const QString previousAssetId = pageList_->currentRow() >= 0 && pageList_->currentRow() < pages_.size()
        ? pages_.at(pageList_->currentRow()).assetId
        : QString();
    pageList_->clear();
    pages_.clear();
    int selectedRow = -1;
    for (const auto& page : allPages_) {
        if (!pageMatchesFilters(page)) {
            continue;
        }
        pages_.append(page);
        const QString thumbPath = context_ ? QDir(context_->thumbRoot()).filePath(page.assetId + ".jpg") : QString();
        auto* item = new QListWidgetItem(QFileInfo::exists(thumbPath) ? QIcon(thumbPath) : QIcon(), pageListText(page), pageList_);
        item->setData(Qt::UserRole, page.assetId);
        if (page.assetId == previousAssetId) {
            selectedRow = pages_.size() - 1;
        }
    }
    if (!pages_.isEmpty()) {
        pageList_->setCurrentRow(selectedRow >= 0 ? selectedRow : 0);
    } else {
        currentAnnotation_ = QJsonObject{};
        annotationUndoStack_.clear();
        annotationRedoStack_.clear();
        selectedRegionId_.clear();
        pageMetaLabel_->setText(QStringLiteral("没有页面匹配当前筛选"));
        if (fileTitleLabel_) {
            fileTitleLabel_->setText(QStringLiteral("未选择图片"));
        }
        if (fileMetaLabel_) {
            fileMetaLabel_->setText(QStringLiteral("选择左侧图片开始标注"));
        }
        if (labelWorkspaceSummary_ && context_) {
            labelWorkspaceSummary_->setText(QStringLiteral("%1 | 共 %2 页 | 当前筛选 0 页")
                    .arg(context_->config.value("project_name").toString(QStringLiteral("未命名项目")))
                    .arg(allPages_.size()));
        }
        if (auto* root = canvasWidget_ ? canvasWidget_->rootObject() : nullptr) {
            root->setProperty("imageSource", QUrl());
        }
        updatePageMetadataPanel();
        refreshCanvasAnnotation();
        updateSelectedRegionPanel();
        updateAnnotationHistoryActions();
    }
}

void MainWindow::filterPages() {
    applyPageFilters();
}

void MainWindow::loadSelectedPage() {
    const int row = pageList_->currentRow();
    if (!context_ || row < 0 || row >= pages_.size()) {
        return;
    }
    const auto page = pages_.at(row);
    currentAnnotation_ = ProjectRepository::readAnnotation(page.annotationPath);
    annotationUndoStack_.clear();
    annotationRedoStack_.clear();
    selectedRegionId_.clear();
    pageMetaLabel_->setText(QString("%1 | %2x%3 | %4 | %5")
        .arg(page.assetId)
        .arg(page.width)
        .arg(page.height)
        .arg(splitDisplayText(page.split), statusDisplayText(page.status)));
    if (fileTitleLabel_) {
        fileTitleLabel_->setText(QFileInfo(page.imagePath).fileName());
    }
    if (fileMetaLabel_) {
        fileMetaLabel_->setText(QStringLiteral("%1 × %2 | %3 | %4")
                .arg(page.width)
                .arg(page.height)
                .arg(splitDisplayText(page.split), statusDisplayText(page.status)));
    }
    if (labelWorkspaceSummary_) {
        labelWorkspaceSummary_->setText(QStringLiteral("%1 | 当前页 %2 | %3x%4 | %5 / %6")
                .arg(context_->config.value("project_name").toString(QStringLiteral("未命名项目")))
                .arg(page.assetId)
                .arg(page.width)
                .arg(page.height)
                .arg(splitDisplayText(page.split), statusDisplayText(page.status)));
    }
    if (pageMetaLabel_) {
        pageMetaLabel_->setText(QDir::toNativeSeparators(context_->root.absolutePath()));
    }
    if (fileMetaLabel_) {
        fileMetaLabel_->setText(QStringLiteral("第 %1 / %2 页 | %3 x %4 | %5 | %6")
                .arg(row + 1)
                .arg(pages_.size())
                .arg(page.width)
                .arg(page.height)
                .arg(page.status.isEmpty() ? QStringLiteral("unlabeled") : page.status)
                .arg(page.split.isEmpty() ? QStringLiteral("train") : page.split));
    }
    int labeledCount = 0;
    for (const auto& item : allPages_) {
        if (item.status == QStringLiteral("labeled") || item.status == QStringLiteral("validated")) {
            ++labeledCount;
        }
    }
    if (labelWorkspaceSummary_) {
        labelWorkspaceSummary_->setText(QStringLiteral("%1 | %2 页 | 已标注 %3 页")
                .arg(QDir::toNativeSeparators(context_->root.absolutePath()))
                .arg(allPages_.size())
                .arg(labeledCount));
    }
    if (auto* root = canvasWidget_->rootObject()) {
        root->setProperty("imageSource", QUrl::fromLocalFile(page.imagePath));
    }
    refreshCanvasAnnotation();
    updateSelectedRegionPanel();
    refreshImageLabelPanel();
    updatePageMetadataPanel();
    updateAnnotationHistoryActions();
}

void MainWindow::appendLog(const QString& text) {
    if (logEdit_) {
        logEdit_->appendPlainText(text);
    }
    if (logsViewer_ && logsSourceCombo_ && comboStoredValue(logsSourceCombo_) == QStringLiteral("session")) {
        logsViewer_->appendPlainText(text);
        logsViewer_->moveCursor(QTextCursor::End);
    }
}

void MainWindow::refreshValidationTable(const QList<ValidationIssue>& issues, const QString& logPath) {
    lastValidationLogPath_ = logPath;
    int errors = 0;
    int warnings = 0;
    int passed = 0;
    for (const auto& issue : issues) {
        if (issue.severity == QStringLiteral("error")) {
            ++errors;
        } else if (issue.severity == QStringLiteral("warning")) {
            ++warnings;
        } else if (issue.severity == QStringLiteral("passed")) {
            ++passed;
        }
    }

    if (validationSummaryLabel_) {
        validationSummaryLabel_->setText(QStringLiteral("校验：%1 个错误，%2 个警告，%3 页通过 | %4")
                .arg(errors)
                .arg(warnings)
                .arg(passed)
                .arg(logPath.isEmpty() ? QStringLiteral("日志未写入") : QDir::toNativeSeparators(logPath)));
        validationSummaryLabel_->setToolTip(logPath);
    }

    if (labelValidationSummaryLabel_) {
        labelValidationSummaryLabel_->setText(QStringLiteral("校验：%1 个错误，%2 个警告，%3 页通过")
                .arg(errors)
                .arg(warnings)
                .arg(passed));
        labelValidationSummaryLabel_->setToolTip(logPath);
    }

    auto fillValidationTable = [&issues](QTableWidget* table, bool includeLocation) {
        if (!table) {
            return;
        }
        table->setRowCount(0);
        for (const auto& issue : issues) {
            const int row = table->rowCount();
            table->insertRow(row);
            QStringList values{
                issue.severity,
                issue.task,
                issue.assetId,
                issue.regionId,
                issue.message,
            };
            if (includeLocation) {
                values.append(issue.location);
            }
            for (int column = 0; column < values.size(); ++column) {
            auto* item = new QTableWidgetItem(values.at(column));
            if (issue.severity == QStringLiteral("error")) {
                item->setData(Qt::ForegroundRole, QColor(QStringLiteral("#ff8b8b")));
            } else if (issue.severity == QStringLiteral("warning")) {
                item->setData(Qt::ForegroundRole, QColor(QStringLiteral("#f2c078")));
            } else if (issue.severity == QStringLiteral("passed")) {
                item->setData(Qt::ForegroundRole, QColor(QStringLiteral("#8dd39e")));
            }
                table->setItem(row, column, item);
            }
        }
        table->resizeColumnsToContents();
    };
    fillValidationTable(validationIssueTable_, true);
    fillValidationTable(labelValidationIssueTable_, false);
    refreshDatasetPage();
    refreshLogsPage();
}

QStringList MainWindow::loadRecentProjects() const {
    QFile file(QDir(baseDir_).filePath("recent_projects.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray()) {
        return {};
    }
    QStringList projects;
    for (const auto& value : doc.array()) {
        const QString path = value.toString();
        if (!path.isEmpty() && QFileInfo::exists(QDir(path).filePath("project.json"))) {
            projects.append(QFileInfo(path).absoluteFilePath());
        }
    }
    projects.removeDuplicates();
    return projects;
}

void MainWindow::saveRecentProjects(const QStringList& projects) const {
    QJsonArray array;
    for (const auto& project : projects) {
        array.append(project);
    }
    QFile file(QDir(baseDir_).filePath("recent_projects.json"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
    }
}

void MainWindow::rememberProject(const QString& projectDir) {
    if (projectDir.isEmpty()) {
        return;
    }
    QStringList projects = loadRecentProjects();
    const QString normalized = QFileInfo(projectDir).absoluteFilePath();
    projects.removeAll(normalized);
    projects.prepend(normalized);
    while (projects.size() > 20) {
        projects.removeLast();
    }
    saveRecentProjects(projects);
    refreshRecentProjects();
}

void MainWindow::refreshRecentProjects() {
    if (!recentProjectsCombo_) {
        return;
    }
    QSignalBlocker blocker(recentProjectsCombo_);
    recentProjectsCombo_->clear();
    if (context_) {
        recentProjectsCombo_->addItem(QStringLiteral("当前项目：%1").arg(context_->root.absolutePath()), QString());
    } else {
        recentProjectsCombo_->addItem(QStringLiteral("当前项目：未打开"), QString());
    }
    for (const auto& project : loadRecentProjects()) {
        if (context_ && QFileInfo(project).absoluteFilePath() == QFileInfo(context_->root.absolutePath()).absoluteFilePath()) {
            continue;
        }
        recentProjectsCombo_->addItem(project, project);
    }
}

void MainWindow::connectCanvasSignals() {
    auto* root = canvasWidget_ ? canvasWidget_->rootObject() : nullptr;
    if (!root) {
        return;
    }
    connect(root, SIGNAL(ocrBoxCreated(QVariant)), this, SLOT(addOcrRegionFromCanvas(QVariant)));
    connect(root, SIGNAL(layoutBoxCreated(QVariant)), this, SLOT(addLayoutRegionFromCanvas(QVariant)));
    connect(root, SIGNAL(regionSelected(QString)), this, SLOT(selectRegionFromCanvas(QString)));
    connect(root, SIGNAL(regionMoved(QString,QVariant)), this, SLOT(moveRegionFromCanvas(QString,QVariant)));
    connect(root, SIGNAL(regionDeleteRequested()), this, SLOT(deleteSelectedRegion()));
    setCanvasToolMode("select");
}

void MainWindow::setCanvasToolMode(const QString& mode) {
    const bool draw = mode == "drawOcr";
    const bool drawLayout = mode == "drawLayout";
    const bool pan = mode == "pan";
    if (selectToolButton_) {
        selectToolButton_->setChecked(!draw && !drawLayout && !pan);
    }
    if (drawOcrToolButton_) {
        drawOcrToolButton_->setChecked(draw);
    }
    if (drawLayoutToolButton_) {
        drawLayoutToolButton_->setChecked(drawLayout);
    }
    if (panToolButton_) {
        panToolButton_->setChecked(pan);
    }
    if (auto* root = canvasWidget_ ? canvasWidget_->rootObject() : nullptr) {
        root->setProperty("toolMode", mode);
    }
    if (canvasModeLabel_) {
        canvasModeLabel_->setText(draw
                ? QStringLiteral("当前模式：OCR 检测框")
                : (drawLayout ? QStringLiteral("当前模式：版面框")
                              : (pan ? QStringLiteral("当前模式：平移") : QStringLiteral("当前模式：选择"))));
    }
    statusBar()->showMessage(
        draw ? QStringLiteral("OCR 框绘制模式")
             : (drawLayout ? QStringLiteral("版面框绘制模式")
                           : (pan ? QStringLiteral("画布平移模式") : QStringLiteral("选择模式"))),
        2000);
}

void MainWindow::refreshCanvasAnnotation() {
    if (auto* root = canvasWidget_ ? canvasWidget_->rootObject() : nullptr) {
        root->setProperty("annotationJson", QString::fromUtf8(QJsonDocument(currentAnnotation_).toJson(QJsonDocument::Compact)));
        root->setProperty("selectedRegionId", selectedRegionId_);
    }
}

void MainWindow::persistCurrentAnnotation() {
    if (!context_) {
        return;
    }
    const int row = pageList_ ? pageList_->currentRow() : -1;
    if (row < 0 || row >= pages_.size()) {
        return;
    }
    ProjectRepository::writeAnnotation(pages_.at(row).annotationPath, currentAnnotation_);
    pages_[row].status = currentAnnotation_.value("status").toString(pages_.at(row).status);
    pages_[row].split = currentAnnotation_.value("split").toString(pages_.at(row).split);
    for (auto& page : allPages_) {
        if (page.assetId == pages_.at(row).assetId) {
            page.status = pages_[row].status;
            page.split = pages_[row].split;
            break;
        }
    }
    pageMetaLabel_->setText(QString("%1 | %2x%3 | %4 | %5")
        .arg(pages_.at(row).assetId)
        .arg(pages_.at(row).width)
        .arg(pages_.at(row).height)
        .arg(splitDisplayText(pages_.at(row).split))
        .arg(statusDisplayText(pages_.at(row).status)));
    if (pageMetaLabel_) {
        pageMetaLabel_->setText(QDir::toNativeSeparators(context_->root.absolutePath()));
    }
    if (fileMetaLabel_) {
        fileMetaLabel_->setText(QStringLiteral("第 %1 / %2 页 | %3 x %4 | %5 | %6")
                .arg(row + 1)
                .arg(pages_.size())
                .arg(pages_.at(row).width)
                .arg(pages_.at(row).height)
                .arg(pages_.at(row).status.isEmpty() ? QStringLiteral("unlabeled") : pages_.at(row).status)
                .arg(pages_.at(row).split.isEmpty() ? QStringLiteral("train") : pages_.at(row).split));
    }
    if (auto* item = pageList_->item(row)) {
        item->setText(pageListText(pages_.at(row)));
    }
    if (!pageMatchesFilters(pages_.at(row))) {
        applyPageFilters();
    }
}

void MainWindow::pushAnnotationUndoState() {
    if (currentAnnotation_.isEmpty()) {
        return;
    }
    if (!annotationUndoStack_.isEmpty() && jsonObjectsEqual(annotationUndoStack_.last(), currentAnnotation_)) {
        return;
    }
    annotationUndoStack_.append(currentAnnotation_);
    while (annotationUndoStack_.size() > 80) {
        annotationUndoStack_.removeFirst();
    }
    annotationRedoStack_.clear();
    updateAnnotationHistoryActions();
}

void MainWindow::updateAnnotationHistoryActions() {
    const bool hasPage = context_ && !currentAnnotation_.isEmpty();
    if (undoAnnotationButton_) {
        undoAnnotationButton_->setEnabled(hasPage && !annotationUndoStack_.isEmpty());
    }
    if (redoAnnotationButton_) {
        redoAnnotationButton_->setEnabled(hasPage && !annotationRedoStack_.isEmpty());
    }
}

void MainWindow::undoAnnotationEdit() {
    if (!context_ || annotationUndoStack_.isEmpty() || currentAnnotation_.isEmpty()) {
        return;
    }
    annotationRedoStack_.append(currentAnnotation_);
    currentAnnotation_ = annotationUndoStack_.takeLast();
    if (!selectedRegionId_.isEmpty() && findRegion(currentAnnotation_, selectedRegionId_).isEmpty()) {
        selectedRegionId_.clear();
    }
    persistCurrentAnnotation();
    refreshCanvasAnnotation();
    updateSelectedRegionPanel();
    refreshImageLabelPanel();
    updateAnnotationHistoryActions();
    statusBar()->showMessage(QStringLiteral("已撤销标注修改"), 2000);
}

void MainWindow::redoAnnotationEdit() {
    if (!context_ || annotationRedoStack_.isEmpty() || currentAnnotation_.isEmpty()) {
        return;
    }
    annotationUndoStack_.append(currentAnnotation_);
    currentAnnotation_ = annotationRedoStack_.takeLast();
    if (!selectedRegionId_.isEmpty() && findRegion(currentAnnotation_, selectedRegionId_).isEmpty()) {
        selectedRegionId_.clear();
    }
    persistCurrentAnnotation();
    refreshCanvasAnnotation();
    updateSelectedRegionPanel();
    refreshImageLabelPanel();
    updateAnnotationHistoryActions();
    statusBar()->showMessage(QStringLiteral("已重做标注修改"), 2000);
}

void MainWindow::updatePageMetadataPanel() {
    if (!pageSplitCombo_ || !pageStatusCombo_) {
        return;
    }
    const int row = pageList_ ? pageList_->currentRow() : -1;
    const bool hasPage = context_ && row >= 0 && row < pages_.size();
    updatingPageMetadata_ = true;
    pageSplitCombo_->setEnabled(hasPage);
    pageStatusCombo_->setEnabled(hasPage);
    if (hasPage) {
        setComboToValue(pageSplitCombo_, pages_.at(row).split);
        setComboToValue(pageStatusCombo_, pages_.at(row).status);
    } else {
        pageSplitCombo_->setCurrentIndex(0);
        pageStatusCombo_->setCurrentIndex(0);
    }
    updatingPageMetadata_ = false;
}

void MainWindow::updateSelectedRegionPanel() {
    if (!regionTextEdit_ || !selectedRegionLabel_ || !saveRegionButton_ || !deleteRegionButton_) {
        return;
    }
    updatingRegionPanel_ = true;
    const QJsonObject region = findRegion(currentAnnotation_, selectedRegionId_);
    const bool hasRegion = !region.isEmpty();
    const QString type = region.value("type").toString();
    const bool isLayout = hasRegion && type == "layout";
    const bool isOcr = hasRegion && type == "ocr_text";
    selectedRegionLabel_->setText(hasRegion
        ? QString("%1 | %2 | %3").arg(region.value("id").toString(), type, region.value("source").toString("manual"))
        : QStringLiteral("未选择区域"));
    regionTextEdit_->setEnabled(hasRegion);
    selectedRegionLabel_->setText(hasRegion
        ? QStringLiteral("ID: %1").arg(region.value("id").toString())
        : QStringLiteral("ID: -"));
    saveRegionButton_->setEnabled(hasRegion);
    deleteRegionButton_->setEnabled(hasRegion);
    regionTextEdit_->setVisible(!isLayout);
    regionTextEdit_->setPlainText(isOcr ? region.value("text").toString() : QString());
    if (layoutLabelCombo_) {
        layoutLabelCombo_->setVisible(isLayout);
        if (isLayout) {
            setComboToValue(layoutLabelCombo_, region.value("label").toString());
        }
    }
    if (layoutLabelCombo_) {
        layoutLabelCombo_->setVisible(true);
        layoutLabelCombo_->setEnabled(isLayout);
        if (!isLayout && layoutLabelCombo_->currentText().isEmpty()) {
            setComboToValue(layoutLabelCombo_, QStringLiteral("title"));
        }
    }
    if (regionReadingOrderSpin_) {
        regionReadingOrderSpin_->setVisible(isOcr);
        regionReadingOrderSpin_->setEnabled(isOcr);
        regionReadingOrderSpin_->setValue(isOcr ? region.value("reading_order").toInt() : 0);
    }
    if (regionCheckedCheck_) {
        regionCheckedCheck_->setEnabled(hasRegion);
        regionCheckedCheck_->setChecked(hasRegion && region.value("checked").toBool());
    }
    if (regionIgnoreCheck_) {
        regionIgnoreCheck_->setVisible(isOcr);
        regionIgnoreCheck_->setEnabled(isOcr);
        regionIgnoreCheck_->setChecked(isOcr && region.value("ignore").toBool());
    }
    updatingRegionPanel_ = false;
    refreshCanvasAnnotation();
}

void MainWindow::fillComboFromLabelSet(QComboBox* combo, const QString& key) {
    if (!combo) {
        return;
    }
    const QString previous = combo->currentText();
    combo->blockSignals(true);
    combo->clear();
    combo->addItem("");
    const QJsonObject labelSets = context_ ? context_->config.value("label_sets").toObject() : QJsonObject{};
    const QJsonArray labels = labelSets.value(key).toArray();
    for (const auto& value : labels) {
        combo->addItem(value.toString());
    }
    combo->blockSignals(false);
    setComboToValue(combo, previous);
}

QString MainWindow::imageLabelValue(const QString& task) const {
    for (const auto& value : currentAnnotation_.value("image_labels").toArray()) {
        const auto item = value.toObject();
        if (item.value("task").toString() == task) {
            return item.value("label").toString();
        }
    }
    return {};
}

void MainWindow::setComboToValue(QComboBox* combo, const QString& value) {
    if (!combo) {
        return;
    }
    int index = combo->findData(value);
    if (index < 0) {
        index = combo->findText(value);
    }
    combo->blockSignals(true);
    if (index >= 0) {
        combo->setCurrentIndex(index);
    } else {
        if (!value.isEmpty()) {
            combo->addItem(value);
            combo->setCurrentText(value);
        } else {
            combo->setCurrentIndex(combo->findText(""));
        }
    }
    combo->blockSignals(false);
}

void MainWindow::refreshLabelModelChoices() {
    const ProjectContext* context = context_ ? &(*context_) : nullptr;
    const auto ocrConfig = PaddleOcrEngine::defaultConfig(baseDir_);
    addPredictionModelChoices(labelDetModelCombo_, context, tasksForKind(QStringLiteral("det")), ocrConfig.textDetectionModelDir);
    addPredictionModelChoices(labelRecModelCombo_, context, tasksForKind(QStringLiteral("rec")), ocrConfig.textRecognitionModelDir);

    const auto textlineClsConfig = PaddleClsEngine::defaultConfig(QStringLiteral("textline_orientation"), baseDir_);
    addPredictionModelChoices(
        labelTextlineClsModelCombo_,
        context,
        classificationTasksForPrediction(QStringLiteral("textline_orientation")),
        textlineClsConfig.modelDir,
        true);

    const auto docClsConfig = PaddleClsEngine::defaultConfig(QStringLiteral("doc_orientation"), baseDir_);
    addPredictionModelChoices(
        labelDocOrientationModelCombo_,
        context,
        classificationTasksForPrediction(QStringLiteral("doc_orientation")),
        docClsConfig.modelDir,
        true);

    const auto layoutConfig = PaddleDocLayoutEngine::defaultConfig(baseDir_);
    addPredictionModelChoices(
        labelLayoutModelCombo_,
        context,
        tasksForKind(QStringLiteral("layout")),
        layoutConfig.modelDir,
        true);

    const QString uvDocDir = QDir(baseDir_).filePath(QStringLiteral("model/infer/UVDoc_infer/UVDoc_infer"));
    addStandaloneModelChoice(labelDocUnwarpingModelCombo_, uvDocDir, true);
}

void MainWindow::loadLabelModelConfig() {
    if (!context_) {
        return;
    }
    const QJsonObject models = context_->config.value(QStringLiteral("local_models")).toObject();
    setComboToPath(labelDetModelCombo_, models.value(QStringLiteral("det_model_dir")).toString());
    setComboToPath(labelRecModelCombo_, models.value(QStringLiteral("rec_model_dir")).toString());
    setComboToPath(labelTextlineClsModelCombo_, models.value(QStringLiteral("cls_model_dir")).toString());
    setComboToPath(labelDocOrientationModelCombo_, models.value(QStringLiteral("doc_orientation_model_dir")).toString());
    setComboToPath(labelDocUnwarpingModelCombo_, models.value(QStringLiteral("doc_unwarping_model_dir")).toString());
    setComboToPath(labelLayoutModelCombo_, models.value(QStringLiteral("layout_model_dir")).toString());
    if (labelUseGpuCheck_) {
        labelUseGpuCheck_->setChecked(models.value(QStringLiteral("use_gpu")).toBool(false));
    }
    if (labelEnableTextlineClsCheck_) {
        labelEnableTextlineClsCheck_->setChecked(models.value(QStringLiteral("enable_cls")).toBool(false));
    }
    if (labelEnableDocOrientationCheck_) {
        labelEnableDocOrientationCheck_->setChecked(models.value(QStringLiteral("enable_doc_orientation")).toBool(false));
    }
    if (labelEnableDocUnwarpingCheck_) {
        labelEnableDocUnwarpingCheck_->setChecked(models.value(QStringLiteral("enable_doc_unwarping")).toBool(false));
    }
    if (prelabelScoreThresholdSpin_) {
        const double threshold = models.value(QStringLiteral("prelabel_score_threshold")).toDouble(0.50);
        prelabelScoreThresholdSpin_->setValue(qMax(0.0, qMin(1.0, threshold)));
    }
}

void MainWindow::saveLabelModelConfig() {
    if (!context_) {
        return;
    }
    QJsonObject models = context_->config.value(QStringLiteral("local_models")).toObject();
    models.insert(QStringLiteral("det_model_dir"), selectedPredictionModelDir(labelDetModelCombo_));
    models.insert(QStringLiteral("rec_model_dir"), selectedPredictionModelDir(labelRecModelCombo_));
    models.insert(QStringLiteral("cls_model_dir"), selectedPredictionModelDir(labelTextlineClsModelCombo_));
    models.insert(QStringLiteral("doc_orientation_model_dir"), selectedPredictionModelDir(labelDocOrientationModelCombo_));
    models.insert(QStringLiteral("doc_unwarping_model_dir"), selectedPredictionModelDir(labelDocUnwarpingModelCombo_));
    models.insert(QStringLiteral("layout_model_dir"), selectedPredictionModelDir(labelLayoutModelCombo_));
    models.insert(QStringLiteral("use_gpu"), labelUseGpuCheck_ && labelUseGpuCheck_->isChecked());
    models.insert(QStringLiteral("enable_cls"), labelEnableTextlineClsCheck_ && labelEnableTextlineClsCheck_->isChecked());
    models.insert(QStringLiteral("enable_doc_orientation"), labelEnableDocOrientationCheck_ && labelEnableDocOrientationCheck_->isChecked());
    models.insert(QStringLiteral("enable_doc_unwarping"), labelEnableDocUnwarpingCheck_ && labelEnableDocUnwarpingCheck_->isChecked());
    models.insert(QStringLiteral("prelabel_score_threshold"), prelabelScoreThreshold());
    context_->config.insert(QStringLiteral("local_models"), models);
    ProjectRepository::saveProject(*context_);
}

void MainWindow::setComboToPath(QComboBox* combo, const QString& path) {
    if (!combo) {
        return;
    }
    combo->blockSignals(true);
    if (path.trimmed().isEmpty()) {
        combo->setCurrentIndex(combo->count() > 0 ? 0 : -1);
        combo->blockSignals(false);
        return;
    }

    const QSet<QString> targets = equivalentModelPaths(path);
    for (int i = 0; i < combo->count(); ++i) {
        const QString value = combo->itemData(i).toString();
        if (!value.isEmpty()) {
            QSet<QString> candidatePaths = equivalentModelPaths(value);
            candidatePaths.intersect(targets);
            if (!candidatePaths.isEmpty()) {
                combo->setCurrentIndex(i);
                combo->blockSignals(false);
                return;
            }
        }
    }
    combo->setCurrentIndex(combo->count() > 0 ? 0 : -1);
    combo->blockSignals(false);
}

QString MainWindow::labelSelectedModelDir(QComboBox* labelCombo, QComboBox* fallbackCombo) const {
    const QString labelModel = selectedPredictionModelDir(labelCombo);
    if (!labelModel.isEmpty()) {
        return labelModel;
    }
    return selectedPredictionModelDir(fallbackCombo);
}

QString MainWindow::labelPrelabelDevice() const {
    if (labelUseGpuCheck_ && labelUseGpuCheck_->isChecked()) {
        return PaddleInferenceRuntime::gpuSupported() ? QStringLiteral("gpu:0") : QStringLiteral("cpu");
    }
    const QString predictionDevice = predictDeviceCombo_ ? predictDeviceCombo_->currentText().trimmed() : QString();
    if (predictionDevice.startsWith(QStringLiteral("gpu"), Qt::CaseInsensitive)
        && !PaddleInferenceRuntime::gpuSupported()) {
        return QStringLiteral("cpu");
    }
    return predictionDevice.isEmpty() ? QStringLiteral("cpu") : predictionDevice;
}

double MainWindow::prelabelScoreThreshold() const {
    if (prelabelScoreThresholdSpin_) {
        return prelabelScoreThresholdSpin_->value();
    }
    return predictScoreThresholdSpin_ ? predictScoreThresholdSpin_->value() : 0.50;
}

QStringList MainWindow::labelModelMissingReasons() const {
    QStringList missing;
    QString reason;
    const QString detDir = labelSelectedModelDir(labelDetModelCombo_, predictDetModelCombo_);
    if (!PaddleInferenceRuntime::modelDirLooksUsable(detDir, &reason)) {
        missing.append(QStringLiteral("Det 检测模型不可用：%1").arg(reason));
    }
    reason.clear();
    const QString recDir = labelSelectedModelDir(labelRecModelCombo_, predictRecModelCombo_);
    if (!PaddleInferenceRuntime::modelDirLooksUsable(recDir, &reason)) {
        missing.append(QStringLiteral("Rec 识别模型不可用：%1").arg(reason));
    }
    if (labelEnableTextlineClsCheck_ && labelEnableTextlineClsCheck_->isChecked()) {
        reason.clear();
        const QString clsDir = labelSelectedModelDir(labelTextlineClsModelCombo_, predictClsModelCombo_);
        if (!PaddleInferenceRuntime::modelDirLooksUsable(clsDir, &reason)) {
            missing.append(QStringLiteral("文本行方向模型不可用：%1").arg(reason));
        }
    }
    if (labelEnableDocOrientationCheck_ && labelEnableDocOrientationCheck_->isChecked()) {
        reason.clear();
        const QString docDir = labelSelectedModelDir(labelDocOrientationModelCombo_, predictClsModelCombo_);
        if (!PaddleInferenceRuntime::modelDirLooksUsable(docDir, &reason)) {
            missing.append(QStringLiteral("文档方向模型不可用：%1").arg(reason));
        }
    }
    if (labelEnableDocUnwarpingCheck_ && labelEnableDocUnwarpingCheck_->isChecked()) {
        reason.clear();
        const QString unwarpDir = selectedPredictionModelDir(labelDocUnwarpingModelCombo_);
        if (unwarpDir.isEmpty() || !PaddleInferenceRuntime::modelDirLooksUsable(unwarpDir, &reason)) {
            missing.append(QStringLiteral("文档矫正模型不可用：%1").arg(reason.isEmpty() ? QStringLiteral("未选择模型") : reason));
        }
    }
    const QString layoutDir = selectedPredictionModelDir(labelLayoutModelCombo_);
    if (!layoutDir.isEmpty()) {
        reason.clear();
        if (!PaddleDocLayoutEngine::modelDirLooksUsable(layoutDir, &reason)) {
            missing.append(QStringLiteral("Layout 版面模型不可用：%1").arg(reason));
        }
    }
    return missing;
}

void MainWindow::refreshImageLabelPanel() {
    fillComboFromLabelSet(layoutLabelCombo_, "layout");
    fillComboFromLabelSet(docOrientationCombo_, "doc_orientation");
    fillComboFromLabelSet(textlineOrientationCombo_, "textline_orientation");
    fillComboFromLabelSet(tableClassificationCombo_, "table_classification");
    setComboToValue(docOrientationCombo_, imageLabelValue("doc_orientation"));
    setComboToValue(textlineOrientationCombo_, imageLabelValue("textline_orientation"));
    setComboToValue(tableClassificationCombo_, imageLabelValue("table_classification"));
}

void MainWindow::newProjectDialog() {
    const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("选择项目目录"));
    if (path.isEmpty()) {
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("项目名称"), QStringLiteral("名称"), QLineEdit::Normal, QFileInfo(path).fileName(), &ok);
    if (!ok) {
        return;
    }
    setProject(ProjectRepository::createProject(path, name));
}

void MainWindow::openProjectDialog() {
    const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("打开项目目录"));
    if (!path.isEmpty()) {
        openProjectPath(path);
    }
}

void MainWindow::openRecentProjectFromCombo() {
    if (!recentProjectsCombo_ || recentProjectsCombo_->currentIndex() < 0) {
        QMessageBox::information(this, QStringLiteral("没有最近项目"), QStringLiteral("当前没有可打开的最近项目。"));
        return;
    }
    const QString path = recentProjectsCombo_->currentData().toString();
    if (path.isEmpty()) {
        return;
    }
    openProjectPath(path);
}

void MainWindow::savePageMetadata() {
    if (updatingPageMetadata_ || !context_ || currentAnnotation_.isEmpty()) {
        return;
    }
    const int row = pageList_ ? pageList_->currentRow() : -1;
    if (row < 0 || row >= pages_.size()) {
        return;
    }
    currentAnnotation_["split"] = pageSplitCombo_ ? comboStoredValue(pageSplitCombo_) : pages_.at(row).split;
    currentAnnotation_["status"] = pageStatusCombo_ ? comboStoredValue(pageStatusCombo_) : pages_.at(row).status;
    persistCurrentAnnotation();
    applyPageFilters();
    statusBar()->showMessage(QStringLiteral("页面元数据已保存"), 2000);
}

void MainWindow::importImagesDialog() {
    if (!context_) {
        QMessageBox::information(this, QStringLiteral("没有项目"), QStringLiteral("请先打开或创建项目。"));
        return;
    }
    const QStringList paths = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("导入图片"),
        QString(),
        "Images (*.jpg *.jpeg *.png *.bmp *.tif *.tiff);;All files (*.*)");
    if (paths.isEmpty()) {
        return;
    }
    try {
        const auto imported = AssetImporter::importPaths(*context_, paths);
        appendLog(QStringLiteral("已导入 %1 页").arg(imported.size()));
        refreshPages();
    } catch (const std::exception& exc) {
        QMessageBox::critical(this, QStringLiteral("导入失败"), QString::fromUtf8(exc.what()));
    }
}

void MainWindow::importPdfsDialog() {
    if (!context_) {
        QMessageBox::information(this, QStringLiteral("没有项目"), QStringLiteral("请先打开或创建项目。"));
        return;
    }
    const QStringList paths = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("导入PDF"),
        QString(),
        "PDF files (*.pdf);;All files (*.*)");
    if (paths.isEmpty()) {
        return;
    }
    try {
        const auto imported = AssetImporter::importPaths(*context_, paths);
        appendLog(QStringLiteral("已导入 %1 页 PDF").arg(imported.size()));
        refreshPages();
    } catch (const std::exception& exc) {
        QMessageBox::critical(this, QStringLiteral("导入PDF失败"), QString::fromUtf8(exc.what()));
    }
}

void MainWindow::validateProject() {
    if (!context_) {
        return;
    }
    const auto issues = Validator::validateProject(*context_);
    const QString logPath = Validator::writeValidationLog(*context_, issues);
    int errors = 0;
    int warnings = 0;
    for (const auto& issue : issues) {
        if (issue.severity == "error") {
            ++errors;
        } else if (issue.severity == "warning") {
            ++warnings;
        }
    }
    refreshValidationTable(issues, logPath);
    appendLog(QStringLiteral("校验：%1 个错误，%2 个警告").arg(errors).arg(warnings));
    statusBar()->showMessage(QStringLiteral("校验完成：%1 个错误，%2 个警告").arg(errors).arg(warnings), 5000);
}

void MainWindow::exportProject() {
    if (!context_) {
        return;
    }
    if (exportRunning_) {
        QMessageBox::information(this, QStringLiteral("导出进行中"), QStringLiteral("已有后台导出任务正在运行。"));
        return;
    }
    if (prelabelAllRunning_) {
        QMessageBox::information(this, QStringLiteral("预标注进行中"), QStringLiteral("后台预标注正在写入标注文件，请完成后再导出数据集。"));
        return;
    }

    QSet<QString> tasks;
    if (!exportDetCheck_ || exportDetCheck_->isChecked()) {
        tasks.insert(QStringLiteral("det"));
    }
    if (!exportRecCheck_ || exportRecCheck_->isChecked()) {
        tasks.insert(QStringLiteral("rec"));
    }
    if (!exportClsCheck_ || exportClsCheck_->isChecked()) {
        tasks.insert(QStringLiteral("cls"));
    }
    if (!exportTextlineClsCheck_ || exportTextlineClsCheck_->isChecked()) {
        tasks.insert(QStringLiteral("textline_cls"));
    }
    if (exportTableClsCheck_ && exportTableClsCheck_->isChecked()) {
        tasks.insert(QStringLiteral("table_cls"));
    }
    if (!exportCocoCheck_ || exportCocoCheck_->isChecked()) {
        tasks.insert(QStringLiteral("coco"));
    }
    if (tasks.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("没有导出任务"), QStringLiteral("请至少选择一个导出任务。"));
        return;
    }

    persistCurrentAnnotation();
    const bool checkedOnly = !exportCheckedOnlyCheck_ || exportCheckedOnlyCheck_->isChecked();
    const ProjectContext context = *context_;
    QPointer<MainWindow> window(this);
    auto* thread = QThread::create([window, context, tasks, checkedOnly]() {
        AsyncExportJobResult result;
        QElapsedTimer timer;
        timer.start();
        auto postProgress = [window](int current, int total, const QString& message) {
            if (!window) {
                return;
            }
            QMetaObject::invokeMethod(window.data(), [window, current, total, message]() {
                if (!window) {
                    return;
                }
                const int percent = total > 0
                    ? qBound(0, static_cast<int>((static_cast<double>(current) * 100.0) / total), 100)
                    : 0;
                if (window->exportProgressBar_) {
                    window->exportProgressBar_->setValue(percent);
                }
                if (window->exportProgressLabel_) {
                    window->exportProgressLabel_->setText(QStringLiteral("导出状态：%1 (%2%)").arg(message).arg(percent));
                }
                window->statusBar()->showMessage(QStringLiteral("%1 (%2%)").arg(message).arg(percent));
            }, Qt::QueuedConnection);
        };

        try {
            result.outputs = Exporter::exportSelected(context, tasks, checkedOnly, true, postProgress);
        } catch (const std::exception& exc) {
            result.error = QString::fromUtf8(exc.what());
            if (result.error.contains(QStringLiteral("validation"), Qt::CaseInsensitive)) {
                result.validationIssues = Validator::validateProject(context);
                result.validationLogPath = Validator::writeValidationLog(context, result.validationIssues);
            }
        }
        result.elapsedMs = timer.elapsed();

        if (!window) {
            return;
        }
        QMetaObject::invokeMethod(window.data(), [window, result]() {
            if (!window) {
                return;
            }
            window->exportRunning_ = false;
            if (window->exportProjectButton_) {
                window->exportProjectButton_->setEnabled(true);
                window->exportProjectButton_->setText(QStringLiteral("开始导出"));
            }
            if (!result.validationIssues.isEmpty() || !result.validationLogPath.isEmpty()) {
                window->refreshValidationTable(result.validationIssues, result.validationLogPath);
            }

            if (result.error.isEmpty()) {
                if (window->exportProgressBar_) {
                    window->exportProgressBar_->setValue(100);
                }
                if (window->exportProgressLabel_) {
                    window->exportProgressLabel_->setText(QStringLiteral("导出状态：完成，用时 %1 秒").arg(result.elapsedMs / 1000.0, 0, 'f', 1));
                }
                window->appendLog(QStringLiteral("已导出 %1 个数据集，用时 %2 秒")
                    .arg(result.outputs.size())
                    .arg(result.elapsedMs / 1000.0, 0, 'f', 1));
                window->refreshOverviewStats();
                window->refreshDatasetPage();
                window->refreshLogsPage();
                window->statusBar()->showMessage(QStringLiteral("导出完成，用时 %1 秒").arg(result.elapsedMs / 1000.0, 0, 'f', 1), 5000);
            } else {
                if (window->exportProgressLabel_) {
                    window->exportProgressLabel_->setText(QStringLiteral("导出状态：失败"));
                }
                window->appendLog(QStringLiteral("导出失败：%1").arg(result.error));
                window->refreshLogsPage();
                window->statusBar()->showMessage(QStringLiteral("导出失败"), 5000);
                QMessageBox::critical(window.data(), QStringLiteral("导出失败"), result.error);
            }
        }, Qt::QueuedConnection);
    });

    activeExportThread_ = thread;
    exportRunning_ = true;
    if (exportProjectButton_) {
        exportProjectButton_->setEnabled(false);
        exportProjectButton_->setText(QStringLiteral("导出中..."));
    }
    if (exportProgressBar_) {
        exportProgressBar_->setRange(0, 100);
        exportProgressBar_->setValue(0);
    }
    if (exportProgressLabel_) {
        exportProgressLabel_->setText(QStringLiteral("导出状态：准备导出"));
    }
    statusBar()->showMessage(QStringLiteral("导出任务已在后台启动"), 3000);
    connect(thread, &QThread::finished, this, [this, thread]() {
        if (activeExportThread_ == thread) {
            activeExportThread_ = nullptr;
        }
        thread->deleteLater();
    });
    thread->start();
}

void MainWindow::showEnvironmentReport() {
    EnvironmentReportOptions options;
    options.baseDir = baseDir_;
    options.pythonExe = paddlexPythonPath();
    const QJsonObject report = EnvironmentReport::build(options);
    const QString logRoot = context_ ? context_->logRoot() : QDir(baseDir_).filePath(QStringLiteral("logs"));
    QDir().mkpath(logRoot);
    const QString reportPath = QDir(logRoot).filePath(QStringLiteral("environment_report.json"));
    EnvironmentReport::writeJson(reportPath, report);
    const QString reportText = QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented));
    appendLog(reportText);
    if (settingsReportView_) {
        settingsReportView_->setPlainText(reportText);
    }
    refreshLogsPage();
    QMessageBox::information(
        this,
        QStringLiteral("环境"),
        QStringLiteral("环境报告已写入概览日志和：\n%1").arg(QDir::toNativeSeparators(reportPath)));
}

void MainWindow::openValidationLog() {
    QString path = lastValidationLogPath_;
    if (path.isEmpty() && context_) {
        path = QDir(context_->logRoot()).filePath(QStringLiteral("validate.log"));
    }
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        QMessageBox::information(this, QStringLiteral("校验日志"), QStringLiteral("请先运行校验。"));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

QString MainWindow::currentTrainingTaskKey() const {
    if (!trainingTaskCombo_) {
        return trainingTasks().first().key;
    }
    const QString key = trainingTaskCombo_->currentData().toString();
    return key.isEmpty() ? trainingTasks().first().key : key;
}

TrainingOptions MainWindow::trainingOptionsFromUi(const TrainingTaskSpec& task) const {
    TrainingOptions options;
    options.pythonExe = paddlexPythonPath();
    options.device = trainingDeviceCombo_ ? trainingDeviceCombo_->currentText().trimmed() : QStringLiteral("cpu");
    options.epochs = trainingEpochsSpin_ ? trainingEpochsSpin_->value() : task.epochs;
    options.batchSize = trainingBatchSpin_ ? trainingBatchSpin_->value() : task.batchSize;
    options.learningRate = trainingLearningRateSpin_ ? trainingLearningRateSpin_->value() : task.learningRate;
    options.numClasses = trainingNumClassesSpin_ ? trainingNumClassesSpin_->value() : 0;
    options.warmupSteps = trainingWarmupSpin_ ? trainingWarmupSpin_->value() : 0;
    options.resumePath = trainingResumePathEdit_ ? trainingResumePathEdit_->text().trimmed() : QString();
    return options;
}

PaddleCommand MainWindow::buildTrainingCommand(const QString& taskKey, const QString& outputDir) const {
    const auto task = trainingTaskByKey(taskKey);
    const TrainingOptions options = trainingOptionsFromUi(task);
    return TrainingPreflight::buildCommand(baseDir_, context_ ? &*context_ : nullptr, task.key, outputDir, options);
}

QString MainWindow::paddlexPythonPath() const {
    return RuntimePaths::defaultPaddlexPython();
}

void MainWindow::appendTrainingText(const QString& text) {
    if (text.isEmpty()) {
        return;
    }
    trainingLogBuffer_.append(text);
    if (!trainingPreview_) {
        return;
    }
    trainingPreview_->moveCursor(QTextCursor::End);
    trainingPreview_->insertPlainText(text);
    trainingPreview_->moveCursor(QTextCursor::End);
    if (logsViewer_ && logsSourceCombo_ && comboStoredValue(logsSourceCombo_) == QStringLiteral("training")) {
        logsViewer_->moveCursor(QTextCursor::End);
        logsViewer_->insertPlainText(text);
        logsViewer_->moveCursor(QTextCursor::End);
    }
}

void MainWindow::appendTrainingMetricsFromText(const QString& text) {
    if (!trainingMetricsTable_ || text.isEmpty()) {
        return;
    }
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("\\r?\\n")), Qt::SkipEmptyParts);
    QString latestScore;
    QString latestLoss;
    for (const auto& line : lines) {
        if (!lineLooksLikeTrainingMetric(line)) {
            continue;
        }
        const TrainingMetricRow parsed = parseTrainingMetricRow(line);
        if (parsed.loss.isEmpty()
            && parsed.lr.isEmpty()
            && parsed.accuracy.isEmpty()
            && parsed.score.isEmpty()
            && parsed.epoch.isEmpty()
            && parsed.step.isEmpty()) {
            continue;
        }
        const int row = trainingMetricsTable_->rowCount();
        trainingMetricsTable_->insertRow(row);
        const QStringList values{
            parsed.time,
            parsed.epoch,
            parsed.step,
            parsed.loss,
            parsed.lr,
            parsed.accuracy,
            parsed.score,
            parsed.precisionRecall,
            parsed.raw,
        };
        for (int column = 0; column < values.size(); ++column) {
            trainingMetricsTable_->setItem(row, column, new QTableWidgetItem(values.at(column)));
        }
        if (!parsed.score.isEmpty()) {
            latestScore = parsed.score;
        }
        if (!parsed.loss.isEmpty()) {
            latestLoss = parsed.loss;
        }
    }
    if (trainingMetricsTable_->rowCount() > 0) {
        trainingMetricsTable_->scrollToBottom();
        if (trainingMetricSummaryLabel_) {
            if (latestScore.isEmpty()) {
                for (int row = trainingMetricsTable_->rowCount() - 1; row >= 0 && latestScore.isEmpty(); --row) {
                    const auto* item = trainingMetricsTable_->item(row, 6);
                    latestScore = item ? item->text() : QString();
                }
            }
            if (latestLoss.isEmpty()) {
                for (int row = trainingMetricsTable_->rowCount() - 1; row >= 0 && latestLoss.isEmpty(); --row) {
                    const auto* item = trainingMetricsTable_->item(row, 3);
                    latestLoss = item ? item->text() : QString();
                }
            }
            trainingMetricSummaryLabel_->setText(QStringLiteral("指标：%1 行 | 最新 score %2 | 最新 loss %3")
                .arg(trainingMetricsTable_->rowCount())
                .arg(latestScore.isEmpty() ? QStringLiteral("--") : latestScore)
                .arg(latestLoss.isEmpty() ? QStringLiteral("--") : latestLoss));
        }
    }
    refreshTrainingMetricCharts();
}

void MainWindow::refreshTrainingMetricCharts() {
    auto setEmpty = [](TrainingPlaceholderChart* chart) {
        if (chart) {
            chart->setValues(QVector<double>());
        }
    };
    if (!trainingMetricsTable_) {
        setEmpty(trainingLossChart_);
        setEmpty(trainingAccuracyChart_);
        setEmpty(trainingLrChart_);
        return;
    }

    auto valuesForColumn = [this](int column) {
        QVector<double> values;
        const int startRow = qMax(0, trainingMetricsTable_->rowCount() - 300);
        const QRegularExpression numberPattern(QStringLiteral("[-+]?\\d+(?:\\.\\d+)?(?:[eE][-+]?\\d+)?"));
        for (int row = startRow; row < trainingMetricsTable_->rowCount(); ++row) {
            const auto* item = trainingMetricsTable_->item(row, column);
            if (!item) {
                continue;
            }
            QString text = item->text().trimmed();
            const QRegularExpressionMatch match = numberPattern.match(text);
            if (!match.hasMatch()) {
                continue;
            }
            bool ok = false;
            double value = match.captured(0).toDouble(&ok);
            if (!ok || !std::isfinite(value)) {
                continue;
            }
            if (text.contains('%')) {
                value /= 100.0;
            }
            values.append(value);
        }
        return values;
    };
    auto latestText = [](const QString& name, const QVector<double>& values) {
        return values.isEmpty()
            ? QString()
            : QStringLiteral("%1 %2").arg(name, QString::number(values.last(), 'g', 6));
    };

    const QVector<double> lossValues = valuesForColumn(3);
    const QVector<double> lrValues = valuesForColumn(4);
    QVector<double> metricValues = valuesForColumn(6);
    QString metricName = QStringLiteral("Score");
    if (metricValues.isEmpty()) {
        metricValues = valuesForColumn(5);
        metricName = QStringLiteral("Accuracy");
    }

    if (trainingLossChart_) {
        trainingLossChart_->setValues(lossValues, latestText(QStringLiteral("Loss"), lossValues));
    }
    if (trainingAccuracyChart_) {
        trainingAccuracyChart_->setValues(metricValues, latestText(metricName, metricValues));
    }
    if (trainingLrChart_) {
        trainingLrChart_->setValues(lrValues, latestText(QStringLiteral("LR"), lrValues));
    }
}

void MainWindow::setTrainingRunning(bool running) {
    const bool taskCanTrain = trainingTaskByKey(currentTrainingTaskKey()).trainSupported;
    if (startTrainingButton_) {
        startTrainingButton_->setEnabled(!running && taskCanTrain);
    }
    if (stopTrainingButton_) {
        stopTrainingButton_->setEnabled(running);
    }
    if (trainingHeaderStatusLabel_) {
        trainingHeaderStatusLabel_->setText(running ? QStringLiteral("状态：训练中") : QStringLiteral("状态：未启动"));
        trainingHeaderStatusLabel_->setObjectName(running ? "StatusWarn" : "StatusOk");
        trainingHeaderStatusLabel_->style()->unpolish(trainingHeaderStatusLabel_);
        trainingHeaderStatusLabel_->style()->polish(trainingHeaderStatusLabel_);
    }
    if (trainingProgressBar_ && running) {
        trainingProgressBar_->setValue(0);
    }
}

QJsonObject MainWindow::parseTrainingMetrics(const QString& logText) const {
    return TrainingRunner::parseMetrics(logText);
}

void MainWindow::refreshTrainingVersions() {
    if (!trainingVersionList_ || !trainingVersionSummary_) {
        refreshModelLibraryPage();
        return;
    }
    const QString previousSelection = selectedTrainingVersionId();
    QSignalBlocker blocker(trainingVersionList_);
    trainingVersionList_->clear();
    QString taskKey = currentTrainingTaskKey();
    if (trainingScreens_ && trainingScreens_->currentIndex() == 2) {
        const QString tableTaskKey = selectedVersionTableData(trainingVersionManagerTable_, Qt::UserRole);
        if (!tableTaskKey.isEmpty()) {
            taskKey = tableTaskKey;
        }
    }
    const auto task = trainingTaskByKey(taskKey);
    if (!context_) {
        trainingVersionSummary_->setText(task.trainSupported
                ? QStringLiteral("当前版本\n打开项目后显示训练版本")
                : QStringLiteral("该任务暂不支持训练"));
        if (trainingVersionDetail_) {
            trainingVersionDetail_->setPlainText(task.trainSupported
                    ? QStringLiteral("打开项目后显示训练版本")
                    : task.note);
        }
        refreshTrainingTaskOverview();
        refreshTrainingVersionManagerTable();
        refreshModelLibraryPage();
        return;
    }

    const QJsonObject manifest = TrainingRunStore::loadVersionManifest(*context_, task.key);
    const QJsonArray versions = manifest.value("versions").toArray();
    trainingVersionSummary_->setText(QStringLiteral("当前版本\n%1\n选中版本\n%2\n版本标签\n当前：%3  Best：%4")
        .arg(task.key,
            manifest.value("current_version_id").toString("-"),
            manifest.value("current_version_id").toString("-"),
            manifest.value("best_version_id").toString("-")));
    int selectedRow = -1;
    for (int i = versions.size() - 1; i >= 0; --i) {
        const QJsonObject version = versions.at(i).toObject();
        auto* item = new QListWidgetItem(versionListText(version), trainingVersionList_);
        item->setSizeHint(QSize(260, 82));
        const QString versionId = version.value("version_id").toString();
        item->setData(Qt::UserRole, versionId);
        item->setData(Qt::UserRole + 1, version.value("version_dir").toString(version.value("output_dir").toString()));
        if (versionId == previousSelection) {
            selectedRow = trainingVersionList_->count() - 1;
        }
    }
    blocker.unblock();
    if (trainingVersionList_->count() > 0) {
        trainingVersionList_->setCurrentRow(selectedRow >= 0 ? selectedRow : 0);
    } else {
        updateSelectedTrainingVersionDetails();
    }
    refreshTrainingTaskOverview();
    refreshTrainingVersionManagerTable();
    refreshModelLibraryPage();
}

void MainWindow::queueTrainingTaskOverviewRefresh() {
    if (trainingTaskOverviewRefreshQueued_) {
        return;
    }
    trainingTaskOverviewRefreshQueued_ = true;
    QTimer::singleShot(0, this, [this]() {
        trainingTaskOverviewRefreshQueued_ = false;
        if (!trainingTaskGrid_ || !trainingTaskHost_) {
            return;
        }
        refreshTrainingTaskOverview();
    });
}

void MainWindow::refreshTrainingTaskOverview() {
    if (!trainingTaskGrid_) {
        return;
    }

    struct TaskRow {
        TrainingTaskSpec task;
        QJsonObject manifest;
        QJsonObject latestRun;
        QJsonObject latestVersion;
        QJsonObject latestSuccess;
        QString statusKey;
        int versionCount = 0;
        int sampleCount = 0;
        QString metricText;
        QString modelDir;
        QDateTime time;
    };

    QList<TaskRow> allRows;
    for (const auto& task : trainingTasks()) {
        TaskRow row;
        row.task = task;
        if (context_) {
            row.manifest = TrainingRunStore::loadVersionManifest(*context_, task.key);
            row.latestRun = TrainingRunStore::latestRun(*context_, task.key);
        }
        const QJsonArray versions = row.manifest.value(QStringLiteral("versions")).toArray();
        row.versionCount = versions.size();
        row.latestVersion = latestObjectByTime(versions);
        row.latestSuccess = latestSuccessfulVersion(versions);
        row.statusKey = taskOverviewStatusKey(task, row.manifest, row.latestRun);
        row.sampleCount = row.latestVersion.value(QStringLiteral("sample_count")).toInt();
        if (row.sampleCount <= 0) {
            row.sampleCount = row.latestRun.value(QStringLiteral("sample_count")).toInt();
        }
        if (row.sampleCount <= 0 && context_) {
            row.sampleCount = datasetItemCount(QDir(context_->exportRoot()).filePath(task.datasetName));
        }
        row.metricText = versionMetricText(task, row.latestSuccess.isEmpty() ? row.latestVersion : row.latestSuccess);
        row.modelDir = inferenceDirFromVersion(task, row.latestSuccess.isEmpty() ? row.latestVersion : row.latestSuccess);
        row.time = storeObjectTime(row.latestVersion);
        if (!row.time.isValid()) {
            row.time = storeObjectTime(row.latestRun);
        }
        allRows.append(row);
    }

    int running = 0;
    int done = 0;
    int failed = 0;
    int today = 0;
    const QDate currentDate = QDate::currentDate();
    for (const auto& row : allRows) {
        if (row.statusKey == QStringLiteral("running")) {
            ++running;
        } else if (row.statusKey == QStringLiteral("success")) {
            ++done;
        } else if (row.statusKey == QStringLiteral("failed")) {
            ++failed;
        }
        if (row.time.isValid() && row.time.date() == currentDate) {
            ++today;
        }
    }
    if (trainingTotalTasksMetric_) {
        trainingTotalTasksMetric_->setText(QString::number(allRows.size()));
    }
    if (trainingRunningTasksMetric_) {
        trainingRunningTasksMetric_->setText(QString::number(running));
    }
    if (trainingDoneTasksMetric_) {
        trainingDoneTasksMetric_->setText(QString::number(done));
    }
    if (trainingFailedTasksMetric_) {
        trainingFailedTasksMetric_->setText(QString::number(failed));
    }
    if (trainingTodayTasksMetric_) {
        trainingTodayTasksMetric_->setText(QString::number(today));
    }

    const QString query = trainingTaskSearchEdit_ ? trainingTaskSearchEdit_->text().trimmed().toLower() : QString();
    const QString kindFilter = trainingTaskTypeFilter_ ? trainingTaskTypeFilter_->currentData().toString() : QString();
    const QString statusFilter = trainingTaskStatusFilter_ ? trainingTaskStatusFilter_->currentData().toString() : QString();
    QList<TaskRow> visibleRows;
    for (const auto& row : allRows) {
        if (!kindFilter.isEmpty() && row.task.kind != kindFilter) {
            continue;
        }
        if (!statusFilter.isEmpty() && row.statusKey != statusFilter) {
            continue;
        }
        if (!query.isEmpty()) {
            const QString haystack = QStringLiteral("%1 %2 %3 %4 %5")
                .arg(row.task.key, row.task.title, row.task.datasetName, row.task.note, row.modelDir)
                .toLower();
            if (!haystack.contains(query)) {
                continue;
            }
        }
        visibleRows.append(row);
    }

    const QString sortKey = trainingTaskSortCombo_ ? trainingTaskSortCombo_->currentData().toString() : QStringLiteral("recent");
    std::sort(visibleRows.begin(), visibleRows.end(), [sortKey](const TaskRow& left, const TaskRow& right) {
        if (sortKey == QStringLiteral("kind")) {
            if (left.task.kind != right.task.kind) {
                return left.task.kind < right.task.kind;
            }
            return left.task.title < right.task.title;
        }
        if (sortKey == QStringLiteral("name")) {
            return left.task.title < right.task.title;
        }
        if (left.time.isValid() != right.time.isValid()) {
            return left.time.isValid();
        }
        if (left.time.isValid() && left.time != right.time) {
            return left.time > right.time;
        }
        return left.task.title < right.task.title;
    });

    clearLayoutItems(trainingTaskGrid_);
    const int availableWidth = trainingTaskHost_ ? trainingTaskHost_->width() : 0;
    const int columns = trainingTaskOverviewColumnsForWidth(availableWidth);
    trainingTaskOverviewColumns_ = columns;
    for (int column = 0; column < 4; ++column) {
        trainingTaskGrid_->setColumnStretch(column, column < columns ? 1 : 0);
    }
    if (visibleRows.isEmpty()) {
        auto* empty = mutedLabel(QStringLiteral("没有匹配的训练任务"), trainingTaskHost_);
        empty->setAlignment(Qt::AlignCenter);
        empty->setMinimumHeight(180);
        trainingTaskGrid_->addWidget(empty, 0, 0);
        return;
    }

    for (int i = 0; i < visibleRows.size(); ++i) {
        const TaskRow row = visibleRows.at(i);
        auto* card = workbenchCard(trainingTaskHost_, QStringLiteral("TrainingTaskCard"));
        card->setMinimumHeight(300);
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 12, 12, 12);
        cardLayout->setSpacing(9);

        auto* top = new QHBoxLayout();
        top->setSpacing(8);
        auto* titleLabel = new QLabel(shortTaskTitle(row.task), card);
        titleLabel->setObjectName("TrainingCardTitle");
        titleLabel->setWordWrap(true);
        titleLabel->setToolTip(QStringLiteral("%1\n%2").arg(row.task.key, row.task.title));
        auto* badgeLabel = new QLabel(taskKindLabel(row.task.kind), card);
        if (row.task.kind == QStringLiteral("det")) {
            badgeLabel->setObjectName("TrainingBadgeDet");
        } else if (row.task.kind == QStringLiteral("rec")) {
            badgeLabel->setObjectName("TrainingBadgeRec");
        } else if (row.task.kind == QStringLiteral("layout")) {
            badgeLabel->setObjectName("TrainingBadgeLayout");
        } else {
            badgeLabel->setObjectName("TrainingBadgeCls");
        }
        top->addWidget(titleLabel, 1);
        top->addWidget(badgeLabel);
        cardLayout->addLayout(top);

        auto* datasetLabel = new QLabel(QStringLiteral("数据集：%1    任务 key：%2").arg(taskDatasetText(row.task), row.task.key), card);
        datasetLabel->setObjectName("TrainingCardStrip");
        datasetLabel->setToolTip(QStringLiteral("%1\n%2").arg(taskDatasetText(row.task), row.task.key));
        cardLayout->addWidget(datasetLabel);

        QString statusText = taskOverviewStatusLabel(row.statusKey);
        if (row.statusKey == QStringLiteral("idle") && row.sampleCount <= 0) {
            statusText = QStringLiteral("待导出");
        }
        auto* statusLabel = new QLabel(statusText, card);
        statusLabel->setObjectName(statusText == QStringLiteral("待导出")
                ? "TrainingStatusExport"
                : trainingStatusObjectName(row.statusKey).toUtf8().constData());
        statusLabel->setMinimumWidth(132);
        statusLabel->setMaximumWidth(150);
        statusLabel->setAlignment(Qt::AlignCenter);
        cardLayout->addWidget(statusLabel);

        const QString currentVersion = row.manifest.value(QStringLiteral("current_version_id")).toString(
                row.versionCount > 0 ? row.latestVersion.value(QStringLiteral("version_id")).toString(QStringLiteral("legacy"))
                                     : QStringLiteral("legacy"));
        const QString latestTime = shortTimeText(row.time);
        const QString metricText = row.metricText.isEmpty() ? QStringLiteral("-") : row.metricText;
        auto* detail = new QLabel(QStringLiteral("当前版本： %1\n最近训练： %2\n版本数量： %3\n训练样本： %4 条\n主要指标： %5")
                .arg(currentVersion,
                     latestTime,
                     QString::number(row.versionCount),
                     QString::number(row.sampleCount),
                     metricText),
                card);
        detail->setObjectName("TrainingCardInfo");
        detail->setWordWrap(true);
        cardLayout->addWidget(detail, 1);

        const QString outputName = row.latestVersion.value(QStringLiteral("version_dir")).toString().isEmpty()
                ? row.task.key
                : QFileInfo(row.latestVersion.value(QStringLiteral("version_dir")).toString()).fileName();
        auto* outputBox = new QWidget(card);
        outputBox->setObjectName("TrainingCardOutput");
        outputBox->setAttribute(Qt::WA_StyledBackground, true);
        auto* outputLayout = new QVBoxLayout(outputBox);
        outputLayout->setContentsMargins(10, 8, 10, 8);
        outputLayout->setSpacing(4);
        auto* outputCaption = new QLabel(QStringLiteral("输出目录"), outputBox);
        outputCaption->setObjectName("Muted");
        auto* outputValue = new QLabel(outputName.isEmpty() ? row.task.key : outputName, outputBox);
        outputValue->setObjectName("TrainingCardOutputValue");
        outputValue->setToolTip(row.latestVersion.value(QStringLiteral("version_dir")).toString(row.task.key));
        outputLayout->addWidget(outputCaption);
        outputLayout->addWidget(outputValue);
        cardLayout->addWidget(outputBox);

        auto* actionBox = new QWidget(card);
        actionBox->setObjectName("TrainingCardActionPanel");
        actionBox->setAttribute(Qt::WA_StyledBackground, true);
        auto* actions = new QHBoxLayout(actionBox);
        actions->setContentsMargins(10, 8, 10, 8);
        actions->addStretch(1);
        auto* openButton = primaryButton(QStringLiteral("进入详情  →"), actionBox);
        openButton->setObjectName("TrainingCardAction");
        connect(openButton, &QPushButton::clicked, this, [this, key = row.task.key]() {
            selectTrainingTaskAndShowDetail(key);
        });
        actions->addWidget(openButton);
        actions->addStretch(1);
        cardLayout->addWidget(actionBox);

        trainingTaskGrid_->addWidget(card, i / columns, i % columns);
    }
}

void MainWindow::refreshTrainingVersionManagerTable() {
    if (!trainingVersionManagerTable_) {
        return;
    }
    trainingVersionManagerTable_->setRowCount(0);
    if (!context_) {
        return;
    }

    struct VersionRow {
        TrainingTaskSpec task;
        QJsonObject version;
        QDateTime time;
    };
    QList<VersionRow> rows;
    for (const auto& task : trainingTasks()) {
        const QJsonObject manifest = TrainingRunStore::loadVersionManifest(*context_, task.key);
        const QJsonArray versions = manifest.value(QStringLiteral("versions")).toArray();
        for (const auto& value : versions) {
            VersionRow row;
            row.task = task;
            row.version = value.toObject();
            row.time = storeObjectTime(row.version);
            rows.append(row);
        }
    }
    std::sort(rows.begin(), rows.end(), [](const VersionRow& left, const VersionRow& right) {
        if (left.time.isValid() != right.time.isValid()) {
            return left.time.isValid();
        }
        if (left.time.isValid() && left.time != right.time) {
            return left.time > right.time;
        }
        return left.task.key < right.task.key;
    });

    for (int rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        const VersionRow row = rows.at(rowIndex);
        const QJsonObject version = row.version;
        const QString versionId = version.value(QStringLiteral("version_id")).toString();
        const QString dir = version.value(QStringLiteral("version_dir")).toString(version.value(QStringLiteral("output_dir")).toString());
        const QStringList values{
            row.task.title,
            versionId,
            statusLabelText(version.value(QStringLiteral("status")).toString()),
            version.value(QStringLiteral("is_current")).toBool() ? QStringLiteral("Yes") : QStringLiteral("No"),
            version.value(QStringLiteral("is_best")).toBool() ? QStringLiteral("Yes") : QStringLiteral("No"),
            version.value(QStringLiteral("started_at")).toString(),
            version.value(QStringLiteral("finished_at")).toString(),
            versionMetricText(row.task, version),
            QString::number(version.value(QStringLiteral("sample_count")).toInt()),
            QDir::toNativeSeparators(dir),
        };
        trainingVersionManagerTable_->insertRow(rowIndex);
        for (int column = 0; column < values.size(); ++column) {
            auto* item = new QTableWidgetItem(values.at(column));
            item->setData(Qt::UserRole, row.task.key);
            item->setData(Qt::UserRole + 1, versionId);
            item->setData(Qt::UserRole + 2, dir);
            trainingVersionManagerTable_->setItem(rowIndex, column, item);
        }
    }
}

void MainWindow::selectTrainingTaskAndShowDetail(const QString& taskKey) {
    if (trainingTaskCombo_) {
        const int index = trainingTaskCombo_->findData(taskKey);
        if (index >= 0) {
            trainingTaskCombo_->setCurrentIndex(index);
        }
    }
    if (trainingScreens_) {
        trainingScreens_->setCurrentIndex(1);
    }
}

void MainWindow::compareSelectedTrainingVersions() {
    if (!context_) {
        QMessageBox::information(this, QStringLiteral("没有项目"), QStringLiteral("请先打开或创建项目。"));
        return;
    }
    QString taskKey = currentTrainingTaskKey();
    if (trainingScreens_ && trainingScreens_->currentIndex() == 2) {
        const QString tableTaskKey = selectedVersionTableData(trainingVersionManagerTable_, Qt::UserRole);
        if (!tableTaskKey.isEmpty()) {
            taskKey = tableTaskKey;
        }
    }
    const auto task = trainingTaskByKey(taskKey);
    const QJsonObject manifest = TrainingRunStore::loadVersionManifest(*context_, task.key);
    const QString selectedId = selectedTrainingVersionId();
    auto findVersion = [&manifest](const QString& versionId) {
        for (const auto& value : manifest.value(QStringLiteral("versions")).toArray()) {
            const QJsonObject version = value.toObject();
            if (version.value(QStringLiteral("version_id")).toString() == versionId) {
                return version;
            }
        }
        return QJsonObject{};
    };
    auto summarize = [&task](const QJsonObject& version) {
        if (version.isEmpty()) {
            return QJsonObject{};
        }
        QString reason;
        const QString inferenceDir = inferenceDirFromVersion(task, version);
        return QJsonObject{
            {QStringLiteral("version_id"), version.value(QStringLiteral("version_id")).toString()},
            {QStringLiteral("status"), version.value(QStringLiteral("status")).toString()},
            {QStringLiteral("started_at"), version.value(QStringLiteral("started_at")).toString()},
            {QStringLiteral("finished_at"), version.value(QStringLiteral("finished_at")).toString()},
            {QStringLiteral("sample_count"), version.value(QStringLiteral("sample_count")).toInt()},
            {QStringLiteral("metric"), versionMetricText(task, version)},
            {QStringLiteral("best_weight_path"), version.value(QStringLiteral("best_weight_path")).toString()},
            {QStringLiteral("inference_model_dir"), inferenceDir},
            {QStringLiteral("inference_usable"), !inferenceDir.isEmpty() && PaddleInferenceRuntime::modelDirLooksUsable(inferenceDir, &reason)},
            {QStringLiteral("inference_error"), reason},
        };
    };
    const QJsonObject report{
        {QStringLiteral("task_key"), task.key},
        {QStringLiteral("selected"), summarize(findVersion(selectedId))},
        {QStringLiteral("current"), summarize(findVersion(manifest.value(QStringLiteral("current_version_id")).toString()))},
        {QStringLiteral("best"), summarize(findVersion(manifest.value(QStringLiteral("best_version_id")).toString()))},
    };
    if (trainingVersionDetail_) {
        trainingVersionDetail_->setPlainText(QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented)));
    }
    statusBar()->showMessage(QStringLiteral("训练版本对比已刷新"), 3000);
}

void MainWindow::browseTrainingResumePath() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择断点续训权重"),
        trainingResumePathEdit_ ? trainingResumePathEdit_->text() : QString(),
        "Paddle weights (*.pdparams *.pdopt *.pdmodel);;All files (*.*)");
    if (!path.isEmpty() && trainingResumePathEdit_) {
        trainingResumePathEdit_->setText(QFileInfo(path).absoluteFilePath());
        previewTrainingCommand();
    }
}

void MainWindow::browseTrainingExportWeight() {
    QString startPath = trainingExportWeightEdit_ ? trainingExportWeightEdit_->text() : QString();
    if (startPath.isEmpty()) {
        startPath = selectedTrainingVersionObject().value(QStringLiteral("best_weight_path")).toString();
    }
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择模型权重"),
        startPath,
        "Paddle weights (*.pdparams *.pdmodel);;All files (*.*)");
    if (!path.isEmpty() && trainingExportWeightEdit_) {
        trainingExportWeightEdit_->setText(QFileInfo(path).absoluteFilePath());
    }
}

void MainWindow::browseTrainingExportDir() {
    QString startPath = trainingExportDirEdit_ ? trainingExportDirEdit_->text() : QString();
    if (startPath.isEmpty() && context_) {
        startPath = context_->path(QStringLiteral("model_exports"));
    }
    const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("选择导出目录"), startPath);
    if (!path.isEmpty() && trainingExportDirEdit_) {
        trainingExportDirEdit_->setText(QFileInfo(path).absoluteFilePath());
    }
}

void MainWindow::exportSelectedTrainingModel() {
    if (!context_) {
        QMessageBox::information(this, QStringLiteral("没有项目"), QStringLiteral("请先打开或创建项目。"));
        return;
    }
    const QJsonObject version = selectedTrainingVersionObject();
    if (version.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("没有版本"), QStringLiteral("请先选择一个训练版本。"));
        return;
    }
    const QString taskKey = version.value(QStringLiteral("task_key")).toString(currentTrainingTaskKey());
    const auto task = trainingTaskByKey(taskKey);
    const QString versionId = version.value(QStringLiteral("version_id")).toString();
    const QString defaultExportDir = context_->path(QStringLiteral("model_exports/%1/%2").arg(task.key, versionId));
    const QString exportDir = trainingExportDirEdit_ && !trainingExportDirEdit_->text().trimmed().isEmpty()
        ? trainingExportDirEdit_->text().trimmed()
        : defaultExportDir;
    const QString weightPath = trainingExportWeightEdit_ && !trainingExportWeightEdit_->text().trimmed().isEmpty()
        ? trainingExportWeightEdit_->text().trimmed()
        : version.value(QStringLiteral("best_weight_path")).toString();
    const QString inferenceDir = inferenceDirFromVersion(task, version);

    if (weightPath.isEmpty() && inferenceDir.isEmpty()) {
        QMessageBox::warning(this, "No model output", "Selected version has no best weight or inference model directory yet.");
        return;
    }
    try {
        QDir().mkpath(exportDir);
        int copiedInferenceFiles = 0;
        QString copiedWeightPath;
        if (!weightPath.isEmpty() && QFileInfo(weightPath).isFile()) {
            const QString weightTargetDir = QDir(exportDir).filePath(QStringLiteral("weights"));
            QDir().mkpath(weightTargetDir);
            copiedWeightPath = QDir(weightTargetDir).filePath(QFileInfo(weightPath).fileName());
            QFile::remove(copiedWeightPath);
            if (!QFile::copy(weightPath, copiedWeightPath)) {
                throw std::runtime_error(QStringLiteral("failed to copy weight: %1").arg(weightPath).toStdString());
            }
        }
        if (!inferenceDir.isEmpty()) {
            copiedInferenceFiles = copyDirectoryContents(inferenceDir, QDir(exportDir).filePath(QStringLiteral("inference")));
        }
        QJsonObject manifest{
            {QStringLiteral("task_key"), task.key},
            {QStringLiteral("version_id"), versionId},
            {QStringLiteral("source_weight_path"), weightPath},
            {QStringLiteral("source_inference_dir"), inferenceDir},
            {QStringLiteral("export_dir"), QFileInfo(exportDir).absoluteFilePath()},
            {QStringLiteral("copied_weight_path"), copiedWeightPath},
            {QStringLiteral("copied_inference_files"), copiedInferenceFiles},
            {QStringLiteral("exported_at"), QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))},
            {QStringLiteral("version"), version},
        };
        QFile manifestFile(QDir(exportDir).filePath(QStringLiteral("export_manifest.json")));
        if (manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            manifestFile.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
        }
        appendLog(QStringLiteral("Exported model version %1 -> %2").arg(versionId, exportDir));
        QMessageBox::information(
            this,
            "Model exported",
            QString("Exported version %1.\nInference files: %2\nDirectory:\n%3")
                .arg(versionId)
                .arg(copiedInferenceFiles)
                .arg(QDir::toNativeSeparators(exportDir)));
    } catch (const std::exception& exc) {
        QMessageBox::warning(this, "Model export failed", exc.what());
    }
}

void MainWindow::applyTrainingTaskDefaults() {
    const auto task = trainingTaskByKey(currentTrainingTaskKey());
    if (trainingEpochsSpin_) {
        trainingEpochsSpin_->setValue(qMax(1, task.epochs));
    }
    if (trainingBatchSpin_) {
        trainingBatchSpin_->setValue(qMax(1, task.batchSize));
    }
    if (trainingLearningRateSpin_) {
        trainingLearningRateSpin_->setValue(task.learningRate > 0.0 ? task.learningRate : 0.001);
    }
    if (trainingNumClassesSpin_) {
        trainingNumClassesSpin_->setValue(qMax(0, task.numClasses));
    }
    if (trainingWarmupSpin_) {
        trainingWarmupSpin_->setValue(qMax(0, task.warmupSteps));
    }
    if (trainingProgressMetaLabel_) {
        trainingProgressMetaLabel_->setText(QStringLiteral("任务名称：%1        开始时间：-        当前轮数：0 / %2 (0%)        预计剩余时间：-        状态：未启动")
            .arg(task.key)
            .arg(qMax(1, task.epochs)));
    }
    if (trainingProgressBar_) {
        trainingProgressBar_->setValue(0);
    }
    setTrainingRunning(trainingProcess_ != nullptr);
}

QJsonObject MainWindow::selectedTrainingVersionObject() const {
    if (!context_) {
        return {};
    }
    const QString versionId = selectedTrainingVersionId();
    if (versionId.isEmpty()) {
        return {};
    }
    QString taskKey = currentTrainingTaskKey();
    if (trainingScreens_ && trainingScreens_->currentIndex() == 2) {
        const QString tableTaskKey = selectedVersionTableData(trainingVersionManagerTable_, Qt::UserRole);
        if (!tableTaskKey.isEmpty()) {
            taskKey = tableTaskKey;
        }
    }
    const QJsonObject manifest = TrainingRunStore::loadVersionManifest(*context_, taskKey);
    for (const auto& value : manifest.value(QStringLiteral("versions")).toArray()) {
        const QJsonObject version = value.toObject();
        if (version.value(QStringLiteral("version_id")).toString() == versionId) {
            return version;
        }
    }
    return {};
}

QString MainWindow::selectedTrainingVersionId() const {
    if (trainingScreens_ && trainingScreens_->currentIndex() == 2) {
        const QString versionId = selectedVersionTableData(trainingVersionManagerTable_, Qt::UserRole + 1);
        if (!versionId.isEmpty()) {
            return versionId;
        }
    }
    const auto* item = trainingVersionList_ ? trainingVersionList_->currentItem() : nullptr;
    return item ? item->data(Qt::UserRole).toString() : QString();
}

QString MainWindow::selectedTrainingVersionDir() const {
    if (trainingScreens_ && trainingScreens_->currentIndex() == 2) {
        const QString dir = selectedVersionTableData(trainingVersionManagerTable_, Qt::UserRole + 2);
        if (!dir.isEmpty()) {
            return dir;
        }
    }
    const auto* item = trainingVersionList_ ? trainingVersionList_->currentItem() : nullptr;
    return item ? item->data(Qt::UserRole + 1).toString() : QString();
}

void MainWindow::trainingTaskChanged() {
    applyTrainingTaskDefaults();
    refreshTrainingVersions();
    if (trainingPreview_) {
        trainingPreview_->clear();
    }
}

void MainWindow::updateSelectedTrainingVersionDetails() {
    if (!trainingVersionDetail_) {
        return;
    }
    const QJsonObject version = selectedTrainingVersionObject();
    if (version.isEmpty()) {
        trainingVersionDetail_->setPlainText(context_
                ? QStringLiteral("未选择版本")
                : QStringLiteral("打开项目后查看训练版本"));
        return;
    }

    QString taskKey = currentTrainingTaskKey();
    if (trainingScreens_ && trainingScreens_->currentIndex() == 2) {
        const QString tableTaskKey = selectedVersionTableData(trainingVersionManagerTable_, Qt::UserRole);
        if (!tableTaskKey.isEmpty()) {
            taskKey = tableTaskKey;
        }
    }
    const auto task = trainingTaskByKey(taskKey);
    QJsonObject detail = version;
    const QString resolvedInferenceDir = inferenceDirFromVersion(task, version);
    detail.insert(QStringLiteral("resolved_inference_model_dir"), resolvedInferenceDir);
    QString reason;
    const bool inferenceUsable = !resolvedInferenceDir.isEmpty()
        && PaddleInferenceRuntime::modelDirLooksUsable(resolvedInferenceDir, &reason);
    detail.insert(QStringLiteral("resolved_inference_model_usable"), inferenceUsable);
    detail.insert(QStringLiteral("resolved_inference_model_error"), reason);
    if (task.kind == QStringLiteral("det")) {
        detail.insert(QStringLiteral("currently_selected_prediction_model"), selectedPredictionModelDir(predictDetModelCombo_));
    } else if (task.kind == QStringLiteral("rec")) {
        detail.insert(QStringLiteral("currently_selected_prediction_model"), selectedPredictionModelDir(predictRecModelCombo_));
    } else if (task.kind == QStringLiteral("layout")) {
        detail.insert(QStringLiteral("currently_selected_prediction_model"), selectedPredictionModelDir(predictLayoutModelCombo_));
    } else {
        detail.insert(QStringLiteral("currently_selected_prediction_model"), selectedPredictionModelDir(predictClsModelCombo_));
    }
    trainingVersionDetail_->setPlainText(QString::fromUtf8(QJsonDocument(detail).toJson(QJsonDocument::Indented)));
}

void MainWindow::setSelectedTrainingVersionCurrent() {
    if (!context_) {
        QMessageBox::information(this, QStringLiteral("没有项目"), QStringLiteral("请先打开或创建项目。"));
        return;
    }
    const QString versionId = selectedTrainingVersionId();
    if (versionId.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("没有版本"), QStringLiteral("请先选择一个成功的训练版本。"));
        return;
    }
    QString taskKey = currentTrainingTaskKey();
    if (trainingScreens_ && trainingScreens_->currentIndex() == 2) {
        const QString tableTaskKey = selectedVersionTableData(trainingVersionManagerTable_, Qt::UserRole);
        if (!tableTaskKey.isEmpty()) {
            taskKey = tableTaskKey;
        }
    }
    const auto task = trainingTaskByKey(taskKey);
    try {
        TrainingRunStore::setCurrentVersion(*context_, task.key, versionId);
        refreshTrainingVersions();
        refreshPredictionModels();
        appendLog(QStringLiteral("已设为当前版本：%1").arg(versionId));
    } catch (const std::exception& exc) {
        QMessageBox::warning(this, QStringLiteral("设置当前版本失败"), QString::fromUtf8(exc.what()));
    }
}

void MainWindow::deleteSelectedTrainingVersion() {
    if (!context_) {
        QMessageBox::information(this, QStringLiteral("没有项目"), QStringLiteral("请先打开或创建项目。"));
        return;
    }
    const QString versionId = selectedTrainingVersionId();
    if (versionId.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("没有版本"), QStringLiteral("请先选择一个训练版本。"));
        return;
    }
    QString taskKey = currentTrainingTaskKey();
    if (trainingScreens_ && trainingScreens_->currentIndex() == 2) {
        const QString tableTaskKey = selectedVersionTableData(trainingVersionManagerTable_, Qt::UserRole);
        if (!tableTaskKey.isEmpty()) {
            taskKey = tableTaskKey;
        }
    }
    const auto task = trainingTaskByKey(taskKey);
    if (QMessageBox::question(
            this,
            QStringLiteral("删除训练版本"),
            QStringLiteral("删除版本 %1 以及它的文件？").arg(versionId))
        != QMessageBox::Yes) {
        return;
    }

    try {
        TrainingRunStore::deleteVersion(*context_, task.key, versionId);
    } catch (const std::exception& exc) {
        const QString message = QString::fromLocal8Bit(exc.what());
        if (!message.contains(QStringLiteral("best version deletion requires confirmation"))) {
            QMessageBox::warning(this, QStringLiteral("删除版本失败"), message);
            return;
        }
        if (QMessageBox::warning(
                this,
                QStringLiteral("删除最佳版本"),
                QStringLiteral("版本 %1 被标记为 best，仍然删除？").arg(versionId),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No)
            != QMessageBox::Yes) {
            return;
        }
        try {
            TrainingRunStore::deleteVersion(*context_, task.key, versionId, true, true);
        } catch (const std::exception& confirmedExc) {
            QMessageBox::warning(this, QStringLiteral("删除版本失败"), QString::fromUtf8(confirmedExc.what()));
            return;
        }
    }

    refreshTrainingVersions();
    refreshPredictionModels();
    appendLog(QStringLiteral("已删除训练版本：%1").arg(versionId));
}

void MainWindow::openSelectedTrainingVersionDir() {
    const QString dir = selectedTrainingVersionDir();
    if (dir.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("没有版本"), QStringLiteral("请先选择一个训练版本。"));
        return;
    }
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void MainWindow::checkTrainingSetup() {
    if (!context_) {
        QMessageBox::information(this, QStringLiteral("没有项目"), QStringLiteral("请先打开或创建项目。"));
        return;
    }

    const auto task = trainingTaskByKey(currentTrainingTaskKey());
    const TrainingOptions options = trainingOptionsFromUi(task);
    const auto result = TrainingPreflight::run(baseDir_, *context_, task.key, options);
    if (trainingPreview_) {
        trainingPreview_->setPlainText(QString::fromUtf8(QJsonDocument(result.report).toJson(QJsonDocument::Indented)));
    }
    appendLog(QStringLiteral("训练前检查 %1：%2").arg(task.key, result.ok ? QStringLiteral("通过") : QStringLiteral("失败")));
    if (result.ok) {
        QMessageBox::information(this, QStringLiteral("训练前检查"), QStringLiteral("训练前检查通过。"));
    } else {
        QMessageBox::warning(this, QStringLiteral("训练前检查"), QStringLiteral("发现 %1 个问题，请查看训练日志。").arg(result.errors.size()));
    }
}

void MainWindow::previewTrainingCommand() {
    const auto task = trainingTaskByKey(currentTrainingTaskKey());
    const QString previewVersionId = TrainingRunStore::newVersionId(task.key);
    const QString outputDir = context_
        ? TrainingRunStore::versionDir(*context_, task.key, previewVersionId)
        : QDir(baseDir_).filePath(QString("training/%1/versions/%2").arg(task.key, previewVersionId));
    const auto command = buildTrainingCommand(task.key, outputDir);
    trainingPreview_->setPlainText(QString(
        "任务：%1\n"
        "版本：%2\n"
        "工作目录：%3\n"
        "数据集：%4\n"
        "输出：%5\n\n"
        "%6")
            .arg(task.key,
                previewVersionId,
                command.workingDirectory,
                context_ ? QDir(context_->exportRoot()).filePath(task.datasetName) : QDir(baseDir_).filePath(QString("exports/%1").arg(task.datasetName)),
                outputDir,
                command.displayText()));
}

void MainWindow::copyTrainingCommand() {
    const auto task = trainingTaskByKey(currentTrainingTaskKey());
    const QString previewVersionId = TrainingRunStore::newVersionId(task.key);
    const QString outputDir = context_
        ? TrainingRunStore::versionDir(*context_, task.key, previewVersionId)
        : QDir(baseDir_).filePath(QString("training/%1/versions/%2").arg(task.key, previewVersionId));
    const auto command = buildTrainingCommand(task.key, outputDir);
    QApplication::clipboard()->setText(command.displayText());
    statusBar()->showMessage(QStringLiteral("训练命令已复制"), 2500);
}

void MainWindow::exportTrainingMetricsCsv() {
    if (!trainingMetricsTable_ || trainingMetricsTable_->rowCount() == 0) {
        QMessageBox::information(this, QStringLiteral("没有指标"), QStringLiteral("当前还没有捕获到训练指标。"));
        return;
    }
    const QString defaultPath = context_
        ? context_->path(QStringLiteral("training_metrics.csv"))
        : QDir(baseDir_).filePath(QStringLiteral("training_metrics.csv"));
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出训练指标 CSV"), defaultPath, "CSV (*.csv)");
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法写入：\n%1").arg(path));
        return;
    }
    auto csvCell = [](QString value) {
        value.replace('"', QStringLiteral("\"\""));
        return QStringLiteral("\"%1\"").arg(value);
    };
    QStringList headers;
    for (int column = 0; column < trainingMetricsTable_->columnCount(); ++column) {
        const auto* header = trainingMetricsTable_->horizontalHeaderItem(column);
        headers.append(csvCell(header ? header->text() : QString::number(column)));
    }
    file.write(headers.join(',').toUtf8());
    file.write("\n");
    for (int row = 0; row < trainingMetricsTable_->rowCount(); ++row) {
        QStringList cells;
        for (int column = 0; column < trainingMetricsTable_->columnCount(); ++column) {
            const auto* item = trainingMetricsTable_->item(row, column);
            cells.append(csvCell(item ? item->text() : QString()));
        }
        file.write(cells.join(',').toUtf8());
        file.write("\n");
    }
    statusBar()->showMessage(QStringLiteral("指标已导出：%1").arg(path), 4000);
}

void MainWindow::clearTrainingMetrics() {
    if (trainingMetricsTable_) {
        trainingMetricsTable_->setRowCount(0);
    }
    if (trainingMetricSummaryLabel_) {
        trainingMetricSummaryLabel_->setText(QStringLiteral("指标：0 行 | 最新 score -- | 最新 loss --"));
    }
    refreshTrainingMetricCharts();
}

void MainWindow::startTraining() {
    if (!context_) {
        QMessageBox::information(this, QStringLiteral("没有项目"), QStringLiteral("请先打开或创建项目。"));
        return;
    }
    if (trainingProcess_) {
        QMessageBox::information(this, QStringLiteral("训练进行中"), QStringLiteral("请先停止当前训练进程。"));
        return;
    }

    const auto task = trainingTaskByKey(currentTrainingTaskKey());
    if (!task.trainSupported) {
        QMessageBox::information(
            this,
            QStringLiteral("暂不支持训练"),
            task.note.isEmpty() ? QStringLiteral("当前任务暂不支持训练：%1").arg(task.key) : task.note);
        return;
    }
    try {
        trainingPreview_->clear();
        trainingLogBuffer_.clear();
        clearTrainingMetrics();
        appendTrainingText(QStringLiteral("正在准备已检查数据集和训练版本：%1...\n").arg(task.title));
        const TrainingOptions options = trainingOptionsFromUi(task);
        activeTrainingStart_ = TrainingRunner::prepare(baseDir_, *context_, task.key, options);
        if (!activeTrainingStart_.ok) {
            throw std::runtime_error(activeTrainingStart_.errors.join(QStringLiteral("; ")).toStdString());
        }

        activeTrainingTaskKey_ = activeTrainingStart_.task.key;
        activeTrainingTaskKind_ = activeTrainingStart_.task.kind;
        activeTrainingVersionId_ = activeTrainingStart_.versionId;
        activeTrainingVersionDir_ = activeTrainingStart_.versionDir;
        activeTrainingRunId_ = activeTrainingStart_.runId;
        const auto command = activeTrainingStart_.preflight.command;

        trainingStopRequested_ = false;
        trainingFinalized_ = false;
        trainingProcess_ = new QProcess(this);
        trainingProcess_->setProgram(command.program);
        trainingProcess_->setArguments(command.arguments);
        trainingProcess_->setWorkingDirectory(command.workingDirectory);
        trainingProcess_->setProcessEnvironment(command.environment);
        trainingProcess_->setProcessChannelMode(QProcess::SeparateChannels);
        connect(trainingProcess_, &QProcess::readyReadStandardOutput, this, &MainWindow::handleTrainingOutput);
        connect(trainingProcess_, &QProcess::readyReadStandardError, this, &MainWindow::handleTrainingOutput);
        connect(trainingProcess_, &QProcess::finished, this, &MainWindow::handleTrainingFinished);
        connect(trainingProcess_, &QProcess::errorOccurred, this, &MainWindow::handleTrainingError);

        appendTrainingText(QStringLiteral("任务：%1\n运行：%2\n版本：%3\n数据集：%4\n输出：%5\n\n%6\n\n")
            .arg(activeTrainingTaskKey_,
                activeTrainingRunId_,
                activeTrainingVersionId_,
                activeTrainingStart_.preflight.datasetDir,
                activeTrainingVersionDir_,
                command.displayText()));
        setTrainingRunning(true);
        refreshTrainingVersions();
        trainingProcess_->start();
        statusBar()->showMessage(QStringLiteral("PaddleX 训练已启动"), 3000);
    } catch (const std::exception& exc) {
        QMessageBox::critical(this, QStringLiteral("训练启动失败"), QString::fromUtf8(exc.what()));
        appendTrainingText(QStringLiteral("启动失败：%1\n").arg(QString::fromUtf8(exc.what())));
        setTrainingRunning(false);
        if (trainingProcess_) {
            trainingProcess_->deleteLater();
            trainingProcess_ = nullptr;
        }
    }
}

void MainWindow::stopTraining() {
    if (!trainingProcess_) {
        return;
    }
    trainingStopRequested_ = true;
    appendTrainingText(QStringLiteral("\n正在停止训练...\n"));
    trainingProcess_->terminate();
    if (!trainingProcess_->waitForFinished(3000)) {
        trainingProcess_->kill();
    }
}

void MainWindow::handleTrainingOutput() {
    auto* process = qobject_cast<QProcess*>(sender());
    if (!process) {
        process = trainingProcess_;
    }
    if (!process) {
        return;
    }
    const QString stdoutText = QString::fromUtf8(process->readAllStandardOutput());
    const QString stderrText = QString::fromUtf8(process->readAllStandardError());
    appendTrainingText(stdoutText);
    appendTrainingMetricsFromText(stdoutText);
    appendTrainingText(stderrText);
    appendTrainingMetricsFromText(stderrText);
}

void MainWindow::handleTrainingFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    handleTrainingOutput();
    const bool ok = exitStatus == QProcess::NormalExit && exitCode == 0;
    const QString status = trainingStopRequested_ ? QString("stopped") : (ok ? QString("success") : QString("failed"));
    const QString errorSummary = status == "success"
        ? QString()
        : (trainingStopRequested_ ? QString("user stopped training") : QString("PaddleX exited with code %1").arg(exitCode));
    finalizeTrainingRun(status, exitCode, errorSummary);
}

void MainWindow::handleTrainingError(QProcess::ProcessError error) {
    if (!trainingProcess_ || error != QProcess::FailedToStart) {
        return;
    }
    const QString errorSummary = trainingProcess_->errorString();
    appendTrainingText(QString("\nFailed to start PaddleX: %1\n").arg(errorSummary));
    finalizeTrainingRun("failed", -1, errorSummary);
}

void MainWindow::finalizeTrainingRun(const QString& status, int exitCode, const QString& errorSummary) {
    if (trainingFinalized_) {
        return;
    }
    trainingFinalized_ = true;
    const TrainingRunResult result = context_
        ? TrainingRunner::finish(*context_, activeTrainingStart_, status, exitCode, errorSummary, trainingLogBuffer_)
        : TrainingRunResult{};
    const QJsonObject metrics = result.metrics;
    const QJsonObject finishedVersion = result.finishedVersion;
    const QString statusText = statusLabelText(status);
    appendTrainingText(QStringLiteral("\n训练状态：%1，退出码：%2\n").arg(statusText).arg(exitCode));
    if (!metrics.isEmpty()) {
        appendTrainingText(QString("Metrics: %1\n").arg(QString::fromUtf8(QJsonDocument(metrics).toJson(QJsonDocument::Compact))));
    }
    if (!finishedVersion.isEmpty()) {
        const QString bestWeight = finishedVersion.value("best_weight_path").toString();
        const QString inferenceDir = finishedVersion.value("inference_model_dir").toString();
        if (!bestWeight.isEmpty()) {
            appendTrainingText(QStringLiteral("最佳权重：%1\n").arg(bestWeight));
        }
        if (!inferenceDir.isEmpty()) {
            appendTrainingText(QStringLiteral("推理模型：%1\n").arg(inferenceDir));
        } else if (status == "success") {
            appendTrainingText(QStringLiteral("推理模型：当前版本下暂未找到，预测会继续使用默认模型或此前导出的模型。\n"));
        }
    }
    appendLog(QStringLiteral("训练%1：%2").arg(statusText, activeTrainingVersionId_));
    statusBar()->showMessage(QStringLiteral("PaddleX 训练%1").arg(statusText), 5000);

    if (trainingProcess_) {
        trainingProcess_->deleteLater();
        trainingProcess_ = nullptr;
    }
    activeTrainingRunId_.clear();
    activeTrainingTaskKey_.clear();
    activeTrainingTaskKind_.clear();
    activeTrainingVersionId_.clear();
    activeTrainingVersionDir_.clear();
    activeTrainingStart_ = TrainingRunStart{};
    trainingStopRequested_ = false;
    setTrainingRunning(false);
    if (trainingHeaderStatusLabel_) {
        if (status == QStringLiteral("success")) {
            trainingHeaderStatusLabel_->setText(QStringLiteral("状态：已完成"));
            trainingHeaderStatusLabel_->setObjectName("StatusOk");
        } else if (status == QStringLiteral("stopped")) {
            trainingHeaderStatusLabel_->setText(QStringLiteral("状态：已停止"));
            trainingHeaderStatusLabel_->setObjectName("StatusWarn");
        } else {
            trainingHeaderStatusLabel_->setText(QStringLiteral("状态：失败"));
            trainingHeaderStatusLabel_->setObjectName("TrainingStatusFailed");
        }
        trainingHeaderStatusLabel_->style()->unpolish(trainingHeaderStatusLabel_);
        trainingHeaderStatusLabel_->style()->polish(trainingHeaderStatusLabel_);
    }
    refreshTrainingVersions();
    refreshPredictionModels();
    refreshOverviewStats();
}

void MainWindow::runInferenceSmoke() {
    const QString imagePath = QDir(baseDir_).filePath("PaddleOCR/deploy/lite/imgs/lite_demo.png");
    const QString outputDir = QDir(baseDir_).filePath("build_vs2026/ocr_output");
    const auto report = PaddleOcrEngine::predictImage(imagePath, outputDir, PaddleOcrEngine::defaultConfig(baseDir_));
    predictionPreview_->setPlainText(QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented)));
    updatePredictionImagePreview(outputDir);
}

QString MainWindow::defaultPredictionOutputDir() const {
    return context_ ? context_->path("predictions/ocr") : QDir(baseDir_).filePath("build_vs2026/ocr_output");
}

QString MainWindow::selectedPredictionModelDir(QComboBox* combo) const {
    if (!combo || combo->currentIndex() < 0) {
        return {};
    }
    return combo->currentData().toString();
}

QString MainWindow::firstPredictionPreviewImage(const QString& path) const {
    const QFileInfo info(path);
    if (info.isFile() && suffixMatches(info.absoluteFilePath(), imageFileSuffixes())) {
        return info.absoluteFilePath();
    }
    if (info.isDir()) {
        const QStringList images = filesWithSuffixes(info.absoluteFilePath(), imageFileSuffixes());
        if (!images.isEmpty()) {
            return QFileInfo(images.first()).absoluteFilePath();
        }
    }
    return {};
}

void MainWindow::updatePredictionImagePreview(const QString& path) {
    if (!predictionImagePreview_) {
        return;
    }
    const QString imagePath = firstPredictionPreviewImage(path);
    predictionImagePreview_->clear();
    predictionImagePreview_->setAlignment(Qt::AlignCenter);
    if (imagePath.isEmpty()) {
        predictionImagePreview_->setText(QStringLiteral("无预览"));
        predictionImagePreview_->setMinimumSize(360, 240);
        predictionImagePreview_->setToolTip({});
        if (predictionImagePathLabel_) {
            predictionImagePathLabel_->setText(path.trimmed().isEmpty()
                    ? QStringLiteral("未选择预览图片")
                    : QStringLiteral("在 %1 下未找到可预览图片").arg(QDir::toNativeSeparators(path)));
            predictionImagePathLabel_->setToolTip(path);
        }
        return;
    }

    QPixmap pixmap(imagePath);
    if (pixmap.isNull()) {
        predictionImagePreview_->setText(QStringLiteral("预览不可用"));
        predictionImagePreview_->setMinimumSize(360, 240);
        predictionImagePreview_->setToolTip(imagePath);
        if (predictionImagePathLabel_) {
            predictionImagePathLabel_->setText(QStringLiteral("预览不可用：%1").arg(QDir::toNativeSeparators(imagePath)));
            predictionImagePathLabel_->setToolTip(imagePath);
        }
        return;
    }

    const QSize maxSize(1100, 720);
    if (pixmap.width() > maxSize.width() || pixmap.height() > maxSize.height()) {
        pixmap = pixmap.scaled(maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    predictionImagePreview_->setPixmap(pixmap);
    predictionImagePreview_->setMinimumSize(pixmap.size().expandedTo(QSize(360, 240)));
    predictionImagePreview_->setToolTip(imagePath);
    if (predictionImagePathLabel_) {
        predictionImagePathLabel_->setText(QDir::toNativeSeparators(imagePath));
        predictionImagePathLabel_->setToolTip(imagePath);
    }
}

void MainWindow::refreshPredictionModels() {
    if (!predictDetModelCombo_ || !predictRecModelCombo_) {
        return;
    }
    const auto config = PaddleOcrEngine::defaultConfig(baseDir_);
    addPredictionModelChoices(predictDetModelCombo_, context_ ? &(*context_) : nullptr, tasksForKind(QStringLiteral("det")), config.textDetectionModelDir);
    addPredictionModelChoices(predictRecModelCombo_, context_ ? &(*context_) : nullptr, tasksForKind(QStringLiteral("rec")), config.textRecognitionModelDir);
    if (predictClsModelCombo_ && predictClsTaskCombo_) {
        const QString clsTask = predictClsTaskCombo_->currentData().toString();
        const auto clsConfig = PaddleClsEngine::defaultConfig(clsTask, baseDir_);
        addPredictionModelChoices(
            predictClsModelCombo_,
            context_ ? &(*context_) : nullptr,
            classificationTasksForPrediction(clsTask),
            clsConfig.modelDir);
    }
    if (predictLayoutModelCombo_) {
        const auto layoutConfig = PaddleDocLayoutEngine::defaultConfig(baseDir_);
        addPredictionModelChoices(predictLayoutModelCombo_, context_ ? &(*context_) : nullptr, tasksForKind(QStringLiteral("layout")), layoutConfig.modelDir);
    }
    refreshLabelModelChoices();
    loadLabelModelConfig();
    refreshPredictionModelCards();
    updatePredictionActiveModelSummary();
}

void MainWindow::refreshPredictionModelCards() {
    if (!predictionModelLayout_) {
        return;
    }

    struct ModelRow {
        QString kind;
        QString title;
        QString path;
        bool selected = false;
    };

    QList<ModelRow> rows;
    QSet<QString> added;
    auto addComboRows = [&rows, &added](QComboBox* combo, const QString& kind) {
        if (!combo) {
            return;
        }
        const QString selectedPath = combo->currentData().toString();
        for (int i = 0; i < combo->count(); ++i) {
            const QString path = combo->itemData(i).toString();
            if (path.isEmpty()) {
                continue;
            }
            const QString key = kind + QChar('|') + path;
            if (added.contains(key)) {
                continue;
            }
            added.insert(key);
            rows.append(ModelRow{kind, combo->itemText(i), path, path == selectedPath});
        }
    };
    addComboRows(predictDetModelCombo_, QStringLiteral("det"));
    addComboRows(predictRecModelCombo_, QStringLiteral("rec"));
    addComboRows(predictClsModelCombo_, QStringLiteral("cls"));
    addComboRows(predictLayoutModelCombo_, QStringLiteral("layout"));

    const QString kindFilter = predictionModelKindFilter_ ? predictionModelKindFilter_->currentData().toString() : QString();
    const QString query = predictionModelSearchEdit_ ? predictionModelSearchEdit_->text().trimmed().toLower() : QString();
    QList<ModelRow> visible;
    for (const auto& row : rows) {
        if (!kindFilter.isEmpty() && row.kind != kindFilter) {
            continue;
        }
        if (!query.isEmpty()) {
            const QString haystack = QStringLiteral("%1 %2 %3").arg(row.kind, row.title, row.path).toLower();
            if (!haystack.contains(query)) {
                continue;
            }
        }
        visible.append(row);
    }

    std::sort(visible.begin(), visible.end(), [](const ModelRow& left, const ModelRow& right) {
        if (left.kind != right.kind) {
            return left.kind < right.kind;
        }
        return left.title < right.title;
    });

    clearLayoutItems(predictionModelLayout_);
    if (visible.isEmpty()) {
        auto* empty = mutedLabel(QStringLiteral("No usable model choices matched the filters."), predictionModelHost_);
        empty->setMinimumHeight(86);
        predictionModelLayout_->addWidget(empty);
        predictionModelLayout_->addStretch(1);
        predictionModelHost_->adjustSize();
        return;
    }

    for (const auto& row : visible) {
        auto* tile = workbenchCard(predictionModelHost_, QStringLiteral("PredictionModelTile"));
        tile->setFixedWidth(205);
        tile->setMinimumHeight(92);
        tile->setMaximumHeight(112);
        auto* tileLayout = new QVBoxLayout(tile);
        tileLayout->setContentsMargins(10, 8, 10, 8);
        tileLayout->setSpacing(5);

        auto* top = new QHBoxLayout();
        auto* nameLabel = new QLabel(row.title, tile);
        nameLabel->setObjectName("TrainingTaskTitle");
        nameLabel->setWordWrap(true);
        nameLabel->setMaximumHeight(36);
        auto* badgeLabel = new QLabel(taskKindLabel(row.kind), tile);
        badgeLabel->setObjectName("TrainingBadge");
        top->addWidget(nameLabel, 1);
        top->addWidget(badgeLabel);
        tileLayout->addLayout(top);

        auto* pathLabel = mutedLabel(QDir::toNativeSeparators(row.path), tile);
        pathLabel->setWordWrap(false);
        pathLabel->setMaximumHeight(18);
        pathLabel->setToolTip(QDir::toNativeSeparators(row.path));
        tileLayout->addWidget(pathLabel, 1);

        QString reason;
        const bool usable = row.kind == QStringLiteral("layout")
            ? PaddleDocLayoutEngine::modelDirLooksUsable(row.path, &reason)
            : PaddleInferenceRuntime::modelDirLooksUsable(row.path, &reason);
        auto* status = new QLabel(
            row.selected
                ? QStringLiteral("Selected")
                : (usable ? QStringLiteral("Ready") : QStringLiteral("Missing files")),
            tile);
        status->setObjectName(usable ? "TrainingStatusDone" : "TrainingStatusWarn");
        if (!reason.isEmpty()) {
            status->setToolTip(reason);
        }
        tileLayout->addWidget(status);

        auto* useButton = row.selected
            ? new QPushButton(QStringLiteral("Using"), tile)
            : primaryButton(QStringLiteral("Use"), tile);
        useButton->setMaximumHeight(28);
        useButton->setEnabled(!row.selected);
        connect(useButton, &QPushButton::clicked, this, [this, kind = row.kind, path = row.path]() {
            selectPredictionModel(kind, path);
        });
        tileLayout->addWidget(useButton);
        predictionModelLayout_->addWidget(tile);
    }
    predictionModelLayout_->addStretch(1);
    predictionModelHost_->adjustSize();
}

void MainWindow::selectPredictionModel(const QString& kind, const QString& modelDir) {
    if (kind == QStringLiteral("det")) {
        setComboToPath(predictDetModelCombo_, modelDir);
    } else if (kind == QStringLiteral("rec")) {
        setComboToPath(predictRecModelCombo_, modelDir);
    } else if (kind == QStringLiteral("cls")) {
        setComboToPath(predictClsModelCombo_, modelDir);
    } else if (kind == QStringLiteral("layout")) {
        setComboToPath(predictLayoutModelCombo_, modelDir);
    }
    updatePredictionActiveModelSummary();
    refreshPredictionModelCards();
}

void MainWindow::updatePredictionActiveModelSummary() {
    if (!predictionActiveModelTitleLabel_ || !predictionActiveModelPathLabel_) {
        return;
    }
    auto modelName = [this](QComboBox* combo) {
        const QString path = selectedPredictionModelDir(combo);
        if (!path.isEmpty()) {
            return modelChoiceTitleFromPath(path);
        }
        QString text = combo && combo->currentIndex() >= 0 ? combo->currentText() : QStringLiteral("-");
        text.replace(QStringLiteral(" | 本地"), QString());
        return text;
    };
    auto modelPath = [this](const QString& label, QComboBox* combo) {
        const QString path = selectedPredictionModelDir(combo);
        return QStringLiteral("%1: %2").arg(label, path.isEmpty() ? QStringLiteral("-") : QDir::toNativeSeparators(path));
    };
    const QString mode = predictModeCombo_ ? predictModeCombo_->currentData().toString() : QStringLiteral("ocr");
    QString title;
    QString body;
    if (mode == QStringLiteral("classification")) {
        title = QStringLiteral("分类模型");
        body = QStringLiteral("%1\n任务：%2")
                .arg(modelName(predictClsModelCombo_),
                     predictClsTaskCombo_ ? predictClsTaskCombo_->currentText() : QStringLiteral("-"));
    } else if (mode == QStringLiteral("layout")) {
        title = QStringLiteral("版面模型");
        body = modelName(predictLayoutModelCombo_);
    } else {
        title = QStringLiteral("OCR 模型组合");
        body = QStringLiteral("Det：%1\nRec：%2")
                .arg(modelName(predictDetModelCombo_),
                     modelName(predictRecModelCombo_));
    }
    predictionActiveModelTitleLabel_->setText(title);
    predictionActiveModelPathLabel_->setText(body);
    predictionActiveModelPathLabel_->setToolTip(QStringLiteral("%1\n%2\n%3\n%4")
            .arg(modelPath(QStringLiteral("Det"), predictDetModelCombo_),
                 modelPath(QStringLiteral("Rec"), predictRecModelCombo_),
                 modelPath(QStringLiteral("Cls"), predictClsModelCombo_),
                 modelPath(QStringLiteral("Layout"), predictLayoutModelCombo_)));
}

void MainWindow::updatePredictionModeButtons() {
    const QString mode = predictModeCombo_ ? predictModeCombo_->currentData().toString() : QStringLiteral("ocr");
    if (startPredictionButton_) {
        if (mode == QStringLiteral("classification")) {
            startPredictionButton_->setText(QStringLiteral("开始分类预测"));
        } else if (mode == QStringLiteral("layout")) {
            startPredictionButton_->setText(QStringLiteral("开始版面预测"));
        } else {
            startPredictionButton_->setText(QStringLiteral("开始 OCR 预测"));
        }
    }
    updatePredictionActiveModelSummary();
}

void MainWindow::appendPredictionText(const QString& text) {
    if (text.isEmpty()) {
        return;
    }
    predictionLogBuffer_.append(text);
    if (!predictionPreview_) {
        return;
    }
    predictionPreview_->moveCursor(QTextCursor::End);
    predictionPreview_->insertPlainText(text);
    predictionPreview_->moveCursor(QTextCursor::End);
    if (logsViewer_ && logsSourceCombo_ && comboStoredValue(logsSourceCombo_) == QStringLiteral("prediction")) {
        logsViewer_->moveCursor(QTextCursor::End);
        logsViewer_->insertPlainText(text);
        logsViewer_->moveCursor(QTextCursor::End);
    }
}

void MainWindow::setPredictionRunning(bool running) {
    if (startPredictionButton_) {
        startPredictionButton_->setEnabled(!running);
    }
    if (startClsPredictionButton_) {
        startClsPredictionButton_->setEnabled(!running);
        startClsPredictionButton_->setVisible(false);
    }
    if (startLayoutPredictionButton_) {
        startLayoutPredictionButton_->setEnabled(!running);
        startLayoutPredictionButton_->setVisible(false);
    }
    if (stopPredictionButton_) {
        stopPredictionButton_->setEnabled(running);
        stopPredictionButton_->setVisible(running);
    }
    if (predictSaveJsonCheck_) {
        predictSaveJsonCheck_->setEnabled(!running);
    }
    if (predictSaveVisualCheck_) {
        predictSaveVisualCheck_->setEnabled(!running);
    }
}

bool MainWindow::preparePredictionRun(const QString& kind, QString* inputPath, QString* publishDir, QString* stagingDir) {
    const QString input = predictInputEdit_ ? predictInputEdit_->text().trimmed() : QString();
    const QString output = predictOutputEdit_ ? predictOutputEdit_->text().trimmed() : QString();
    if (input.isEmpty() || !QFileInfo::exists(input)) {
        QMessageBox::warning(this, QStringLiteral("缺少输入"), QStringLiteral("请先选择存在的图片文件或图片目录。"));
        return false;
    }
    if (output.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("缺少输出目录"), QStringLiteral("请先选择预测输出目录。"));
        return false;
    }
    if (!QDir().mkpath(output)) {
        QMessageBox::warning(this, QStringLiteral("输出目录不可用"), QStringLiteral("无法创建输出目录：\n%1").arg(output));
        return false;
    }

    const QString stagingRoot = context_
        ? context_->path(QStringLiteral("cache/prediction_staging"))
        : QDir(baseDir_).filePath(QStringLiteral("build_vs2026/prediction_staging"));
    QString safeKind = kind.toLower();
    safeKind.replace(QRegularExpression(QStringLiteral("[^a-z0-9_]+")), QStringLiteral("_"));
    const QString name = QStringLiteral("%1_%2")
        .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz")), safeKind);
    const QString staging = QDir(stagingRoot).filePath(name);
    if (!QDir().mkpath(staging)) {
        QMessageBox::warning(this, QStringLiteral("暂存目录不可用"), QStringLiteral("无法创建预测暂存目录：\n%1").arg(staging));
        return false;
    }

    activePredictionStagingDir_ = QFileInfo(staging).absoluteFilePath();
    activePredictionPublishDir_ = QFileInfo(output).absoluteFilePath();
    activePredictionPublishJson_ = !predictSaveJsonCheck_ || predictSaveJsonCheck_->isChecked();
    activePredictionPublishVisual_ = !predictSaveVisualCheck_ || predictSaveVisualCheck_->isChecked();
    predictionTimer_.restart();
    if (predictionStructuredText_) {
        predictionStructuredText_->clear();
    }
    if (predictionResultTotalLabel_) {
        predictionResultTotalLabel_->setText(QStringLiteral("Rows: 0"));
    }
    if (predictionResultConfidenceLabel_) {
        predictionResultConfidenceLabel_->setText(QStringLiteral("Average score: --"));
    }
    if (predictionElapsedLabel_) {
        predictionElapsedLabel_->setText(QStringLiteral("Elapsed: -"));
    }

    if (inputPath) {
        *inputPath = QFileInfo(input).absoluteFilePath();
    }
    if (publishDir) {
        *publishDir = activePredictionPublishDir_;
    }
    if (stagingDir) {
        *stagingDir = activePredictionStagingDir_;
    }
    return true;
}

QPair<int, int> MainWindow::publishPredictionOutputs(const QString& stagingDir, const QString& publishDir) const {
    int jsonCount = 0;
    int visualCount = 0;
    if (!QFileInfo(stagingDir).isDir()) {
        return {jsonCount, visualCount};
    }
    if (!QDir().mkpath(publishDir)) {
        throw std::runtime_error(QStringLiteral("failed to create publish directory: %1").arg(publishDir).toStdString());
    }
    if (activePredictionPublishJson_) {
        for (const auto& path : filesWithSuffixes(stagingDir, {QStringLiteral(".json")})) {
            copyPreservingRelativePath(stagingDir, publishDir, path);
            ++jsonCount;
        }
    }
    if (activePredictionPublishVisual_) {
        for (const auto& path : filesWithSuffixes(stagingDir, imageFileSuffixes())) {
            copyPreservingRelativePath(stagingDir, publishDir, path);
            ++visualCount;
        }
    }
    return {jsonCount, visualCount};
}

void MainWindow::summarizePredictionOutputs(
    const QString& stagingDir,
    const QString& publishDir,
    const QPair<int, int>& published,
    qint64 elapsedMs) {
    const QList<PredictionRow> rows = collectPredictionRows(stagingDir);
    double scoreSum = 0.0;
    int scoreCount = 0;
    if (predictionResultTable_) {
        predictionResultTable_->setRowCount(0);
        for (const auto& item : rows) {
            const int row = predictionResultTable_->rowCount();
            predictionResultTable_->insertRow(row);
            predictionResultTable_->setItem(row, 0, new QTableWidgetItem(item.kind));
            predictionResultTable_->setItem(row, 1, new QTableWidgetItem(item.label));
            predictionResultTable_->setItem(row, 2, new QTableWidgetItem(item.scoreText));
            predictionResultTable_->setItem(row, 3, new QTableWidgetItem(item.source));
            if (item.hasScore) {
                scoreSum += item.score;
                ++scoreCount;
            }
        }
        predictionResultTable_->resizeColumnsToContents();
    } else {
        for (const auto& item : rows) {
            if (item.hasScore) {
                scoreSum += item.score;
                ++scoreCount;
            }
        }
    }

    if (predictionStructuredText_) {
        QJsonArray rowArray;
        for (const auto& item : rows) {
            QJsonObject rowObject{
                {QStringLiteral("kind"), item.kind},
                {QStringLiteral("label"), item.label},
                {QStringLiteral("score_text"), item.scoreText},
                {QStringLiteral("source"), item.source},
            };
            if (item.hasScore) {
                rowObject.insert(QStringLiteral("score"), item.score);
            }
            rowArray.append(rowObject);
        }
        QJsonObject summary{
            {QStringLiteral("generated_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
            {QStringLiteral("staging_dir"), QDir::toNativeSeparators(stagingDir)},
            {QStringLiteral("publish_dir"), QDir::toNativeSeparators(publishDir)},
            {QStringLiteral("published_json"), published.first},
            {QStringLiteral("published_visuals"), published.second},
            {QStringLiteral("elapsed_ms"), static_cast<double>(elapsedMs)},
            {QStringLiteral("result_count"), rows.size()},
            {QStringLiteral("rows"), rowArray},
        };
        summary.insert(
            QStringLiteral("average_score"),
            scoreCount > 0 ? QJsonValue(scoreSum / scoreCount) : QJsonValue(QJsonValue::Null));
        predictionStructuredText_->setPlainText(QString::fromUtf8(QJsonDocument(summary).toJson(QJsonDocument::Indented)));
    }

    if (predictionResultTotalLabel_) {
        predictionResultTotalLabel_->setText(QStringLiteral("结果行：%1").arg(rows.size()));
    }
    if (predictionResultConfidenceLabel_) {
        predictionResultConfidenceLabel_->setText(scoreCount > 0
                ? QStringLiteral("平均分：%1").arg(scoreSum / scoreCount, 0, 'f', 4)
                : QStringLiteral("平均分：--"));
    }
    if (predictionElapsedLabel_) {
        predictionElapsedLabel_->setText(elapsedMs >= 0
                ? QStringLiteral("耗时：%1 ms").arg(elapsedMs)
                : QStringLiteral("耗时：--"));
    }
    appendPredictionText(QStringLiteral(
        "\n[workbench] staging: %1\n"
        "[workbench] publish: %2\n"
        "[workbench] published json: %3\n"
        "[workbench] published visuals: %4\n"
        "[workbench] summary rows: %5\n")
        .arg(stagingDir, publishDir)
        .arg(published.first)
        .arg(published.second)
        .arg(rows.size()));

    const QString stagedVisual = firstPredictionPreviewImage(stagingDir);
    if (!stagedVisual.isEmpty()) {
        QString previewPath = stagedVisual;
        if (activePredictionPublishVisual_ && published.second > 0) {
            const QString relative = QDir(stagingDir).relativeFilePath(stagedVisual);
            const QString publishedVisual = QDir(publishDir).filePath(relative);
            if (QFileInfo::exists(publishedVisual)) {
                previewPath = publishedVisual;
            }
        }
        updatePredictionImagePreview(previewPath);
    } else if (predictInputEdit_) {
        updatePredictionImagePreview(predictInputEdit_->text().trimmed());
    }
}

void MainWindow::browsePredictInputFile() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择预测图片"),
        predictInputEdit_ ? predictInputEdit_->text() : QString(),
        "Images (*.jpg *.jpeg *.png *.bmp *.tif *.tiff)");
    if (!path.isEmpty() && predictInputEdit_) {
        predictInputEdit_->setText(path);
        updatePredictionImagePreview(path);
    }
}

void MainWindow::browsePredictInputDir() {
    const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("选择预测图片目录"), predictInputEdit_ ? predictInputEdit_->text() : QString());
    if (!path.isEmpty() && predictInputEdit_) {
        predictInputEdit_->setText(path);
        updatePredictionImagePreview(path);
    }
}

void MainWindow::pastePredictImageFromClipboard() {
    auto* clipboard = QApplication::clipboard();
    if (!clipboard || !predictInputEdit_) {
        return;
    }

    QString imagePath;
    const QMimeData* mimeData = clipboard->mimeData();
    if (mimeData && mimeData->hasUrls()) {
        for (const QUrl& url : mimeData->urls()) {
            if (!url.isLocalFile()) {
                continue;
            }
            const QString candidate = url.toLocalFile();
            if (suffixMatches(candidate, imageFileSuffixes()) && QFileInfo(candidate).isFile()) {
                imagePath = QFileInfo(candidate).absoluteFilePath();
                break;
            }
        }
    }

    if (imagePath.isEmpty()) {
        const QImage image = clipboard->image();
        if (image.isNull()) {
            QMessageBox::information(this, QStringLiteral("剪贴板没有图片"), QStringLiteral("请先复制图片或图片文件。"));
            return;
        }
        const QString pasteRoot = context_
            ? context_->path(QStringLiteral("cache/prediction_clipboard"))
            : QDir(baseDir_).filePath(QStringLiteral("build_vs2026/prediction_clipboard"));
        if (!QDir().mkpath(pasteRoot)) {
            QMessageBox::warning(this, QStringLiteral("剪贴板缓存不可用"), QStringLiteral("无法创建剪贴板缓存目录：\n%1").arg(pasteRoot));
            return;
        }
        imagePath = QDir(pasteRoot).filePath(QStringLiteral("paste_%1.png")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz"))));
        if (!image.save(imagePath, "PNG")) {
            QMessageBox::warning(this, QStringLiteral("剪贴板保存失败"), QStringLiteral("无法保存剪贴板图片：\n%1").arg(imagePath));
            return;
        }
    }

    predictInputEdit_->setText(imagePath);
    updatePredictionImagePreview(imagePath);
    statusBar()->showMessage(QStringLiteral("已从剪贴板设置预测输入"), 3000);
}

void MainWindow::browsePredictOutputDir() {
    const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("选择预测输出目录"), predictOutputEdit_ ? predictOutputEdit_->text() : QString());
    if (!path.isEmpty() && predictOutputEdit_) {
        predictOutputEdit_->setText(path);
    }
}

void MainWindow::openPredictOutputDir() {
    if (!predictOutputEdit_) {
        return;
    }
    const QString outputDir = predictOutputEdit_->text().trimmed();
    if (outputDir.isEmpty()) {
        return;
    }
    QDir().mkpath(outputDir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(outputDir));
}

void MainWindow::startSelectedPredictionMode() {
    const QString mode = predictModeCombo_ ? predictModeCombo_->currentData().toString() : QStringLiteral("ocr");
    if (mode == QStringLiteral("classification")) {
        startClassificationPrediction();
    } else if (mode == QStringLiteral("layout")) {
        startLayoutPrediction();
    } else {
        startPrediction();
    }
}

void MainWindow::startPrediction() {
    if (predictionProcess_) {
        QMessageBox::information(this, QStringLiteral("预测正在运行"), QStringLiteral("请先停止当前预测进程。"));
        return;
    }
    QString inputPath;
    QString publishDir;
    QString stagingDir;
    if (!preparePredictionRun(QStringLiteral("ocr"), &inputPath, &publishDir, &stagingDir)) {
        return;
    }
    auto cancelPreparedRun = [this]() {
        if (!activePredictionStagingDir_.isEmpty()) {
            QDir(activePredictionStagingDir_).removeRecursively();
        }
        activePredictionStagingDir_.clear();
        activePredictionPublishDir_.clear();
    };

    const QString detModelDir = selectedPredictionModelDir(predictDetModelCombo_);
    const QString recModelDir = selectedPredictionModelDir(predictRecModelCombo_);
    QString reason;
    if (!PaddleInferenceRuntime::modelDirLooksUsable(detModelDir, &reason)) {
        QMessageBox::warning(this, "Det model invalid", reason);
        cancelPreparedRun();
        return;
    }
    if (!PaddleInferenceRuntime::modelDirLooksUsable(recModelDir, &reason)) {
        QMessageBox::warning(this, "Rec model invalid", reason);
        cancelPreparedRun();
        return;
    }

    const QString toolPath = QDir(QCoreApplication::applicationDirPath()).filePath("ppocr_ocr_predict.exe");
    if (!QFileInfo::exists(toolPath)) {
        QMessageBox::critical(this, "Prediction tool missing", QString("Cannot find %1").arg(toolPath));
        cancelPreparedRun();
        return;
    }

    QStringList arguments{
        "--input", inputPath,
        "--output", stagingDir,
        "--det-model-dir", detModelDir,
        "--rec-model-dir", recModelDir,
        "--device", predictDeviceCombo_ ? predictDeviceCombo_->currentText() : QString("cpu"),
        "--score-threshold", QString::number(predictScoreThresholdSpin_ ? predictScoreThresholdSpin_->value() : 0.0, 'g', 4),
    };
    if (predictSaveVisualCheck_ && !predictSaveVisualCheck_->isChecked()) {
        arguments.append("--no-visual");
    }

    predictionLogBuffer_.clear();
    if (predictionPreview_) {
        predictionPreview_->clear();
    }
    if (predictionResultTable_) {
        predictionResultTable_->setRowCount(0);
    }
    appendPredictionText(QString("%1 %2\n\n").arg(toolPath, arguments.join(' ')));
    appendPredictionText(QStringLiteral("[workbench] staging output: %1\n[workbench] publish output: %2\n[workbench] save_json=%3 save_visual=%4\n\n")
        .arg(stagingDir, publishDir, boolText(activePredictionPublishJson_), boolText(activePredictionPublishVisual_)));
    predictionProcess_ = new QProcess(this);
    predictionProcess_->setProgram(toolPath);
    predictionProcess_->setArguments(arguments);
    predictionProcess_->setWorkingDirectory(baseDir_);
    predictionProcess_->setProcessChannelMode(QProcess::MergedChannels);
    connect(predictionProcess_, &QProcess::readyReadStandardOutput, this, &MainWindow::handlePredictionOutput);
    connect(predictionProcess_, &QProcess::finished, this, &MainWindow::handlePredictionFinished);
    connect(predictionProcess_, &QProcess::errorOccurred, this, &MainWindow::handlePredictionError);
    setPredictionRunning(true);
    if (predictionStatusLabel_) {
        predictionStatusLabel_->setText(QStringLiteral("OCR 预测运行中"));
    }
    predictionProcess_->start();
    statusBar()->showMessage(QStringLiteral("C++ OCR 预测已启动"), 3000);
}

void MainWindow::startClassificationPrediction() {
    if (predictionProcess_) {
        QMessageBox::information(this, QStringLiteral("预测正在运行"), QStringLiteral("请先停止当前预测进程。"));
        return;
    }
    QString inputPath;
    QString publishDir;
    QString stagingDir;
    if (!preparePredictionRun(QStringLiteral("classification"), &inputPath, &publishDir, &stagingDir)) {
        return;
    }
    auto cancelPreparedRun = [this]() {
        if (!activePredictionStagingDir_.isEmpty()) {
            QDir(activePredictionStagingDir_).removeRecursively();
        }
        activePredictionStagingDir_.clear();
        activePredictionPublishDir_.clear();
    };

    const QString clsTask = predictClsTaskCombo_ ? predictClsTaskCombo_->currentData().toString() : QString("doc_orientation");
    const QString modelDir = selectedPredictionModelDir(predictClsModelCombo_);
    QString reason;
    if (!PaddleInferenceRuntime::modelDirLooksUsable(modelDir, &reason)) {
        QMessageBox::warning(this, "Cls model invalid", reason);
        cancelPreparedRun();
        return;
    }

    const QString toolPath = QDir(QCoreApplication::applicationDirPath()).filePath("ppocr_cls_predict.exe");
    if (!QFileInfo::exists(toolPath)) {
        QMessageBox::critical(this, "Classification tool missing", QString("Cannot find %1").arg(toolPath));
        cancelPreparedRun();
        return;
    }

    QStringList arguments{
        "--task", clsTask,
        "--input", inputPath,
        "--output", stagingDir,
        "--model-dir", modelDir,
        "--device", predictDeviceCombo_ ? predictDeviceCombo_->currentText() : QString("cpu"),
    };
    if (predictSaveVisualCheck_ && !predictSaveVisualCheck_->isChecked()) {
        arguments.append("--no-visual");
    }

    predictionLogBuffer_.clear();
    if (predictionPreview_) {
        predictionPreview_->clear();
    }
    if (predictionResultTable_) {
        predictionResultTable_->setRowCount(0);
    }
    appendPredictionText(QString("%1 %2\n\n").arg(toolPath, arguments.join(' ')));
    appendPredictionText(QStringLiteral("[workbench] staging output: %1\n[workbench] publish output: %2\n[workbench] save_json=%3 save_visual=%4\n\n")
        .arg(stagingDir, publishDir, boolText(activePredictionPublishJson_), boolText(activePredictionPublishVisual_)));
    predictionProcess_ = new QProcess(this);
    predictionProcess_->setProgram(toolPath);
    predictionProcess_->setArguments(arguments);
    predictionProcess_->setWorkingDirectory(baseDir_);
    predictionProcess_->setProcessChannelMode(QProcess::MergedChannels);
    connect(predictionProcess_, &QProcess::readyReadStandardOutput, this, &MainWindow::handlePredictionOutput);
    connect(predictionProcess_, &QProcess::finished, this, &MainWindow::handlePredictionFinished);
    connect(predictionProcess_, &QProcess::errorOccurred, this, &MainWindow::handlePredictionError);
    setPredictionRunning(true);
    if (predictionStatusLabel_) {
        predictionStatusLabel_->setText(QStringLiteral("分类预测运行中"));
    }
    predictionProcess_->start();
    statusBar()->showMessage(QStringLiteral("C++ 分类预测已启动"), 3000);
}

void MainWindow::startLayoutPrediction() {
    if (predictionProcess_) {
        QMessageBox::information(this, QStringLiteral("预测正在运行"), QStringLiteral("请先停止当前预测进程。"));
        return;
    }
    QString inputPath;
    QString publishDir;
    QString stagingDir;
    if (!preparePredictionRun(QStringLiteral("layout"), &inputPath, &publishDir, &stagingDir)) {
        return;
    }
    auto cancelPreparedRun = [this]() {
        if (!activePredictionStagingDir_.isEmpty()) {
            QDir(activePredictionStagingDir_).removeRecursively();
        }
        activePredictionStagingDir_.clear();
        activePredictionPublishDir_.clear();
    };

    const QString modelDir = selectedPredictionModelDir(predictLayoutModelCombo_);
    QString reason;
    if (!PaddleDocLayoutEngine::modelDirLooksUsable(modelDir, &reason)) {
        QMessageBox::warning(this, "Layout model invalid", reason);
        cancelPreparedRun();
        return;
    }

    const QString toolPath = QDir(QCoreApplication::applicationDirPath()).filePath("ppocr_layout_predict.exe");
    if (!QFileInfo::exists(toolPath)) {
        QMessageBox::critical(this, "Layout tool missing", QString("Cannot find %1").arg(toolPath));
        cancelPreparedRun();
        return;
    }

    QStringList arguments{
        "--base-dir", baseDir_,
        "--input", inputPath,
        "--output", stagingDir,
        "--model-dir", modelDir,
        "--device", predictDeviceCombo_ ? predictDeviceCombo_->currentText() : QString("cpu"),
        "--threshold", QString::number(predictScoreThresholdSpin_ ? predictScoreThresholdSpin_->value() : 0.5, 'g', 4),
    };
    if (predictSaveVisualCheck_ && !predictSaveVisualCheck_->isChecked()) {
        arguments.append("--no-visual");
    }

    predictionLogBuffer_.clear();
    if (predictionPreview_) {
        predictionPreview_->clear();
    }
    if (predictionResultTable_) {
        predictionResultTable_->setRowCount(0);
    }
    appendPredictionText(QString("%1 %2\n\n").arg(toolPath, arguments.join(' ')));
    appendPredictionText(QStringLiteral("[workbench] staging output: %1\n[workbench] publish output: %2\n[workbench] save_json=%3 save_visual=%4\n\n")
        .arg(stagingDir, publishDir, boolText(activePredictionPublishJson_), boolText(activePredictionPublishVisual_)));
    predictionProcess_ = new QProcess(this);
    predictionProcess_->setProgram(toolPath);
    predictionProcess_->setArguments(arguments);
    predictionProcess_->setWorkingDirectory(baseDir_);
    predictionProcess_->setProcessChannelMode(QProcess::MergedChannels);
    connect(predictionProcess_, &QProcess::readyReadStandardOutput, this, &MainWindow::handlePredictionOutput);
    connect(predictionProcess_, &QProcess::finished, this, &MainWindow::handlePredictionFinished);
    connect(predictionProcess_, &QProcess::errorOccurred, this, &MainWindow::handlePredictionError);
    setPredictionRunning(true);
    if (predictionStatusLabel_) {
        predictionStatusLabel_->setText(QStringLiteral("版面预测运行中"));
    }
    predictionProcess_->start();
    statusBar()->showMessage(QStringLiteral("版面预测已启动"), 3000);
}

void MainWindow::stopPrediction() {
    if (!predictionProcess_) {
        return;
    }
    appendPredictionText(QStringLiteral("\n正在停止预测...\n"));
    predictionProcess_->terminate();
    if (!predictionProcess_->waitForFinished(3000)) {
        predictionProcess_->kill();
    }
}

void MainWindow::handlePredictionOutput() {
    auto* process = qobject_cast<QProcess*>(sender());
    if (!process) {
        process = predictionProcess_;
    }
    if (!process) {
        return;
    }
    appendPredictionText(QString::fromUtf8(process->readAllStandardOutput()));
}

void MainWindow::handlePredictionFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    handlePredictionOutput();
    const bool ok = exitStatus == QProcess::NormalExit && exitCode == 0;
    const qint64 elapsedMs = predictionTimer_.isValid() ? predictionTimer_.elapsed() : -1;
    appendPredictionText(QString("\n[predict] finished with exit code %1\n").arg(exitCode));
    bool publishOk = true;
    QPair<int, int> published{0, 0};
    if (ok && !activePredictionStagingDir_.isEmpty()) {
        try {
            published = publishPredictionOutputs(activePredictionStagingDir_, activePredictionPublishDir_);
            summarizePredictionOutputs(activePredictionStagingDir_, activePredictionPublishDir_, published, elapsedMs);
            QDir staging(activePredictionStagingDir_);
            if (staging.exists() && !staging.removeRecursively()) {
                appendPredictionText(QStringLiteral("[workbench] staging cleanup failed: %1\n").arg(activePredictionStagingDir_));
            } else {
                appendPredictionText(QStringLiteral("[workbench] staging cleaned: %1\n").arg(activePredictionStagingDir_));
            }
        } catch (const std::exception& exc) {
            publishOk = false;
            appendPredictionText(QStringLiteral("[workbench] publish failed: %1\n[workbench] staging kept: %2\n")
                .arg(QString::fromUtf8(exc.what()), activePredictionStagingDir_));
            QMessageBox::warning(this, QStringLiteral("发布失败"), QStringLiteral("预测已结束，但发布输出失败：\n%1\n\n暂存目录保留在：\n%2")
                .arg(QString::fromUtf8(exc.what()), activePredictionStagingDir_));
        }
    } else if (!activePredictionStagingDir_.isEmpty()) {
        appendPredictionText(QStringLiteral("[workbench] prediction failed, staging kept: %1\n").arg(activePredictionStagingDir_));
    }

    if (predictionStatusLabel_) {
        if (ok && publishOk) {
            predictionStatusLabel_->setText(QStringLiteral("已完成 | JSON %1 | 可视化 %2").arg(published.first).arg(published.second));
        } else if (ok) {
            predictionStatusLabel_->setText(QStringLiteral("发布失败"));
        } else {
            predictionStatusLabel_->setText(QStringLiteral("失败"));
        }
    }
    appendLog(QString("Prediction %1").arg(ok && publishOk ? "finished" : "failed"));
    statusBar()->showMessage(ok && publishOk ? QStringLiteral("预测完成") : QStringLiteral("预测失败"), 5000);
    if (predictionProcess_) {
        predictionProcess_->deleteLater();
        predictionProcess_ = nullptr;
    }
    activePredictionStagingDir_.clear();
    activePredictionPublishDir_.clear();
    setPredictionRunning(false);
}

void MainWindow::handlePredictionError(QProcess::ProcessError error) {
    if (!predictionProcess_ || error != QProcess::FailedToStart) {
        return;
    }
    appendPredictionText(QStringLiteral("\n预测启动失败：%1\n").arg(predictionProcess_->errorString()));
    if (!activePredictionStagingDir_.isEmpty()) {
        QDir(activePredictionStagingDir_).removeRecursively();
    }
    activePredictionStagingDir_.clear();
    activePredictionPublishDir_.clear();
    if (predictionStatusLabel_) {
        predictionStatusLabel_->setText(QStringLiteral("启动失败"));
    }
    predictionProcess_->deleteLater();
    predictionProcess_ = nullptr;
    setPredictionRunning(false);
}

void MainWindow::prelabelSelectedPage() {
    startPrelabelCurrentPageAsync(true, false, false, false, QStringLiteral("OCR 预标注当前页"));
    return;
    if (prelabelAllRunning_) {
        QMessageBox::information(this, QStringLiteral("预标注进行中"), QStringLiteral("后台全量预标注正在运行，请等待完成后再运行单页预标注。"));
        return;
    }
    if (!context_) {
        QMessageBox::information(this, "No project", "Open or create a project first.");
        return;
    }
    const int row = pageList_->currentRow();
    if (row < 0 || row >= pages_.size()) {
        QMessageBox::information(this, "No page", "Select a page first.");
        return;
    }

    const auto page = pages_.at(row);
    saveLabelModelConfig();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    statusBar()->showMessage(QString("Running C++ OCR prelabel: %1").arg(page.assetId));
    try {
        const QString outputDir = context_->path(QString("cache/ocr_prelabel/%1").arg(page.assetId));
        auto config = PaddleOcrEngine::defaultConfig(baseDir_);
        const QString detModelDir = labelSelectedModelDir(labelDetModelCombo_, predictDetModelCombo_);
        const QString recModelDir = labelSelectedModelDir(labelRecModelCombo_, predictRecModelCombo_);
        if (!detModelDir.isEmpty()) {
            config.textDetectionModelDir = detModelDir;
        }
        if (!recModelDir.isEmpty()) {
            config.textRecognitionModelDir = recModelDir;
        }
        config.device = labelPrelabelDevice();
        config.scoreThreshold = prelabelScoreThreshold();
        const auto report = PaddleOcrEngine::predictImage(
            page.imagePath,
            outputDir,
            config,
            true);
        if (predictionPreview_) {
            predictionPreview_->setPlainText(QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented)));
        }
        if (!report.value("ok").toBool()) {
            throw std::runtime_error(report.value("error").toString("C++ OCR prelabel failed").toStdString());
        }

        const auto annotation = ProjectRepository::readAnnotation(page.annotationPath);
        const auto updated = OcrPrelabeler::applyOcrResult(annotation, report);
        ProjectRepository::writeAnnotation(page.annotationPath, updated);
        appendLog(QString("Prelabelled %1 with %2 OCR region(s), threshold=%3, filtered=%4")
            .arg(page.assetId)
            .arg(OcrPrelabeler::autoOcrRegionCount(updated))
            .arg(config.scoreThreshold, 0, 'f', 2)
            .arg(report.value(QStringLiteral("filtered_count")).toInt(0)));
        refreshPages();
        if (row >= 0 && row < pageList_->count()) {
            pageList_->setCurrentRow(row);
            loadSelectedPage();
        }
        statusBar()->showMessage(QString("C++ OCR prelabel complete: %1").arg(page.assetId), 5000);
    } catch (const std::exception& exc) {
        QMessageBox::critical(this, "Prelabel failed", exc.what());
        statusBar()->showMessage("C++ OCR prelabel failed", 5000);
    }
    QApplication::restoreOverrideCursor();
}

void MainWindow::prelabelSelectedPageClassification() {
    startPrelabelCurrentPageAsync(false, true, false, false, QStringLiteral("方向预标注当前页"));
    return;
    if (prelabelAllRunning_) {
        QMessageBox::information(this, QStringLiteral("预标注进行中"), QStringLiteral("后台全量预标注正在运行，请等待完成后再运行单页预标注。"));
        return;
    }
    if (!context_) {
        QMessageBox::information(this, "No project", "Open or create a project first.");
        return;
    }
    const int row = pageList_->currentRow();
    if (row < 0 || row >= pages_.size()) {
        QMessageBox::information(this, "No page", "Select a page first.");
        return;
    }

    const auto page = pages_.at(row);
    struct Task {
        QString predictionTask;
        QComboBox* modelCombo = nullptr;
    };
    QList<Task> tasks;
    if (!labelEnableDocOrientationCheck_ || labelEnableDocOrientationCheck_->isChecked()) {
        tasks.append({QStringLiteral("doc_orientation"), labelDocOrientationModelCombo_});
    }
    if (!labelEnableTextlineClsCheck_ || labelEnableTextlineClsCheck_->isChecked()) {
        tasks.append({QStringLiteral("textline_orientation"), labelTextlineClsModelCombo_});
    }
    if (tasks.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("分类预标注未启用"), 3000);
        return;
    }

    saveLabelModelConfig();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    statusBar()->showMessage(QString("Running C++ classification prelabel: %1").arg(page.assetId));
    try {
        QJsonObject annotation = ProjectRepository::readAnnotation(page.annotationPath);
        QJsonArray reports;
        QStringList applied;
        for (const Task& task : tasks) {
            PaddleClsModelConfig config = PaddleClsEngine::defaultConfig(task.predictionTask, baseDir_);
            const QString selectedModel = selectedPredictionModelDir(task.modelCombo);
            config.modelDir = selectedModel.isEmpty()
                ? preferredModelDir(
                    context_ ? &(*context_) : nullptr,
                    classificationTasksForPrediction(task.predictionTask),
                    config.modelDir)
                : selectedModel;
            config.device = labelPrelabelDevice();

            const QString outputDir = context_->path(QString("cache/cls_prelabel/%1/%2").arg(page.assetId, task.predictionTask));
            const auto report = PaddleClsEngine::predictImage(page.imagePath, outputDir, config, true);
            reports.append(report);
            if (!report.value("ok").toBool()) {
                throw std::runtime_error(report.value("error").toString("C++ classification prelabel failed").toStdString());
            }

            const QString label = ClassificationPrelabeler::normalizedLabel(task.predictionTask, report);
            annotation = ClassificationPrelabeler::applyClassificationResult(annotation, task.predictionTask, report);
            if (!label.isEmpty()) {
                applied.append(QString("%1=%2").arg(task.predictionTask, label));
            }
        }

        ProjectRepository::writeAnnotation(page.annotationPath, annotation);
        if (predictionPreview_) {
            predictionPreview_->setPlainText(QString::fromUtf8(QJsonDocument(QJsonObject{
                {QStringLiteral("ok"), true},
                {QStringLiteral("asset_id"), page.assetId},
                {QStringLiteral("applied"), QJsonArray::fromStringList(applied)},
                {QStringLiteral("results"), reports},
            }).toJson(QJsonDocument::Indented)));
        }
        appendLog(QString("Classification prelabelled %1: %2").arg(page.assetId, applied.join(", ")));
        refreshPages();
        if (row >= 0 && row < pageList_->count()) {
            pageList_->setCurrentRow(row);
            loadSelectedPage();
        }
        statusBar()->showMessage(QString("C++ classification prelabel complete: %1").arg(page.assetId), 5000);
    } catch (const std::exception& exc) {
        QMessageBox::critical(this, "Classification prelabel failed", exc.what());
        statusBar()->showMessage("C++ classification prelabel failed", 5000);
    }
    QApplication::restoreOverrideCursor();
}

void MainWindow::prelabelSelectedPageTableClassification() {
    startPrelabelCurrentPageAsync(false, false, true, false, QStringLiteral("表格分类预标注当前页"));
    return;
    if (prelabelAllRunning_) {
        QMessageBox::information(this, QStringLiteral("预标注进行中"), QStringLiteral("后台全量预标注正在运行，请等待完成后再运行单页预标注。"));
        return;
    }
    if (!context_) {
        QMessageBox::information(this, "No project", "Open or create a project first.");
        return;
    }
    const int row = pageList_->currentRow();
    if (row < 0 || row >= pages_.size()) {
        QMessageBox::information(this, "No page", "Select a page first.");
        return;
    }

    const auto page = pages_.at(row);
    PaddleClsModelConfig config = PaddleClsEngine::defaultConfig(QStringLiteral("table_classification"), baseDir_);
    config.modelDir = preferredModelDir(
        context_ ? &(*context_) : nullptr,
        classificationTasksForPrediction(QStringLiteral("table_classification")),
        config.modelDir);
    config.device = labelPrelabelDevice();
    QString reason;
    if (!PaddleInferenceRuntime::modelDirLooksUsable(config.modelDir, &reason)) {
        QMessageBox::warning(this, "Table classification model invalid", reason);
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    statusBar()->showMessage(QString("Running table classification prelabel: %1").arg(page.assetId));
    try {
        const QString outputDir = context_->path(QString("cache/table_cls_prelabel/%1").arg(page.assetId));
        const auto report = PaddleClsEngine::predictImage(page.imagePath, outputDir, config, true);
        if (predictionPreview_) {
            predictionPreview_->setPlainText(QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented)));
        }
        if (!report.value("ok").toBool()) {
            throw std::runtime_error(report.value("error").toString("table classification prelabel failed").toStdString());
        }

        const QJsonObject annotation = currentAnnotation_.isEmpty()
            ? ProjectRepository::readAnnotation(page.annotationPath)
            : currentAnnotation_;
        const QJsonObject updated = ClassificationPrelabeler::applyClassificationResult(
            annotation,
            QStringLiteral("table_classification"),
            report);
        if (!jsonObjectsEqual(updated, currentAnnotation_)) {
            pushAnnotationUndoState();
            currentAnnotation_ = updated;
            persistCurrentAnnotation();
            refreshCanvasAnnotation();
            refreshImageLabelPanel();
            updateSelectedRegionPanel();
        }
        const QString label = ClassificationPrelabeler::normalizedLabel(QStringLiteral("table_classification"), report);
        appendLog(QString("Table classification prelabelled %1: %2").arg(page.assetId, label));
        statusBar()->showMessage(QString("Table classification prelabel complete: %1").arg(page.assetId), 5000);
    } catch (const std::exception& exc) {
        QMessageBox::critical(this, "Table classification prelabel failed", exc.what());
        statusBar()->showMessage("Table classification prelabel failed", 5000);
    }
    QApplication::restoreOverrideCursor();
}

void MainWindow::prelabelSelectedPageLayout() {
    startPrelabelCurrentPageAsync(false, false, false, true, QStringLiteral("版面预标注当前页"));
    return;
    if (prelabelAllRunning_) {
        QMessageBox::information(this, QStringLiteral("预标注进行中"), QStringLiteral("后台全量预标注正在运行，请等待完成后再运行单页预标注。"));
        return;
    }
    if (!context_) {
        QMessageBox::information(this, "No project", "Open or create a project first.");
        return;
    }
    const int row = pageList_->currentRow();
    if (row < 0 || row >= pages_.size()) {
        QMessageBox::information(this, "No page", "Select a page first.");
        return;
    }

    const auto page = pages_.at(row);
    saveLabelModelConfig();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    statusBar()->showMessage(QString("Running layout prelabel: %1").arg(page.assetId));
    try {
        PaddleDocLayoutModelConfig config = PaddleDocLayoutEngine::defaultConfig(baseDir_);
        const QString selectedModel = selectedPredictionModelDir(labelLayoutModelCombo_);
        config.modelDir = selectedModel.isEmpty()
            ? preferredModelDir(context_ ? &(*context_) : nullptr, tasksForKind(QStringLiteral("layout")), config.modelDir)
            : selectedModel;
        config.device = labelPrelabelDevice();
        config.threshold = prelabelScoreThreshold();

        const QString outputDir = context_->path(QString("cache/layout_prelabel/%1").arg(page.assetId));
        const auto report = PaddleDocLayoutEngine::predictImage(page.imagePath, outputDir, config, true);
        if (predictionPreview_) {
            predictionPreview_->setPlainText(QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented)));
        }
        if (!report.value("ok").toBool()) {
            throw std::runtime_error(report.value("error").toString("layout prelabel failed").toStdString());
        }

        const auto annotation = ProjectRepository::readAnnotation(page.annotationPath);
        const auto updated = LayoutPrelabeler::applyLayoutResult(annotation, report);
        ProjectRepository::writeAnnotation(page.annotationPath, updated);
        appendLog(QString("Layout prelabelled %1 with %2 region(s), threshold=%3")
            .arg(page.assetId)
            .arg(LayoutPrelabeler::autoLayoutRegionCount(updated))
            .arg(config.threshold, 0, 'f', 2));
        refreshPages();
        if (row >= 0 && row < pageList_->count()) {
            pageList_->setCurrentRow(row);
            loadSelectedPage();
        }
        statusBar()->showMessage(QString("Layout prelabel complete: %1").arg(page.assetId), 5000);
    } catch (const std::exception& exc) {
        QMessageBox::critical(this, "Layout prelabel failed", exc.what());
        statusBar()->showMessage("Layout prelabel failed", 5000);
    }
    QApplication::restoreOverrideCursor();
}

void MainWindow::startPrelabelCurrentPageAsync(
    bool runOcr,
    bool runOrientation,
    bool runTableClassification,
    bool runLayout,
    const QString& title) {
    if (prelabelAllRunning_) {
        QMessageBox::information(this, title, QStringLiteral("已有后台预标注任务正在运行，请等待当前任务完成。"));
        return;
    }
    if (exportRunning_) {
        QMessageBox::information(this, title, QStringLiteral("后台导出正在读取标注文件，请完成后再运行预标注。"));
        return;
    }
    if (!context_) {
        QMessageBox::information(this, "No project", "Open or create a project first.");
        return;
    }
    const int row = pageList_ ? pageList_->currentRow() : -1;
    if (row < 0 || row >= pages_.size()) {
        QMessageBox::information(this, "No page", "Select a page first.");
        return;
    }
    if (!runOcr && !runOrientation && !runTableClassification && !runLayout) {
        statusBar()->showMessage(QStringLiteral("没有启用需要运行的预标注任务"), 3000);
        return;
    }

    saveLabelModelConfig();
    persistCurrentAnnotation();

    const PageInfo page = pages_.at(row);
    const QString projectRoot = context_->root.absolutePath();
    const QString device = labelPrelabelDevice();
    const double scoreThreshold = prelabelScoreThreshold();
    QString reason;

    PaddleOcrModelConfig ocrConfig;
    if (runOcr) {
        ocrConfig = PaddleOcrEngine::defaultConfig(baseDir_);
        const QString detModelDir = labelSelectedModelDir(labelDetModelCombo_, predictDetModelCombo_);
        const QString recModelDir = labelSelectedModelDir(labelRecModelCombo_, predictRecModelCombo_);
        if (!detModelDir.isEmpty()) {
            ocrConfig.textDetectionModelDir = detModelDir;
        }
        if (!recModelDir.isEmpty()) {
            ocrConfig.textRecognitionModelDir = recModelDir;
        }
        ocrConfig.device = device;
        ocrConfig.scoreThreshold = scoreThreshold;
        if (!PaddleInferenceRuntime::modelDirLooksUsable(ocrConfig.textDetectionModelDir, &reason)) {
            QMessageBox::warning(this, title, QStringLiteral("Det 检测模型不可用：\n%1").arg(reason));
            return;
        }
        reason.clear();
        if (!PaddleInferenceRuntime::modelDirLooksUsable(ocrConfig.textRecognitionModelDir, &reason)) {
            QMessageBox::warning(this, title, QStringLiteral("Rec 识别模型不可用：\n%1").arg(reason));
            return;
        }
    }

    QList<AsyncPrelabelClassificationTask> orientationTasks;
    if (runOrientation) {
        struct UiTask {
            QString predictionTask;
            QComboBox* modelCombo = nullptr;
        };
        QList<UiTask> tasks;
        if (!labelEnableDocOrientationCheck_ || labelEnableDocOrientationCheck_->isChecked()) {
            tasks.append({QStringLiteral("doc_orientation"), labelDocOrientationModelCombo_});
        }
        if (!labelEnableTextlineClsCheck_ || labelEnableTextlineClsCheck_->isChecked()) {
            tasks.append({QStringLiteral("textline_orientation"), labelTextlineClsModelCombo_});
        }
        if (tasks.isEmpty()) {
            statusBar()->showMessage(QStringLiteral("分类预标注未启用"), 3000);
            return;
        }
        for (const auto& task : tasks) {
            PaddleClsModelConfig config = PaddleClsEngine::defaultConfig(task.predictionTask, baseDir_);
            const QString selectedModel = selectedPredictionModelDir(task.modelCombo);
            config.modelDir = selectedModel.isEmpty()
                ? preferredModelDir(
                    context_ ? &(*context_) : nullptr,
                    classificationTasksForPrediction(task.predictionTask),
                    config.modelDir)
                : selectedModel;
            config.device = device;
            reason.clear();
            if (!PaddleInferenceRuntime::modelDirLooksUsable(config.modelDir, &reason)) {
                QMessageBox::warning(this, title, QStringLiteral("分类模型不可用：\n%1").arg(reason));
                return;
            }
            orientationTasks.append({task.predictionTask, config});
        }
    }

    PaddleClsModelConfig tableConfig;
    if (runTableClassification) {
        tableConfig = PaddleClsEngine::defaultConfig(QStringLiteral("table_classification"), baseDir_);
        tableConfig.modelDir = preferredModelDir(
            context_ ? &(*context_) : nullptr,
            classificationTasksForPrediction(QStringLiteral("table_classification")),
            tableConfig.modelDir);
        tableConfig.device = device;
        reason.clear();
        if (!PaddleInferenceRuntime::modelDirLooksUsable(tableConfig.modelDir, &reason)) {
            QMessageBox::warning(this, title, QStringLiteral("表格分类模型不可用：\n%1").arg(reason));
            return;
        }
    }

    PaddleDocLayoutModelConfig layoutConfig;
    if (runLayout) {
        layoutConfig = PaddleDocLayoutEngine::defaultConfig(baseDir_);
        const QString selectedModel = selectedPredictionModelDir(labelLayoutModelCombo_);
        layoutConfig.modelDir = selectedModel.isEmpty()
            ? preferredModelDir(context_ ? &(*context_) : nullptr, tasksForKind(QStringLiteral("layout")), layoutConfig.modelDir)
            : selectedModel;
        layoutConfig.device = device;
        layoutConfig.threshold = scoreThreshold;
        reason.clear();
        if (!PaddleDocLayoutEngine::modelDirLooksUsable(layoutConfig.modelDir, &reason)) {
            QMessageBox::warning(this, title, QStringLiteral("Layout 版面模型不可用：\n%1").arg(reason));
            return;
        }
    }

    QPointer<MainWindow> window(this);
    auto* thread = QThread::create([
        window,
        projectRoot,
        page,
        row,
        title,
        runOcr,
        runOrientation,
        runTableClassification,
        runLayout,
        ocrConfig,
        orientationTasks,
        tableConfig,
        layoutConfig
    ]() {
        AsyncPrelabelJobResult result;
        result.title = title;
        QElapsedTimer timer;
        timer.start();
        const QDir projectDir(projectRoot);
        auto projectPath = [&projectDir](const QString& relativePath) {
            return projectDir.filePath(relativePath);
        };
        auto postStatus = [window](const QString& message) {
            if (!window) {
                return;
            }
            QMetaObject::invokeMethod(window.data(), [window, message]() {
                if (!window) {
                    return;
                }
                window->statusBar()->showMessage(message);
            }, Qt::QueuedConnection);
        };
        QJsonArray previewResults;
        auto appendPreview = [&previewResults](const QString& task, const QJsonObject& report) {
            previewResults.append(QJsonObject{
                {QStringLiteral("task"), task},
                {QStringLiteral("result"), report},
            });
        };

        if (runOcr) {
            AsyncPrelabelStepSummary step;
            step.name = QStringLiteral("OCR");
            try {
                postStatus(QStringLiteral("OCR 预标注当前页：%1").arg(page.assetId));
                const QJsonObject report = PaddleOcrEngine::predictImage(
                    page.imagePath,
                    projectPath(QStringLiteral("cache/ocr_prelabel/%1").arg(page.assetId)),
                    ocrConfig,
                    false);
                appendPreview(QStringLiteral("ocr"), report);
                if (!report.value(QStringLiteral("ok")).toBool()) {
                    throw std::runtime_error(report.value(QStringLiteral("error")).toString(QStringLiteral("OCR prelabel failed")).toStdString());
                }
                const QJsonObject annotation = ProjectRepository::readAnnotation(page.annotationPath);
                const QJsonObject updated = OcrPrelabeler::applyOcrResult(annotation, report);
                ProjectRepository::writeAnnotation(page.annotationPath, updated);
                step.regionCount = OcrPrelabeler::autoOcrRegionCount(updated);
                ++step.okCount;
                result.logLines.append(QStringLiteral("Prelabelled %1 with %2 OCR region(s), threshold=%3, filtered=%4")
                    .arg(page.assetId)
                    .arg(step.regionCount)
                    .arg(ocrConfig.scoreThreshold, 0, 'f', 2)
                    .arg(report.value(QStringLiteral("filtered_count")).toInt(0)));
            } catch (const std::exception& exc) {
                ++step.failedCount;
                step.details.append(QStringLiteral("%1: %2").arg(page.assetId, QString::fromUtf8(exc.what())));
            }
            result.steps.append(step);
        }

        if (runOrientation) {
            AsyncPrelabelStepSummary step;
            step.name = QStringLiteral("方向分类");
            QStringList applied;
            QJsonArray reports;
            try {
                QJsonObject annotation = ProjectRepository::readAnnotation(page.annotationPath);
                for (const auto& task : orientationTasks) {
                    postStatus(QStringLiteral("方向分类预标注当前页：%1").arg(page.assetId));
                    const QJsonObject report = PaddleClsEngine::predictImage(
                        page.imagePath,
                        projectPath(QStringLiteral("cache/cls_prelabel/%1/%2").arg(page.assetId, task.predictionTask)),
                        task.config,
                        false);
                    reports.append(report);
                    appendPreview(task.predictionTask, report);
                    if (!report.value(QStringLiteral("ok")).toBool()) {
                        throw std::runtime_error(report.value(QStringLiteral("error")).toString(QStringLiteral("classification prelabel failed")).toStdString());
                    }
                    const QString label = ClassificationPrelabeler::normalizedLabel(task.predictionTask, report);
                    annotation = ClassificationPrelabeler::applyClassificationResult(annotation, task.predictionTask, report);
                    if (!label.isEmpty()) {
                        applied.append(QStringLiteral("%1=%2").arg(task.predictionTask, label));
                    }
                }
                ProjectRepository::writeAnnotation(page.annotationPath, annotation);
                ++step.okCount;
                result.logLines.append(QStringLiteral("Classification prelabelled %1: %2").arg(page.assetId, applied.join(QStringLiteral(", "))));
            } catch (const std::exception& exc) {
                ++step.failedCount;
                step.details.append(QStringLiteral("%1: %2").arg(page.assetId, QString::fromUtf8(exc.what())));
            }
            result.steps.append(step);
        }

        if (runTableClassification) {
            AsyncPrelabelStepSummary step;
            step.name = QStringLiteral("表格分类");
            try {
                postStatus(QStringLiteral("表格分类预标注当前页：%1").arg(page.assetId));
                const QJsonObject report = PaddleClsEngine::predictImage(
                    page.imagePath,
                    projectPath(QStringLiteral("cache/table_cls_prelabel/%1").arg(page.assetId)),
                    tableConfig,
                    false);
                appendPreview(QStringLiteral("table_classification"), report);
                if (!report.value(QStringLiteral("ok")).toBool()) {
                    throw std::runtime_error(report.value(QStringLiteral("error")).toString(QStringLiteral("table classification prelabel failed")).toStdString());
                }
                const QJsonObject annotation = ProjectRepository::readAnnotation(page.annotationPath);
                const QJsonObject updated = ClassificationPrelabeler::applyClassificationResult(
                    annotation,
                    QStringLiteral("table_classification"),
                    report);
                ProjectRepository::writeAnnotation(page.annotationPath, updated);
                ++step.okCount;
                const QString label = ClassificationPrelabeler::normalizedLabel(QStringLiteral("table_classification"), report);
                result.logLines.append(QStringLiteral("Table classification prelabelled %1: %2").arg(page.assetId, label));
            } catch (const std::exception& exc) {
                ++step.failedCount;
                step.details.append(QStringLiteral("%1: %2").arg(page.assetId, QString::fromUtf8(exc.what())));
            }
            result.steps.append(step);
        }

        if (runLayout) {
            AsyncPrelabelStepSummary step;
            step.name = QStringLiteral("版面");
            try {
                postStatus(QStringLiteral("版面预标注当前页：%1").arg(page.assetId));
                const QJsonObject report = PaddleDocLayoutEngine::predictImage(
                    page.imagePath,
                    projectPath(QStringLiteral("cache/layout_prelabel/%1").arg(page.assetId)),
                    layoutConfig,
                    false);
                appendPreview(QStringLiteral("layout"), report);
                if (!report.value(QStringLiteral("ok")).toBool()) {
                    throw std::runtime_error(report.value(QStringLiteral("error")).toString(QStringLiteral("layout prelabel failed")).toStdString());
                }
                const QJsonObject annotation = ProjectRepository::readAnnotation(page.annotationPath);
                const QJsonObject updated = LayoutPrelabeler::applyLayoutResult(annotation, report);
                ProjectRepository::writeAnnotation(page.annotationPath, updated);
                step.regionCount = LayoutPrelabeler::autoLayoutRegionCount(updated);
                ++step.okCount;
                result.logLines.append(QStringLiteral("Layout prelabelled %1 with %2 region(s), threshold=%3")
                    .arg(page.assetId)
                    .arg(step.regionCount)
                    .arg(layoutConfig.threshold, 0, 'f', 2));
            } catch (const std::exception& exc) {
                ++step.failedCount;
                step.details.append(QStringLiteral("%1: %2").arg(page.assetId, QString::fromUtf8(exc.what())));
            }
            result.steps.append(step);
        }

        result.elapsedMs = timer.elapsed();
        result.preview = QJsonObject{
            {QStringLiteral("ok"), true},
            {QStringLiteral("asset_id"), page.assetId},
            {QStringLiteral("results"), previewResults},
        };
        if (!window) {
            return;
        }
        QMetaObject::invokeMethod(window.data(), [window, result, row]() {
            if (!window) {
                return;
            }
            window->prelabelAllRunning_ = false;
            for (const QString& line : result.logLines) {
                window->appendLog(line);
            }
            bool failed = false;
            for (const auto& step : result.steps) {
                if (step.failedCount > 0) {
                    failed = true;
                    window->appendLog(QStringLiteral("%1 failures:\n%2")
                        .arg(step.name, step.details.mid(0, 8).join(QLatin1Char('\n'))));
                }
            }
            const int currentRow = row;
            window->refreshPages();
            if (currentRow >= 0 && currentRow < window->pageList_->count()) {
                window->pageList_->setCurrentRow(currentRow);
                window->loadSelectedPage();
            }
            if (window->predictionPreview_) {
                window->predictionPreview_->setPlainText(QString::fromUtf8(QJsonDocument(result.preview).toJson(QJsonDocument::Indented)));
            }
            if (failed) {
                window->statusBar()->showMessage(QStringLiteral("%1失败，用时 %2 秒")
                    .arg(result.title)
                    .arg(result.elapsedMs / 1000.0, 0, 'f', 1), 5000);
                QMessageBox::warning(window.data(), result.title, prelabelSummaryText(result));
            } else {
                window->statusBar()->showMessage(QStringLiteral("%1完成，用时 %2 秒")
                    .arg(result.title)
                    .arg(result.elapsedMs / 1000.0, 0, 'f', 1), 5000);
            }
        }, Qt::QueuedConnection);
    });

    activePrelabelThread_ = thread;
    prelabelAllRunning_ = true;
    statusBar()->showMessage(QStringLiteral("%1已在后台启动").arg(title), 3000);
    connect(thread, &QThread::finished, this, [this, thread]() {
        if (activePrelabelThread_ == thread) {
            activePrelabelThread_ = nullptr;
        }
        thread->deleteLater();
    });
    thread->start();
}

void MainWindow::startOneClickPrelabelAll() {
    const bool runOrientation =
        (labelEnableTextlineClsCheck_ && labelEnableTextlineClsCheck_->isChecked())
        || (labelEnableDocOrientationCheck_ && labelEnableDocOrientationCheck_->isChecked());
    const bool runLayout = !selectedPredictionModelDir(labelLayoutModelCombo_).isEmpty();
    startPrelabelAllAsync(
        true,
        runOrientation,
        false,
        runLayout,
        QStringLiteral("一键预标注全部图片"));
}

void MainWindow::startPrelabelAllAsync(
    bool runOcr,
    bool runOrientation,
    bool runTableClassification,
    bool runLayout,
    const QString& title) {
    if (prelabelAllRunning_) {
        QMessageBox::information(this, title, QStringLiteral("已有后台预标注任务正在运行，请等待当前任务完成。"));
        return;
    }
    if (!context_) {
        QMessageBox::information(this, "No project", "Open or create a project first.");
        return;
    }
    const auto pages = ProjectRepository::listPages(*context_);
    if (pages.isEmpty()) {
        QMessageBox::information(this, "No pages", "Import images or PDFs first.");
        return;
    }
    if (!runOcr && !runOrientation && !runTableClassification && !runLayout) {
        statusBar()->showMessage(QStringLiteral("没有启用需要运行的预标注任务"), 3000);
        return;
    }

    saveLabelModelConfig();
    const QString projectRoot = context_->root.absolutePath();
    const QString device = labelPrelabelDevice();
    if (labelUseGpuCheck_ && labelUseGpuCheck_->isChecked() && device == QStringLiteral("cpu")) {
        appendLog(QStringLiteral("当前 Paddle Inference SDK 为 CPU 版，预标注已自动使用 CPU。"));
        statusBar()->showMessage(QStringLiteral("当前 Paddle Inference SDK 为 CPU 版，已自动使用 CPU"), 5000);
    }
    const double scoreThreshold = prelabelScoreThreshold();
    QString reason;
    QStringList stepNames;

    PaddleOcrModelConfig ocrConfig;
    if (runOcr) {
        ocrConfig = PaddleOcrEngine::defaultConfig(baseDir_);
        const QString detModelDir = labelSelectedModelDir(labelDetModelCombo_, predictDetModelCombo_);
        const QString recModelDir = labelSelectedModelDir(labelRecModelCombo_, predictRecModelCombo_);
        if (!detModelDir.isEmpty()) {
            ocrConfig.textDetectionModelDir = detModelDir;
        }
        if (!recModelDir.isEmpty()) {
            ocrConfig.textRecognitionModelDir = recModelDir;
        }
        ocrConfig.device = device;
        ocrConfig.scoreThreshold = scoreThreshold;
        if (!PaddleInferenceRuntime::modelDirLooksUsable(ocrConfig.textDetectionModelDir, &reason)) {
            QMessageBox::warning(this, title, QStringLiteral("Det 检测模型不可用：\n%1").arg(reason));
            return;
        }
        reason.clear();
        if (!PaddleInferenceRuntime::modelDirLooksUsable(ocrConfig.textRecognitionModelDir, &reason)) {
            QMessageBox::warning(this, title, QStringLiteral("Rec 识别模型不可用：\n%1").arg(reason));
            return;
        }
        stepNames.append(QStringLiteral("OCR 框标注"));
    }

    QList<AsyncPrelabelClassificationTask> orientationTasks;
    if (runOrientation) {
        struct UiTask {
            QString predictionTask;
            QComboBox* modelCombo = nullptr;
        };
        QList<UiTask> tasks;
        if (labelEnableDocOrientationCheck_ && labelEnableDocOrientationCheck_->isChecked()) {
            tasks.append({QStringLiteral("doc_orientation"), labelDocOrientationModelCombo_});
        }
        if (labelEnableTextlineClsCheck_ && labelEnableTextlineClsCheck_->isChecked()) {
            tasks.append({QStringLiteral("textline_orientation"), labelTextlineClsModelCombo_});
        }
        if (tasks.isEmpty()) {
            QMessageBox::information(this, title, QStringLiteral("未启用文档方向或文本行方向分类。"));
            return;
        }
        for (const auto& task : tasks) {
            PaddleClsModelConfig config = PaddleClsEngine::defaultConfig(task.predictionTask, baseDir_);
            const QString selectedModel = selectedPredictionModelDir(task.modelCombo);
            config.modelDir = selectedModel.isEmpty()
                ? preferredModelDir(
                    context_ ? &(*context_) : nullptr,
                    classificationTasksForPrediction(task.predictionTask),
                    config.modelDir)
                : selectedModel;
            config.device = device;
            reason.clear();
            if (!PaddleInferenceRuntime::modelDirLooksUsable(config.modelDir, &reason)) {
                QMessageBox::warning(this, title, QStringLiteral("分类模型不可用：\n%1").arg(reason));
                return;
            }
            orientationTasks.append({task.predictionTask, config});
        }
        stepNames.append(QStringLiteral("方向分类"));
    }

    PaddleClsModelConfig tableConfig;
    if (runTableClassification) {
        tableConfig = PaddleClsEngine::defaultConfig(QStringLiteral("table_classification"), baseDir_);
        tableConfig.modelDir = preferredModelDir(
            context_ ? &(*context_) : nullptr,
            classificationTasksForPrediction(QStringLiteral("table_classification")),
            tableConfig.modelDir);
        tableConfig.device = device;
        reason.clear();
        if (!PaddleInferenceRuntime::modelDirLooksUsable(tableConfig.modelDir, &reason)) {
            QMessageBox::warning(this, title, QStringLiteral("表格分类模型不可用：\n%1").arg(reason));
            return;
        }
        stepNames.append(QStringLiteral("表格分类"));
    }

    PaddleDocLayoutModelConfig layoutConfig;
    if (runLayout) {
        layoutConfig = PaddleDocLayoutEngine::defaultConfig(baseDir_);
        const QString selectedModel = selectedPredictionModelDir(labelLayoutModelCombo_);
        layoutConfig.modelDir = selectedModel.isEmpty()
            ? preferredModelDir(context_ ? &(*context_) : nullptr, tasksForKind(QStringLiteral("layout")), layoutConfig.modelDir)
            : selectedModel;
        layoutConfig.device = device;
        layoutConfig.threshold = scoreThreshold;
        reason.clear();
        if (!PaddleDocLayoutEngine::modelDirLooksUsable(layoutConfig.modelDir, &reason)) {
            QMessageBox::warning(this, title, QStringLiteral("Layout 版面模型不可用：\n%1").arg(reason));
            return;
        }
        stepNames.append(QStringLiteral("版面标注"));
    }

    const QString prompt = QStringLiteral("将在后台预标注 %1 页。\n\n任务：%2\n置信度阈值：%3\n\n运行期间可以继续查看界面，但不要手动编辑同一项目的标注文件。")
        .arg(pages.size())
        .arg(stepNames.join(QStringLiteral("、")))
        .arg(scoreThreshold, 0, 'f', 2);
    const auto reply = QMessageBox::question(
        this,
        title,
        prompt,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }

    QPointer<MainWindow> window(this);
    auto* thread = QThread::create([
        window,
        projectRoot,
        pages,
        title,
        runOcr,
        runOrientation,
        runTableClassification,
        runLayout,
        ocrConfig,
        orientationTasks,
        tableConfig,
        layoutConfig
    ]() {
        AsyncPrelabelJobResult result;
        result.title = title;
        QElapsedTimer timer;
        timer.start();
        const QDir projectDir(projectRoot);
        auto projectPath = [&projectDir](const QString& relativePath) {
            return projectDir.filePath(relativePath);
        };
        auto postProgress = [window](const QString& step, int current, int total, const QString& imagePath) {
            if (!window) {
                return;
            }
            const QString fileName = QFileInfo(imagePath).fileName();
            QMetaObject::invokeMethod(window.data(), [window, step, current, total, fileName]() {
                if (!window) {
                    return;
                }
                window->statusBar()->showMessage(QStringLiteral("%1 %2/%3：%4")
                    .arg(step)
                    .arg(current)
                    .arg(total)
                    .arg(fileName));
            }, Qt::QueuedConnection);
        };

        if (runOcr) {
            AsyncPrelabelStepSummary step;
            step.name = QStringLiteral("OCR");
            QList<QPair<QString, QString>> jobs;
            jobs.reserve(pages.size());
            for (const auto& page : pages) {
                jobs.append(qMakePair(page.imagePath, projectPath(QStringLiteral("cache/ocr_prelabel/%1").arg(page.assetId))));
            }
            const QList<QJsonObject> reports = PaddleOcrEngine::predictImages(
                jobs,
                ocrConfig,
                false,
                [&postProgress](int current, int total, const QString& imagePath) {
                    postProgress(QStringLiteral("OCR"), current, total, imagePath);
                });
            for (int i = 0; i < pages.size(); ++i) {
                const auto page = pages.at(i);
                try {
                    const QJsonObject report = i < reports.size() ? reports.at(i) : QJsonObject{};
                    if (report.isEmpty() || !report.value(QStringLiteral("ok")).toBool()) {
                        throw std::runtime_error(report.value(QStringLiteral("error")).toString(QStringLiteral("OCR prelabel failed")).toStdString());
                    }
                    const auto annotation = ProjectRepository::readAnnotation(page.annotationPath);
                    const auto updated = OcrPrelabeler::applyOcrResult(annotation, report);
                    ProjectRepository::writeAnnotation(page.annotationPath, updated);
                    step.regionCount += OcrPrelabeler::autoOcrRegionCount(updated);
                    ++step.okCount;
                } catch (const std::exception& exc) {
                    ++step.failedCount;
                    step.details.append(QStringLiteral("%1: %2").arg(page.assetId, QString::fromUtf8(exc.what())));
                }
            }
            result.steps.append(step);
        }

        if (runOrientation) {
            AsyncPrelabelStepSummary step;
            step.name = QStringLiteral("方向分类");
            QVector<QJsonObject> annotations(pages.size());
            QVector<bool> annotationLoaded(pages.size(), false);
            QVector<bool> pageFailed(pages.size(), false);
            QVector<QStringList> pageFailureDetails(pages.size());
            for (const auto& task : orientationTasks) {
                QList<QPair<QString, QString>> jobs;
                jobs.reserve(pages.size());
                for (const auto& page : pages) {
                    jobs.append(qMakePair(
                        page.imagePath,
                        projectPath(QStringLiteral("cache/cls_prelabel/%1/%2").arg(page.assetId, task.predictionTask))));
                }
                const QList<QJsonObject> reports = PaddleClsEngine::predictImages(
                    jobs,
                    task.config,
                    false,
                    [&postProgress, task](int current, int total, const QString& imagePath) {
                        postProgress(QStringLiteral("方向分类 %1").arg(task.predictionTask), current, total, imagePath);
                    });
                for (int i = 0; i < pages.size(); ++i) {
                    try {
                        const QJsonObject report = i < reports.size() ? reports.at(i) : QJsonObject{};
                        if (report.isEmpty() || !report.value(QStringLiteral("ok")).toBool()) {
                            throw std::runtime_error(report.value(QStringLiteral("error")).toString(QStringLiteral("classification prelabel failed")).toStdString());
                        }
                        if (!annotationLoaded.at(i)) {
                            annotations[i] = ProjectRepository::readAnnotation(pages.at(i).annotationPath);
                            annotationLoaded[i] = true;
                        }
                        annotations[i] = ClassificationPrelabeler::applyClassificationResult(annotations.at(i), task.predictionTask, report);
                    } catch (const std::exception& exc) {
                        pageFailed[i] = true;
                        pageFailureDetails[i].append(QString::fromUtf8(exc.what()));
                    }
                }
            }
            for (int i = 0; i < pages.size(); ++i) {
                const auto page = pages.at(i);
                if (pageFailed.at(i)) {
                    ++step.failedCount;
                    step.details.append(QStringLiteral("%1: %2").arg(page.assetId, pageFailureDetails.at(i).join(QStringLiteral("; "))));
                    continue;
                }
                try {
                    if (annotationLoaded.at(i)) {
                        ProjectRepository::writeAnnotation(page.annotationPath, annotations.at(i));
                    }
                    ++step.okCount;
                } catch (const std::exception& exc) {
                    ++step.failedCount;
                    step.details.append(QStringLiteral("%1: %2").arg(page.assetId, QString::fromUtf8(exc.what())));
                }
            }
            result.steps.append(step);
        }

        if (runTableClassification) {
            AsyncPrelabelStepSummary step;
            step.name = QStringLiteral("表格分类");
            QList<QPair<QString, QString>> jobs;
            jobs.reserve(pages.size());
            for (const auto& page : pages) {
                jobs.append(qMakePair(page.imagePath, projectPath(QStringLiteral("cache/table_cls_prelabel/%1").arg(page.assetId))));
            }
            const QList<QJsonObject> reports = PaddleClsEngine::predictImages(
                jobs,
                tableConfig,
                false,
                [&postProgress](int current, int total, const QString& imagePath) {
                    postProgress(QStringLiteral("表格分类"), current, total, imagePath);
                });
            for (int i = 0; i < pages.size(); ++i) {
                const auto page = pages.at(i);
                try {
                    const QJsonObject report = i < reports.size() ? reports.at(i) : QJsonObject{};
                    if (report.isEmpty() || !report.value(QStringLiteral("ok")).toBool()) {
                        throw std::runtime_error(report.value(QStringLiteral("error")).toString(QStringLiteral("table classification prelabel failed")).toStdString());
                    }
                    const auto annotation = ProjectRepository::readAnnotation(page.annotationPath);
                    const auto updated = ClassificationPrelabeler::applyClassificationResult(
                        annotation,
                        QStringLiteral("table_classification"),
                        report);
                    ProjectRepository::writeAnnotation(page.annotationPath, updated);
                    ++step.okCount;
                } catch (const std::exception& exc) {
                    ++step.failedCount;
                    step.details.append(QStringLiteral("%1: %2").arg(page.assetId, QString::fromUtf8(exc.what())));
                }
            }
            result.steps.append(step);
        }

        if (runLayout) {
            AsyncPrelabelStepSummary step;
            step.name = QStringLiteral("版面");
            QList<QPair<QString, QString>> jobs;
            jobs.reserve(pages.size());
            for (const auto& page : pages) {
                jobs.append(qMakePair(page.imagePath, projectPath(QStringLiteral("cache/layout_prelabel/%1").arg(page.assetId))));
            }
            const QList<QJsonObject> reports = PaddleDocLayoutEngine::predictImages(
                jobs,
                layoutConfig,
                false,
                [&postProgress](int current, int total, const QString& imagePath) {
                    postProgress(QStringLiteral("版面"), current, total, imagePath);
                });
            for (int i = 0; i < pages.size(); ++i) {
                const auto page = pages.at(i);
                try {
                    const QJsonObject report = i < reports.size() ? reports.at(i) : QJsonObject{};
                    if (report.isEmpty() || !report.value(QStringLiteral("ok")).toBool()) {
                        throw std::runtime_error(report.value(QStringLiteral("error")).toString(QStringLiteral("layout prelabel failed")).toStdString());
                    }
                    const auto annotation = ProjectRepository::readAnnotation(page.annotationPath);
                    const auto updated = LayoutPrelabeler::applyLayoutResult(annotation, report);
                    ProjectRepository::writeAnnotation(page.annotationPath, updated);
                    step.regionCount += LayoutPrelabeler::autoLayoutRegionCount(updated);
                    ++step.okCount;
                } catch (const std::exception& exc) {
                    ++step.failedCount;
                    step.details.append(QStringLiteral("%1: %2").arg(page.assetId, QString::fromUtf8(exc.what())));
                }
            }
            result.steps.append(step);
        }

        result.elapsedMs = timer.elapsed();
        if (!window) {
            return;
        }
        QMetaObject::invokeMethod(window.data(), [window, result]() {
            if (!window) {
                return;
            }
            window->prelabelAllRunning_ = false;
            for (const auto& step : result.steps) {
                window->appendLog(QStringLiteral("%1: %2 success, %3 failed, %4 region(s)")
                    .arg(step.name)
                    .arg(step.okCount)
                    .arg(step.failedCount)
                    .arg(step.regionCount));
                if (!step.details.isEmpty()) {
                    window->appendLog(QStringLiteral("%1 failures:\n%2").arg(step.name, step.details.mid(0, 8).join(QLatin1Char('\n'))));
                }
            }
            const int row = window->pageList_ ? window->pageList_->currentRow() : -1;
            window->refreshPages();
            if (row >= 0 && row < window->pageList_->count()) {
                window->pageList_->setCurrentRow(row);
                window->loadSelectedPage();
            }
            window->statusBar()->showMessage(QStringLiteral("%1完成，用时 %2 秒")
                .arg(result.title)
                .arg(result.elapsedMs / 1000.0, 0, 'f', 1), 5000);
            QMessageBox::information(window.data(), result.title, prelabelSummaryText(result));
        }, Qt::QueuedConnection);
    });
    activePrelabelThread_ = thread;
    prelabelAllRunning_ = true;
    statusBar()->showMessage(QStringLiteral("%1已在后台启动").arg(title), 3000);
    connect(thread, &QThread::finished, this, [this, thread]() {
        if (activePrelabelThread_ == thread) {
            activePrelabelThread_ = nullptr;
        }
        thread->deleteLater();
    });
    thread->start();
}

void MainWindow::prelabelAllPages() {
    startPrelabelAllAsync(true, false, false, false, QStringLiteral("Prelabel all OCR"));
    return;
    if (!context_) {
        QMessageBox::information(this, "No project", "Open or create a project first.");
        return;
    }
    const auto pages = ProjectRepository::listPages(*context_);
    if (pages.isEmpty()) {
        QMessageBox::information(this, "No pages", "Import images or PDFs first.");
        return;
    }
    const auto reply = QMessageBox::question(
        this,
        "Prelabel all OCR",
        QString("Run OCR prelabel on all %1 page(s)? Existing OCR/layout annotations on each page will be replaced by OCR output.").arg(pages.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }

    auto config = PaddleOcrEngine::defaultConfig(baseDir_);
    saveLabelModelConfig();
    const QString detModelDir = labelSelectedModelDir(labelDetModelCombo_, predictDetModelCombo_);
    const QString recModelDir = labelSelectedModelDir(labelRecModelCombo_, predictRecModelCombo_);
    if (!detModelDir.isEmpty()) {
        config.textDetectionModelDir = detModelDir;
    }
    if (!recModelDir.isEmpty()) {
        config.textRecognitionModelDir = recModelDir;
    }
    config.device = labelPrelabelDevice();
    config.scoreThreshold = prelabelScoreThreshold();
    QString reason;
    if (!PaddleInferenceRuntime::modelDirLooksUsable(config.textDetectionModelDir, &reason)) {
        QMessageBox::warning(this, "Det model invalid", reason);
        return;
    }
    if (!PaddleInferenceRuntime::modelDirLooksUsable(config.textRecognitionModelDir, &reason)) {
        QMessageBox::warning(this, "Rec model invalid", reason);
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    int okCount = 0;
    int failedCount = 0;
    int regionCount = 0;
    QStringList failures;
    QList<QPair<QString, QString>> jobs;
    jobs.reserve(pages.size());
    for (int i = 0; i < pages.size(); ++i) {
        const auto page = pages.at(i);
        jobs.append(qMakePair(page.imagePath, context_->path(QString("cache/ocr_prelabel/%1").arg(page.assetId))));
    }
    const QList<QJsonObject> reports = PaddleOcrEngine::predictImages(
        jobs,
        config,
        false,
        [this](int current, int total, const QString& imagePath) {
            statusBar()->showMessage(QString("OCR prelabel %1/%2: %3")
                    .arg(current)
                    .arg(total)
                    .arg(QFileInfo(imagePath).fileName()));
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        });
    for (int i = 0; i < pages.size(); ++i) {
        const auto page = pages.at(i);
        try {
            const QJsonObject report = i < reports.size() ? reports.at(i) : QJsonObject{};
            if (report.isEmpty() || !report.value("ok").toBool()) {
                throw std::runtime_error(report.value("error").toString("C++ OCR prelabel failed").toStdString());
            }
            const auto annotation = ProjectRepository::readAnnotation(page.annotationPath);
            const auto updated = OcrPrelabeler::applyOcrResult(annotation, report);
            ProjectRepository::writeAnnotation(page.annotationPath, updated);
            regionCount += OcrPrelabeler::autoOcrRegionCount(updated);
            ++okCount;
        } catch (const std::exception& exc) {
            ++failedCount;
            failures.append(QString("%1: %2").arg(page.assetId, QString::fromUtf8(exc.what())));
        }
    }
    QApplication::restoreOverrideCursor();

    appendLog(QString("OCR prelabel all: %1 success, %2 failed, %3 region(s)").arg(okCount).arg(failedCount).arg(regionCount));
    if (!failures.isEmpty()) {
        appendLog("OCR prelabel failures:\n" + failures.mid(0, 8).join('\n'));
    }
    const int row = pageList_ ? pageList_->currentRow() : -1;
    refreshPages();
    if (row >= 0 && row < pageList_->count()) {
        pageList_->setCurrentRow(row);
        loadSelectedPage();
    }
    QMessageBox::information(
        this,
        "OCR prelabel complete",
        QString("Success: %1\nFailed: %2\nRegions: %3").arg(okCount).arg(failedCount).arg(regionCount));
}

void MainWindow::prelabelAllPagesClassification() {
    startPrelabelAllAsync(false, true, false, false, QStringLiteral("Prelabel all orientation"));
    return;
    if (!context_) {
        QMessageBox::information(this, "No project", "Open or create a project first.");
        return;
    }
    const auto pages = ProjectRepository::listPages(*context_);
    if (pages.isEmpty()) {
        QMessageBox::information(this, "No pages", "Import images or PDFs first.");
        return;
    }
    const auto reply = QMessageBox::question(
        this,
        "Prelabel all orientation",
        QString("Run document/textline orientation prelabel on all %1 page(s)?").arg(pages.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }

    struct Task {
        QString predictionTask;
        QComboBox* modelCombo = nullptr;
    };
    QList<Task> tasks;
    if (!labelEnableDocOrientationCheck_ || labelEnableDocOrientationCheck_->isChecked()) {
        tasks.append({QStringLiteral("doc_orientation"), labelDocOrientationModelCombo_});
    }
    if (!labelEnableTextlineClsCheck_ || labelEnableTextlineClsCheck_->isChecked()) {
        tasks.append({QStringLiteral("textline_orientation"), labelTextlineClsModelCombo_});
    }
    if (tasks.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("分类预标注未启用"), 3000);
        return;
    }
    QMap<QString, PaddleClsModelConfig> configs;
    QString reason;
    saveLabelModelConfig();
    for (const auto& task : tasks) {
        PaddleClsModelConfig config = PaddleClsEngine::defaultConfig(task.predictionTask, baseDir_);
        const QString selectedModel = selectedPredictionModelDir(task.modelCombo);
        config.modelDir = selectedModel.isEmpty()
            ? preferredModelDir(
                context_ ? &(*context_) : nullptr,
                classificationTasksForPrediction(task.predictionTask),
                config.modelDir)
            : selectedModel;
        config.device = labelPrelabelDevice();
        if (!PaddleInferenceRuntime::modelDirLooksUsable(config.modelDir, &reason)) {
            QMessageBox::warning(this, "Classification model invalid", reason);
            return;
        }
        configs.insert(task.predictionTask, config);
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    int okCount = 0;
    int failedCount = 0;
    QStringList failures;
    QVector<QJsonObject> annotations(pages.size());
    QVector<bool> annotationLoaded(pages.size(), false);
    QVector<bool> pageFailed(pages.size(), false);
    QVector<QStringList> pageFailureDetails(pages.size());

    for (const auto& task : tasks) {
        QList<QPair<QString, QString>> jobs;
        jobs.reserve(pages.size());
        for (const auto& page : pages) {
            jobs.append(qMakePair(
                page.imagePath,
                context_->path(QString("cache/cls_prelabel/%1/%2").arg(page.assetId, task.predictionTask))));
        }
        const QString taskName = task.predictionTask;
        const QList<QJsonObject> reports = PaddleClsEngine::predictImages(
            jobs,
            configs.value(taskName),
            false,
            [this, taskName](int current, int total, const QString& imagePath) {
                statusBar()->showMessage(QString("Orientation %1 %2/%3: %4")
                        .arg(taskName)
                        .arg(current)
                        .arg(total)
                        .arg(QFileInfo(imagePath).fileName()));
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            });
        for (int i = 0; i < pages.size(); ++i) {
            const auto page = pages.at(i);
            try {
                const QJsonObject report = i < reports.size() ? reports.at(i) : QJsonObject{};
                if (report.isEmpty() || !report.value("ok").toBool()) {
                    throw std::runtime_error(report.value("error").toString("C++ classification prelabel failed").toStdString());
                }
                if (!annotationLoaded.at(i)) {
                    annotations[i] = ProjectRepository::readAnnotation(page.annotationPath);
                    annotationLoaded[i] = true;
                }
                annotations[i] = ClassificationPrelabeler::applyClassificationResult(annotations.at(i), taskName, report);
            } catch (const std::exception& exc) {
                pageFailed[i] = true;
                pageFailureDetails[i].append(QString::fromUtf8(exc.what()));
            }
        }
    }

    for (int i = 0; i < pages.size(); ++i) {
        const auto page = pages.at(i);
        if (pageFailed.at(i)) {
            ++failedCount;
            failures.append(QString("%1: %2").arg(page.assetId, pageFailureDetails.at(i).join("; ")));
            continue;
        }
        try {
            if (annotationLoaded.at(i)) {
                ProjectRepository::writeAnnotation(page.annotationPath, annotations.at(i));
            }
            ++okCount;
        } catch (const std::exception& exc) {
            ++failedCount;
            failures.append(QString("%1: %2").arg(page.assetId, QString::fromUtf8(exc.what())));
        }
    }
    QApplication::restoreOverrideCursor();

    appendLog(QString("Orientation prelabel all: %1 success, %2 failed").arg(okCount).arg(failedCount));
    if (!failures.isEmpty()) {
        appendLog("Orientation prelabel failures:\n" + failures.mid(0, 8).join('\n'));
    }
    const int row = pageList_ ? pageList_->currentRow() : -1;
    refreshPages();
    if (row >= 0 && row < pageList_->count()) {
        pageList_->setCurrentRow(row);
        loadSelectedPage();
    }
    QMessageBox::information(this, "Orientation prelabel complete", QString("Success: %1\nFailed: %2").arg(okCount).arg(failedCount));
}

void MainWindow::prelabelAllPagesTableClassification() {
    startPrelabelAllAsync(false, false, true, false, QStringLiteral("Prelabel all table types"));
    return;
    if (!context_) {
        QMessageBox::information(this, "No project", "Open or create a project first.");
        return;
    }
    const auto pages = ProjectRepository::listPages(*context_);
    if (pages.isEmpty()) {
        QMessageBox::information(this, "No pages", "Import images or PDFs first.");
        return;
    }
    const auto reply = QMessageBox::question(
        this,
        "Prelabel all table types",
        QString("Run table classification prelabel on all %1 page(s)? Existing table classification labels will be replaced.").arg(pages.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }

    PaddleClsModelConfig config = PaddleClsEngine::defaultConfig(QStringLiteral("table_classification"), baseDir_);
    config.modelDir = preferredModelDir(
        context_ ? &(*context_) : nullptr,
        classificationTasksForPrediction(QStringLiteral("table_classification")),
        config.modelDir);
    config.device = labelPrelabelDevice();
    QString reason;
    if (!PaddleInferenceRuntime::modelDirLooksUsable(config.modelDir, &reason)) {
        QMessageBox::warning(this, "Table classification model invalid", reason);
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    int okCount = 0;
    int failedCount = 0;
    QStringList applied;
    QStringList failures;
    QList<QPair<QString, QString>> jobs;
    jobs.reserve(pages.size());
    for (int i = 0; i < pages.size(); ++i) {
        const auto page = pages.at(i);
        jobs.append(qMakePair(page.imagePath, context_->path(QString("cache/table_cls_prelabel/%1").arg(page.assetId))));
    }
    const QList<QJsonObject> reports = PaddleClsEngine::predictImages(
        jobs,
        config,
        false,
        [this](int current, int total, const QString& imagePath) {
            statusBar()->showMessage(QString("Table classification prelabel %1/%2: %3")
                    .arg(current)
                    .arg(total)
                    .arg(QFileInfo(imagePath).fileName()));
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        });
    for (int i = 0; i < pages.size(); ++i) {
        const auto page = pages.at(i);
        try {
            const QJsonObject report = i < reports.size() ? reports.at(i) : QJsonObject{};
            if (report.isEmpty() || !report.value("ok").toBool()) {
                throw std::runtime_error(report.value("error").toString("table classification prelabel failed").toStdString());
            }
            const auto annotation = ProjectRepository::readAnnotation(page.annotationPath);
            const auto updated = ClassificationPrelabeler::applyClassificationResult(
                annotation,
                QStringLiteral("table_classification"),
                report);
            ProjectRepository::writeAnnotation(page.annotationPath, updated);
            const QString label = ClassificationPrelabeler::normalizedLabel(QStringLiteral("table_classification"), report);
            if (!label.isEmpty()) {
                applied.append(QString("%1=%2").arg(page.assetId, label));
            }
            ++okCount;
        } catch (const std::exception& exc) {
            ++failedCount;
            failures.append(QString("%1: %2").arg(page.assetId, QString::fromUtf8(exc.what())));
        }
    }
    QApplication::restoreOverrideCursor();

    appendLog(QString("Table classification prelabel all: %1 success, %2 failed").arg(okCount).arg(failedCount));
    if (!applied.isEmpty()) {
        appendLog("Table classification labels:\n" + applied.mid(0, 12).join('\n'));
    }
    if (!failures.isEmpty()) {
        appendLog("Table classification failures:\n" + failures.mid(0, 8).join('\n'));
    }
    const int row = pageList_ ? pageList_->currentRow() : -1;
    refreshPages();
    if (row >= 0 && row < pageList_->count()) {
        pageList_->setCurrentRow(row);
        loadSelectedPage();
    }
    QMessageBox::information(this, "Table classification prelabel complete", QString("Success: %1\nFailed: %2").arg(okCount).arg(failedCount));
}

void MainWindow::prelabelAllPagesLayout() {
    startPrelabelAllAsync(false, false, false, true, QStringLiteral("Prelabel all layout"));
    return;
    if (!context_) {
        QMessageBox::information(this, "No project", "Open or create a project first.");
        return;
    }
    const auto pages = ProjectRepository::listPages(*context_);
    if (pages.isEmpty()) {
        QMessageBox::information(this, "No pages", "Import images or PDFs first.");
        return;
    }
    const auto reply = QMessageBox::question(
        this,
        "Prelabel all layout",
        QString("Run layout prelabel on all %1 page(s)? Auto layout regions will be replaced; manual regions are kept.").arg(pages.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }

    PaddleDocLayoutModelConfig config = PaddleDocLayoutEngine::defaultConfig(baseDir_);
    saveLabelModelConfig();
    const QString selectedModel = selectedPredictionModelDir(labelLayoutModelCombo_);
    config.modelDir = selectedModel.isEmpty()
        ? preferredModelDir(context_ ? &(*context_) : nullptr, tasksForKind(QStringLiteral("layout")), config.modelDir)
        : selectedModel;
    config.device = labelPrelabelDevice();
    config.threshold = prelabelScoreThreshold();
    QString reason;
    if (!PaddleDocLayoutEngine::modelDirLooksUsable(config.modelDir, &reason)) {
        QMessageBox::warning(this, "Layout model invalid", reason);
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    int okCount = 0;
    int failedCount = 0;
    int regionCount = 0;
    QStringList failures;
    QList<QPair<QString, QString>> jobs;
    jobs.reserve(pages.size());
    for (int i = 0; i < pages.size(); ++i) {
        const auto page = pages.at(i);
        jobs.append(qMakePair(page.imagePath, context_->path(QString("cache/layout_prelabel/%1").arg(page.assetId))));
    }
    const QList<QJsonObject> reports = PaddleDocLayoutEngine::predictImages(
        jobs,
        config,
        false,
        [this](int current, int total, const QString& imagePath) {
            statusBar()->showMessage(QString("Layout prelabel %1/%2: %3")
                    .arg(current)
                    .arg(total)
                    .arg(QFileInfo(imagePath).fileName()));
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        });
    for (int i = 0; i < pages.size(); ++i) {
        const auto page = pages.at(i);
        try {
            const QJsonObject report = i < reports.size() ? reports.at(i) : QJsonObject{};
            if (report.isEmpty() || !report.value("ok").toBool()) {
                throw std::runtime_error(report.value("error").toString("layout prelabel failed").toStdString());
            }
            const auto annotation = ProjectRepository::readAnnotation(page.annotationPath);
            const auto updated = LayoutPrelabeler::applyLayoutResult(annotation, report);
            ProjectRepository::writeAnnotation(page.annotationPath, updated);
            regionCount += LayoutPrelabeler::autoLayoutRegionCount(updated);
            ++okCount;
        } catch (const std::exception& exc) {
            ++failedCount;
            failures.append(QString("%1: %2").arg(page.assetId, QString::fromUtf8(exc.what())));
        }
    }
    QApplication::restoreOverrideCursor();

    appendLog(QString("Layout prelabel all: %1 success, %2 failed, %3 region(s)").arg(okCount).arg(failedCount).arg(regionCount));
    if (!failures.isEmpty()) {
        appendLog("Layout prelabel failures:\n" + failures.mid(0, 8).join('\n'));
    }
    const int row = pageList_ ? pageList_->currentRow() : -1;
    refreshPages();
    if (row >= 0 && row < pageList_->count()) {
        pageList_->setCurrentRow(row);
        loadSelectedPage();
    }
    QMessageBox::information(
        this,
        "Layout prelabel complete",
        QString("Success: %1\nFailed: %2\nRegions: %3").arg(okCount).arg(failedCount).arg(regionCount));
}

void MainWindow::clearCurrentPageAnnotations() {
    if (!context_) {
        QMessageBox::information(this, "No project", "Open or create a project first.");
        return;
    }
    const int row = pageList_ ? pageList_->currentRow() : -1;
    if (row < 0 || row >= pages_.size()) {
        QMessageBox::information(this, "No page", "Select a page first.");
        return;
    }
    const auto page = pages_.at(row);
    const auto reply = QMessageBox::question(
        this,
        "Clear current page",
        "Clear OCR boxes, layout boxes, image labels, and Rec crop records on the current page?",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }

    QJsonObject annotation = currentAnnotation_.isEmpty()
        ? ProjectRepository::readAnnotation(page.annotationPath)
        : currentAnnotation_;
    const int removedCrops = removeAnnotationCrops(*context_, annotation);
    pushAnnotationUndoState();
    annotation = AnnotationOps::clearAnnotation(annotation);
    currentAnnotation_ = annotation;
    selectedRegionId_.clear();
    persistCurrentAnnotation();
    refreshCanvasAnnotation();
    updateSelectedRegionPanel();
    refreshImageLabelPanel();
    appendLog(QString("Cleared annotations on %1; removed %2 crop file(s)").arg(page.assetId).arg(removedCrops));
    statusBar()->showMessage(QString("Cleared current page: %1").arg(page.assetId), 3000);
}

void MainWindow::clearAllAnnotations() {
    if (!context_) {
        QMessageBox::information(this, "No project", "Open or create a project first.");
        return;
    }
    const auto reply = QMessageBox::question(
        this,
        "Clear all annotations",
        "Clear OCR boxes, layout boxes, image labels, and Rec crop records on every page in this project?",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }

    const auto pages = ProjectRepository::listPages(*context_);
    int removedCrops = 0;
    for (const auto& page : pages) {
        QJsonObject annotation = ProjectRepository::readAnnotation(page.annotationPath);
        removedCrops += removeAnnotationCrops(*context_, annotation);
        ProjectRepository::writeAnnotation(page.annotationPath, AnnotationOps::clearAnnotation(annotation));
    }
    selectedRegionId_.clear();
    currentAnnotation_ = {};
    refreshPages();
    if (pageList_ && pageList_->count() > 0) {
        pageList_->setCurrentRow(0);
        loadSelectedPage();
    } else {
        refreshCanvasAnnotation();
        updateSelectedRegionPanel();
        refreshImageLabelPanel();
    }
    appendLog(QString("Cleared all annotations on %1 page(s); removed %2 crop file(s)").arg(pages.size()).arg(removedCrops));
    QMessageBox::information(this, "Clear complete", QString("Cleared %1 page(s).").arg(pages.size()));
}

void MainWindow::generateCurrentRecCrops() {
    if (!context_) {
        QMessageBox::information(this, "No project", "Open or create a project first.");
        return;
    }
    const int row = pageList_ ? pageList_->currentRow() : -1;
    if (row < 0 || row >= pages_.size()) {
        QMessageBox::information(this, "No page", "Select a page first.");
        return;
    }
    try {
        const auto page = pages_.at(row);
        QJsonObject annotation = currentAnnotation_.isEmpty()
            ? ProjectRepository::readAnnotation(page.annotationPath)
            : currentAnnotation_;
        removeAnnotationCrops(*context_, annotation);
        annotation = CropGenerator::generateRecCrops(*context_, annotation);
        ProjectRepository::writeAnnotation(page.annotationPath, annotation);
        currentAnnotation_ = annotation;
        const int count = annotation.value(QStringLiteral("rec_crops")).toArray().size();
        refreshPages();
        if (row >= 0 && row < pageList_->count()) {
            pageList_->setCurrentRow(row);
            loadSelectedPage();
        }
        appendLog(QString("Generated %1 Rec crop(s) for %2").arg(count).arg(page.assetId));
        QMessageBox::information(this, "Rec crops", QString("Generated %1 crop(s).").arg(count));
    } catch (const std::exception& exc) {
        QMessageBox::critical(this, "Rec crop failed", exc.what());
    }
}

void MainWindow::addOcrRegionFromCanvas(const QVariant& points) {
    if (!context_) {
        return;
    }
    const int row = pageList_ ? pageList_->currentRow() : -1;
    if (row < 0 || row >= pages_.size()) {
        return;
    }

    const QJsonArray jsonPoints = pointsFromVariant(points);
    if (jsonPoints.size() != 4) {
        statusBar()->showMessage("Ignored OCR box: invalid points", 3000);
        return;
    }

    pushAnnotationUndoState();
    currentAnnotation_ = AnnotationOps::addOcrRegion(
        currentAnnotation_,
        jsonPoints,
        QString(),
        QStringLiteral("manual"),
        true);
    const QJsonArray regions = currentAnnotation_.value("regions").toArray();
    selectedRegionId_ = regions.isEmpty() ? QString() : regions.last().toObject().value("id").toString();
    persistCurrentAnnotation();
    refreshCanvasAnnotation();
    updateSelectedRegionPanel();
    appendLog(QString("Added OCR region: %1").arg(selectedRegionId_));
}

void MainWindow::addLayoutRegionFromCanvas(const QVariant& points) {
    if (!context_) {
        return;
    }
    const int row = pageList_ ? pageList_->currentRow() : -1;
    if (row < 0 || row >= pages_.size()) {
        return;
    }
    const QJsonArray jsonPoints = pointsFromVariant(points);
    if (jsonPoints.size() != 4) {
        statusBar()->showMessage("Ignored layout box: invalid points", 3000);
        return;
    }
    double minX = 1e12;
    double minY = 1e12;
    double maxX = -1e12;
    double maxY = -1e12;
    for (const auto& value : jsonPoints) {
        const auto point = value.toArray();
        minX = std::min(minX, point.at(0).toDouble());
        minY = std::min(minY, point.at(1).toDouble());
        maxX = std::max(maxX, point.at(0).toDouble());
        maxY = std::max(maxY, point.at(1).toDouble());
    }
    const QString label = layoutLabelCombo_ ? layoutLabelCombo_->currentText() : QString("text");
    pushAnnotationUndoState();
    currentAnnotation_ = AnnotationOps::addLayoutRegion(
        currentAnnotation_,
        QRectF(minX, minY, std::max(1.0, maxX - minX), std::max(1.0, maxY - minY)),
        label.isEmpty() ? QString("text") : label,
        QStringLiteral("manual"),
        true);
    const QJsonArray regions = currentAnnotation_.value("regions").toArray();
    selectedRegionId_ = regions.isEmpty() ? QString() : regions.last().toObject().value("id").toString();
    persistCurrentAnnotation();
    refreshCanvasAnnotation();
    updateSelectedRegionPanel();
    appendLog(QString("Added layout region: %1").arg(selectedRegionId_));
}

void MainWindow::selectRegionFromCanvas(const QString& regionId) {
    selectedRegionId_ = regionId;
    updateSelectedRegionPanel();
}

void MainWindow::moveRegionFromCanvas(const QString& regionId, const QVariant& points) {
    if (regionId.isEmpty() || findRegion(currentAnnotation_, regionId).isEmpty()) {
        return;
    }
    const QJsonArray jsonPoints = pointsFromVariant(points);
    if (jsonPoints.size() < 2) {
        return;
    }
    selectedRegionId_ = regionId;
    const QJsonObject region = findRegion(currentAnnotation_, regionId);
    QJsonObject updatedAnnotation;
    if (region.value("type").toString() == "layout") {
        double minX = 1e12;
        double minY = 1e12;
        double maxX = -1e12;
        double maxY = -1e12;
        for (const auto& value : jsonPoints) {
            const auto point = value.toArray();
            minX = std::min(minX, point.at(0).toDouble());
            minY = std::min(minY, point.at(1).toDouble());
            maxX = std::max(maxX, point.at(0).toDouble());
            maxY = std::max(maxY, point.at(1).toDouble());
        }
        updatedAnnotation = AnnotationOps::updateRegion(
            currentAnnotation_,
            regionId,
            QJsonObject{{"bbox", QJsonArray{minX, minY, std::max(1.0, maxX - minX), std::max(1.0, maxY - minY)}}});
    } else {
        updatedAnnotation = AnnotationOps::updateRegion(
            currentAnnotation_,
            regionId,
            QJsonObject{
                {"points", jsonPoints},
                {"shape", jsonPoints.size() == 4 ? "quad" : "polygon"},
            });
    }
    if (jsonObjectsEqual(updatedAnnotation, currentAnnotation_)) {
        return;
    }
    pushAnnotationUndoState();
    currentAnnotation_ = updatedAnnotation;
    persistCurrentAnnotation();
    refreshCanvasAnnotation();
    updateSelectedRegionPanel();
    statusBar()->showMessage(QString("Moved OCR region: %1").arg(regionId), 2000);
}

void MainWindow::saveSelectedRegionText() {
    if (updatingRegionPanel_ || selectedRegionId_.isEmpty() || !regionTextEdit_) {
        return;
    }
    const QJsonObject region = findRegion(currentAnnotation_, selectedRegionId_);
    if (region.isEmpty()) {
        return;
    }
    QJsonObject updates{
        {"checked", regionCheckedCheck_ ? regionCheckedCheck_->isChecked() : true},
    };
    if (region.value("type").toString() == "layout") {
        updates["label"] = layoutLabelCombo_ ? layoutLabelCombo_->currentText() : region.value("label").toString();
    } else {
        updates["text"] = regionTextEdit_->toPlainText();
        updates["ignore"] = regionIgnoreCheck_ ? regionIgnoreCheck_->isChecked() : false;
        updates["reading_order"] = regionReadingOrderSpin_ ? regionReadingOrderSpin_->value() : region.value("reading_order").toInt();
    }
    const QJsonObject updatedAnnotation = AnnotationOps::updateRegion(currentAnnotation_, selectedRegionId_, updates);
    if (jsonObjectsEqual(updatedAnnotation, currentAnnotation_)) {
        return;
    }
    pushAnnotationUndoState();
    currentAnnotation_ = updatedAnnotation;
    persistCurrentAnnotation();
    refreshCanvasAnnotation();
    updateSelectedRegionPanel();
    statusBar()->showMessage(QString("Saved region: %1").arg(selectedRegionId_), 3000);
}

void MainWindow::saveImageLabels() {
    if (updatingRegionPanel_ || !context_ || currentAnnotation_.isEmpty()) {
        return;
    }
    QJsonObject updatedAnnotation = AnnotationOps::setImageLabel(currentAnnotation_, "doc_orientation", docOrientationCombo_ ? docOrientationCombo_->currentText() : QString());
    updatedAnnotation = AnnotationOps::setImageLabel(updatedAnnotation, "textline_orientation", textlineOrientationCombo_ ? textlineOrientationCombo_->currentText() : QString());
    updatedAnnotation = AnnotationOps::setImageLabel(updatedAnnotation, "table_classification", tableClassificationCombo_ ? tableClassificationCombo_->currentText() : QString());
    if (jsonObjectsEqual(updatedAnnotation, currentAnnotation_)) {
        return;
    }
    pushAnnotationUndoState();
    currentAnnotation_ = updatedAnnotation;
    persistCurrentAnnotation();
    statusBar()->showMessage("Saved image labels", 2000);
}

void MainWindow::deleteSelectedRegion() {
    if (selectedRegionId_.isEmpty() || findRegion(currentAnnotation_, selectedRegionId_).isEmpty()) {
        return;
    }
    const QString deletedId = selectedRegionId_;
    pushAnnotationUndoState();
    currentAnnotation_ = AnnotationOps::deleteRegion(currentAnnotation_, selectedRegionId_);
    selectedRegionId_.clear();
    persistCurrentAnnotation();
    refreshCanvasAnnotation();
    updateSelectedRegionPanel();
    appendLog(QString("Deleted region: %1").arg(deletedId));
}

}  // namespace ppocr
