$ErrorActionPreference = "Stop"

$qtRoot = "D:\IDE\Qt\6.10.3\msvc2022_64"
$opencvRoot = "D:\IDE\opencv\install_vs2026_cuda89_nvcodec_opengl"
$cudaRoot = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.4"

$env:Qt6_DIR = Join-Path $qtRoot "lib\cmake\Qt6"
$env:OpenCV_DIR = Join-Path $opencvRoot "lib"
$env:CUDA_PATH = $cudaRoot

$prependPaths = @(
    (Join-Path $qtRoot "bin"),
    (Join-Path $opencvRoot "bin"),
    (Join-Path $cudaRoot "bin")
)

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

Write-Host "Qt/OpenCV/CUDA environment loaded."
Write-Host "Qt6_DIR=$env:Qt6_DIR"
Write-Host "OpenCV_DIR=$env:OpenCV_DIR"
Write-Host "CUDA_PATH=$env:CUDA_PATH"
