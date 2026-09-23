# Uneven wall and ceiling placement

The bounded ground-fitting fallback now works in the hit surface's local frame
for walls and ceilings too. It retains the same sample coverage, eight-unit
relief/translation limits, normal-deviation limit, and full-aperture solid
clearance test. The opening stays flat and is offset beyond the sampled bumps.
The exact flat-surface path is unchanged.

Standing-player clearance is centered tangentially on the opening, keeping the
back of the hull in front of the fitted plane. This makes the clearance check
appropriate for wall openings instead of placing the player's feet at their
center. Floors and ceilings use the same orientation-independent calculation.

No new setting or save format change; this extends the existing portal tool.

Validation: the independently authored rough-ground fixture is rotated into
wall and ceiling orientations. The previous build rejects both; the new build
fits both and crosses each once. `rough_surfaces.cfg` and the test harness check
placement, fitting, and physical traversal. Ground fitting, static meshes,
original surface clearance, ceiling entry, and blocked-exit regressions are
also checked. Automated tests are hidden and muted.

The existing small-obstacle fixture now has a valid shifted placement above the obstacle. Its assertion checks oval clearance instead of requiring rejection. A new narrow-gap fixture blocks the whole search region and must still reject placement.
