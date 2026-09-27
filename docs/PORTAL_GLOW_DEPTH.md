# Portal glow depth stability

The `catwalkflicker` save on Harvester B exposed whole sections of the nearby
wallwalk emission turning on and off when looking around through a scripted
portal. This was separate from the material's intentional jitter-table pulse.

The depth prepass cuts subviews using an alpha-notch texture. Ambient stages
also enabled a hardware clip plane, generating different clipped triangles.
Their interpolated depth could then differ from the prepass, failing the exact
GL_EQUAL test. The bloom mask amplified the missing emission.

Ambient stages that use GL_EQUAL now rely on the already clipped depth buffer,
as opaque lighting interactions already do. Stages that cannot rely on equal
depth, including translucent and postprocess surfaces, retain the geometric
clip plane and per-body correction. No bloom strength, material, map geometry,
portal transform, shadow pass or texture asset changes are involved.

Validation: Release engine build; hidden muted twelve-angle before/after
capture sweep from catwalkflicker. The old build loses near catwalk glow at
several angles; the updated build keeps it across the sweep. See
`tools/portalgun/tests/catwalk_glow.cfg` for the local-save reproduction.

Additional hidden save/load visual checks: multiportal4 (player pieces), multiportal2 (nested views), nofog, portalred and floorsubtle2. All completed without a crash or new visible clipping problem.
