# Nested portal rendering

The multiportal save demonstrated a visible second portal with no remoteRenderView. Entity::Present only populated that view inside the local player's PVS; the second portal also remained dormant and did not run Present. Recursive rendering reached it but returned without a destination.

Portal entities now populate destination frames independently of the direct-player PVS. Before single-player drawing, visible registered portal entities with missing remote views receive the frame from their camera target. This does not awaken their simulation or alter puzzle/teleport logic. The renderer can then use its existing recursive projection and destination clip plane.

r_portalMaxDepth bounds direct portal layers (1–4, default 3); the first portal counts as layer one. Existing same-surface cycle prevention remains active. Prey Settings exposes Portal nesting and defaults to three layers. Distance multipliers and material-authored limits are unchanged. Portal skybox behavior and remote camera rendering are not changed by this feature.

r_portalTrace is an off-by-default diagnostic that logs candidate culls, cycle rejection, missing destinations and generated portal views. In the reproduction it reported entity 2637 at depth 1 with remote 0 before the dormant-view repair and remote 1 afterward.

Validation: muted multiportal capture shows the inner aperture black at depth 1 and textured at depth 3 (mean RGB in a fixed inner-aperture crop: 0.16 vs 32.75). Renderer trace confirms the second portal generates its own view. Save/reload with nesting, RealMultiPortal and portal_bloom all completed NESTED_PASS. r_portalDistanceScale stayed at 3 throughout; no distance logic changed. Configurator layout tests passed all three widths and text scales.
