# Prey2006 Reawakened

**Unlocked FPS, corrected bloom, increased portal render distance, and dynamic flashlight shadows.**

An enhanced version of Prey (2006), built on FriskTheFallenHuman's [Prey2006 source port](https://github.com/FriskTheFallenHuman/Prey2006), with additional fixes adapted from themuffinator's [openPREY](https://github.com/themuffinator/openPREY). See [CREDITS.md](CREDITS.md) for the full lineage.

## Status

Version 1.0.0, release candidate. The full campaign has been played through on Windows. No retail game files, saves, keys, or game downloads are provided: Setup imports Prey's data from the player's own copy, and the optional Doom 3 and Portal content from theirs. See the [changelog](CHANGELOG.md) and [known issues](docs/release/KNOWN-ISSUES.txt).

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
- A lightweight native settings launcher with guided Setup.
- A portal gun, optional Doom 3 weapons, and Half-Life style movement.
- Fixes for loading crashes, in-world GUI interactions, eye textures, and numerous portal rendering problems.

## Building and installation

See [BUILDING.md](BUILDING.md). `tools/package-release.ps1` builds the release archives (portable zip, source zip and checksums) from a clean build of the current commit; see the [release checklist](docs/RELEASE_CHECKLIST.md). The launcher's Setup imports the required retail data from the player's PC copy of Prey on first launch.

Windows x64 is the current playtested target. Native Linux and Proton gameplay are not yet release-tested. The launcher has previously passed isolated Wine checks.

## Credits and licensing

See [CREDITS.md](CREDITS.md) and [licensing review](docs/LICENSING.md). Existing source notices and bundled dependency licenses are retained. A copy of the upstream GPL text is in [COPYING.txt](COPYING.txt); it is not a blanket claim that every included component or retail asset has the same license.
