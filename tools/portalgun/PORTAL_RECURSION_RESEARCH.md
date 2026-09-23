# Portal recursive rendering research

Reviewed 2026-09-23. Research only; no renderer changes.

Reference: SonicEraZoR/Portal-Base revision
`c4584551916acfb1e9583d54587ac84be48c9768`. This is a community adaptation of
Portal code to Source SDK 2013, not verification of the installed retail binary.
Use the mechanics as reference for an independent Prey implementation.

## What the code does

- Normal recursive views use stencil rendering. `r_portal_stencil_depth` defaults
  to 2; the effective limit also respects stencil capacity and the compile-time
  recursive-view limit.
- `_rt_DepthDoubler` is a fixed 512 by 512 texture. The separate `_rt_Portal1`
  and `_rt_Portal2` targets use full-frame dimensions. These are different paths.
- Before drawing first-person viewmodels, the renderer copies the current view
  into the depth-doubler texture if a linked, usable portal needs it.
- At the recursive cutoff, eligible portals draw a depth-doubler mesh instead
  of another world view. The material receives a saved alternate view matrix.
- The view matrix is updated after consuming the doubler. The code explicitly
  describes compensating for the captured image being one frame behind.
  This is temporal image reuse, not unlimited fresh low-resolution scene views.
- The usability calculation tests portals generally facing one another and
  normals within 45 degrees of opposition. However, the same function also
  unconditionally marks the linked portal usable. Do not describe this branch
  as a strict, reliable facing-only gate without that qualification.
- The complete depth-doubler shader is not present in the inspected files.
  Matrix plumbing and framebuffer capture are verified; exact shader sampling
  and retail visual equivalence remain unverified.

## Source locations

- [Target allocation](https://github.com/SonicEraZoR/Portal-Base/blob/c4584551916acfb1e9583d54587ac84be48c9768/sp/src/game/client/portal/portal_render_targets.cpp#L79)
- [Capture implementation](https://github.com/SonicEraZoR/Portal-Base/blob/c4584551916acfb1e9583d54587ac84be48c9768/sp/src/game/client/portal/PortalRender.cpp#L964)
- [Capture before viewmodels](https://github.com/SonicEraZoR/Portal-Base/blob/c4584551916acfb1e9583d54587ac84be48c9768/sp/src/game/client/viewrender.cpp#L1032)
- [Eligibility and saved matrix](https://github.com/SonicEraZoR/Portal-Base/blob/c4584551916acfb1e9583d54587ac84be48c9768/sp/src/game/client/portal/portalrenderable_flatbasic.cpp#L153)
- [Terminal mesh selection](https://github.com/SonicEraZoR/Portal-Base/blob/c4584551916acfb1e9583d54587ac84be48c9768/sp/src/game/client/portal/portalrenderable_flatbasic.cpp#L1172)

## Prey comparison and proposal

Our `tr_subview.cpp` currently defaults `r_portalMaxDepth` to 3 and permits 1 to
4 layers. Direct SC_PORTAL views share the framebuffer, with explicit aperture
background restoration. Portal render distance is a separate setting.

Start by retaining native resolution for the first two layers, then render deeper
views into reduced-resolution targets sized by their projected screen footprint.
Keep a total view budget as well as a depth cap: two children per portal can
multiply scene submissions even when each image is small. Lower resolution saves
pixel work, but does not remove CPU scene traversal, draw calls, or geometry work.

A later terminal image-reuse fallback could extend convincing repeated views
without more world draws. Cache by portal path and invalidate on replacement,
movement, teleportation and save loads. A single global captured image is not a
safe general solution for Prey's many independent portal pairs.

Preserve aperture masks, exit clipping, fog, skybox views, sibling views and bloom.
Verify that bloom is neither duplicated nor filtered inconsistently between
resolutions. Test facing loops, multiple sibling portals, moving portals, fast
camera turns, replacement, crossing and skybox nesting. Measure both CPU/GPU time
and visual differences before selecting quality defaults.
