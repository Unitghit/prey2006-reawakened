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
if(!(Test-Path -LiteralPath "$enginePath/base/def/doom3_chaingun.def")){throw 'Import save-compatible assets first'}
$base=Join-Path $profile 'base'
New-Item -ItemType Directory -Path "$base/savegames" -Force | Out-Null
Copy-Item -LiteralPath $seed -Destination "$base/savegames/seed.save"
@'
set g_doom3Shotgun 1
loadGame seed
wait 120
give weapon_autocannon
wait 100
chaingunInfo seed 0 0
chaingunInfo pickup 1
chaingunInfo pickup 1
saveGame fraction
loadGame fraction
wait 120
chaingunInfo
chaingunInfo pickup 2
chaingunInfo seed 200 0
chaingunInfo pickup 40
chaingunInfo seed 0 300
chaingunInfo pickup 40
chaingunInfo seed 200 300
chaingunInfo pickup 1
chaingunInfo altpickup 1
chaingunInfo seed 100 120
weaponPackInfo 11
wait 150
set com_fpsTestAttack 1
wait 150
set com_fpsTestAttack 0
wait 100
chaingunInfo
weaponAmmoInfo reload
wait 400
chaingunInfo
saveGame equipped
loadGame equipped
wait 150
chaingunInfo
set g_doom3Shotgun 0
wait 100
chaingunInfo
saveGame disabled
loadGame disabled
wait 150
chaingunInfo
set g_doom3Shotgun 1
wait 100
chaingunInfo
condump chaingun.log
quit
'@ | Set-Content -LiteralPath "$base/test.cfg"
$arguments='+set fs_basepath "'+$enginePath+'" +set fs_cdpath "'+$assetPath+'" +set fs_savepath "'+$profile+'" +set fs_configpath "'+$profile+'" +set fs_game "" +set r_fullscreen 0 +set r_fullscreenDesktop 0 +set r_mode -1 +set r_customWidth 960 +set r_customHeight 540 +set r_multiSamples 0 +set s_volume_dB -60 +set com_unlockedFPS 0 +set com_fixedTic 1 +set developer 1 +set ai_disable 1 +exec test.cfg'
$process=Start-Process -FilePath "$enginePath/prey06.exe" -WorkingDirectory $enginePath -ArgumentList $arguments -WindowStyle Hidden -PassThru
try {
    if(!$process.WaitForExit(60000)){throw 'Chaingun test timed out'}
    if($process.ExitCode -ne 0){throw "Game exited with $($process.ExitCode)"}
    $log=(Get-Content -LiteralPath "$base/chaingun.log" -Raw).Replace("`r",'').Replace("`n",'')
    if($log.Contains('ERROR:') -or $log.Contains("Real Script checksum didn't match") -or $log.Contains('WARNING: script')){throw 'Game reported a script/save error'}
    $states=[regex]::Matches($log,'CHAINAMMO auto=(\d+) belt=(\d+) autoMax=(\d+) beltMax=(\d+) autoFraction=([0-9.eE+-]+) beltFraction=([0-9.eE+-]+) clip=(-?\d+) held=(\d+) alt=(\d+)')
    if($states.Count -ne 19){throw "Expected 19 ammo records, got $($states.Count)"}
    $expected=@(@(0,0),@(0,0),@(1,1),@(1,1),@(2,3),@(200,0),@(200,30),@(0,300),@(20,300),@(200,300),@(200,300),@(200,300),@(100,120))
    for($i=0;$i -lt $expected.Count;$i++) {
        if([int]$states[$i].Groups[1].Value -ne $expected[$i][0] -or [int]$states[$i].Groups[2].Value -ne $expected[$i][1]){throw "Wrong allocation at $i : $($states[$i].Value)"}
    }
    foreach($idx in 2,3){if($states[$idx].Groups[6].Value -ne '0.5'){throw 'Fraction lost across save/load'}}
    if([regex]::Matches($log,'CHAINPICKUP accepted=0').Count -ne 1){throw 'Full reserve accepted supply'}
    $alt=[int]$states[0].Groups[9].Value
    for($i=0;$i -le 10;$i++){if([int]$states[$i].Groups[9].Value -ne $alt){throw 'Primary pickup changed grenades'}}
    if([int]$states[11].Groups[9].Value -ne $alt+1){throw 'Grenade pickup was split'}
    $remaining=[int]$states[13].Groups[2].Value
    if($remaining -ge 120 -or $remaining -lt 60){throw 'Chaingun did not fire as expected'}
    for($i=13;$i -lt 19;$i++) {
        if([int]$states[$i].Groups[1].Value -ne 100 -or [int]$states[$i].Groups[2].Value -ne $remaining){throw 'Firing/reload/toggle changed independent reserves'}
        if([int]$states[$i].Groups[9].Value -ne $alt+1){throw 'Weapon consumed grenades'}
        if($i -ge 14 -and [int]$states[$i].Groups[7].Value -ne 60){throw 'Reloaded magazine was not preserved'}
    }
    foreach($idx in 16,17){if($states[$idx].Groups[8].Value -ne '0'){throw 'Disabled gun remained selectable'}}
    if($states[18].Groups[8].Value -ne '1'){throw 'Autocannon ownership did not restore Chaingun'}
    'PASS: slot-5 independent reserves, fractions, fixed halves, grenades, firing/reload and equipped/disabled saves.'
} finally {
    if(!$process.HasExited){Stop-Process -Id $process.Id}
}
