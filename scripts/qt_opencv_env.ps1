$ErrorActionPreference = "Stop"

$required = @(
    "QT_ROOT",
    "OpenCV_DIR"
)

$missing = @()
foreach ($name in $required) {
    if ([string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable($name))) {
        $missing += $name
    }
}
if ($missing.Count -gt 0) {
    throw "Missing required environment variable(s): $($missing -join ', ')"
}

if ([string]::IsNullOrWhiteSpace($env:OPENCV_RUNTIME_DIR)) {
    $env:OPENCV_RUNTIME_DIR = Join-Path (Split-Path -Parent $env:OpenCV_DIR) "bin"
}

$prependPaths = @(
    (Join-Path $env:QT_ROOT "bin"),
    $env:OPENCV_RUNTIME_DIR
)

if (-not [string]::IsNullOrWhiteSpace($env:CUDA_PATH)) {
    $prependPaths += (Join-Path $env:CUDA_PATH "bin")
}
if (-not [string]::IsNullOrWhiteSpace($env:TENSORRT_ROOT)) {
    $prependPaths += (Join-Path $env:TENSORRT_ROOT "bin")
}

$validPrependPaths = @()
foreach ($path in $prependPaths) {
    if (Test-Path $path) {
        $validPrependPaths += $path
    }
    else {
        Write-Warning "Missing environment path: $path"
    }
}

$existingPaths = $env:PATH -split ";" | Where-Object { $_ }
$remainingPaths = $existingPaths | Where-Object { $validPrependPaths -notcontains $_ }
$env:PATH = (($validPrependPaths + $remainingPaths) -join ";")

Write-Host "Qt/OpenCV environment loaded."
Write-Host "QT_ROOT=$env:QT_ROOT"
Write-Host "OpenCV_DIR=$env:OpenCV_DIR"
Write-Host "OPENCV_RUNTIME_DIR=$env:OPENCV_RUNTIME_DIR"
if (-not [string]::IsNullOrWhiteSpace($env:CUDA_PATH)) {
    Write-Host "CUDA_PATH=$env:CUDA_PATH"
}
if (-not [string]::IsNullOrWhiteSpace($env:TENSORRT_ROOT)) {
    Write-Host "TENSORRT_ROOT=$env:TENSORRT_ROOT"
}
