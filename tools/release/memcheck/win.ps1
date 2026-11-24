# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# The memory check's runner on Windows (tools/release/memcheck.sh copies it over and starts it by ssh). An ssh
# session has no desktop, so the games are run by a scheduled task in the logged-in user's session (schtasks /it):
# this script, run by ssh, makes the task, starts it, prints what the task's run of this script (-Inner) writes as it
# comes, and deletes the task. The run measures each program's private bytes every 2 s. The play and the lines printed
# are play.sh's (kept alike).
#   powershell -NoProfile -ExecutionPolicy Bypass -File win.ps1 -Dir DIR -Roms ROMS [-Work WORK]
#   (DIR: the Spelunkomp folder as unpacked from Spelunkomp-windows-x64.zip, or the folder it was unpacked in; ROMS:
#   the folder of the ROM sets, <id>.zip; WORK: a folder for each run's settings, saves, log and pictures,
#   %TEMP%\spelunkomp-memcheck by default)
param(
    [Parameter(Mandatory = $true)][string]$Dir,
    [Parameter(Mandatory = $true)][string]$Roms,
    [string]$Work = (Join-Path $env:TEMP 'spelunkomp-memcheck'),
    [string]$Games = 'mmpork:1260 ddpsdoj:1920 ddonpach:2940 ket:1320', # (ID:TITLE, play.sh's)
    [int]$Seconds = 120,
    [string]$Rewind = 'ddpsdoj',
    [int]$KillMB = 2048,
    [int]$KillVramMB = 3072,
    [switch]$Inner
)
$ErrorActionPreference = 'Stop'
$task = 'SpelunkompMemcheck'

# (an argument as Windows' command line wants it: quoted when it has a space)
function Quote([string]$a) { if ($a -match '[\s"]') { '"' + ($a -replace '"', '\"') + '"' } else { $a } }

if (-not $Inner) {
    if (Test-Path (Join-Path $Dir 'Spelunkomp\spelunkomp-launcher.exe')) { $Dir = Join-Path $Dir 'Spelunkomp' }
    if (-not (Test-Path (Join-Path $Dir 'spelunkomp-launcher.exe'))) { Write-Error "no spelunkomp-launcher.exe in $Dir (-Dir: the unpacked Spelunkomp folder)" }
    if (-not (Test-Path $Roms)) { Write-Error "no ROM folder $Roms" }
    # (no \ at the end: before a closing quote it would escape it)
    $Dir = (Resolve-Path $Dir).Path.TrimEnd('\'); $Roms = (Resolve-Path $Roms).Path.TrimEnd('\'); $Work = $Work.TrimEnd('\')
    # (only a warning: the task's run is what tells. query and qwinsta are not on every edition of Windows; explorer
    # runs while someone is at the desktop, and -IncludeUserName says who, if this session may ask: elevated)
    $desktop = $null
    try { $desktop = @(Get-Process explorer -IncludeUserName -ErrorAction Stop | Where-Object { $_.UserName -match "\\$([regex]::Escape($env:USERNAME))$" }).Count -gt 0 } catch {}
    if ($null -eq $desktop -and (Get-Command qwinsta -ErrorAction SilentlyContinue)) {
        try { $desktop = ((& qwinsta 2>$null) -join "`n") -match "(?im)^\W?\S*\s+$([regex]::Escape($env:USERNAME))\s+\d+\s+Active" } catch {}
    }
    if ($null -eq $desktop) { $desktop = @(Get-Process explorer -ErrorAction SilentlyContinue).Count -gt 0 }
    if (-not $desktop) { Write-Output "warning: no desktop session of $env:USERNAME seen: the task may not start" }
    Remove-Item -Recurse -Force $Work -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force $Work | Out-Null
    Copy-Item $PSCommandPath (Join-Path $Work 'win.ps1')
    $log = Join-Path $Work 'log.txt'; $done = Join-Path $Work 'done.txt'
    $line = 'powershell -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File ' + ((
            (Join-Path $Work 'win.ps1'), '-Inner', '-Dir', $Dir, '-Roms', $Roms, '-Work', $Work, '-Games', $Games,
            '-Seconds', "$Seconds", '-Rewind', $Rewind, '-KillMB', "$KillMB", '-KillVramMB', "$KillVramMB") | ForEach-Object { Quote $_ }) -join ' '
    Set-Content -Encoding ASCII (Join-Path $Work 'run.cmd') "@$line > `"$Work\inner.txt`" 2>&1"
    # (schtasks' /tr: a path without spaces, the short one)
    $run = (New-Object -ComObject Scripting.FileSystemObject).GetFile((Join-Path $Work 'run.cmd')).ShortPath
    & schtasks /create /tn $task /tr $run /sc once /st 23:59 /it /f | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Error "schtasks /create failed" }
    & schtasks /run /tn $task | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Error "schtasks /run failed" }
    $runs = @($Games -split '\s+' | Where-Object { $_ }).Count + 1
    $t0 = Get-Date; $limit = $t0.AddSeconds($runs * ($Seconds * 2 + 90) + 60); $shown = 0
    while ((Get-Date) -lt $limit) {
        $finished = Test-Path $done
        if (Test-Path $log) {
            try { $lines = @(Get-Content $log) } catch { $lines = @() } # (the task may be writing it)
            if ($lines.Count -gt $shown) { $lines[$shown..($lines.Count - 1)] | Write-Output; $shown = $lines.Count }
        } elseif (((Get-Date) - $t0).TotalSeconds -gt 60) {
            Write-Output 'error: the task has not started in a minute (is the user logged in at the desktop?)'; break
        }
        if ($finished) { break }
        Start-Sleep 2
    }
    if (-not (Test-Path $done)) {
        Write-Output "error: the task did not finish in time"
        if (Test-Path "$Work\inner.txt") { Get-Content "$Work\inner.txt" | ForEach-Object { "log task $_" } }
    }
    & schtasks /delete /tn $task /f | Out-Null
    exit 0
}

# The task's run, on the desktop
$log = Join-Path $Work 'log.txt'
function Say([string]$s) { # (the ssh side may be reading it: tried again)
    # (LF lines, as play.sh's; the ssh side prints them with Windows' CRLF all the same: the driver drops the CR)
    for ($k = 0; $k -lt 20; $k++) { try { [IO.File]::AppendAllText($log, "$s`n"); return } catch { Start-Sleep -Milliseconds 100 } }
}
# (PowerShell's names are the same in either case: $T, the run's folder, is $dir here, and the title's frame $tf)
function PlayArgs([int]$secs, [int]$tf, [string]$dir) { # (play.sh's play_args)
    $frames = $tf + $secs * 60
    $a = @('--frames', "$frames", '--real-time', '--mute', '--window-shot', "$($tf + 300):$dir\start.png",
        '--window-shot', "$($tf + $secs * 30):$dir\middle.png", '--window-shot', "$($frames - 120):$dir\late.png")
    $credits = @(($tf + 60), ($tf + 900)); for ($c = $tf + 1800; $c -lt $frames; $c += 3000) { $credits += $c }
    foreach ($c in $credits) { $a += @('--system', "$($c):5:0x04", '--system', "$($c + 100):5:0x10", '--system', "$($c + 200):5:0x10", '--system', "$($c + 300):5:0x10") }
    $i = 0
    for ($c = $tf + 60; $c -lt $frames; $c += 90) {
        if ($i % 2) { $stick = '0x08' } else { $stick = '0x04' }
        $a += @('--press', "$($c):85:0x10", '--press', "$($c):90:$stick"); $i++
    }
    return $a
}
# The program's dedicated GPU memory in MB, from the GPU Process Memory counters (Windows 10 1709 on; English names),
# or $null where they are not to be had (a program has none before its GPU device is made; asked no more after five
# failures with no success, the counters not there)
$script:gpuFails = 0; $script:gpuSeen = $false
function GpuMB([int]$id) {
    if (-not $script:gpuSeen -and $script:gpuFails -ge 5) { return $null }
    try {
        $c = Get-Counter -Counter "\GPU Process Memory(pid_$($id)_luid*)\Dedicated Usage" -ErrorAction Stop
        $script:gpuSeen = $true
        return [int](($c.CounterSamples | Measure-Object CookedValue -Sum).Sum / 1MB)
    } catch { $script:gpuFails++; return $null }
}
# (play.sh's stop_gently: asked to quit first, its window closed (SDL's quit event, the game's own ending), then
# taskkill without /F; 10 s to end; killed only then; $false if it has not ended 20 s after even that)
function StopGently($p, [string]$name) {
    $ErrorActionPreference = 'Continue' # (taskkill's complaints are not this script's errors)
    try { $asked = $p.CloseMainWindow() } catch { $asked = $false }
    if (-not $asked) { & taskkill /pid $p.Id 2>&1 | Out-Null }
    if ($p.WaitForExit(10000)) { return $true }
    Say "error: $name did not quit within 10 s of being asked: killed"
    & taskkill /f /pid $p.Id 2>&1 | Out-Null
    return $p.WaitForExit(20000)
}
function RunCase([string]$name, [string]$id, [int]$title, [string]$features) {
    $T = Join-Path $Work $name
    New-Item -ItemType Directory -Force (Join-Path $T 'saves') | Out-Null
    Say "case $name"
    $exe = Join-Path $Dir "$id.exe"
    if (-not (Test-Path $exe)) { Say "end $name missing"; return }
    $a = @('--roms', (Join-Path $Roms "$id.zip"), '--settings', (Join-Path $T 'settings.ini'), '--saves', (Join-Path $T 'saves'),
        '--features', $features) + (PlayArgs $Seconds $title $T)
    $p = Start-Process $exe -WorkingDirectory $Dir -PassThru -ArgumentList ((@($a) | ForEach-Object { Quote $_ }) -join ' ') `
        -RedirectStandardOutput (Join-Path $T 'out.txt') -RedirectStandardError (Join-Path $T 'err.txt')
    $null = $p.Handle # (so that ExitCode is there once it has exited)
    $t0 = Get-Date; $got = $t0; $status = ''
    while (-not $p.HasExited) {
        $tick = Get-Date; $s = [int]($tick - $t0).TotalSeconds
        $mb = 0; try { $p.Refresh(); $mb = [int]($p.PrivateMemorySize64 / 1MB) } catch {}
        if ($mb -gt 0) {
            $vmb = GpuMB $p.Id
            if ($null -ne $vmb -and -not $script:saidVram) { Say 'vram the program''s dedicated GPU memory (GPU Process Memory counters)'; $script:saidVram = $true }
            if ($null -ne $vmb) { Say "sample $name $s $mb $vmb" } else { Say "sample $name $s $mb" }
            $got = Get-Date
            if ($mb -gt $KillMB) { $status = "killed:$mb" }
            if ($null -ne $vmb -and $vmb -gt $KillVramMB) { $status = "killed-vram:$vmb" }
        } elseif (((Get-Date) - $got).TotalSeconds -ge 10) {
            Say "error: no sample within 10 s ($name, pid $($p.Id))"; $status = 'no-samples'
        }
        if (-not $status -and $s -gt 40 -and -not (Test-Path "$T\start.png")) { $status = "no-progress:start.png@$($s)s" }
        if (-not $status -and $s -gt [int]($Seconds * 3 / 4) + 30 -and -not (Test-Path "$T\middle.png")) { $status = "no-progress:middle.png@$($s)s" }
        if (-not $status -and $s -gt $Seconds * 2 + 60) { $status = 'timeout' }
        if ($status) { break }
        $left = 2000 - [int]((Get-Date) - $tick).TotalMilliseconds
        if ($left -gt 0) { Start-Sleep -Milliseconds $left }
    }
    if ($status -and -not (StopGently $p $name)) {
        Say "error: $name could not be stopped, even killed (stuck in its GPU driver?): no more runs on this machine; it may need restarting"
        Say "end $name $status+stuck"; $script:stuck = $true; return
    }
    $p.WaitForExit()
    if (-not $status) { $status = "exit:$($p.ExitCode)" }
    Say "end $name $status"
    if ($status -ne 'exit:0') { Get-Content (Join-Path $T 'err.txt'), (Join-Path $T 'out.txt') -Tail 5 -ErrorAction SilentlyContinue | ForEach-Object { Say "log $name $_" } }
}
$script:stuck = $false; $script:saidVram = $false
try {
    Say 'memory private bytes (Process.PrivateMemorySize64)'
    $rewound = -1
    foreach ($game in ($Games -split '\s+' | Where-Object { $_ })) {
        $id, $title = $game -split ':', 2; if (-not $title) { $title = 0 }
        RunCase $id $id $title 'free_play=1'
        if ($script:stuck) { break }
        if ($id -eq $Rewind) { $rewound = [int]$title }
    }
    if ($rewound -ge 0 -and -not $script:stuck) { RunCase "$Rewind+rewind" $Rewind $rewound 'free_play=1,rewind=1' }
} catch {
    Say "log task $_"
} finally {
    Set-Content (Join-Path $Work 'done.txt') 'done'
}
