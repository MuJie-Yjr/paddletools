#include "app/MainWindowInternal.h"
#include "application/ExportService.h"
#include "app/controllers/ModelLibraryController.h"

namespace ppocr {
QString MainWindow::currentTrainingTaskKey() const {
    if (!trainingTaskCombo_) {
        return TrainingService::trainingTasks().first().key;
    }
    const QString key = trainingTaskCombo_->currentData().toString();
    return key.isEmpty() ? TrainingService::trainingTasks().first().key : key;
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
    const auto task = TrainingService::trainingTaskByKey(taskKey);
    const TrainingOptions options = trainingOptionsFromUi(task);
    return trainingController_->buildCommand(context_ ? &*context_ : nullptr, task.key, outputDir, options);
}

QString MainWindow::paddlexPythonPath() const {
    return TrainingService::defaultPaddlexPython();
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
    const bool taskCanTrain = TrainingService::trainingTaskByKey(currentTrainingTaskKey()).trainSupported;
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
    return trainingController_ ? trainingController_->parseMetrics(logText) : QJsonObject{};
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
    const auto task = TrainingService::trainingTaskByKey(taskKey);
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

    const QJsonObject manifest = ModelLibraryController::loadManifest(*context_, task.key);
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
    for (const auto& task : TrainingService::trainingTasks()) {
        TaskRow row;
        row.task = task;
        if (context_) {
            row.manifest = ModelLibraryController::loadManifest(*context_, task.key);
            row.latestRun = ModelLibraryController::latestRun(*context_, task.key);
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
            row.sampleCount = datasetItemCount(ExportService::datasetOutputDir(*context_, task.datasetName));
        }
        row.metricText = versionMetricText(task, row.latestSuccess.isEmpty() ? row.latestVersion : row.latestSuccess);
        row.modelDir = ModelLibraryController::resolvedInferenceDir(task, row.latestSuccess.isEmpty() ? row.latestVersion : row.latestSuccess);
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
        if (!kindFilter.isEmpty() && trainingTaskKindGroupKey(row.task.kind) != kindFilter) {
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
                return static_cast<int>(left.task.kind) < static_cast<int>(right.task.kind);
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
        if (row.task.kind == TrainingTaskKind::OcrDet) {
            badgeLabel->setObjectName("TrainingBadgeDet");
        } else if (row.task.kind == TrainingTaskKind::OcrRec) {
            badgeLabel->setObjectName("TrainingBadgeRec");
        } else if (row.task.kind == TrainingTaskKind::Layout) {
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
    for (const auto& task : TrainingService::trainingTasks()) {
        const QJsonObject manifest = ModelLibraryController::loadManifest(*context_, task.key);
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
    const auto task = TrainingService::trainingTaskByKey(taskKey);
    const QJsonObject report = ModelLibraryController::compareVersions(*context_, task, selectedTrainingVersionId());
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
    const auto task = TrainingService::trainingTaskByKey(taskKey);
    const QString versionId = version.value(QStringLiteral("version_id")).toString();
    const QString defaultExportDir = context_->path(QStringLiteral("model_exports/%1/%2").arg(task.key, versionId));
    const QString exportDir = trainingExportDirEdit_ && !trainingExportDirEdit_->text().trimmed().isEmpty()
        ? trainingExportDirEdit_->text().trimmed()
        : defaultExportDir;
    const QString weightPath = trainingExportWeightEdit_ && !trainingExportWeightEdit_->text().trimmed().isEmpty()
        ? trainingExportWeightEdit_->text().trimmed()
        : version.value(QStringLiteral("best_weight_path")).toString();
    const QString inferenceDir = ModelLibraryController::resolvedInferenceDir(task, version);

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
    const auto task = TrainingService::trainingTaskByKey(currentTrainingTaskKey());
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
    setTrainingRunning(trainingController_ && trainingController_->isRunning());
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
    return ModelLibraryController::findVersion(*context_, taskKey, versionId);
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
    const auto task = TrainingService::trainingTaskByKey(taskKey);
    QJsonObject detail = version;
    const QString resolvedInferenceDir = ModelLibraryController::resolvedInferenceDir(task, version);
    detail.insert(QStringLiteral("resolved_inference_model_dir"), resolvedInferenceDir);
    QString reason;
    const bool inferenceUsable = !resolvedInferenceDir.isEmpty()
        && InferenceService::modelDirLooksUsable(resolvedInferenceDir, &reason);
    detail.insert(QStringLiteral("resolved_inference_model_usable"), inferenceUsable);
    detail.insert(QStringLiteral("resolved_inference_model_error"), reason);
    if (task.kind == TrainingTaskKind::OcrDet) {
        detail.insert(QStringLiteral("currently_selected_prediction_model"), selectedPredictionModelDir(predictDetModelCombo_));
    } else if (task.kind == TrainingTaskKind::OcrRec) {
        detail.insert(QStringLiteral("currently_selected_prediction_model"), selectedPredictionModelDir(predictRecModelCombo_));
    } else if (task.kind == TrainingTaskKind::Layout) {
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
    const auto task = TrainingService::trainingTaskByKey(taskKey);
    try {
        ModelLibraryController::setCurrentVersion(*context_, task.key, versionId);
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
    const auto task = TrainingService::trainingTaskByKey(taskKey);
    if (QMessageBox::question(
            this,
            QStringLiteral("删除训练版本"),
            QStringLiteral("删除版本 %1 以及它的文件？").arg(versionId))
        != QMessageBox::Yes) {
        return;
    }

    try {
        ModelLibraryController::deleteVersion(*context_, task.key, versionId);
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
            ModelLibraryController::deleteVersion(*context_, task.key, versionId, true, true);
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

    const auto task = TrainingService::trainingTaskByKey(currentTrainingTaskKey());
    const TrainingOptions options = trainingOptionsFromUi(task);
    const auto result = trainingController_->runPreflight(*context_, task.key, options);
    if (trainingPreview_) {
        trainingPreview_->setPlainText(QString::fromUtf8(QJsonDocument(result.report).toJson(QJsonDocument::Indented)));
    }
    if (trainingExpertPreview_) {
        trainingExpertPreview_->setPlainText(QString::fromUtf8(QJsonDocument(result.report).toJson(QJsonDocument::Indented)));
    }
    appendLog(QStringLiteral("训练前检查 %1：%2").arg(task.key, result.ok ? QStringLiteral("通过") : QStringLiteral("失败")));
    if (result.ok) {
        QMessageBox::information(this, QStringLiteral("训练前检查"), QStringLiteral("训练前检查通过。"));
    } else {
        QStringList lines = result.errors;
        const QJsonArray suggestions = result.report.value(QStringLiteral("suggestions")).toArray();
        if (!suggestions.isEmpty()) {
            lines.append(QStringLiteral(""));
            lines.append(QStringLiteral("处理建议："));
            for (const auto& value : suggestions) {
                lines.append(QStringLiteral("- %1").arg(value.toString()));
            }
        }
        QMessageBox::warning(
            this,
            QStringLiteral("训练前检查"),
            lines.isEmpty()
                ? QStringLiteral("发现 %1 个问题，请查看训练日志。").arg(result.errors.size())
                : lines.join(QLatin1Char('\n')));
    }
}

void MainWindow::previewTrainingCommand() {
    const auto task = TrainingService::trainingTaskByKey(currentTrainingTaskKey());
    const QString previewVersionId = ModelLibraryController::newVersionId(task.key);
    const QString outputDir = context_
        ? ModelLibraryController::versionDir(*context_, task.key, previewVersionId)
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
                context_ ? ExportService::datasetOutputDir(*context_, task.datasetName) : QDir(baseDir_).filePath(QString("exports/%1/current").arg(task.datasetName)),
                outputDir,
                command.displayText()));
    if (trainingExpertPreview_) {
        trainingExpertPreview_->setPlainText(trainingPreview_->toPlainText());
    }
}

void MainWindow::copyTrainingCommand() {
    const auto task = TrainingService::trainingTaskByKey(currentTrainingTaskKey());
    const QString previewVersionId = ModelLibraryController::newVersionId(task.key);
    const QString outputDir = context_
        ? ModelLibraryController::versionDir(*context_, task.key, previewVersionId)
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
    if (trainingController_ && trainingController_->isRunning()) {
        QMessageBox::information(this, QStringLiteral("训练进行中"), QStringLiteral("请先停止当前训练进程。"));
        return;
    }

    const auto task = TrainingService::trainingTaskByKey(currentTrainingTaskKey());
    if (!task.trainSupported) {
        QMessageBox::information(
            this,
            QStringLiteral("暂不支持训练"),
            task.note.isEmpty() ? QStringLiteral("当前任务暂不支持训练：%1").arg(task.key) : task.note);
        return;
    }

    if (trainingPreview_) {
        trainingPreview_->clear();
    }
    trainingLogBuffer_.clear();
    clearTrainingMetrics();
    appendTrainingText(QStringLiteral("[training] preparing dataset and version for %1...\n").arg(task.title));

    const TrainingOptions options = trainingOptionsFromUi(task);
    QString errorMessage;
    if (!trainingController_ || !trainingController_->startTraining(*context_, task.key, options, &errorMessage)) {
        QMessageBox::critical(this, QStringLiteral("训练启动失败"), errorMessage);
        appendTrainingText(QStringLiteral("[training] start failed: %1\n").arg(errorMessage));
        setTrainingRunning(false);
        return;
    }
    statusBar()->showMessage(QStringLiteral("PaddleX 训练已启动"), 3000);
}

void MainWindow::stopTraining() {
    if (!trainingController_ || !trainingController_->isRunning()) {
        return;
    }
    trainingController_->stopTraining();
}

void MainWindow::handleTrainingPrepared(
    const QString& taskKey,
    const QString& taskKind,
    const QString& runId,
    const QString& versionId,
    const QString& datasetDir,
    const QString& versionDir,
    const QString& commandText) {
    activeTrainingTaskKey_ = taskKey;
    activeTrainingTaskKind_ = taskKind;
    activeTrainingRunId_ = runId;
    activeTrainingVersionId_ = versionId;
    activeTrainingVersionDir_ = versionDir;

    appendTrainingText(QStringLiteral("Task: %1\nRun: %2\nVersion: %3\nDataset: %4\nOutput: %5\n\n%6\n\n")
        .arg(taskKey, runId, versionId, datasetDir, versionDir, commandText));
    refreshTrainingVersions();
}

void MainWindow::handleTrainingCompleted(
    const QString& status,
    int exitCode,
    const QString& errorSummary,
    const QJsonObject& metrics,
    const QJsonObject& finishedVersion,
    const QString& versionId) {
    const QString finishedVersionId = versionId.isEmpty() ? activeTrainingVersionId_ : versionId;
    const QString statusText = statusLabelText(status);
    appendTrainingText(QStringLiteral("\nTraining status: %1, exit code: %2\n").arg(statusText).arg(exitCode));
    if (!errorSummary.isEmpty()) {
        appendTrainingText(QStringLiteral("Error: %1\n").arg(errorSummary));
    }
    if (!metrics.isEmpty()) {
        appendTrainingText(QString("Metrics: %1\n").arg(QString::fromUtf8(QJsonDocument(metrics).toJson(QJsonDocument::Compact))));
    }
    if (!finishedVersion.isEmpty()) {
        const QString bestWeight = finishedVersion.value("best_weight_path").toString();
        const QString inferenceDir = finishedVersion.value("inference_model_dir").toString();
        if (!bestWeight.isEmpty()) {
            appendTrainingText(QStringLiteral("Best weight: %1\n").arg(bestWeight));
        }
        if (!inferenceDir.isEmpty()) {
            appendTrainingText(QStringLiteral("Inference model: %1\n").arg(inferenceDir));
        } else if (status == QStringLiteral("success")) {
            appendTrainingText(QStringLiteral("Inference model: not found under this version yet.\n"));
        }
    }
    appendLog(QStringLiteral("Training %1: %2").arg(statusText, finishedVersionId));
    statusBar()->showMessage(QStringLiteral("PaddleX training %1").arg(statusText), 5000);

    activeTrainingRunId_.clear();
    activeTrainingTaskKey_.clear();
    activeTrainingTaskKind_.clear();
    activeTrainingVersionId_.clear();
    activeTrainingVersionDir_.clear();
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
    const auto report = InferenceService::predictOcrImage(imagePath, outputDir, InferenceService::defaultOcrConfig(baseDir_));
    predictionPreview_->setPlainText(QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented)));
    updatePredictionImagePreview(outputDir);
}

}  // namespace ppocr
