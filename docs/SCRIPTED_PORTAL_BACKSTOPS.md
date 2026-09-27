# Scripted portal player-clip backstops

The local `portalproblem` save in Superportal stops the player's hull about
1.77 units in front of sppSuperPortal1's plane. The blocking world face is
textures/common/player_clip; the portal remains active and tracks the player,
but the physical position never crosses its plane.

For approaching players at linked campaign portals only, a short forward hull
trace now detects an invisible world player-clip backstop facing the portal.
Within the historical 6.5-unit near-plane margin it permits an earlier ownership
crossing. The actual source/destination transform is unchanged, preserving the
visible arrival position. Solid walls, entities, reverse movement, distant
blockers and gun portals do not qualify. Existing exit validation still runs.

Hidden muted tests of the save pass for walking and jumping approaches, both
with zero landing error and zero collision correction. Existing shallow and
fast floor-portal entry tests pass, and a blocked deep exit remains blocked.
