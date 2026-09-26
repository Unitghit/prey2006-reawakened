# Portal placement spacing

Replace the fixed 160-unit center exclusion with an orientation-aware test of the visible oval openings. Each ellipse has a one-unit margin and a thin normal extent. Exact support projections prove separation; sampled axes are conservative for angled pairs. A broad-phase radius skips distant pairs.

Use the same test for the initial placement candidate and the fitted terrain pose, so reticle previews and actual placements agree. Existing traversal dimensions and collision rules are unchanged. Failed overlapping shots retain the existing endpoint and link.

Validation: portalclose's original aim places orange about 98 units from blue, compared with about 162 before. The close fixture places a pair 82 units apart, rejects an overlapping replacement, verifies the old endpoint remains intact, and walks through the adjacent pair. Reticle and replacement_occupants tests also pass. Tests run hidden and muted.
