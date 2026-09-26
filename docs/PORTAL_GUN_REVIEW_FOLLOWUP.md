# Portal gun review follow-up

The review's cleanup is applied without merging the aperture dimensions. Artwork backing (39 x 49 half-size), traversal clearance (47 x 71), the player corner allowance, flat foot clearance, entity trigger bounds and occupancy candidate bounds have distinct names. The original numbers are unchanged. Occupancy is a broad-phase filter followed by actual closing collision checks, not another definition of the visible opening.

The common single-player/base-game enable predicate now has one helper. Function-specific health, spirit, vehicle, cinematic and GUI checks remain. The simpler checks used to cancel flights or deselect a disabled tool retain their original semantics.

Cover-plane matching now uses the saved portal surface offset, as the collision opening already does. The spawnArg and legacy save migration remain. Fire presentation is requested only when the projectile successfully spawns. A launched shot that later fails surface validation still plays its firing animation and sound.

## Lookup measurement

Temporary instrumentation in an isolated Release build timed FindEntity plus one offset GetFloat per lookup. Three 1,000-call batches in the corner fixture took 93.4, 102.6 and 168.6 microseconds total (about 0.09 to 0.17 microseconds per lookup/read pair). The timer overhead is included. This is a local sample, not a worst-case performance guarantee; game time crosses a save reload, so it must not be used to calculate a frame rate for all batches.

FindEntity already uses the entity-name hash. No persistent cache was introduced: the measured cost does not justify new invalidation rules for replacement, removal, map shutdown and save restoration. Temporary timing code was removed before the final build.

## Regression coverage

The new corner fixture places a portal near the outer edge of a finite solid wall. Center entry must cross exactly once. At the side, a direct hull sweep must hit the solid wall; normal approach assistance may then guide the player into the valid opening and cross once. A movable cube also drops through a floor portal near that block's edge. This tests the current intentional clearance, rather than requiring artwork and hull bounds to match.

The rejected-fire fixture overrides the projectile definition only in the isolated test profile with a nonexistent spawn class. Both attack buttons must attempt and fail spawning without firing presentation or a flight launch. The override is removed after the case. Normal input and shot-flight tests cover successful shots and invalid-surface impacts separately.

Related cases cover occupant evacuation and blocked replacement, save/reload, the maximum actor-clip cover depth and the too-far negative control, floor corners and sloped ceiling exits. These fixtures are checked into tools/portalgun/tests and exercised through tools/portalgun/test.ps1. Personal save names are not stable regression identifiers and are not committed.

## Physics entry points

- neo/game/physics/Clip.cpp: cutout use in translations, motion, contacts and contents, with world/entity cover handling.
- neo/game/physics/Physics_Player.cpp: ground-entry and approach assistance.
- neo/game/physics/Physics_PreyPlayer.cpp: temporary hull-axis holding during crossing.
- neo/Prey/game_portal.cpp: crossing detection, bounded exit clearance, transformation and save migration upkeep.

Those mechanisms are intentionally not rewritten by this cleanup. Their geometry exceptions and limits are documented in the existing PORTAL_* notes and covered by the regression suite.

Validation: Release build and cases input, corner, replacement, replacement_occupants, cover_limit, cover_far, floor_corner, sloped_ceiling, shots and fire_rejected passed in hidden, muted sessions. The final DLL and game package were installed with matching SHA256 hashes and a backup of the previous build.
