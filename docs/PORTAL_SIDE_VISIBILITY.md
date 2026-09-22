# Portal side-angle visibility

The portalside save reached R_PreciseCullSurface and was rejected before generating a portal view. The old test rejected the entire mesh on the first back-facing triangle, assuming every subview aperture was a single planar polygon. Prey's portal geometry can contain multiple faces, so an edge facing away could reject the clearly visible opening.

For SC_PORTAL surfaces, back-facing triangles are now skipped individually while front-facing triangles contribute to the clipped screen bounds. Fully back-facing or off-screen apertures still produce no bounds and are rejected. Mirror behavior, the portal plane gate, destination clipping, distance limits and recursion limits remain unchanged. This fixes visibility rather than introducing an arbitrary angle multiplier.

The configurator/custom launcher uses portal-sides-build; this correctness fix is always included. Existing user options and legacy preset launchers are preserved.

Validation: muted baseline portalside capture logged PORTAL_CULL and a black opening. The fixed build logs PORTAL_RENDER and shows destination geometry from the same saved position. Two progressively farther side positions also retain the visible portal. A rear-side capture shows the opaque back/frame as expected. Loading multiportal afterward completed successfully. Build and diff whitespace checks passed. Screenshots/logs are under validation/portal-sides.
