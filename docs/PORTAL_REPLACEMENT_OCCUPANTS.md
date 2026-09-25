# Portal replacement with nearby occupants

Replacement uses aperture bounds only as a broad phase. Touching the rim or resting on the original supporting plane does not block a shot. For a hull extending behind that plane, a closing-world collision query determines whether it needs evacuation. Eligible players and projectiles use the same bounded, swept clearance search as movable props. All destinations are checked before any occupant moves; an actually trapped occupant still prevents unsafe closure.

Validation:
- Retail `barportal`: reproduced refusal from player overlap, then a floor-contact stool; both colors replaced repeatedly after the fix.
- `replacement_occupants`: free prop and player safely evacuated; enclosed prop still rejects closure without moving occupants.
- `objects`, `replacement`, and `blocked`: traversal, reconnecting/replacement, and blocked exits.

Retail saves are local test inputs and are not distributed.
