#include "app/MainWindowInternal.h"

#include "application/DatasetService.h"
#include "application/ExportService.h"
#include "app/controllers/ModelLibraryController.h"

namespace ppocr {
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
    if (projectController_) {
        projectController_->rememberProject(context.root.absolutePath());
    }
    refreshRecentProjects();
    refreshPages();
    refreshTrainingVersions();
    if (predictOutputEdit_) {
        predictOutputEdit_->setText(defaultPredictionOutputDir());
    }
    if (exportOutputDirEdit_) {
        exportOutputDirEdit_->clear();
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
    allPages_ = DatasetController::listPages(*context_);
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
        if (page.status == PageStatus::Labeled || page.status == PageStatus::Checked) {
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
            datasetNextStepLabel_->setText(QStringLiteral("Create or open a project"));
            datasetNextStepLabel_->setObjectName("StatusWarn");
            datasetNextStepLabel_->style()->unpolish(datasetNextStepLabel_);
            datasetNextStepLabel_->style()->polish(datasetNextStepLabel_);
        }
        appendTableRow(datasetOverviewTable_, {
            QStringLiteral("Dataset"),
            QStringLiteral("No project"),
            QStringLiteral("-"),
            QStringLiteral("打开项目"),
        });
        appendTableRow(datasetPathTable_, {
            QStringLiteral("Workbench root"),
            pathStateText(baseDir_),
            nativePathText(baseDir_),
        });
        return;
    }

    const QList<PageInfo> pages = DatasetController::listPages(*context_);
    const SplitSummary splitSummary = DatasetController::splitSummary(*context_);
    const int trainPages = splitSummary.train;
    const int valPages = splitSummary.val;
    const int testPages = splitSummary.test;
    const int unassignedPages = splitSummary.unassigned;
    int labeledPages = 0;
    int validatedPages = 0;
    int ocrRegions = 0;
    int layoutRegions = 0;
    int uncheckedRegions = 0;
    int recCrops = 0;
    int validationErrors = 0;
    int validationWarnings = 0;
    for (const auto& page : pages) {
        if (page.status == PageStatus::Labeled || page.status == PageStatus::Checked) {
            ++labeledPages;
        }
        if (page.status == PageStatus::Checked) {
            ++validatedPages;
        }
        try {
            const QJsonObject annotation = annotationController_->readAnnotation(page.annotationPath);
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
        const QString root = ExportService::datasetOutputDir(*context_, spec.first);
        const int count = datasetItemCount(root);
        if (count > 0) {
            ++exportedDatasets;
            exportedSamples += count;
            exportDetails.append(QStringLiteral("%1:%2").arg(spec.second).arg(count));
        }
    }

    int successVersions = 0;
    for (const auto& task : TrainingService::trainingTasks()) {
        if (!task.trainSupported) {
            continue;
        }
        const QJsonArray versions = ModelLibraryController::loadManifest(*context_, task.key)
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
    } else if (uncheckedRegions > 0) {
        nextStep = QStringLiteral("下一步：批量确认未确认标注");
    } else if (labeledPages < pages.size()) {
        nextStep = QStringLiteral("Review labels");
    } else if (lastValidationLogPath_.isEmpty() && validatedPages < pages.size()) {
        nextStep = QStringLiteral("下一步：运行校验");
    } else if (validationErrors > 0) {
        nextStep = QStringLiteral("Fix validation issues");
    } else if (exportedDatasets == 0) {
        nextStep = QStringLiteral("下一步：导出训练集");
    } else {
        nextStep = QStringLiteral("Start training");
        nextObject = QStringLiteral("StatusOk");
    }

    if (datasetSummaryLabel_) {
        datasetSummaryLabel_->setText(QStringLiteral("%1 | pages %2 | labeled %3 | regions %4 | exports %5")
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
        QStringLiteral("Dataset"),
        pages.isEmpty() ? QStringLiteral("Empty") : QStringLiteral("Ready"),
        QStringLiteral("Pages %1 | train %2 / val %3 / test %4 / unassigned %5")
            .arg(pages.size())
            .arg(trainPages)
            .arg(valPages)
            .arg(testPages)
            .arg(unassignedPages),
        pages.isEmpty() ? QStringLiteral("导入图片 / PDF") : QStringLiteral("进入标注"),
    });
    appendTableRow(datasetOverviewTable_, {
        QStringLiteral("数据划分"),
        unassignedPages > 0 ? QStringLiteral("需处理") : QStringLiteral("已配置"),
        QStringLiteral("训练集 %1 页 | 验证集 %2 页 | 测试集 %3 页 | 当前策略：%4")
            .arg(trainPages)
            .arg(valPages)
            .arg(testPages)
            .arg(splitSummary.strategyText),
        QStringLiteral("按比例划分 / 手动调整 / 导出报告"),
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
        QStringLiteral("Exports"),
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
        {QStringLiteral("Exports"), context_->exportRoot()},
        {QStringLiteral("训练版本"), context_->path(QStringLiteral("training"))},
        {QStringLiteral("预测结果"), context_->path(QStringLiteral("predictions"))},
        {QStringLiteral("Logs"), context_->logRoot()},
    };
    for (const auto& row : paths) {
        appendTableRow(datasetPathTable_, {row.first, pathStateText(row.second), nativePathText(row.second)});
    }
    appendTableRow(datasetPathTable_, {
        QStringLiteral("页面扫描规则"),
        QStringLiteral("提示"),
        QStringLiteral("请通过导入按钮导入素材；当前仅索引 assets/pages/page_*.jpg，手工复制 png/tif 或非标准命名文件不会出现在页面列表。"),
    });
    if (datasetOverviewTable_) {
        datasetOverviewTable_->resizeColumnsToContents();
    }
    if (datasetPathTable_) {
        datasetPathTable_->resizeColumnsToContents();
    }
}

void MainWindow::reassignDatasetSplits() {
    if (!context_) {
        QMessageBox::information(this, QStringLiteral("No project"), QStringLiteral("Open or create a project first."));
        return;
    }

    bool ok = false;
    const QString strategyText = QInputDialog::getItem(
        this,
        QStringLiteral("Dataset split"),
        QStringLiteral("划分策略"),
        QStringList{
            QStringLiteral("固定比例随机划分"),
            QStringLiteral("按文件顺序划分"),
            QStringLiteral("手动调整"),
        },
        0,
        false,
        &ok);
    if (!ok || strategyText.isEmpty()) {
        return;
    }
    if (strategyText == QStringLiteral("手动调整")) {
        if (stack_) {
            stack_->setCurrentIndex(1);
        }
        statusBar()->showMessage(QStringLiteral("请在页面元数据里手动调整 train/val/test split"), 5000);
        return;
    }

    const double trainPercent = QInputDialog::getDouble(
        this,
        QStringLiteral("Dataset split"),
        QStringLiteral("Train percentage"),
        80.0,
        1.0,
        99.0,
        1,
        &ok);
    if (!ok) {
        return;
    }
    int seed = 42;
    if (strategyText == QStringLiteral("固定比例随机划分")) {
        seed = QInputDialog::getInt(
            this,
            QStringLiteral("Dataset split"),
            QStringLiteral("随机种子"),
            42,
            0,
            999999,
            1,
            &ok);
        if (!ok) {
            return;
        }
    }

    try {
        const double trainRatio = trainPercent / 100.0;
        const int changed = strategyText == QStringLiteral("固定比例随机划分")
            ? DatasetController::reassignSplitsRandom(*context_, trainRatio, 1.0 - trainRatio, 0.0, seed)
            : DatasetController::reassignSplitsByOrder(*context_, trainRatio, 1.0 - trainRatio, 0.0);
        appendLog(QStringLiteral("Dataset split reassigned: %1 page(s), strategy=%2, train=%3%, val=%4%")
            .arg(changed)
            .arg(strategyText)
            .arg(trainPercent, 0, 'f', 1)
            .arg(100.0 - trainPercent, 0, 'f', 1));
        refreshPages();
        refreshDatasetPage();
        statusBar()->showMessage(QStringLiteral("Dataset split updated"), 3000);
    } catch (const std::exception& exc) {
        QMessageBox::warning(this, QStringLiteral("Dataset split failed"), QString::fromUtf8(exc.what()));
    }
}

void MainWindow::exportDatasetSplitReport() {
    if (!context_) {
        QMessageBox::information(this, QStringLiteral("No project"), QStringLiteral("Open or create a project first."));
        return;
    }

    const QString defaultPath = context_->path(QStringLiteral("logs/dataset_split_report.json"));
    const QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Export split report"),
        defaultPath,
        QStringLiteral("JSON (*.json)"));
    if (path.isEmpty()) {
        return;
    }

    try {
        DatasetController::exportSplitReport(*context_, path);
        appendLog(QStringLiteral("Dataset split report exported: %1").arg(path));
        statusBar()->showMessage(QStringLiteral("Dataset split report exported"), 3000);
    } catch (const std::exception& exc) {
        QMessageBox::warning(this, QStringLiteral("Export split report failed"), QString::fromUtf8(exc.what()));
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
            modelLibrarySummaryLabel_->setText(QStringLiteral("No project open"));
        }
        appendTableRow(modelLibraryTable_, {
            QStringLiteral("All tasks"),
            QStringLiteral("-"),
            QStringLiteral("No project"),
            QStringLiteral("-"),
            QStringLiteral("-"),
            QStringLiteral("-"),
            QStringLiteral("-"),
            QStringLiteral("-"),
        }, 2);
        return;
    }

    const ModelLibrarySummary summary = ModelLibraryController::summarize(*context_);
    for (const auto& row : summary.rows) {
        appendTableRow(modelLibraryTable_, {
            row.taskTitle,
            row.versionId,
            row.status,
            row.metric,
            row.current,
            row.best,
            nativePathText(row.modelDir),
            row.finished,
        }, 2);
        const int tableRow = modelLibraryTable_ ? modelLibraryTable_->rowCount() - 1 : -1;
        if (modelLibraryTable_ && tableRow >= 0) {
            for (int column = 0; column < modelLibraryTable_->columnCount(); ++column) {
                if (auto* item = modelLibraryTable_->item(tableRow, column)) {
                    item->setData(Qt::UserRole, row.taskKey);
                    item->setData(Qt::UserRole + 1, row.versionId);
                }
            }
        }
    }
    if (modelLibrarySummaryLabel_) {
        modelLibrarySummaryLabel_->setText(QStringLiteral("Trainable tasks %1 | versions %2 | usable models %3 | current %4 | best %5")
            .arg(summary.trainableTasks)
            .arg(summary.versionCount)
            .arg(summary.usableModels)
            .arg(summary.currentCount)
            .arg(summary.bestCount));
    }
    if (modelLibraryTable_) {
        modelLibraryTable_->resizeColumnsToContents();
    }
}void MainWindow::refreshLogsPage() {
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
            const QJsonObject status = EnvironmentService::executableStatus(path);
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

    addPath(QStringLiteral("Workbench root"), baseDir_);
    addPath(QStringLiteral("PaddleOCR"), QDir(baseDir_).filePath(QStringLiteral("PaddleOCR")));
    addPath(QStringLiteral("PaddleX"), QDir(baseDir_).filePath(QStringLiteral("PaddleX")));
    addPath(QStringLiteral("PaddleX Python"), paddlexPythonPath(), true);
    addPath(QStringLiteral("Model root"), QDir(baseDir_).filePath(QStringLiteral("model/infer")));
    addPath(QStringLiteral("CPU 推理 SDK"), QDir(baseDir_).filePath(QStringLiteral("third_party/paddle_inference")));
    addPath(QStringLiteral("GPU 推理 SDK"), QDir(baseDir_).filePath(QStringLiteral("third_party/paddle_inference_gpu")));
    addPath(QStringLiteral("运行目录"), QCoreApplication::applicationDirPath());
    if (context_) {
        addPath(QStringLiteral("当前项目"), context_->root.absolutePath());
        addPath(QStringLiteral("页面素材"), context_->imageRoot());
        addPath(QStringLiteral("标注 JSON"), context_->annotationRoot());
        addPath(QStringLiteral("Exports"), context_->exportRoot());
        addPath(QStringLiteral("训练版本"), context_->path(QStringLiteral("training")));
        addPath(QStringLiteral("预测结果"), context_->path(QStringLiteral("predictions")));
        addPath(QStringLiteral("Logs"), context_->logRoot());
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
        QJsonObject quick{
            {QStringLiteral("base_dir"), nativePathText(baseDir_)},
            {QStringLiteral("project_dir"), context_ ? nativePathText(context_->root.absolutePath()) : QString()},
            {QStringLiteral("missing_paths"), missing},
            {QStringLiteral("full_report"), QStringLiteral("Use Environment report to run Paddle SDK/model smoke checks.")},
        };
        settingsReportView_->setPlainText(QString::fromUtf8(QJsonDocument(quick).toJson(QJsonDocument::Indented)));
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
        for (const auto& value : ModelLibraryController::loadRuns(*context_)) {
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
            {QStringLiteral("ppocr_rec"), QStringLiteral("Rec dataset")},
            {QStringLiteral("ppocr_det"), QStringLiteral("Det 检测")},
            {QStringLiteral("table_classification"), QStringLiteral("Table cls dataset")},
            {QStringLiteral("coco_layout"), QStringLiteral("Layout dataset")},
        };
        for (const auto& spec : specs) {
            const QString root = ExportService::datasetOutputDir(*context_, spec.first);
            const int count = datasetItemCount(root);
            const QDateTime latest = latestFileTime(root);
            if (count > 0) {
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

    const int totalRuns = runs.isEmpty() ? TrainingService::trainingTasks().size() : runs.size();
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
            {QStringLiteral("Failed"), failed, QColor("#b0444d")},
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
        const QString haystack = QString("%1 %2 %3 %4").arg(page.assetId, page.relativeImagePath, toString(page.split), toString(page.status)).toLower();
        if (!haystack.contains(query)) {
            return false;
        }
    }
    const QString splitFilter = comboStoredValue(pageSplitFilterCombo_);
    if (!splitFilter.isEmpty()) {
        return false;
    }
    const QString statusFilter = comboStoredValue(pageStatusFilterCombo_);
    if (!statusFilter.isEmpty() && toString(page.status) != statusFilter) {
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
        annotationController_->clearCurrentAnnotation();
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
    annotationController_->setCurrentAnnotation(annotationController_->readAnnotation(page.annotationPath));
    pageMetaLabel_->setText(QString("%1 | %2x%3 | %4 | %5")
        .arg(page.assetId)
        .arg(page.width)
        .arg(page.height)
        .arg(splitDisplayText(page.split), statusDisplayText(toString(page.status))));
    if (fileTitleLabel_) {
        fileTitleLabel_->setText(QFileInfo(page.imagePath).fileName());
    }
    if (fileMetaLabel_) {
        fileMetaLabel_->setText(QStringLiteral("%1 × %2 | %3 | %4")
                .arg(page.width)
                .arg(page.height)
                .arg(splitDisplayText(page.split), statusDisplayText(toString(page.status))));
    }
    if (labelWorkspaceSummary_) {
        labelWorkspaceSummary_->setText(QStringLiteral("%1 | 当前页 %2 | %3x%4 | %5 / %6")
                .arg(context_->config.value("project_name").toString(QStringLiteral("未命名项目")))
                .arg(page.assetId)
                .arg(page.width)
                .arg(page.height)
                .arg(splitDisplayText(page.split), statusDisplayText(toString(page.status))));
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
                .arg(toString(page.status))
                .arg(toString(page.split)));
    }
    int labeledCount = 0;
    for (const auto& item : allPages_) {
        if (item.status == PageStatus::Labeled || item.status == PageStatus::Checked) {
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
        labelValidationSummaryLabel_->setText(QStringLiteral("Validation: %1 errors, %2 warnings, %3 passed")
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
    const QStringList projects = projectController_ ? projectController_->recentProjects() : QStringList{};
    for (const auto& project : projects) {
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
    connect(root, SIGNAL(toolModeRequested(QString)), this, SLOT(setCanvasToolMode(QString)));
    connect(root, SIGNAL(undoRequested()), this, SLOT(undoAnnotationEdit()));
    connect(root, SIGNAL(redoRequested()), this, SLOT(redoAnnotationEdit()));
    connect(root, SIGNAL(regionConfirmRequested()), this, SLOT(confirmSelectedRegionFromCanvas()));
    connect(root, SIGNAL(regionCheckedToggleRequested()), this, SLOT(toggleSelectedRegionCheckedFromCanvas()));
    connect(root, SIGNAL(regionIgnoreToggleRequested()), this, SLOT(toggleSelectedRegionIgnoreFromCanvas()));
    connect(root, SIGNAL(canvasMessage(QString)), this, SLOT(showCanvasMessage(QString)));
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
                ? QStringLiteral("Mode: OCR")
                : (drawLayout ? QStringLiteral("Mode: Layout")
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
        root->setProperty("annotationJson", QString::fromUtf8(QJsonDocument(annotationController_->currentAnnotation()).toJson(QJsonDocument::Compact)));
        root->setProperty("selectedRegionId", annotationController_->selectedRegionId());
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
    const QJsonObject annotation = annotationController_->currentAnnotation();
    annotationController_->writeAnnotation(pages_.at(row).annotationPath, annotation);
    pages_[row].status = pageStatusFromString(annotation.value("status").toString(toString(pages_.at(row).status))).value_or(PageStatus::Error);
    pages_[row].split = pageSplitFromString(annotation.value("split").toString(toString(pages_.at(row).split))).value_or(PageSplit::Unassigned);
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
        .arg(statusDisplayText(toString(pages_.at(row).status))));
    if (pageMetaLabel_) {
        pageMetaLabel_->setText(QDir::toNativeSeparators(context_->root.absolutePath()));
    }
    if (fileMetaLabel_) {
        fileMetaLabel_->setText(QStringLiteral("第 %1 / %2 页 | %3 x %4 | %5 | %6")
                .arg(row + 1)
                .arg(pages_.size())
                .arg(pages_.at(row).width)
                .arg(pages_.at(row).height)
                .arg(toString(pages_.at(row).status))
                .arg(toString(pages_.at(row).split)));
    }
    if (auto* item = pageList_->item(row)) {
        item->setText(pageListText(pages_.at(row)));
    }
    if (!pageMatchesFilters(pages_.at(row))) {
        applyPageFilters();
    }
}

void MainWindow::pushAnnotationUndoState() {
    annotationController_->pushUndoState();
    updateAnnotationHistoryActions();
}

void MainWindow::updateAnnotationHistoryActions() {
    const bool hasPage = context_ && annotationController_->hasCurrentAnnotation();
    if (undoAnnotationButton_) {
        undoAnnotationButton_->setEnabled(hasPage && annotationController_->canUndo());
    }
    if (redoAnnotationButton_) {
        redoAnnotationButton_->setEnabled(hasPage && annotationController_->canRedo());
    }
}

void MainWindow::undoAnnotationEdit() {
    if (!context_ || !annotationController_->undo()) {
        return;
    }
    persistCurrentAnnotation();
    refreshCanvasAnnotation();
    updateSelectedRegionPanel();
    refreshImageLabelPanel();
    updateAnnotationHistoryActions();
    statusBar()->showMessage(QStringLiteral("Redo annotation edit"), 2000);
}

void MainWindow::redoAnnotationEdit() {
    if (!context_ || !annotationController_->redo()) {
        return;
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
        setComboToValue(pageSplitCombo_, toString(pages_.at(row).split));
        setComboToValue(pageStatusCombo_, toString(pages_.at(row).status));
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
    const QJsonObject region = annotationController_->selectedRegion();
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
    for (const auto& value : annotationController_->currentAnnotation().value("image_labels").toArray()) {
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
    const auto ocrConfig = InferenceService::defaultOcrConfig(baseDir_);
    addPredictionModelChoices(labelDetModelCombo_, context, tasksForKind(QStringLiteral("det")), ocrConfig.textDetectionModelDir);
    addPredictionModelChoices(labelRecModelCombo_, context, tasksForKind(QStringLiteral("rec")), ocrConfig.textRecognitionModelDir);

    const auto textlineClsConfig = InferenceService::defaultClassificationConfig(QStringLiteral("textline_orientation"), baseDir_);
    addPredictionModelChoices(
        labelTextlineClsModelCombo_,
        context,
        classificationTasksForPrediction(QStringLiteral("textline_orientation")),
        textlineClsConfig.modelDir,
        true);

    const auto docClsConfig = InferenceService::defaultClassificationConfig(QStringLiteral("doc_orientation"), baseDir_);
    addPredictionModelChoices(
        labelDocOrientationModelCombo_,
        context,
        classificationTasksForPrediction(QStringLiteral("doc_orientation")),
        docClsConfig.modelDir,
        true);

    const auto layoutConfig = InferenceService::defaultLayoutConfig(baseDir_);
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
    DatasetController::saveProject(*context_);
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
        return InferenceService::gpuSupported() ? QStringLiteral("gpu:0") : QStringLiteral("cpu");
    }
    const QString predictionDevice = predictDeviceCombo_ ? predictDeviceCombo_->currentText().trimmed() : QString();
    if (predictionDevice.startsWith(QStringLiteral("gpu"), Qt::CaseInsensitive)
        && !InferenceService::gpuSupported()) {
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
    if (!InferenceService::modelDirLooksUsable(detDir, &reason)) {
        missing.append(QStringLiteral("Det 检测模型不可用：%1").arg(reason));
    }
    reason.clear();
    const QString recDir = labelSelectedModelDir(labelRecModelCombo_, predictRecModelCombo_);
    if (!InferenceService::modelDirLooksUsable(recDir, &reason)) {
        missing.append(QStringLiteral("Rec 识别模型不可用：%1").arg(reason));
    }
    if (labelEnableTextlineClsCheck_ && labelEnableTextlineClsCheck_->isChecked()) {
        reason.clear();
        const QString clsDir = labelSelectedModelDir(labelTextlineClsModelCombo_, predictClsModelCombo_);
        if (!InferenceService::modelDirLooksUsable(clsDir, &reason)) {
            missing.append(QStringLiteral("文本行方向模型不可用：%1").arg(reason));
        }
    }
    if (labelEnableDocOrientationCheck_ && labelEnableDocOrientationCheck_->isChecked()) {
        reason.clear();
        const QString docDir = labelSelectedModelDir(labelDocOrientationModelCombo_, predictClsModelCombo_);
        if (!InferenceService::modelDirLooksUsable(docDir, &reason)) {
            missing.append(QStringLiteral("文档方向模型不可用：%1").arg(reason));
        }
    }
    if (labelEnableDocUnwarpingCheck_ && labelEnableDocUnwarpingCheck_->isChecked()) {
        reason.clear();
        const QString unwarpDir = selectedPredictionModelDir(labelDocUnwarpingModelCombo_);
        if (unwarpDir.isEmpty() || !InferenceService::modelDirLooksUsable(unwarpDir, &reason)) {
            missing.append(QStringLiteral("文档矫正模型不可用：%1").arg(reason.isEmpty() ? QStringLiteral("未选择模型") : reason));
        }
    }
    const QString layoutDir = selectedPredictionModelDir(labelLayoutModelCombo_);
    if (!layoutDir.isEmpty()) {
        reason.clear();
        if (!InferenceService::layoutModelDirLooksUsable(layoutDir, &reason)) {
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
    const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("Select project directory"));
    if (path.isEmpty()) {
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("Project name"), QStringLiteral("Name"), QLineEdit::Normal, QFileInfo(path).fileName(), &ok);
    if (!ok) {
        return;
    }
    setProject(projectController_->createProject(path, name));
}

void MainWindow::openProjectDialog() {
    const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("Open project directory"));
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
    if (updatingPageMetadata_ || !context_ || !annotationController_->hasCurrentAnnotation()) {
        return;
    }
    const int row = pageList_ ? pageList_->currentRow() : -1;
    if (row < 0 || row >= pages_.size()) {
        return;
    }
    QJsonObject annotation = annotationController_->currentAnnotation();
    annotation["split"] = pageSplitCombo_ ? comboStoredValue(pageSplitCombo_) : toString(pages_.at(row).split);
    annotation["status"] = pageStatusCombo_ ? comboStoredValue(pageStatusCombo_) : toString(pages_.at(row).status);
    annotationController_->replaceCurrentAnnotation(annotation);
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
        const auto imported = DatasetService::importAssets(*context_, paths);
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
        const auto imported = DatasetService::importAssets(*context_, paths);
        appendLog(QStringLiteral("已导入 %1 页 PDF").arg(imported.size()));
        refreshPages();
    } catch (const std::exception& exc) {
        QMessageBox::critical(this, QStringLiteral("Import PDFs failed"), QString::fromUtf8(exc.what()));
    }
}

void MainWindow::validateProject() {
    if (!context_) {
        return;
    }
    const auto issues = ExportService::validateProject(*context_);
    const QString logPath = ExportService::writeValidationLog(*context_, issues);
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

void MainWindow::browseExportOutputDir() {
    const QString startDir = exportOutputDirEdit_ && !exportOutputDirEdit_->text().trimmed().isEmpty()
        ? exportOutputDirEdit_->text().trimmed()
        : (context_ ? context_->exportRoot() : baseDir_);
    const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("选择导出目录"), startDir);
    if (!path.isEmpty() && exportOutputDirEdit_) {
        exportOutputDirEdit_->setText(QFileInfo(path).absoluteFilePath());
    }
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
        QMessageBox::information(this, QStringLiteral("Prelabel running"), QStringLiteral("Wait for prelabel writes to finish before exporting."));
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
    Exporter::ExportOptions exportOptions;
    const QString requestedOutputRoot = exportOutputDirEdit_ ? exportOutputDirEdit_->text().trimmed() : QString();
    if (!requestedOutputRoot.isEmpty()) {
        const QString requestedRoot = QFileInfo(requestedOutputRoot).absoluteFilePath();
        const QString projectExportRoot = QFileInfo(context_->exportRoot()).absoluteFilePath();
        if (QDir::cleanPath(requestedRoot).compare(QDir::cleanPath(projectExportRoot), Qt::CaseInsensitive) != 0) {
            exportOptions.outputRoot = requestedRoot;
            exportOptions.timestampedTaskDirs = true;
            if (!pathIsSameOrInside(requestedRoot, context_->exportRoot())) {
                const auto reply = QMessageBox::question(
                    this,
                    QStringLiteral("外部导出目录"),
                    QStringLiteral("你选择的目录不是当前项目 exports 子目录。\n\n不会清空该目录，实际输出会写入带时间戳的任务子目录，例如：\n%1\n\n继续导出？")
                        .arg(nativePathText(QDir(requestedRoot).filePath(QStringLiteral("ppocr_det_yyyyMMdd_HHmmss")))),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No);
                if (reply != QMessageBox::Yes) {
                    return;
                }
            }
        }
    }
    const ProjectContext context = *context_;
    QPointer<MainWindow> window(this);
    auto* thread = QThread::create([window, context, tasks, checkedOnly, exportOptions]() {
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
            result.outputs = ExportService::exportSelected(context, tasks, checkedOnly, true, postProgress, exportOptions);
        } catch (const std::exception& exc) {
            result.error = QString::fromUtf8(exc.what());
            if (result.error.contains(QStringLiteral("validation"), Qt::CaseInsensitive)) {
                result.validationIssues = ExportService::validateProject(context);
                result.validationLogPath = ExportService::writeValidationLog(context, result.validationIssues);
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
                    window->exportProgressLabel_->setText(QStringLiteral("Export failed"));
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
        exportProgressLabel_->setText(QStringLiteral("Preparing export"));
    }
    statusBar()->showMessage(QStringLiteral("Export started"), 3000);
    connect(thread, &QThread::finished, this, [this, thread]() {
        if (activeExportThread_ == thread) {
            activeExportThread_ = nullptr;
        }
        thread->deleteLater();
    });
    thread->start();
}

void MainWindow::showEnvironmentReport() {
    EnvironmentReportRequest options;
    options.baseDir = baseDir_;
    options.pythonExe = paddlexPythonPath();
    const QJsonObject report = EnvironmentService::build(options);
    const QString logRoot = context_ ? context_->logRoot() : QDir(baseDir_).filePath(QStringLiteral("logs"));
    QDir().mkpath(logRoot);
    const QString reportPath = QDir(logRoot).filePath(QStringLiteral("environment_report.json"));
    EnvironmentService::writeJson(reportPath, report);
    const QString reportText = QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented));
    appendLog(reportText);
    if (settingsReportView_) {
        settingsReportView_->setPlainText(reportText);
    }
    refreshLogsPage();
    QMessageBox::information(
        this,
        QStringLiteral("Environment report"),
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

}  // namespace ppocr

