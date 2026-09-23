param(
    [Parameter(Mandatory=$true)][string]$Engine,
    [Parameter(Mandatory=$true)][string]$RetailBase,
    [Parameter(Mandatory=$true)][string]$Profile
)
$ErrorActionPreference='Stop'
$Engine=(Resolve-Path -LiteralPath $Engine).Path
$RetailBase=(Resolve-Path -LiteralPath $RetailBase).Path
$Profile=[IO.Path]::GetFullPath($Profile)
if(Test-Path -LiteralPath $Profile){throw 'Use a new isolated profile directory.'}
$base=Join-Path $Profile 'base'
New-Item -ItemType Directory -Path "$base/maps","$base/materials" -Force | Out-Null
Copy-Item "$PSScriptRoot/tests/rw_portal_lab.map" "$base/maps/"
Copy-Item "$PSScriptRoot/tests/portal_lab.mtr" "$base/materials/"
foreach($name in @('input','regression','floor','ceiling','blocked','replacement')) {
    Copy-Item "$PSScriptRoot/tests/$name.cfg" "$base/test.cfg" -Force
    $arguments='+set fs_basepath "'+$Engine+'" +set fs_cdpath "'+(Split-Path $RetailBase -Parent)+'" +set fs_devpath "'+$Profile+'" +set fs_savepath "'+$Profile+'" +set fs_configpath "'+$Profile+'" +set fs_game "" +set r_fullscreen 0 +set r_fullscreenDesktop 0 +set r_mode -1 +set r_customWidth 960 +set r_customHeight 540 +set r_multiSamples 0 +set s_volume_dB -60 +set com_unlockedFPS 0 +set com_fixedTic 1 +set r_gammaInShader 1 +set developer 1 +set ai_disable 1 +set logfile 2 +exec test.cfg'
    $process=Start-Process (Join-Path $Engine 'prey06.exe') -WorkingDirectory $Engine -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if(!$process.WaitForExit(60000)){Stop-Process -Id $process.Id; throw "$name timed out"}
    if($process.ExitCode -ne 0){throw "$name exited with $($process.ExitCode)"}
    $log=Get-Content -LiteralPath "$base/$name.log" -Raw
    if($log -match 'ERROR:|shutting down:'){throw "$name reported an engine error"}
    if($name -eq 'input' -and ($log -notmatch 'PORTALGUN placed blue' -or $log -notmatch 'PORTALGUN placed orange')){throw 'Input placement failed'}
    if($name -in @('regression','floor','ceiling') -and $log -notmatch 'PORTAL_EXIT[\s\S]{0,100}collision_adjustment 0\.000000'){throw "$name failed continuous traversal"}
    if($name -eq 'regression' -and $log -notmatch 'origin=495\.75 100'){throw 'Solid-wall control failed'}
    if($name -in @('floor','ceiling') -and $log -notmatch 'PORTAL_EXIT[^\r\n]*speed 596\.960'){throw "$name lost fall velocity"}
    if($name -eq 'blocked' -and ($log -notmatch 'PORTALGUN blocked exit' -or $log -notmatch 'ROCKETTARGET name=portalblock health=100')){throw 'Blocked-exit protection failed'}
    if($name -eq 'replacement') {
        $log=$log -replace '\r?\n',' ' -replace '(?<![0-9])-\s*0(?![0-9.])','0'
        $log=$log -replace ' +',' '
        if(([regex]::Matches($log,'PORTALGUN placed blue')).Count -ne 3 -or ([regex]::Matches($log,'PORTALGUN placed orange')).Count -ne 2){throw 'Repeated replacement failed'}
        if($log -match 'PORTALGUN rejected:'){throw 'Replacement rejected unexpectedly'}
        if($log -notmatch 'rw_gun_orange origin=-256 128 1 normal=0 0 1 up=1 0 0'){throw 'Vertical floor placement orientation failed'}
        if($log -notmatch 'normal=0 0 1 up=0 1 0'){throw 'Angled floor placement orientation failed'}
        if($log -notmatch 'PORTAL_EXIT[\s\S]{0,100}collision_adjustment 0\.000000'){throw 'Replaced pair failed traversal after reload'}
    }
    Write-Output "PASS $name"
}
