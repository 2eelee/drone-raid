$ErrorActionPreference = 'Stop'

$ToolRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ToolRoot '..\..')
$DataRoot = Join-Path $RepoRoot 'Saved\BalanceTelemetry'
$VenvRoot = Join-Path $DataRoot '.venv'
$VenvPython = Join-Path $VenvRoot 'Scripts\python.exe'
$BundledPython = Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
$PythonCommand = Get-Command python -ErrorAction SilentlyContinue
$BasePython = if ($PythonCommand) {
    $PythonCommand.Source
} elseif (Test-Path -LiteralPath $BundledPython) {
    $BundledPython
} else {
    throw 'Python을 찾지 못했습니다. Python 3을 설치한 뒤 다시 실행하세요.'
}

New-Item -ItemType Directory -Force -Path $DataRoot | Out-Null
if (-not (Test-Path -LiteralPath $VenvPython)) {
    & $BasePython -m venv $VenvRoot
}
& $VenvPython -m pip install --disable-pip-version-check -r (Join-Path $ToolRoot 'requirements-dashboard.txt')
Write-Host '설치 완료. 이제 OpenBalanceDashboard.ps1을 실행하세요.'
