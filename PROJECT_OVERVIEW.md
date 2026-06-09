# PPOCR Workbench 项目结构与功能梳理

整理时间：2026-06-06

本文档基于当前工作区源码静态阅读整理，重点覆盖项目自身代码、主要功能链路、构建运行方式，以及当前能看到的问题和风险点。`PaddleOCR`、`PaddleX`、`third_party`、`model`、`build_*`、`dist` 等目录体量较大，本文按“外部依赖 / 模型 / 构建产物”归类说明，不逐文件展开。

## 一、项目定位

本项目是一个面向 PaddleOCR / PaddleX 的本地离线工作台，当前主实现已经切到 C++ / Qt / QML / QSS / CMake。

核心目标：

- 管理 OCR 标注项目：创建、打开、最近项目、导入图片 / PDF。
- 在 Qt 工作台内完成 OCR 文本框、版面区域、图片级分类标签标注。
- 调用本地 Paddle Inference C++ 后端进行 OCR、分类、版面预标注和预测。
- 导出 PaddleOCR / PaddleX 训练所需数据集。
- 发起 PaddleX 训练 preflight、训练运行、日志解析、版本管理。
- 提供 CLI smoke / 环境检查 / 预测 / 训练辅助工具。

默认入口：

- C++ GUI：`build_vs2026/Release/ppocr_workbench.exe`
- 兼容启动器：`python run_labeler.py`

## 二、根目录结构

```text
paddletools/
  CMakeLists.txt                 C++/Qt 主构建脚本
  CMakePresets.json              VS 2026 Release / GPU Release 构建预设
  README.md                      当前运行、验证、打包说明
  run_labeler.py                 Python 兼容启动器，默认启动 C++ 工作台
  requirements.txt               当前主程序不依赖 Python UI 包，仅保留说明
  recent_projects.json           最近项目记录
  src/                           C++ 主源码
  qml/                           QML 标注画布
  resources/                     Qt 资源和 QSS 主题
  tools/                         C++ 命令行工具 / smoke 工具
  scripts/                       环境、验证、打包、模型下载脚本
  tests/                         C++ 测试
  PaddleOCR/                     本地 PaddleOCR 源码依赖
  PaddleX/                       本地 PaddleX 源码依赖
  model/                         训练权重和推理模型
  third_party/                   Paddle Inference / cpp_infer 依赖
  build_vs2026*/                 CMake / Visual Studio 构建产物
  dist/                          打包输出
  cache/                         模型缓存 / 临时缓存
  training/                      根级训练样例或历史输出
```

## 三、C++ 源码模块

### 1. `src/app`

GUI 主程序。

- `main.cpp`
  - 解析 `--help`、`--project <dir>`、`--last`。
  - 自动解析 base dir：优先 `PPOCR_BASE_DIR`，再看当前目录、exe 目录、构建目录上级、编译期 `PPOCR_PROJECT_ROOT`。
  - 加载 `resources/qss/dark.qss`。

- `MainWindow.h/.cpp`
  - 工作台主窗口，当前约 8300 行，是 UI 和业务编排中心。
  - 包含概览、标注、训练、预测四大页面。
  - 负责项目加载、页面列表、画布联动、标注保存、预标注、导出、环境检查、训练运行、预测运行、训练版本管理等。

### 2. `src/core`

项目数据、标注、导出、校验、训练流程的核心逻辑。

- `ProjectTypes.h`
  - 定义 `PageInfo`、`ProjectContext`、`ValidationIssue`。
  - `ProjectContext` 统一封装项目根目录和常用路径。

- `ProjectRepository.*`
  - 创建 / 打开 / 保存项目。
  - 列出 `assets/pages/page_*.jpg` 页面。
  - 读写 `annotations/page_*.json`。
  - 创建标准目录：`assets`、`annotations`、`crops`、`exports`、`training`、`cache`、`predictions`、`logs`。

- `AssetImporter.*`
  - 导入图片和 PDF。
  - 图片统一转换为 `assets/pages/page_XXXXXX.jpg`。
  - 原始文件复制到 `assets/raw`。
  - 生成缩略图到 `assets/thumbs`。
  - PDF 使用 `QPdfDocument` 渲染为页面图片。
  - 新导入页面按页号分割：每 5 页一页 `val`，其余 `train`。

- `AnnotationOps.*`
  - 新增 OCR 文本框、版面框。
  - 更新 / 删除 region。
  - 设置图片级标签。
  - 清空标注、维护 status、reading order、checked 等字段。

- `OcrPrelabeler.*`
  - 将 OCR 预测结果写入 annotation。
  - 支持 `rec_polys` / `dt_polys`、`rec_texts`、`rec_scores`。
  - 提取文档方向和文本行方向。
  - 自动 OCR region 默认 `checked=false`。

- `ClassificationPrelabeler.*`
  - 将文档方向、文本行方向、表格分类结果写入 `image_labels`。
  - 统一归一化角度标签。

- `LayoutPrelabeler.*`
  - 将版面检测结果写入 layout region。
  - 支持 `coordinate` 或 `bbox` 两种输入。
  - 自动 layout region 默认 `checked=false`。

- `Validator.*`
  - 校验图片存在性、尺寸一致性。
  - 校验 OCR 点位、文本是否为空、越界、面积过小、reading order 重复。
  - 校验 layout bbox、layout label。
  - 校验图片级分类 label 是否在 label set 内。
  - 写入 `logs/validate.log`，并回写每页 annotation 的 `validation` 字段。

- `CropGenerator.*`
  - 根据 OCR 四点框生成识别 crop。
  - 输出到 `crops/rec_train` 或 `crops/rec_val`。
  - 回写 `rec_crops`。

- `Exporter.*`
  - 导出 PaddleOCR / PaddleX 数据集：
    - `exports/ppocr_det`
    - `exports/ppocr_rec`
    - `exports/ppocr_cls`
    - `exports/ppocr_textline_cls`
    - `exports/table_classification`
    - `exports/coco_layout`
  - 默认先校验项目，有 error 时停止导出。
  - 默认只导出 `checked=true` 的 OCR / layout / crop。
  - 写入 `logs/export.log`。

- `TrainingTasks.*`
  - 注册训练任务表。
  - 覆盖 PP-OCRv5 / PP-OCRv4 det / rec，doc/textline/table 分类，PP-DocLayout plus-L/L/M/S/Block。
  - `uvdoc` 作为不可训练兼容项列出。

- `TrainingPreflight.*`
  - 根据任务导出对应数据集。
  - 检查 PaddleX 工作目录、`main.py`、训练 config、Python 路径。
  - 生成 PaddleX 命令和 JSON report。

- `TrainingRunner.*`
  - prepare / finish / runBlocking / simulateSuccess。
  - 用 `QProcess` 运行 PaddleX。
  - 解析日志中的 `loss`、`acc`、`hmean`、`mAP` 等指标。

- `TrainingRunStore.*`
  - 管理 `training/runs.json`。
  - 管理每个 task 的 `training/<task>/versions.json`。
  - 支持 current / best version、删除版本、记录 best weight 和 inference model dir。

- `RuntimePaths.*`、`EnvironmentReport.*`
  - 解析 base dir、PaddleX Python、可执行文件。
  - 构建环境 JSON 报告，覆盖 Qt / OpenCV / Paddle runtime / 模型 / PaddleOCR / PaddleX 等状态。

### 3. `src/paddle`

Paddle 推理和训练命令桥接。

- `PaddleProcess.*`
  - 构造 PaddleX 训练命令。
  - 设置 `PYTHONIOENCODING=utf-8`、`PYTHONUTF8=1`。
  - 设置 `PADDLE_PDX_PADDLEOCR_PATH` 和 `PYTHONPATH`。

- `PaddleInferenceRuntime.*`
  - 解析 Paddle Inference SDK 路径。
  - 支持 `PPOCR_PADDLE_INFERENCE_ROOT`。
  - 检查 SDK / app-local runtime DLL。
  - 检查模型目录是否包含 `inference.pdiparams` 和 `inference.json` / `inference.pdmodel` / `model.json`。
  - 提供 smoke report。

- `PaddleOcrEngine.*`
  - C++ OCR 预测封装。
  - 支持 det / rec 模型、设备、MKLDNN、线程数、可视化输出等配置。

- `PaddleClsEngine.*`
  - 文档方向、文本行方向、表格分类预测封装。

- `PaddleDocLayoutEngine.*`
  - PP-DocLayout 直接走 Paddle Inference C++ 后端。
  - 不再依赖 PaddleX Python 预测桥接。

### 4. `qml` 和 `resources`

- `qml/AnnotationCanvas.qml`
  - QML 标注画布。
  - 支持图片显示、缩放、平移、网格背景。
  - 支持绘制 OCR / layout 框、选择、移动、缩放选中区域、Delete 删除。
  - 与 C++ 通过 signal 传递新增 / 选中 / 移动事件。

- `resources/app.qrc`
  - 将 QML 画布和 QSS 主题打包进 Qt 资源。

- `resources/qss/dark.qss`
  - 深色主题样式。

## 四、命令行工具

`tools/` 下会随 CMake 构建多个 exe：

- `ppocr_env_check.exe`
  - 输出环境检查 JSON。

- `ppocr_infer_smoke.exe`
  - Paddle Inference SDK / 模型可用性 smoke。

- `ppocr_ocr_predict.exe`
  - OCR 预测 CLI。

- `ppocr_cls_predict.exe`
  - 分类预测 CLI。

- `ppocr_layout_predict.exe`
  - 版面预测 CLI。

- `ppocr_training_preflight.exe`
  - 训练前检查和数据集导出。
  - 支持 `--list-tasks`。

- `ppocr_training_run.exe`
  - 发起训练或模拟训练。
  - 管理训练 run / version 记录。

- `ppocr_workflow_smoke.exe`
  - 创建临时项目，导入图片 / PDF，做合成预标注、校验、导出、训练 preflight / simulate。
  - 可用 `--with-ocr` 跑真实 C++ OCR 预标注写回。

## 五、旧 Python 实现退役记录

旧 PySide 标注器已经从当前工作区移除，不再维护双 UI 逻辑。

已清理：

- `ppocr_labeler/`
- `tests/legacy_python/`
- `ppocr_labeler.spec`
- `requirements-legacy-python.txt`

## 六、项目数据结构

一个标注项目大致如下：

```text
project/
  project.json
  assets/
    raw/                  原始导入文件
    pages/                标准页面图 page_XXXXXX.jpg
    thumbs/               页面缩略图
  annotations/
    page_XXXXXX.json      每页一份标注
  crops/
    rec_train/            识别训练 crop
    rec_val/              识别验证 crop
    preview/
  exports/
    ppocr_det/
    ppocr_rec/
    ppocr_cls/
    ppocr_textline_cls/
    table_classification/
    coco_layout/
  training/
    runs.json
    <task>/
      versions.json
      versions/<version_id>/
  cache/
    ocr_prelabel/
    cls_prelabel/
    table_cls_prelabel/
    layout_prelabel/
    prediction_staging/
    prediction_clipboard/
  predictions/
  logs/
    validate.log
    export.log
```

`project.json` 关键内容：

- `project_name`
- `version`
- `offline_mode`
- `image_root`
- `annotation_root`
- `crop_root`
- `export_root`
- `label_sets`
- `splits`
- `local_models`
- `ui_state`

单页 annotation 关键字段：

- `asset_id`
- `image_path`
- `width` / `height`
- `page_index`
- `split`
- `status`
- `image_labels`
- `regions`
- `rec_crops`
- `validation`
- `prelabel`

region 类型：

- OCR 文本：`type=ocr_text`，包含 `points`、`text`、`ignore`、`reading_order`、`source`、`checked`、`confidence`。
- 版面区域：`type=layout`，包含 `bbox`、`label`、`source`、`checked`、`confidence`、`class_id` 等。

## 七、主要功能流程

### 1. 创建 / 打开项目

`ProjectRepository::createProject` 创建目录和 `project.json`。  
`ProjectRepository::openProject` 读取已有项目并补齐缺失目录。

### 2. 导入素材

图片 / PDF 通过 `AssetImporter` 导入：

1. 原始文件复制到 `assets/raw`。
2. 页面统一保存为 `assets/pages/page_XXXXXX.jpg`。
3. 生成 `assets/thumbs/page_XXXXXX.jpg`。
4. 创建 `annotations/page_XXXXXX.json`。
5. 默认 split：页号能被 5 整除为 `val`，否则为 `train`。

### 3. 手工标注

GUI 使用 `QQuickWidget` 加载 `AnnotationCanvas.qml`。

支持：

- 选择 / 平移 / 绘制 OCR 框 / 绘制 layout 框。
- 修改 OCR 文本、reading order、checked、ignore。
- 修改 layout label。
- 设置图片级标签：文档方向、文本行方向、表格分类。
- 撤销 / 重做。
- 当前页或全项目清空标注。
- 当前页生成识别 crops。

### 4. 自动预标注

支持单页和全项目预标注：

- OCR：`PaddleOcrEngine` -> `OcrPrelabeler`
- 文档 / 文本行 / 表格分类：`PaddleClsEngine` -> `ClassificationPrelabeler`
- 版面：`PaddleDocLayoutEngine` -> `LayoutPrelabeler`

自动生成的 region 默认 `checked=false`，导出时如果保持默认 `checkedOnly=true`，需要人工确认后才会进入训练数据。

### 5. 校验

`Validator::validateProject` 会遍历页面并回写每页 validation 状态。  
有 error 时，默认导出 / 训练 preflight 会失败。

### 6. 导出

`Exporter::exportSelected` 支持多任务组合导出：

- Det：`train.txt` / `val.txt`，每行图片路径 + OCR polygon JSON。
- Rec：生成 crop、`rec_gt_train.txt`、`rec_gt_test.txt`、`train.txt`、`val.txt`、`dict.txt`。
- Doc cls / Textline cls / Table cls：`label.txt`、`train.txt`、`val.txt`。
- Layout：COCO 格式 `instance_train.json` / `instance_val.json`。

### 7. 训练

训练任务由 `TrainingTasks` 注册。  
`TrainingPreflight` 会：

1. 根据任务导出对应数据集。
2. 检查 PaddleX / PaddleOCR / config / Python。
3. 生成 PaddleX 命令。
4. 输出 JSON report。

`TrainingRunner` 会：

1. 创建 run 和 version。
2. 通过 `QProcess` 运行 PaddleX。
3. 收集 stdout / stderr。
4. 解析指标。
5. 完成 version，更新 current / best。

### 8. 预测

预测页面和 CLI 支持：

- OCR 预测。
- 分类预测。
- Layout 预测。
- 输入可为文件或目录。
- 输出先进入 staging，再发布到 `predictions` 或用户指定目录。
- 可发布 JSON 和可视化图。

## 八、构建、验证、打包

### 构建

CPU Release：

```powershell
cmake --preset vs2026-release
cmake --build --preset release
```

GPU Release：

```powershell
cmake --preset vs2026-gpu-release
cmake --build --preset release-gpu
```

### 验证

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\verify_cpp_workbench.ps1
```

该脚本覆盖：

- C++ build。
- CTest core tests。
- GUI help / GUI smoke。
- Paddle SDK smoke。
- 环境检查。
- OCR / 分类 / layout / table 分类 smoke。
- 训练 preflight。
- 训练 runner simulate。
- workflow smoke。
- 可选真实 PaddleX 短训练 probe。
- 可选 dist 打包验证。

### 打包

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package_workbench.ps1
```

输出：

```text
dist/ppocr_workbench
```

打包会复制 GUI、CLI 工具、Qt / OpenCV / Paddle runtime DLL，并生成 `run_workbench.bat`。

## 九、当前发现的问题和风险

### 1. `MainWindow.cpp` 职责过重

`src/app/MainWindow.cpp` 当前约 8300 行，`MainWindow.h` 也有大量 UI 状态字段。它同时管理：

- 页面 UI 构建。
- 项目状态。
- 标注保存。
- 预标注线程。
- 导出。
- 训练 preflight / run / metrics / version。
- 预测输入输出。
- 环境检查。

风险：

- 后续维护成本高。
- 改一个页面容易影响其它页面。
- 单元测试难写。

建议拆分：

- `ProjectController`
- `LabelController`
- `TrainingController`
- `PredictionController`
- `RecentProjectStore`
- 独立 UI widget / presenter。

### 2. 源码、依赖、模型、构建产物混在同一根目录

当前根目录包含源码、PaddleOCR/PaddleX 源码、Paddle Inference SDK、模型、build、dist、cache、训练结果、日志等。

风险：

- 目录扫描非常重。
- 误删 / 误提交概率高。
- 很难区分“项目源码”和“可再生成产物”。
- 当前工作区不是 git 仓库，无法看到 `.gitignore` 保护策略。

建议：

- 明确哪些目录应进入版本管理。
- 给 `build_*`、`dist`、`cache`、`training`、日志、模型大文件建立清理 / 忽略策略。
- 如果要纳入 git，先补 `.gitignore`。

### 3. 构建路径强绑定本机环境

`CMakeLists.txt`、`CMakePresets.json`、`scripts/package_workbench.ps1` 中有多处硬编码路径：

- `D:/IDE/Qt/6.10.3/msvc2022_64`
- `D:/IDE/opencv/install_vs2026_cuda89_nvcodec_opengl`
- `D:/IDE/anconda/envs/paddleX-py312/python.exe`
- `D:/IDE/TensorRT/bin`
- `C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.4/bin`

风险：

- 换机器后构建容易失败。
- CI 很难复现。

建议：

- 保留当前 preset 作为本机 preset。
- 增加可移植 preset，例如从环境变量读取 `QT_ROOT`、`OpenCV_DIR`、`PADDLE_LIB`、`PPOCR_PADDLEX_PYTHON`。
- README 里明确最小环境矩阵。

### 4. CMake configure 阶段会下载第三方包

`CMakeLists.txt` 在第三方目录为空时会下载并解压：

- `abseil-cpp`
- `clipper_ver6.4.2`
- `nlohmann`

风险：

- configure 依赖网络。
- 网络失败会导致构建失败。
- 第三方版本和下载源需要锁定校验。

建议：

- 增加 checksum。
- 或把下载步骤移动到独立 bootstrap 脚本。
- 或用明确的 third_party manifest。

### 5. `run_labeler.py` 和 README 的默认构建偏好不完全一致

README 主程序写的是：

```text
build_vs2026/Release/ppocr_workbench.exe
```

但 `run_labeler.py` 候选顺序是：

1. `build_vs2026_gpu/Release/ppocr_workbench.exe`
2. `build_vs2026/Release/ppocr_workbench.exe`
3. `dist/ppocr_workbench/ppocr_workbench.exe`

风险：

- 同时存在 CPU / GPU 构建时，启动器会优先跑 GPU 版。
- 如果用户以为默认是 CPU 版，可能出现 DLL / CUDA / TensorRT 相关问题。

建议：

- README 明确启动器优先 GPU。
- 或提供 `--cpu` / `--gpu` / `PPOCR_WORKBENCH_EXE` 示例。

### 6. 页面扫描只认 `page_*.jpg`

`ProjectRepository::listPages` 只扫描：

```text
assets/pages/page_*.jpg
```

正常导入流程会统一生成 jpg，所以主流程没问题。  
但如果用户手工放入 png / tif / 非标准命名文件，页面列表不会识别。

建议：

- README 或 UI 提示“必须通过导入流程进入项目”。
- 或扩展 listPages 支持项目配置中的多扩展名。

### 7. Split 配置和实际自动分割策略不完全一致

`project.json` 中写了：

```json
"splits": { "train_ratio": 0.8, "val_ratio": 0.2, "strategy": "manual" }
```

实际新导入页面是按页号规则：

```text
index % 5 == 0 -> val
其他 -> train
```

风险：

- 用户看到 0.8 / 0.2 可能以为有可配置随机或比例分割。
- 大量导入后不能自动重平衡。

建议：

- 明确当前是固定页号规则。
- 后续增加“按比例重新划分 train/val”的显式功能。

### 8. 导出函数会清空目标输出目录

`Exporter::resetDir` 会 `removeRecursively` 再重建导出目录。

这是符合“重新导出”的预期行为，但需要注意：

- `exports/ppocr_det/images`
- `exports/ppocr_rec/train`
- `exports/ppocr_rec/test`
- `exports/*/images`

都会被覆盖。

建议：

- UI 上保留明确提示。
- 对用户自定义输出目录时增加保护，避免选择到非项目内重要目录。

### 9. 自动预标注默认不进入 checked-only 导出

自动 OCR / layout region 默认 `checked=false`。  
导出和训练 preflight 默认 `checkedOnly=true`。

优点：

- 防止未审阅的自动结果直接进训练集。

风险：

- 新用户可能一键预标注后直接训练，发现导出样本为 0 或样本过少。

建议：

- UI 在预标注完成后提示“需要确认 checked”。
- 训练 preflight 对 `checkedOnly=true` 且样本为 0 的情况给更明确建议。

### 10. 手工画布以矩形 / 四点框为主

QML 画布绘制 OCR 和 layout 时生成轴对齐矩形四点。  
移动可以整体移动，缩放会回到矩形形态。

风险：

- 对倾斜文本、复杂多边形、精细版面框编辑能力有限。

建议：

- 增加四角独立编辑、旋转框或多边形编辑。
- 对 OCR 检测任务优先支持倾斜四边形。

### 11. 训练指标解析依赖日志正则

`TrainingRunner::parseMetrics` 从 stdout/stderr 文本中用正则抓：

- `hmean`
- `precision`
- `recall`
- `acc`
- `accuracy`
- `score`
- `loss`
- `train_loss`
- `val_loss`
- `lr`
- `mAP`
- `map`

风险：

- PaddleX 日志格式变化时，指标可能抓不到或抓错。
- 多阶段日志中同名指标会被后出现的值覆盖。

建议：

- 优先读取 PaddleX 结构化结果文件。
- 正则解析作为 fallback。

### 12. 测试和验证依赖本地模型 / Windows 图形环境

`verify_cpp_workbench.ps1` 覆盖很全面，但依赖：

- Windows。
- Qt / OpenCV / Paddle runtime。
- 本地模型。
- 图形环境。
- 可选 PaddleX Python 环境。

风险：

- 对日常快速回归较重。
- 新环境首次跑通成本高。

建议：

- 分层验证：
  - core fast tests。
  - CLI smoke。
  - real model smoke。
  - GUI smoke。
  - training probe。
- 在 README 中列出每层耗时和依赖。

### 13. 旧 Python UI 已移除

旧 PySide 实现已经删除，后续维护重点集中在 C++ / Qt 工作台和 C++ 回归测试。

## 十、优先级建议

短期优先：

1. 给根目录补版本管理 / 忽略策略，明确大文件和产物边界。
2. README 补充 GPU / CPU 启动顺序、checked-only 导出行为、split 行为。
3. 把 `MainWindow.cpp` 中训练、预测、标注保存相关逻辑先拆出 controller。
4. 增加更轻量的 fast verification 命令。

中期优先：

1. 环境配置从硬编码路径迁移到环境变量 / toolchain preset。
2. 训练指标改为优先读取结构化结果。
3. 画布增强倾斜四边形和多边形编辑。
4. 对导出目录和版本删除增加更明显的 UI 保护和确认。

长期优先：

1. 建立 CI 或最小可复现构建环境。
2. 整理 PaddleOCR / PaddleX / model / third_party 的获取方式。
3. 补齐 C++ 工作台的 CI 或最小可复现构建环境。
