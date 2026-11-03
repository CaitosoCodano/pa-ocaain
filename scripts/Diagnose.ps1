[CmdletBinding()]
param(
    [string]$GameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\Half-Life',
    [switch]$SkipSelfTest
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Write-Host 'Ocaain v0.2-menu-fix - installation diagnostics'
$clientDir = Join-Path $GameDirectory 'cstrike/cl_dlls'
$client = Join-Path $clientDir 'client.dll'
$backup = Join-Path $clientDir 'client.original.dll'
$manifest = Join-Path $clientDir 'ocaain-install.json'
$menu = Join-Path $clientDir 'ocaain-menu/Ocaain.Menu.exe'
foreach ($entry in @{
    'Game executable' = (Join-Path $GameDirectory 'hl.exe')
    'Client DLL' = $client
    'Original backup' = $backup
    'Install manifest' = $manifest
    'Menu executable' = $menu
}.GetEnumerator()) {
    Write-Host ($entry.Key + ': ' + (Test-Path -LiteralPath $entry.Value))
}
if (Test-Path -LiteralPath $manifest) {
    try {
        $info = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
        if (Test-Path -LiteralPath $client) {
            Write-Host ('Client matches install: ' + ((Get-FileHash -LiteralPath $client -Algorithm SHA256).Hash -eq $info.ProxySha256))
        }
        if (Test-Path -LiteralPath $backup) {
            Write-Host ('Backup matches original: ' + ((Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash -eq $info.OriginalSha256))
        }
        if (Test-Path -LiteralPath $menu) {
            Write-Host ('Menu matches install: ' + ((Get-FileHash -LiteralPath $menu -Algorithm SHA256).Hash -eq $info.MenuSha256))
        }
    }
    catch { Write-Host ('Manifest check failed: ' + $_.Exception.Message) }
}
$games = @(Get-Process hl -ErrorAction SilentlyContinue)
$menus = @(Get-Process Ocaain.Menu -ErrorAction SilentlyContinue)
Write-Host ('Running game processes: ' + $games.Count)
Write-Host ('Running menu processes: ' + $menus.Count)
foreach ($game in $games) {
    Write-Host ('Game PID: ' + $game.Id)
    try {
        # Show only required flag presence, never the full command line, which
        # can contain a server password or other unrelated private arguments.
        $details = Get-CimInstance Win32_Process -Filter ('ProcessId=' + $game.Id)
        $commandLine = [string]$details.CommandLine
        Write-Host ('Launch -insecure: ' + [bool]($commandLine -match '(?i)(?:^|\s)-insecure(?:\s|$)'))
        Write-Host ('Launch -gl: ' + [bool]($commandLine -match '(?i)(?:^|\s)-gl(?:\s|$)'))
    }
    catch { Write-Host ('Launch flags unavailable: ' + $_.Exception.Message) }
    try {
        $modules = @($game.Modules | Where-Object { $_.ModuleName -in @('client.dll', 'client.original.dll') })
        foreach ($module in $modules) { Write-Host ('Loaded: ' + $module.FileName) }
        if ($modules.Count -eq 0) { Write-Host 'Client modules not found in this process.' }
    }
    catch { Write-Host ('Module inspection unavailable: ' + $_.Exception.Message) }
    foreach ($name in @('Ocaain.Native-', 'Ocaain.Menu-')) {
        $log = Join-Path ([IO.Path]::GetTempPath()) ($name + $game.Id + '.log')
        if (Test-Path -LiteralPath $log) {
            Write-Host ('Log: ' + $log)
            Get-Content -LiteralPath $log -Tail 40 | ForEach-Object { Write-Host $_ }
        }
    }
}
if (-not $SkipSelfTest -and (Test-Path -LiteralPath $menu)) {
    $prefix = Join-Path ([IO.Path]::GetTempPath()) ('Ocaain.Check-' + [guid]::NewGuid().ToString('N'))
    $result = $prefix + '.json'
    Write-Host 'Testing installed WinForms renderer (no game files are changed)...'
    try {
        $test = Start-Process -FilePath $menu -ArgumentList ('--self-test "' + $result + '"') -PassThru -WindowStyle Hidden -RedirectStandardOutput ($prefix + '.stdout.log') -RedirectStandardError ($prefix + '.stderr.log')
        if (-not $test.WaitForExit(30000)) {
            $test.Kill()
            $test.WaitForExit()
            Write-Host 'Renderer self-test timed out after 30 seconds.'
        }
        else {
            $test.Refresh()
            Write-Host ('Renderer exit code: ' + $test.ExitCode)
            if (Test-Path -LiteralPath $result) {
                Get-Content -LiteralPath $result -Raw | Write-Host
            }
            else { Write-Host 'Renderer did not write a result; initialization may have failed.' }
        }
        $startupLog = Join-Path ([IO.Path]::GetTempPath()) ('Ocaain.Menu-' + $test.Id + '.log')
        if (Test-Path -LiteralPath $startupLog) {
            Write-Host ('Renderer initialization log: ' + $startupLog)
            Get-Content -LiteralPath $startupLog -Tail 40 | ForEach-Object { Write-Host $_ }
        }
        if ((Test-Path -LiteralPath ($prefix + '.stderr.log')) -and (Get-Item -LiteralPath ($prefix + '.stderr.log')).Length -gt 0) {
            Get-Content -LiteralPath ($prefix + '.stderr.log') -Tail 40 | ForEach-Object { Write-Host $_ }
        }
        Write-Host ('Diagnostic files: ' + $prefix + '.*')
    }
    catch { Write-Host ('Renderer test failed to start: ' + $_.Exception.Message) }
}
Write-Host 'Copy this output to report the failure. No game files were changed.'
