param(
    [Parameter(Mandatory=$true)][string]$Engine,
    [Parameter(Mandatory=$true)][string]$RetailBase,
    [Parameter(Mandatory=$true)][string]$Profile
)
# Records per-frame movement traces for the Half-Life movement modes on a long
# flat track, for compare-halflife.py to replay through reference models of
# Valve's movement code. Isolated profile, muted, hidden window.
$ErrorActionPreference='Stop'
if(Test-Path $Profile){throw 'Use a new isolated test profile'}
$Engine=[IO.Path]::GetFullPath($Engine)
$Profile=[IO.Path]::GetFullPath($Profile)
$RetailBase=[IO.Path]::GetFullPath($RetailBase)
$base=Join-Path $Profile 'base'
New-Item -ItemType Directory "$base/maps","$base/materials" -Force | Out-Null
Copy-Item "$PSScriptRoot/rw_movement_track.map" "$base/maps/"
Copy-Item "$PSScriptRoot/../portalgun/tests/portal_lab.mtr" "$base/materials/"
foreach($mode in @(3,4)) {
    @"
set g_portalGun 1
set g_bunnyHop $mode
set g_halfLifeAutoHop 0
set in_alwaysRun 0
dmap maps/rw_movement_track.map
map rw_movement_track
wait 100
set developer 1
god
notarget
set com_fpsTrace 1
set g_movementTrace 1
setviewpos -7000 0 68 0
portalGun velocity 0 0 0
wait 60
set g_movementTraceTag JUMP
wait 15
set g_presentationTestUp 127
wait 20
set g_presentationTestUp 0
wait 180
set g_movementTraceTag ""
setviewpos -7000 0 68 0
portalGun velocity 0 0 0
wait 60
set g_movementTraceTag RUN
wait 15
set g_presentationTestForward 127
wait 270
set g_presentationTestForward 0
wait 270
set g_movementTraceTag ""
set g_halfLifeAutoHop 1
setviewpos -7800 0 68 0
portalGun velocity 0 0 0
wait 60
set g_movementTraceTag HOP
wait 15
set g_presentationTestForward 127
set g_presentationTestUp 127
wait 900
set g_presentationTestUp 0
set g_presentationTestForward 0
wait 120
set g_movementTraceTag ""
setviewpos 7800 0 68 0
portalGun velocity 0 0 0
wait 60
set g_movementTraceTag BACK
wait 15
set g_presentationTestForward -127
set g_presentationTestUp 127
wait 900
set g_presentationTestUp 0
set g_presentationTestForward 0
wait 120
set g_movementTraceTag ""
set g_halfLifeAutoHop 0
setviewpos -7000 0 230 0
portalGun velocity 400 0 0
set g_movementTraceTag STRAFE
set g_presentationTestRight 127
wait 135
set g_presentationTestRight 0
wait 180
set g_movementTraceTag ""
setviewpos -7000 0 68 0
portalGun velocity 0 0 0
wait 60
portalGun velocity 700 0 0
set g_movementTraceTag FAST
set g_presentationTestUp 127
wait 30
set g_presentationTestUp 0
wait 240
set g_movementTraceTag ""
setviewpos -7000 0 68 0
portalGun velocity 0 0 0
wait 60
portalGun velocity 300 0 0
set g_movementTraceTag JSTRAFE
set g_presentationTestRight 127
set g_presentationTestUp 127
wait 20
set g_presentationTestUp 0
wait 150
set g_presentationTestRight 0
wait 120
set g_movementTraceTag ""
quit
"@ | Set-Content "$base/test.cfg" -Encoding ascii
    $arguments='+set fs_basepath "'+$Engine+'" +set fs_cdpath "'+(Split-Path $RetailBase -Parent)+'" +set fs_devpath "'+$Profile+'" +set fs_savepath "'+$Profile+'" +set fs_configpath "'+$Profile+'" +set fs_game "" +set r_fullscreen 0 +set r_fullscreenDesktop 0 +set r_mode -1 +set r_customWidth 960 +set r_customHeight 540 +set r_multiSamples 0 +set s_volume_dB -60 +set com_unlockedFPS 0 +set com_fixedTic 1 +set developer 1 +set ai_disable 1 +exec test.cfg'
    $process=Start-Process (Join-Path $Engine 'prey06.exe') -WorkingDirectory $Engine -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if(!$process.WaitForExit(120000)){Stop-Process -Id $process.Id;throw "Mode $mode timed out"}
    if($process.ExitCode -ne 0){throw "Mode $mode exited $($process.ExitCode)"}
    Move-Item "$base/movement-trace.txt" "$base/trace$mode.txt" -Force
    Write-Output "Recorded mode $mode"
}
