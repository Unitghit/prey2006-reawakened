# Gameplay loading work

The native launcher exposes Asset preloading (experimental) and Hitch logging.
Both are enabled by default in the launcher for playtesting; engine-only defaults
remain off. Existing preset batch files are unchanged.

## Implemented

- Trace CPU frames of at least 25 ms, game ticks of at least 8 ms, and file reads,
  archive decompression, image loads, entity spawns and media caching of at least
  2 ms. Frame columns separate event/command processing, session work (including
  frame-limit waits and prefetch preparation), and rendering/presentation work.
- Precache spawner-specific media overrides when creating/restoring spawners.
  Normal entity definition precaching already existed. This additional work can
  move loading earlier; it does not predict arbitrary future script decisions.
- Learn up to 512 eligible asset names opened on the main thread during play in a
  per-map manifest under `base/preload/`. On later map/save loads, prepare archive
  reads and decompress them on a low-priority worker with independent zip handles.
- Limit cache reservations to 128 MiB and individual files to 16 MiB. Prepare at
  most two queued files per frame within a soft one-millisecond budget. A single
  filesystem operation cannot be interrupted at the budget boundary.
- Resolve each requested asset through the normal search path before using its
  cached data. Preserve overrides and pure restrictions; loose files stay live.
  Missing, failed, oversized, or not-yet-ready assets use normal loading. Cached
  streams retain shared ownership of their bytes across cache cleanup.
- Stop/join the worker before map or filesystem teardown. Do not activate this
  cache for multiplayer or event-journal playback. No save-format changes.

The worker must never use the engine allocator, shared zip handles, entity state,
OpenGL, cvars or console output. Shutdown joins it before filesystem teardown.
Minizip now uses the CRT allocator consistently so private worker zip handles do
not touch the engine's shared heap. CMake explicitly links the thread library.

## Reading diagnostics

Hitch logging writes `base/diagnostics/hitches.log` inside the selected save profile.
It does not put diagnostic notifications over gameplay. The log is buffered, capped
at 16 MiB, and overwritten when logging starts in a new process. Copy it before
relaunching if investigating an earlier session. No log file is required for play.

Run `python tools/summarize-hitches.py <path-to-hitches.log>` to summarize slow
gameplay frames and nearby operations. The summary excludes loading and the first
second after map-ready. Nested operation times overlap and must not be added.
These are CPU timings, not GPU-query or mouse-to-photon measurements; OS scheduling
and frame-limit/VSync waits can contribute. Only slow frames are logged, so the log
cannot establish average FPS or frame-time percentiles. Logging adds overhead.

Console controls: `com_hitchTrace 0/1`, `com_hitchThreshold 25`, and
`com_assetPreload 0/1` (reload the map/save after changing preloading).
`preloadStatus` reports queue/cache counters. `preloadVerify` is a diagnostic
byte-for-byte comparison with normal archive reads; it intentionally does synchronous
work and should not be used during a performance measurement.

## Validation and limits

Development and private release builds passed. Native launcher configuration,
launch and layout checks passed for all 16 controls and multiple screen sizes/DPIs.
Automated game tests were isolated and muted with `s_volume_dB -60`.

- `multiportal` loaded and ran with preloading disabled and enabled.
- A controlled 24-file set covering DDS textures, MD5 animations and OGG audio
  verified byte-for-byte against uncached reads, including after repeated save loads.
- A first-pass learning test recorded 24 opened assets and prepared all 24 on
  reload, with 24 successful cache verifications and no mismatches.
- A nonexistent candidate fell back safely; traversal, absolute and non-asset
  manifest entries were filtered. Immediate reload cleared pending work safely.
- Sampled gameplay captures rendered normally. The summary parser was checked
  against a synthetic trace containing a loading frame, one asset-related gameplay
  hitch and an unload frame.
- Short enabled/disabled runs did not reproduce the user's intermittent hitch:
  zero gameplay frames above the default 25 ms threshold after the settling period.
  This is not evidence that the original hitch is fixed or that the options improve
  every scene. The first unseeded run opened no new tracked assets during sampling.

This is a conservative first implementation of background loading and budgeted
preparation. JPEG/TGA decoding, GPU uploads, script execution and final entity
creation remain synchronous. Splitting those atomic operations or separating the
renderer requires additional work justified by actual hitch traces. Learned
prefetch primarily helps revisits; first-time dynamically chosen assets may still
stall. Native Linux builds and long play sessions have not been validated here.
