#pragma once
#include "app/MainWindow.h"

#include "application/AnnotationService.h"
#include "application/EnvironmentService.h"
#include "application/InferenceService.h"
#include "application/TrainingService.h"

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
#include <QElapsedTimer>
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
    const double metric = TrainingService::mainMetricValue(metrics, version.value("task_kind").toString(), &hasMetric);
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
    if (!explicitDir.isEmpty() && InferenceService::modelDirLooksUsable(explicitDir, &reason)) {
        return explicitDir;
    }
    const QString root = version.value("version_dir").toString(version.value("output_dir").toString());
    if (root.isEmpty()) {
        return {};
    }
    const QString preferred = QDir(root).filePath(task.inferDirRel);
    if (InferenceService::modelDirLooksUsable(preferred, &reason)) {
        return preferred;
    }
    return {};
}

QList<TrainingTaskSpec> tasksForKind(const QString& kind) {
    QList<TrainingTaskSpec> result;
    for (const auto& task : TrainingService::trainingTasks()) {
        if (task.trainSupported && trainingTaskKindGroupKey(task.kind) == kind) {
            result.append(task);
        }
    }
    return result;
}

QList<TrainingTaskSpec> classificationTasksForPrediction(const QString& predictionTask) {
    QList<TrainingTaskSpec> result;
    for (const auto& task : TrainingService::trainingTasks()) {
        if (!task.trainSupported || !isClassificationTrainingTaskKind(task.kind)) {
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
    return InferenceService::resolveModelDir(outerDir.absoluteFilePath());
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
            if (!InferenceService::modelDirLooksUsable(modelDir, &reason)) {
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
        return TrainingService::defaultBaseDir();
    }
    const QString resolved = InferenceService::resolveModelDir(modelDir);
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
    return TrainingService::defaultBaseDir();
}

QString localModelCategoryFromPath(const QString& modelDir) {
    const QString resolved = InferenceService::resolveModelDir(modelDir);
    const QFileInfo info(resolved);
    QString category = inferModelCategory(info.fileName());
    if (category != QStringLiteral("other")) {
        return category;
    }
    category = inferModelCategory(info.dir().dirName());
    return category == QStringLiteral("other") ? QString() : category;
}

QString modelChoiceTitleFromPath(const QString& modelDir) {
    const QString resolved = InferenceService::resolveModelDir(modelDir);
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
            || combo->findData(InferenceService::resolveModelDir(choice.path)) >= 0) {
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
        const QString modelDir = InferenceService::resolveModelDir(defaultModelDir);
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
        const QString modelDir = InferenceService::resolveModelDir(defaultModelDir);
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
    const QString resolved = canonicalExistingOrCleanPath(InferenceService::resolveModelDir(path));
    if (!resolved.isEmpty()) {
        paths.insert(resolved);
    }
    return paths;
}

QString preferredModelDir(const ProjectContext* context, const TrainingTaskSpec& task, const QString& defaultModelDir) {
    if (!context) {
        return defaultModelDir;
    }
    const QJsonObject manifest = TrainingService::loadVersionManifest(*context, task.key);
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

QString splitDisplayText(PageSplit value);
QString statusDisplayText(const QString& value);

QString pageListText(const PageInfo& page) {
    const QString name = QFileInfo(page.imagePath).fileName().isEmpty()
        ? page.assetId
        : QFileInfo(page.imagePath).fileName();
    return QString("%1\n%2 x %3    %4    %5")
        .arg(name)
        .arg(page.width)
        .arg(page.height)
        .arg(toString(page.status))
        .arg(toString(page.split));
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

QString splitDisplayText(PageSplit value) {
    if (value == PageSplit::Train) {
        return QStringLiteral("train");
    }
    if (value == PageSplit::Val) {
        return QStringLiteral("val");
    }
    if (value == PageSplit::Test) {
        return QStringLiteral("test");
    }
    return QStringLiteral("unassigned");
}

QString statusDisplayText(const QString& value) {
    if (value == QStringLiteral("unlabeled")) {
        return QStringLiteral("未标注");
    }
    if (value == QStringLiteral("labeled")) {
        return QStringLiteral("已标注");
    }
    if (value == QStringLiteral("checked") || value == QStringLiteral("validated")) {
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

inline QString absoluteCleanPath(const QString& path) {
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

inline bool pathIsSameOrInside(const QString& candidate, const QString& root) {
    const QString candidatePath = absoluteCleanPath(candidate);
    const QString rootPath = absoluteCleanPath(root);
    if (candidatePath.compare(rootPath, Qt::CaseInsensitive) == 0) {
        return true;
    }
    const QString relative = QDir(rootPath).relativeFilePath(candidatePath);
    return relative != QStringLiteral("..")
        && !relative.startsWith(QStringLiteral("../"))
        && !relative.startsWith(QStringLiteral("..\\"))
        && !QDir::isAbsolutePath(relative);
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
    const auto parsed = trainingTaskKindFromString(kind);
    const QString group = parsed ? trainingTaskKindGroupKey(*parsed) : kind;
    if (group == QStringLiteral("det")) {
        return QStringLiteral("Det");
    }
    if (group == QStringLiteral("rec")) {
        return QStringLiteral("Rec");
    }
    if (group == QStringLiteral("cls")) {
        return QStringLiteral("Cls");
    }
    if (group == QStringLiteral("layout")) {
        return QStringLiteral("Layout");
    }
    if (group == QStringLiteral("uvdoc")) {
        return QStringLiteral("UVDoc");
    }
    return kind;
}

QString taskKindLabel(TrainingTaskKind kind) {
    return taskKindLabel(trainingTaskKindGroupKey(kind));
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
    const double metric = TrainingService::mainMetricValue(version.value(QStringLiteral("metrics")).toObject(), task.kind, &ok);
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
    QDir root(rootPath);
    if (!root.exists()) {
        return 0;
    }
    auto countAt = [](const QDir& rootDir) {
        int total = 0;
        for (const QString& name : {QStringLiteral("train.txt"), QStringLiteral("val.txt"), QStringLiteral("label.txt")}) {
            QFile file(rootDir.filePath(name));
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
        const QDir images(rootDir.filePath(QStringLiteral("images")));
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
    };
    const QDir current(root.filePath(QStringLiteral("current")));
    if (current.exists()) {
        const int currentCount = countAt(current);
        if (currentCount > 0) {
            return currentCount;
        }
    }
    return countAt(root);
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
        if (InferenceService::modelDirLooksUsable(dir.absolutePath(), &reason)) {
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
    InferenceService::ClassificationConfig config;
};

struct AsyncPrelabelStepSummary {
    QString name;
    int okCount = 0;
    int failedCount = 0;
    int regionCount = 0;
    int uncheckedRegionCount = 0;
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

inline bool regionMatchesAnyType(const QJsonObject& region, const QList<RegionType>& types) {
    if (types.isEmpty()) {
        return true;
    }
    const auto type = regionTypeFromString(region.value(QStringLiteral("type")).toString());
    return type.has_value() && types.contains(type.value());
}

inline bool regionLooksAutoPrelabelled(const QJsonObject& region) {
    const QString sourceText = region.value(QStringLiteral("source")).toString();
    const auto source = annotationSourceFromString(sourceText);
    return sourceText == QStringLiteral("auto")
        || (source.has_value()
            && (source.value() == AnnotationSource::OcrPrelabel
                || source.value() == AnnotationSource::LayoutPrelabel));
}

inline int uncheckedAutoRegionCount(const QJsonObject& annotation, const QList<RegionType>& types = {}) {
    int count = 0;
    for (const auto& value : annotation.value(QStringLiteral("regions")).toArray()) {
        const QJsonObject region = value.toObject();
        if (region.value(QStringLiteral("checked")).toBool(false)) {
            continue;
        }
        if (!regionLooksAutoPrelabelled(region) || !regionMatchesAnyType(region, types)) {
            continue;
        }
        ++count;
    }
    return count;
}

inline int setAnnotationRegionsChecked(QJsonObject* annotation, bool checked = true) {
    if (!annotation) {
        return 0;
    }
    int changed = 0;
    QJsonArray regions = annotation->value(QStringLiteral("regions")).toArray();
    for (int i = 0; i < regions.size(); ++i) {
        QJsonObject region = regions.at(i).toObject();
        if (region.value(QStringLiteral("checked")).toBool(false) == checked) {
            continue;
        }
        region.insert(QStringLiteral("checked"), checked);
        regions[i] = region;
        ++changed;
    }
    annotation->insert(QStringLiteral("regions"), regions);

    QJsonArray crops = annotation->value(QStringLiteral("rec_crops")).toArray();
    for (int i = 0; i < crops.size(); ++i) {
        QJsonObject crop = crops.at(i).toObject();
        if (crop.value(QStringLiteral("checked")).toBool(false) == checked) {
            continue;
        }
        crop.insert(QStringLiteral("checked"), checked);
        crops[i] = crop;
    }
    if (!crops.isEmpty()) {
        annotation->insert(QStringLiteral("rec_crops"), crops);
    }
    if (checked && !regions.isEmpty()) {
        annotation->insert(QStringLiteral("status"), toString(PageStatus::Checked));
    }
    return changed;
}

inline int totalPrelabelRegionCount(const AsyncPrelabelJobResult& result) {
    int total = 0;
    for (const auto& step : result.steps) {
        total += step.regionCount;
    }
    return total;
}

inline int totalUncheckedPrelabelRegionCount(const AsyncPrelabelJobResult& result) {
    int total = 0;
    for (const auto& step : result.steps) {
        total += step.uncheckedRegionCount;
    }
    return total;
}

inline QString prelabelCheckedOnlyWarningText(int uncheckedCount) {
    if (uncheckedCount <= 0) {
        return {};
    }
    return QStringLiteral("其中 %1 个未确认，不会进入 checked-only 导出和训练集。\n请点击“批量确认当前页”或“批量确认全项目”，也可以在确认风险后关闭 checked-only 导出。")
        .arg(uncheckedCount);
}

inline QString prelabelStatusMessage(const AsyncPrelabelJobResult& result) {
    const int regionCount = totalPrelabelRegionCount(result);
    const int uncheckedCount = totalUncheckedPrelabelRegionCount(result);
    if (uncheckedCount > 0) {
        return QStringLiteral("已完成预标注：检测到 %1 个框，其中 %2 个未确认，不会进入训练集")
            .arg(regionCount)
            .arg(uncheckedCount);
    }
    return QStringLiteral("%1完成，用时 %2 秒")
        .arg(result.title)
        .arg(result.elapsedMs / 1000.0, 0, 'f', 1);
}

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
        if (step.uncheckedRegionCount > 0) {
            line += QStringLiteral("，未确认 %1").arg(step.uncheckedRegionCount);
        }
        lines.append(line);
    }
    const QString checkedOnlyWarning = prelabelCheckedOnlyWarningText(totalUncheckedPrelabelRegionCount(result));
    if (!checkedOnlyWarning.isEmpty()) {
        lines.append(QString());
        lines.append(checkedOnlyWarning);
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

}  // namespace ppocr
