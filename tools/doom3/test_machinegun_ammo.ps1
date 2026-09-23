param(
    [Parameter(Mandatory=$true)][string]$Engine,
    [Parameter(Mandatory=$true)][string]$PreyAssets,
    [Parameter(Mandatory=$true)][string]$CampaignSave,
    [Parameter(Mandatory=$true)][string]$Output
)
# Hidden, muted independent-ammunition regression. Supply a stationary ordinary campaign
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
give weapon_soulstripper
wait 30
weaponAmmoInfo
weaponAmmoInfo norecharge
weaponAmmoInfo seed 0 0 0
weaponAmmoInfo pickup 5
weaponAmmoInfo pickup 5
weaponAmmoInfo pickup 5
weaponAmmoInfo useRifle 1
weaponAmmoInfo useShells 1
weaponAmmoInfo useBullets 1
saveGame ammo_fraction
loadGame ammo_fraction
wait 120
weaponAmmoInfo
weaponAmmoInfo seed 50 0 0
weaponAmmoInfo pickup 30
weaponAmmoInfo seed 0 16 0
weaponAmmoInfo pickup 30
weaponAmmoInfo seed 50 16 180
weaponAmmoInfo pickup 1
set g_doom3Shotgun 0
wait 20
weaponAmmoInfo
weaponAmmoInfo pickup 75
set g_doom3Shotgun 1
wait 20
weaponAmmoInfo
weaponAmmoInfo pickup 1
set g_doom3Shotgun 0
wait 20
weaponAmmoInfo
set g_doom3Shotgun 1
wait 20
weaponAmmoInfo
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
    $pattern='AMMOPOOL rifle=(\d+) shells=(\d+) rifleMax=(\d+) shellMax=(\d+) rifleFraction=([0-9.eE+-]+)\s*shellFraction=([0-9.eE+-]+) clip=(-?\d+) split=(\d+) need=([0-9.]+)'
    $states=[regex]::Matches($log,$pattern)
    $mg=[regex]::Matches($log,'MACHINEGUN ammo=(\d+) max=(\d+) fraction=([0-9.eE+-]+) clip=(-?\d+) held=(\d+)')
    if($states.Count -ne 22 -or $mg.Count -ne 22){throw "Expected 22 ammo records; got $($states.Count)/$($mg.Count)"}
    $expected=@(@(0,0,0),@(1,0,4),@(3,0,8),@(5,1,12),@(4,1,12),@(4,0,12),@(4,0,11),@(4,0,11),@(50,0,0),@(50,2,24),@(0,16,0),@(10,16,24),@(50,16,180),@(50,16,180),@(50,16,180),@(125,16,180),@(125,16,180),@(125,16,180),@(125,16,180),@(125,16,180))
    for($i=0;$i -lt $expected.Count;$i++) {
        $state=$states[$i+2]
        if([int]$state.Groups[1].Value -ne $expected[$i][0] -or [int]$state.Groups[2].Value -ne $expected[$i][1] -or [int]$mg[$i+2].Groups[1].Value -ne $expected[$i][2]){throw "Wrong three-way totals at record $($i+2): $($state.Value) / $($mg[$i+2].Value)"}
        if([int]$state.Groups[4].Value -ne 16 -or [int]$mg[$i+2].Groups[2].Value -ne 180){throw 'Addon cap changed'}
    }
    $culture=[Globalization.CultureInfo]::InvariantCulture
    foreach($idx in 8,9) {
        $sf=[double]::Parse($states[$idx].Groups[6].Value,$culture)
        if([Math]::Abs($sf-0.0) -gt 0.000001){throw 'Fractional shells were lost during spending/save/load'}
    }
    if([regex]::Matches($log,'AMMOPICKUP accepted=0').Count -ne 2){throw 'Full reserves must reject pickup'}
    'PASS: three independent pools, fixed thirds, fractional pickups, 16-shell/180-round caps, save persistence and non-refilling mode toggles.'

} finally {
    if(!$process.HasExited){Stop-Process -Id $process.Id}
}
