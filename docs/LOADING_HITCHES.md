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
frames and nearby operations. The summary separates gameplay, menu, and transition
frames using start/end frame state and UI transition markers. Older logs without
state are explicitly unclassified. `worst_ms` includes all reported categories;
`worst_gameplay_ms` includes only gameplay. The summary excludes loading and the
first second after map-ready. Nested operation times overlap and must not be added.
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

## Menu stall follow-up

The user's first capture included map-list media precaching and menu music loads.
Map definitions share the entity-definition parser, which was precaching every
browsed map's media while a game was loaded. Map metadata parsing now omits that
automatic media pass; ExecuteMapChange explicitly retains the selected map's
precaching. With asset preloading enabled, the menu-music shader's existing samples
are loaded and marked referenced during map loading, without playing them or
changing sound worlds. On-demand sound shaders retain their original behavior.

Additional scopes record input pumping, event polling/dispatch, individual command
execution, configuration writes, network update, menu setup, and save/map-list
construction. Command arguments and key values are not logged. These wall-clock
durations can also include OS scheduling delays; a long event-poll scope is not
proof of expensive game code.

Gameplay prefetch now ignores assets opened while a session menu is active. The
manifest has a v2 gameplay-only header; legacy lists are ignored and rebuilt once
because they may contain unrelated menu/map music.

A muted 144 FPS, 960x540 repeated-menu comparison reproduced 90.505 ms and 49.637 ms
post-load frames in the previous build. The changed build had no post-settling
frames above 25 ms in that matching run. Menu music archive reads moved before
map-ready; unrelated map-media precaching was absent from menu opening. This
demonstrates those menu fixes, not a fix for every intermittent gameplay hitch.

Five analyzer regression tests cover menu/gameplay classification, entering/exiting
menus, transient menus within one frame, legacy logs, and loading exclusion. A
separate integration test used a deliberately low 5 ms reporting threshold to
exercise real menu, gameplay and transition labels. That run overlapped build/UI
verification activity and is not a performance benchmark. A controlled gameplay
asset was learned, a menu-only asset was excluded, and the learned cache verified
byte-for-byte after reload. Both source builds and native launcher verification pass.
