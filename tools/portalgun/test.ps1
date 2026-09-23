param(
    [Parameter(Mandatory=$true)][string]$Engine,
    [Parameter(Mandatory=$true)][string]$RetailBase,
    [Parameter(Mandatory=$true)][string]$Profile,
    [string[]]$Cases = @('input','regression','floor','ceiling','blocked','replacement','access','floor_edge','guidance','guide_lookaway','guide_steering','guide_fast','floor_exit','placement','objects','replacement_occupants','through_portals','floor_escape','floor_partial','floor_continuous','floor_approach','floor_edge_slide','floor_corner','floor_clearance','surface_fit','ceiling_entry','floor_slab','wall_step','wall_approach','tapered_shell','sloped_ceiling','clip_column','oblique','terrain_fit','mesh_ground','rough_surfaces','reverse','static_exit')
)
$ErrorActionPreference='Stop'
$Engine=(Resolve-Path -LiteralPath $Engine).Path
$RetailBase=(Resolve-Path -LiteralPath $RetailBase).Path
$Profile=[IO.Path]::GetFullPath($Profile)
if(Test-Path -LiteralPath $Profile){throw 'Use a new isolated profile directory.'}
$base=Join-Path $Profile 'base'
New-Item -ItemType Directory -Path "$base/maps","$base/materials","$base/models" -Force | Out-Null
Copy-Item "$PSScriptRoot/tests/rw_portal_lab.map" "$base/maps/"
Copy-Item "$PSScriptRoot/tests/rw_portal_surface.map" "$base/maps/"
Copy-Item "$PSScriptRoot/tests/rw_portal_slab.map" "$base/maps/"
Copy-Item "$PSScriptRoot/tests/rw_portal_taper.map" "$base/maps/"
Copy-Item "$PSScriptRoot/tests/rw_portal_slope.map" "$base/maps/"
Copy-Item "$PSScriptRoot/tests/rw_portal_column.map" "$base/maps/"
Copy-Item "$PSScriptRoot/tests/rw_portal_terrain.map" "$base/maps/"
Copy-Item "$PSScriptRoot/tests/portal_lab.mtr" "$base/materials/"
Copy-Item "$PSScriptRoot/tests/portal_obstacle.ase" "$base/models/"
Copy-Item "$PSScriptRoot/tests/portal_terrain_cap.ase" "$base/models/"
Copy-Item "$PSScriptRoot/tests/portal_rough_ground.ase" "$base/models/"
Copy-Item "$PSScriptRoot/tests/portal_decal_test.ase" "$base/models/"
Copy-Item "$PSScriptRoot/tests/portal_decal_test.mtr" "$base/materials/"
foreach($name in $Cases) {
    Copy-Item "$PSScriptRoot/tests/$name.cfg" "$base/test.cfg" -Force
    $arguments='+set fs_basepath "'+$Engine+'" +set fs_cdpath "'+(Split-Path $RetailBase -Parent)+'" +set fs_devpath "'+$Profile+'" +set fs_savepath "'+$Profile+'" +set fs_configpath "'+$Profile+'" +set fs_game "" +set r_fullscreen 0 +set r_fullscreenDesktop 0 +set r_mode -1 +set r_customWidth 960 +set r_customHeight 540 +set r_multiSamples 0 +set s_volume_dB -60 +set com_unlockedFPS 0 +set com_fixedTic 1 +set r_gammaInShader 1 +set developer 1 +set ai_disable 1 +set logfile 2 +exec test.cfg'
    $process=Start-Process (Join-Path $Engine 'prey06.exe') -WorkingDirectory $Engine -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if(!$process.WaitForExit(60000)){Stop-Process -Id $process.Id; throw "$name timed out"}
    if($process.ExitCode -ne 0){throw "$name exited with $($process.ExitCode)"}
    $log=Get-Content -LiteralPath "$base/$name.log" -Raw
    if($log -match 'ERROR:|shutting down:'){throw "$name reported an engine error"}
    if($name -eq 'deep_views') {
        if($log -notmatch 'PORTAL_DEEP depth 6' -or $log -notmatch 'PORTAL_REPEAT_FALLBACK depth 7') { throw 'Deep portal views or repeating fallback did not activate' }
        $noRepeat=Get-Content "$base/deep_no_repeat.log" -Raw
        $standard=Get-Content "$base/deep_standard.log" -Raw
        if($noRepeat -match 'PORTAL_REPEAT_FALLBACK' -or $noRepeat -notmatch 'PORTAL_DEEP depth 6') { throw 'No-repeat mode did not preserve real deep views' }
        if($standard -match 'PORTAL_DEEP|PORTAL_REPEAT_FALLBACK' -or $standard -notmatch 'PORTAL_DEPTH_LIMIT depth 3') { throw 'Standard portal mode did not retain its limit' }
        if(($log+$noRepeat+$standard) -match 'GL_INVALID|program error|ERROR:') { throw 'Deep portal renderer reported an error' }
    }
    if($name -eq 'through_portals') {
        foreach($phase in @('PLAYER','SCRIPT','RELOAD','CHAIN')) {
            $part=[regex]::Match($log,"(?s)THROUGH_${phase}_BEGIN(.*?)THROUGH_${phase}_END").Groups[1].Value
            if($part -notmatch 'impact (blue|orange) success=1' -or $part -match 'rejected:'){throw "Portal shot $phase failed"}
            if($phase -eq 'PLAYER' -and $part -notmatch 'portal=rw_gun_blue'){throw 'Player portal was not traversed'}
            if($phase -eq 'SCRIPT' -and $part -notmatch 'portal=script_in'){throw 'Scripted portal was not traversed'}
            if($phase -eq 'CHAIN' -and $part -notmatch 'portal=chain_in hops=2'){throw 'Portal chain was not traversed'}
        }
        $loop=[regex]::Match($log,'(?s)THROUGH_LOOP_BEGIN(.*?)THROUGH_LOOP_END').Groups[1].Value
        if($loop -notmatch 'hops=8' -or $loop -notmatch 'impact blue success=0 blocked=1' -or $loop -match 'placed blue'){throw 'Portal loop was not bounded'}
        $outside=[regex]::Match($log,'(?s)THROUGH_OUTSIDE_BEGIN(.*?)THROUGH_OUTSIDE_END').Groups[1].Value
        if($outside -match 'PORTALGUN_SHOT portal=' -or $outside -notmatch 'impact blue success=1'){throw 'Shot outside oval incorrectly entered portal'}
    }
    if($name -eq 'rough_surfaces') {
        foreach($surface in @('WALL','CEILING')) {
            $part=[regex]::Match($log,"(?s)ROUGH_${surface}_BEGIN(.*?)ROUGH_${surface}_END").Groups[1].Value
            if($part -notmatch 'PORTAL_TERRAIN_FIT' -or $part -notmatch 'PORTALGUN placed blue' -or
                ([regex]::Matches($part,'PORTAL_EXIT ')).Count -ne 1 -or $part -match 'PORTALGUN rejected|blocked exit') {
                throw "Uneven $surface placement or traversal failed"
            }
        }
    }
    if($name -eq 'decal_mask') {
        if($log -match 'GL_INVALID|program error|Couldn.t load.*test_decal'){throw 'Decal fixture rendering failed'}
        foreach($capture in @('decal_clean.tga','decal_mask.tga')) {
            if(!(Test-Path "$base/$capture")){throw "Missing $capture"}
        }
        # Visual regression: compare with decal_clean. Green must stay outside
        # the aperture, and the small red foreground square must remain inside.
    }
    if($name -eq 'input' -and ($log -notmatch 'PORTALGUN placed blue' -or $log -notmatch 'PORTALGUN placed orange')){throw 'Input placement failed'}
    if($name -eq 'viewmodel') {
        foreach($color in @('blue','orange')) {
            if(([regex]::Matches($log,"PORTALGUN_VIEW fire $color")).Count -ne 1){throw "Viewmodel $color firing failed"}
        }
        if($log -notmatch 'Saved viewmodel' -or $log -notmatch 'PORTALGUN_VIEW selected weaponobj_wrench' -or $log -notmatch 'PORTALGUN_VIEW selected weaponobj_portalgun'){throw 'Viewmodel save/toggle failed'}
    }
    if($name -eq 'materials') {
        foreach($color in @('blue','orange')) {
            if(([regex]::Matches($log,"PORTALGUN_VIEW fire $color")).Count -ne 1){throw "Material $color firing failed"}
        }
        if($log -notmatch 'Saved material_orange' -or $log -notmatch 'PORTAL_EXIT'){throw 'Material save/crossing failed'}
        if($log -match 'GL_INVALID|error at|R_AutospriteDeform:|unknown token|Couldn.t load.*portalgun'){throw 'Material shader/asset failure'}
    }
    if($name -eq 'shots') {
        if($log -match 'Unknown classname|Couldn.t load.*(rw_portal_(blue|orange)|portalgun)|invalid joint|unknown token'){throw 'Shot asset failure'}
        $flight=[regex]::Match($log,'(?s)SHOT_FLIGHT_SAVE_BEGIN(.*?)SHOT_FLIGHT_SAVE_END').Groups[1].Value
        if($flight -notmatch 'Saved shot_flight' -or $flight -notmatch 'Saved shot_opening' -or
            ([regex]::Matches($flight,'PORTALGUN_SHOT impact blue success=1')).Count -ne 1 -or
            $flight -notmatch 'PORTALGUN_OPENING rw_gun_blue remaining=0'){throw 'In-flight/opening save failed'}
        if($flight -notmatch 'PORTALGUN_ENDPOINT rw_gun_blue origin=511 0 71' -or
            $flight -notmatch 'PORTALGUN placed blue at 511 180 71'){throw 'Portal replacement moved before arrival or lost the captured target'}
        $look=[regex]::Match($log,'(?s)SHOT_LOOK_BEGIN(.*?)SHOT_LOOK_END').Groups[1].Value
        if($look -notmatch 'PORTALGUN placed orange at -511 180 71' -or $look -notmatch 'portal=rw_gun_orange'){throw 'Looking away changed a shot already in flight'}
        $supersede=[regex]::Match($log,'(?s)SHOT_SUPERSEDE_BEGIN(.*?)SHOT_SUPERSEDE_END').Groups[1].Value
        if(([regex]::Matches($supersede,'PORTALGUN_SHOT launch blue')).Count -ne 2 -or
            ([regex]::Matches($supersede,'PORTALGUN_SHOT impact blue success=1')).Count -ne 1){throw 'Superseded shot placed an old portal'}
        foreach($phase in @('REJECT','BLOCKED')) {
            $part=[regex]::Match($log,"(?s)SHOT_${phase}_BEGIN(.*?)SHOT_${phase}_END").Groups[1].Value
            if($part -notmatch 'PORTALGUN_SHOT impact blue success=0' -or $part -match 'PORTALGUN placed'){throw "$phase replaced a portal"}
        }
        $disabled=[regex]::Match($log,'(?s)SHOT_DISABLE_BEGIN(.*?)SHOT_DISABLE_END').Groups[1].Value
        if($disabled -notmatch 'PORTALGUN_FLIGHTS 0' -or $disabled -match 'PORTALGUN placed'){throw 'Disabled flight survived'}
    }
    if($name -in @('regression','floor','ceiling') -and $log -notmatch 'PORTAL_EXIT[\s\S]{0,100}collision_adjustment 0\.000000'){throw "$name failed continuous traversal"}
    if($name -eq 'regression' -and $log -notmatch 'origin=495\.75 100'){throw 'Solid-wall control failed'}
    if($name -in @('floor','ceiling')) {
        $motion=[regex]::Matches($log,'PORTAL_MOTION[\s\S]{0,150}?speed ([0-9.]+)')
        $exit=[regex]::Match($log,'PORTAL_EXIT[^\r\n]*speed ([0-9.]+)')
        if(!$motion.Count -or !$exit.Success -or [math]::Abs([double]$motion[$motion.Count-1].Groups[1].Value - [double]$exit.Groups[1].Value) -gt .01){throw "$name lost crossing velocity"}
    }
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
    if($name -eq 'access') {
        if(([regex]::Matches($log,'PORTAL_ENTITY_EXIT hhProjectile')).Count -lt 2){throw 'Projectile traversal failed'}
        if($log -notmatch 'PORTAL_EXIT'){throw 'Standing floor entry failed'}
        if($log -notmatch 'PORTAL_ENTITY_EXIT hhMoveable'){throw 'Movable prop traversal failed'}
    }
    if($name -eq 'floor_edge' -and $log -notmatch 'PORTAL_EXIT'){throw 'Floor edge clearance failed'}
    if($name -eq 'guidance' -and ($log -notmatch 'PORTAL_GUIDANCE' -or $log -notmatch 'PORTAL_EXIT')){throw 'Floor guidance failed'}
    if($name -in @('guide_lookaway','guide_steering','guide_fast') -and $log -match 'PORTAL_GUIDANCE'){throw "$name incorrectly applied guidance"}
    if($name -eq 'floor_exit' -and ($log -notmatch 'PORTAL_FLOOR_EXIT' -or $log -notmatch 'PORTAL_EXIT')){throw 'Low-speed floor exit failed'}
    if($name -eq 'placement') {
        if(([regex]::Matches($log,'PORTALGUN placed blue')).Count -ne 3 -or $log -match 'PORTALGUN rejected:'){throw 'Bounded placement fitting failed'}
    }
    if($name -eq 'reverse' -and ([regex]::Matches($log,'PORTAL_EXIT ')).Count -lt 4){throw 'Repeated immediate reversal failed'}
    if($name -eq 'floor_escape') {
        $probes=[regex]::Matches($log,'PORTAL_PROBE fraction=([0-9.]+)')
        if($probes.Count -ne 2 -or [double]$probes[0].Groups[1].Value -ne 1 -or [double]$probes[1].Groups[1].Value -ge 1){throw 'Floor edge clearance or embedded-hull boundary failed'}
    }
    if($name -eq 'floor_continuous') {
        foreach($phase in @('PARTIAL','REVERSE')) {
            $part=[regex]::Match($log,"(?s)CONTINUOUS_${phase}_BEGIN(.*?)CONTINUOUS_${phase}_END").Groups[1].Value
            if(!$part -or $part -match 'PORTAL_EXIT '){throw "$phase teleported before the eye crossed"}
        }
        foreach($phase in @('CROSS','FAST')) {
            $part=[regex]::Match($log,"(?s)CONTINUOUS_${phase}_BEGIN(.*?)CONTINUOUS_${phase}_END").Groups[1].Value
            if(([regex]::Matches($part,'PORTAL_EXIT ')).Count -ne 1 -or $part -notmatch 'PORTAL_EYE_CROSS feet=-68.000 eye=0.000'){throw "$phase did not cross exactly at the viewpoint"}
        }
        if($log -notmatch 'Saved partial_camera'){throw 'Partial-entry save failed'}
    }
    if($name -eq 'floor_partial') {
        $partial=[regex]::Match($log,'(?s)FLOOR_PARTIAL_BEGIN(.*?)FLOOR_PARTIAL_END').Groups[1].Value
        $backside=[regex]::Match($log,'(?s)FLOOR_BACKSIDE_BEGIN(.*?)FLOOR_BACKSIDE_END').Groups[1].Value
        if(([regex]::Matches($partial,'PORTAL_EXIT ')).Count -ne 1){throw 'Partial floor crossing did not recover exactly once'}
        if($backside -match 'PORTAL_EXIT '){throw 'Backside floor entry incorrectly recovered'}
    }
    if($name -eq 'sloped_ceiling' -or $name -eq 'clip_column') {
        $part=[regex]::Match($log,'(?s)SLOPE_EXIT_BEGIN(.*?)SLOPE_EXIT_END').Groups[1].Value
        $positions=[regex]::Matches($part,'selected=\d+ current=\d+ origin=(-?[0-9.]+)\s+(-?[0-9.]+)\s+(-?[0-9.]+)')
        if(([regex]::Matches($part,'PORTAL_EXIT ')).Count -ne 1 -or !$positions.Count -or
            [double]$positions[$positions.Count-1].Groups[3].Value -ge 80){throw 'Player failed to fully emerge from sloped ceiling'}
    }
    if($name -eq 'mesh_ground') {
        $place=[regex]::Match($log,'(?s)MESH_PLACE_BEGIN(.*?)MESH_PLACE_END').Groups[1].Value
        if($place -notmatch 'PORTAL_TERRAIN_FIT' -or $place -notmatch 'PORTALGUN_SHOT impact blue success=1'){throw 'Static rough mesh placement failed'}
        foreach($phase in @('CROSS','RELOAD')) {
            $part=[regex]::Match($log,"(?s)MESH_${phase}_BEGIN(.*?)MESH_${phase}_END").Groups[1].Value
            $positions=[regex]::Matches($part,'selected=\d+ current=\d+ origin=(-?[0-9.]+)\s+(-?[0-9.]+)\s+(-?[0-9.]+)')
            if(([regex]::Matches($part,'PORTAL_EXIT ')).Count -ne 1 -or !$positions.Count -or
                [double]$positions[$positions.Count-1].Groups[2].Value -ge 450){throw "$phase static mesh traversal failed"}
        }
        $movable=[regex]::Match($log,'(?s)MESH_MOVABLE_BEGIN(.*?)MESH_MOVABLE_END').Groups[1].Value
        if($movable -notmatch 'PORTALGUN rejected: aim at a stationary world surface' -or $movable -match 'PORTALGUN placed'){throw 'Movable support was accepted'}
    }
    if($name -eq 'terrain_fit') {
        $gentle=[regex]::Match($log,'(?s)TERRAIN_GENTLE_BEGIN(.*?)TERRAIN_GENTLE_END').Groups[1].Value
        if($gentle -notmatch 'PORTAL_TERRAIN_FIT' -or $gentle -notmatch 'PORTALGUN placed blue'){throw 'Gentle convex ground failed fitting'}
        foreach($phase in @('CROSS','RELOAD','SLOPE')) {
            $part=[regex]::Match($log,"(?s)TERRAIN_${phase}_BEGIN(.*?)TERRAIN_${phase}_END").Groups[1].Value
            $positions=[regex]::Matches($part,'selected=\d+ current=\d+ origin=(-?[0-9.]+)\s+(-?[0-9.]+)\s+(-?[0-9.]+)')
            if(([regex]::Matches($part,'PORTAL_EXIT ')).Count -ne 1 -or !$positions.Count -or
                [double]$positions[$positions.Count-1].Groups[2].Value -ge 450){throw "$phase fitted ground traversal failed"}
            if($phase -eq 'SLOPE' -and $part -notmatch 'PORTAL_TERRAIN_FIT'){throw 'Sloped terrain did not exercise plane fitting'}
        }
        foreach($phase in @('RIDGE','LEDGE','HEADROOM')) {
            $part=[regex]::Match($log,"(?s)TERRAIN_${phase}_BEGIN(.*?)TERRAIN_${phase}_END").Groups[1].Value
            if($part -notmatch 'PORTALGUN rejected: no nearby supported opening' -or $part -match 'PORTALGUN placed'){throw "$phase incorrectly accepted terrain portal"}
        }
    }
    if($name -eq 'oblique') {
        foreach($marker in @('ANGLE_0','ANGLE_60','ANGLE_80','ANGLE_85','ANGLE_88','FLOOR_85')) {
            $part=[regex]::Match($log,"(?s)${marker}_BEGIN(.*?)${marker}_END").Groups[1].Value
            if($part -notmatch 'PORTALGUN_SHOT impact blue success=1 blocked=0' -or $part -match 'PORTALGUN rejected:'){throw "$marker camera-aimed shot failed"}
        }
    }
    if($name -eq 'clip_column') {
        $clearance=[regex]::Match($log,'PORTAL_EXIT_CLEARANCE distance=([0-9.]+)')
        if(!$clearance.Success -or [double]$clearance.Groups[1].Value -le 16){throw 'Column fixture did not exercise extended clearance'}
    }
    if($name -eq 'tapered_shell') {
        $part=[regex]::Match($log,'(?s)TAPER_EXIT_BEGIN(.*?)TAPER_EXIT_END').Groups[1].Value
        $positions=[regex]::Matches($part,'selected=\d+ current=\d+ origin=(-?[0-9.]+)\s+(-?[0-9.]+)\s+(-?[0-9.]+)')
        if(([regex]::Matches($part,'PORTAL_EXIT ')).Count -ne 1 -or !$positions.Count -or
            [double]$positions[$positions.Count-1].Groups[1].Value -ge 463){throw 'Tapered shell prevented physical exit after teleport'}
    }
    if($name -eq 'wall_approach') {
        $idle=[regex]::Match($log,'(?s)APPROACH_IDLE_BEGIN(.*?)APPROACH_IDLE_END').Groups[1].Value
        $walk=[regex]::Match($log,'(?s)APPROACH_WALK_BEGIN(.*?)APPROACH_WALK_END').Groups[1].Value
        if($idle -match 'PORTAL_APPROACH_GUIDANCE|PORTAL_EXIT '){throw 'Portal approach moved an idle player'}
        if($walk -notmatch 'PORTAL_APPROACH_GUIDANCE' -or ([regex]::Matches($walk,'PORTAL_EXIT ')).Count -ne 1){throw 'Portal shoulder approach failed crossing'}
        $positions=[regex]::Matches($walk,'selected=\d+ current=\d+ origin=(-?[0-9.]+)\s+(-?[0-9.]+)\s+(-?[0-9.]+)')
        if(!$positions.Count -or [double]$positions[$positions.Count-1].Groups[2].Value -ge 463){throw 'Player failed to walk fully beyond portal exit'}
    }
    if($name -eq 'wall_step') {
        if($log -notmatch 'PORTAL_EXIT_CLEARANCE' -or $log -notmatch 'PORTAL_EXIT ' -or $log -match 'PORTALGUN blocked exit'){throw 'Raised wall portal exit failed step clearance'}
    }
    if($name -eq 'static_exit') {
        $behind=[regex]::Match($log,'(?s)STATIC_BEHIND_BEGIN(.*?)STATIC_BEHIND_END').Groups[1].Value
        $front=[regex]::Match($log,'(?s)STATIC_FRONT_BEGIN(.*?)STATIC_FRONT_END').Groups[1].Value
        if($behind -notmatch 'PORTAL_EXIT ' -or $behind -match 'PORTALGUN blocked exit'){throw 'Static geometry behind exit blocked crossing'}
        if($front -notmatch 'PORTALGUN blocked exit|PORTAL_PARTIAL_BLOCK' -or $front -match 'PORTAL_EXIT '){throw 'Static geometry in front of exit failed to block crossing'}
    }
    if($name -eq 'ceiling_entry') {
        $jump=[regex]::Match($log,'(?s)CEILING_JUMP_BEGIN(.*?)CEILING_JUMP_END').Groups[1].Value
        if(([regex]::Matches($jump,'PORTAL_EXIT ')).Count -ne 1 -or $jump -notmatch 'PORTAL_EYE_CROSS feet=68.000 eye=0.000'){throw 'Ceiling jump failed viewpoint crossing'}
        $positions=[regex]::Matches($jump,'selected=\d+ current=\d+ origin=(-?[0-9.]+)\s+(-?[0-9.]+)\s+(-?[0-9.]+)')
        if(!$positions.Count -or [double]$positions[$positions.Count-1].Groups[2].Value -ge 495){throw 'Player remained behind ceiling portal exit'}
    }
    if($name -eq 'surface_fit') {
        $recess=[regex]::Match($log,'(?s)SURFACE_RECESS_BEGIN(.*?)SURFACE_RECESS_END').Groups[1].Value
        $obstacle=[regex]::Match($log,'(?s)SURFACE_OBSTACLE_BEGIN(.*?)SURFACE_OBSTACLE_END').Groups[1].Value
        $low=[regex]::Match($log,'(?s)SURFACE_LOW_BEGIN(.*?)SURFACE_LOW_END').Groups[1].Value
        if($recess -notmatch 'placed blue at 507 0 160'){throw 'Quarter-area flat patch with recessed surround was rejected'}
        # The surface fitter can now move above this small obstacle. Check
        # its nearest corner stays outside the oval, not an obsolete rejection.
        $placed=[regex]::Match(($obstacle -replace '\r?\n',''),'placed blue at (-?[0-9.]+) (-?[0-9.]+) (-?[0-9.]+)')
        if(!$placed.Success){throw 'Nearby clear wall placement failed'}
        $dy=[math]::Max(0,27-[double]$placed.Groups[2].Value)
        $dz=[math]::Max(0,[double]$placed.Groups[3].Value-176)
        if(($dy*$dy/(39*39)+$dz*$dz/(49*49)) -le 1){throw 'Shifted portal overlaps obstacle'}
        $blocked=[regex]::Match($log,'(?s)SURFACE_BLOCKED_BEGIN(.*?)SURFACE_BLOCKED_END').Groups[1].Value
        if($blocked -notmatch 'rejected: no nearby supported opening' -or $blocked -match 'placed blue'){throw 'Portal fit through blocked opening'}
        if($low -notmatch 'placed orange at 511 -200 71'){throw 'Lower wall placement failed'}
    }
    if($name -eq 'floor_slab') {
        $entry=[regex]::Match($log,'(?s)SLAB_ENTRY_BEGIN(.*?)SLAB_ENTRY_END').Groups[1].Value
        if($entry -notmatch 'PORTAL_ENTRY_CLEARANCE' -or ([regex]::Matches($entry,'PORTAL_EXIT ')).Count -ne 1){throw 'Thin floor underside trapped partial portal entry'}
    }
    if($name -eq 'floor_clearance') {
        $shallow=[regex]::Match($log,'(?s)CLEARANCE_SHALLOW_BEGIN(.*?)CLEARANCE_SHALLOW_END').Groups[1].Value
        $deep=[regex]::Match($log,'(?s)CLEARANCE_DEEP_BEGIN(.*?)CLEARANCE_DEEP_END').Groups[1].Value
        $fast=[regex]::Match($log,'(?s)CLEARANCE_FAST_BEGIN(.*?)CLEARANCE_FAST_END').Groups[1].Value
        if($shallow -notmatch 'PORTAL_ENTRY_CLEARANCE' -or ([regex]::Matches($shallow,'PORTAL_EXIT ')).Count -ne 1){throw 'Shallow exit-floor overlap trapped the player'}
        if($fast -notmatch 'PORTAL_ENTRY_CLEARANCE' -or ([regex]::Matches($fast,'PORTAL_EXIT ')).Count -ne 1){throw 'Fast exit-floor overlap trapped the player'}
        if($deep -notmatch 'PORTAL_PARTIAL_BLOCK' -or $deep -match 'PORTAL_EXIT |PORTAL_ENTRY_CLEARANCE'){throw 'Deep obstruction incorrectly received clearance assistance'}
    }
    if($name -eq 'floor_corner') {
        $behind=[regex]::Match($log,'(?s)CORNER_BEHIND_BEGIN(.*?)CORNER_BEHIND_END').Groups[1].Value
        $front=[regex]::Match($log,'(?s)CORNER_FRONT_BEGIN(.*?)CORNER_FRONT_END').Groups[1].Value
        if(([regex]::Matches($behind,'PORTAL_EXIT ')).Count -ne 1 -or $behind -match 'blocked exit|PORTAL_PARTIAL_BLOCK'){throw 'Hidden portion of angled trim blocked portal crossing'}
        if($front -notmatch 'PORTAL_PARTIAL_BLOCK|blocked exit' -or $front -match 'PORTAL_EXIT '){throw 'Visible portion of angled trim failed to block crossing'}
    }
    if($name -eq 'floor_edge_slide') {
        $blocked=[regex]::Match($log,'(?s)EDGE_BLOCK_BEGIN(.*?)EDGE_BLOCK_END').Groups[1].Value
        $slide=[regex]::Match($log,'(?s)EDGE_SLIDE_BEGIN(.*?)EDGE_SLIDE_END').Groups[1].Value
        if($blocked -notmatch 'PORTAL_PARTIAL_BLOCK' -or $blocked -match 'PORTAL_EXIT '){throw 'Edge obstacle did not stop inward motion'}
        if(([regex]::Matches($slide,'PORTAL_EXIT ')).Count -ne 1){throw 'Blocked ground entry prevented sliding to a clear exit'}
    }
    if($name -eq 'floor_approach') {
        foreach($side in @('FIRST','SECOND')) {
            $part=[regex]::Match($log,"(?s)APPROACH_${side}_BEGIN(.*?)APPROACH_${side}_END").Groups[1].Value
            if(([regex]::Matches($part,'PORTAL_EXIT ')).Count -ne 1 -or $part -match 'PORTAL_PARTIAL_BLOCK'){throw "Floor approach $side hit an invisible barrier"}
        }
    }
    if($name -eq 'replacement_occupants') {
        $release=[regex]::Match($log,'(?s)OCCUPANT_RELEASE_BEGIN(.*?)OCCUPANT_RELEASE_END').Groups[1].Value
        if($release -notmatch 'PORTAL_REPLACEMENT_CLEAR name=release_prop' -or $release -notmatch 'placed blue' -or $release -match 'rejected:'){throw 'Movable object locked replacement'}
        foreach($case in @('BLOCKED','PLAYER')) {
            $part=[regex]::Match($log,"(?s)OCCUPANT_${case}_BEGIN(.*?)OCCUPANT_${case}_END").Groups[1].Value
            if($part -notmatch 'rejected: leave the opening clear' -or $part -match 'PORTAL_REPLACEMENT_CLEAR|placed blue'){throw "Unsafe $case replacement allowed"}
        }
    }
    if($name -eq 'objects') {
        foreach($prop in @('polish_a','polish_b','polish_c')) {
            if(([regex]::Matches($log,'PORTAL_ENTITY_EXIT hhMoveable name='+$prop)).Count -ne 1){throw "Prop $prop did not cross exactly once"}
        }
        if($log -notmatch 'PORTAL_REPLACEMENT_CLEAR name=polish_occupant'){throw 'Movable occupant clearance failed'}
    }
    Write-Output "PASS $name"
}
