# Fractional mouse presentation

The launcher forces g_lateMouse off. An initial default-on trial did not improve the reported mouse feel and caused visible reticle displacement during rapid shaking; it was reverted. Smooth motion continues to control the original interpolation bundle only.
Sensitivity is unchanged. GetPresentationLook retains fractional accumulated
angles rather than re-quantizing them to the 16-bit command format. MakeCurrent
normalizes accumulated angles after the pitch-delta clamp, avoiding loss of
float precision in long sessions. Save/network formats remain unchanged.

Stable nonstandard gravity uses the player-space transform; gravity transitions
still obey interpolation eligibility and portal camera correction. Menus,
m_smooth != 1, active gamepad sticks, bound players and spirit/deathwalk retain
the existing fallback. The reticle remains projected from the authoritative
eye trace. Camera preview does not change hit detection.

Muted tests under validation/mouse-look use 24 one-count motions at sensitivity
0.025. The disabled control holds its rendered angle until the command changes;
the enabled view shows changes of about 0.00055 degrees. Tests at 144 and 240 FPS
cover repeated non-consuming reads, inversion, smoothing and menu inhibition.
A visualjolt test covers nonstandard gravity. These are functional traces, not
physical mouse latency measurements. Actual mouse counts and sensitivity still
determine the angular size of each hardware movement.

User playtesting supersedes the narrow diagnostic result: fractional preview is not a confirmed fix for the reported blocky aiming. The opt-in diagnostic path remains available but is not selected by the launcher. Saved sensitivity 5 and m_yaw/m_pitch 0.022 produce 0.11 degrees per raw count before scope scaling. No sensitivity change was made.
