# Portal collision-recovery camera repair

farportal reproduces a crossing where exit collision recovery moves the player
by 5.47 units. That deliberately invalidates interpolation history. The portal
can run after the player has already built its view, leaving the camera in the
entrance room for the rest of the tick despite destination physics coordinates.

TeleportNoKillBox now recalculates first-person and render views for snapped
teleports. A local portal snap records the old view for that tick so Draw can
rebase view models and their lights, using render-only copies restored afterward.
The crossing body is suppressed on that frame as for interpolated crossings.
Collision recovery, movement, and normal continuous portal interpolation stay
unchanged.

Muted validation: validation/far-portal. Baseline captures show the wrong room
at the crossing. Fixed forward and reverse captures retain the destination view
and weapon. Reverse traversal starts from a repositioned test camera near the
exit. Collision adjustments are retained in the trace.
Regression: portal_bloom continuous crossing retains PORTAL_PRESENTATION with zero landing error. Engine build and configurator verification passed.
