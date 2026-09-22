# Retail sun-corona parity

Read-only inspection of the running original PREY.exe PID 42376 and its loaded
gamex86.dll (base 0x1ff40000) confirmed the separate sun-corona draw call:
0x200e928a loads the sunCorona pointer, tests it, pushes the player and calls
0x1ff8af20. That function is only `ret 4`. The entity is still spawned, saved,
and restored; the retail screen-space draw is deliberately a no-op.
Evidence: validation/sun-bloom/game-live.bin, corona-refs.py and
retail-corona-draw.asm. Addresses are specific to this loaded module.

Our hhSunCorona::Draw still drew a 1280x960 additive flare using a legacy FOV
projection. It created the giant bright spot over the otherwise correct sky.
Make this draw method a no-op to match retail, preserving the entity and save
layout. Authored sun materials, sky bloom, and other view effects remain active.

Earlier sky tests used g_skipViewEffects 1, inadvertently hiding this bug.
New validation/sun-corona/baseline.ps1 explicitly enables effects and reproduces
the white glare, then disables effects to isolate it. test.ps1 uses the fixed
build with effects enabled and shows the small authored yellow sun. Muted
Release tests loaded sunbright successfully and exited normally. Before/after
captures: validation/sun-corona/comparison.png. Builds and configurator --verify
passed. Settings/custom launcher target validation/sun-corona-build.
