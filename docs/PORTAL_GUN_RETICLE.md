# Portal gun placement reticle

The portal gun now uses blue/orange aim indicators with filled and outlined states. The small outer marks indicate the last-fired color separately from placement eligibility. Other weapons keep their normal crosshair. The launcher exposes g_portalGunReticle as Portal gun crosshair, defaulting to Placement indicators; Standard crosshair restores the previous presentation. Existing crosshair/HUD visibility and screen interaction remain respected.

## Research and adaptation

Reference: [Portal-Base HUD quickinfo](https://github.com/SonicEraZoR/Portal-Base/blob/master/sp/src/game/client/portal/hud_quickinfo.cpp), specifically its non-beta branch; [weapon placement polling](https://github.com/SonicEraZoR/Portal-Base/blob/master/sp/src/game/shared/portal/weapon_portalgun.cpp). This is a community adaptation, not proof of an unchanged retail executable. It uses per-color placement eligibility and a separate last-fired marker, with placement polling at 0.1-second intervals. No Source implementation code was copied.

The atlas and its rectangles were inspected in the user's installed Portal VPK (materials/sprites/hud/portal_crosshairs.vtf and scripts/mod_textures.txt). tools/portalgun/import_reticle.py converts that local atlas to TGA and creates a Prey GUI material. The existing viewmodel importer invokes it automatically. Generated Valve artwork remains local-only and is not included in Git.

## Placement preview

PlaceGunPortal has a read-only preview mode using the actual backing, terrain fitting, orientation, opposite-portal separation and bounded placement search. It returns before occupant evacuation, entity creation, relinking, animation or any other world mutation. It does not consume developer aim overrides.

The aim ray follows both scripted and player-created linked portals using the projectile's transform/range rules and eight-hop limit. Blue and orange are evaluated independently because the opposite endpoint can make one color ineligible. The result describes surface placement; occupied openings or geometry that moves during projectile flight can still prevent the actual shot.

Completed queries refresh after 100 ms. Camera changes invalidate the query, and difficult searches resume in batches of at most eight placement candidates per color per game tick rather than doing the full search in a single tick. While a new query is pending, an invalidated indicator stays outlined. This bounds candidate work, not a universal wall-clock time limit. Preview fields are transient player state and reset on save restore; the save stream layout is unchanged. Drawing only consumes cached results and applies the existing presentation cursor offset and aspect correction.

## Validation

Hidden, muted tests cover valid surfaces, per-color separation, movable blockers, player-created and scripted portal aim paths, save/reload and the standard-crosshair option. The reticle test checks explicit state and forbids placement/occupant-movement messages during previews. Captures were inspected for fill, outline and color presentation.

Existing input, corner, replacement_occupants, shots and through_portals cases passed. The gui_create test still opens the cabinet, delivers press/release events, prevents held GUI clicks from firing portals, fires both colors after leaving the screen and switches weapons.

The native launcher verification covers all option values, configuration round-trips, layout containment at several window sizes and DPI levels, scrolling and no-console Save & Play. Both the development build and private build workflow completed successfully.
