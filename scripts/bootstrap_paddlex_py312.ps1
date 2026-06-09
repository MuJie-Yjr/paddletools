$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $Root

$env:SETUPTOOLS_SCM_PRETEND_VERSION_FOR_PADDLEX = "3.5.2"
$env:SETUPTOOLS_SCM_PRETEND_VERSION_FOR_PADDLEOCR = "3.5.0"

conda run -n paddleX-py312 python -m pip install -e .\PaddleX --no-deps
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
conda run -n paddleX-py312 python -m pip install -e .\PaddleOCR --no-deps
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "[bootstrap] PaddleX/PaddleOCR editable sources installed for the C++ training bridge."
