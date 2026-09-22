# Portal view and teleport alignment

The portal-crossing build preserved the remainder of the movement step, but
its visible portal transform still differed from its physical transform.
`hhPortal::GetRenderView` added `-bounds[0].x` along the exit normal to the
remote camera frame. `PortalEntity` and `TransformPortalPresentation` used the
actual destination origin. The portal_bloom pair measured a 4.000-unit view
bias and landing error in each direction, without any collision recovery.
This made arrival look like a backward step even with continuous physics.

The destination render frame now uses the physical portal origin and axis.
For a point p, both paths implement:

    destination + ((p - source) * sourceAxis.Transpose()) * diag(-1,-1,1) * destinationAxis

The renderer's initial visibility-area probe is still four units inside the
exit, but that probe only selects a world area. It no longer shifts the
projected eye or exit clipping plane. No player-position offset is added:
adding one would preserve the visual bias and distort reciprocal crossings.
The full movement remainder, collision recovery, velocity transform, bloom,
and temporary first-person body suppression remain in place.

Trace-gated `PORTAL_ALIGNMENT` records the remote render-frame bias and the
physical landing error relative to the visible mapped point. A nonzero landing
error remains valid when exit collision checking requires a real relocation;
we do not interpolate the player through solid geometry to hide that case.

Validation lives in `validation/portal-alignment`, using copied saves and
muted profiles. It includes a measured baseline, round trips at 144/360 caps,
matched static viewpoints immediately before/after the portal, a close/far
clipping sweep and multi_portals, and the existing save-load/menu/black-floor
smoke test. The build manifest records measured results and package hashes.
This is targeted single-player coverage, not every campaign portal or gravity
configuration. Deployment: `validation/portal-alignment-build`.
