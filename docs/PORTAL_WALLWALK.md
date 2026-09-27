# Portal gun openings on gravity catwalks

Wallwalk contact disappears inside an open portal by design. Previously both
`CheckWallWalk` and `AirMove` treated this as leaving the catwalk and reset gravity
before the eye crossed the portal. That also changed the portal crossing plane
while the player's feet were already behind it.

Retain existing wallwalk gravity only while the player's hull fits a linked gun
portal's collision aperture, the hull and gravity face its supporting surface,
and the eye has not crossed. Forced destination checks still acquire the exit's
gravity. No wallwalk state is created for ordinary players, and no collision
aperture is enlarged.

The camera clearance trace now identifies the player as its owner. Otherwise a
sideways camera traced against the intact supporting wall even though the body
was passing through its portal, collapsing the eye offset and triggering an
early crossing. Other scenery remains part of camera collision.

Validation: hidden, muted `portalwalk` replay with a 220-unit tangent approach;
crossing at approximately 68 units of eye offset, zero teleport correction;
144 FPS cap; save/reload during partial entry; an obstacle at the exit rejects
entry. Existing shallow floor clearance, fast crossing and blocked deep exit
tests pass. No save format changes. `portalGun status` reports gravity, hull up
and eye position in developer mode for future arbitrary-gravity diagnostics.
