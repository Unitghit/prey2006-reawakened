# Experimental lighter shadows

`g_lighterShadows 1` selects `lights/playerlighter_shadows` for the physical player lighter in single-player. Default is 0. The material preserves the retail activation curves, flicker, scrolling light texture and falloff, but omits `noshadows`. Shadow-casting world geometry and entities within the original light volume use the existing stencil-shadow renderer. Objects/materials authored not to cast shadows still respect those rules.

The material is selected when turning the lighter on, each presentation update, and restoring an active lighter from a save. This permits runtime toggling and prevents saves from overriding the current preference. No save layout or engine/game ABI changes. Existing light ID exclusions keep the owner's body/head and weapon world model from casting shadows into this light.

Portal-projected copies explicitly retain `noShadows = true`: their virtual origins lie behind destination walls and require separate portal-aware shadow handling. Weapon light blending also retains its existing shadowless copies. The experimental feature does not claim to solve portal shadow projection.

Prey Settings exposes the feature under Bloom & lighting, default disabled. Only the configurator's custom launcher moves to validation/lighter-shadows-build; legacy presets retain their previous build. Radius and shadow quality remain unchanged. Cost is scene dependent; no guaranteed frame-rate claim.

Validation: Release engine/game and self-contained configurator builds passed. Muted automated runs loaded black_floor and bar_fight, enabled the lighter, captured shadows off/on and the shadow-volume diagnostic, saved/reloaded with shadows enabled, then disabled the option. Both completed LIGHTER_SHADOWS_PASS and exited. Captures are under validation/lighter-shadows; the black_floor comparison visibly adds contact/occlusion shadows and the diagnostic displays shadow volumes. These are functional smoke tests, not a comprehensive performance benchmark.

User-approved default: Prey Settings now enables lighter shadows by default, including Restore defaults. The engine fallback remains off for older preset launchers. The option can still be disabled in the configurator.
