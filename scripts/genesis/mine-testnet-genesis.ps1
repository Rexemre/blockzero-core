# Mine the Block Zero testnet genesis block (RandomX).
#
# Usage (PowerShell, from blockzero-core):
#   .\scripts\genesis\mine-testnet-genesis.ps1
#   .\scripts\genesis\mine-testnet-genesis.ps1 -BinDir ".\build\bin\Release"
#   .\scripts\genesis\mine-testnet-genesis.ps1 -UseWsl   # slow fallback if no Windows build
#
# Native Windows: ~1-5 min. WSL2 fallback: ~30-90 min (RandomX is slow in WSL).

param(
    [string]$BinDir = "",
    [string]$LogFile = "genesis-mine.log",
    [switch]$UseWsl,
    [string]$WslDistro = "Ubuntu-22.04",
    [string]$WslMinerPath = "/home/USER/blockzero-core/build/bin/bz-genesis-miner"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

function Find-WindowsMiner {
    if ($BinDir) {
        $p = Join-Path $BinDir "bz-genesis-miner.exe"
        if (Test-Path $p) { return $p }
    }
    $candidates = @(
        (Join-Path $RepoRoot "build\bin\Release\bz-genesis-miner.exe")
        (Join-Path $RepoRoot "build\bin\bz-genesis-miner.exe")
        (Join-Path $env:LOCALAPPDATA "BlockZero\bin\bz-genesis-miner.exe")
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { return $c }
    }
    return $null
}

function Show-BuildHelp {
    Write-Host ""
    Write-Host "Cannot find bz-genesis-miner.exe on Windows."
    Write-Host ""
    Write-Host "Option A - Build on Windows (fast, recommended):"
    Write-Host "  Install Visual Studio 2022 with C++ desktop workload"
    Write-Host "  Install CMake: winget install Kitware.CMake"
    Write-Host "  Open a NEW PowerShell window, then:"
    Write-Host "    cd $RepoRoot"
    Write-Host "    cmake -B build --preset vs2026-static"
    Write-Host "    cmake --build build --config Release --target bz-genesis-miner"
    Write-Host ""
    Write-Host "Option B - WSL fallback (slow, but works without VS):"
    Write-Host "    .\scripts\genesis\mine-testnet-genesis.ps1 -UseWsl"
    Write-Host ""
    Write-Host "Option C - Download a release zip that includes bz-genesis-miner.exe"
    Write-Host "  https://github.com/Rexemre/blockzero-core/releases"
}

$specPath = Join-Path $RepoRoot "artifacts\genesis\testnet.json"
if (-not (Test-Path $specPath)) {
    throw "Missing $specPath"
}
$spec = Get-Content $specPath -Raw | ConvertFrom-Json

Write-Host "Block Zero testnet genesis miner"
Write-Host "Message: $($spec.message)"
Write-Host "nTime:   $($spec.nTime) ($($spec.nTimeHuman))"
Write-Host ""

$logPath = Join-Path (Get-Location) $LogFile
$winMiner = Find-WindowsMiner

if ($UseWsl -or (-not $winMiner)) {
    if (-not $UseWsl -and -not $winMiner) {
        Show-BuildHelp
        if (-not (Get-Command wsl -ErrorAction SilentlyContinue)) {
            throw "No Windows miner found and WSL is not available."
        }
        Write-Host "Trying WSL fallback automatically..."
    } else {
        Write-Host "Using WSL miner (slow in WSL2 - expect 30-90 min)."
    }

    $wslRepo = "/home/USER/blockzero-core"
    $sync = @"
cp /mnt/c/Users/USER/blockzero/blockzero-core/src/bz_genesis_miner.cpp $wslRepo/src/bz_genesis_miner.cpp && cd $wslRepo && cmake --build build --target bz-genesis-miner -j`$(nproc) && $WslMinerPath
"@
    Write-Host "WSL: building/running bz-genesis-miner..."
    wsl -d $WslDistro -e bash -lc $sync 2>&1 | Tee-Object -FilePath $logPath
} else {
    Write-Host "Using: $winMiner"
    Write-Host "Logging to: $LogFile"
    Write-Host ""
    & $winMiner 2>&1 | Tee-Object -FilePath $logPath
}

if (-not (Test-Path $logPath)) {
    throw "No log written to $logPath"
}
if ((Get-Content $logPath -Raw) -notmatch '\[testnet\] FOUND') {
    throw "Mining did not finish successfully. Check $logPath"
}

Write-Host ""
Write-Host "Done. Next step:"
Write-Host "  .\scripts\genesis\apply-testnet-genesis.ps1 -LogFile $LogFile"
