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

## Per-sample cutout testing

The remaining edge stipple is addressed with GL_ARB_sample_shading (or core
OpenGL 4). Alpha is evaluated at each MSAA sample's own texture coordinate with
the authored cutoff, instead of estimating a coverage mask from one lookup.
Only the alpha-tested depth draw enables sample shading; it is disabled before
subsequent passes. Lighting reuses the sample depth coverage. Unsupported GPUs
retain alpha-to-coverage; single-sample rendering retains ordinary alpha tests.
The actual GL_SAMPLES count is queried during context initialization/restart,
so changing a pending r_multiSamples value cannot select an incompatible path.
Reference: https://registry.khronos.org/OpenGL/extensions/ARB/ARB_sample_shading.txt

Validation correction: startup hardware settings reset MSAA in some earlier
isolated profiles, so their initial-view comparisons did not exercise either
MSAA path. The test script now explicitly supplies r_multiSamples at startup.
The new engine also logs the actual framebuffer sample count. The prior
multi-restart smoke tests exercised settings after vid_restart, but initial
MSAA visual claims should be treated as superseded by the following tests.

At verified 4x MSAA, dense-coverage4 and dense-samples4 compare 32 fixed-position
camera steps of 0.02 degrees in floorsubtle. The sampled silhouette loses the
coverage fallback's visible stipple. In a fixed left-edge crop (540,430 to
780,820), the mean absolute temporal second difference among pixels affected
by the change fell from 1.008 to 0.808 RGB levels. This is a diagnostic for this
particular sweep, not a universal perceived-flicker percentage. Finite sample
counts still permit ordinary subpixel aliasing. The samples-restart portal
smoke test logged actual sample counts 4,0,4,8 and no GL errors. All runs were
hidden, muted, and isolated; the player's running game was left untouched.

## Angle-dependent overlay bands (floorsubtle2)

The newer save reproduces horizontal bands across the flat mat even with
specular and bump disabled. Skipping ambient passes removes them. This is
remaining overlay depth conflict, not solely cutout-edge aliasing; the earlier
interpretation that the residual was entirely alpha aliasing was incomplete.
The loose-material bias experiments did not establish a changed parsed
material and are not evidence against this diagnosis. Runtime global bias
changes and the compiled shared-helper change do separate the layers.

The units-only separation did not cover sample-depth differences on sloped
screen-space polygons. RB_SetMaterialPolygonOffset now also separates blended
polygon-offset materials by one screen-space depth slope in the direction of
their authored units offset. The existing 1% units guard covers nearly face-on
surfaces. A zero units offset retains the caller's slope; positive/negative
units preserve their direction. Opaque/perforated prepass and lighting still
use exactly their former bias. No material names, map geometry, brightness,
or specular settings are patched. This applies to blended polygon-offset
materials in all normal and portal views.

Hidden muted tests: subtle2-layers isolates the passes; subtle2-slope removes
the saved-view bands; subtle2-angles covers 16 yaw/pitch combinations with the
player position fixed; subtle2-slow-fixed and subtle2-slow-old compare 24
view-only steps of 0.05 degrees. Final renderer restarts log actual MSAA counts
4,0,8 and bloom-off is also exercised. No GL errors were reported. The runtime
stage is decal-slope-build. Prior edge anti-aliasing remains enabled but is
not the mechanism fixing these overlay bands.
