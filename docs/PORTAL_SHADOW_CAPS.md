# Clipped-view shadow caps

strangeshadow reproduces a black band inside the left portal in feedingtowerc.
Muted isolated diagnostics: r_shadows 0 removes the band; disabling depth bounds
alone does not; r_useExternalShadows 0 fixes it with shadows still enabled.
The ordinary shadow optimization removes caps / uses z-pass based on the viewer
and light volume. Those tests do not account for the extra subview clip plane.

RB_T_Shadow now uses full index counts and the internal (z-fail) path whenever
the current view has clip planes. Main views retain their optimized behavior.
No global shadow disable, map-specific exception, or user setting is needed.
Clipped portal/mirror views may draw more shadow triangles, a correctness cost.

Release build passed. Exact-save screenshot verifies the band disappears with
r_shadows, depth bounds, shadow scissoring and external-shadow CVars enabled.
Additional distance and portal regression script: validation/strange-shadow.
Settings/custom launcher now target validation/portal-shadow-build, preserving
user settings and prior fixes. Legacy named launchers are unchanged.

Nearer/farther captures and threeportals/portal_bloom save-load regression passed; images visually reviewed. Configurator publish and layout/serialization verification passed.
