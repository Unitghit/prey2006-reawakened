# Optional bunny hopping

The native launcher exposes Gameplay > Bunny hopping, disabled by default (`g_bunnyHop 0`). Enabled mode is single-player only. In dry, normal airborne movement, lateral speed cannot decrease from friction or air steering; steering can redirect or increase it. Lateral means perpendicular to the current gravity direction, including wall-walk areas. Original vertical damping/gravity and collision response remain in effect. Speed preservation occurs before slope and collision clipping, so it cannot restore speed lost into walls. Ground friction, swimming, spectator movement, and multiplayer retain original behavior. Jumping remains manual.

No physics state or save format fields are added. The CVar is archived and emitted by the custom launcher; older named presets are unchanged.
