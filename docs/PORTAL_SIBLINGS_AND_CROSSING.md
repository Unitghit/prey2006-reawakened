# Repeated portal views and near-plane crossing

## Missing second portal in threeportals

The upper-left inner portal is entity 2574, the same physical surface as the entrance, seen from a different transformed viewpoint. The old same-surface recursion guard rejected it with PORTAL_CYCLE while entity 2553 rendered normally.

Direct SC_PORTAL views now rely on r_portalMaxDepth for bounded recursion, allowing that repeated surface to render. Mirrors and texture subviews keep the same-surface guard. Siblings do not consume each other's depth budget. No settings or default limits changed.

## Energy-portal crossing flash

The planar energy aperture introduced by the grazing-angle repair could fall inside the camera's three-unit near plane. Both precise subview culling and the parent's depth pass then clipped it, exposing local geometry behind the portal just before teleport.

On OpenGL 3.2 or ARB/NV depth-clamp hardware, direct portal apertures omit the homogeneous Z rejection and enable GL_DEPTH_CLAMP only while filling their depth mask. XY clipping, the portal plane gate, destination clipping and normal scene depth remain intact. Clamp state is restored before other surfaces draw. Older hardware lacking depth clamp retains the former clipping behavior. No player position, momentum or camera offsets change.

## Validation

Release engine and configurator builds and renderer diff whitespace checks passed. All game tests used isolated profiles and s_volume_dB -60.

- Baseline threeportals capture shows the upper-left inner portal black; the corrected capture renders both inner destinations.
- Nesting limits 1 through 4 were captured. Limit 1 excludes both inner views, while 2 and higher render both in this scene; the depth-limit trace confirms rejection at limit 1.
- A 144 FPS forward crossing from threeportals captured every rendered frame near the portal. The baseline sequence exposes the local wall in frames 003-005. The corrected sequence retains the destination throughout the crossing, with no physics-preservation assertion failures.
- portalrenderover retains the thin edge-on aperture; portalside retains the visible framed portal; multiportal retains its nested view. Sequential save loads completed cleanly.

Scripts, logs, captures and contact sheets are in validation/portal-siblings. Prey Settings and the custom launcher deploy validation/portal-siblings-build, preserving user selections. Reopen the settings program before launching.
