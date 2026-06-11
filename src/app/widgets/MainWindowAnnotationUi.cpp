#include "app/MainWindowInternal.h"

namespace ppocr {
void MainWindow::prelabelSelectedPage() {
    startPrelabelCurrentPageAsync(true, false, false, false, QStringLiteral("OCR prelabel current page"));
    return;
    if (prelabelAllRunning_) {
        QMessageBox::information(this, QStringLiteral("Task running"), QStringLiteral("Wait for the current background task to finish."));
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
        auto config = InferenceService::defaultOcrConfig(baseDir_);
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
        const auto report = InferenceService::predictOcrImage(
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

        const auto annotation = AnnotationController::readAnnotationFile(page.annotationPath);
        const auto updated = AnnotationService::applyOcrResult(annotation, report);
        AnnotationController::writeAnnotationFile(page.annotationPath, updated);
        appendLog(QString("Prelabelled %1 with %2 OCR region(s), threshold=%3, filtered=%4")
            .arg(page.assetId)
            .arg(AnnotationService::autoOcrRegionCount(updated))
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
    startPrelabelCurrentPageAsync(false, true, false, false, QStringLiteral("Classification prelabel current page"));
    return;
    if (prelabelAllRunning_) {
        QMessageBox::information(this, QStringLiteral("Task running"), QStringLiteral("Wait for the current background task to finish."));
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
        QJsonObject annotation = AnnotationController::readAnnotationFile(page.annotationPath);
        QJsonArray reports;
        QStringList applied;
        for (const Task& task : tasks) {
            InferenceService::ClassificationConfig config = InferenceService::defaultClassificationConfig(task.predictionTask, baseDir_);
            const QString selectedModel = selectedPredictionModelDir(task.modelCombo);
            config.modelDir = selectedModel.isEmpty()
                ? preferredModelDir(
                    context_ ? &(*context_) : nullptr,
                    classificationTasksForPrediction(task.predictionTask),
                    config.modelDir)
                : selectedModel;
            config.device = labelPrelabelDevice();

            const QString outputDir = context_->path(QString("cache/cls_prelabel/%1/%2").arg(page.assetId, task.predictionTask));
            const auto report = InferenceService::predictClassificationImage(page.imagePath, outputDir, config, true);
            reports.append(report);
            if (!report.value("ok").toBool()) {
                throw std::runtime_error(report.value("error").toString("C++ classification prelabel failed").toStdString());
            }

            const QString label = AnnotationService::normalizedClassificationLabel(task.predictionTask, report);
            annotation = AnnotationService::applyClassificationResult(annotation, task.predictionTask, report);
            if (!label.isEmpty()) {
                applied.append(QString("%1=%2").arg(task.predictionTask, label));
            }
        }

        AnnotationController::writeAnnotationFile(page.annotationPath, annotation);
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
    startPrelabelCurrentPageAsync(false, false, true, false, QStringLiteral("Table prelabel current page"));
    return;
    if (prelabelAllRunning_) {
        QMessageBox::information(this, QStringLiteral("Task running"), QStringLiteral("Wait for the current background task to finish."));
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
    InferenceService::ClassificationConfig config = InferenceService::defaultClassificationConfig(QStringLiteral("table_classification"), baseDir_);
    config.modelDir = preferredModelDir(
        context_ ? &(*context_) : nullptr,
        classificationTasksForPrediction(QStringLiteral("table_classification")),
        config.modelDir);
    config.device = labelPrelabelDevice();
    QString reason;
    if (!InferenceService::modelDirLooksUsable(config.modelDir, &reason)) {
        QMessageBox::warning(this, "Table classification model invalid", reason);
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    statusBar()->showMessage(QString("Running table classification prelabel: %1").arg(page.assetId));
    try {
        const QString outputDir = context_->path(QString("cache/table_cls_prelabel/%1").arg(page.assetId));
        const auto report = InferenceService::predictClassificationImage(page.imagePath, outputDir, config, true);
        if (predictionPreview_) {
            predictionPreview_->setPlainText(QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented)));
        }
        if (!report.value("ok").toBool()) {
            throw std::runtime_error(report.value("error").toString("table classification prelabel failed").toStdString());
        }

        const QJsonObject annotation = annotationController_->hasCurrentAnnotation()
            ? annotationController_->currentAnnotation()
            : AnnotationController::readAnnotationFile(page.annotationPath);
        const QJsonObject updated = AnnotationService::applyClassificationResult(
            annotation,
            QStringLiteral("table_classification"),
            report);
        if (annotationController_->replaceCurrentAnnotation(updated)) {
            persistCurrentAnnotation();
            refreshCanvasAnnotation();
            refreshImageLabelPanel();
            updateSelectedRegionPanel();
        }
        const QString label = AnnotationService::normalizedClassificationLabel(QStringLiteral("table_classification"), report);
        appendLog(QString("Table classification prelabelled %1: %2").arg(page.assetId, label));
        statusBar()->showMessage(QString("Table classification prelabel complete: %1").arg(page.assetId), 5000);
    } catch (const std::exception& exc) {
        QMessageBox::critical(this, "Table classification prelabel failed", exc.what());
        statusBar()->showMessage("Table classification prelabel failed", 5000);
    }
    QApplication::restoreOverrideCursor();
}

void MainWindow::prelabelSelectedPageLayout() {
    startPrelabelCurrentPageAsync(false, false, false, true, QStringLiteral("Layout prelabel current page"));
    return;
    if (prelabelAllRunning_) {
        QMessageBox::information(this, QStringLiteral("Task running"), QStringLiteral("Wait for the current background task to finish."));
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
        InferenceService::LayoutConfig config = InferenceService::defaultLayoutConfig(baseDir_);
        const QString selectedModel = selectedPredictionModelDir(labelLayoutModelCombo_);
        config.modelDir = selectedModel.isEmpty()
            ? preferredModelDir(context_ ? &(*context_) : nullptr, tasksForKind(QStringLiteral("layout")), config.modelDir)
            : selectedModel;
        config.device = labelPrelabelDevice();
        config.threshold = prelabelScoreThreshold();

        const QString outputDir = context_->path(QString("cache/layout_prelabel/%1").arg(page.assetId));
        const auto report = InferenceService::predictLayoutImage(page.imagePath, outputDir, config, true);
        if (predictionPreview_) {
            predictionPreview_->setPlainText(QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented)));
        }
        if (!report.value("ok").toBool()) {
            throw std::runtime_error(report.value("error").toString("layout prelabel failed").toStdString());
        }

        const auto annotation = AnnotationController::readAnnotationFile(page.annotationPath);
        const auto updated = AnnotationService::applyLayoutResult(annotation, report);
        AnnotationController::writeAnnotationFile(page.annotationPath, updated);
        appendLog(QString("Layout prelabelled %1 with %2 region(s), threshold=%3")
            .arg(page.assetId)
            .arg(AnnotationService::autoLayoutRegionCount(updated))
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

    InferenceService::OcrConfig ocrConfig;
    if (runOcr) {
        ocrConfig = InferenceService::defaultOcrConfig(baseDir_);
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
        if (!InferenceService::modelDirLooksUsable(ocrConfig.textDetectionModelDir, &reason)) {
            QMessageBox::warning(this, title, QStringLiteral("Det 检测模型不可用：\n%1").arg(reason));
            return;
        }
        reason.clear();
        if (!InferenceService::modelDirLooksUsable(ocrConfig.textRecognitionModelDir, &reason)) {
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
            InferenceService::ClassificationConfig config = InferenceService::defaultClassificationConfig(task.predictionTask, baseDir_);
            const QString selectedModel = selectedPredictionModelDir(task.modelCombo);
            config.modelDir = selectedModel.isEmpty()
                ? preferredModelDir(
                    context_ ? &(*context_) : nullptr,
                    classificationTasksForPrediction(task.predictionTask),
                    config.modelDir)
                : selectedModel;
            config.device = device;
            reason.clear();
            if (!InferenceService::modelDirLooksUsable(config.modelDir, &reason)) {
                QMessageBox::warning(this, title, QStringLiteral("分类模型不可用：\n%1").arg(reason));
                return;
            }
            orientationTasks.append({task.predictionTask, config});
        }
    }

    InferenceService::ClassificationConfig tableConfig;
    if (runTableClassification) {
        tableConfig = InferenceService::defaultClassificationConfig(QStringLiteral("table_classification"), baseDir_);
        tableConfig.modelDir = preferredModelDir(
            context_ ? &(*context_) : nullptr,
            classificationTasksForPrediction(QStringLiteral("table_classification")),
            tableConfig.modelDir);
        tableConfig.device = device;
        reason.clear();
        if (!InferenceService::modelDirLooksUsable(tableConfig.modelDir, &reason)) {
            QMessageBox::warning(this, title, QStringLiteral("表格分类模型不可用：\n%1").arg(reason));
            return;
        }
    }

    InferenceService::LayoutConfig layoutConfig;
    if (runLayout) {
        layoutConfig = InferenceService::defaultLayoutConfig(baseDir_);
        const QString selectedModel = selectedPredictionModelDir(labelLayoutModelCombo_);
        layoutConfig.modelDir = selectedModel.isEmpty()
            ? preferredModelDir(context_ ? &(*context_) : nullptr, tasksForKind(QStringLiteral("layout")), layoutConfig.modelDir)
            : selectedModel;
        layoutConfig.device = device;
        layoutConfig.threshold = scoreThreshold;
        reason.clear();
        if (!InferenceService::layoutModelDirLooksUsable(layoutConfig.modelDir, &reason)) {
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
                const QJsonObject report = InferenceService::predictOcrImage(
                    page.imagePath,
                    projectPath(QStringLiteral("cache/ocr_prelabel/%1").arg(page.assetId)),
                    ocrConfig,
                    false);
                appendPreview(QStringLiteral("ocr"), report);
                if (!report.value(QStringLiteral("ok")).toBool()) {
                    throw std::runtime_error(report.value(QStringLiteral("error")).toString(QStringLiteral("OCR prelabel failed")).toStdString());
                }
                const QJsonObject annotation = AnnotationController::readAnnotationFile(page.annotationPath);
                const QJsonObject updated = AnnotationService::applyOcrResult(annotation, report);
                AnnotationController::writeAnnotationFile(page.annotationPath, updated);
                step.regionCount = AnnotationService::autoOcrRegionCount(updated);
                step.uncheckedRegionCount = uncheckedAutoRegionCount(updated, {RegionType::OcrText});
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
                QJsonObject annotation = AnnotationController::readAnnotationFile(page.annotationPath);
                for (const auto& task : orientationTasks) {
                    postStatus(QStringLiteral("方向分类预标注当前页：%1").arg(page.assetId));
                    const QJsonObject report = InferenceService::predictClassificationImage(
                        page.imagePath,
                        projectPath(QStringLiteral("cache/cls_prelabel/%1/%2").arg(page.assetId, task.predictionTask)),
                        task.config,
                        false);
                    reports.append(report);
                    appendPreview(task.predictionTask, report);
                    if (!report.value(QStringLiteral("ok")).toBool()) {
                        throw std::runtime_error(report.value(QStringLiteral("error")).toString(QStringLiteral("classification prelabel failed")).toStdString());
                    }
                    const QString label = AnnotationService::normalizedClassificationLabel(task.predictionTask, report);
                    annotation = AnnotationService::applyClassificationResult(annotation, task.predictionTask, report);
                    if (!label.isEmpty()) {
                        applied.append(QStringLiteral("%1=%2").arg(task.predictionTask, label));
                    }
                }
                AnnotationController::writeAnnotationFile(page.annotationPath, annotation);
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
                const QJsonObject report = InferenceService::predictClassificationImage(
                    page.imagePath,
                    projectPath(QStringLiteral("cache/table_cls_prelabel/%1").arg(page.assetId)),
                    tableConfig,
                    false);
                appendPreview(QStringLiteral("table_classification"), report);
                if (!report.value(QStringLiteral("ok")).toBool()) {
                    throw std::runtime_error(report.value(QStringLiteral("error")).toString(QStringLiteral("table classification prelabel failed")).toStdString());
                }
                const QJsonObject annotation = AnnotationController::readAnnotationFile(page.annotationPath);
                const QJsonObject updated = AnnotationService::applyClassificationResult(
                    annotation,
                    QStringLiteral("table_classification"),
                    report);
                AnnotationController::writeAnnotationFile(page.annotationPath, updated);
                ++step.okCount;
                const QString label = AnnotationService::normalizedClassificationLabel(QStringLiteral("table_classification"), report);
                result.logLines.append(QStringLiteral("Table classification prelabelled %1: %2").arg(page.assetId, label));
            } catch (const std::exception& exc) {
                ++step.failedCount;
                step.details.append(QStringLiteral("%1: %2").arg(page.assetId, QString::fromUtf8(exc.what())));
            }
            result.steps.append(step);
        }

        if (runLayout) {
            AsyncPrelabelStepSummary step;
            step.name = QStringLiteral("Layout");
            try {
                postStatus(QStringLiteral("版面预标注当前页：%1").arg(page.assetId));
                const QJsonObject report = InferenceService::predictLayoutImage(
                    page.imagePath,
                    projectPath(QStringLiteral("cache/layout_prelabel/%1").arg(page.assetId)),
                    layoutConfig,
                    false);
                appendPreview(QStringLiteral("layout"), report);
                if (!report.value(QStringLiteral("ok")).toBool()) {
                    throw std::runtime_error(report.value(QStringLiteral("error")).toString(QStringLiteral("layout prelabel failed")).toStdString());
                }
                const QJsonObject annotation = AnnotationController::readAnnotationFile(page.annotationPath);
                const QJsonObject updated = AnnotationService::applyLayoutResult(annotation, report);
                AnnotationController::writeAnnotationFile(page.annotationPath, updated);
                step.regionCount = AnnotationService::autoLayoutRegionCount(updated);
                step.uncheckedRegionCount = uncheckedAutoRegionCount(updated, {RegionType::Layout});
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
                window->statusBar()->showMessage(prelabelStatusMessage(result), 7000);
                if (totalUncheckedPrelabelRegionCount(result) > 0) {
                    QMessageBox::information(window.data(), result.title, prelabelSummaryText(result));
                }
            }
        }, Qt::QueuedConnection);
    });

    activePrelabelThread_ = thread;
    prelabelAllRunning_ = true;
    statusBar()->showMessage(QStringLiteral("%1 started").arg(title), 3000);
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
    const auto pages = AnnotationController::listProjectPages(*context_);
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

    InferenceService::OcrConfig ocrConfig;
    if (runOcr) {
        ocrConfig = InferenceService::defaultOcrConfig(baseDir_);
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
        if (!InferenceService::modelDirLooksUsable(ocrConfig.textDetectionModelDir, &reason)) {
            QMessageBox::warning(this, title, QStringLiteral("Det 检测模型不可用：\n%1").arg(reason));
            return;
        }
        reason.clear();
        if (!InferenceService::modelDirLooksUsable(ocrConfig.textRecognitionModelDir, &reason)) {
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
            InferenceService::ClassificationConfig config = InferenceService::defaultClassificationConfig(task.predictionTask, baseDir_);
            const QString selectedModel = selectedPredictionModelDir(task.modelCombo);
            config.modelDir = selectedModel.isEmpty()
                ? preferredModelDir(
                    context_ ? &(*context_) : nullptr,
                    classificationTasksForPrediction(task.predictionTask),
                    config.modelDir)
                : selectedModel;
            config.device = device;
            reason.clear();
            if (!InferenceService::modelDirLooksUsable(config.modelDir, &reason)) {
                QMessageBox::warning(this, title, QStringLiteral("分类模型不可用：\n%1").arg(reason));
                return;
            }
            orientationTasks.append({task.predictionTask, config});
        }
        stepNames.append(QStringLiteral("方向分类"));
    }

    InferenceService::ClassificationConfig tableConfig;
    if (runTableClassification) {
        tableConfig = InferenceService::defaultClassificationConfig(QStringLiteral("table_classification"), baseDir_);
        tableConfig.modelDir = preferredModelDir(
            context_ ? &(*context_) : nullptr,
            classificationTasksForPrediction(QStringLiteral("table_classification")),
            tableConfig.modelDir);
        tableConfig.device = device;
        reason.clear();
        if (!InferenceService::modelDirLooksUsable(tableConfig.modelDir, &reason)) {
            QMessageBox::warning(this, title, QStringLiteral("表格分类模型不可用：\n%1").arg(reason));
            return;
        }
        stepNames.append(QStringLiteral("表格分类"));
    }

    InferenceService::LayoutConfig layoutConfig;
    if (runLayout) {
        layoutConfig = InferenceService::defaultLayoutConfig(baseDir_);
        const QString selectedModel = selectedPredictionModelDir(labelLayoutModelCombo_);
        layoutConfig.modelDir = selectedModel.isEmpty()
            ? preferredModelDir(context_ ? &(*context_) : nullptr, tasksForKind(QStringLiteral("layout")), layoutConfig.modelDir)
            : selectedModel;
        layoutConfig.device = device;
        layoutConfig.threshold = scoreThreshold;
        reason.clear();
        if (!InferenceService::layoutModelDirLooksUsable(layoutConfig.modelDir, &reason)) {
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
            const QList<QJsonObject> reports = InferenceService::predictOcrImages(
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
                    const auto annotation = AnnotationController::readAnnotationFile(page.annotationPath);
                    const auto updated = AnnotationService::applyOcrResult(annotation, report);
                    AnnotationController::writeAnnotationFile(page.annotationPath, updated);
                    step.regionCount += AnnotationService::autoOcrRegionCount(updated);
                    step.uncheckedRegionCount += uncheckedAutoRegionCount(updated, {RegionType::OcrText});
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
                const QList<QJsonObject> reports = InferenceService::predictClassificationImages(
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
                            annotations[i] = AnnotationController::readAnnotationFile(pages.at(i).annotationPath);
                            annotationLoaded[i] = true;
                        }
                        annotations[i] = AnnotationService::applyClassificationResult(annotations.at(i), task.predictionTask, report);
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
                        AnnotationController::writeAnnotationFile(page.annotationPath, annotations.at(i));
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
            const QList<QJsonObject> reports = InferenceService::predictClassificationImages(
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
                    const auto annotation = AnnotationController::readAnnotationFile(page.annotationPath);
                    const auto updated = AnnotationService::applyClassificationResult(
                        annotation,
                        QStringLiteral("table_classification"),
                        report);
                    AnnotationController::writeAnnotationFile(page.annotationPath, updated);
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
            step.name = QStringLiteral("Layout");
            QList<QPair<QString, QString>> jobs;
            jobs.reserve(pages.size());
            for (const auto& page : pages) {
                jobs.append(qMakePair(page.imagePath, projectPath(QStringLiteral("cache/layout_prelabel/%1").arg(page.assetId))));
            }
            const QList<QJsonObject> reports = InferenceService::predictLayoutImages(
                jobs,
                layoutConfig,
                false,
                [&postProgress](int current, int total, const QString& imagePath) {
                    postProgress(QStringLiteral("Layout"), current, total, imagePath);
                });
            for (int i = 0; i < pages.size(); ++i) {
                const auto page = pages.at(i);
                try {
                    const QJsonObject report = i < reports.size() ? reports.at(i) : QJsonObject{};
                    if (report.isEmpty() || !report.value(QStringLiteral("ok")).toBool()) {
                        throw std::runtime_error(report.value(QStringLiteral("error")).toString(QStringLiteral("layout prelabel failed")).toStdString());
                    }
                    const auto annotation = AnnotationController::readAnnotationFile(page.annotationPath);
                    const auto updated = AnnotationService::applyLayoutResult(annotation, report);
                    AnnotationController::writeAnnotationFile(page.annotationPath, updated);
                    step.regionCount += AnnotationService::autoLayoutRegionCount(updated);
                    step.uncheckedRegionCount += uncheckedAutoRegionCount(updated, {RegionType::Layout});
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
                window->appendLog(QStringLiteral("%1: %2 success, %3 failed, %4 region(s), %5 unchecked")
                    .arg(step.name)
                    .arg(step.okCount)
                    .arg(step.failedCount)
                    .arg(step.regionCount)
                    .arg(step.uncheckedRegionCount));
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
            window->statusBar()->showMessage(prelabelStatusMessage(result), 7000);
            QMessageBox::information(window.data(), result.title, prelabelSummaryText(result));
        }, Qt::QueuedConnection);
    });
    activePrelabelThread_ = thread;
    prelabelAllRunning_ = true;
    statusBar()->showMessage(QStringLiteral("%1 started").arg(title), 3000);
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
    const auto pages = AnnotationController::listProjectPages(*context_);
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

    auto config = InferenceService::defaultOcrConfig(baseDir_);
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
    if (!InferenceService::modelDirLooksUsable(config.textDetectionModelDir, &reason)) {
        QMessageBox::warning(this, "Det model invalid", reason);
        return;
    }
    if (!InferenceService::modelDirLooksUsable(config.textRecognitionModelDir, &reason)) {
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
    const QList<QJsonObject> reports = InferenceService::predictOcrImages(
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
            const auto annotation = AnnotationController::readAnnotationFile(page.annotationPath);
            const auto updated = AnnotationService::applyOcrResult(annotation, report);
            AnnotationController::writeAnnotationFile(page.annotationPath, updated);
            regionCount += AnnotationService::autoOcrRegionCount(updated);
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
    const auto pages = AnnotationController::listProjectPages(*context_);
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
    QMap<QString, InferenceService::ClassificationConfig> configs;
    QString reason;
    saveLabelModelConfig();
    for (const auto& task : tasks) {
        InferenceService::ClassificationConfig config = InferenceService::defaultClassificationConfig(task.predictionTask, baseDir_);
        const QString selectedModel = selectedPredictionModelDir(task.modelCombo);
        config.modelDir = selectedModel.isEmpty()
            ? preferredModelDir(
                context_ ? &(*context_) : nullptr,
                classificationTasksForPrediction(task.predictionTask),
                config.modelDir)
            : selectedModel;
        config.device = labelPrelabelDevice();
        if (!InferenceService::modelDirLooksUsable(config.modelDir, &reason)) {
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
        const QList<QJsonObject> reports = InferenceService::predictClassificationImages(
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
                    annotations[i] = AnnotationController::readAnnotationFile(page.annotationPath);
                    annotationLoaded[i] = true;
                }
                annotations[i] = AnnotationService::applyClassificationResult(annotations.at(i), taskName, report);
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
                AnnotationController::writeAnnotationFile(page.annotationPath, annotations.at(i));
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
    const auto pages = AnnotationController::listProjectPages(*context_);
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

    InferenceService::ClassificationConfig config = InferenceService::defaultClassificationConfig(QStringLiteral("table_classification"), baseDir_);
    config.modelDir = preferredModelDir(
        context_ ? &(*context_) : nullptr,
        classificationTasksForPrediction(QStringLiteral("table_classification")),
        config.modelDir);
    config.device = labelPrelabelDevice();
    QString reason;
    if (!InferenceService::modelDirLooksUsable(config.modelDir, &reason)) {
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
    const QList<QJsonObject> reports = InferenceService::predictClassificationImages(
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
            const auto annotation = AnnotationController::readAnnotationFile(page.annotationPath);
            const auto updated = AnnotationService::applyClassificationResult(
                annotation,
                QStringLiteral("table_classification"),
                report);
            AnnotationController::writeAnnotationFile(page.annotationPath, updated);
            const QString label = AnnotationService::normalizedClassificationLabel(QStringLiteral("table_classification"), report);
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
    const auto pages = AnnotationController::listProjectPages(*context_);
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

    InferenceService::LayoutConfig config = InferenceService::defaultLayoutConfig(baseDir_);
    saveLabelModelConfig();
    const QString selectedModel = selectedPredictionModelDir(labelLayoutModelCombo_);
    config.modelDir = selectedModel.isEmpty()
        ? preferredModelDir(context_ ? &(*context_) : nullptr, tasksForKind(QStringLiteral("layout")), config.modelDir)
        : selectedModel;
    config.device = labelPrelabelDevice();
    config.threshold = prelabelScoreThreshold();
    QString reason;
    if (!InferenceService::layoutModelDirLooksUsable(config.modelDir, &reason)) {
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
    const QList<QJsonObject> reports = InferenceService::predictLayoutImages(
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
            const auto annotation = AnnotationController::readAnnotationFile(page.annotationPath);
            const auto updated = AnnotationService::applyLayoutResult(annotation, report);
            AnnotationController::writeAnnotationFile(page.annotationPath, updated);
            regionCount += AnnotationService::autoLayoutRegionCount(updated);
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

void MainWindow::confirmCurrentPageAnnotations() {
    if (!context_) {
        QMessageBox::information(this, QStringLiteral("没有项目"), QStringLiteral("请先打开或创建项目。"));
        return;
    }
    const int row = pageList_ ? pageList_->currentRow() : -1;
    if (row < 0 || row >= pages_.size()) {
        QMessageBox::information(this, QStringLiteral("没有页面"), QStringLiteral("请先选择一个页面。"));
        return;
    }

    QJsonObject annotation = annotationController_->hasCurrentAnnotation()
        ? annotationController_->currentAnnotation()
        : AnnotationController::readAnnotationFile(pages_.at(row).annotationPath);
    const int changed = setAnnotationRegionsChecked(&annotation, true);
    if (changed <= 0) {
        statusBar()->showMessage(QStringLiteral("当前页没有未确认区域"), 3000);
        return;
    }
    annotationController_->replaceCurrentAnnotation(annotation);
    persistCurrentAnnotation();
    refreshCanvasAnnotation();
    updateSelectedRegionPanel();
    refreshPages();
    if (row >= 0 && row < pageList_->count()) {
        pageList_->setCurrentRow(row);
        loadSelectedPage();
    }
    appendLog(QStringLiteral("批量确认当前页：%1 个区域").arg(changed));
    statusBar()->showMessage(QStringLiteral("已批量确认当前页 %1 个区域，可进入 checked-only 导出和训练").arg(changed), 5000);
}

void MainWindow::confirmAllProjectAnnotations() {
    if (!context_) {
        QMessageBox::information(this, QStringLiteral("没有项目"), QStringLiteral("请先打开或创建项目。"));
        return;
    }
    const auto pages = AnnotationController::listProjectPages(*context_);
    if (pages.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("没有页面"), QStringLiteral("请先导入图片或 PDF。"));
        return;
    }
    const auto reply = QMessageBox::question(
        this,
        QStringLiteral("批量确认全项目"),
        QStringLiteral("将把全项目 %1 页里的所有 OCR/Layout 区域标记为 checked=true。\n\n请确认你已经检查过自动预标注结果。")
            .arg(pages.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }

    persistCurrentAnnotation();
    int changed = 0;
    int changedPages = 0;
    for (const auto& page : pages) {
        QJsonObject annotation = AnnotationController::readAnnotationFile(page.annotationPath);
        const int pageChanged = setAnnotationRegionsChecked(&annotation, true);
        if (pageChanged <= 0) {
            continue;
        }
        AnnotationController::writeAnnotationFile(page.annotationPath, annotation);
        changed += pageChanged;
        ++changedPages;
    }
    const int row = pageList_ ? pageList_->currentRow() : -1;
    refreshPages();
    if (row >= 0 && row < pageList_->count()) {
        pageList_->setCurrentRow(row);
        loadSelectedPage();
    }
    appendLog(QStringLiteral("批量确认全项目：%1 页，%2 个区域").arg(changedPages).arg(changed));
    statusBar()->showMessage(QStringLiteral("已批量确认全项目 %1 个区域").arg(changed), 5000);
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

    QJsonObject annotation = annotationController_->hasCurrentAnnotation()
        ? annotationController_->currentAnnotation()
        : AnnotationController::readAnnotationFile(page.annotationPath);
    const int removedCrops = removeAnnotationCrops(*context_, annotation);
    annotationController_->setCurrentAnnotation(annotation);
    annotationController_->clearAnnotation();
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

    const auto pages = AnnotationController::listProjectPages(*context_);
    int removedCrops = 0;
    for (const auto& page : pages) {
        QJsonObject annotation = AnnotationController::readAnnotationFile(page.annotationPath);
        removedCrops += removeAnnotationCrops(*context_, annotation);
        AnnotationController::writeAnnotationFile(page.annotationPath, AnnotationService::clearAnnotation(annotation));
    }
    annotationController_->clearCurrentAnnotation();
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
        QJsonObject annotation = annotationController_->hasCurrentAnnotation()
            ? annotationController_->currentAnnotation()
            : AnnotationController::readAnnotationFile(page.annotationPath);
        removeAnnotationCrops(*context_, annotation);
        annotation = AnnotationService::generateRecCrops(*context_, annotation);
        AnnotationController::writeAnnotationFile(page.annotationPath, annotation);
        annotationController_->setCurrentAnnotation(annotation);
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

    if (!annotationController_->addOcrRegion(jsonPoints)) {
        return;
    }
    persistCurrentAnnotation();
    refreshCanvasAnnotation();
    updateSelectedRegionPanel();
    appendLog(QString("Added OCR region: %1").arg(annotationController_->selectedRegionId()));
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
    const QString label = layoutLabelCombo_ ? layoutLabelCombo_->currentText() : QString("text");
    if (!annotationController_->addLayoutRegion(jsonPoints, label)) {
        return;
    }
    persistCurrentAnnotation();
    refreshCanvasAnnotation();
    updateSelectedRegionPanel();
    appendLog(QString("Added layout region: %1").arg(annotationController_->selectedRegionId()));
}

void MainWindow::selectRegionFromCanvas(const QString& regionId) {
    annotationController_->setSelectedRegionId(regionId);
    updateSelectedRegionPanel();
}

void MainWindow::moveRegionFromCanvas(const QString& regionId, const QVariant& points) {
    if (regionId.isEmpty() || annotationController_->findRegion(regionId).isEmpty()) {
        return;
    }
    const QJsonArray jsonPoints = pointsFromVariant(points);
    if (jsonPoints.size() < 4) {
        statusBar()->showMessage(QStringLiteral("Ignored region edit: at least 4 points required"), 3000);
        return;
    }
    if (!annotationController_->moveRegion(regionId, jsonPoints)) {
        statusBar()->showMessage(QStringLiteral("Ignored region edit: invalid polygon"), 3000);
        return;
    }
    persistCurrentAnnotation();
    refreshCanvasAnnotation();
    updateSelectedRegionPanel();
    statusBar()->showMessage(QStringLiteral("Updated region shape: %1").arg(regionId), 2000);
}

void MainWindow::showCanvasMessage(const QString& message) {
    if (!message.isEmpty()) {
        statusBar()->showMessage(message, 3000);
    }
}

void MainWindow::confirmSelectedRegionFromCanvas() {
    if (!context_ || annotationController_->selectedRegionId().isEmpty()) {
        return;
    }
    const QJsonObject region = annotationController_->selectedRegion();
    if (region.isEmpty()) {
        return;
    }
    if (region.value(QStringLiteral("checked")).toBool(false)) {
        statusBar()->showMessage(QStringLiteral("Region already checked"), 2000);
        return;
    }
    if (!annotationController_->updateSelectedRegion(QJsonObject{{QStringLiteral("checked"), true}})) {
        return;
    }
    persistCurrentAnnotation();
    refreshCanvasAnnotation();
    updateSelectedRegionPanel();
    statusBar()->showMessage(QStringLiteral("Region checked"), 2000);
}

void MainWindow::toggleSelectedRegionCheckedFromCanvas() {
    if (!context_ || annotationController_->selectedRegionId().isEmpty()) {
        return;
    }
    const QJsonObject region = annotationController_->selectedRegion();
    if (region.isEmpty()) {
        return;
    }
    const bool nextChecked = !region.value(QStringLiteral("checked")).toBool(false);
    if (!annotationController_->updateSelectedRegion(QJsonObject{{QStringLiteral("checked"), nextChecked}})) {
        return;
    }
    persistCurrentAnnotation();
    refreshCanvasAnnotation();
    updateSelectedRegionPanel();
    statusBar()->showMessage(nextChecked ? QStringLiteral("Region checked") : QStringLiteral("Region unchecked"), 2000);
}

void MainWindow::toggleSelectedRegionIgnoreFromCanvas() {
    if (!context_ || annotationController_->selectedRegionId().isEmpty()) {
        return;
    }
    const QJsonObject region = annotationController_->selectedRegion();
    if (region.isEmpty() || region.value(QStringLiteral("type")).toString() != QStringLiteral("ocr_text")) {
        statusBar()->showMessage(QStringLiteral("Ignore applies to OCR regions only"), 2000);
        return;
    }
    const bool nextIgnore = !region.value(QStringLiteral("ignore")).toBool(false);
    if (!annotationController_->updateSelectedRegion(QJsonObject{{QStringLiteral("ignore"), nextIgnore}})) {
        return;
    }
    persistCurrentAnnotation();
    refreshCanvasAnnotation();
    updateSelectedRegionPanel();
    statusBar()->showMessage(nextIgnore ? QStringLiteral("Region ignored") : QStringLiteral("Region included"), 2000);
}

void MainWindow::saveSelectedRegionText() {
    if (updatingRegionPanel_ || annotationController_->selectedRegionId().isEmpty() || !regionTextEdit_) {
        return;
    }
    const QJsonObject region = annotationController_->selectedRegion();
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
    const QString savedId = annotationController_->selectedRegionId();
    if (!annotationController_->updateSelectedRegion(updates)) {
        return;
    }
    persistCurrentAnnotation();
    refreshCanvasAnnotation();
    updateSelectedRegionPanel();
    statusBar()->showMessage(QString("Saved region: %1").arg(savedId), 3000);
}

void MainWindow::saveImageLabels() {
    if (updatingRegionPanel_ || !context_ || !annotationController_->hasCurrentAnnotation()) {
        return;
    }
    if (!annotationController_->setImageLabels(
            docOrientationCombo_ ? docOrientationCombo_->currentText() : QString(),
            textlineOrientationCombo_ ? textlineOrientationCombo_->currentText() : QString(),
            tableClassificationCombo_ ? tableClassificationCombo_->currentText() : QString())) {
        return;
    }
    persistCurrentAnnotation();
    statusBar()->showMessage("Saved image labels", 2000);
}

void MainWindow::deleteSelectedRegion() {
    if (annotationController_->selectedRegionId().isEmpty()
            || annotationController_->findRegion(annotationController_->selectedRegionId()).isEmpty()) {
        return;
    }
    const QString deletedId = annotationController_->selectedRegionId();
    if (!annotationController_->deleteSelectedRegion()) {
        return;
    }
    persistCurrentAnnotation();
    refreshCanvasAnnotation();
    updateSelectedRegionPanel();
    appendLog(QString("Deleted region: %1").arg(deletedId));
}

}  // namespace ppocr


