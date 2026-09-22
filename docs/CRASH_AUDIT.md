# Focused crash audit — 2026-09-21

Scope: sound-cache lifetime, channel sample gathering, sound save restoration,
and renderer backend access to entity/light definitions during level changes.
This is a source audit plus loading regression test, not an exhaustive engine audit.

## Findings fixed

- Renderer draw_common.cpp: the X-ray ambient pass dereferenced entityDef to
  obtain xrayIndex, despite viewEntity's explicit backend lifetime contract.
  Snapshot xrayIndex into frame-owned viewEntity alongside depth-hack settings.
  No remaining entityDef-> or lightDef-> accesses were found in draw*.cpp,
  tr_backend.cpp or tr_render.cpp. Frontend accesses remain intentional.
- Sound GatherChannelSamples: an empty looping sound reached modulo by zero.
  Empty loops now produce silence. Hold the existing recursive decoder/storage
  lock across sample length calculations and decoding to prevent reload races.
- Sound save restoration: channel == SOUND_MAX_CHANNELS bypassed the bounds
  check and indexed past the fixed channel array. Reject it before indexing.
  This concerns invalid save data; normal retail saves are not known to trigger it.
- Sound cache GetObject: index == listCache.Num() was accepted. Return NULL
  at that boundary. Existing listSounds caller uses a valid bounded loop, so
  this is defensive rather than a reproduced normal-play failure.
- EmitterForIndex: negative indexes bypassed validation. Reject them through
  the existing controlled error path rather than indexing before the array.

## Validation and limitations

Release build passed. Muted regression script validation/crash-audit/test.ps1
loads AutoSave__Poor_Bastards, game/lotaa, boxrender, reloadSounds, and game/lotaa
again. Audio processing remains enabled at s_volume_dB -60. Initial run passed;
final-build results are in validation/crash-audit/fixed/base/result.log.
These runs exercise normal loading and decoding, not malformed saves, empty
custom sound assets, or a forced X-ray/entity-free interleaving. The new defects
were established by code inspection; do not describe them as reproduced crashes.

Future audit areas: truncated save/asset readers, renderer temporary-buffer
bounds under deep portal nesting, and map-specific entity teardown. The unused
amplitudeData path also contains unchecked offsets but currently has no producer
in this source tree; it was not changed speculatively in this pass.

Final-build regression passed both markers and exited normally with no new dump. Configurator publish and --verify passed. Settings/custom launcher deployed to crash-audit-build; user choices preserved.
