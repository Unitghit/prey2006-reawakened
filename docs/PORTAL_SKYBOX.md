# Skybox room through a portal

The boxrender save shows the rock and metal cube frame through an ordinary portal, but its skybox faces lack the surrounding giant room. SC_PORTAL_SKYBOX was explicitly disabled whenever isSubview was true, and its renderer also imposed a single global skybox draw per frame.

Each parent view can now generate one skybox background before its foreground portals. All skybox faces in that parent share the complete parent scissor, rather than the bounds of the first face. The skybox camera retains the parent's transformed orientation, moves to the authored remote skybox origin, and clears inherited exit clip planes because those planes belong to a different room.

A skybox already in the ancestor chain blocks another skybox descent, preventing recursive backgrounds. Ordinary portal nesting remains bounded by r_portalMaxDepth. The skybox background itself does not consume another portal layer. The existing game skybox entity already supplies its remote camera independently of player PVS, so no game simulation change was needed.

The configurator and custom launcher use validation/portal-skybox-build; existing choices are preserved. This is an always-included rendering correction.

Validation: Release engine/configurator builds and renderer diff checks passed. Muted isolated boxrender captures show the giant room through the portal, including with portal nesting set to one. Crossing onto the rock and aiming up/down preserve the surrounding room. threeportals still shows both inner destinations and portalgunclip retains the complete gun. Sequential save loads exited cleanly. Screenshots and logs are under validation/portal-skybox.
