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

## Remaining cutout-edge shimmer (floorsubtle)

The user clarified that the residual shimmer is at the mat outline. Increasing
the grime bias from 1 to 2 produced identical floor pixels at sampled nearby
poses, unlike the original overlapping-decal bands. The outline comes from
an alpha-tested texture. Standard MSAA geometry coverage does not smooth its
binary per-fragment alpha rejection.

RB_T_FillDepthBuffer now uses GL_SAMPLE_ALPHA_TO_COVERAGE for standard cutouts
when MSAA has more than one sample. It replaces the 0.5 alpha cutoff with a
positive-alpha test while writing sample coverage to depth. Equal-depth
lighting then shades only covered samples. Coverage is disabled immediately
after each draw, before other materials, lighting, UI, and bloom. Non-0.5
thresholds, alpha-modulated stages, and MSAA-off rendering keep the old path.
This is enabled by the launcher's existing MSAA setting, not a new option.
See https://registry.khronos.org/OpenGL/specs/gl/glspec20.pdf for multisample
fragment operations and alpha-to-coverage.

Hidden, muted validation compared 12 small camera position/angle steps from
floorsubtle. Captures show a smoother mat silhouette without changing the
interior lighting. A portal_bloom smoke test completed renderer restarts at
0x, 4x, and 8x MSAA with no reported GL errors. At MSAA off the 550,250 to
1100,650 floor crop matched the old build byte for byte. These screenshots
verify the edge treatment; they do not establish that every possible source
of temporal shimmer is eliminated. User movement testing remains valuable.
