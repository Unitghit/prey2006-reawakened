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
