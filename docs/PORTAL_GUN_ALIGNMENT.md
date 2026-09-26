# Portal shot rules and low-wall alignment

Gun shots and reticle previews follow campaign/scripted portals only. Gun-created portals expose the backing wall/floor to the shot, allowing same-color replacement and reorientation. Opposite-color overlap remains rejected by the existing opening test. Scripted chains, the eight-hop limit, and in-flight saves remain supported.

Low wall placement previously forced a 71-unit center height, inherited from the taller opening. It now derives minimum height from the current 49-unit half-height artwork and its conservative clearance prism. On a flat floor the result is 54 units; small floor trim is handled in one-unit increments before using the broader placement search. Impossible below-prism candidates are skipped without retracing their support samples. Existing saved endpoints are not repositioned merely by loading the save.

A higher source wall opening can map the standing player into a lower destination floor. Existing exit clearance now permits up to 32 units of upward recovery only when a reverse sweep identifies a walkable floor, destination occupancy is clear, and source movement and aperture fit are also clear. Partial floor entry uses the same bounded world-floor allowance, with source sweep and destination occupancy checks. Lateral clearance limits and wall/obstacle rejection remain unchanged.

Validation in hidden, muted profiles:
- portalalign: same saved aim places the opening at z=502 rather than z=519, above the z=448 floor.
- realign: different low-wall aim heights agree on the flat-floor placement; same-color floor shots rotate the existing portal locally without moving its partner.
- through_portals: same-color local replacement, rejected opposite-color overlap, scripted traversal, saved flight, chained portals and bounded loops.
- reticle, close, floor, blocked, body_split, replacement_occupants, rough_surfaces, npc, shots, surface_fit, corner, floor_clearance, cover_far, floor_corner and floor_partial checks.
- The blocked fixture now marks its obstacle NPC noPortal so it remains an obstruction rather than using the recently added NPC traversal feature.

No launcher choice or save format change is needed.
