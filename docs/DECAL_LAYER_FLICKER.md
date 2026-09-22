# Coplanar decal flicker

The floor_flicker save in Feeding Tower B places two retail surfaces at z=448.5:
textures/decals/doormat (alpha-tested, lit, depth-writing) and
textures/feedingtower/ftower_decal_grime6 (blended grime, no depth writes).
Both request polygonOffset 1. Their identical depth bias lets rasterization
rounding reject alternating strips of the grime against the doormat. The
pattern changes with the viewpoint. Increasing the shared bias or near plane
does not separate these layers. Disabling ambient stages removes the grime
and the strips; disabling bloom does not.

The shared material polygon-offset helper gives translucent polygon-offset
materials 1% additional units bias (6 depth units at the default -600), leaving
the slope factor unchanged. Opaque/perforated materials retain their existing
bias and matching depth/lighting passes. This applies to all such materials in
normal views and portal subviews without replacing retail assets or moving
geometry. It addresses this layered-decal cause, not arbitrary intersecting
opaque meshes. Explicit per-stage private offsets retain their authored values.

Validation: hidden, muted isolated profiles with engine screenshots at 1920x1080.
The original save and four nearby camera poses reproduce strips in the old
build and show continuous grime in the candidate. Additional captures cover
bloom disabled and a renderer restart with 4x MSAA. Local evidence and scripts
are in validation/floor-flicker, outside the source repository. Game physics,
saves and the game DLL are unchanged.
