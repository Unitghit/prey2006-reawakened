param([int]$Jobs=8)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$stage=Join-Path $projectRoot 'output/private-preview'
function Check([string]$Step) { if($LASTEXITCODE -ne 0){throw "$Step failed"} }
cmake -S "$projectRoot/neo" -B "$projectRoot/build/engine" -A x64 -DTOOLS=OFF -DDEDICATED=OFF "-DPREY_OUTPUT_ROOT=$stage/engine"
Check 'Engine configure'
cmake --build "$projectRoot/build/engine" --config Release --parallel $Jobs
Check 'Engine build'
cmake -S "$projectRoot/PreyConfigurator/native" -B "$projectRoot/PreyConfigurator/native/build" -A x64
Check 'Settings configure'
cmake --build "$projectRoot/PreyConfigurator/native/build" --config Release --parallel $Jobs
Check 'Settings build'
$settings=Join-Path $projectRoot 'PreyConfigurator/native/build/Release/Prey2006 Reawakened Launcher.exe'
$verify=Start-Process -FilePath $settings -ArgumentList ('--root "'+$projectRoot+'" --verify') -WindowStyle Hidden -PassThru -Wait
if($verify.ExitCode -ne 0){throw 'Settings verification failed'}
Copy-Item -LiteralPath $settings -Destination "$stage/Prey2006 Reawakened Launcher.exe" -Force
# Copy only runtime libraries built from this source checkout.
Copy-Item -LiteralPath "$projectRoot/neo/libs/SDL_GameControllerDB/gamecontrollerdb.txt" -Destination "$stage/engine/gamecontrollerdb.txt" -Force
foreach($spec in @(@('SDL2.dll','libs/SDL2/Release/SDL2.dll'),@('OpenAL32.dll','libs/OpenalSoft/Release/OpenAL32.dll'))) {
    Copy-Item -LiteralPath (Join-Path "$projectRoot/build/engine" $spec[1]) -Destination (Join-Path "$stage/engine" $spec[0]) -Force
}
Write-Output "Private test build: $stage"
Write-Output 'No retail assets included. Import a local PC installation before launching.'
