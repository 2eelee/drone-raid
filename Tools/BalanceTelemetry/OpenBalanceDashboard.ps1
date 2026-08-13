$ErrorActionPreference = 'Stop'

$ToolRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ToolRoot '..\..')
$DataRoot = Join-Path $RepoRoot 'Saved\BalanceTelemetry'
$Inbox = Join-Path $DataRoot 'inbox'
$Database = Join-Path $DataRoot 'balance.db'
$VenvPython = Join-Path $DataRoot '.venv\Scripts\python.exe'
$BundledPython = Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
$PythonCommand = Get-Command python -ErrorAction SilentlyContinue
$Python = if (Test-Path -LiteralPath $VenvPython) {
    $VenvPython
} elseif ($PythonCommand) {
    $PythonCommand.Source
} elseif (Test-Path -LiteralPath $BundledPython) {
    $BundledPython
} else {
    throw 'Python을 찾지 못했습니다. Python 3을 설치한 뒤 다시 실행하세요.'
}

New-Item -ItemType Directory -Force -Path $Inbox | Out-Null
$SourceCommit = 'Unknown'
$GitCommand = Get-Command git -ErrorAction SilentlyContinue
if ($GitCommand) {
    $GitOutput = & $GitCommand.Source -C $RepoRoot rev-parse --short HEAD 2>$null
    if ($LASTEXITCODE -eq 0 -and $GitOutput) {
        $SourceCommit = ($GitOutput | Select-Object -First 1).Trim()
    }
}

$LocalLogs = Join-Path $RepoRoot 'Saved\Logs'
$LogFiles = @()
if (Test-Path -LiteralPath $LocalLogs) {
    $LogFiles += Get-ChildItem -LiteralPath $LocalLogs -Filter 'DroneProto*.log' -File
}
$LogFiles += Get-ChildItem -LiteralPath $Inbox -Filter '*.log' -File
$LogFiles | Sort-Object FullName -Unique | ForEach-Object {
    & $Python (Join-Path $ToolRoot 'import_telemetry.py') import `
        --input $_.FullName `
        --db $Database `
        --source-commit $SourceCommit
}

& $Python -c 'import streamlit' 2>$null
if ($LASTEXITCODE -ne 0) {
	Write-Host 'Streamlit이 설치되지 않았습니다. SetupBalanceDashboard.ps1을 한 번 실행하세요.'
	exit 1
}

& $Python -m streamlit run (Join-Path $ToolRoot 'dashboard.py') -- --db $Database
