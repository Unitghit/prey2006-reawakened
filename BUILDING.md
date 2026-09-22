# Windows private test build

Install Visual Studio 2022 Build Tools with Desktop development with C++, CMake, Git, and Python 3.

From the repository root:

```powershell
./tools/build-private.ps1
```

The output is `output/private-preview/` with `Prey2006 Reawakened Launcher.exe` at its root and engine binaries under `engine/`. The launcher creates settings and `userdata/` relative to that root. The original development installation is not used or modified.

This is a private build pipeline, not a public release packager. Required retail data and the excluded upstream artwork must be reviewed and supplied locally before gameplay validation. Do not copy the current development pak007 wholesale into a public package.

Run `python tools/audit-source.py` after staging source changes and before pushing. The audit checks tracked file paths and common credential formats; manual license and provenance review is still required.

The main build uses the upstream CMake dependency sources. Linux build scripts and source remain available, but native Linux and Proton are not yet validated release targets.
