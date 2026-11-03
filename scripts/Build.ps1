[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent

function Invoke-Checked([string]$Command, [string[]]$Arguments) {
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Command failed with exit code $LASTEXITCODE" }
}

Push-Location $root
try {
    Get-Command cmake, dotnet, git -ErrorAction Stop | Out-Null
    Invoke-Checked cmake @('-S', '.', '-B', 'build/windows', '-A', 'Win32')
    Invoke-Checked cmake @('--build', 'build/windows', '--config', 'Release', '--parallel', '2')
    Invoke-Checked ctest @('--test-dir', 'build/windows', '-C', 'Release', '--output-on-failure')
    & (Join-Path $root 'tests/install_roundtrip.ps1')
    New-Item dist -ItemType Directory -Force | Out-Null
    Invoke-Checked dotnet @('publish', 'menu/Ocaain.Menu.csproj', '-c', 'Release', '-r', 'win-x86', '--self-contained', 'true', '-o', 'dist/ocaain-menu')
    Copy-Item build/windows/Release/client.dll dist/client.dll -Force
    $check = Start-Process -FilePath (Join-Path $root 'dist/ocaain-menu/Ocaain.Menu.exe') -ArgumentList '--self-test' -Wait -PassThru
    if ($check.ExitCode -ne 0) { throw 'WinForms self-test failed. See dist/ocaain-menu/self-test-results.json.' }
    Copy-Item scripts/Install.ps1, scripts/Uninstall.ps1, scripts/Diagnose.ps1, README.md, VERSION.txt -Destination dist -Force
    Copy-Item third_party/*.txt -Destination dist -Force
    Write-Host 'Build concluído em dist/. Consulte README.md antes de instalar.'
}
finally { Pop-Location }
