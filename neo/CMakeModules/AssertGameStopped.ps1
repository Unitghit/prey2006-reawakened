param([Parameter(Mandatory=$true)][string]$OutputRoot)
$ErrorActionPreference = 'Stop'
$expectedRoot = [System.IO.Path]::GetFullPath($OutputRoot).TrimEnd('\','/')
foreach ($process in @(Get-Process -Name prey06 -ErrorAction SilentlyContinue)) {
    $exePath = $process.Path
    if (!$exePath -or [string]::Equals([System.IO.Path]::GetDirectoryName($exePath).TrimEnd('\','/'), $expectedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        [Console]::Error.WriteLine("Cannot replace game packages while Prey2006 is running from this output folder (PID $($process.Id)). Close the game, then rebuild. Existing packages were not changed.")
        exit 1
    }
}
exit 0
