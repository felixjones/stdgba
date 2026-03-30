param(
    [switch]$Async,
    [switch]$Run,
    [int]$WaitSeconds = 4,
    [string]$MgbaPath = "C:\Program Files\mGBA\mGBA.exe"
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $PSCommandPath
$luaScript = Join-Path $scriptDir "screenshot.lua"
$imgDir = Resolve-Path (Join-Path $scriptDir "..\src\img")
$buildDir = Resolve-Path (Join-Path $scriptDir "..\..\build\book\demos")
$logPath = Join-Path $scriptDir "screenshot.log"
$errPath = Join-Path $scriptDir "screenshot.err.log"

if ($Async) {
    if (Test-Path $logPath) { Remove-Item -Force $logPath }
    if (Test-Path $errPath) { Remove-Item -Force $errPath }

    $argString = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Run -WaitSeconds $WaitSeconds -MgbaPath `"$MgbaPath`""

    Start-Process -WindowStyle Hidden -FilePath "powershell.exe" -ArgumentList $argString -WorkingDirectory $scriptDir -RedirectStandardOutput $logPath -RedirectStandardError $errPath | Out-Null
    Write-Host "Started async screenshot job (see $logPath, $errPath)."
    exit 0
}

if (-not $Run) {
    Write-Host "Usage:"
    Write-Host "  powershell.exe -NoProfile -ExecutionPolicy Bypass -File screenshot.ps1"
    Write-Host "  powershell.exe -NoProfile -ExecutionPolicy Bypass -File screenshot.ps1 -Async"
    Write-Host ""
    Write-Host "Options: -WaitSeconds <n>, -MgbaPath <path>"
    exit 0
}

if (-not (Test-Path $MgbaPath)) {
    Write-Error "mGBA not found at $MgbaPath"
}

if (-not (Test-Path $luaScript)) {
    Write-Error "screenshot.lua not found at $luaScript"
}

if (-not (Test-Path $buildDir)) {
    Write-Error "Build directory not found: $buildDir"
}

New-Item -ItemType Directory -Force -Path $imgDir | Out-Null

Get-ChildItem -Path $buildDir -Filter "demo_*.elf" | ForEach-Object {
    $elf = $_.FullName
    $romDir = $_.DirectoryName
    $name = $_.BaseName
    $screenshotName = $name -replace '^demo_', ''

    Write-Host "$screenshotName..."

    Get-ChildItem -Path $romDir -Filter "$name-*.png" -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue

    $p = Start-Process -PassThru -FilePath $MgbaPath -ArgumentList @(
        "-C", "updateAutoCheck=0",
        "-C", "skipBios=1",
        "--script", $luaScript,
        $elf
    )

    Start-Sleep -Seconds $WaitSeconds

    if (-not $p.HasExited) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    }

    Start-Sleep -Milliseconds 200

    $src = Join-Path $romDir "$name-0.png"
    $dst = Join-Path $imgDir "$screenshotName.png"

    if (Test-Path $src) {
        Move-Item -Force $src $dst
        Write-Host "  ok"
    } else {
        Write-Host "  FAILED - no screenshot found"
    }
}

Write-Host "Done. Screenshots in $imgDir"
