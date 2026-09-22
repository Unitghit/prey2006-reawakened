# Optional bunny hopping

The native launcher exposes Gameplay > Bunny hopping, disabled by default (`g_bunnyHop 0`). Enabled mode is single-player only. In dry, normal airborne movement, lateral speed cannot decrease from friction or air steering; steering can redirect or increase it. Lateral means perpendicular to the current gravity direction, including wall-walk areas. Original vertical damping/gravity and collision response remain in effect. Speed preservation occurs before slope and collision clipping, so it cannot restore speed lost into walls. Ground friction, swimming, spectator movement, and multiplayer retain original behavior. Jumping remains manual.

Runtime hop-chain state is reset on load; the save format is unchanged. The CVar is archived and emitted by the custom launcher; older named presets are unchanged.

A moving repeat jump within 100 ms of landing gains 5% lateral speed, up to twice walking speed. The first jump gets no bonus. Existing speed above the bonus cap is not reduced. Waiting on the ground, water, ladders, disabling the option, or leaving normal single-player movement clears the chain. Jump impulse and gravity remain unchanged; slopes/collisions can still redirect velocity.
