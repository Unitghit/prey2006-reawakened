# Portal viewing distance

`r_portalDistanceScale` defaults to 3 (archived, range 1–16). All four current
play launchers explicitly select 3. Set it to 1 in the console to compare the
original authored ranges. A 600-unit cutoff becomes 1800 units; individualized
portal limits keep their relative differences.

The renderer scales positive direct-portal cutoffs, and evaluates portal
material distance expressions at actual distance / scale. This also expands
the fade-to-black interval instead of leaving a black overlay at the old limit.
Unlimited (zero) and explicitly disabled (negative) direct-portal cutoffs keep
their special meaning. Other materials and skybox views are not scaled.

Game-side visibility-area cutoffs use the same multiplier at comparison time.
Saved distances and shader parameters remain unmodified, so existing saves
work and repeated save/load cannot multiply the values again. Proximity opening
triggers (`distanceToggle`), scripted closures, occlusion, recursion and the
portal alignment/crossing fixes remain unchanged. A scripted closed portal
still needs its usual trigger before anything can be viewed through it.

Muted validation is under `validation/portal-distance`. The isolated probe
replaces the portal material's far/near parameters with 64/32 units so both
sides of the cutoff can be inspected within the existing room. At 128 units,
1x should be black and 3x should show the destination with its scaled fade;
at 224 units both should be beyond the cutoff. This material fixture is used
only in the test profile and is not deployed. The production build also runs
the save-load, menu, black-floor, spirit/third-person and multi-portal smoke test.
Deployment: `validation/portal-distance-build`.

## Corrupted-bar range follow-up (2026-09-21)

Muted comparisons using the user's `shortportals` save at 3x, 8x and 16x
confirmed that the existing distance multiplier expands these energy portals'
visibility. At 16x the left portal destination and a nested portal are visible
from the saved viewpoint instead of the dark interior seen at 3x.

Prey Settings now offers 16x and selects it by default. The user's saved
selection and custom launcher select 16x as well. The engine's standalone
CVar default remains 3; older preset launchers retain their existing choices.
No portal triggering, recursion depth, clipping, or map data changed.
More distant portal rendering can increase GPU/CPU work.

Evidence: `validation/short-portals/fixed/base/before.tga`, `scale8.tga`,
`scale16.tga`; configurator publish and `--verify` passed. Game tests used
isolated profiles with `s_volume_dB -60` and exited normally.
