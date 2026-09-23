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
if(!(Test-Path -LiteralPath "$enginePath/base/def/doom3_supershotgun.def")){throw 'Import save-compatible assets first'}
$base=Join-Path $profile 'base'
New-Item -ItemType Directory -Path "$base/savegames" -Force | Out-Null
Copy-Item -LiteralPath $seed -Destination "$base/savegames/seed.save"
@'
set g_doom3Shotgun 1
loadGame seed
wait 1
god
wait 120
give weapon_hiderweapon
give weapon_soulstripper
wait 100
weaponAmmoInfo seed 0 26 0
weaponPackInfo 8
wait 150
weaponAmmoInfo reload
wait 300
weaponPackInfo 15
wait 150
weaponAmmoInfo reload
wait 200
superShotgunInfo
weaponAmmoInfo seed 0 12 0
set com_fpsTestAttack 1
wait 1000
set com_fpsTestAttack 0
wait 150
superShotgunInfo
weaponAmmoInfo reload
wait 100
superShotgunInfo
saveGame shared
loadGame shared
wait 150
superShotgunInfo
weaponAmmoInfo pickup 30
superShotgunInfo
plasmaInfo seed 0 0
plasmaInfo pickup 4
superShotgunInfo
set g_doom3Shotgun 0
wait 100
set g_doom3Shotgun 1
wait 100
superShotgunInfo
condump plasma.log
quit
'@ | Set-Content -LiteralPath "$base/test.cfg"
$arguments='+set fs_basepath "'+$enginePath+'" +set fs_cdpath "'+$assetPath+'" +set fs_savepath "'+$profile+'" +set fs_configpath "'+$profile+'" +set fs_game "" +set r_fullscreen 0 +set r_fullscreenDesktop 0 +set r_mode -1 +set r_customWidth 960 +set r_customHeight 540 +set r_multiSamples 0 +set s_volume_dB -60 +set com_unlockedFPS 0 +set com_fixedTic 1 +set developer 1 +set ai_disable 1 +exec test.cfg'
$process=Start-Process -FilePath "$enginePath/prey06.exe" -WorkingDirectory $enginePath -ArgumentList $arguments -WindowStyle Hidden -PassThru
try {
    if(!$process.WaitForExit(60000)){throw 'Super Shotgun test timed out'}
    if($process.ExitCode -ne 0){throw "Game exited with $($process.ExitCode)"}
    $log=(Get-Content -LiteralPath "$base/plasma.log" -Raw).Replace("`r",'').Replace("`n",'')
    if($log.Contains('ERROR:') -or $log.Contains("Real Script checksum didn't match") -or $log.Contains('WARNING: script')){throw 'Game reported a script/save error'}
    $states=[regex]::Matches($log,'SHAREDSHELLS total=(\d+) reserve=(\d+) shotgun=(-?\d+) super=(-?\d+) availableSG=(\d+) availableSSG=(\d+) capacity=(\d+) legacy=(\d+)')
    if($states.Count -ne 7){throw "Expected 7 shared-shell records, got $($states.Count)"}
    $expected=@(@(26,16,8,2,24,18,26,0),@(8,0,8,0,8,0,24,0),@(8,0,8,0,8,0,24,0),@(8,0,8,0,8,0,24,0),@(10,2,8,0,10,2,24,0),@(10,2,8,0,10,2,24,0),@(10,2,8,0,10,2,24,0))
    for($i=0;$i -lt $expected.Count;$i++) {
        for($j=0;$j -lt 8;$j++) {
            if([int]$states[$i].Groups[$j+1].Value -ne $expected[$i][$j]){throw "Wrong shared ammo at record $i field $j : $($states[$i].Value)"}
        }
    }
    if($log -notmatch 'PLASMAAMMO acid=2 cells=40'){throw 'Acid/Plasma half shares failed'}
    'PASS: 16 loose shells plus 8/2 private magazines, isolated firing/reload, save/load, unchanged rifle shell supply, acid exclusion, and mode toggles.'
} finally {
    if(!$process.HasExited){Stop-Process -Id $process.Id}
}
