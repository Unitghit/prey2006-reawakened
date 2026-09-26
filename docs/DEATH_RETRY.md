# Automatic retry after final death

Single-player deaths that previously opened the restart menu now retry the latest successfully saved or loaded checkpoint in the current session. An explicit load promotes that save even when another file has a newer disk timestamp. Manual saves, quicksaves, and automatic saves all use the same registration path. The asynchronous writer registers its checkpoint only after a successful write and atomic replacement; death waits for pending writes before choosing.

Missing or invalid-header candidates are skipped in reverse interaction order. With no usable candidate, the current map restarts with the persistent inventory captured when that map was entered or its save loaded. Starting a new game clears the previous retry history. The history is session-local and does not change the save format or select unrelated files merely because they exist on disk.

The change handles the existing `died` session event after the game frame returns. Spirit deathwalk/resurrection logic and multiplayer spawning are unchanged.

Hidden, muted engine tests cover a real death after loading an older save, a subsequent manual save, asynchronous saving, a fresh map with autosaves disabled, and deletion of the latest checkpoint. The first two must log `DEATH_RETRY save=retry_a` and `save=retry_c`; the fresh map must log `DEATH_RETRY restart=rw_portal_lab`. Deleted latest saves must fall back to the previous loaded checkpoint. Fixtures live in tools/tests/death-retry and require the isolated rw_portal_lab input save. Use s_volume_dB -60 and the private desktop runner.

The launcher Gameplay option **No spirit resurrections** sets `g_noSpiritResurrections` (default 0). Enabled, normal single-player lethal damage uses final death even after spirit power is acquired; spirit walking and multiplayer are unaffected. Changing it applies on launch and does not remove the saved spirit ability.
