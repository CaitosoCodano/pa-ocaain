$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
$temp = Join-Path ([IO.Path]::GetTempPath()) ('ocaain-test-' + [guid]::NewGuid().ToString('N'))
$game = Join-Path $temp 'Half-Life'
$clientDir = Join-Path $game 'cstrike/cl_dlls'
$package = Join-Path $temp 'package'
$global:OcaainFixtureRunning = $false
$script:checks = 0

# Fixture scripts must never act on a real game or inspect live game state.
function Get-Process {
    [CmdletBinding()]param([string]$Name)
    if ($global:OcaainFixtureRunning) { [pscustomobject]@{ ProcessName = 'hl' } }
}
function Assert([bool]$Condition, [string]$Message) {
    $script:checks++
    if (-not $Condition) { throw "FAIL: $Message" }
}
function Reject([scriptblock]$Action, [string]$Message) {
    $failed = $false
    try { & $Action } catch { $failed = $true }
    Assert $failed $Message
}
try {
    New-Item -ItemType Directory -Path $clientDir, (Join-Path $package 'ocaain-menu') -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $game 'hl.exe'), 'fixture, not executable')
    [IO.File]::WriteAllText((Join-Path $clientDir 'client.dll'), 'original client fixture')
    [IO.File]::WriteAllText((Join-Path $package 'client.dll'), 'proxy fixture')
    [IO.File]::WriteAllText((Join-Path $package 'ocaain-menu/Ocaain.Menu.exe'), 'menu fixture')
    $originalHash = (Get-FileHash (Join-Path $clientDir 'client.dll')).Hash
    $global:OcaainFixtureRunning = $true
    Reject { & "$root/scripts/Install.ps1" -GameDirectory $game -PackageDirectory $package } 'Running game rejected'
    $global:OcaainFixtureRunning = $false
    & "$root/scripts/Install.ps1" -GameDirectory $game -PackageDirectory $package
    Assert ((Get-FileHash (Join-Path $clientDir 'client.original.dll')).Hash -eq $originalHash) 'Exact original preserved'
    Assert ((Get-FileHash (Join-Path $clientDir 'client.dll')).Hash -eq (Get-FileHash (Join-Path $package 'client.dll')).Hash) 'Proxy installed'
    Reject { & "$root/scripts/Install.ps1" -GameDirectory $game -PackageDirectory $package } 'Existing backup never overwritten'
    Assert ((Get-FileHash (Join-Path $clientDir 'client.original.dll')).Hash -eq $originalHash) 'Backup unchanged by rejected reinstall'
    [IO.File]::WriteAllText((Join-Path $clientDir 'client.dll'), 'Steam update fixture')
    Reject { & "$root/scripts/Uninstall.ps1" -GameDirectory $game } 'Updated client preserved'
    Assert ([IO.File]::ReadAllText((Join-Path $clientDir 'client.dll')) -eq 'Steam update fixture') 'Update not overwritten'
    Copy-Item (Join-Path $package 'client.dll') (Join-Path $clientDir 'client.dll') -Force
    [IO.File]::WriteAllText((Join-Path $clientDir 'client.original.dll'), 'altered backup fixture')
    Reject { & "$root/scripts/Uninstall.ps1" -GameDirectory $game } 'Altered backup rejected'
    [IO.File]::WriteAllText((Join-Path $clientDir 'client.original.dll'), 'original client fixture')
    & "$root/scripts/Uninstall.ps1" -GameDirectory $game
    Assert ((Get-FileHash (Join-Path $clientDir 'client.dll')).Hash -eq $originalHash) 'Original restored byte for byte'
    Assert (-not (Test-Path (Join-Path $clientDir 'client.original.dll'))) 'Backup moved back'
    Assert (-not (Test-Path (Join-Path $clientDir 'ocaain-menu'))) 'Installed menu removed'
    Assert (-not (Test-Path (Join-Path $clientDir 'ocaain-install.json'))) 'Manifest cleaned up'
    Write-Host "$script:checks installation checks passed (temporary fixtures only)."
}
finally {
    if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Recurse -Force }
    Remove-Variable OcaainFixtureRunning -Scope Global
}
