# PPOCR Workbench

本目录是 PPOCR / PaddleX 本地离线标注、训练和预测工作台。当前主入口已经切到 C++ / Qt / QML / QSS / CMake。

- 主程序：`build_vs2026/Release/ppocr_workbench.exe`
- 兼容启动器：`run_labeler.py` 会构建并启动 C++ 工作台；找不到 exe 时会提示先构建

## VS Code 启动

在 VS Code 里优先使用这些任务：

- `PPOCR: run labeler`：启动 C++ 工作台
- `C++: run workbench`：启动 C++ 工作台
- `C++: verify workbench`：构建并跑默认回归
- `C++: verify packaged workbench`：构建、打包并验证 `dist/ppocr_workbench`
- `C++: workflow smoke`：跑端到端项目、导入、标注、导出、训练模拟 smoke
- `C++: workflow smoke with OCR`：在 workflow smoke 中额外跑真实 C++ OCR 预标注并验证写回
- `C++: environment check`：输出 C++ 工作台环境 JSON 报告

调试配置优先使用：

- `Debug C++ Workbench`
- `Debug C++ Workbench Last Project`

## 命令行启动

```powershell
cmake --preset vs2026-release
cmake --build --preset release
.\build_vs2026\Release\ppocr_workbench.exe
```

打开指定项目：

```powershell
.\build_vs2026\Release\ppocr_workbench.exe --project D:\path\to\project
```

打开最近项目：

```powershell
.\build_vs2026\Release\ppocr_workbench.exe --last
```

## 验证

默认完整验证：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\verify_cpp_workbench.ps1
```

打包验证：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\verify_cpp_workbench.ps1 -Package
```

真实 PaddleX 短训练探针：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\verify_cpp_workbench.ps1 -TrainingProbe -TrainingProbeTimeoutSeconds 120
```

C++ 环境报告：

```powershell
.\build_vs2026\Release\ppocr_env_check.exe --base-dir . --report .\build_vs2026\environment_report.json
```

补齐 table classification 官方推理模型：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\download_table_cls_model.ps1
```

## 当前 C++ 覆盖范围

- 项目创建/打开、最近项目、图片/PDF 导入、缩略图
- OCR/Layout 标注，QML 画布缩放、移动、绘制、选中、移动区域
- 图片级分类标签、Rec crop、校验、清空、撤销/重做
- Det / Rec / Doc cls / Textline cls / Table cls / COCO Layout 导出
- C++ OCR / Cls / Layout 预测和预标注入口
- PP-DocLayout 直接使用 Paddle Inference C++ 后端，不再经过 PaddleX Python 预测桥接
- C++ 环境检查报告，覆盖 Qt/OpenCV/Paddle runtime、PaddleOCR/PaddleX 依赖目录、默认模型和训练 Python 解析结果
- PaddleX 训练 preflight、启动、停止、日志、指标解析、版本 current/best 管理
- 训练任务表覆盖 PP-OCRv5/PP-OCRv4 det/rec、doc/textline/table 分类、PP-DocLayout plus-L/L/M/S/Block；UVDoc 作为不可训练兼容项列出
- `ppocr_workflow_smoke.exe` 端到端验证工具，可用 `--with-ocr` 验证真实 C++ OCR 预标注写回

## 打包

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package_workbench.ps1
```

输出目录：

```text
dist/ppocr_workbench
```

该目录会包含 `ppocr_workbench.exe`、预测/训练/smoke 工具，以及 Qt、OpenCV、Paddle Inference 运行时 DLL。
