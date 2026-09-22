# First-person weapon clipping at an energy portal

portalgunclip places the eye about half a unit in front of the aperture. The near-plane repair clamps the portal mask to depth zero. The original weapon depth hack uses [0, 0.5], overlapping the world's [0, 1], so that mask can win the depth test against the weapon. Changing aim moves the resulting seam across the gun.

RB_BeginDrawingView now reserves [0, 0.01] for weaponDepthHack surfaces in views containing both a direct portal and first-person attachments. World geometry, including the aperture, uses [0.01, 1]. Weapon depth, lighting, and ambient passes share the same foreground mapping; ordinary model depth hacks and restoration use the world's range. Shadow depth bounds are remapped to match. Each view recomputes its range, including the retail bloom prepass. Views without the combination retain their original depth ranges.

This preserves the planar portal aperture and its near-plane clamp without allowing the mask to slice the first-person weapon. It does not move the player, alter gun transforms or lighting, or change portal recursion.

Validation: Release engine/configurator builds passed. Muted isolated portalgunclip baseline showed only a narrow weapon slice; the fixed capture shows the complete weapon at the identical saved camera position. Stationary look-up/look-down captures retain the weapon. A 144 FPS threeportals crossing sequence contains ten near-portal frames with the weapon and destination maintained throughout. Nested siblings and portalrenderover's thin side aperture remain rendered, and sequential save loads exited cleanly. Artifacts and scripts: validation/portal-gunclip.

Prey Settings and its custom launcher now use validation/portal-gunclip-build; selections, including crosshair style, are preserved.
