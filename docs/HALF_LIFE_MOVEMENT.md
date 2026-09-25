# Optional Half-Life movement

The launcher exposes **Movement style**, retaining `g_bunnyHop` values 0 (Prey),
1 (Quake), and 2 (Painkiller) for existing settings. New values:

| Value | Style | Ordinary speed | Shift/run binding | Jump height | Normal gravity |
| --- | --- | --- | --- | --- | --- |
| 3 | Half-Life 1 (Classic) | 320 | Walk at 120 | 45 | 800 |
| 4 | Half-Life 2 (Old Engine) | 190 | Sprint at 320 | 21.33 | 600 |

Speeds and heights use engine units. The existing always-run preference affects
the run binding. `g_halfLifeAutoHop 1` allows holding jump; default 0 requires
release between jumps. It only affects the two Half-Life styles. Original Prey
remains the default, and the older named BAT presets are unchanged.

## Mechanics and references

The implementation is independently written within Prey's movement solver.
It reuses our existing Quake-derived directional air-acceleration rule: a
30-unit projected wish-speed limit and acceleration based on the uncapped
wish speed, with air acceleration 10. Jump input does not dilute horizontal
movement input. Ground friction is 4, ground acceleration 10, and stop speed
100. HL2 reduces airborne acceleration during the upward near-apex portion.

Classic HL1 deliberately omits the later 1.7x bunny-hop correction. Its normal
jump impulse at gravity 800 is sqrt(2 * 800 * 45). HL2 uses the old SDK's signed
forward-input jump boost (50% normally, 10% sprinting), without the later
overspeed subtraction that produces accelerated backwards hopping. At gravity
600 its normal jump impulse is 160.

Primary implementation references:

- [Valve HL1 movement](https://github.com/ValveSoftware/halflife/blob/master/pm_shared/pm_shared.c): air acceleration, friction and jump impulse; the later bunny-hop correction is intentionally omitted.
- [Valve's old SDK movement, preserved by AlliedModders](https://github.com/alliedmodders/hl2sdk/blob/episode1/game_shared/gamemovement.cpp): `CheckJumpButton`, `AirAccelerate`, `CategorizePosition`.
- [Old SDK movement defaults](https://github.com/alliedmodders/hl2sdk/blob/episode1/game_shared/movevars_shared.cpp).
- [ZHalfLifeV author's changelog](https://zolika.dev/mods/vhalflife/changelog): identifies its January 24, 2023 jump-boost conversion as 2006 HL2. The local compiled mod and INI are supporting references, not source code.

## Prey adaptations

These are movement styles, not replacements for Prey's entire physics engine.
Collision hulls, portal aperture collision, moving platforms, step handling,
swimming, ladders, spirit/death walking, and special wall-walk jumping remain
Prey's systems. The optional rules only apply to a living single-player body
on dry ground/in air. No save fields or network formats are added.

Gravity scaling is applied to movement acceleration, never written back into
zone gravity. This preserves zone direction and relative strength, prevents
compounded scaling during portal crossings, and leaves other entities alone.
Jump impulses scale with local acceleration to retain the selected jump height.
Finite ballistic safety ceilings are 2000/3500 total speed, rather than the
original engines' world-axis component limits, so changing gravity orientation
does not change the limit. Ordinary running speed is not a bunny-hop cap.

Lower jumps can prevent reaching campaign ledges intended for Prey's 64-unit
jump. Switching the launcher back to Original Prey restores native movement.

## Verification

`tools/movement/test-halflife.ps1` launches hidden, muted isolated profiles.
`check-halflife.py` checks real engine traces for jump height, manual/automatic
jump behavior, successive old-HL2 boosts, air-strafe momentum, friction,
sideways gravity, save/reload, restoration of original movement, and a floor
portal crossing. `g_movementTrace` is a developer-only diagnostic, default off.
The launcher verifier checks round trips and launch arguments for each choice,
as well as layout at multiple sizes and DPI scales.
