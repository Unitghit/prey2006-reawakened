# Prey2006 Reawakened

**Unlocked FPS, corrected bloom, increased portal render distance, and dynamic flashlight shadows.**

An enhanced version of Prey (2006), built on the [Prey2006 source port](https://github.com/FriskTheFallenHuman/Prey2006), with additional fixes adapted from [openPREY](https://github.com/themuffinator/openPREY).

## Status

Private release preparation. Full campaign testing is in progress, approximately one third of the way through the first playthrough. This is not a finished or fully verified release. No retail game files, saves, keys, or game downloads are provided.

## Enhancements

- Unlocked FPS with smoother camera, weapons, objects, and portal transitions.
- Corrected retail lighting, weapon rendering, and resolution-scaled bloom.
- Increased portal render distance, nested portal rendering, and skyboxes and bloom through portals.
- Lighting for weapons and flashlight across portals.
- Optional dynamic flashlight shadows.
- Xbox controller support alongside keyboard and mouse.
- Windowed fullscreen, MSAA selection, and a display-aware cap with 3 FPS of headroom.
- Optional English translation of alien screen text.
- Cherokee difficulty unlocked, with an optional difficulty override.
- A lightweight native settings launcher.
- Fixes for loading crashes, in-world GUI interactions, eye textures, and numerous portal rendering problems.

## Building and installation

See [BUILDING.md](BUILDING.md). The source snapshot intentionally excludes upstream sample maps and game artwork. Local installation must obtain required retail assets from a user's PC copy of Prey. The guided Setup.exe is tracked in the [release checklist](docs/RELEASE_CHECKLIST.md); it is not available yet.

Windows x64 is the current playtested target. Native Linux and Proton gameplay are not yet release-tested. The launcher has previously passed isolated Wine checks.

## Credits and licensing

See [CREDITS.md](CREDITS.md) and [licensing review](docs/LICENSING.md). Existing source notices and bundled dependency licenses are retained. A copy of the upstream GPL text is in [COPYING.txt](COPYING.txt); it is not a blanket claim that every included component or retail asset has the same license.
