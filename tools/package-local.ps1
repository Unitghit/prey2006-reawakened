# Builds a LOCAL TEST package in the release layout:
#   <Output>\Prey2006 Reawakened Launcher.exe
#   <Output>\engine\   engine binaries and base\ content (no retail archives)
#   <Output>\userdata\ empty; saves and settings go here
#   <Output>\setup-content\doom3\, portal\   optional content, installed by setup
# The engine folder is copied from an existing build (-Engine). Content that build
# imported from Doom 3 / Portal is moved out of engine\base into setup-content, so
# the package runs without it and the launcher's setup installs it only when the
# player has that game and chooses it. LOCAL TESTING ONLY: never commit or publish
# the output. Retail Prey data is not included; setup imports it from the
# player's own installation.
param(
	[Parameter(Mandatory)][string]$Engine,
	[string]$Output = '',
	[string]$Launcher = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not $Output) { $Output = Join-Path $projectRoot 'output/local-test' }
if (-not $Launcher) { $Launcher = Join-Path $projectRoot 'PreyConfigurator/native/build/Release/Prey2006 Reawakened Launcher.exe' }
$Engine = (Resolve-Path -LiteralPath $Engine).Path
foreach ($required in 'prey06.exe', 'gamex86_64.dll', 'SDL2.dll', 'OpenAL32.dll', 'base') {
	if (-not (Test-Path -LiteralPath (Join-Path $Engine $required))) { throw "Engine folder is missing $required" }
}
if (-not (Test-Path -LiteralPath $Launcher)) { throw "Launcher not built: $Launcher" }

if (Test-Path -LiteralPath $Output) {
	# Keep an existing test installation's retail import and user data between rebuilds.
	# Optional content is removed; setup installs it again from setup-content.
	Get-ChildItem -LiteralPath $Output -Force | Where-Object { $_.Name -notin 'engine', 'userdata' } | Remove-Item -Recurse -Force
	$engineOut = Join-Path $Output 'engine'
	if (Test-Path -LiteralPath $engineOut) {
		Get-ChildItem -LiteralPath $engineOut -Force | Where-Object { $_.Name -ne 'base' } | Remove-Item -Recurse -Force
		Get-ChildItem -LiteralPath (Join-Path $engineOut 'base') -Force -ErrorAction SilentlyContinue |
			Where-Object { $_.Name -notmatch '^pak00[0-6]\.pk4$' } | Remove-Item -Recurse -Force
	}
}
$engineOut = Join-Path $Output 'engine'
$baseOut = Join-Path $engineOut 'base'
New-Item -ItemType Directory -Force $baseOut, (Join-Path $Output 'userdata') | Out-Null

foreach ($file in 'prey06.exe', 'gamex86_64.dll', 'SDL2.dll', 'OpenAL32.dll') {
	Copy-Item -LiteralPath (Join-Path $Engine $file) -Destination $engineOut -Force
}
$sourceBase = Join-Path $Engine 'base'
Get-ChildItem -LiteralPath $sourceBase -Force | Where-Object {
	# Retail archives come from first-run setup, never from a build folder.
	$_.Name -notmatch '^pak00[0-6]\.pk4$' -and $_.Name -notmatch '\.(pdb|part)$' -and $_.Name -notin 'texcache', 'savegames', 'diagnostics', 'preload', 'maps' -and
	$_.Name -notmatch '^reawakened-(doom3|portal)-files\.txt$'
} | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $baseOut -Recurse -Force }

# Move the optional content out of the engine into setup-content.
. (Join-Path $PSScriptRoot 'optional-content.ps1')
$staged = @{}
foreach ($game in 'doom3', 'portal') {
	$stage = Join-Path (Join-Path $Output 'setup-content') $game
	$files = @(Get-OptionalContent -Base $sourceBase -Game $game)
	foreach ($relative in $files) {
		$dest = Join-Path $stage $relative
		New-Item -ItemType Directory -Force (Split-Path $dest -Parent) | Out-Null
		Copy-Item -LiteralPath (Join-Path $sourceBase $relative) -Destination $dest -Force
		Remove-Item -LiteralPath (Join-Path $baseOut $relative) -Force -ErrorAction SilentlyContinue
	}
	$staged[$game] = $files.Count
}
Get-ChildItem -LiteralPath $baseOut -Recurse -Directory | Sort-Object { $_.FullName.Length } -Descending |
	Where-Object { -not (Get-ChildItem -LiteralPath $_.FullName -Force) } | Remove-Item
Copy-Item -LiteralPath $Launcher -Destination (Join-Path $Output 'Prey2006 Reawakened Launcher.exe') -Force

$retail = @(0..6 | Where-Object { Test-Path -LiteralPath (Join-Path $baseOut "pak00$_.pk4") }).Count
$size = (Get-ChildItem -LiteralPath $Output -Recurse -File | Measure-Object Length -Sum).Sum / 1MB
Write-Output ("Local test package: {0} ({1:N0} MB, retail archives present: {2}/7)" -f $Output, $size, $retail)
Write-Output ("Optional content staged: Doom 3 {0} files, Portal {1} files" -f $staged['doom3'], $staged['portal'])
Write-Output 'Local testing only: contains locally imported Doom 3 / Portal content. Do not publish.'
