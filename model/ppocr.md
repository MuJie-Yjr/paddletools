# paddlelabel/model 模型说明

本文说明 `paddlelabel/model` 目录下各类 PaddleOCR / PaddleX 模型的用途、路径选择方式，以及在本地 `paddlelabel` 标注工具中应该如何配置。

参考官方文档：

- PaddleOCR v3 通用 OCR 产线使用教程：https://www.paddleocr.ai/main/version3.x/pipeline_usage/OCR.html
- 本地代码目录：`D:\model\V-model\PPOCR\PaddleX-3.5.2\PaddleX-3.5.2\paddlelabel\model`

整理日期：2026-05-28

## 1. 目录总览

当前模型根目录：

```text
paddlelabel/model
├── infer/   推理部署模型，给 PaddleOCR 预测、预标注、离线推理使用
└── train/   训练或微调用预训练权重，给训练配置使用，不直接填到标注工具的模型目录里
```

关键区别：

| 目录 | 文件形式 | 主要用途 | 是否直接填到标注工具 |
|---|---|---|---|
| `model/infer` | 解压后的推理模型目录，另有 `.tar` 备份包 | OCR 预测、预标注、离线推理 | 是 |
| `model/train` | `.pdparams` 预训练权重 | 训练、微调、继续训练 | 否 |

标注工具右侧“模型”页里的 `Det 模型目录`、`Rec 模型目录`、`Cls 模型目录` 应该优先填写 `model/infer` 下已经解压的模型目录，而且要填到包含 `inference.yml` / `inference.json` / `inference.pdiparams` 的那一层。

例如不要只填：

```text
paddlelabel\model\infer\PP-OCRv5_server_det_infer
```

更推荐填：

```text
paddlelabel\model\infer\PP-OCRv5_server_det_infer\PP-OCRv5_server_det_infer
```

因为当前这些 `.tar` 解压后大多是“外层目录 + 内层同名目录”的结构，真正的推理文件在内层。

## 2. 官方 OCR 产线对应关系

PaddleOCR v3 通用 OCR 产线通常由 5 个模块组成：

| 模块 | 是否必需 | 作用 | 本地目录中的模型 |
|---|---:|---|---|
| 文档图像方向分类 | 可选 | 判断整张图是 `0/90/180/270` 哪个方向 | `PP-LCNet_x1_0_doc_ori_infer` |
| 文档图像矫正 | 可选 | 处理弯曲、透视、拍摄变形的文档图像 | `UVDoc_infer` |
| 文本行方向分类 | 可选 | 判断单个文本行是否倒置，主要是 `0/180` | `PP-LCNet_x0_25_textline_ori_infer`、`PP-LCNet_x1_0_textline_ori_infer` |
| 文本检测 | 必需 | 在图里找出文本框 | `PP-OCRv4_*_det_infer`、`PP-OCRv5_*_det_infer` |
| 文本识别 | 必需 | 对文本框里的图像内容识别成字符串 | `PP-OCRv4_*_rec_infer`、`PP-OCRv5_*_rec_infer`、`en_PP-OCRv4_mobile_rec_infer` |

在 `paddlelabel` 当前代码里，预标注主要用到：

```python
PaddleOCR(
    text_detection_model_dir=det_model_dir,
    text_recognition_model_dir=rec_model_dir,
    textline_orientation_model_dir=cls_model_dir or None,
    use_doc_orientation_classify=False,
    use_doc_unwarping=False,
    use_textline_orientation=enable_cls,
    device="gpu:0" if use_gpu else "cpu",
)
```

也就是说，当前标注工具的“预标注当前页”只直接使用：

- `Det 模型目录`：文本检测模型。
- `Rec 模型目录`：文本识别模型。
- `Cls 模型目录`：文本行方向分类模型，只有勾选“启用方向分类”时才使用。

当前标注工具没有自动启用文档方向分类模型和 UVDoc 矫正模型。右侧“分类”页里的“文档方向”是人工标注/导出用标签，不等同于预标注时自动调用 `PP-LCNet_x1_0_doc_ori_infer`。

## 3. infer 推理模型清单

### 3.1 文本检测模型 Det

文本检测模型负责从整张图片中找出文字区域，输出文本框坐标。它不识别文字内容，只负责“哪里有字”。

| 模型目录 | 官方模型名 | 特点 | 推荐场景 |
|---|---|---|---|
| `PP-OCRv5_server_det_infer` | `PP-OCRv5_server_det` | PP-OCRv5 服务端检测模型，精度优先 | 服务器、GPU、批量高质量预标注 |
| `PP-OCRv5_mobile_det_infer` | `PP-OCRv5_mobile_det` | PP-OCRv5 轻量检测模型，速度和体积优先 | CPU、普通笔记本、快速预标注 |
| `PP-OCRv4_server_det_infer` | `PP-OCRv4_server_det` | PP-OCRv4 服务端检测模型，旧版稳定 | 和 v4 识别模型搭配、兼容旧流程 |
| `PP-OCRv4_mobile_det_infer` | `PP-OCRv4_mobile_det` | PP-OCRv4 轻量检测模型 | 低资源环境、快速测试 |

建议：

- 新项目优先试 `PP-OCRv5_server_det_infer`。
- 如果 CPU 太慢，换 `PP-OCRv5_mobile_det_infer`。
- 如果你后续训练或评估基于 PP-OCRv4，检测和识别都用 v4 系列更容易保持一致。

标注工具填写示例：

```text
Det 模型目录：
D:\model\V-model\PPOCR\PaddleX-3.5.2\PaddleX-3.5.2\paddlelabel\model\infer\PP-OCRv5_server_det_infer\PP-OCRv5_server_det_infer
```

### 3.2 文本识别模型 Rec

文本识别模型负责把检测框内的图像识别成文字。它通常和 Det 模型配套使用。

| 模型目录 | 官方模型名 | 特点 | 推荐场景 |
|---|---|---|---|
| `PP-OCRv5_server_rec_infer` | `PP-OCRv5_server_rec` | PP-OCRv5 服务端识别模型，兼顾中文、英文、日文、繁体、复杂字符 | 综合文档、中文票据、质量优先 |
| `PP-OCRv5_mobile_rec_infer` | `PP-OCRv5_mobile_rec` | PP-OCRv5 轻量识别模型 | CPU 快速预标注、体积敏感 |
| `PP-OCRv4_server_rec_doc_infer` | `PP-OCRv4_server_rec_doc` | 文档场景增强，支持更多中文、繁体、日文、特殊符号 | 中文文档、表单、票据、复杂符号 |
| `PP-OCRv4_server_rec_infer` | `PP-OCRv4_server_rec` | PP-OCRv4 服务端识别模型 | 旧版通用中文场景 |
| `PP-OCRv4_mobile_rec_infer` | `PP-OCRv4_mobile_rec` | PP-OCRv4 轻量识别模型 | 低资源环境 |
| `en_PP-OCRv4_mobile_rec_infer` | `en_PP-OCRv4_mobile_rec` | 英文和数字轻量识别模型 | 纯英文、数字、编号、码号 |

建议：

- 综合中文文档：优先 `PP-OCRv5_server_rec_infer`。
- 文档、票据、复杂符号较多：可以对比 `PP-OCRv4_server_rec_doc_infer`。
- 纯英文或纯数字编号：可试 `en_PP-OCRv4_mobile_rec_infer`，速度更轻。
- 不建议把中文场景长期固定成 `en_PP-OCRv4_mobile_rec_infer`，它主要面向英文和数字，对中文能力有限。

标注工具填写示例：

```text
Rec 模型目录：
D:\model\V-model\PPOCR\PaddleX-3.5.2\PaddleX-3.5.2\paddlelabel\model\infer\PP-OCRv5_server_rec_infer\PP-OCRv5_server_rec_infer
```

### 3.3 文本行方向分类模型 Cls

文本行方向分类模型用于判断文本行是否倒置。这里的“文本行方向”主要是 `0/180`，不是 `90/270` 竖排方向分类。

| 模型目录 | 官方模型名 | 特点 | 推荐场景 |
|---|---|---|---|
| `PP-LCNet_x0_25_textline_ori_infer` | `PP-LCNet_x0_25_textline_ori` | 极轻量，速度快 | CPU 快速预标注，普通倒置文本判断 |
| `PP-LCNet_x1_0_textline_ori_infer` | `PP-LCNet_x1_0_textline_ori` | 更大一些，精度更高 | 对倒置文本判断更敏感的场景 |

使用建议：

- 如果图片中文字方向基本正常，可以不启用。
- 如果经常出现上下颠倒的单行文字，勾选“启用方向分类”，并填写 `Cls 模型目录`。
- 侧着的竖向文字，例如整体旋转 90 度的数字串，不应简单理解为文本行方向 `90`。当前文本行方向分类只处理 `0/180`。

标注工具填写示例：

```text
Cls 模型目录：
D:\model\V-model\PPOCR\PaddleX-3.5.2\PaddleX-3.5.2\paddlelabel\model\infer\PP-LCNet_x1_0_textline_ori_infer\PP-LCNet_x1_0_textline_ori_infer
```

### 3.4 文档方向分类模型 Doc Orientation

| 模型目录 | 官方模型名 | 作用 |
|---|---|---|
| `PP-LCNet_x1_0_doc_ori_infer` | `PP-LCNet_x1_0_doc_ori` | 判断整张文档图片的方向，类别是 `0/90/180/270` |

注意：

- 这个模型判断的是整张图是否需要旋转，不是某个文字框是否侧着。
- 在 `paddlelabel` 当前工具中，预标注没有自动调用这个模型。
- 右侧“分类标注”里的“文档方向”可以人工填写，用于后续导出分类数据或人工记录。

人工标注时的理解：

```text
文档方向：整张图要不要旋转
文本行方向：文字行是不是上下颠倒，只管 0/180
```

### 3.5 文档图像矫正模型 UVDoc

| 模型目录 | 官方模型名 | 作用 |
|---|---|---|
| `UVDoc_infer` | `UVDoc` | 对拍摄造成的弯曲、透视变形、页面扭曲进行矫正 |

注意：

- UVDoc 是 OCR 前置预处理模型，不是 Det/Rec/Cls 模型。
- `paddlelabel` 当前预标注逻辑中 `use_doc_unwarping=False`，所以不会自动使用它。
- 如果后续要做拍照文档矫正，可以在独立 OCR pipeline 或后续代码里开启 `use_doc_unwarping=True` 并指定 `doc_unwarping_model_dir`。

### 3.6 文档版面检测模型 Layout

当前目录中有多种版面检测相关模型：

| 模型目录 | 作用倾向 |
|---|---|
| `PP-DocBlockLayout_infer` | 文档块级版面检测 |
| `PP-DocLayout-S_infer` | 小型版面检测模型，速度优先 |
| `PP-DocLayout-M_infer` | 中型版面检测模型，速度和精度折中 |
| `PP-DocLayout-L_infer` | 大型版面检测模型，精度优先 |
| `PP-DocLayout_plus-L_infer` | 增强大型版面检测模型，适合更复杂版面 |

注意：

- 这些模型不属于通用 OCR pipeline 的 Det/Rec/Cls 三件套。
- 它们更接近 PP-Structure / 版面分析 / 文档解析流程，用来识别标题、正文、表格、图片、公式等区域。
- 当前 `paddlelabel` 里的“版面框”是人工 COCO layout 标注，暂时没有接入这些 layout 模型做自动版面预标注。

如果后续要扩展自动版面预标注，可以把这些模型接到 PaddleX 或 PP-StructureV3 的 layout detection pipeline，再把预测 bbox 写入 annotation 的 `layout` region。

## 4. train 训练权重清单

`model/train` 下是 `.pdparams` 文件，属于训练或微调阶段的预训练权重，不是 PaddleOCR 推理目录。

| 文件 | 对应模型 | 用途 |
|---|---|---|
| `PP-OCRv5_server_det_pretrained.pdparams` | `PP-OCRv5_server_det` | 检测模型训练/微调初始化 |
| `PP-OCRv5_mobile_det_pretrained.pdparams` | `PP-OCRv5_mobile_det` | 轻量检测模型训练/微调初始化 |
| `PP-OCRv4_server_det_pretrained.pdparams` | `PP-OCRv4_server_det` | v4 服务端检测训练/微调初始化 |
| `PP-OCRv4_mobile_det_pretrained.pdparams` | `PP-OCRv4_mobile_det` | v4 轻量检测训练/微调初始化 |
| `PP-OCRv5_server_rec_pretrained.pdparams` | `PP-OCRv5_server_rec` | 识别模型训练/微调初始化 |
| `PP-OCRv5_mobile_rec_pretrained.pdparams` | `PP-OCRv5_mobile_rec` | 轻量识别模型训练/微调初始化 |
| `PP-OCRv4_server_rec_doc_pretrained.pdparams` | `PP-OCRv4_server_rec_doc` | 文档增强识别模型训练/微调初始化 |
| `PP-OCRv4_server_rec_pretrained.pdparams` | `PP-OCRv4_server_rec` | v4 服务端识别训练/微调初始化 |
| `PP-OCRv4_mobile_rec_pretrained.pdparams` | `PP-OCRv4_mobile_rec` | v4 轻量识别训练/微调初始化 |
| `en_PP-OCRv4_mobile_rec_pretrained.pdparams` | `en_PP-OCRv4_mobile_rec` | 英文/数字识别训练/微调初始化 |
| `PP-LCNet_x1_0_doc_ori_pretrained.pdparams` | `PP-LCNet_x1_0_doc_ori` | 文档方向分类训练/微调初始化 |
| `PP-LCNet_x1_0_textline_ori_pretrained.pdparams` | `PP-LCNet_x1_0_textline_ori` | 文本行方向分类训练/微调初始化 |
| `PP-LCNet_x0_25_textline_ori_pretrained.pdparams` | `PP-LCNet_x0_25_textline_ori` | 轻量文本行方向分类训练/微调初始化 |
| `UVDoc_pretrained.pdparams` | `UVDoc` | 文档图像矫正相关训练权重 |
| `PP-DocLayout-*.pdparams`、`PP-DocBlockLayout_pretrained.pdparams` | Layout 系列 | 版面检测训练/微调初始化 |

不要把 `.pdparams` 文件填到标注工具的 `Det 模型目录`、`Rec 模型目录` 或 `Cls 模型目录` 里。标注工具需要的是推理模型目录，不是训练权重文件。

## 5. 推荐模型组合

### 5.1 精度优先，适合服务器或 GPU

```text
Det:
paddlelabel\model\infer\PP-OCRv5_server_det_infer\PP-OCRv5_server_det_infer

Rec:
paddlelabel\model\infer\PP-OCRv5_server_rec_infer\PP-OCRv5_server_rec_infer

Cls:
paddlelabel\model\infer\PP-LCNet_x1_0_textline_ori_infer\PP-LCNet_x1_0_textline_ori_infer
```

适合：

- 中文、英文、日文、繁体混合文档。
- 票据、表单、合同、扫描件。
- 预标注质量优先，能接受更慢速度。

### 5.2 速度优先，适合 CPU 快速跑

```text
Det:
paddlelabel\model\infer\PP-OCRv5_mobile_det_infer\PP-OCRv5_mobile_det_infer

Rec:
paddlelabel\model\infer\PP-OCRv5_mobile_rec_infer\PP-OCRv5_mobile_rec_infer

Cls:
paddlelabel\model\infer\PP-LCNet_x0_25_textline_ori_infer\PP-LCNet_x0_25_textline_ori_infer
```

适合：

- 标注前快速生成初稿。
- CPU 环境。
- 大批量图片先粗预标注，再人工修正。

### 5.3 PP-OCRv4 文档场景

```text
Det:
paddlelabel\model\infer\PP-OCRv4_server_det_infer\PP-OCRv4_server_det_infer

Rec:
paddlelabel\model\infer\PP-OCRv4_server_rec_doc_infer\PP-OCRv4_server_rec_doc_infer

Cls:
paddlelabel\model\infer\PP-LCNet_x1_0_textline_ori_infer\PP-LCNet_x1_0_textline_ori_infer
```

适合：

- 已经基于 PP-OCRv4 做过训练或评估。
- 中文文档、繁体字、日文字符、特殊符号较多。
- 想对比 v4 doc 模型和 v5 server 模型的识别差异。

### 5.4 英文和数字编号场景

```text
Det:
paddlelabel\model\infer\PP-OCRv5_mobile_det_infer\PP-OCRv5_mobile_det_infer

Rec:
paddlelabel\model\infer\en_PP-OCRv4_mobile_rec_infer\en_PP-OCRv4_mobile_rec_infer

Cls:
可留空，或使用 PP-LCNet_x0_25_textline_ori_infer
```

适合：

- 纯英文。
- 数字串、编号、序列号。
- 对中文识别没有要求。

## 6. 在 PPOCR Workbench 标注工具中的填写方式

启动工具：

```powershell
cmake --build --preset release
.\build_vs2026\Release\ppocr_workbench.exe
```

VS Code 中也可以直接运行任务 `PPOCR: run labeler`，该任务会启动 C++/Qt 工作台。

打开项目后，右侧切到“模型”页：

1. `Det 模型目录` 填文本检测推理模型目录。
2. `Rec 模型目录` 填文本识别推理模型目录。
3. `Cls 模型目录` 填文本行方向分类推理模型目录。
4. 如果需要检测倒置文本，勾选“启用方向分类”。
5. 如果用 GPU，勾选“使用 GPU”，否则保持 CPU。
6. 点击“检查模型”。
7. 点击“预标注当前页”。

当前工具只检查目录是否存在，不能完全检查模型内部文件是否匹配。填写路径时请确认该目录下至少有这些文件：

```text
inference.yml
inference.json
inference.pdiparams
```

部分模型可能还会包含其他配置或字典文件，以模型目录实际内容为准。

## 7. Python 离线调用示例

如果想绕过 UI 直接测试本地模型，可以用类似下面的脚本：

```python
from paddleocr import PaddleOCR

base = r"D:\model\V-model\PPOCR\PaddleX-3.5.2\PaddleX-3.5.2\paddlelabel\model\infer"

ocr = PaddleOCR(
    text_detection_model_dir=base + r"\PP-OCRv5_server_det_infer\PP-OCRv5_server_det_infer",
    text_recognition_model_dir=base + r"\PP-OCRv5_server_rec_infer\PP-OCRv5_server_rec_infer",
    textline_orientation_model_dir=base + r"\PP-LCNet_x1_0_textline_ori_infer\PP-LCNet_x1_0_textline_ori_infer",
    use_doc_orientation_classify=False,
    use_doc_unwarping=False,
    use_textline_orientation=True,
    device="cpu",
)

result = ocr.predict(r"path\to\image.jpg")
for item in result:
    item.print()
```

如果不想启用文本行方向分类：

```python
ocr = PaddleOCR(
    text_detection_model_dir=base + r"\PP-OCRv5_server_det_infer\PP-OCRv5_server_det_infer",
    text_recognition_model_dir=base + r"\PP-OCRv5_server_rec_infer\PP-OCRv5_server_rec_infer",
    use_doc_orientation_classify=False,
    use_doc_unwarping=False,
    use_textline_orientation=False,
    device="cpu",
)
```

## 8. CLI 离线调用示例

官方文档支持通过参数指定本地模型目录。命令行示例：

```powershell
paddleocr ocr `
  -i .\demo.jpg `
  --text_detection_model_dir .\paddlelabel\model\infer\PP-OCRv5_server_det_infer\PP-OCRv5_server_det_infer `
  --text_recognition_model_dir .\paddlelabel\model\infer\PP-OCRv5_server_rec_infer\PP-OCRv5_server_rec_infer `
  --textline_orientation_model_dir .\paddlelabel\model\infer\PP-LCNet_x1_0_textline_ori_infer\PP-LCNet_x1_0_textline_ori_infer `
  --use_doc_orientation_classify false `
  --use_doc_unwarping false `
  --use_textline_orientation true `
  --device cpu
```

如果要避免联网下载模型，尽量显式传入本地模型目录，并关闭不使用的可选模块：

```text
use_doc_orientation_classify = false
use_doc_unwarping = false
use_textline_orientation = false 或 true
```

只要某个模块开启但没有指定本地模型路径，PaddleOCR 就可能尝试使用默认模型或下载官方模型。

## 9. 标注概念和模型概念的区别

### 9.1 文档方向

文档方向是整张图片的方向。

例子：

- 页面正着，不需要旋转：`0`
- 整张页面顺时针旋转 90 度才能正常阅读：`90`
- 整张页面倒置：`180`
- 整张页面逆时针旋转 90 度才能正常阅读：`270`

它对应的是 `PP-LCNet_x1_0_doc_ori` 这类文档方向分类模型。

### 9.2 文本行方向

文本行方向是单个文本行是否倒置。

当前 PaddleOCR 文本行方向分类主要是：

```text
0
180
```

它不是用来标 `90/270` 侧排文字的。比如一串竖着显示的数字，如果整张图片没有旋转，文档方向仍然可以是 `0`；文本内容按人能读出的顺序填写即可。

### 9.3 OCR 框文本

OCR 框的文本内容应该填写最终希望训练识别模型输出的字符串。

例如图中侧着的数字串，人读出来是：

```text
879676037857
```

则 OCR 文本就填这个字符串，而不是按屏幕从上到下或从下到上机械拆分。

## 10. 常见问题

### 10.1 为什么模型目录存在，但预标注失败？

优先检查是否填到了内层目录。

正确目录通常长这样：

```text
...\PP-OCRv5_server_det_infer\PP-OCRv5_server_det_infer
```

并且里面能看到：

```text
inference.yml
inference.json
inference.pdiparams
```

如果只填外层：

```text
...\PP-OCRv5_server_det_infer
```

工具的“检查模型”可能显示目录存在，但 PaddleOCR 真正加载时可能找不到推理文件。

### 10.2 Det 和 Rec 可以混用 v4/v5 吗？

技术上可以尝试，但不建议长期混用。推荐组合：

- v5 det + v5 rec
- v4 det + v4 rec
- v4 det + v4 server rec doc

如果只是临时对比效果，可以混用测试；如果要做稳定数据生产，尽量保持版本一致。

### 10.3 `.tar` 文件要不要删除？

`.tar` 是下载或缓存的压缩包。只要解压后的目录完整，推理时不需要 `.tar`。

保留 `.tar` 的好处：

- 可以重新解压。
- 方便核对模型来源。

删除 `.tar` 的好处：

- 节省磁盘空间。

### 10.4 `train` 下的 `.pdparams` 能不能用于预标注？

不能直接用于当前标注工具预标注。

`.pdparams` 是训练权重文件，通常要配合训练配置、网络结构和导出流程。训练完成并导出 inference 模型后，才适合填到 `Det 模型目录`、`Rec 模型目录` 或 `Cls 模型目录`。

### 10.5 当前工具怎样保持离线？

建议：

1. 使用 `conda activate paddleX-py312`。
2. 确认 `paddlelabel/PaddleOCR` 和 `paddlelabel/PaddleX` 是本地源码。
3. 预标注时显式填写本地 Det、Rec、Cls 目录。
4. 关闭不需要的可选模块。
5. 不要只写模型名称，例如 `PP-OCRv5_server_rec`，而要写本地模型目录。

当前 `run_labeler.py` 是兼容启动器：默认构建并启动 C++/Qt `ppocr_workbench.exe`，找不到 C++ exe 时会提示先构建。
