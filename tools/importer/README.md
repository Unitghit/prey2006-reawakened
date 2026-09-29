# Content importer

`reawakened-import.exe` converts the optional content from the player's own games.
The launcher's Setup / Import games window runs it hidden, from `engine\importer\`,
and installs its output into `engine\base`:

- `doom3`: the original Doom 3 (with or without Resurrection of Evil) or the BFG
  Edition, using `tools/doom3` (BFG files are converted by `bfg_source.py`).
- `portal`: Portal from Steam (VPK archives) or an unpacked copy, using
  `tools/portalgun` and the bundled Crowbar command-line decompiler.

Nothing is distributed from those games. The source games are only read, and
output goes to a fresh folder that the launcher installs from. It records the
installed files, and keeps any shipped file the import replaces (for example
the portal shots' retail-texture fallback material) so that removing the content
restores it.

Build, then package:

```powershell
python -m pip install pyinstaller numpy pillow
./tools/importer/build-importer.ps1 -Crowbar <path to Crowbar.exe>
./tools/package-local.ps1 -Engine <engine build>
```

Crowbar: <https://github.com/UltraTechX/Crowbar-Command-Line> (tested at revision
`0c5950af196fe1edcce55b2b54d3c159490e5db0`, .NET Framework 4.8). Its license is
copied next to it as `crowbar\License.txt`.

Protocol: the importer prints `PROGRESS <0-100> <text>`, then `DONE <files>` or
`ERROR <message>` on stdout; diagnostics go to stderr, which the launcher saves
as `engine\import-work\<game>.log`.
