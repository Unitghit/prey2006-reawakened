# Builds the bundled content importer (reawakened-import.exe) with PyInstaller,
# plus the Crowbar command-line model decompiler it uses for Portal.
#   <Output>\reawakened-import.exe, _internal\   Python runtime, numpy, Pillow, importers
#   <Output>\crowbar\Crowbar.exe, License.txt    https://github.com/UltraTechX/Crowbar-Command-Line
# The package copies <Output> to engine\importer\. Requires: pip install pyinstaller numpy pillow
param(
	[Parameter(Mandatory)][string]$Crowbar,
	[string]$Output = ''
)
$ErrorActionPreference = 'Stop'
$tools = Split-Path $PSScriptRoot -Parent
$projectRoot = Split-Path $tools -Parent
if (-not $Output) { $Output = Join-Path $projectRoot 'build/importer/reawakened-import' }
$Crowbar = (Resolve-Path -LiteralPath $Crowbar).Path
$crowbarLicense = Join-Path (Split-Path (Split-Path (Split-Path (Split-Path (Split-Path $Crowbar -Parent) -Parent) -Parent) -Parent) -Parent) 'License'
$work = Join-Path $projectRoot 'build/importer/work'
$dist = Split-Path $Output -Parent

# Importer data files are read next to their modules (Path(__file__).with_name).
$data = @()
foreach ($folder in 'doom3', 'portalgun') {
	Get-ChildItem -LiteralPath (Join-Path $tools $folder) -File | Where-Object { $_.Extension -in '.def', '.script', '.mtr' } |
		ForEach-Object { $data += '--add-data'; $data += "$($_.FullName);." }
}
New-Item -ItemType Directory -Force $work | Out-Null
# PyInstaller logs to stderr; Windows PowerShell would treat that as an error.
$ErrorActionPreference = 'Continue'
python -m PyInstaller --noconfirm --clean --onedir --console --name reawakened-import `
	--distpath $dist --workpath $work --specpath $work `
	--paths (Join-Path $tools 'doom3') --paths (Join-Path $tools 'portalgun') `
	--exclude-module tkinter --exclude-module unittest --exclude-module pydoc `
	@data (Join-Path $PSScriptRoot 'reawakened_import.py') *> (Join-Path $work 'pyinstaller.log')
$built = $LASTEXITCODE
$ErrorActionPreference = 'Stop'
if ($built -ne 0) { throw "PyInstaller failed; see $work\pyinstaller.log" }

$crowbarOut = Join-Path $Output 'crowbar'
New-Item -ItemType Directory -Force $crowbarOut | Out-Null
Copy-Item -LiteralPath $Crowbar -Destination $crowbarOut -Force
if (Test-Path -LiteralPath $crowbarLicense) { Copy-Item -LiteralPath $crowbarLicense -Destination (Join-Path $crowbarOut 'License.txt') -Force }
$size = (Get-ChildItem -LiteralPath $Output -Recurse -File | Measure-Object Length -Sum).Sum / 1MB
Write-Output ("Importer: {0} ({1:N0} MB)" -f $Output, $size)
