# Bloom through portals

Retail bloom (`r_glowMode 2`) now runs for direct portal views and authored
`portalRenderMap` captures. `r_glowPortals` defaults to 1; set it to 0 for an
immediate comparison with the previous behavior. The switch is not archived.
The main-view bloom settings, including resolution and kernel, also apply to
eligible portal views. Mirrors, remote cameras, skyboxes, editor views and the
two older bloom modes retain their previous eligibility rules.

## Rendering and clipping

Subview commands execute before their parent view, without recursive backend
draw calls. Each portal finishes its mask, blur and overlay before the shared
scratch images are reused. The parent depth pass preserves the direct portal
surface and covers the area outside its actual outline with foreground scene
geometry. Render-to-texture portals capture the completed bloom with the scene.

The glow prepass saves the full color viewport and clears the full scratch
viewport to black. Geometry retains its scaled, intersected scissors and the
portal view's near clipping plane. Blurring runs over the full scratch viewport,
so it cannot sample leftover scene color outside a small portal scissor. The
entire saved color viewport is restored afterward, preserving earlier sibling
and nested views. The final additive overlay uses the original view scissor;
it does not write depth. Child bloom is retained in scene color, not inserted
again into its parent's glow mask.

This adds GPU work for each visible portal view with active glow stages. It does
not perform additional gameplay updates. No claim is made that retail Prey
enabled bloom through portals: the earlier retail capture established the blur
sequence, not this particular rendering behavior.

## Validation

Evidence is in `../../validation/portal-bloom/`. The user's `portal_bloom` save
in Feeding Tower B provides a real scene with glowing lights beyond a portal.
At 1280x720, the fixed-camera native-resolution on/off comparison changes 23,162
pixels, all inside the visible remote scene (bounding rectangle x=649..860,
y=262..474). The foreground weapon and surrounding room are unchanged. Returning
to native resolution after 256 and 512 captures reproduces the native image
exactly. All automated sessions use `s_volume_dB -60` in an isolated profile.

At 1920x1080, an angled Feeding Tower B view and the `dmescher2` portal scene
completed shader reload, renderer restart, save reload, map transition and menu
checks. These are visual/stability checks, not exact pixel comparisons: some
presentation effects continue evolving independently of the stopped simulation.
The attempted scripted forward movement was unsupported, so it did not validate
traversal. Actual traversal, nested portal scenes and `portalRenderMap` captures
remain additional coverage opportunities. No performance benchmark was taken.

`validation/portal-bloom/verify.py` checks the final deployed executable against
the build output, archive CRCs, packaged game DLL and symbol maps, the isolated
mute setting, the remote-scene-only pixel changes and exact native repeat.
Results are recorded in `validation/portal-bloom/report.json`.

## Local deployment

The update is in `validation/portal-bloom-build`; the previous
`validation/presentation-build` remains intact because it was running during
deployment. AllFixes, AlienText, TranslatedText and 144FPS launchers point to the
new build. The existing player profile and saves remain in use. Restart through
one of these launchers to load the new executable. The older RecoveredBloom
comparison launcher still uses mode 1 and the previous build.
