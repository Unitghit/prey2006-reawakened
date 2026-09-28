# Targetless delayed events after save loading

The September 27 crash dump (prey06.exe.28108.dmp) shows a null receiver in
idClass::ProcessEventArgPtr, called by idEvent::ServiceEvents. The event is
<immediateremove>. The current onward29 save reproduces a null target for this
same event during restore on keeperfortress.

WriteObject encodes an object missing from the saved object list as index zero.
Previously Restore kept the delayed event even when ReadObject returned null,
and ServiceEvents relied on an assertion before calling the receiver. Release
builds therefore crashed when the event became due, potentially long after load.

Restore now consumes all event arguments before freeing targetless events, so
old saves remain aligned and usable. ServiceEvents also discards a null-target
event before decoding arguments or dispatching. A warning identifies the event.
Valid targets and null entity *arguments* are unaffected. This does not attempt
to recreate an object absent from the save or restore an action for that object.

Validation: Release DLL build; hidden muted onward29 load with AI enabled,
save/reload roundtrip, and accelerated delayed-event run (com_fixedTic 10,
1000 rendered waits) with AI disabled. All exited cleanly; the original save
logged the expected removal-event discard, and the new roundtrip did not add a
new discard on reload. The original gameplay sequence was not replayed exactly.

## Root cause: broken team chains (follow-up audit)

The orphaned target in onward29 is an hhEntityFx (the save records the event's
class next to the null object index). Entities are only reached by the save
loop through their team master's chain, and idEntity::Unbind, when the
unbinding entity is its team's master, rebuilt the leftover members into a new
team with a loop that stopped before the last member. That member kept
pointing at a master whose chain no longer contained it, so it and its queued
events were omitted from the save. This state arises when an entity is still
bound to something that has left the team (for example, effects bound to a
removed or gibbed monster). A developer test building M <- A <- X, Y/Z <- M,
then M quitting the team and A unbinding, reproduced Z becoming unreachable;
the fixed loop gives Z a reachable master.

Fixes: the Unbind loop now includes the last member; QuitTeam no longer
dereferences a missing teamChain for a single-entity team; saving adds any
entity not reached through team chains (SAVE_TEAM_ORPHAN warning) and names
any queued event whose target is still not saved (SAVE_EVENT_ORPHAN).
Delayed-event handlers that dereferenced a possibly vanished entity argument
(DamageEntity, ControlVehicle and EndLevel activation, radio chatter) now
guard it; EndLevel still performs the level transition.

Validation: all 25 single-player maps, scripted walkthroughs (every trigger,
every monster/spawner position) with 187 saves, each map's last save reloaded
in a fresh session: no team or event orphans, no restore discards, no crashes.
onward29 with six combat saves: only its two pre-existing damaged events are
discarded on load; no new orphans.
