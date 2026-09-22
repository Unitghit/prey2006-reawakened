# Xbox 360 crosshair selection

The Xbox disc FrontEnd.BPak0 contains d:\base\preyconfig.cfg with `seta g_crosshair "3"`. The PC game defaults to 1. Both releases include the eight basic crosshair designs; option 3 is the three-segment circular reticle with a center dot.

Prey Settings now exposes PC (1), Xbox 360 (3), and Off (0) using the existing g_crosshair CVar. This reuses the PC cursor GUI and artwork, including its active targeting variant. Scope overlays and aiming behavior are unchanged. No Xbox binary asset is added to the game package, and no engine rebuild or Xbox installation is required. PC remains selected by default.

Validation: decoded Xbox texture assets and the disc default were compared with PC assets; source evidence is in validation/crosshair. A muted isolated game test switched between all three choices and captured each in-game. Configurator publish and settings serialization/layout validation passed, including the new row at all existing size/font checks. The Xbox window capture API failed, so the reference was established from disc assets rather than a live screenshot.

Update: the duplicate configurator choice was removed at the user's request. Crosshairs are now selected exclusively in the in-game menu; the custom launcher no longer forces g_crosshair.
