param(
    [Parameter(Mandatory=$true)][string]$Engine,
    [Parameter(Mandatory=$true)][string]$RetailBase,
    [Parameter(Mandatory=$true)][string]$Profile
)
$ErrorActionPreference='Stop'
if(Test-Path $Profile){throw 'Use a new isolated test profile'}
$Engine=[IO.Path]::GetFullPath($Engine)
$Profile=[IO.Path]::GetFullPath($Profile)
$RetailBase=[IO.Path]::GetFullPath($RetailBase)
$base=Join-Path $Profile 'base'
New-Item -ItemType Directory "$base/maps","$base/materials" -Force | Out-Null
Copy-Item "$PSScriptRoot/../portalgun/tests/rw_portal_lab.map" "$base/maps/"
Copy-Item "$PSScriptRoot/../portalgun/tests/portal_lab.mtr" "$base/materials/"
foreach($mode in @(0,3,4)) {
    @"
set g_portalGun 1
set g_bunnyHop $mode
set g_halfLifeAutoHop 0
set in_alwaysRun 0
dmap maps/rw_portal_lab.map
map rw_portal_lab
wait 100
set developer 1
portalGun lab
set com_fpsTrace 1
set g_movementTrace 1
echo STATIONARY_BEGIN
setviewpos 0 0 68 0
portalGun velocity 0 0 0
wait 60
set g_presentationTestUp 127
wait 600
set g_presentationTestUp 0
echo STATIONARY_END
condump movement${mode}_STATIONARY.log
wait 60
echo AUTO_BEGIN
set g_halfLifeAutoHop 1
set g_presentationTestUp 127
wait 600
set g_presentationTestUp 0
echo AUTO_END
condump movement${mode}_AUTO.log
wait 200
echo BOOST_BEGIN
setviewpos -400 0 68 0
portalGun velocity 0 0 0
set g_presentationTestForward 127
wait 100
set g_presentationTestUp 127
wait 130
set g_presentationTestUp 0
set g_presentationTestForward 0
echo BOOST_END
condump movement${mode}_BOOST.log
wait 200
echo AIR_BEGIN
setviewpos 0 0 230 0
portalGun velocity 400 0 0
set g_presentationTestRight 127
wait 45
set g_presentationTestRight 0
echo AIR_END
condump movement${mode}_AIR.log
wait 200
echo FRICTION_BEGIN
setviewpos 0 0 68 0
portalGun velocity 320 0 0
wait 150
echo FRICTION_END
condump movement${mode}_FRICTION.log
echo SIDEWAYS_BEGIN
setviewpos 0 0 140 0
portalGun velocity 0 0 0
fpsTestView sideways
wait 150
echo SIDEWAYS_END
condump movement${mode}_SIDEWAYS.log
fpsTestView down
setviewpos 0 0 68 0
portalGun velocity 0 0 0
wait 100
saveGame movement
loadGame movement
wait 100
echo RESTORE_BEGIN
set g_bunnyHop 0
set g_presentationTestUp 127
wait 450
set g_presentationTestUp 0
echo RESTORE_END
condump movement${mode}_RESTORE.log
set g_bunnyHop $mode
wait 100
echo PORTAL_BEGIN
setviewpos -256 0 68 0
wait 30
portalGun aim 90 0
portalGun blue
wait 100
setviewpos 0 0 68 0
portalGun aim 0 0
portalGun orange
wait 100
setviewpos -256 0 230 0
portalGun velocity 0 0 0
wait 500
echo PORTAL_END
condump movement$mode.log
quit
"@ | Set-Content "$base/test.cfg" -Encoding ascii
    $arguments='+set fs_basepath "'+$Engine+'" +set fs_cdpath "'+(Split-Path $RetailBase -Parent)+'" +set fs_devpath "'+$Profile+'" +set fs_savepath "'+$Profile+'" +set fs_configpath "'+$Profile+'" +set fs_game "" +set r_fullscreen 0 +set r_fullscreenDesktop 0 +set r_mode -1 +set r_customWidth 960 +set r_customHeight 540 +set r_multiSamples 0 +set s_volume_dB -60 +set com_unlockedFPS 0 +set com_fixedTic 1 +set developer 1 +set ai_disable 1 +set logfile 2 +exec test.cfg'
    $process=Start-Process (Join-Path $Engine 'prey06.exe') -WorkingDirectory $Engine -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if(!$process.WaitForExit(60000)){Stop-Process -Id $process.Id;throw "Mode $mode timed out"}
    if($process.ExitCode -ne 0){throw "Mode $mode exited $($process.ExitCode)"}
    $log=Get-Content "$base/movement$mode.log" -Raw
    if($log -match 'ERROR:|shutting down:|Unknown command'){throw "Mode $mode reported an error"}
    Write-Output "Recorded mode $mode"
}
python "$PSScriptRoot/check-halflife.py" $base
if($LASTEXITCODE -ne 0){throw 'Movement assertions failed'}
