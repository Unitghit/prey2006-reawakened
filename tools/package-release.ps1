# Builds the release archives from a clean source build of the current commit:
#   output\release\Prey2006-Reawakened-<version>\                 portable folder
#   output\release\Prey2006-Reawakened-<version>-win64.zip         that folder, zipped
#   output\release\Prey2006-Reawakened-<version>-source.zip        git archive of HEAD
#   output\release\SHA256SUMS.txt
# Requires a committed, clean working tree (the source archive must match the
# binaries), tools\build-private.ps1 already run (or -Build), and the importer
# built with tools\importer\build-importer.ps1. The package contains no retail
# Prey, Doom 3 or Portal data and nothing derived from it; the audit at the end
# fails the build if any appears. Publishing is a separate, manual step.
param(
	[string]$Version = '',
	[switch]$Build,
	[switch]$AllowDirty
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not $Version) {
	$match = Select-String -LiteralPath (Join-Path $projectRoot 'neo/framework/Licensee.h') -Pattern 'REAWAKENED_VERSION\s+"([^"]+)"'
	if (-not $match) { throw 'REAWAKENED_VERSION not found in Licensee.h' }
	$Version = $match.Matches[0].Groups[1].Value
}
# Content changes to tracked files (line-ending-only differences and untracked
# files do not reach the source archive, which is taken from the commit).
$dirty = git -C $projectRoot diff --ignore-cr-at-eol --name-only HEAD
if ($dirty -and -not $AllowDirty) { throw "Tracked files have uncommitted changes; commit them first so the source archive matches.`n$dirty" }
$commit = (git -C $projectRoot rev-parse HEAD).Trim()

if ($Build) {
	& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'build-private.ps1')
	if ($LASTEXITCODE -ne 0) { throw 'Source build failed' }
}
$preview = Join-Path $projectRoot 'output/private-preview'
$gameDll = Join-Path $projectRoot 'build/engine/Release/gamex86_64.dll'
$importer = Join-Path $projectRoot 'build/importer/reawakened-import'
$launcher = Join-Path $preview 'Prey2006 Reawakened Launcher.exe'
foreach ($required in (Join-Path $preview 'engine/prey06.exe'), (Join-Path $preview 'engine/base/game00.pk4'),
		(Join-Path $preview 'engine/base/pak007.pk4'), $gameDll, (Join-Path $importer 'reawakened-import.exe'), $launcher) {
	if (-not (Test-Path -LiteralPath $required)) { throw "Missing build output: $required" }
}

$name = "Prey2006-Reawakened-$Version"
$releaseRoot = Join-Path $projectRoot 'output/release'
$folder = Join-Path $releaseRoot $name
if (Test-Path -LiteralPath $releaseRoot) { Remove-Item -LiteralPath $releaseRoot -Recurse -Force }
New-Item -ItemType Directory -Force $releaseRoot | Out-Null

# Engine exactly as the source build produced it, plus the game library.
$engineStage = Join-Path $projectRoot 'build/release-engine'
if (Test-Path -LiteralPath $engineStage) { Remove-Item -LiteralPath $engineStage -Recurse -Force }
New-Item -ItemType Directory -Force (Join-Path $engineStage 'base') | Out-Null
foreach ($file in 'prey06.exe', 'SDL2.dll', 'OpenAL32.dll') { Copy-Item -LiteralPath (Join-Path $preview "engine/$file") -Destination $engineStage }
foreach ($file in 'game00.pk4', 'pak007.pk4') { Copy-Item -LiteralPath (Join-Path $preview "engine/base/$file") -Destination (Join-Path $engineStage 'base') }
Copy-Item -LiteralPath $gameDll -Destination $engineStage
& (Join-Path $PSScriptRoot 'package-local.ps1') -Engine $engineStage -Output $folder -Launcher $launcher -Importer $importer | Out-Null
Remove-Item -LiteralPath $engineStage -Recurse -Force

# Player documents and license texts.
$docs = Join-Path $projectRoot 'docs/release'
Copy-Item -LiteralPath (Join-Path $docs 'README.txt'), (Join-Path $docs 'KNOWN-ISSUES.txt'), (Join-Path $docs 'THIRD-PARTY-NOTICES.txt') -Destination $folder
Copy-Item -LiteralPath (Join-Path $projectRoot 'CHANGELOG.md') -Destination (Join-Path $folder 'CHANGELOG.txt')
Set-Content -LiteralPath (Join-Path $folder 'userdata/README.txt') -Encoding ascii -Value 'Your Prey2006 Reawakened settings and saves are stored in this folder.'
$licenses = Join-Path $folder 'licenses'
New-Item -ItemType Directory -Force $licenses | Out-Null
$libs = Join-Path $projectRoot 'neo/libs'
Copy-Item -LiteralPath (Join-Path $projectRoot 'COPYING.txt') -Destination (Join-Path $licenses 'COPYING.txt')
Copy-Item -LiteralPath (Join-Path $libs 'SDL2/LICENSE.txt') -Destination (Join-Path $licenses 'SDL2.txt')
Copy-Item -LiteralPath (Join-Path $libs 'OpenalSoft/COPYING') -Destination (Join-Path $licenses 'OpenAL-Soft.txt')
Copy-Item -LiteralPath (Join-Path $libs 'Curl/COPYING') -Destination (Join-Path $licenses 'curl.txt')
Copy-Item -LiteralPath (Join-Path $libs 'imgui/LICENSE.txt') -Destination (Join-Path $licenses 'imgui.txt')
# mikktspace carries its zlib license in its header comment.
$header = Get-Content -LiteralPath (Join-Path $libs 'mikktspace/mikktspace.h') -Raw
$notice = [regex]::Match($header, '/\*\*[\s\S]*?\*/').Value
if (-not $notice) { throw 'mikktspace license comment not found' }
Set-Content -LiteralPath (Join-Path $licenses 'mikktspace.txt') -Encoding ascii -Value $notice
# The importer's Python runtime and packages, from the machine that built it.
$pythonLicense = python -c "import sys, pathlib; print(pathlib.Path(sys.base_prefix, 'LICENSE.txt'))"
$pillowLicense = python -c "import importlib.metadata as m, pathlib; d = m.distribution('pillow'); print(next(pathlib.Path(d.locate_file(f)) for f in d.files if pathlib.PurePath(f).name.upper().startswith('LICENSE')))"
$numpyLicense = Get-ChildItem -LiteralPath (Join-Path $importer '_internal') -Recurse -File -Filter 'LICENSE.txt' |
	Where-Object { $_.FullName -match 'numpy-[^\\]+\.dist-info' } | Select-Object -First 1
foreach ($pair in @(@($pythonLicense, 'Python.txt'), @($pillowLicense, 'Pillow.txt'), @($numpyLicense.FullName, 'NumPy.txt'))) {
	if (-not $pair[0] -or -not (Test-Path -LiteralPath $pair[0])) { throw "License text not found for $($pair[1])" }
	Copy-Item -LiteralPath $pair[0] -Destination (Join-Path $licenses $pair[1])
}
Set-Content -LiteralPath (Join-Path $folder 'VERSION.txt') -Encoding ascii -Value "Prey2006 Reawakened $Version`r`nSource commit $commit"

# Audit: nothing retail, nothing derived from retail, no personal or build files.
$problems = @()
Get-ChildItem -LiteralPath $folder -Recurse -File | ForEach-Object {
	$relative = $_.FullName.Substring($folder.Length + 1).Replace('\', '/')
	if ($relative -match '(^|/)pak00[0-6]\.pk4$') { $problems += "retail archive: $relative" }
	if ($relative -match '\.(save|pdb|part|tmp)$' -or $relative -match '(^|/)(preykey|prey06\.cfg|Prey-settings\.json|setup-complete)$') { $problems += "personal or build file: $relative" }
	if ($relative -match '^engine/base/(models/reawakened/portalgun/[^/]+|def/reawakened_portalgun_opening\.def|materials/reawakened_portalgun(_retail|_energy)?\.mtr|zz_reawakened_jen_seam\.pk4)$') { $problems += "derived from retail: $relative" }
	if ($relative -match '^engine/base/(doom3-import-manifest\.json|reawakened-(doom3|portal|prey)-files\.txt)$' -or $relative -match '^(setup-content|engine/replaced)/') { $problems += "imported content: $relative" }
	if ($relative -match '^userdata/' -and $relative -ne 'userdata/README.txt') { $problems += "user data: $relative" }
}
if ($problems) { throw "Release audit failed:`n$($problems -join "`n")" }

# Archives and checksums. The zip keeps the top-level folder. Entries are written
# one by one: .NET Framework's CreateFromDirectory stores '\' separators, which
# the zip format forbids and some extractors turn into flat file names.
$zip = Join-Path $releaseRoot "$name-win64.zip"
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$stream = [System.IO.File]::Open($zip, [System.IO.FileMode]::CreateNew)
$archive = New-Object System.IO.Compression.ZipArchive($stream, [System.IO.Compression.ZipArchiveMode]::Create)
try {
	foreach ($file in Get-ChildItem -LiteralPath $folder -Recurse -File) {
		$entry = "$name/" + $file.FullName.Substring($folder.Length + 1).Replace('\', '/')
		[void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $file.FullName, $entry, [System.IO.Compression.CompressionLevel]::Optimal)
	}
} finally { $archive.Dispose(); $stream.Dispose() }
$check = [System.IO.Compression.ZipFile]::OpenRead($zip)
try { $badEntries = @($check.Entries | Where-Object { $_.FullName.Contains('\') }) } finally { $check.Dispose() }
if ($badEntries) { throw 'Zip entries contain backslashes' }
$source = Join-Path $releaseRoot "$name-source.zip"
git -C $projectRoot archive --format=zip --prefix="$name-source/" -o $source HEAD
if ($LASTEXITCODE -ne 0) { throw 'git archive failed' }
$sums = foreach ($file in $zip, $source) { '{0}  {1}' -f (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLower(), (Split-Path $file -Leaf) }
# LF line endings: sha256sum -c rejects CRLF entries.
[System.IO.File]::WriteAllText((Join-Path $releaseRoot 'SHA256SUMS.txt'), (($sums -join "`n") + "`n"), (New-Object System.Text.ASCIIEncoding))

$files = (Get-ChildItem -LiteralPath $folder -Recurse -File).Count
Write-Output ("Release {0} from {1}: {2} files; audit passed" -f $Version, $commit.Substring(0, 10), $files)
foreach ($file in $zip, $source) { Write-Output ("  {0} ({1:N1} MB)" -f (Split-Path $file -Leaf), ((Get-Item -LiteralPath $file).Length / 1MB)) }
