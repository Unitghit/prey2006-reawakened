# Licensing review before public distribution

Preserved evidence: upstream GPLv3 text in COPYING.txt, original source headers, dependency licenses, and docs/upstream/README.md describing the Prey SDK / Doom 3 GPL integration.

Before public source or binary release:

- Confirm redistribution terms of the Prey SDK-derived game code and GUI scripts, rather than assuming the engine GPL covers them.
- Identify the exact openPREY adaptations and retain any required notices.
- Inventory dependency notices and include those needed for the distributed binaries. The release package carries them in `licenses/` with `THIRD-PARTY-NOTICES.txt` (source: `docs/release/`).
- Review icon and artwork provenance. For 1.0.0, seven upstream Prey2006 base files that the menus and console reference were added to `base/` (the Human Head and 3D Realms logo videos, console backgrounds, loading screen and a black texture; identical to `../Prey2006/base`). Upstream tutorial and test maps and material-editor models remain omitted.
- Files derived from retail Prey (the portal gun openings and the Jen seam repair) are not packaged; Setup builds them from the player's own archives.
- Provide corresponding source and build instructions with any distributed binary where required.

Retail pak000 through pak006 archives, music, textures, maps, activation keys, and personal saves are not part of the repository or downloadable package. Users must supply their own game data. No public release has been authorized yet.
