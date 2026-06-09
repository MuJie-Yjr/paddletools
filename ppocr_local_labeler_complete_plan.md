# PPOCR 本地离线标注工具完整方案

## 1. 项目定位

本项目不是在线标注平台，也不是云端 OCR 平台，而是一个本地单机运行的 PPOCR / PaddleX 数据标注工具。

核心定位：

本地图片 / PDF 导入 → 本地人工标注 → 可选本地模型预标注 → 本地数据校验 → 本地导出 PaddleOCR / PaddleX 可训练数据集。

第一版必须坚持 5 个边界：

1. 不联网。
2. 不登录。
3. 不使用数据库。
4. 不做多人协同。
5. 不依赖云端模型或在线接口。

第一版不要追求大而全。真正有价值的是把 OCR 检测、OCR 识别、分类、版面检测这四条数据链路打通。

---

## 2. 一句话架构

Qt 本地桌面端 + 本地项目文件夹 + JSON 中间标注格式 + 本地 PaddleOCR 预标注引擎 + 数据校验器 + 多格式导出器。

整体流程：

```text
新建项目
  ↓
导入图片 / PDF
  ↓
生成页面图片
  ↓
人工标注 / 本地模型预标注
  ↓
统一保存为 annotation.json
  ↓
数据校验
  ↓
导出 PaddleOCR / PaddleX 数据集
```

---

## 3. 第一版功能边界

### 3.1 必须做

第一版必须完成这些功能：

1. 新建 / 打开本地项目。
2. 图片导入。
3. PDF 导入并转图片。
4. 图片列表管理。
5. 图片预览、缩放、拖拽。
6. OCR 文本框标注。
7. 文本内容编辑。
8. 忽略区域标注。
9. 阅读顺序调整。
10. 文档方向分类。
11. 文本行方向分类。
12. 表格类型分类。
13. 版面矩形框标注。
14. 文本识别 crop 自动生成。
15. 数据校验。
16. 导出 PaddleOCR Det 数据集。
17. 导出 PaddleOCR Rec 数据集。
18. 导出 PaddleOCR / PaddleX Cls 数据集。
19. 导出 COCO 检测数据集。
20. 可选加载本地 PaddleOCR 模型做预标注。

### 3.2 第一版不要做

第一版不要做这些：

1. 在线 OCR。
2. 在线模型下载。
3. 在线 VLM。
4. 图表解析。
5. 表格结构还原。
6. LaTeX 公式识别。
7. 多人协同。
8. 账号权限。
9. 云端同步。
10. 复杂审核流。

这些不是没价值，而是复杂度太高。第一版做进去会拖垮项目。

---

## 4. 本地项目目录结构

每个项目就是一个普通文件夹。

```text
my_ppocr_project/
├── project.json
├── assets/
│   ├── raw/
│   │   ├── origin_001.pdf
│   │   └── origin_002.jpg
│   ├── pages/
│   │   ├── page_000001.jpg
│   │   ├── page_000002.jpg
│   │   └── page_000003.jpg
│   └── thumbs/
├── annotations/
│   ├── page_000001.json
│   ├── page_000002.json
│   └── page_000003.json
├── crops/
│   ├── rec_train/
│   ├── rec_val/
│   └── preview/
├── exports/
│   ├── ppocr_det/
│   ├── ppocr_rec/
│   ├── ppocr_cls/
│   └── coco_layout/
├── cache/
│   ├── ocr_prelabel/
│   └── image_info.json
└── logs/
    ├── validate.log
    └── export.log
```

不建议第一版上数据库。原因很简单：

1. 标注文件天然适合 JSON。
2. 用户可以直接备份项目文件夹。
3. 崩溃后容易恢复。
4. 导出器读取 JSON 足够快。
5. 后期需要数据库时可以迁移。

---

## 5. project.json 设计

```json
{
  "project_name": "invoice_ocr_project",
  "version": "1.0",
  "offline_mode": true,
  "created_at": "2026-05-25 16:00:00",
  "image_root": "assets/pages",
  "annotation_root": "annotations",
  "crop_root": "crops",
  "export_root": "exports",
  "label_sets": {
    "doc_orientation": ["0", "90", "180", "270"],
    "textline_orientation": ["0", "180"],
    "layout": ["title", "text", "table", "image", "formula", "seal"],
    "table_type": ["wired_table", "wireless_table"]
  },
  "splits": {
    "train_ratio": 0.8,
    "val_ratio": 0.2,
    "strategy": "manual_or_stratified"
  },
  "local_models": {
    "det_model_dir": "",
    "rec_model_dir": "",
    "cls_model_dir": "",
    "use_gpu": false
  }
}
```

`offline_mode=true` 必须落实到程序行为，不是摆设。程序不允许自动下载模型、不允许访问在线 OCR、不允许检查在线更新。

---

## 6. 中间标注格式

核心原则：内部永远不直接保存 PaddleOCR 的 `train.txt`，而是保存统一 JSON。导出时再转换成目标格式。

每张图片对应一个 JSON 文件。

```json
{
  "asset_id": "page_000001",
  "image_path": "assets/pages/page_000001.jpg",
  "width": 2480,
  "height": 3508,
  "page_index": 1,
  "split": "train",
  "status": "labeled",
  "image_labels": [
    {
      "task": "doc_orientation",
      "label": "0"
    },
    {
      "task": "table_classification",
      "label": "wired_table"
    }
  ],
  "regions": [
    {
      "id": "text_000001",
      "type": "ocr_text",
      "shape": "quad",
      "points": [[100, 120], [500, 120], [500, 180], [100, 180]],
      "text": "合同编号：A20260525",
      "ignore": false,
      "reading_order": 1,
      "source": "manual",
      "checked": true,
      "confidence": null
    },
    {
      "id": "layout_000001",
      "type": "layout",
      "label": "table",
      "bbox": [80, 300, 2200, 1200],
      "source": "manual",
      "checked": true
    }
  ],
  "rec_crops": [
    {
      "region_id": "text_000001",
      "crop_path": "crops/rec_train/page_000001_text_000001.jpg",
      "text": "合同编号：A20260525",
      "checked": true
    }
  ],
  "validation": {
    "passed": false,
    "errors": [],
    "warnings": []
  }
}
```

字段解释：

- `image_labels`：整图分类标签。
- `regions`：原图上的框、多边形、版面区域。
- `rec_crops`：从 OCR 框裁剪出来的文本识别训练样本。
- `checked`：是否人工确认。
- `source`：manual / auto。
- `validation`：质检结果。

导出训练集时，默认只导出 `checked=true` 的数据。自动预标注数据未经人工确认不能直接进入训练集。

---

## 7. 功能模块设计

### 7.1 项目管理模块

功能：

1. 新建项目。
2. 打开项目。
3. 保存项目配置。
4. 读取项目图片列表。
5. 维护图片状态。
6. 维护 train / val 划分。

核心类：

```text
ProjectManager
AssetManager
AnnotationStore
```

职责划分：

- `ProjectManager`：管理项目目录和 project.json。
- `AssetManager`：管理图片、PDF 转图、缩略图。
- `AnnotationStore`：读写 annotations/*.json。

### 7.2 图片 / PDF 导入模块

图片支持：

```text
jpg
jpeg
png
bmp
tif
tiff
```

PDF 支持：

```text
PDF 转 page_000001.jpg
默认 200 DPI
可选 300 DPI
```

PDF 转图建议使用：

```text
PyMuPDF / fitz
```

不要依赖在线转换。

### 7.3 画布模块

画布是整个工具的核心。

必须支持：

1. 鼠标缩放。
2. 鼠标拖拽。
3. 适应窗口。
4. 100% 显示。
5. 矩形框。
6. 四点框。
7. 多边形框。
8. 点位拖拽。
9. 框选中高亮。
10. 删除框。
11. 复制框。
12. 撤销 / 重做。
13. 快捷键。

不要用 QLabel 硬画。建议用：

```text
QGraphicsView + QGraphicsScene + QGraphicsItem
```

这套更适合缩放、拖拽、图层和标注对象管理。

### 7.4 OCR 检测标注模块

用于生成文本检测数据。

每个 OCR 框需要保存：

```text
points
text
ignore
reading_order
checked
source
```

框类型：

```text
rect
quad
polygon
```

第一版至少要支持 `rect` 和 `quad`。多边形可以放到 V2。

忽略区域：

```text
ignore=true
导出时 transcription="###"
```

### 7.5 OCR 识别 crop 模块

这个模块从 OCR 检测框生成识别训练数据。

流程：

```text
读取 OCR points
  ↓
四点透视矫正
  ↓
保存 crop 图片
  ↓
继承 OCR 框里的 text
  ↓
人工确认
  ↓
导出 rec_gt_train.txt / rec_gt_test.txt
```

裁剪必须考虑：

1. 点顺序。
2. 越界保护。
3. 空 crop 检测。
4. 透视变换失败处理。
5. crop 文件名唯一。

### 7.6 分类标注模块

分类任务包括：

```text
doc_orientation: 0 / 90 / 180 / 270
textline_orientation: 0 / 180
table_classification: wired_table / wireless_table
```

分类模式应该靠快捷键提升效率。

示例：

```text
1 = 0度
2 = 90度
3 = 180度
4 = 270度
```

### 7.7 版面检测模块

用于 layout_detection。

类别：

```text
title
text
table
image
formula
seal
```

只做矩形框，不写文本内容。

保存字段：

```json
{
  "id": "layout_000001",
  "type": "layout",
  "label": "table",
  "bbox": [100, 200, 900, 600],
  "checked": true
}
```

### 7.8 本地 PaddleOCR 预标注模块

这是增强功能，不是第一优先级。

模型来源必须是本地目录。

模型设置：

```text
检测模型目录 det_model_dir
识别模型目录 rec_model_dir
方向分类模型目录 cls_model_dir
是否使用 GPU
是否启用方向分类
```

预标注流程：

```text
选择图片
  ↓
调用本地 PaddleOCR
  ↓
生成 text regions
  ↓
source=auto
  ↓
用户人工修改
  ↓
checked=true
```

限制：

1. 不自动下载模型。
2. 不联网。
3. 不把未经确认的 auto 结果导出训练。

---

## 8. 导出器设计

导出器必须独立，不要写死在 UI 里。

```text
exporters/
├── ppocr_det_exporter.py
├── ppocr_rec_exporter.py
├── ppocr_cls_exporter.py
└── coco_layout_exporter.py
```

### 8.1 PaddleOCR Det 导出

目录：

```text
exports/ppocr_det/
├── images/
├── train.txt
└── val.txt
```

每行格式：

```text
images/page_000001.jpg	[{"transcription":"合同编号","points":[[100,120],[500,120],[500,180],[100,180]]}]
```

注意：图片路径和 JSON 之间必须是 `\t` 制表符。

### 8.2 PaddleOCR Rec 导出

目录：

```text
exports/ppocr_rec/
├── train/
├── test/
├── rec_gt_train.txt
└── rec_gt_test.txt
```

每行格式：

```text
train/page_000001_text_000001.jpg	合同编号：A20260525
```

### 8.3 Cls 导出

目录：

```text
exports/ppocr_cls/
├── images/
├── label.txt
├── train.txt
└── val.txt
```

`label.txt` 示例：

```text
0
90
180
270
```

`train.txt` 示例：

```text
images/page_000001.jpg	0
images/page_000002.jpg	180
```

### 8.4 COCO Layout 导出

目录：

```text
exports/coco_layout/
├── images/
└── annotations/
    ├── instance_train.json
    └── instance_val.json
```

用于：

```text
layout_detection
table_cells_detection
```

---

## 9. 数据校验规则

数据校验必须作为独立模块。导出前强制执行。

### 9.1 图片校验

检查：

1. 图片路径是否存在。
2. 图片是否可读取。
3. 宽高是否和 annotation 记录一致。
4. 图片是否损坏。

### 9.2 OCR Det 校验

检查：

1. points 至少 4 个点。
2. 点坐标不能越界。
3. 多边形不能自交。
4. 非 ignore 框 text 不能为空。
5. ignore 框导出为 `###`。
6. reading_order 不能重复。
7. 框面积不能过小。

### 9.3 OCR Rec 校验

检查：

1. crop 文件是否存在。
2. crop 是否能读取。
3. crop 宽高是否大于 0。
4. text 是否为空。
5. text 是否包含未知字符。
6. checked 是否为 true。

### 9.4 Cls 校验

检查：

1. 标签是否在 label_set 中。
2. train / val 是否都有样本。
3. 每类是否至少有验证样本。
4. 类别是否极度失衡。

### 9.5 COCO 校验

检查：

1. bbox 宽高是否大于 0。
2. bbox 是否越界。
3. category_id 是否合法。
4. image_id 是否存在。
5. annotation_id 是否唯一。

---

## 10. UI 页面设计

### 10.1 页面一：项目首页

用途：打开项目、新建项目、查看最近项目、导入数据。

布局：

```text
┌────────────────────────────────────────────────────────────┐
│ PPOCR 本地离线标注工具                                      │
├────────────────────────────────────────────────────────────┤
│                                                            │
│  ┌────────────────────┐   ┌────────────────────┐           │
│  │ 新建项目            │   │ 打开项目            │           │
│  │ 创建本地标注工程     │   │ 选择已有项目文件夹   │           │
│  └────────────────────┘   └────────────────────┘           │
│                                                            │
│  最近项目                                                   │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ invoice_ocr_project     D:/datasets/invoice           │  │
│  │ contract_label_project  D:/datasets/contract          │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                            │
│  离线状态：已启用    网络：禁用    本地模型：未配置          │
└────────────────────────────────────────────────────────────┘
```

设计重点：

1. 明确显示“离线状态”。
2. 最近项目直接进入。
3. 不出现登录、云端、同步按钮。

### 10.2 页面二：主标注工作台

这是最核心页面。

布局：

```text
┌────────────────────────────────────────────────────────────────────────┐
│ 顶部工具栏：打开项目 | 导入图片 | 导入PDF | 预标注 | 校验 | 导出       │
├───────────────┬───────────────────────────────────────┬────────────────┤
│ 左侧文件列表   │ 中间图片画布                           │ 右侧属性面板    │
│               │                                       │                │
│ 筛选：全部     │        [ 当前图片显示区域 ]              │ 当前模式：OCR   │
│ 状态：未标注   │                                       │                │
│               │   ┌───────────────────────────────┐   │ 文本内容        │
│ page_000001   │   │                               │   │ [合同编号...]   │
│ page_000002   │   │    OCR 框 / 版面框 / 多边形     │   │                │
│ page_000003   │   │                               │   │ ignore □       │
│               │   └───────────────────────────────┘   │ checked ☑      │
│               │                                       │ reading_order  │
│               │                                       │                │
├───────────────┴───────────────────────────────────────┴────────────────┤
│ 底部状态栏：图片尺寸 | 缩放比例 | 当前框数量 | 校验错误数 | 保存状态     │
└────────────────────────────────────────────────────────────────────────┘
```

模式切换：

```text
分类模式
OCR检测模式
OCR识别模式
版面检测模式
质检模式
```

设计重点：

1. 标注效率第一。
2. 中间画布最大。
3. 右侧只显示当前模式需要的属性。
4. 左侧文件状态必须清晰。

### 10.3 页面三：本地模型设置

用途：配置本地 PaddleOCR 模型路径。

布局：

```text
┌────────────────────────────────────────────────────────────┐
│ 本地模型设置                                                │
├────────────────────────────────────────────────────────────┤
│ 检测模型 det_model_dir                                      │
│ [D:/models/ppocr_det/]                         [选择目录]   │
│                                                            │
│ 识别模型 rec_model_dir                                      │
│ [D:/models/ppocr_rec/]                         [选择目录]   │
│                                                            │
│ 方向分类模型 cls_model_dir                                  │
│ [D:/models/ppocr_cls/]                         [选择目录]   │
│                                                            │
│ 使用 GPU      □                                             │
│ 启用方向分类   ☑                                             │
│                                                            │
│ [检查模型]    [测试当前图片]    [保存设置]                   │
│                                                            │
│ 状态：模型完整，本地加载成功                                 │
└────────────────────────────────────────────────────────────┘
```

设计重点：

1. 用户手动选择模型目录。
2. 不出现“下载模型”。
3. 检查模型文件是否存在。
4. 测试推理只使用本地图片。

### 10.4 页面四：导出与校验页面

用途：导出前选择任务，执行质检，生成数据集。

布局：

```text
┌────────────────────────────────────────────────────────────┐
│ 数据校验与导出                                              │
├────────────────────────────────────────────────────────────┤
│ 导出任务                                                    │
│ ☑ PaddleOCR 文本检测 Det                                    │
│ ☑ PaddleOCR 文本识别 Rec                                    │
│ ☑ PaddleOCR 方向分类 Cls                                    │
│ ☑ COCO 版面检测 Layout                                      │
│                                                            │
│ 导出范围                                                    │
│ ○ 全部图片   ● 仅 checked=true   ○ 当前筛选结果              │
│                                                            │
│ 数据划分                                                    │
│ train: 80%       val: 20%       策略：按类别均衡              │
│                                                            │
│ [开始校验]    [查看错误]    [开始导出]                       │
│                                                            │
│ 校验结果                                                    │
│ ┌──────────────────────────────────────────────────────┐   │
│ │ 错误 3：page_000012 OCR框越界                         │   │
│ │ 警告 5：doc_orientation 类别 270 验证集样本过少        │   │
│ │ 通过 986：可导出                                      │   │
│ └──────────────────────────────────────────────────────┘   │
└────────────────────────────────────────────────────────────┘
```

设计重点：

1. 导出前必须先校验。
2. 错误可以点击跳转到对应图片和框。
3. 默认只导出人工确认数据。
4. 导出结果写入项目 exports 目录。

---

## 11. 技术选型

### 11.1 GUI

推荐：

```text
PySide6
```

原因：

1. LGPL 授权更友好。
2. 官方 Qt for Python。
3. 适合桌面工具。
4. 和 PyInstaller 打包相对稳定。

### 11.2 图像处理

推荐：

```text
OpenCV
Pillow
numpy
```

用于：

1. 图片读取。
2. 透视裁剪。
3. crop 生成。
4. 图片尺寸读取。
5. 简单图像增强。

### 11.3 PDF 转图片

推荐：

```text
PyMuPDF
```

不要用在线 PDF 转换。

### 11.4 本地 OCR

推荐：

```text
PaddleOCR
```

但要做成可选依赖。

没有安装 PaddleOCR 时，工具仍然可以纯手工标注。

### 11.5 打包

推荐：

```text
PyInstaller
```

打包时要注意：

1. Qt plugins。
2. PaddleOCR 依赖。
3. OpenCV DLL。
4. 本地模型不要硬打进 exe，建议用户外部选择模型目录。

---

## 12. 开发排期

### 第一周：项目骨架

目标：能打开项目、导入图片、显示图片。

任务：

1. PySide6 主窗口。
2. 项目目录创建。
3. project.json 读写。
4. 图片导入。
5. PDF 转图片。
6. 左侧文件列表。
7. 中间图片显示。

### 第二周：标注画布

目标：能画框、保存框、重新加载框。

任务：

1. QGraphicsView 画布。
2. 矩形框。
3. 四点框。
4. 框选中。
5. 点位拖拽。
6. 删除框。
7. 保存 annotation json。
8. 加载 annotation json。

### 第三周：OCR 标注闭环

目标：能标文本检测数据和识别数据。

任务：

1. OCR 文本属性面板。
2. transcription 编辑。
3. ignore 标记。
4. reading_order。
5. crop 自动生成。
6. crop 预览。
7. rec 文本确认。

### 第四周：分类与版面框

目标：支持分类和 COCO 框。

任务：

1. 分类模式。
2. 快捷键打标签。
3. 版面矩形框。
4. 类别选择。
5. train / val split 设置。

### 第五周：导出器

目标：导出四类数据集。

任务：

1. ppocr_det_exporter。
2. ppocr_rec_exporter。
3. ppocr_cls_exporter。
4. coco_layout_exporter。
5. 导出日志。

### 第六周：校验器

目标：导出前自动质检。

任务：

1. 图片校验。
2. OCR Det 校验。
3. OCR Rec 校验。
4. Cls 校验。
5. COCO 校验。
6. 错误点击定位。

### 第七周：本地模型预标注

目标：支持本地 PaddleOCR 模型辅助标注。

任务：

1. 模型路径配置。
2. 模型完整性检查。
3. 单张图片预标注。
4. 批量预标注。
5. auto 标注转 checked。

### 第八周：打包和稳定性

目标：能交付给别人本地使用。

任务：

1. PyInstaller 打包。
2. 配置文件外置。
3. 日志系统。
4. 异常捕获。
5. 大图片性能优化。
6. 基础说明文档。

---

## 13. 最小可行版本 MVP

最小版本只做这些：

1. 打开项目。
2. 导入图片。
3. 显示图片。
4. 画 OCR 四点框。
5. 输入文本。
6. 保存 JSON。
7. 导出 PaddleOCR Det。

这个做通后再扩展 Rec、Cls、COCO。

不要一开始就写全量功能。正确策略是：

```text
先打通 Det
再打通 Rec
再打通 Cls
最后打通 COCO
```

---

## 14. 风险点

### 14.1 最大风险：功能范围失控

解决方案：

第一版只做 Det / Rec / Cls / COCO。

### 14.2 第二风险：画布难维护

解决方案：

用 QGraphicsView，不要自己用 QLabel 乱画。

### 14.3 第三风险：导出格式不稳定

解决方案：

内部统一 JSON，导出器独立测试。

### 14.4 第四风险：自动标注污染训练集

解决方案：

auto 结果必须人工确认后才能导出。

### 14.5 第五风险：数据校验缺失

解决方案：

导出前强校验，错误不清零不允许导出正式数据集。

---

## 15. 最终结论

这个工具的根基不是界面，也不是模型，而是四个东西：

1. 统一中间 JSON。
2. 本地项目目录结构。
3. 独立导出器。
4. 强数据校验。

本地 PaddleOCR 预标注只是辅助能力。不要本末倒置。

第一版只要稳定支持：

```text
PaddleOCR Det
PaddleOCR Rec
PaddleOCR Cls
COCO Layout
```

这个工具就已经有实际生产价值。

