# Save difficulty modes

The launcher defaults to Respect saved difficulty (g_forceCherokee 0).
Existing saved launcher choices remain intact. The legacy CVar name is retained
for compatibility: 1 forces Cherokee, and 2 forces Normal. It is now an archived
integer constrained to 0..2, rather than a boolean.

Prey records g_wicked in SaveGame and restores it in InitFromSaveGame.
g_skill is unused for this game's difficulty, and g_nightmare only controls
whether Cherokee is unlocked. The override is applied after reading the saved
g_wicked value. It applies to single-player save loading, not new-game selection.
Existing saves are not rewritten; future saves record the effective mode.

Muted isolated validation created both Normal and Cherokee fixtures, set the
opposite live difficulty, and loaded each with override 0. Both restored their
saved values. Loading Normal with override 1 produced Cherokee; loading Cherokee
with override 2 produced Normal. All four checks passed. Evidence remains in the
local development workspace under validation/difficulty-modes/base/result.log;
no personal save files or captures are distributed.

This validates current behavior and does not establish the cause of a reported
difficulty reversion in an earlier build.
