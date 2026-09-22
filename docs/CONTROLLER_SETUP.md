# Xbox controller alongside keyboard/mouse

The existing SDL backend and user-command generator support gamepads, keyboard,
and mouse together. `in_useGamepad` was enabled in the player profile, but the
profile had no controller bindings. `base/controller_xbox.cfg` in the
`fps-play-profile` directory now adds those bindings via `autoexec.cfg`, without
unbinding or rewriting the keyboard/mouse configuration.

Restart normally, or run `exec controller_xbox.cfg` from the game console.
Edit that file to customize controller bindings; it runs at startup.

Left stick: move. Right stick: look. RT: fire. LT: alternate fire/weapon zoom.
A: jump. B/right-stick click: crouch. X: lighter. Y: spirit walk.
LB/RB: previous/next weapon (zoom steps while the weapon is zoomed).
Left-stick click: grenade. D-pad: weapons 1–4. Start: menu (handled by SDL).
In-game panels use the existing attack interaction; menu input is handled by
the existing backend. This is a custom PC layout, not a claimed copy of the
Xbox 360 mapping. Aim assist and rumble parity have not been added or verified.

The key/action names were checked against the engine's input tables, and the
original keyboard/mouse profile was verified unchanged. Physical controller
movement, menus, hotplug and simultaneous mouse use still need a hands-on check.
Backups are in `validation/controller-setup` in the workspace.
