#include "core/TrainingTasks.h"

namespace ppocr {
namespace {

TrainingTaskSpec makeTask(
    const QString& key,
    const QString& title,
    const QString& kind,
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

}  // namespace

QList<TrainingTaskSpec> trainingTasks() {
    const QString bestModelWeight = QStringLiteral("best_model/best_model.pdparams");
    const QString bestModelInfer = QStringLiteral("best_model/inference");

    return {
        makeTask(
            QStringLiteral("det_v5_server"),
            QStringLiteral("Det | PP-OCRv5 server"),
            QStringLiteral("det"),
            QStringLiteral("det"),
            QStringLiteral("ppocr_det"),
            moduleConfig(QStringLiteral("text_detection/PP-OCRv5_server_det.yaml")),
            100,
            4,
            0.001),
        makeTask(
            QStringLiteral("det_v5_mobile"),
            QStringLiteral("Det | PP-OCRv5 mobile"),
            QStringLiteral("det"),
            QStringLiteral("det"),
            QStringLiteral("ppocr_det"),
            moduleConfig(QStringLiteral("text_detection/PP-OCRv5_mobile_det.yaml")),
            100,
            8,
            0.001),
        makeTask(
            QStringLiteral("det_v4_server"),
            QStringLiteral("Det | PP-OCRv4 server"),
            QStringLiteral("det"),
            QStringLiteral("det"),
            QStringLiteral("ppocr_det"),
            moduleConfig(QStringLiteral("text_detection/PP-OCRv4_server_det.yaml")),
            100,
            4,
            0.001),
        makeTask(
            QStringLiteral("det_v4_mobile"),
            QStringLiteral("Det | PP-OCRv4 mobile"),
            QStringLiteral("det"),
            QStringLiteral("det"),
            QStringLiteral("ppocr_det"),
            moduleConfig(QStringLiteral("text_detection/PP-OCRv4_mobile_det.yaml")),
            100,
            8,
            0.001),
        makeTask(
            QStringLiteral("rec_v5_server"),
            QStringLiteral("Rec | PP-OCRv5 server"),
            QStringLiteral("rec"),
            QStringLiteral("rec"),
            QStringLiteral("ppocr_rec"),
            moduleConfig(QStringLiteral("text_recognition/PP-OCRv5_server_rec.yaml")),
            20,
            8,
            0.001),
        makeTask(
            QStringLiteral("rec_v5_mobile"),
            QStringLiteral("Rec | PP-OCRv5 mobile"),
            QStringLiteral("rec"),
            QStringLiteral("rec"),
            QStringLiteral("ppocr_rec"),
            moduleConfig(QStringLiteral("text_recognition/PP-OCRv5_mobile_rec.yaml")),
            20,
            16,
            0.001),
        makeTask(
            QStringLiteral("rec_v4_server_doc"),
            QStringLiteral("Rec | PP-OCRv4 server document"),
            QStringLiteral("rec"),
            QStringLiteral("rec"),
            QStringLiteral("ppocr_rec"),
            moduleConfig(QStringLiteral("text_recognition/PP-OCRv4_server_rec_doc.yaml")),
            20,
            8,
            0.001),
        makeTask(
            QStringLiteral("rec_v4_server"),
            QStringLiteral("Rec | PP-OCRv4 server"),
            QStringLiteral("rec"),
            QStringLiteral("rec"),
            QStringLiteral("ppocr_rec"),
            moduleConfig(QStringLiteral("text_recognition/PP-OCRv4_server_rec.yaml")),
            20,
            8,
            0.001),
        makeTask(
            QStringLiteral("rec_v4_mobile"),
            QStringLiteral("Rec | PP-OCRv4 mobile"),
            QStringLiteral("rec"),
            QStringLiteral("rec"),
            QStringLiteral("ppocr_rec"),
            moduleConfig(QStringLiteral("text_recognition/PP-OCRv4_mobile_rec.yaml")),
            20,
            16,
            0.001),
        makeTask(
            QStringLiteral("rec_v4_mobile_en"),
            QStringLiteral("Rec | PP-OCRv4 mobile English"),
            QStringLiteral("rec"),
            QStringLiteral("rec"),
            QStringLiteral("ppocr_rec"),
            moduleConfig(QStringLiteral("text_recognition/en_PP-OCRv4_mobile_rec.yaml")),
            20,
            16,
            0.001),
        makeTask(
            QStringLiteral("doc_ori_x1"),
            QStringLiteral("Cls | Document orientation"),
            QStringLiteral("cls"),
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
            QStringLiteral("cls"),
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
            QStringLiteral("cls"),
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
            QStringLiteral("cls"),
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
            QStringLiteral("layout"),
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
            QStringLiteral("layout"),
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
            QStringLiteral("layout"),
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
            QStringLiteral("layout"),
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
            QStringLiteral("layout"),
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
            QStringLiteral("uvdoc"),
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
