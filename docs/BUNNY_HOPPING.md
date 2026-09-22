# Optional bunny hopping

The launcher provides Disabled (0, default), Quake style (1), and Painkiller style (uncapped) (2) through archived `g_bunnyHop`. Existing enabled settings remain Quake style. Both modes are single-player only and preserve Prey jump impulse, gravity, water, ladders, and collision handling.

## Quake

Uses the Quake 1 projection-limited air rule: cap wish-direction projection at 30, with acceleration 10 * wish speed * timestep. Strafe and turn to gain speed; opposing input can brake. Manual jumps and original ground friction apply. No artificial hop bonus or total speed cap.

## Painkiller-inspired

Hold a movement direction and jump to chain hops automatically. The first jump is normal; moving subsequent hops within 200 ms of landing add 30% of Prey's walking speed per hop, with no total-speed ceiling or declining gain at higher speeds. Steering approaches the requested direction at a timestep-based rate while preserving lateral speed. Releasing jump and staying grounded for over 200 ms resets the chain; ground friction and obstacles can slow the player. Transient chain state resets on save load. All acceleration and steering lie in the current gravity plane.

Research: PK++ exposes PlayerMove.BunnyHopAcceleration=0.3, 0.2-second before/after landing windows, strong/weak air control, and a MaximalBunnyHopSpeed. See https://github.com/EKBlowfish/PKPlusPlus/blob/master/lscripts/Main/Tweak.lua and lscripts/Main/GameMP.lua. Those scripts delegate actual movement to PHYSICS, so the gain formula and hold-to-hop convenience here are adaptations, not verified copies of Painkiller engine internals. The original maximum-speed setting is intentionally omitted per user request.
