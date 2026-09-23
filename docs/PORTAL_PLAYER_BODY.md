# Player models across portals

The local player's third-person body, head and eligible held-world-model attachments are rendered as complementary pieces while they intersect an open teleport portal. The entrance piece retains the material and lighting of that room; the other piece uses the same posed mesh transformed into the linked room. Both participate in normal render interactions and shadow generation.

## Implementation

`game_portal_body.h` runs after presentation interpolation and before rendering. It clips the posed triangles against the entrance plane and aperture volume, including the portal gun's visible oval. The pieces share the same sampled animation; no second actor, physics object, animation controller or damage target is created. Original renderer definitions are restored after each draw. Temporary models and handles are cleared with presentation resets and map shutdown, and are rebuilt on save restore without changing the save format.

`hhPortal::GetBodyPortalTransform` uses the current entrance and destination transforms, so the math also applies to moving and differently oriented portals. The current implementation selects the nearest eligible portal intersecting the local player's posed bounds. It does not duplicate NPCs or multiplayer players, and leaves vehicles, spirit/death walking and cinematics on their existing paths.

Eye materials require their original three triangle islands to compute UV projection. `Model_eyeball.h` shares that existing projection between the normal renderer and the split-mesh builder; eye projection happens before clipping and is not applied twice. A transient eye-position exclusion prevents a portal's virtual camera from drawing the inside of its own player copy. The older crossing-frame body suppression remains a fallback when no split piece is available.

`g_portalBodySplit` defaults to 1 (automatic rendering fix); 0 is an A/B diagnostic. `g_portalBodyTrace` logs piece counts and coarse integer build time. No extra launcher setting or old BAT preset changes are needed.

## Validation

- `multiportal3`: before/after captures show the missing section of Tommy restored through the portal.
- Unlocked 144 FPS target, forward/reverse crossing captures: no first-person head spikes after the virtual-eye exclusion. Position/velocity invariants remain unchanged.
- New `body_split` fixture checks wall overlap, save/reload, floor overlap and cleanup after moving away; rejects eye-deform errors.
- Existing `input`, `floor_continuous`, `reverse`, `deep_views`, and `through_portals` tests pass in hidden, muted profiles.
- Development and private engine/game builds and private launcher verification.

The renderer-only `benchmark view` command bypasses game-side presentation setup, so its timings must not be used to claim the cost of this feature. The diagnostic build time is millisecond resolution, not a precise GPU or complete frame benchmark.

## Remote-half visibility correction

The remote piece must clear `suppressSurfaceInViewID` and `suppressShadowInViewID`: those flags refer to the player's main camera globally, not to a location. Inheriting them hid the emerging half even when it was directly visible in front of the exit. Its transformed-eye proximity exclusion remains active to avoid first-person head geometry during crossing.

The `body_split` regression now points from the entrance toward the exit and requires an actual remote mesh submission in view ID 1. Merely allocating a remote mesh is not evidence of visibility. `multiportal3` comparison captures and unlocked forward/reverse crossing captures verify the corrected silhouette.

## Boundary depth continuity

A later `multiportal3` save revealed a narrow missing band, caused by the wall-mounted aperture's polygon offset overtaking the player and the 0.25-unit wall-exclusion clip margin trimming the other half. Split meshes for portal-gun pairs now carry a transient depth-bias marker. Depth, ambient, light-interaction, blend-light and fog passes all use the same offset as the opaque aperture. Interaction meshes inherit it through their ambient surface; no unrelated materials or global offset CVars change.

The body alone excludes the world-only clip margin. A 0.25-unit rendering-only overlap across the shared cut, also allowed by the body clip plane, prevents the two separately resolved MSAA images from exposing background between their edge samples. World surfaces retain their wall-exclusion margin. The gun aperture is flattened to the teleport plane, removing its previous 0.06-unit visual displacement while leaving its decorative rims and physical portal placement untouched. This aligns the complementary mesh cuts with the visible boundary.

Verification includes the updated `multiportal3` save at zero and 4x MSAA, before/after silhouette captures, and the body, nested-view and decal-mask regression fixtures.

The updated crossing test also exercises attachments that temporarily remain wholly on one side. Those uncut attachments now retain the virtual-eye exclusion, preventing an inside-head flash while another part of the body straddles the opening.
