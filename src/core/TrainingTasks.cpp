#include "core/TrainingTasks.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QSet>

namespace ppocr {
namespace {

TrainingTaskSpec makeTask(
    const QString& key,
    const QString& title,
    TrainingTaskKind kind,
    const QString& exportTask,
    const QString& datasetName,
    const QString& configRel,
    int epochs,
    int batchSize,
    double learningRate,
    const QString& bestWeightRel = QStringLiteral("best_accuracy/best_accuracy.pdparams"),
    const QString& inferDirRel = QStringLiteral("best_accuracy/inference"),
    int numClasses = 0,
    int warmupSteps = 0,
    bool trainSupported = true,
    const QString& note = QString()) {
    TrainingTaskSpec task;
    task.key = key;
    task.title = title;
    task.kind = kind;
    task.exportTask = exportTask;
    task.datasetName = datasetName;
    task.configRel = configRel;
    task.bestWeightRel = bestWeightRel;
    task.inferDirRel = inferDirRel;
    task.epochs = epochs;
    task.batchSize = batchSize;
    task.learningRate = learningRate;
    task.numClasses = numClasses;
    task.warmupSteps = warmupSteps;
    task.trainSupported = trainSupported;
    task.note = note;
    return task;
}

QString moduleConfig(const QString& relativePath) {
    return QStringLiteral("PaddleX/paddlex/configs/modules/%1").arg(relativePath);
}

QList<TrainingTaskSpec> builtInTrainingTasks() {
    const QString bestModelWeight = QStringLiteral("best_model/best_model.pdparams");
    const QString bestModelInfer = QStringLiteral("best_model/inference");

    return {
        makeTask(
            QStringLiteral("det_v5_server"),
            QStringLiteral("Det | PP-OCRv5 server"),
            TrainingTaskKind::OcrDet,
            QStringLiteral("det"),
            QStringLiteral("ppocr_det"),
            moduleConfig(QStringLiteral("text_detection/PP-OCRv5_server_det.yaml")),
            100,
            4,
            0.001),
        makeTask(
            QStringLiteral("det_v5_mobile"),
            QStringLiteral("Det | PP-OCRv5 mobile"),
            TrainingTaskKind::OcrDet,
            QStringLiteral("det"),
            QStringLiteral("ppocr_det"),
            moduleConfig(QStringLiteral("text_detection/PP-OCRv5_mobile_det.yaml")),
            100,
            8,
            0.001),
        makeTask(
            QStringLiteral("det_v4_server"),
            QStringLiteral("Det | PP-OCRv4 server"),
            TrainingTaskKind::OcrDet,
            QStringLiteral("det"),
            QStringLiteral("ppocr_det"),
            moduleConfig(QStringLiteral("text_detection/PP-OCRv4_server_det.yaml")),
            100,
            4,
            0.001),
        makeTask(
            QStringLiteral("det_v4_mobile"),
            QStringLiteral("Det | PP-OCRv4 mobile"),
            TrainingTaskKind::OcrDet,
            QStringLiteral("det"),
            QStringLiteral("ppocr_det"),
            moduleConfig(QStringLiteral("text_detection/PP-OCRv4_mobile_det.yaml")),
            100,
            8,
            0.001),
        makeTask(
            QStringLiteral("rec_v5_server"),
            QStringLiteral("Rec | PP-OCRv5 server"),
            TrainingTaskKind::OcrRec,
            QStringLiteral("rec"),
            QStringLiteral("ppocr_rec"),
            moduleConfig(QStringLiteral("text_recognition/PP-OCRv5_server_rec.yaml")),
            20,
            8,
            0.001),
        makeTask(
            QStringLiteral("rec_v5_mobile"),
            QStringLiteral("Rec | PP-OCRv5 mobile"),
            TrainingTaskKind::OcrRec,
            QStringLiteral("rec"),
            QStringLiteral("ppocr_rec"),
            moduleConfig(QStringLiteral("text_recognition/PP-OCRv5_mobile_rec.yaml")),
            20,
            16,
            0.001),
        makeTask(
            QStringLiteral("rec_v4_server_doc"),
            QStringLiteral("Rec | PP-OCRv4 server document"),
            TrainingTaskKind::OcrRec,
            QStringLiteral("rec"),
            QStringLiteral("ppocr_rec"),
            moduleConfig(QStringLiteral("text_recognition/PP-OCRv4_server_rec_doc.yaml")),
            20,
            8,
            0.001),
        makeTask(
            QStringLiteral("rec_v4_server"),
            QStringLiteral("Rec | PP-OCRv4 server"),
            TrainingTaskKind::OcrRec,
            QStringLiteral("rec"),
            QStringLiteral("ppocr_rec"),
            moduleConfig(QStringLiteral("text_recognition/PP-OCRv4_server_rec.yaml")),
            20,
            8,
            0.001),
        makeTask(
            QStringLiteral("rec_v4_mobile"),
            QStringLiteral("Rec | PP-OCRv4 mobile"),
            TrainingTaskKind::OcrRec,
            QStringLiteral("rec"),
            QStringLiteral("ppocr_rec"),
            moduleConfig(QStringLiteral("text_recognition/PP-OCRv4_mobile_rec.yaml")),
            20,
            16,
            0.001),
        makeTask(
            QStringLiteral("rec_v4_mobile_en"),
            QStringLiteral("Rec | PP-OCRv4 mobile English"),
            TrainingTaskKind::OcrRec,
            QStringLiteral("rec"),
            QStringLiteral("ppocr_rec"),
            moduleConfig(QStringLiteral("text_recognition/en_PP-OCRv4_mobile_rec.yaml")),
            20,
            16,
            0.001),
        makeTask(
            QStringLiteral("doc_ori_x1"),
            QStringLiteral("Cls | Document orientation"),
            TrainingTaskKind::DocCls,
            QStringLiteral("cls"),
            QStringLiteral("ppocr_cls"),
            moduleConfig(QStringLiteral("doc_text_orientation/PP-LCNet_x1_0_doc_ori.yaml")),
            50,
            16,
            0.08,
            bestModelWeight,
            bestModelInfer,
            4,
            100),
        makeTask(
            QStringLiteral("textline_ori_x1"),
            QStringLiteral("Cls | Text line orientation x1.0"),
            TrainingTaskKind::TextlineCls,
            QStringLiteral("textline_cls"),
            QStringLiteral("ppocr_textline_cls"),
            moduleConfig(QStringLiteral("textline_orientation/PP-LCNet_x1_0_textline_ori.yaml")),
            20,
            32,
            0.8,
            bestModelWeight,
            bestModelInfer,
            2,
            100),
        makeTask(
            QStringLiteral("textline_ori_x025"),
            QStringLiteral("Cls | Text line orientation x0.25"),
            TrainingTaskKind::TextlineCls,
            QStringLiteral("textline_cls"),
            QStringLiteral("ppocr_textline_cls"),
            moduleConfig(QStringLiteral("textline_orientation/PP-LCNet_x0_25_textline_ori.yaml")),
            20,
            64,
            0.8,
            bestModelWeight,
            bestModelInfer,
            2,
            100),
        makeTask(
            QStringLiteral("table_cls_x1"),
            QStringLiteral("Cls | Table classification"),
            TrainingTaskKind::TableCls,
            QStringLiteral("table_cls"),
            QStringLiteral("table_classification"),
            moduleConfig(QStringLiteral("table_classification/PP-LCNet_x1_0_table_cls.yaml")),
            20,
            128,
            0.1,
            bestModelWeight,
            bestModelInfer,
            2,
            5),
        makeTask(
            QStringLiteral("layout_plus_l"),
            QStringLiteral("Layout | PP-DocLayout plus-L"),
            TrainingTaskKind::Layout,
            QStringLiteral("coco"),
            QStringLiteral("coco_layout"),
            moduleConfig(QStringLiteral("layout_detection/PP-DocLayout_plus-L.yaml")),
            100,
            1,
            0.0001,
            bestModelWeight,
            bestModelInfer,
            6,
            100),
        makeTask(
            QStringLiteral("layout_l"),
            QStringLiteral("Layout | PP-DocLayout-L"),
            TrainingTaskKind::Layout,
            QStringLiteral("coco"),
            QStringLiteral("coco_layout"),
            moduleConfig(QStringLiteral("layout_detection/PP-DocLayout-L.yaml")),
            100,
            1,
            0.0001,
            bestModelWeight,
            bestModelInfer,
            6,
            100),
        makeTask(
            QStringLiteral("layout_m"),
            QStringLiteral("Layout | PP-DocLayout-M"),
            TrainingTaskKind::Layout,
            QStringLiteral("coco"),
            QStringLiteral("coco_layout"),
            moduleConfig(QStringLiteral("layout_detection/PP-DocLayout-M.yaml")),
            100,
            2,
            0.0001,
            bestModelWeight,
            bestModelInfer,
            6,
            100),
        makeTask(
            QStringLiteral("layout_s"),
            QStringLiteral("Layout | PP-DocLayout-S"),
            TrainingTaskKind::Layout,
            QStringLiteral("coco"),
            QStringLiteral("coco_layout"),
            moduleConfig(QStringLiteral("layout_detection/PP-DocLayout-S.yaml")),
            100,
            4,
            0.0001,
            bestModelWeight,
            bestModelInfer,
            6,
            100),
        makeTask(
            QStringLiteral("layout_block"),
            QStringLiteral("Layout | PP-DocBlockLayout"),
            TrainingTaskKind::Layout,
            QStringLiteral("coco"),
            QStringLiteral("coco_layout"),
            moduleConfig(QStringLiteral("layout_detection/PP-DocBlockLayout.yaml")),
            100,
            1,
            0.0001,
            bestModelWeight,
            bestModelInfer,
            6,
            100),
        makeTask(
            QStringLiteral("uvdoc"),
            QStringLiteral("Doc unwarping | UVDoc"),
            TrainingTaskKind::Unknown,
            QStringLiteral("uvdoc"),
            QStringLiteral("uvdoc_unwarping"),
            moduleConfig(QStringLiteral("image_unwarping/UVDoc.yaml")),
            0,
            1,
            0.0,
            QStringLiteral("best_accuracy"),
            QStringLiteral("best_accuracy"),
            0,
            0,
            false,
            QStringLiteral("UVDoc training is listed for compatibility, but the workbench does not generate an unwarping dataset yet.")),
    };
}

QStringList manifestSearchDirs() {
    QStringList candidates;
    const QByteArray envDir = qgetenv("PPOCR_TRAINING_TASKS_DIR");
    if (!envDir.isEmpty()) {
        candidates.append(QString::fromLocal8Bit(envDir));
    }
#ifdef PPOCR_PROJECT_ROOT
    candidates.append(QDir(QStringLiteral(PPOCR_PROJECT_ROOT)).filePath(QStringLiteral("resources/training_tasks")));
#endif
    const QString appDir = QCoreApplication::applicationDirPath();
    if (!appDir.isEmpty()) {
        candidates.append(QDir(appDir).filePath(QStringLiteral("resources/training_tasks")));
        candidates.append(QDir(appDir).filePath(QStringLiteral("../resources/training_tasks")));
    }
    candidates.append(QDir::current().filePath(QStringLiteral("resources/training_tasks")));

    QStringList result;
    QSet<QString> seen;
    for (const auto& path : candidates) {
        const QString absolute = QFileInfo(path).absoluteFilePath();
        if (seen.contains(absolute)) {
            continue;
        }
        seen.insert(absolute);
        result.append(absolute);
    }
    return result;
}

int jsonInt(const QJsonObject& object, const QString& key, int fallback) {
    const QJsonValue value = object.value(key);
    if (value.isDouble()) {
        return value.toInt(fallback);
    }
    if (value.isString()) {
        bool ok = false;
        const int parsed = value.toString().toInt(&ok);
        if (ok) {
            return parsed;
        }
    }
    return fallback;
}

double jsonDouble(const QJsonObject& object, const QString& key, double fallback) {
    const QJsonValue value = object.value(key);
    if (value.isDouble()) {
        return value.toDouble(fallback);
    }
    if (value.isString()) {
        bool ok = false;
        const double parsed = value.toString().toDouble(&ok);
        if (ok) {
            return parsed;
        }
    }
    return fallback;
}

TrainingTaskSpec taskFromManifest(const QJsonObject& object, bool* ok) {
    TrainingTaskSpec task;
    task.key = object.value(QStringLiteral("key")).toString().trimmed();
    task.title = object.value(QStringLiteral("title")).toString(task.key).trimmed();
    task.kind = trainingTaskKindFromString(object.value(QStringLiteral("kind")).toString()).value_or(TrainingTaskKind::Unknown);
    task.exportTask = object.value(QStringLiteral("export_task")).toString().trimmed();
    task.datasetName = object.value(QStringLiteral("dataset_name")).toString().trimmed();
    task.configRel = object.value(QStringLiteral("config_rel")).toString(object.value(QStringLiteral("config")).toString()).trimmed();
    task.bestWeightRel = object.value(QStringLiteral("best_weight_rel")).toString(task.bestWeightRel).trimmed();
    task.inferDirRel = object.value(QStringLiteral("infer_dir_rel")).toString(task.inferDirRel).trimmed();
    task.trainSupported = object.value(QStringLiteral("train_supported")).toBool(true);
    task.note = object.value(QStringLiteral("note")).toString();

    const QJsonObject params = object.value(QStringLiteral("default_params")).toObject();
    task.epochs = jsonInt(params, QStringLiteral("epochs"), jsonInt(object, QStringLiteral("epochs"), task.epochs));
    task.batchSize = jsonInt(params, QStringLiteral("batch_size"), jsonInt(object, QStringLiteral("batch_size"), task.batchSize));
    task.learningRate = jsonDouble(params, QStringLiteral("learning_rate"), jsonDouble(object, QStringLiteral("learning_rate"), task.learningRate));
    task.numClasses = jsonInt(params, QStringLiteral("num_classes"), jsonInt(object, QStringLiteral("num_classes"), task.numClasses));
    task.warmupSteps = jsonInt(params, QStringLiteral("warmup_steps"), jsonInt(object, QStringLiteral("warmup_steps"), task.warmupSteps));

    if (ok) {
        *ok = !task.key.isEmpty()
            && !task.title.isEmpty()
            && !task.exportTask.isEmpty()
            && !task.datasetName.isEmpty()
            && !task.configRel.isEmpty();
    }
    return task;
}

QList<TrainingTaskSpec> orderedManifestTasks(const QList<TrainingTaskSpec>& tasks) {
    QMap<QString, TrainingTaskSpec> byKey;
    for (const auto& task : tasks) {
        byKey.insert(task.key, task);
    }

    QList<TrainingTaskSpec> ordered;
    for (const auto& builtIn : builtInTrainingTasks()) {
        if (!byKey.contains(builtIn.key)) {
            continue;
        }
        ordered.append(byKey.take(builtIn.key));
    }
    for (auto it = byKey.begin(); it != byKey.end(); ++it) {
        ordered.append(it.value());
    }
    return ordered;
}

QList<TrainingTaskSpec> loadManifestTasks() {
    for (const auto& dirPath : manifestSearchDirs()) {
        QDir dir(dirPath);
        if (!dir.exists()) {
            continue;
        }
        const auto files = dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        QList<TrainingTaskSpec> tasks;
        QSet<QString> keys;
        for (const auto& info : files) {
            QFile file(info.absoluteFilePath());
            if (!file.open(QIODevice::ReadOnly)) {
                continue;
            }
            QJsonParseError error;
            const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error != QJsonParseError::NoError || !doc.isObject()) {
                continue;
            }
            bool ok = false;
            TrainingTaskSpec task = taskFromManifest(doc.object(), &ok);
            if (!ok || keys.contains(task.key)) {
                continue;
            }
            keys.insert(task.key);
            tasks.append(task);
        }
        if (!tasks.isEmpty()) {
            return orderedManifestTasks(tasks);
        }
    }
    return {};
}

}  // namespace

QList<TrainingTaskSpec> trainingTasks() {
    static const QList<TrainingTaskSpec> tasks = [] {
        const QList<TrainingTaskSpec> manifestTasks = loadManifestTasks();
        return manifestTasks.isEmpty() ? builtInTrainingTasks() : manifestTasks;
    }();
    return tasks;
}

TrainingTaskSpec trainingTaskByKey(const QString& key) {
    const QString normalizedKey = key == QStringLiteral("layout_doc_s") ? QStringLiteral("layout_s") : key;
    const auto tasks = trainingTasks();
    for (const auto& task : tasks) {
        if (task.key == normalizedKey) {
            return task;
        }
    }
    return tasks.first();
}

}  // namespace ppocr
