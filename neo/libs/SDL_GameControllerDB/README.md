# SDL_GameControllerDB

Community controller mappings for SDL, so controllers missing from SDL's built-in
list are recognised by the game and the launcher's controller window.

- Source: https://github.com/mdqinc/SDL_GameControllerDB
- Revision: c1d5289a1f71 (2026-10-02)
- License: zlib, see `LICENSE`

The build copies `gamecontrollerdb.txt` next to `prey06.exe` and `SDL2.dll`; the
game and the launcher load it at startup. To update, replace the file and the
revision above.
