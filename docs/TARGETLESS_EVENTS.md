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
