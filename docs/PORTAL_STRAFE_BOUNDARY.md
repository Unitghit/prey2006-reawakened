# Strafe crossing at the exit plane

straferightclip reproduces a blocked remainder trace (fraction zero) that lands
physics at y=2021. Camera basis rounding puts the eye at y=2021.000366, behind
the destination portal whose normal points toward negative Y. For one tick the
aperture is back-facing and the destination doorway backing geometry is exposed.

After a local snapped portal crossing, camera distances within +/-0.01 units
of the exit plane are resolved to +0.01 on its front side. Larger offsets are
unchanged. This affects only the cached render view; collision position,
momentum, and teleport alignment are unchanged. The existing snap view-model
mapping follows the corrected camera. Continuous interpolated crossings retain
their current path.

Muted validation under validation/portal-strafe captures the baseline and fixed
right-strafe crossing at 144 FPS, plus a 240 FPS check. The diagnostic strafe
input is non-archived, gated by com_fpsTrace and single-player mode, and reset
on map initialization.
