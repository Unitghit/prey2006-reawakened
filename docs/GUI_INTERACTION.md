# In-world GUI hit detection

The renderer's interactive-mask overload was a stub that discarded its mask.
The underlying trace also left `guiPoint_t::frac` uninitialized and used the
shared MD5 model's snapshot for animated entities. A cabinet therefore could
be traced against an absent or unrelated pose instead of its visible screen.

The trace now:

- Instantiates/uses the entity-specific dynamic model through the renderer's
  normal model-resolution path.
- Filters GUI surfaces by the supplied interactive-slot mask.
- Initializes a miss to fraction 1, records the actual hit fraction, and chooses
  the nearest eligible surface rather than the first surface in model order.
- Rejects degenerate texture axes before computing cursor coordinates.

The interaction reach remains 70 units; this does not expand interaction through
walls or alter GUI scripts. Accurate occlusion checks now receive a defined hit
fraction. No game/renderer ABI or save-format change is required.

Reproduction: the user's `gui_create` save faces `object_itemcabinet_6` in
feedingtowerb. With identical diagnostic game DLLs, the previous renderer reports
no GUI hit; the corrected renderer reports GUI 1 around UV (0.475, 0.502), at
fraction 0.435. A normal attack-input click dispatches `opencabinet` and opens
the cabinet. Screenshots and muted test logs are in the parent workspace's
`validation/gui-interaction` directory.

Opt-in diagnostics: `com_fpsTrace 1` plus `g_guiTrace 1` logs candidates/click
commands. `g_presentationTestAttack` supplies attack input only in single-player
with `com_fpsTrace` enabled and resets at shutdown. Normal play leaves these off.
