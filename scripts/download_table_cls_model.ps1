param(
    [string]$ModelRoot = "model/infer",
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$modelRootPath = if ([System.IO.Path]::IsPathRooted($ModelRoot)) { $ModelRoot } else { Join-Path $root $ModelRoot }
$modelName = "PP-LCNet_x1_0_table_cls_infer"
$modelDir = Join-Path $modelRootPath $modelName
$nestedModelDir = Join-Path $modelDir $modelName
$url = "https://paddle-model-ecology.bj.bcebos.com/paddlex/official_inference_model/paddle3.0.0/PP-LCNet_x1_0_table_cls_infer.tar"

function Test-InferenceModel($Path) {
    return (Test-Path -LiteralPath (Join-Path $Path "inference.json") -PathType Leaf) -and
        (Test-Path -LiteralPath (Join-Path $Path "inference.pdiparams") -PathType Leaf)
}

if (-not $Force -and ((Test-InferenceModel $modelDir) -or (Test-InferenceModel $nestedModelDir))) {
    Write-Host "[model] table classification model already exists: $modelDir"
    exit 0
}

New-Item -ItemType Directory -Force -Path $modelRootPath | Out-Null
$tarPath = Join-Path ([System.IO.Path]::GetTempPath()) "$modelName.tar"

Write-Host "[model] download $url"
Invoke-WebRequest -Uri $url -OutFile $tarPath

Write-Host "[model] extract to $modelRootPath"
tar -xf $tarPath -C $modelRootPath
if ($LASTEXITCODE -ne 0) {
    throw "tar extraction failed with exit code $LASTEXITCODE"
}

Remove-Item -LiteralPath $tarPath -Force -ErrorAction SilentlyContinue

if (-not ((Test-InferenceModel $modelDir) -or (Test-InferenceModel $nestedModelDir))) {
    throw "downloaded model is missing inference.json or inference.pdiparams under $modelDir"
}

Write-Host "[model] ready: $modelDir"
