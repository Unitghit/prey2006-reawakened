# Framerate and stability work plan

Keep the original 16 ms gameplay simulation. Improve presentation without changing
physics, AI, scripts, collision, save compatibility, or authoritative aiming.
Implement and validate these items in order, using isolated builds and profiles.

1. **Menu deadlock - completed and validated.** Reproduce the invalid sound portal-area error;
   validate cached areas and make affected sound locks exception-safe. Test menu
   cycling and map changes, including feedingtowerb, with sound enabled.
2. **World interpolation - completed and validated.** Interpolate moving render entities and character poses.
   Reset history on spawn, removal, teleport, map/save transitions, and discontinuities.
   Check moving platforms, ragdolls, portals, shadows, and collision alignment.
3. **First-person animation and effects - completed and validated.** Smooth weapon-local animation, bob,
   recoil, and attached lights/effects together. Verify firing and reload timing.
4. **Mouse-look latency - completed and validated.** Add a selectable presentation-only late mouse update
   with explicit handling of sensitivity, input consumption, aiming, and crosshair
   consistency. Compare responsiveness against the current interpolation path.
5. **Visual timing audit - completed and validated.** Inspect render-time mutations in particles, trails,
   fades, overlays, and special views. Fix demonstrated frame-dependent behavior;
   validate equal durations and trajectories at 30, 60, 144, and uncapped rendering.

For each item, record the change, tests, limitations, and completion status here.
Keep comparison switches where useful. Never replace packages used by a running
game. Broaden interpolation to special views only when transition tests support it.

## Evidence and validation

The captured menu hang is documented in validation/menu-hang/FINDINGS.md in the
workspace. Existing FPS timing and camera checks are described in FPS_PACING.md.
Use regression tests for actual failure paths and isolated in-game captures for
visual changes. A successful build alone does not establish gameplay equivalence.

### Item 1 validation

Menu audio now has no map render world, cached emitter areas are checked against
NumAreas, and StartSound/ForegroundUpdate use exception-safe lock guards.
Release build passed. 80 real menu open/close cycles on each of feedingtowera and
feedingtowerb completed with sound enabled, plus a map change between them.
The standalone thread-lock regression test passed normal return, early return,
and nested exception cases. This is targeted coverage, not proof of every cause
of invalid area IDs. The original hung process and dump remain untouched.
A tested build is preserved in workspace validation/menu-fixed-build.

### Silent playtests

All automated playtests must start with `+set s_volume_dB -60`. The actual
master-volume CVar is s_volume_dB; the earlier s_volume argument was ineffective.
At -60 dB this engine returns exactly zero gain. Keep sound processing enabled
for sound regression tests. Do not alter normal player profiles or system volume.

### Item 2 validation

Renderer-only entity origins/orientations and local skeletal joints now interpolate
between adjacent ticks. Joint rotations use slerp and are composed down the bone
hierarchy. `g_interpolateWorld 0` provides a comparison switch. Custom render
callbacks, portal entities, and special camera modes keep their existing path.

Bar-fight tests at 60 and 144 FPS completed. The 144 FPS trace showed changing
joint poses in all 172 measured groups with multiple renders per simulation tick.
A pickup callback crash found in the portal-map transition test was fixed by
calling only the base animator refresh, not view-dependent derived callbacks.
Save/load and teleport tests on dmescher2 then exited with code 0. Forty menu
cycles across feedingtowera/b also exited with code 0. Ragdoll tests at 60 and 144
FPS each produced 123 identical physics samples. These targeted tests do not
constitute a complete campaign playthrough.
Evidence lives in workspace validation/world-*, validation/ragdoll-world-*, and
validation/world-crash. The test runners now check exit codes to prevent stale
artifacts from masking a failed rerun.

### Moving-platform follow-up (item 2)

Added an original convex box fixture and `g_presentationProbe`, an opt-in trace
that observes actual pusher contacts, simulation positions, displayed positions,
and physics state before/after drawing. The fixture carries the player up 64
units and back down. `run_platform_validation.ps1` uses isolated muted profiles;
`summarize_platform_validation.py` checks the resulting samples and deliberately
requires the disabled-interpolation control to exhibit the expected mismatch.

30, 60, 144 and uncapped runs completed both trips with every sampled ground
contact on the platform. Rendering never changed either participant's position;
repeated draws never advanced authoritative state. At 111 shared platform
positions/directions, player and eye positions matched exactly across all four
rates and the disabled control. Maximum camera/platform presentation correction
disagreement was 0.000062 units with interpolation enabled versus 0.512024 in
the disabled control. Small settling at the endpoints remains simulation-owned.
Evidence: workspace validation/platform-report.json and platform-*/base traces.
The platform follow-up originally used validation/ragdoll-build. Its diagnostics
and the later beam fixes are now also included in the delivered presentation-build;
the updated manifest records the matching binaries and evidence hashes.
These checks cover a vertical pusher, not arbitrary rotating platform behavior.

### Shadow follow-up (item 2)

`r_presentationTrace` records skeletal surface-position and transform hashes at
ambient submission, shadow construction and shadow submission. It is disabled
by default, requires com_fpsTrace, and closes its file when tracing stops or the
renderer shuts down. The bar-fight on/off comparison exercised seven shadowed
character/head models. With world interpolation enabled, all 1,400 visible
surface/shadow pairs matched, all 1,600 shadow submissions matched their recorded
construction state, and 609 simulation-tick groups changed shadow geometry
between draws. The disabled control had zero within-tick geometry changes;
1,393 visible/shadow pairs matched. Its cached shadows were checked against
their most recent recorded construction (1,576 of 1,592 submissions; the first
16 predated trace activation). The screenshot was visually checked for separation.

Evidence: workspace validation/shadow-report.json, shadows-144-*/base traces,
run_shadow_validation.ps1 and summarize_shadow_validation.py. This verifies
the shared animated geometry and transforms, not exhaustive GPU pixel equality.

### Item 3 validation

`g_interpolateWeapons` blends camera-local weapon transforms and skeletal poses.
It updates muzzle/nozzle light positions and rebases bound FX action definitions
with their displayed master/bone, then restores renderer state after the draw.
Newly spawned flashes retain tick-owned activation and expiration.

Rifle firing and wrench swings completed at 60/144 FPS. The 144 FPS capture
exercised up to two weapon lights and nine attached FX models. The comparison
with interpolation disabled had zero changing weapon poses within 1,033
multi-render ticks; interpolation enabled advanced animation within ticks.
Screenshots and traces are in validation/weapons-*. The tested build is preserved
in validation/weapon-build. The subsequent beam follow-up below covers the
leech gun's separate renderer path; these remain targeted combat tests rather
than a complete campaign playthrough.

### Beam and leech-light follow-up (items 3 and 5)

Beam-node arrays now have deep presentation history and separate display buffers.
World nodes interpolate in world space; canister nodes interpolate in camera
space. Reprojecting into beam-local space uses the actual matrix inverse, avoiding
a small uncapped-only error from assuming an interpolated matrix is perfectly
orthogonal. Bounds include the displayed nodes and beam thickness. Beam scripts,
random updates, hit traces, damage and activation/expiration remain tick-owned.

World beams attach to the displayed muzzle joint, including late mouse preview,
by shifting their near nodes with decreasing weight toward the unchanged far
endpoint. The leech gun's separate world light interpolates its position,
orientation and length without changing its lifetime. All altered renderer
definitions are restored after drawing. Diagnostic tracing checks that no draw
changes the simulation's node data.

`run_beam_validation.ps1` exercises actual extraction from an energy node, railgun
canisters/shot, plasma attached FX, and continuous sunbeam firing. Runs at 30,
60, 144 and uncapped FPS, a disabled control, and a late-mouse run passed
`summarize_beam_validation.py`. The final enabled traces have less than 0.000001
units of recorded muzzle attachment error and zero far-end displacement. The
simulation's beam data and ammo stay constant across repeated draws. Every run
consumes sunbeam ammo once per 16 ms, including when low FPS skips samples.
The 144 FPS enabled trace has 592 same-tick groups with changing beam geometry;
the disabled control has none. All phases retain the leech gun, checked in the
trace, so automatic weapon switching cannot produce a false pass.

Evidence: workspace validation/beam-report.json and beams-*/base. The runner uses
the existing giveEnergy cheat and isolated original box geometry for its target.
The held-input diagnostic now accepts 2 for alternate fire (1 remains primary).

### Item 4 validation

Added opt-in `g_lateMouse 1` (default 0). It reads accumulated input and pending
mouse motion under the input lock without consuming events, emitting commands,
or changing game aim. It respects sensitivity, pitch/yaw scales, inversion,
pitch limits, and input inhibition. Normal first-person mouse look with
`m_smooth 1` is supported; menus, active gamepad sticks, strafing, smoothing, spirit/deathwalk,
bound players, and unusual gravity retain the previous path. The reticle is
projected from the authoritative weapon eye trace while the camera previews input.

The engine/game API is now 15; keep executable and DLL together. The diagnostic
`fpsTestMouse` requires com_fpsTrace and verifies that repeated preview calls
leave every byte of input-generator state unchanged. Normal/inverted input,
smoothing fallback, menu inhibition, and 30/144 FPS runs passed. An additional
run with default gamepad support enabled verified that an idle device does not
disable mouse preview; active stick input retains the fallback. On/off runs ended
at exactly the same authoritative yaw (-19.797363 degrees). At 144 FPS, ten
captured frames displayed pending input before its simulation tick. This is
functional evidence, not an end-to-end mouse-to-monitor latency measurement.
Artifacts: validation/late-*.

### Special-view follow-up (items 2, 4 and 5)

`fpsTestView` queues diagnostic spirit/body and sideways/down-gravity changes
inside the next simulation tick, after presentation-history capture. It calls
the existing player transition methods, is gated by com_fpsTrace and single
player, and clears pending work on map shutdown. Applying a console change
before history capture would not exercise the intended discontinuity check.
Presentation traces now include mode, gravity and transition flags.

At 30 and 144 FPS, spiritwalk, third-person and sideways-gravity phases all ran
and returned to normal first-person rendering. Late mouse preview remained off
in those special modes; third person did not use the first-person interpolation
path. Spiritwalk and stable sideways gravity continued to interpolate. The
144 FPS capture contained ten mode/gravity transition samples, all with blending
and late look disabled. The 30 FPS run skipped over those single simulation
ticks but verified each mode and recovery. Spirit and sideways screenshots were
visually inspected. This uses the gravity/orientation methods rather than a
full wall-walk traversal, and does not exercise a vehicle or deathwalk level.

Evidence: workspace validation/view-report.json, views-*/base,
run_view_validation.ps1 and summarize_view_validation.py.

### Item 5 validation

Material and particle display time now interpolates between ticks; smoke geometry
uses the same display time while pool emission/expiration stays on gameplay time.
Damage-blob fade/drift display uses that clock. Timed overlay expiration moved to
the simulation update (multiplayer retains its client fallback), HUD ammo slots
now refresh as a complete snapshot, and sun-corona drawing uses the displayed view.
Pickup icon timers, kick spring integration, particle emission/trail creation and
beam processing were already driven by game time/ticks and were not globally sped
up or rescaled. Specialized effects and unusual camera modes remain a playtest area.

`run_effects_validation.ps1` exercised 30, 60, 144 and uncapped rendering, including
rifle firing/reloads through the dmescher2 portal scene. The deterministic fixture
disables health recharge only for that isolated test player. At 136 common
simulation timestamps, blob position/lifetime, low-health smoothing, overlay
expiration and ammo/reload state matched exactly across all four runs. Repeated
draws did not mutate those states. Measured rates: 30.006, 60.020, 143.982 and
884.822 FPS (the uncapped number is scene/hardware-specific).
See validation/effects-report.json and summarize_effects_validation.py.

### Delivery

Release build and the exception-lock regression test pass. Menu regression checks
completed 160 cycles on feedingtowera/b, followed by a successful final portal-map
save/load/teleport test. Explicit teleport invalidation prevents even short entity teleports from
blending across their discontinuity. Legacy scheduling skips pose-history work.

`Play-Prey2006-144FPS.cmd` and `Play-Prey2006-RecoveredBloom.cmd` now point to the
isolated `validation/presentation-build`, keeping their existing player profiles
and normal volume settings. Previous launcher copies are under that build's
launchers-before directory. Late mouse preview remains off by default.

The packaged build passed a further smoke test loading the existing `bar` and
`bar_fight` saves, cycling the menu ten times, switching third-person on/off,
switching legacy/unlocked pacing, and enabling late mouse preview. Screenshots
were checked after the load fade completed. At that delivery, package CRCs and
source/deployed binary hashes matched; see validation/presentation-build/build-manifest.json.
All automated game processes exited normally.

### Final delivery and completion audit

All five planned items are complete within the documented single-player scope.
FRAMERATE_COMPLETION_AUDIT.md maps each requirement to its checked evidence and
states the coverage limits. The final beam fix passed 30/60/144/uncapped tests,
an interpolation-disabled control and late mouse preview. The complete four-rate
effects test was rerun on the final source and passed with 136 identical shared
simulation samples. The recursive-lock regression and diff checks passed.

The updated executable and game packages are installed in validation/presentation-build,
which both FPS and recovered-bloom launchers already use. The delivered build
passed another bar/bar-fight/menu/mode smoke test and a separate portal-map
save/teleport/load test, restoring (290 -1440 800.25), yaw 0. Screenshots were
inspected. CRCs and deployed/source binary hashes match. The manifest records
changed-source hashes, evidence hashes and matching linker maps under symbols/.
The preceding playable binaries are preserved in validation/presentation-before-beams.
All playtests were muted and exited normally; normal player volume was preserved.
