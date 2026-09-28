# Lists the base-relative files in an engine base folder that were generated from
# the optional Doom 3 or Portal imports. Everything else in base is Reawakened
# itself (or derived from retail Prey) and works without either game.
#   . tools/optional-content.ps1; Get-OptionalContent -Base <engine\base> -Game doom3|portal
function Get-OptionalContent {
	param([Parameter(Mandatory)][string]$Base, [Parameter(Mandatory)][ValidateSet('doom3', 'portal')][string]$Game)
	$Base = (Resolve-Path -LiteralPath $Base).Path
	if ($Game -eq 'doom3') {
		$manifest = Join-Path $Base 'doom3-import-manifest.json'
		if (-not (Test-Path -LiteralPath $manifest)) { return @() }
		$names = @((Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json).PSObject.Properties.Name) + 'doom3-import-manifest.json'
		return @($names | Where-Object { Test-Path -LiteralPath (Join-Path $Base $_) } | Sort-Object -Unique)
	}
	# Portal importer outputs (tools/portalgun/import_viewmodel.py, import_worldmodel.py,
	# import_reticle.py). The wall openings come from retail Prey, and the shot
	# entities and particles (portalgun_shots.def/.prt) ship with Reawakened.
	# materials/portalgun_shots.mtr also ships, with a retail-texture fallback
	# that the import replaces.
	$patterns = @(
		'^models/reawakened/portalgun/(view|world)/',
		'^textures/reawakened/portalgun/',
		'^sound/reawakened/portalgun/',
		'^guis/assets/portalgun/',
		'^def/portalgun_view\.def$',
		'^materials/portalgun_(view|shots|reticle)\.mtr$',
		'^script/reawakened/weapon_portalgun_v1\.script$',
		'^sound/portalgun_view\.sndshd$'
	)
	Get-ChildItem -LiteralPath $Base -Recurse -File | ForEach-Object {
		$relative = $_.FullName.Substring($Base.Length + 1) -replace '\\', '/'
		foreach ($p in $patterns) { if ($relative -match $p) { $relative; break } }
	} | Sort-Object
}
