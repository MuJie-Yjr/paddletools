param(
    [string]$Configuration = "Release",
    [string]$BuildDir = "build_vs2026",
    [string]$PackageDir = "dist/ppocr_workbench",
    [switch]$IncludeModels,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildPath = Join-Path $root $BuildDir
$releasePath = Join-Path $buildPath $Configuration
$packagePath = Join-Path $root $PackageDir
$qtRoot = $env:QT_ROOT
$qtBin = if ([string]::IsNullOrWhiteSpace($qtRoot)) { "" } else { Join-Path $qtRoot "bin" }
$opencvBin = $env:OPENCV_RUNTIME_DIR
if ([string]::IsNullOrWhiteSpace($opencvBin) -and -not [string]::IsNullOrWhiteSpace($env:OpenCV_DIR)) {
    $opencvBin = Join-Path (Split-Path -Parent $env:OpenCV_DIR) "bin"
}
$paddleRoot = if ([string]::IsNullOrWhiteSpace($env:PPOCR_PADDLE_INFERENCE_ROOT)) {
    Join-Path $root "third_party/paddle_inference"
} else {
    $env:PPOCR_PADDLE_INFERENCE_ROOT
}
$cmakeExe = Get-Command cmake -ErrorAction SilentlyContinue

function Invoke-Step($Description, [scriptblock]$Action) {
    Write-Host "[package] $Description"
    if (-not $DryRun) {
        & $Action
    }
}

function Copy-IfExists($Path, $Destination) {
    if (Test-Path $Path) {
        Invoke-Step "copy $Path -> $Destination" {
            New-Item -ItemType Directory -Force -Path $Destination | Out-Null
            Copy-Item -Path $Path -Destination $Destination -Force
        }
    }
}

function Copy-Matches($Pattern, $Destination) {
    Get-ChildItem -Path $Pattern -File -ErrorAction SilentlyContinue | ForEach-Object {
        Copy-IfExists $_.FullName $Destination
    }
}

Invoke-Step "build Release" {
    if (-not $cmakeExe) {
        throw "cmake was not found in PATH"
    }
    & $cmakeExe.Source --build --preset release
}

Invoke-Step "prepare package directory $packagePath" {
    if (Test-Path $packagePath) {
        Remove-Item -LiteralPath $packagePath -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $packagePath | Out-Null
}

$executables = @(
    "ppocr_workbench.exe",
    "ppocr_env_check.exe",
    "ppocr_infer_smoke.exe",
    "ppocr_ocr_predict.exe",
    "ppocr_cls_predict.exe",
    "ppocr_layout_predict.exe",
    "ppocr_training_preflight.exe",
    "ppocr_training_run.exe",
    "ppocr_workflow_smoke.exe"
)
foreach ($exe in $executables) {
    Copy-IfExists (Join-Path $releasePath $exe) $packagePath
}

if ([string]::IsNullOrWhiteSpace($opencvBin)) {
    Write-Warning "OPENCV_RUNTIME_DIR is not set; OpenCV DLL copy will be skipped."
}
else {
    foreach ($opencvDll in @(
        "opencv_world*.dll",
        "opencv_imgcodecs*.dll",
        "opencv_imgproc*.dll",
        "opencv_core*.dll"
    )) {
        Copy-Matches (Join-Path $opencvBin $opencvDll) $packagePath
    }
}

Get-ChildItem -Path (Join-Path $paddleRoot "paddle/lib") -Filter "*.dll" -ErrorAction SilentlyContinue | ForEach-Object {
    Copy-IfExists $_.FullName $packagePath
}
foreach ($rel in @(
    "third_party/install/mklml/lib",
    "third_party/install/onednn/lib",
    "third_party/install/mkldnn/lib",
    "third_party/install/glog/lib",
    "third_party/install/gflags/lib",
    "third_party/install/protobuf/lib",
    "third_party/install/xxhash/lib",
    "third_party/install/utf8proc/lib"
)) {
    $dir = Join-Path $paddleRoot $rel
    Copy-Matches (Join-Path $dir "*.dll") $packagePath
}

$windeployqt = if ([string]::IsNullOrWhiteSpace($qtBin)) { "" } else { Join-Path $qtBin "windeployqt.exe" }
if (-not [string]::IsNullOrWhiteSpace($windeployqt) -and (Test-Path $windeployqt)) {
    Invoke-Step "run windeployqt" {
        & $windeployqt --release --no-translations --qmldir (Join-Path $root "qml") (Join-Path $packagePath "ppocr_workbench.exe")
    }
}

if ($IncludeModels) {
    Invoke-Step "copy inference models" {
        Copy-Item -Path (Join-Path $root "model/infer") -Destination (Join-Path $packagePath "model/infer") -Recurse -Force
    }
}

Invoke-Step "write launch helper" {
@"
@echo off
setlocal
cd /d "%~dp0"
if not defined PPOCR_BASE_DIR if exist "%~dp0..\..\PaddleX" set "PPOCR_BASE_DIR=%~dp0..\.."
ppocr_workbench.exe %*
"@ | Set-Content -Path (Join-Path $packagePath "run_workbench.bat") -Encoding ASCII
}

Write-Host "[package] done: $packagePath"
