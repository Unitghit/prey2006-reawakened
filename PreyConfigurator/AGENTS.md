# Active launcher

The production launcher is native C++/Win32 in `native/`.

- Options, labels and defaults: `native/options.inc`.
- Option bundles, JSON migration and launching: `native/settings.cpp`.
- Current engine build directory: `EngineDirectory` in `native/settings.h`.
  It drives both direct launch and the custom batch file.
- Window, layout, scrolling and buttons: `native/main.cpp`.
- Icon: `prey.ico`, embedded by `native/app.rc`.

Build and verify with `./tools/build-private.ps1` from the repository root. Native runtime code
must not require .NET, a webview, or additional GUI DLLs. Preserve existing JSON
settings, fixed footer, measured startup size, page scrolling over dropdowns,
keyboard navigation, and direct no-console Save & Play. Avoid new preset changes.

The development-only test child is never part of the distributed package.
`--verify` uses it to test Save & Play without launching the real game or audio.
