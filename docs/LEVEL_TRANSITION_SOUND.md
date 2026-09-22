# Level-transition sound decoder lifetime fix

The September 21 crash while loading game/lotaa was an access violation in
Vorbis get_bits (prey06.exe+0x1e72a5 in portal-skybox-build), reached through
stb_vorbis_get_frame_float and stb_vorbis_get_samples_float. The preserved
console log showed old sound samples being purged during that transition.
The dump and source are consistent with a decoder retaining a borrowed pointer
to sound data that EndLevelLoad freed.

Sound samples now carry a storage generation. Load, default generation and
purge invalidate existing decoder generations. Decode clears a stale Vorbis
handle before using replacement storage and emits silence for purged storage.
Storage changes and the complete decode operation share CRITICAL_SECTION_ONE,
preventing a purge from freeing data during a decode. Existing recursive locking
allows Load/MakeDefault and decoder cleanup to reuse this lock.

Validation: Release engine build passed. An isolated muted run loaded the
Poor Bastards autosave, loaded game/lotaa, reloaded boxrender, ran reloadSounds,
and loaded game/lotaa again. Both LOTAA_PASS and REPEAT_PASS are present in
validation/lotaa-crash/fixed/base/result.log; the process exited and no new crash
dump appeared. Audio processing was enabled with s_volume_dB -60. This tests
map loading and save reloading, rather than the exact walking route to the exit.
Configurator publish and --verify passed. Current settings/custom launcher now
use validation/sound-lifetime-build. Existing user settings are preserved.
