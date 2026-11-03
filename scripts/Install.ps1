[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$GameDirectory,
    [string]$PackageDirectory
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not $PackageDirectory) {
    if (Test-Path (Join-Path $PSScriptRoot 'client.dll')) { $PackageDirectory = $PSScriptRoot }
    else { $PackageDirectory = Join-Path (Split-Path $PSScriptRoot -Parent) 'dist' }
}
$game = (Resolve-Path -LiteralPath $GameDirectory).Path
$package = (Resolve-Path -LiteralPath $PackageDirectory).Path
if (-not (Test-Path -LiteralPath (Join-Path $game 'hl.exe'))) { throw 'A pasta deve conter hl.exe.' }
if (Get-Process hl -ErrorAction SilentlyContinue) { throw 'Feche o jogo antes de instalar.' }
if (-not (Test-Path -LiteralPath (Join-Path $package 'client.dll')) -or
    -not (Test-Path -LiteralPath (Join-Path $package 'ocaain-menu/Ocaain.Menu.exe'))) {
    throw 'Pacote incompleto. Execute Build.ps1 primeiro.'
}
$clientDir = Join-Path $game 'cstrike/cl_dlls'
$client = Join-Path $clientDir 'client.dll'
$backup = Join-Path $clientDir 'client.original.dll'
$menu = Join-Path $clientDir 'ocaain-menu'
$manifest = Join-Path $clientDir 'ocaain-install.json'
if (-not (Test-Path -LiteralPath $client)) { throw 'client.dll original não encontrada.' }
if ((Test-Path -LiteralPath $backup) -or (Test-Path -LiteralPath $manifest) -or (Test-Path -LiteralPath $menu)) {
    throw 'Já existe uma instalação ou backup. Execute Uninstall.ps1 antes de reinstalar; nenhum arquivo foi substituído.'
}
$originalHash = (Get-FileHash -LiteralPath $client -Algorithm SHA256).Hash
$proxyHash = (Get-FileHash -LiteralPath (Join-Path $package 'client.dll') -Algorithm SHA256).Hash
$stage = Join-Path $clientDir ('.ocaain-stage-' + [guid]::NewGuid().ToString('N'))
$moved = $false
$menuInstalled = $false
New-Item -ItemType Directory -Path $stage | Out-Null
try {
    Copy-Item -LiteralPath (Join-Path $package 'client.dll') -Destination (Join-Path $stage 'client.dll')
    Copy-Item -LiteralPath (Join-Path $package 'ocaain-menu') -Destination (Join-Path $stage 'ocaain-menu') -Recurse
    $menuHash = (Get-FileHash -LiteralPath (Join-Path $stage 'ocaain-menu/Ocaain.Menu.exe') -Algorithm SHA256).Hash
    Move-Item -LiteralPath $client -Destination $backup
    $moved = $true
    if ((Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash -ne $originalHash) { throw 'Falha na verificação do backup.' }
    Move-Item -LiteralPath (Join-Path $stage 'ocaain-menu') -Destination $menu
    $menuInstalled = $true
    Move-Item -LiteralPath (Join-Path $stage 'client.dll') -Destination $client
    if ((Get-FileHash -LiteralPath $client -Algorithm SHA256).Hash -ne $proxyHash) { throw 'Falha na verificação da DLL instalada.' }
    @{
        Version = 1; OriginalSha256 = $originalHash; ProxySha256 = $proxyHash; MenuSha256 = $menuHash
    } | ConvertTo-Json | Set-Content -LiteralPath $manifest -Encoding UTF8
    Write-Host 'Instalado com backup verificado. Inicie pela Steam com -insecure -gl, entre num mapa e pressione Insert.'
}
catch {
    if ($moved) {
        if (Test-Path -LiteralPath $client) { Remove-Item -LiteralPath $client -Force }
        if (Test-Path -LiteralPath $backup) { Move-Item -LiteralPath $backup -Destination $client }
    }
    if ($menuInstalled -and (Test-Path -LiteralPath $menu)) { Remove-Item -LiteralPath $menu -Recurse -Force }
    if (Test-Path -LiteralPath $manifest) { Remove-Item -LiteralPath $manifest -Force }
    throw
}
finally { if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force } }
