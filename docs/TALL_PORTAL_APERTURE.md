# Tall doorway aperture side spill

The cullingissue save reproduces destination imagery to the right of the tall
metal doorway at a grazing angle. The retail portal64x148.ase aperture consists
of ten triangles with an outer rim at local x=0 and a smaller back face at x=-16.
The recessed sides therefore project outside the planar opening when viewed
from the side, especially with the expanded portal viewing-angle support.

Extend the existing frame-owned aperture projection to textures/portals/portal,
used by rectangular doorway portals. Project vertices onto local x=0 for both
precise subview bounds and the parent depth mask. This also covers the moving
version, using the entity's current transform. Frame models, collision bounds,
teleport placement and map assets are unchanged. Energy apertures retain their
existing material-specific projection.

Muted validation: cullingissue before/after removes the right-side sliver;
repositioning toward the front reveals the destination within the doorway.
weirdartifact renders without the old scratch rectangle. A crossing of the
moving-door model reports zero render bias and zero landing error. threeportals
renders both nested destinations. portalrenderover also loads. Release build
and configurator --verify passed. Evidence: validation/tall-portal.

Deployment: validation/portal-aperture-build via Prey Settings and custom
launcher. Existing user choices and legacy launchers preserved.
