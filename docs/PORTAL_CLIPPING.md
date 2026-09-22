# Portal geometry clipping and skybox ordering

Backing away from a portal should move its virtual eye backward as well: that
preserves perspective. Geometry between this eye and the destination portal
plane must be rejected. The camera transform and portal plane remain unchanged.

## Reproduced failures

The user's `portal_bloom` save in Feeding Tower B has a portal at
`(224, 6592, 1536)`, facing approximately `(-0.241922, 0.970296, 0)`. Fixed test
viewpoints face yaw -76 and back away along that normal, rather than a world axis.

At 64 units, the previous build draws a dark arch through the remote corridor.
The renderer binds its alpha-notch clip texture during the depth pass, but
opaque surfaces do not enable alpha testing. The fix tests that texture for
solid depth draws, then restores the disabled state. Perforated materials keep
their own alpha-test thresholds.

Depth clipping cannot cover translucent geometry that does not write depth.
An eye-space OpenGL clip plane now bounds ambient/material passes (including
world-space refraction) and translucent light interactions. Its world-space
equation is transformed with the view's world model-view matrix, independent
of individual entity transforms. It is disabled before stencil shadow volumes
and screen-space bloom; ordinary views and HUD rendering have no subview plane.

At 192 and 256 units, a second issue replaces the portal view with the starfield.
Diagnostic view logging at 192 units showed a clipped portal command followed
by an unclipped skybox command, then the main view. Direct skyboxes and portals
share the framebuffer and the same material sort class. Their changing tie
order allowed the background to overwrite the portal's completed image.
Subview generation now schedules direct skyboxes first, then other subviews,
preserving the relative order within each group and the existing recursion gates.

At 8 units from the plane, enabling GL error reporting exposed an empty light
interaction scissor being submitted as `(32000, 32000, -63999, -63999)`.
`near.trace` and `near-calls.txt` identify the rejected `glScissor` calls.
Both individual light interactions and the shared shadow/fog surface-chain
renderer now skip empty scissors when scissoring is enabled, preventing both
`GL_INVALID_VALUE` and reuse of stale clipping bounds. The traced failing draw
was a stencil shadow volume.

## Evidence

`../../run_portal_clipping_validation.ps1` runs muted, isolated sessions against
the previous portal-bloom build and the candidate. It copies the user's saves;
it does not modify them. Captures are under `../../validation/portal-clipping/`.
The main sweep covers 32, 64, 128, 192 and 256 units and also loads `multi_portals`.
Baseline/candidate image differences at 192 units are confined to the portal.
The archived diagnostic `candidate/base/debug.log` records the faulty ordering;
temporary logging is removed from the final source.

The release smoke test additionally covers an 8-unit close-up, bloom at native
and 256-square resolutions, shader reload, renderer restart, menu transitions,
both portal saves and the bar save. Automated sessions use `s_volume_dB -60`.
The final checks and package hashes are recorded by
`validation/portal-clipping/verify.py` in `report.json`. The new deployment lives
in `validation/portal-clipping-build`, with AllFixes, AlienText, TranslatedText
and 144FPS launchers updated to use it and the existing player profile.

This is targeted coverage, not a complete campaign or multi-GPU certification.
The fixed viewpoint sweep tests rendering as distance changes; it does not
simulate walking through a portal or prove every nested-portal arrangement.

The installed retail executable and the user's running games were not modified.
This fix follows the local renderer's intended clip plane and the reproduced
framebuffer-order failure; it does not claim a new retail-engine capture.

## Subsequent alignment correction

[Portal alignment](PORTAL_ALIGNMENT.md) now places the remote camera and clip
plane at the physical exit transform. Only the visibility-area seed is biased
inside the destination. The depth/ambient/translucent clipping and skybox
ordering fixes described above remain active.
