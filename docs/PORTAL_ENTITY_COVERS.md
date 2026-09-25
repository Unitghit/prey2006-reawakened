# Separate portal collision covers and cached collision materials

The updated retail `portalstuck` save links a roadhouse window to the raised area. Destination collision is covered by both a separate `func_static` player-clip brush and a stationary `func_mover` player-push brush. Previously, cover sampling applied only to world collision. The map uses a beveled cover face, approximately 45 degrees from the portal normal.

`RW_StaticPortalPlane` now samples the queried model alone, using its actual collision-model contents rather than the entity's broad-phase contents mask. A pure player-clip model may supply a cover plane if it is static or a stationary unbound mover, with no solid contents. The probe remains bounded to 32 units in front of the support, accepts bevels up to 45 degrees, and transforms the detected plane into the model's local coordinates. Normal aperture fitting and contact clipping still apply. Moving movers, solid geometry, loose props, and characters retain normal collision.

The same save exposed a crash in blocked-exit diagnostic output: `.cm` brush parsing did not initialize `cm_brush_t::material`, although the format stores no material field. Initialize it to null so position-test contacts cannot expose an uninitialized material pointer. Brush contents and collision behavior are unchanged. This requires the updated executable as well as the game DLL.

Validation: retail save traversal; previous window save in both directions; new `cover_static` and `cover_mover` tests with beveled entity covers; existing fixed-fixture, blocked-exit, object, tapered-shell, column, partial-floor, sloped-ceiling, and occupant-replacement tests. Retail saves are local test inputs and are not distributed.

The reverse crossing also exposed an overly strict step-up angle test. The upward sample in a near-upright portal plane now accepts a tilt up to the existing wall-aperture alignment threshold (dot product 0.95). It still requires a walkable floor contact, a reverse sweep, source-side clearance, and the unchanged maximum step height. The saved pair passes both directions, with two-unit and fourteen-unit exit corrections.

## Cover-depth boundary on approach

The later return-side save approached the broad face of the same cover. That face lies exactly 32 units ahead of the portal support. Starting the cover probe at 32 units produced a zero-fraction contact, so the cover was missed and the player stopped before reaching the portal. Both world and entity cover probes now start at 33 units, while accepting the detected face only within 32 units plus the existing 0.15-unit plane tolerance. This changes the probe's start margin, not the permitted cover depth.

New `cover_limit` and `cover_far` regressions verify that the 32-unit cover passes and a 34-unit cover still blocks. Static, mover, and tapered cover regressions also pass; the updated retail save traverses its pair from both approaches.
