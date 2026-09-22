# Independent framerate: first-person playtest build

Start `Play-Prey2006-144FPS.cmd` in the workspace root, or `Play-Prey2006-RecoveredBloom.cmd` to retain the recovered bloom setup. Both now use `validation/presentation-build`. It uses the rebuilt executable, copied retail assets, a separate `fps-play-profile`, 1280x720 windowed rendering, vsync off and a 144 FPS cap. The original launcher remains available. No retail installation files are changed.

## Controls

- `com_unlockedFPS 1`: render independently of the fixed simulation clock. Default is 0.
- `com_maxFPS 144`: rendering cap; 0 is uncapped. Vsync can still constrain presentation.
- `g_interpolateView 0` / `1`: compare stepped and interpolated first-person views while retaining the same rendering rate.
- `g_interpolateWorld 0` / `1`: compare world/entity and skeletal pose interpolation.
- `g_interpolateWeapons 0` / `1`: compare weapon-local animation, bob, recoil and attached light interpolation.
- `g_interpolateEffects 0` / `1`: compare fixed-tick and interpolated material/particle display time.
- `g_lateMouse 1`: opt-in non-consuming mouse preview; default is 0. Requires normal first-person mouse look and `m_smooth 1`. The reticle continues to mark the simulation's aim point, so it can briefly move away from screen center.
- `com_unlockedFPS 0`: return to legacy tick-gated rendering.

## Changes

The simulation still uses its original 16 ms step. Presentation can run zero, one or several times per game tick without synthesizing additional user commands. A fractional millisecond deadline limits rendering; millisecond timer resolution and OS scheduling still introduce some pacing jitter. Existing catch-up limits remain intact. Menus, demos, editors, network play and modified simulation-clock settings retain their existing scheduling paths.

Camera origin, rotation (quaternion slerp), and FOV interpolate between adjacent fixed-tick views. First-person renderer entities are shifted with that camera. Authoritative camera/entity state is restored after drawing; physics and aiming are not interpolated. The fraction uses a coherent timestamp/tick snapshot from the async clock. The engine/game interface version is now 15, so use the rebuilt executable and game DLL together.

Interpolation resets on map shutdown, save restore and player orientation/teleport changes. It also rejects mismatched player identity, non-adjacent ticks, camera mode transitions, changed gravity direction, large positional/rotational jumps, third-person views, spectators, vehicles and cinematics. These views remain usable but do not get this interpolation pass.

Damage-splash drift and low-health overlay smoothing now update on simulation ticks rather than draws. Sound-driven camera shake was already tick-based.

## Scope and playtest focus

This remains an experimental first-person playtest build. It now interpolates ordinary moving entities, skeletal poses, first-person weapon animation, muzzle/nozzle lights, bound FX and the renderer's material/particle clock. It adds CPU work and transient pose memory. Camera/world interpolation introduces up to one tick of display delay; optional late mouse preview reduces look delay without changing gameplay input or physics. Custom callbacks, special world-spanning weapon beams, multiplayer and some special views retain their existing paths. Full campaign, all weapon variants, vehicles, wallwalking and spirit/deathwalk transitions still need broader playtesting. See [the work plan](FRAMERATE_WORK_PLAN.md) for implementation and test evidence.

Please compare walking and looking around at 60/120/144 FPS, watch the weapon against walls, and try portals, wallwalking, combat, spiritwalk and save/load. Report visible jumps, weapon sliding, aiming lag or effects that seem to change speed. `g_interpolateView` gives a direct A/B comparison without changing the FPS cap.

## Reproducible validation

Workspace scripts:

- `run_fps_validation.ps1`: isolated windowed profiles, identical dmescher2 portal viewpoint, legacy/30/60/120/144/165/uncapped tests, screenshots, console logs and CSV timings.
- `run_fps_validation.ps1 -Caps 144 -Turn 15 -Interpolate 1` (and 0): deterministic tick-based yaw input; compare intermediate camera angles at the same simulation time.
- `run_fps_transitions.ps1`: capture, save, teleport to a different pose, reload, capture again, and verify interpolation continues afterward.
- `summarize_fps_validation.py`: checks caps within 5%, simulation time against counted 16 ms ticks, and game/wall-clock divergence under 50 ms per sample.
- `summarize_fps_motion.py`: requires intermediate camera views on more than 95% of same-tick frame pairs with interpolation enabled, none in the disabled control, and an adjusted first-person model.

The tests use config scripts because the original engine startup parser has a fixed 32-command array. `wait` counts command-buffer processing rather than milliseconds, so use measured CSV duration. Keep benchmarks sequential. Sound remains enabled but muted. Transition tests leave view effects enabled; the rate/turn tests disable them to isolate pacing and interpolation.

`com_fpsTrace 1` writes `fps-trace.csv` and `fps-presentation.csv` into the save profile. Set it to 0 to close the files; subsequent captures overwrite them. These are CPU Session/draw measurements, not GPU scanout or input-latency measurements. `com_fpsTestTurn` injects diagnostic yaw in degrees/second on simulation ticks; it is non-archived, disabled by default and restricted away from network/demo paths. Do not use it for ordinary play.

The previous compatibility executable is preserved in `validation/fps-baseline`. Test profiles, console output, screenshots and summary tables live in `validation/`. Build logs are in `Prey2006/build/compat/fps-*-build.log`.

## Measured results

Release build succeeded. The seven-rate sweep passed the timing assertions; game time differed from wall time by at most 9 ms over these samples. These are static-scene measurements, not a general performance guarantee.
| Setting | Session FPS | Wall seconds | Game seconds | Zero-tick frames |
|---|---:|---:|---:|---:|
| Legacy | 62.51 | 8.591 | 8.592 | 0 |
| 30 | 30.00 | 14.266 | 14.272 | 0 |
| 60 | 60.01 | 7.565 | 7.568 | 5 |
| 120 | 120.01 | 6.333 | 6.336 | 365 |
| 144 | 144.01 | 6.937 | 6.928 | 566 |
| 165 | 165.05 | 6.065 | 6.064 | 622 |
| Uncapped | 687.65 | 2.843 | 2.848 | 1778 |

At 144 FPS with diagnostic turning, the final interpolation run changed the rendered camera angle on all 566 same-tick frame pairs; the disabled control changed none of 566 pairs. The enabled trace reported one adjusted first-person model. Save/teleport/reload exited successfully, restored the saved coordinates `(290, -1440, 800.25)` and yaw `0`, and produced valid interpolated views afterward. Captures were visually inspected for portal and weapon placement. These checks do not prove full campaign or combat equivalence.

## Keyboard defaults and profile isolation correction

Playtest feedback exposed an empty `base/default.cfg` in the port overlay, which masked the retail default bindings. It now supplies the 46 retail keyboard/mouse bindings; custom `prey06.cfg` bindings still load afterward. The FPS playtest profile has been seeded with those defaults. An engine startup/listBinds check confirmed movement, attack, spiritwalk, weapon switching and quicksave/load bindings (plus the engine's F10 settings shortcut).

Both launchers and the three validation runners now set `fs_configpath` alongside `fs_savepath`. Earlier runs isolated saves but still shared the engine's default config location; they were not fully configuration-isolated. Future runs write settings within their respective profile folders. Existing saves and the former shared config were left intact.

## Live-package replacement crash and build guard

The keyboard-default packaging update was mistakenly run while the user's existing game process was still open. The subsequent map transition reported `guis/mainmenu.gui` at ZIP directory position 9079667. That position exactly matches the previous pak007 archive; the replacement moved the entry to 9080159. The replacement archive passes a full ZIP CRC check, and the menu file's CRC/content are unchanged. This establishes stale cached archive offsets rather than a damaged menu asset as the cause of that reported error.

Recovery is to fully exit the old process, restart, and load an existing save. The user's `bar` and `bar_fight` saves were found and left untouched; completing their level transition after restart remains a user verification step.

`PackagePk4.cmake` now calls `AssertGameStopped.ps1` on Windows before changing output files. It blocks packaging while a prey06 process uses that output directory (and conservatively blocks a process whose path cannot be read). The generated CMake script was tested against the running PID: it failed before mutation, and the pak007 SHA-256 stayed identical. This guard protects package generation; it is not an OS-level lock against every external archive-writing program.

## Follow-up validation

All automated playtests use `+set s_volume_dB -60`; the engine maps this to zero
gain while leaving sound processing enabled. Normal player profiles keep their volume.

The added runners are `run_menu_validation.ps1`, `run_world_validation.ps1`,
`run_weapon_validation.ps1`, `run_late_validation.ps1`,
`run_effects_validation.ps1`, and `run_release_validation.ps1`.
`summarize_effects_validation.py` checks timings and gameplay ammo state against
fixed-tick expectations and across rendering rates. Earlier validation below is
historical; the follow-up work and limitations are recorded in FRAMERATE_WORK_PLAN.md.
