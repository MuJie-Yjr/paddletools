#include "app/MainWindowInternal.h"
#include "app/controllers/ModelLibraryController.h"

namespace ppocr {
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
    const auto config = InferenceService::defaultOcrConfig(baseDir_);
    addPredictionModelChoices(predictDetModelCombo_, context_ ? &(*context_) : nullptr, tasksForKind(QStringLiteral("det")), config.textDetectionModelDir);
    addPredictionModelChoices(predictRecModelCombo_, context_ ? &(*context_) : nullptr, tasksForKind(QStringLiteral("rec")), config.textRecognitionModelDir);
    if (predictClsModelCombo_ && predictClsTaskCombo_) {
        const QString clsTask = predictClsTaskCombo_->currentData().toString();
        const auto clsConfig = InferenceService::defaultClassificationConfig(clsTask, baseDir_);
        addPredictionModelChoices(
            predictClsModelCombo_,
            context_ ? &(*context_) : nullptr,
            classificationTasksForPrediction(clsTask),
            clsConfig.modelDir);
    }
    if (predictLayoutModelCombo_) {
        const auto layoutConfig = InferenceService::defaultLayoutConfig(baseDir_);
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
            ? InferenceService::layoutModelDirLooksUsable(row.path, &reason)
            : InferenceService::modelDirLooksUsable(row.path, &reason);
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

void MainWindow::exportPredictionResults() {
    if (!predictOutputEdit_) {
        return;
    }
    const QString sourceDir = predictOutputEdit_->text().trimmed();
    if (sourceDir.isEmpty() || !QFileInfo(sourceDir).isDir()) {
        QMessageBox::information(this, QStringLiteral("没有预测结果"), QStringLiteral("请先运行预测，或选择一个已有的预测输出目录。"));
        return;
    }
    const QString targetRoot = QFileDialog::getExistingDirectory(this, QStringLiteral("选择预测结果导出目录"), sourceDir);
    if (targetRoot.isEmpty()) {
        return;
    }
    const QString targetDir = QDir(targetRoot).filePath(QStringLiteral("prediction_results_%1")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss"))));
    try {
        const int copied = copyDirectoryContents(sourceDir, targetDir);
        appendPredictionText(QStringLiteral("[workbench] exported prediction results: %1 files -> %2\n")
            .arg(copied)
            .arg(targetDir));
        statusBar()->showMessage(QStringLiteral("预测结果已导出：%1").arg(targetDir), 5000);
    } catch (const std::exception& exc) {
        QMessageBox::warning(this, QStringLiteral("导出预测结果失败"), QString::fromUtf8(exc.what()));
    }
}

void MainWindow::addPredictionInputToErrorSamples() {
    if (!context_) {
        QMessageBox::information(this, QStringLiteral("没有项目"), QStringLiteral("请先打开或创建项目。"));
        return;
    }
    const QString input = predictInputEdit_ ? predictInputEdit_->text().trimmed() : QString();
    if (input.isEmpty() || !QFileInfo::exists(input)) {
        QMessageBox::information(this, QStringLiteral("没有测试输入"), QStringLiteral("请先选择测试图片或文件夹。"));
        return;
    }

    const QString sampleDir = context_->path(QStringLiteral("predictions/error_samples/%1")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss"))));
    const QString inputDir = QDir(sampleDir).filePath(QStringLiteral("input"));
    QDir().mkpath(inputDir);

    int copied = 0;
    const QFileInfo inputInfo(input);
    try {
        if (inputInfo.isFile()) {
            const QString target = QDir(inputDir).filePath(inputInfo.fileName());
            if (!QFile::copy(inputInfo.absoluteFilePath(), target)) {
                throw std::runtime_error(QStringLiteral("failed to copy %1").arg(inputInfo.absoluteFilePath()).toStdString());
            }
            copied = 1;
        } else if (inputInfo.isDir()) {
            const QStringList images = filesWithSuffixes(inputInfo.absoluteFilePath(), imageFileSuffixes());
            for (const QString& image : images) {
                copyPreservingRelativePath(inputInfo.absoluteFilePath(), inputDir, image);
                ++copied;
            }
        }
    } catch (const std::exception& exc) {
        QMessageBox::warning(this, QStringLiteral("加入错误样本集失败"), QString::fromUtf8(exc.what()));
        return;
    }

    QJsonObject manifest;
    manifest.insert(QStringLiteral("created_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    manifest.insert(QStringLiteral("source_input"), inputInfo.absoluteFilePath());
    manifest.insert(QStringLiteral("copied_files"), copied);
    manifest.insert(QStringLiteral("prediction_output"), predictOutputEdit_ ? QFileInfo(predictOutputEdit_->text().trimmed()).absoluteFilePath() : QString());
    manifest.insert(QStringLiteral("mode"), predictModeCombo_ ? predictModeCombo_->currentData().toString() : QStringLiteral("ocr"));
    manifest.insert(QStringLiteral("det_model"), selectedPredictionModelDir(predictDetModelCombo_));
    manifest.insert(QStringLiteral("rec_model"), selectedPredictionModelDir(predictRecModelCombo_));
    manifest.insert(QStringLiteral("cls_model"), selectedPredictionModelDir(predictClsModelCombo_));
    manifest.insert(QStringLiteral("layout_model"), selectedPredictionModelDir(predictLayoutModelCombo_));
    QFile manifestFile(QDir(sampleDir).filePath(QStringLiteral("error_sample.json")));
    if (manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        manifestFile.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
    }

    appendPredictionText(QStringLiteral("[workbench] added %1 input file(s) to error samples: %2\n")
        .arg(copied)
        .arg(sampleDir));
    statusBar()->showMessage(QStringLiteral("已加入错误样本集：%1").arg(sampleDir), 5000);
}

void MainWindow::comparePredictionWithPreviousVersion() {
    if (!context_) {
        QMessageBox::information(this, QStringLiteral("没有项目"), QStringLiteral("请先打开或创建项目。"));
        return;
    }
    auto activeModelPath = [this]() {
        const QString mode = predictModeCombo_ ? predictModeCombo_->currentData().toString() : QStringLiteral("ocr");
        if (mode == QStringLiteral("classification")) {
            return selectedPredictionModelDir(predictClsModelCombo_);
        }
        if (mode == QStringLiteral("layout")) {
            return selectedPredictionModelDir(predictLayoutModelCombo_);
        }
        return selectedPredictionModelDir(predictDetModelCombo_);
    };
    auto normalizedPath = [](const QString& path) {
        const QFileInfo info(path);
        const QString canonical = info.canonicalFilePath();
        return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
    };

    const QString selectedPath = activeModelPath();
    if (selectedPath.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("没有模型"), QStringLiteral("请先选择一个模型版本。"));
        return;
    }
    const QString selectedCanonical = normalizedPath(selectedPath);

    struct VersionCandidate {
        TrainingTaskSpec task;
        QJsonObject version;
        QString inferenceDir;
    };
    QList<VersionCandidate> taskVersions;
    int currentIndex = -1;
    for (const auto& task : TrainingService::trainingTasks()) {
        const QJsonArray versions = ModelLibraryController::loadManifest(*context_, task.key).value(QStringLiteral("versions")).toArray();
        QList<VersionCandidate> candidates;
        for (const auto& value : versions) {
            const QJsonObject version = value.toObject();
            const QString inferenceDir = ModelLibraryController::resolvedInferenceDir(task, version);
            if (inferenceDir.isEmpty()) {
                continue;
            }
            candidates.append(VersionCandidate{task, version, inferenceDir});
        }
        for (int i = 0; i < candidates.size(); ++i) {
            if (normalizedPath(candidates.at(i).inferenceDir) == selectedCanonical) {
                taskVersions = candidates;
                currentIndex = i;
                break;
            }
        }
        if (currentIndex >= 0) {
            break;
        }
    }

    if (currentIndex < 0) {
        QMessageBox::information(this, QStringLiteral("无法对比"), QStringLiteral("当前模型不来自项目训练版本，无法找到上一版本。"));
        return;
    }
    const QString currentVersionId = taskVersions.at(currentIndex).version.value(QStringLiteral("version_id")).toString();
    std::stable_sort(taskVersions.begin(), taskVersions.end(), [](const VersionCandidate& left, const VersionCandidate& right) {
        const QDateTime leftTime = storeObjectTime(left.version);
        const QDateTime rightTime = storeObjectTime(right.version);
        if (leftTime.isValid() != rightTime.isValid()) {
            return leftTime.isValid();
        }
        if (leftTime.isValid() && leftTime != rightTime) {
            return leftTime < rightTime;
        }
        return left.version.value(QStringLiteral("version_id")).toString()
            < right.version.value(QStringLiteral("version_id")).toString();
    });
    currentIndex = -1;
    for (int i = 0; i < taskVersions.size(); ++i) {
        if (taskVersions.at(i).version.value(QStringLiteral("version_id")).toString() == currentVersionId) {
            currentIndex = i;
            break;
        }
    }
    if (currentIndex < 0) {
        QMessageBox::information(this, QStringLiteral("无法对比"), QStringLiteral("当前版本记录缺少可用于排序的版本信息。"));
        return;
    }
    if (currentIndex == 0) {
        QMessageBox::information(this, QStringLiteral("没有上一版本"), QStringLiteral("当前模型已经是该任务的第一个可用版本。"));
        return;
    }

    const VersionCandidate current = taskVersions.at(currentIndex);
    const VersionCandidate previous = taskVersions.at(currentIndex - 1);
    bool currentOk = false;
    bool previousOk = false;
    const double currentMetric = TrainingService::mainMetricValue(
        current.version.value(QStringLiteral("metrics")).toObject(), current.task.kind, &currentOk);
    const double previousMetric = TrainingService::mainMetricValue(
        previous.version.value(QStringLiteral("metrics")).toObject(), previous.task.kind, &previousOk);

    QJsonObject summary;
    summary.insert(QStringLiteral("task_key"), current.task.key);
    summary.insert(QStringLiteral("current_version"), current.version.value(QStringLiteral("version_id")).toString());
    summary.insert(QStringLiteral("previous_version"), previous.version.value(QStringLiteral("version_id")).toString());
    summary.insert(QStringLiteral("current_inference_dir"), current.inferenceDir);
    summary.insert(QStringLiteral("previous_inference_dir"), previous.inferenceDir);
    if (currentOk) {
        summary.insert(QStringLiteral("current_main_metric"), currentMetric);
    }
    if (previousOk) {
        summary.insert(QStringLiteral("previous_main_metric"), previousMetric);
    }
    if (currentOk && previousOk) {
        summary.insert(QStringLiteral("metric_delta"), currentMetric - previousMetric);
    }
    if (predictionStructuredText_) {
        predictionStructuredText_->setPlainText(QString::fromUtf8(QJsonDocument(summary).toJson(QJsonDocument::Indented)));
    }
    appendPredictionText(QStringLiteral("[workbench] compared model versions: %1 vs %2\n")
        .arg(summary.value(QStringLiteral("current_version")).toString(),
             summary.value(QStringLiteral("previous_version")).toString()));
    statusBar()->showMessage(QStringLiteral("已生成上一版本对比摘要"), 4000);
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
    if (predictionController_ && predictionController_->isRunning()) {
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
    if (!InferenceService::modelDirLooksUsable(detModelDir, &reason)) {
        QMessageBox::warning(this, "Det model invalid", reason);
        cancelPreparedRun();
        return;
    }
    if (!InferenceService::modelDirLooksUsable(recModelDir, &reason)) {
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

    PredictionRunRequest request;
    request.program = toolPath;
    request.arguments = arguments;
    request.workingDirectory = baseDir_;
    QString errorMessage;
    if (!predictionController_ || !predictionController_->startPrediction(request, &errorMessage)) {
        if (errorMessage.isEmpty()) {
            errorMessage = QStringLiteral("Prediction controller is not available.");
        }
        appendPredictionText(QStringLiteral("\nPrediction start failed: %1\n").arg(errorMessage));
        QMessageBox::critical(this, QStringLiteral("预测启动失败"), errorMessage);
        cancelPreparedRun();
        setPredictionRunning(false);
        return;
    }
    if (predictionStatusLabel_) {
        predictionStatusLabel_->setText(QStringLiteral("OCR 预测运行中"));
    }
    statusBar()->showMessage(QStringLiteral("C++ OCR 预测已启动"), 3000);
}

void MainWindow::startClassificationPrediction() {
    if (predictionController_ && predictionController_->isRunning()) {
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
    if (!InferenceService::modelDirLooksUsable(modelDir, &reason)) {
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

    PredictionRunRequest request;
    request.program = toolPath;
    request.arguments = arguments;
    request.workingDirectory = baseDir_;
    QString errorMessage;
    if (!predictionController_ || !predictionController_->startPrediction(request, &errorMessage)) {
        if (errorMessage.isEmpty()) {
            errorMessage = QStringLiteral("Prediction controller is not available.");
        }
        appendPredictionText(QStringLiteral("\nPrediction start failed: %1\n").arg(errorMessage));
        QMessageBox::critical(this, QStringLiteral("预测启动失败"), errorMessage);
        cancelPreparedRun();
        setPredictionRunning(false);
        return;
    }
    if (predictionStatusLabel_) {
        predictionStatusLabel_->setText(QStringLiteral("分类预测运行中"));
    }
    statusBar()->showMessage(QStringLiteral("C++ 分类预测已启动"), 3000);
}

void MainWindow::startLayoutPrediction() {
    if (predictionController_ && predictionController_->isRunning()) {
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
    if (!InferenceService::layoutModelDirLooksUsable(modelDir, &reason)) {
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

    PredictionRunRequest request;
    request.program = toolPath;
    request.arguments = arguments;
    request.workingDirectory = baseDir_;
    QString errorMessage;
    if (!predictionController_ || !predictionController_->startPrediction(request, &errorMessage)) {
        if (errorMessage.isEmpty()) {
            errorMessage = QStringLiteral("Prediction controller is not available.");
        }
        appendPredictionText(QStringLiteral("\nPrediction start failed: %1\n").arg(errorMessage));
        QMessageBox::critical(this, QStringLiteral("预测启动失败"), errorMessage);
        cancelPreparedRun();
        setPredictionRunning(false);
        return;
    }
    if (predictionStatusLabel_) {
        predictionStatusLabel_->setText(QStringLiteral("版面预测运行中"));
    }
    statusBar()->showMessage(QStringLiteral("版面预测已启动"), 3000);
}

void MainWindow::stopPrediction() {
    if (!predictionController_ || !predictionController_->isRunning()) {
        return;
    }
    predictionController_->stopPrediction();
}

void MainWindow::handlePredictionFinished(bool processOk, int exitCode, qint64 elapsedMs) {
    const bool ok = processOk;
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
    activePredictionStagingDir_.clear();
    activePredictionPublishDir_.clear();
    setPredictionRunning(false);
}

void MainWindow::handlePredictionStartFailed(const QString& errorMessage) {
    appendPredictionText(QStringLiteral("\n预测启动失败：%1\n").arg(errorMessage));
    if (!activePredictionStagingDir_.isEmpty()) {
        QDir(activePredictionStagingDir_).removeRecursively();
    }
    activePredictionStagingDir_.clear();
    activePredictionPublishDir_.clear();
    if (predictionStatusLabel_) {
        predictionStatusLabel_->setText(QStringLiteral("启动失败"));
    }
    QMessageBox::critical(this, QStringLiteral("预测启动失败"), errorMessage);
    setPredictionRunning(false);
}
}  // namespace ppocr
