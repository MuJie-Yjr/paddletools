param(
    [string]$BuildDir = "build_vs2026",
    [string]$Configuration = "Release",
    [switch]$Package,
    [switch]$TrainingProbe,
    [int]$TrainingProbeTimeoutSeconds = 120
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$buildPath = Join-Path $root $BuildDir
$binPath = Join-Path $buildPath $Configuration
$distPath = Join-Path $root "dist\ppocr_workbench"
$runId = [guid]::NewGuid().ToString("N").Substring(0, 8)
$verifyRoot = Join-Path $buildPath "verify_$runId"
New-Item -ItemType Directory -Force -Path $verifyRoot | Out-Null

function Step($message) {
    Write-Host "[verify] $message"
}

function Require-File($path) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "missing file: $path"
    }
}

function Run-Exe($exe, [string[]]$arguments = @(), $workingDirectory = $root, [int[]]$okExitCodes = @(0)) {
    Require-File $exe
    Push-Location $workingDirectory
    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = & $exe @arguments 2>&1
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
        Pop-Location
    }
    if ($okExitCodes -notcontains $exitCode) {
        $text = $output -join "`n"
        throw "command failed ($exitCode): $exe $($arguments -join ' ')`n$text"
    }
    return $output -join "`n"
}

function Smoke-Gui($exe, $workingDirectory, [string[]]$arguments = @()) {
    Require-File $exe
    $logStem = "verify_gui_{0}" -f ([guid]::NewGuid().ToString("N"))
    $stdout = Join-Path $workingDirectory "$logStem.stdout.log"
    $stderr = Join-Path $workingDirectory "$logStem.stderr.log"
    Remove-Item -LiteralPath $stdout,$stderr -ErrorAction SilentlyContinue
    $startOptions = @{
        FilePath = $exe
        WorkingDirectory = $workingDirectory
        WindowStyle = "Hidden"
        RedirectStandardOutput = $stdout
        RedirectStandardError = $stderr
        PassThru = $true
    }
    if ($arguments.Count -gt 0) {
        $startOptions.ArgumentList = $arguments
    }
    $process = Start-Process @startOptions
    Start-Sleep -Seconds 5
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
    } elseif ($process.ExitCode -ne 0) {
        throw "GUI exited early with code $($process.ExitCode)"
    }
    $stderrText = Get-Content -LiteralPath $stderr -Raw -ErrorAction SilentlyContinue
    if (-not [string]::IsNullOrWhiteSpace($stderrText)) {
        throw "GUI smoke stderr is not empty:`n$stderrText"
    }
}

function New-TrainingPreflightProject($projectPath) {
    Remove-Item -LiteralPath $projectPath -Recurse -Force -ErrorAction SilentlyContinue
    foreach ($rel in @(
        "assets\raw",
        "assets\pages",
        "assets\thumbs",
        "annotations",
        "crops\rec_train",
        "crops\rec_val",
        "crops\preview",
        "exports",
        "training",
        "logs"
    )) {
        New-Item -ItemType Directory -Force -Path (Join-Path $projectPath $rel) | Out-Null
    }

    Add-Type -AssemblyName System.Drawing
    $imagePath = Join-Path $projectPath "assets\pages\page_000001.jpg"
    $bitmap = New-Object System.Drawing.Bitmap 320, 220
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $pen = $null
    $font = $null
    try {
        $graphics.Clear([System.Drawing.Color]::White)
        $pen = New-Object System.Drawing.Pen -ArgumentList ([System.Drawing.Color]::Black), 2
        $font = New-Object System.Drawing.Font -ArgumentList "Arial", 18
        $graphics.DrawRectangle($pen, 20, 24, 220, 48)
        $graphics.DrawString("VERIFY2026", $font, [System.Drawing.Brushes]::Black, 28, 34)
    } finally {
        if ($pen) {
            $pen.Dispose()
        }
        if ($font) {
            $font.Dispose()
        }
        $graphics.Dispose()
        $bitmap.Save($imagePath, [System.Drawing.Imaging.ImageFormat]::Jpeg)
        $bitmap.Dispose()
    }

    $project = [ordered]@{
        project_name = "verify_training_preflight"
        version = "1.0"
        offline_mode = $true
        created_at = "2026-06-04 00:00:00"
        image_root = "assets/pages"
        annotation_root = "annotations"
        crop_root = "crops"
        export_root = "exports"
        label_sets = [ordered]@{
            doc_orientation = @("0", "90", "180", "270")
            textline_orientation = @("0", "180")
            layout = @("title", "text", "table", "image", "formula", "seal")
            table_classification = @("wired_table", "wireless_table")
            table_type = @("wired_table", "wireless_table")
        }
        splits = [ordered]@{
            train_ratio = 0.8
            val_ratio = 0.2
            strategy = "manual"
        }
        local_models = [ordered]@{
            det_model_dir = ""
            rec_model_dir = ""
            cls_model_dir = ""
            doc_orientation_model_dir = ""
            doc_unwarping_model_dir = ""
            layout_model_dir = ""
            use_gpu = $false
            enable_cls = $false
            enable_doc_orientation = $false
            enable_doc_unwarping = $false
        }
        ui_state = [ordered]@{}
    }
    $project | ConvertTo-Json -Depth 16 | Set-Content -Path (Join-Path $projectPath "project.json") -Encoding UTF8

    $annotation = [ordered]@{
        asset_id = "page_000001"
        image_path = "assets/pages/page_000001.jpg"
        width = 320
        height = 220
        page_index = 1
        split = "train"
        status = "labeled"
        image_labels = @(
            [ordered]@{ task = "doc_orientation"; label = "0" },
            [ordered]@{ task = "textline_orientation"; label = "0" },
            [ordered]@{ task = "table_classification"; label = "wired_table" }
        )
        regions = @(
            [ordered]@{
                id = "r_001"
                type = "ocr_text"
                source = "manual"
                text = "VERIFY2026"
                checked = $true
                ignore = $false
                reading_order = 1
                points = @(
                    @(20, 24),
                    @(240, 24),
                    @(240, 72),
                    @(20, 72)
                )
            },
            [ordered]@{
                id = "layout_001"
                type = "layout"
                label = "text"
                source = "manual"
                checked = $true
                bbox = @(18, 20, 230, 60)
            }
        )
        rec_crops = @()
        validation = [ordered]@{
            passed = $false
            errors = @()
            warnings = @()
        }
    }
    $annotation | ConvertTo-Json -Depth 16 | Set-Content -Path (Join-Path $projectPath "annotations\page_000001.json") -Encoding UTF8
}

Step "build"
cmake --build --preset release

Step "core tests"
ctest --test-dir $buildPath -C $Configuration --output-on-failure

Step "CLI help"
Run-Exe (Join-Path $binPath "ppocr_workbench.exe") @("--help") | Out-Null
$pythonCommand = Get-Command python -ErrorAction SilentlyContinue
if ($pythonCommand -and (Test-Path -LiteralPath $pythonCommand.Source -PathType Leaf)) {
    $previousWorkbenchExe = $env:PPOCR_WORKBENCH_EXE
    try {
        $env:PPOCR_WORKBENCH_EXE = Join-Path $binPath "ppocr_workbench.exe"
        $launcherHelp = Run-Exe $pythonCommand.Source @((Join-Path $root "run_labeler.py"), "--help")
        if ($launcherHelp -notmatch "ppocr_workbench.exe") {
            throw "run_labeler.py did not delegate --help to the C++ workbench"
        }
        $env:PPOCR_WORKBENCH_EXE = Join-Path $buildPath "missing_ppocr_workbench.exe"
        $launcherMissing = Run-Exe $pythonCommand.Source @((Join-Path $root "run_labeler.py"), "--help") $root @(127)
        if ($launcherMissing -notmatch "C\+\+ PPOCR Workbench executable was not found") {
            throw "run_labeler.py did not report a missing C++ workbench"
        }
    } finally {
        $env:PPOCR_WORKBENCH_EXE = $previousWorkbenchExe
    }
}
Run-Exe (Join-Path $binPath "ppocr_ocr_predict.exe") @("--help") | Out-Null
Run-Exe (Join-Path $binPath "ppocr_cls_predict.exe") @("--help") | Out-Null
Run-Exe (Join-Path $binPath "ppocr_layout_predict.exe") @("--help") | Out-Null
Run-Exe (Join-Path $binPath "ppocr_training_preflight.exe") @("--help") | Out-Null
Run-Exe (Join-Path $binPath "ppocr_training_preflight.exe") @("--list-tasks") | Out-Null
Run-Exe (Join-Path $binPath "ppocr_training_run.exe") @("--help") | Out-Null
Run-Exe (Join-Path $binPath "ppocr_training_run.exe") @("--list-tasks") | Out-Null
Run-Exe (Join-Path $binPath "ppocr_workflow_smoke.exe") @("--help") | Out-Null
Run-Exe (Join-Path $binPath "ppocr_env_check.exe") @("--help") | Out-Null

Step "Paddle SDK smoke"
Run-Exe (Join-Path $binPath "ppocr_infer_smoke.exe") @(
    (Join-Path $root "model\infer\PP-OCRv5_server_det_infer")
) | Out-Null

Step "environment check"
$envReport = Join-Path $verifyRoot "environment_report.json"
$envCheckResult = Run-Exe (Join-Path $binPath "ppocr_env_check.exe") @(
    "--base-dir", $root,
    "--report", $envReport
)
$envCheckJson = $envCheckResult | ConvertFrom-Json
if (-not $envCheckJson.ok) {
    throw "environment check failed`n$envCheckResult"
}
Require-File $envReport

Step "OCR prediction smoke"
$ocrOut = Join-Path $verifyRoot "ocr"
Remove-Item -LiteralPath $ocrOut -Recurse -Force -ErrorAction SilentlyContinue
$ocrResult = Run-Exe (Join-Path $binPath "ppocr_ocr_predict.exe") @(
    "--input", (Join-Path $root "PaddleOCR\deploy\lite\imgs\lite_demo.png"),
    "--output", $ocrOut,
    "--det-model-dir", (Join-Path $root "model\infer\PP-OCRv5_server_det_infer\PP-OCRv5_server_det_infer"),
    "--rec-model-dir", (Join-Path $root "model\infer\PP-OCRv5_server_rec_infer\PP-OCRv5_server_rec_infer"),
    "--device", "cpu",
    "--no-visual"
)
if ($ocrResult -notmatch '"ok"\s*:\s*true') {
    throw "OCR prediction smoke did not return ok=true"
}

Step "classification smoke"
$clsOut = Join-Path $verifyRoot "cls_textline"
Remove-Item -LiteralPath $clsOut -Recurse -Force -ErrorAction SilentlyContinue
Run-Exe (Join-Path $binPath "ppocr_cls_predict.exe") @(
    "--task", "textline_orientation",
    "--input", (Join-Path $root "PaddleOCR\deploy\lite\imgs\lite_demo.png"),
    "--output", $clsOut,
    "--model-dir", (Join-Path $root "model\infer\PP-LCNet_x1_0_textline_ori_infer"),
    "--device", "cpu",
    "--no-visual"
) | Out-Null

Step "layout smoke"
$layoutOut = Join-Path $verifyRoot "layout"
Remove-Item -LiteralPath $layoutOut -Recurse -Force -ErrorAction SilentlyContinue
Run-Exe (Join-Path $binPath "ppocr_layout_predict.exe") @(
    "--base-dir", $root,
    "--input", (Join-Path $root "PaddleOCR\deploy\lite\imgs\lite_demo.png"),
    "--output", $layoutOut,
    "--model-dir", (Join-Path $root "model\infer\PP-DocLayout-S_infer"),
    "--device", "cpu",
    "--no-visual"
) | Out-Null

Step "table classification smoke"
$tableOut = Join-Path $verifyRoot "table_missing"
Remove-Item -LiteralPath $tableOut -Recurse -Force -ErrorAction SilentlyContinue
$tableModelRoot = Join-Path $root "model\infer\PP-LCNet_x1_0_table_cls_infer"
$tableModelNested = Join-Path $tableModelRoot "PP-LCNet_x1_0_table_cls_infer"
$tableModelAvailable = (Test-Path -LiteralPath (Join-Path $tableModelRoot "inference.pdiparams") -PathType Leaf) -or
    (Test-Path -LiteralPath (Join-Path $tableModelNested "inference.pdiparams") -PathType Leaf)
if ($tableModelAvailable) {
    $tableResult = Run-Exe (Join-Path $binPath "ppocr_cls_predict.exe") @(
        "--task", "table_classification",
        "--input", (Join-Path $root "PaddleOCR\deploy\lite\imgs\lite_demo.png"),
        "--output", $tableOut,
        "--model-dir", $tableModelRoot,
        "--device", "cpu",
        "--no-visual"
    )
    if ($tableResult -notmatch '"ok"\s*:\s*true' -or $tableResult -notmatch 'wired_table|wireless_table') {
        throw "table classification smoke did not return a table label"
    }
} else {
    $tableResult = Run-Exe (Join-Path $binPath "ppocr_cls_predict.exe") @(
        "--task", "table_classification",
        "--input", (Join-Path $root "PaddleOCR\deploy\lite\imgs\lite_demo.png"),
        "--output", $tableOut,
        "--device", "cpu",
        "--no-visual"
    ) $root @(2)
    if ($tableResult -notmatch "classification model is not usable") {
        throw "table missing-model smoke did not return the expected structured error"
    }
}

Step "training preflight smoke"
$preflightProject = Join-Path $verifyRoot "training_preflight_project"
New-TrainingPreflightProject $preflightProject
foreach ($taskKey in @(
    "det_v5_server",
    "det_v5_mobile",
    "det_v4_server",
    "det_v4_mobile",
    "rec_v5_server",
    "rec_v5_mobile",
    "rec_v4_server_doc",
    "rec_v4_server",
    "rec_v4_mobile",
    "rec_v4_mobile_en",
    "doc_ori_x1",
    "textline_ori_x1",
    "textline_ori_x025",
    "table_cls_x1",
    "layout_plus_l",
    "layout_l",
    "layout_m",
    "layout_s",
    "layout_block"
)) {
    $preflightReport = Join-Path $verifyRoot "training_preflight_$taskKey.json"
    $preflightResult = Run-Exe (Join-Path $binPath "ppocr_training_preflight.exe") @(
        "--base-dir", $root,
        "--project", $preflightProject,
        "--task", $taskKey,
        "--python", "python",
        "--device", "cpu",
        "--epochs", "1",
        "--batch-size", "1",
        "--learning-rate", "0.001",
        "--report", $preflightReport
    )
    $preflightJson = $preflightResult | ConvertFrom-Json
    if (-not $preflightJson.ok) {
        throw "training preflight smoke failed for $taskKey`n$preflightResult"
    }
    if ($preflightJson.sample_count -le 0) {
        throw "training preflight smoke exported no samples for $taskKey"
    }
    Require-File $preflightReport
}
Require-File (Join-Path $preflightProject "exports\ppocr_det\train.txt")
Require-File (Join-Path $preflightProject "exports\ppocr_rec\train.txt")
Require-File (Join-Path $preflightProject "exports\coco_layout\annotations\instance_train.json")

Step "training runner simulate smoke"
$trainingRunReport = Join-Path $verifyRoot "training_run_simulate.json"
$trainingRunLog = Join-Path $verifyRoot "training_run_simulate.log"
$trainingRunResult = Run-Exe (Join-Path $binPath "ppocr_training_run.exe") @(
    "--base-dir", $root,
    "--project", $preflightProject,
    "--task", "det_v5_server",
    "--python", "python",
    "--device", "cpu",
    "--epochs", "1",
    "--batch-size", "1",
    "--learning-rate", "0.001",
    "--simulate-success",
    "--version-id", "verify_det_simulated",
    "--report", $trainingRunReport,
    "--log", $trainingRunLog
)
$trainingRunJson = $trainingRunResult | ConvertFrom-Json
if (-not $trainingRunJson.ok -or $trainingRunJson.status -ne "success") {
    throw "training runner simulate smoke failed`n$trainingRunResult"
}
Require-File $trainingRunReport
Require-File $trainingRunLog
$versionManifestPath = Join-Path $preflightProject "training\det_v5_server\versions.json"
Require-File $versionManifestPath
$versionManifest = Get-Content -LiteralPath $versionManifestPath -Raw | ConvertFrom-Json
if ($versionManifest.current_version_id -ne "verify_det_simulated" -or $versionManifest.best_version_id -ne "verify_det_simulated") {
    throw "training runner simulate did not promote current/best version"
}
if ($versionManifest.versions[0].status -ne "success") {
    throw "training runner simulate version was not marked success"
}
Require-File (Join-Path $preflightProject "training\runs.json")

Step "workflow smoke"
$workflowProject = Join-Path $verifyRoot "workflow_smoke_project"
$workflowReport = Join-Path $verifyRoot "workflow_smoke.json"
$workflowResult = Run-Exe (Join-Path $binPath "ppocr_workflow_smoke.exe") @(
    "--base-dir", $root,
    "--project", $workflowProject,
    "--report", $workflowReport
)
$workflowJson = $workflowResult | ConvertFrom-Json
if (-not $workflowJson.ok) {
    throw "workflow smoke failed`n$workflowResult"
}
Require-File $workflowReport
Require-File (Join-Path $workflowProject "project.json")
Require-File (Join-Path $workflowProject "training\runs.json")
Require-File (Join-Path $workflowProject "training\det_v5_server\versions.json")
Require-File (Join-Path $workflowProject "exports\ppocr_det\train.txt")
Require-File (Join-Path $workflowProject "exports\ppocr_rec\train.txt")
Require-File (Join-Path $workflowProject "exports\coco_layout\annotations\instance_train.json")

Step "workflow smoke with OCR prelabel"
$workflowOcrProject = Join-Path $verifyRoot "workflow_smoke_with_ocr_project"
$workflowOcrReport = Join-Path $verifyRoot "workflow_smoke_with_ocr.json"
$workflowOcrResult = Run-Exe (Join-Path $binPath "ppocr_workflow_smoke.exe") @(
    "--base-dir", $root,
    "--project", $workflowOcrProject,
    "--report", $workflowOcrReport,
    "--with-ocr"
)
Require-File $workflowOcrReport
$workflowOcrJson = Get-Content -LiteralPath $workflowOcrReport -Raw | ConvertFrom-Json
if (-not $workflowOcrJson.ok) {
    throw "workflow smoke with OCR failed`n$workflowOcrResult"
}
if ([int]$workflowOcrJson.real_ocr_auto_regions -le 0) {
    throw "workflow smoke with OCR did not write any auto OCR regions`n$workflowOcrResult"
}
Require-File (Join-Path $workflowOcrProject "project.json")
Require-File (Join-Path $workflowOcrProject "exports\ppocr_det\train.txt")
Require-File (Join-Path $workflowOcrProject "exports\ppocr_rec\train.txt")
Require-File (Join-Path $workflowOcrProject "predictions\workflow_ocr\page_000001_res.json")

if ($TrainingProbe) {
    Step "training runner real PaddleX probe"
    $probePython = $env:PPOCR_PADDLEX_PYTHON
    if ([string]::IsNullOrWhiteSpace($probePython)) {
        $preferredPython = "D:/IDE/anconda/envs/paddleX-py312/python.exe"
        $probePython = if (Test-Path -LiteralPath $preferredPython -PathType Leaf) { $preferredPython } else { "python" }
    }
    $probeReport = Join-Path $verifyRoot "training_run_probe.json"
    $probeLog = Join-Path $verifyRoot "training_run_probe.log"
    $probeResult = Run-Exe (Join-Path $binPath "ppocr_training_run.exe") @(
        "--base-dir", $root,
        "--project", $preflightProject,
        "--task", "det_v5_server",
        "--python", $probePython,
        "--device", "cpu",
        "--epochs", "1",
        "--batch-size", "1",
        "--learning-rate", "0.001",
        "--timeout-seconds", "$TrainingProbeTimeoutSeconds",
        "--version-id", "verify_det_real_probe",
        "--report", $probeReport,
        "--log", $probeLog
    )
    $probeJson = $probeResult | ConvertFrom-Json
    if (-not $probeJson.ok -or $probeJson.status -ne "success") {
        throw "training runner real PaddleX probe failed`n$probeResult"
    }
    Require-File $probeReport
    Require-File $probeLog
}

Step "GUI smoke"
Smoke-Gui (Join-Path $binPath "ppocr_workbench.exe") $root
Smoke-Gui (Join-Path $binPath "ppocr_workbench.exe") $root @("--project", $workflowProject)

if ($Package) {
    Step "package"
    powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "package_workbench.ps1") `
        -BuildDir $BuildDir `
        -Configuration $Configuration

    Step "dist smoke"
    Run-Exe (Join-Path $distPath "ppocr_workbench.exe") @("--help") | Out-Null
    Run-Exe (Join-Path $distPath "ppocr_ocr_predict.exe") @("--help") | Out-Null
    Run-Exe (Join-Path $distPath "ppocr_cls_predict.exe") @("--help") | Out-Null
    Run-Exe (Join-Path $distPath "ppocr_layout_predict.exe") @("--help") | Out-Null
    Run-Exe (Join-Path $distPath "ppocr_env_check.exe") @("--help") | Out-Null
    Run-Exe (Join-Path $distPath "ppocr_training_preflight.exe") @("--help") | Out-Null
    Run-Exe (Join-Path $distPath "ppocr_training_preflight.exe") @("--list-tasks") | Out-Null
    Run-Exe (Join-Path $distPath "ppocr_training_run.exe") @("--help") | Out-Null
    Run-Exe (Join-Path $distPath "ppocr_training_run.exe") @("--list-tasks") | Out-Null
    Run-Exe (Join-Path $distPath "ppocr_workflow_smoke.exe") @("--help") | Out-Null
    Run-Exe (Join-Path $distPath "ppocr_infer_smoke.exe") @(
        (Join-Path $root "model\infer\PP-OCRv5_server_det_infer")
    ) $distPath | Out-Null
    $distEnvReport = Join-Path $verifyRoot "dist_environment_report.json"
    $distEnvResult = Run-Exe (Join-Path $distPath "ppocr_env_check.exe") @(
        "--base-dir", $root,
        "--report", $distEnvReport
    ) $distPath
    $distEnvJson = $distEnvResult | ConvertFrom-Json
    if (-not $distEnvJson.ok) {
        throw "dist environment check failed`n$distEnvResult"
    }
    Require-File $distEnvReport
    $previousPaddleRoot = $env:PPOCR_PADDLE_INFERENCE_ROOT
    try {
        $env:PPOCR_PADDLE_INFERENCE_ROOT = $distPath
        Run-Exe (Join-Path $distPath "ppocr_infer_smoke.exe") @(
            (Join-Path $root "model\infer\PP-OCRv5_server_det_infer")
        ) $distPath | Out-Null
    } finally {
        $env:PPOCR_PADDLE_INFERENCE_ROOT = $previousPaddleRoot
    }
    $distWorkflowProject = Join-Path $verifyRoot "dist_workflow_smoke_project"
    $distWorkflowReport = Join-Path $verifyRoot "dist_workflow_smoke.json"
    $distWorkflowResult = Run-Exe (Join-Path $distPath "ppocr_workflow_smoke.exe") @(
        "--base-dir", $root,
        "--project", $distWorkflowProject,
        "--report", $distWorkflowReport
    ) $distPath
    $distWorkflowJson = $distWorkflowResult | ConvertFrom-Json
    if (-not $distWorkflowJson.ok) {
        throw "dist workflow smoke failed`n$distWorkflowResult"
    }
    Require-File $distWorkflowReport
    $distWorkflowOcrProject = Join-Path $verifyRoot "dist_workflow_smoke_with_ocr_project"
    $distWorkflowOcrReport = Join-Path $verifyRoot "dist_workflow_smoke_with_ocr.json"
    $distWorkflowOcrResult = Run-Exe (Join-Path $distPath "ppocr_workflow_smoke.exe") @(
        "--base-dir", $root,
        "--project", $distWorkflowOcrProject,
        "--report", $distWorkflowOcrReport,
        "--with-ocr"
    ) $distPath
    Require-File $distWorkflowOcrReport
    $distWorkflowOcrJson = Get-Content -LiteralPath $distWorkflowOcrReport -Raw | ConvertFrom-Json
    if (-not $distWorkflowOcrJson.ok) {
        throw "dist workflow smoke with OCR failed`n$distWorkflowOcrResult"
    }
    if ([int]$distWorkflowOcrJson.real_ocr_auto_regions -le 0) {
        throw "dist workflow smoke with OCR did not write any auto OCR regions`n$distWorkflowOcrResult"
    }
    Require-File (Join-Path $distWorkflowOcrProject "predictions\workflow_ocr\page_000001_res.json")
    Smoke-Gui (Join-Path $distPath "ppocr_workbench.exe") $distPath
    Smoke-Gui (Join-Path $distPath "ppocr_workbench.exe") $distPath @("--project", $distWorkflowProject)
}

Step "ok"
