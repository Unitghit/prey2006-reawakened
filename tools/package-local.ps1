# Builds a LOCAL TEST package in the release layout:
#   <Output>\Prey2006 Reawakened Launcher.exe
#   <Output>\engine\            engine binaries and base\ content (no retail archives)
#   <Output>\engine\importer\   bundled content converter (tools/importer/build-importer.ps1)
#   <Output>\userdata\          empty; saves and settings go here
# The engine folder is copied from an existing build (-Engine). Content that build
# imported from Doom 3 / Portal is removed from engine\base, and shipped files it
# replaced are restored from the repository's base\, so the package runs without
# it. The launcher's setup converts that content from the player's own games.
# Without a built importer, the removed content is staged in setup-content\ for
# setup to install instead (-StageContent forces this). LOCAL TESTING ONLY: never
# commit or publish the output. Retail Prey data is not included; setup imports it
# from the player's own installation.
param(
	[Parameter(Mandatory)][string]$Engine,
	[string]$Output = '',
	[string]$Launcher = '',
	[string]$Importer = '',
	[switch]$StageContent
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not $Output) { $Output = Join-Path $projectRoot 'output/local-test' }
if (-not $Launcher) { $Launcher = Join-Path $projectRoot 'PreyConfigurator/native/build/Release/Prey2006 Reawakened Launcher.exe' }
if (-not $Importer) { $Importer = Join-Path $projectRoot 'build/importer/reawakened-import' }
$Engine = (Resolve-Path -LiteralPath $Engine).Path
foreach ($required in 'prey06.exe', 'gamex86_64.dll', 'SDL2.dll', 'OpenAL32.dll', 'base') {
	if (-not (Test-Path -LiteralPath (Join-Path $Engine $required))) { throw "Engine folder is missing $required" }
}
if (-not (Test-Path -LiteralPath $Launcher)) { throw "Launcher not built: $Launcher" }
$haveImporter = Test-Path -LiteralPath (Join-Path $Importer 'reawakened-import.exe')
$stage = $StageContent -or -not $haveImporter

if (Test-Path -LiteralPath $Output) {
	# Keep an existing test installation's retail import and user data between rebuilds.
	# Optional content is removed; setup installs it again.
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

foreach ($file in 'prey06.exe', 'gamex86_64.dll', 'SDL2.dll', 'OpenAL32.dll', 'gamecontrollerdb.txt') {
	if (Test-Path -LiteralPath (Join-Path $Engine $file)) { Copy-Item -LiteralPath (Join-Path $Engine $file) -Destination $engineOut -Force }
}
$sourceBase = Join-Path $Engine 'base'
Get-ChildItem -LiteralPath $sourceBase -Force | Where-Object {
	# Retail archives come from first-run setup, never from a build folder.
	$_.Name -notmatch '^pak00[0-6]\.pk4$' -and $_.Name -notmatch '\.(pdb|part)$' -and $_.Name -notin 'texcache', 'savegames', 'diagnostics', 'preload', 'maps' -and
	$_.Name -notmatch '^reawakened-(doom3|portal)-files\.txt$'
} | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $baseOut -Recurse -Force }

# Remove the optional content from the engine (staging it when there is no importer),
# restoring any shipped file it had replaced.
. (Join-Path $PSScriptRoot 'optional-content.ps1')
$repoBase = Join-Path $projectRoot 'base'
$removed = @{}; $restored = 0
foreach ($game in 'doom3', 'portal') {
	$files = @(Get-OptionalContent -Base $sourceBase -Game $game)
	foreach ($relative in $files) {
		if ($stage) {
			$dest = Join-Path (Join-Path (Join-Path $Output 'setup-content') $game) $relative
			New-Item -ItemType Directory -Force (Split-Path $dest -Parent) | Out-Null
			Copy-Item -LiteralPath (Join-Path $sourceBase $relative) -Destination $dest -Force
		}
		Remove-Item -LiteralPath (Join-Path $baseOut $relative) -Force -ErrorAction SilentlyContinue
		$shipped = Join-Path $repoBase $relative
		if (Test-Path -LiteralPath $shipped) {
			Copy-Item -LiteralPath $shipped -Destination (Join-Path $baseOut $relative) -Force
			$restored++
		}
	}
	$removed[$game] = $files.Count
}
# Files derived from retail Prey are never packaged: setup builds them from the
# player's own archives (reawakened-import prey). Includes the portal test-wall
# material, used only by the development test maps.
$derived = @(Get-ChildItem -LiteralPath $baseOut -Recurse -File | Where-Object {
	$relative = $_.FullName.Substring($baseOut.Length + 1).Replace('\', '/')
	$relative -match '^models/reawakened/portalgun/[^/]+$' -or $relative -match '^def/reawakened_portalgun_opening\.def$' -or
	$relative -match '^materials/reawakened_portalgun(_retail|_energy)?\.mtr$' -or
	$relative -in 'zz_reawakened_jen_seam.pk4', 'reawakened-prey-files.txt'
})
$derived | Remove-Item -Force
Get-ChildItem -LiteralPath $baseOut -Recurse -Directory | Sort-Object { $_.FullName.Length } -Descending |
	Where-Object { -not (Get-ChildItem -LiteralPath $_.FullName -Force) } | Remove-Item
if ($haveImporter -and -not $StageContent) {
	Copy-Item -LiteralPath $Importer -Destination (Join-Path $engineOut 'importer') -Recurse -Force
}
Copy-Item -LiteralPath $Launcher -Destination (Join-Path $Output 'Prey2006 Reawakened Launcher.exe') -Force

$retail = @(0..6 | Where-Object { Test-Path -LiteralPath (Join-Path $baseOut "pak00$_.pk4") }).Count
$size = (Get-ChildItem -LiteralPath $Output -Recurse -File | Measure-Object Length -Sum).Sum / 1MB
Write-Output ("Local test package: {0} ({1:N0} MB, retail archives present: {2}/7)" -f $Output, $size, $retail)
Write-Output ("Optional content removed from engine: Doom 3 {0} files, Portal {1} files; {2} shipped files restored; {3} Prey-derived files left to setup" -f $removed['doom3'], $removed['portal'], $restored, $derived.Count)
if ($stage) {
	Write-Output 'Setup installs optional content from setup-content (pre-converted). Local testing only: do not publish.'
} else {
	Write-Output 'Setup converts optional content from the player''s games with engine\importer.'
}
