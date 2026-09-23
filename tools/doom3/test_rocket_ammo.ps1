param(
    [Parameter(Mandatory=$true)][string]$Engine,
    [Parameter(Mandatory=$true)][string]$PreyAssets,
    [Parameter(Mandatory=$true)][string]$CampaignSave,
    [Parameter(Mandatory=$true)][string]$Output
)
# Stationary campaign seed; never run in the player's profile.
$ErrorActionPreference='Stop'
$enginePath=(Resolve-Path -LiteralPath $Engine).Path
$assetPath=(Resolve-Path -LiteralPath $PreyAssets).Path
$seed=(Resolve-Path -LiteralPath $CampaignSave).Path
$profile=[IO.Path]::GetFullPath($Output)
if(Test-Path -LiteralPath $profile){throw 'Output must be a new isolated directory'}
if(!(Test-Path -LiteralPath "$enginePath/base/def/doom3_rocketlauncher.def")){throw 'Import save-compatible assets first'}
$base=Join-Path $profile 'base'
New-Item -ItemType Directory -Path "$base/savegames" -Force | Out-Null
Copy-Item -LiteralPath $seed -Destination "$base/savegames/seed.save"
@'
set g_doom3Shotgun 1
loadGame seed
wait 120
give weapon_rocketlauncher
wait 100
rocketInfo seed 0 0
rocketInfo pickup 1
saveGame fraction
loadGame fraction
wait 120
rocketInfo
rocketInfo pickup 1
rocketInfo pickup 2
rocketInfo seed 12 0
rocketInfo pickup 4
rocketInfo seed 0 12
rocketInfo pickup 4
rocketInfo seed 12 12
rocketInfo pickup 1
rocketInfo
rocketInfo seed 10 12
weaponPackInfo 14
wait 150
set com_fpsTestAttack 1
wait 150
set com_fpsTestAttack 0
wait 100
rocketInfo
weaponAmmoInfo reload
wait 1000
rocketInfo
saveGame equipped
loadGame equipped
wait 150
rocketInfo
set g_doom3Shotgun 0
wait 100
rocketInfo
saveGame disabled
loadGame disabled
wait 150
rocketInfo
set g_doom3Shotgun 1
wait 100
rocketInfo
condump rocket.log
quit
'@ | Set-Content -LiteralPath "$base/test.cfg"
$arguments='+set fs_basepath "'+$enginePath+'" +set fs_cdpath "'+$assetPath+'" +set fs_savepath "'+$profile+'" +set fs_configpath "'+$profile+'" +set fs_game "" +set r_fullscreen 0 +set r_fullscreenDesktop 0 +set r_mode -1 +set r_customWidth 960 +set r_customHeight 540 +set r_multiSamples 0 +set s_volume_dB -60 +set com_unlockedFPS 0 +set com_fixedTic 1 +set developer 1 +set ai_disable 1 +exec test.cfg'
$process=Start-Process -FilePath "$enginePath/prey06.exe" -WorkingDirectory $enginePath -ArgumentList $arguments -WindowStyle Hidden -PassThru
try {
    if(!$process.WaitForExit(60000)){throw 'Rocket Launcher test timed out'}
    if($process.ExitCode -ne 0){throw "Game exited with $($process.ExitCode)"}
    $log=(Get-Content -LiteralPath "$base/rocket.log" -Raw).Replace("`r",'').Replace("`n",'')
    if($log.Contains('ERROR:') -or $log.Contains("Real Script checksum didn't match") -or $log.Contains('WARNING: script')){throw 'Game reported a script/save error'}
    $states=[regex]::Matches($log,'ROCKETAMMO prey=(\d+) rockets=(\d+) preyMax=(\d+) rocketMax=(\d+) preyFraction=([0-9.eE+-]+) rocketFraction=([0-9.eE+-]+) clip=(-?\d+) held=(\d+) alt=(\d+)')
    if($states.Count -ne 19){throw "Expected 19 ammo records, got $($states.Count)"}
    $expected=@(@(0,0),@(0,0),@(0,0),@(1,1),@(2,2),@(12,0),@(12,2),@(0,12),@(2,12),@(12,12),@(12,12),@(12,12),@(10,12))
    for($i=0;$i -lt $expected.Count;$i++) {
        if([int]$states[$i].Groups[1].Value -ne $expected[$i][0] -or [int]$states[$i].Groups[2].Value -ne $expected[$i][1]){throw "Wrong allocation at $i : $($states[$i].Value)"}
    }
    foreach($idx in 1,2){if($states[$idx].Groups[5].Value -ne '0.5'){throw 'Fraction lost across save/load'}}
    if([regex]::Matches($log,'ROCKETPICKUP accepted=0').Count -ne 1){throw 'Full reserve accepted supply'}
    $alt=[int]$states[0].Groups[9].Value
    for($i=0;$i -le 10;$i++){if([int]$states[$i].Groups[9].Value -ne $alt){throw 'Primary pickup changed grenades'}}
    if([int]$states[11].Groups[9].Value -ne $alt){throw 'Unrelated ammo changed'}
    $remaining=[int]$states[13].Groups[2].Value
    if($remaining -ge 12 -or $remaining -lt 5){throw 'Rocket Launcher did not fire as expected'}
    for($i=13;$i -lt 19;$i++) {
        if([int]$states[$i].Groups[1].Value -ne 10 -or [int]$states[$i].Groups[2].Value -ne $remaining){throw 'Firing/reload/toggle changed independent reserves'}
        if([int]$states[$i].Groups[9].Value -ne $alt){throw 'Weapon consumed grenades'}
        if($i -ge 14 -and [int]$states[$i].Groups[7].Value -ne 5){throw 'Reloaded magazine was not preserved'}
    }
    foreach($idx in 16,17){if($states[$idx].Groups[8].Value -ne '0'){throw 'Disabled gun remained selectable'}}
    if($states[18].Groups[8].Value -ne '1'){throw 'Prey Rocket Launcher ownership did not restore Rocket Launcher'}
    'PASS: slot-7 independent reserves, fractions, fixed halves, grenades, firing/reload and equipped/disabled saves.'
} finally {
    if(!$process.HasExited){Stop-Process -Id $process.Id}
}
