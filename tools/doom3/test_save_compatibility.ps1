param(
    [Parameter(Mandatory=$true)][string]$Engine,
    [Parameter(Mandatory=$true)][string]$PreyAssets,
    [Parameter(Mandatory=$true)][string]$CampaignSave,
    [Parameter(Mandatory=$true)][string]$Output
)
# Hidden, muted campaign-save round trip. Supply a stationary ordinary campaign
# save with the Hunter Rifle owned. Never use the player's profile as Output.
$ErrorActionPreference='Stop'
$enginePath=(Resolve-Path -LiteralPath $Engine).Path
$assetPath=(Resolve-Path -LiteralPath $PreyAssets).Path
$seed=(Resolve-Path -LiteralPath $CampaignSave).Path
$profile=[IO.Path]::GetFullPath($Output)
if(Test-Path -LiteralPath $profile){throw 'Output must be a new isolated directory'}
if(!(Test-Path -LiteralPath "$enginePath/base/doom3-import-manifest.json")){throw 'Import save-compatible assets first'}
$base=Join-Path $profile 'base'
New-Item -ItemType Directory -Path "$base/savegames" -Force | Out-Null
Copy-Item -LiteralPath $seed -Destination "$base/savegames/compatibility_seed.save"
@"
set g_doom3Shotgun 1
loadGame compatibility_seed
wait 120
weaponPackInfo 8
wait 120
weaponPackInfo
saveGame compatibility_on
set g_doom3Shotgun 0
loadGame compatibility_on
wait 120
weaponPackInfo
saveGame compatibility_off
set g_doom3Shotgun 1
loadGame compatibility_off
wait 120
weaponPackInfo 8
wait 120
weaponPackInfo
condump compatibility.log
quit
"@ | Set-Content -LiteralPath "$base/compatibility.cfg"
$arguments='+set fs_basepath "'+$enginePath+'" +set fs_cdpath "'+$assetPath+'" +set fs_savepath "'+$profile+'" +set fs_configpath "'+$profile+'" +set fs_game "" +set r_fullscreen 0 +set r_fullscreenDesktop 0 +set r_mode -1 +set r_customWidth 960 +set r_customHeight 540 +set r_multiSamples 0 +set s_volume_dB -60 +set com_unlockedFPS 0 +set com_fixedTic 1 +set developer 1 +set ai_disable 1 +set g_weaponPackTrace 1 +exec compatibility.cfg'
$process=Start-Process -FilePath "$enginePath/prey06.exe" -WorkingDirectory $enginePath -ArgumentList $arguments -WindowStyle Hidden -PassThru
try {
    if(!$process.WaitForExit(60000)){throw 'Compatibility test timed out'}
    if($process.ExitCode -ne 0){throw "Game exited with $($process.ExitCode)"}
    $log=(Get-Content -LiteralPath "$base/compatibility.log" -Raw).Replace("`r",'').Replace("`n",'')
    if($log.Contains("ERROR:") -or $log.Contains("Real Script checksum didn't match")){throw 'Save restoration reported an error'}
    $pattern='WEAPONPACK STATE enabled=(\d) owned=(\d) held=(\d) clip=(-?\d+) rifleAmmo=(-?\d+) current=(-?\d+) ideal=(-?\d+) health=(-?\d+) origin=([-\d. ]+)'
    $states=[regex]::Matches($log,$pattern)
    if($states.Count -ne 5){throw "Expected 5 state records, got $($states.Count)"}
    $on=$states[1]; $off=$states[2]; $restored=$states[4]
    if($on.Groups[6].Value -ne '8' -or $off.Groups[3].Value -ne '0' -or $off.Groups[6].Value -eq '8' -or $restored.Groups[6].Value -ne '8'){throw 'Weapon mode switching failed'}
    foreach($field in 4,5,8,9) {
        if($on.Groups[$field].Value.Trim() -ne $off.Groups[$field].Value.Trim() -or $on.Groups[$field].Value.Trim() -ne $restored.Groups[$field].Value.Trim()){throw "State field $field changed across the round trip"}
    }
    'PASS: save/load modes preserve magazine, ammo, health and position.'
} finally {
    if(!$process.HasExited){Stop-Process -Id $process.Id}
}
