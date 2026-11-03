[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$GameDirectory)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (Get-Process hl -ErrorAction SilentlyContinue) { throw 'Feche o jogo antes de restaurar.' }
$game = (Resolve-Path -LiteralPath $GameDirectory).Path
if (-not (Test-Path -LiteralPath (Join-Path $game 'hl.exe'))) { throw 'A pasta deve conter hl.exe.' }
$clientDir = Join-Path $game 'cstrike/cl_dlls'
$client = Join-Path $clientDir 'client.dll'
$backup = Join-Path $clientDir 'client.original.dll'
$manifest = Join-Path $clientDir 'ocaain-install.json'
$menu = Join-Path $clientDir 'ocaain-menu'
if (-not (Test-Path -LiteralPath $manifest) -or -not (Test-Path -LiteralPath $backup)) {
    throw 'Instalação Ocaain ou backup não encontrados. Nenhum arquivo foi alterado.'
}
$info = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
if ($info.Version -ne 1) { throw 'Manifesto incompatível.' }
if ((Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash -ne $info.OriginalSha256) {
    throw 'O backup foi alterado. Restauração automática interrompida.'
}
if ((Test-Path -LiteralPath $client) -and
    (Get-FileHash -LiteralPath $client -Algorithm SHA256).Hash -ne $info.ProxySha256) {
    throw 'client.dll mudou após a instalação (possível atualização Steam). Preserve os arquivos e use Verificar integridade na Steam.'
}
$removed = Join-Path $clientDir ('.ocaain-remove-' + [guid]::NewGuid().ToString('N'))
if (Test-Path -LiteralPath $client) { Move-Item -LiteralPath $client -Destination $removed }
try { Move-Item -LiteralPath $backup -Destination $client }
catch {
    if (Test-Path -LiteralPath $removed) { Move-Item -LiteralPath $removed -Destination $client }
    throw
}
if (Test-Path -LiteralPath $removed) { Remove-Item -LiteralPath $removed -Force }
if (Test-Path -LiteralPath $menu) { Remove-Item -LiteralPath $menu -Recurse -Force }
Remove-Item -LiteralPath $manifest -Force
Write-Host 'DLL original restaurada. Remova -insecure das opções de inicialização para voltar ao cliente normal.'
