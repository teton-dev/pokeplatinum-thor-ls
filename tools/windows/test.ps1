# Windows counterpart of tools/docker/desktop.sh test (docs/thor/dev/desktop-windows.md): runs a test script
# (tests/README.md) with the win64 build in a fresh data folder, from a fixture's save, muted, its window parked off
# the screens without taking focus, and saves the window (PrintWindow) on each "[test] shot NAME" line.
# THOR_DEV (default: thor-dev beside the repository's folder) holds desktop/ (the data folder with the extracted ROM),
# fixtures/, PKMNB2.nds; runs go in test-run, shots and the log in test-shots/<script>[-Name].
#   tools\windows\test.ps1 -Script tests/scripts/x.txt [-From fixture] [-Data dir] [-Config 'K=v K=v'] [-ConfigPrj 'K=v']
#                          [-Scale 1] [-Hour 16] [-Env @{SIM_SUNSHADOW_DEBUG='1'}] [-Limit 600] [-Name tag] [-Repo dir]
# -Config is libntr's [General] settings (TEST_CONFIG), -ConfigPrj the game's (TEST_CONFIG_PRJ), -Hour
# POKEPLATINUM_ENHANCED_HOUR, -Scale the internal resolution (1 unless asked: the shots get large)
param(
    [string]$Repo = '',
    [Parameter(Mandatory)] [string]$Script,
    [string]$From = '',
    [string]$Data = '',
    [string]$Config = '',
    [string]$ConfigPrj = '',
    [int]$Scale = 1,
    [string]$Hour = '',
    [hashtable]$Env = @{},
    [int]$Limit = 600,
    [string]$Name = '',
    [string]$Exe = ''
)
$ErrorActionPreference = 'Stop'
if (-not $Repo) { $Repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path }
# thor-dev beside the repository (the usual layout), or beside the folder holding the repositories
$ThorDev = if ($env:THOR_DEV) { $env:THOR_DEV } else {
    @((Join-Path (Split-Path $Repo) 'thor-dev'), (Join-Path (Split-Path (Split-Path $Repo)) 'thor-dev')) |
        Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $ThorDev) { throw 'No thor-dev folder: set THOR_DEV' }
if (-not $Data) { $Data = Join-Path $ThorDev 'desktop' }
$scriptPath = (Resolve-Path (Join-Path $Repo $Script)).Path
$testName = [IO.Path]::GetFileNameWithoutExtension($scriptPath) + $(if ($Name) { "-$Name" } else { '' })
$run = Join-Path $ThorDev 'test-run'
$shots = Join-Path $ThorDev "test-shots\$testName"
if (-not $Exe) {
    # The packaged game (beside its DLLs and the built file system), not the bare link output in build_win64
    $Exe = Get-ChildItem (Join-Path $Repo 'build_win64') -Directory | ForEach-Object { Join-Path $_.FullName 'main.exe' } |
        Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $Exe) { $Exe = Join-Path $Repo 'build_win64\main.exe' } # Goldenrod's isn't packaged
}
if (-not (Test-Path $Exe)) { throw "No build: $Exe" }
$binDir = Split-Path $Exe
if (-not (Test-Path (Join-Path $Data 'arm9.bin'))) { throw "No extracted ROM in $Data" }

foreach ($d in $run, $shots) { if (Test-Path $d) { Remove-Item -Recurse -Force $d } ; New-Item -ItemType Directory $d | Out-Null }
# The extracted ROM linked in (junctions for folders, hard links for files), none of the saves or settings
foreach ($e in Get-ChildItem $Data) {
    if ($e.Name -match '^(save\.bin|save_slot.*|.*\.sav|DbgLog\.txt|.*\.nds|sim_config\.ini|sim_config_prj\.ini|imgui\.ini|gen5|gen5\.tmp)$') { continue }
    $dst = Join-Path $run $e.Name
    if ($e.Name -in 'carts.ini', 'cart.name') { Copy-Item $e.FullName $dst; continue }
    if ($e.PSIsContainer) { New-Item -ItemType Junction -Path $dst -Target $e.FullName | Out-Null }
    else { New-Item -ItemType HardLink -Path $dst -Target $e.FullName | Out-Null }
}
# gen5 (the imported BW2 files) is shared read-only from the data folder when there
if (Test-Path (Join-Path $Data 'gen5')) { New-Item -ItemType Junction -Path (Join-Path $run 'gen5') -Target (Join-Path $Data 'gen5') | Out-Null }
$cfg = "[General]`nInternalResolutionScale=$Scale`nMasterVolume=0`n"
foreach ($s in ($Config -split '\s+' | Where-Object { $_ })) { $cfg += "$s`n" }
Set-Content -NoNewline -Path (Join-Path $run 'sim_config.ini') -Value $cfg
if ($ConfigPrj) {
    $p = "[General]`n"; foreach ($s in ($ConfigPrj -split '\s+' | Where-Object { $_ })) { $p += "$s`n" }
    Set-Content -NoNewline -Path (Join-Path $run 'sim_config_prj.ini') -Value $p
}
if ($From) {
    $fx = Join-Path $ThorDev "fixtures\$From\save.bin"
    if (-not (Test-Path $fx)) { throw "No fixture $fx" }
    Copy-Item $fx (Join-Path $run 'save.bin')
}
foreach ($f in 'firmware.bin', 'header.bin') { if (Test-Path (Join-Path $binDir $f)) { Copy-Item (Join-Path $binDir $f) $run } }
# Goldenrod's firmware.bin (a Wi-Fi settings placeholder) is the repository's res\pc_port\dwc_nv.bin
$dwc = Join-Path $Repo 'res\pc_port\dwc_nv.bin'
if (-not (Test-Path (Join-Path $run 'firmware.bin')) -and (Test-Path $dwc)) { Copy-Item $dwc (Join-Path $run 'firmware.bin') }

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class W {
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [DllImport("user32.dll")] public static extern int GetSystemMetrics(int i);
  public struct RECT { public int L, T, R, B; }
  public struct POINT { public int X, Y; }
}
'@
[W]::SetProcessDPIAware() | Out-Null

$psi = New-Object Diagnostics.ProcessStartInfo $Exe
$psi.WorkingDirectory = $run
$psi.UseShellExecute = $false
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
# SDL2.dll and the rest beside main.exe (Eterna's packaged build), else MSYS2's (Goldenrod's build_win64\main.exe)
$psi.EnvironmentVariables['PATH'] = "$binDir;C:\msys64\ucrt64\bin;" + $psi.EnvironmentVariables['PATH']
$psi.EnvironmentVariables['LIBNTR_TEST_MODE'] = '1'
$psi.EnvironmentVariables['POKEPLATINUM_TEST_SCRIPT'] = $scriptPath
$psi.EnvironmentVariables['SDL_AUDIODRIVER'] = 'dummy'
$psi.EnvironmentVariables['POKEPLATINUM_ENHANCED_HOUR'] = $Hour
$gen5Rom = Join-Path $ThorDev 'PKMNB2.nds'
if (Test-Path $gen5Rom) { $psi.EnvironmentVariables['POKEPLATINUM_GEN5_ROM'] = $gen5Rom }
# Goldenrod reads the same settings as POKEHEARTGOLD_*
foreach ($k in @($psi.EnvironmentVariables.Keys)) {
    if ($k -like 'pokeplatinum_*') { $psi.EnvironmentVariables['POKEHEARTGOLD_' + $k.Substring(13).ToUpper()] = $psi.EnvironmentVariables[$k] }
}
foreach ($k in $Env.Keys) { $psi.EnvironmentVariables[$k] = [string]$Env[$k] }
$proc = [Diagnostics.Process]::Start($psi)
$log = Join-Path $shots 'log.txt'
$errTask = $proc.StandardError.ReadToEndAsync()
Write-Host "Running $testName$(if ($From) { " from $From" })"

# Headless as far as Windows allows: the window is moved off every screen (no focus taken) as soon as it shows,
# and shots are read from the window itself (PrintWindow, full content), not from the screen
$script:parked = $false
function Hide-Window {
    if ($script:parked) { return }
    $proc.Refresh()
    $h = $proc.MainWindowHandle
    if ($h -eq [IntPtr]::Zero) { return }
    $x = [W]::GetSystemMetrics(76) + [W]::GetSystemMetrics(78) + 200 # right of the virtual screen
    [W]::SetWindowPos($h, [IntPtr]1, $x, 0, 0, 0, 0x0001 -bor 0x0010) | Out-Null # HWND_BOTTOM, NOSIZE, NOACTIVATE
    $script:parked = $true
}
function Save-Shot([string]$shotName) {
    Hide-Window
    $proc.Refresh()
    $h = $proc.MainWindowHandle
    if ($h -eq [IntPtr]::Zero) { Write-Host "[test] shot ${shotName}: no window"; return }
    $r = New-Object W+RECT; [W]::GetClientRect($h, [ref]$r) | Out-Null
    $w = $r.R - $r.L; $hgt = $r.B - $r.T
    if ($w -le 0 -or $hgt -le 0) { return }
    $bmp = New-Object Drawing.Bitmap $w, $hgt
    $g = [Drawing.Graphics]::FromImage($bmp)
    $dc = $g.GetHdc()
    [W]::PrintWindow($h, $dc, 3) | Out-Null # PW_CLIENTONLY | PW_RENDERFULLCONTENT
    $g.ReleaseHdc($dc)
    $file = Join-Path $shots "$shotName.png"
    $bmp.Save($file, [Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host "[test] shot $shotName -> $file"
}

$start = Get-Date
$lines = New-Object Collections.Generic.List[string]
$readTask = $proc.StandardOutput.ReadLineAsync()
while ($true) {
    Hide-Window
    if ($readTask.Wait(200)) {
        $line = $readTask.Result
        if ($null -eq $line) { break }
        $lines.Add($line)
        if ($line -match '^\[test\] shot (.+)$') { Save-Shot $Matches[1] }
        elseif ($line -match '^\[test\] |\[assert\]|Assert') { Write-Host $line }
        $readTask = $proc.StandardOutput.ReadLineAsync()
    }
    if (((Get-Date) - $start).TotalSeconds -ge $Limit) {
        Write-Host "[test] FAIL: still running after $Limit s"
        Save-Shot 'timeout'
        $proc.Kill()
        break
    }
}
$proc.WaitForExit(5000) | Out-Null
$lines.Add('--- stderr ---'); $lines.Add($errTask.Result)
Set-Content -Path $log -Value $lines
$code = if ($proc.HasExited) { $proc.ExitCode } else { -1 }
if ($code -ne 0 -and $code -ne 1) { Write-Host "[test] FAIL: the game exited with code $code" }
Write-Host "exit $code; log $log"
exit $code
