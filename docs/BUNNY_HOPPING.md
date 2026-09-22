# Optional bunny hopping

Gameplay > Bunny hopping enables Quake 1-style air acceleration in single-player (`g_bunnyHop 1`). Disabled is the default and uses original Prey air movement.

The rule caps the velocity projection in the wish direction at 30 units/s, while acceleration is 10 * uncapped wish speed * timestep. It does not cap total speed. Strafe and turn to gain speed; opposing input can brake. There is no fixed jump bonus, speed floor, 100 ms grace period, or 2x speed cap.

Prey input scaling, ground friction, manual jump/release requirements, jump impulse, gravity, water, ladders, and collisions remain unchanged. The wish direction is projected onto the current gravity plane, so air acceleration works with rotated gravity. This is an adaptation of the Quake air rule, not a replacement of all Prey physics with Quake movement. Existing saves are compatible; no extra physics state is stored.

Reference: https://github.com/id-Software/Quake/blob/master/WinQuake/sv_user.c
