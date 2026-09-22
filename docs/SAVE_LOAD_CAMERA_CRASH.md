# Save restore before the first camera update

The user's 2026-09-21 11:51 crash is preserved in
`validation/save-load-crash/user.dmp` and `user-console.txt` in the workspace.
Windows recorded access violation `0xc0000005` at game DLL RVA `0x2f164d`.
The matching deployed map identifies `hhGameLocal::Draw + 0x13d`.
The exception context has RAX=0 at `movups xmm0, [rax]`, immediately after
`player->GetRenderView()`: the FPS presentation snapshot dereferenced a null
camera. The dump's exception context, rather than the WER thread's later waiting
context, is required to see the fault.

Restored players may have no render view until their next simulation tick.
`Draw` now returns false before taking a presentation snapshot when that view
is absent. The session can render its existing fallback and menus until the
normal tick creates the camera. Rendering does not force a simulation tick or
manufacture camera state. The existing `RunFrame` history capture already checks
for a missing view.

## Reproduction and validation

`validation/save-load-crash/run.ps1` uses isolated profiles and `s_volume_dB -60`.
The old portal-clipping build crashes on the first transition from `portal_bloom`
to `Autosave_game_feedingtowerb`, at the identical DLL RVA and exception code.
Its additional dump is `reproduced.dmp`; the checkpoint log identifies that save.

The candidate exercises eight consecutive restores at each of two configured
FPS caps (144 and 360), using the beginning-of-level autosave, `portal_bloom`,
the Wall Walking autosave and `multi_portals`. Every restore requests an immediate
screenshot before waiting for normal ticks, then captures the initialized view.
Each sequence finishes with a menu round trip and an explicit completion marker.
These are render-timing regression tests, not performance measurements.

The deployment is `validation/save-load-build`, containing the updated packaged
game DLL, matching symbols, and the existing portal/bloom renderer. AllFixes,
AlienText, TranslatedText and 144FPS launchers use it with the existing player
profile. Original saves and the original retail installation are unchanged.
`validation/save-load-crash/report.json` records test results and package hashes.
