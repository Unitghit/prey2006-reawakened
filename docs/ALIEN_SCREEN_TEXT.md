# Preserve authored alien text

The upstream Prey2006 renderer automatically substitutes a readable font when
the player approaches a world GUI. `gui_translateAlienFont` defaults to `fonts`
and `gui_translateAlienFontDistance` defaults to 200. The distance check is in
`neo/renderer/tr_guisurf.cpp`; the CVars are in `neo/renderer/DeviceContext.cpp`.
These settings were already present before our local changes.

The string `Thanks for playing Prey` is present as `#str_10023` in the original
retail PC localization assets. Revealing readable text is a font substitution;
the message itself was not added by our framerate or bloom work.

The existing four workspace PC launchers pass
`+set gui_translateAlienFontDistance 0`. The `fps-play-profile`,
`bloom-play-profile` and `play-profile` autoexec files also set that value, so
previously archived distance settings cannot re-enable it on startup. This
disables proximity-based font substitution without changing screen content,
screen interaction or the separate scripted console/Talon translation behavior.

No binaries, packages or currently running game were modified. For an already
running session, enter `gui_translateAlienFontDistance 0` in the game console,
or restart using a workspace launcher. Setting the distance back to 200 restores
the upstream automatic behavior for the current session.

Two explicit variants share the current AllFixes build, bloom settings, 144 FPS
cap and `fps-play-profile` saves/settings:

- `Play-Prey2006-AlienText.bat`: translation distance 0.
- `Play-Prey2006-TranslatedText.bat`: translation distance 200, readable `fonts`.

Command-line settings are reapplied after autoexec execution in Common.cpp,
so the translated launcher overrides the profile's alien-text default. Each
launcher selects its mode explicitly, regardless of the previous session.
