# Lit floor decals rendering black

The `black_floor` save supplied on 2026-09-21 identifies the black surface as
`textures/decals/doormat` in Feeding Tower B. Its retail diffuse, normal, and
specular assets are present. Reloading the material did not repair the surface;
disabling shadows, bloom, scissoring, and light culling did not repair it either.

The material uses `decal_alphatest_macro`, polygon offset, and an alpha-tested
diffuse stage. The depth prepass applies its material polygon offset, but
`RB_CreateSingleDrawInteractions` did not. The lighting pass uses an equal-depth
test for this perforated surface, so its unoffset fragments failed against the
offset depth and left the black prepass visible. Setting the offset to zero in
an isolated diagnostic made the floor texture reappear, confirming the mismatch.

The shared interaction helper now enables the material's polygon offset before
its lighting draws and disables it afterward. The default offset remains intact;
no retail materials, textures, saves, or lighting values are changed. The local
openPREY renderer has the same matching-offset treatment. This applies to normal
views and portal subviews, and retains the previous empty-scissor guard.

Evidence and muted test scripts are under `validation/portal-black` in the
workspace. The initial diagnostic uses `portal_bloom`; `black-floor` reproduces
the exact user surface, `material` identifies it, and `offset` isolates the cause.
The candidate repeats the surface tests using the compiled correction.

The deployed `validation/decal-lighting-build` was checked at native and
256-square bloom resolutions, after shader reload and menu transitions, at
portal distances 8/64/192/256, and in `multi_portals`. The scripted restores
resume simulation before freezing each test pose so restored portal entities
receive their normal first update. No GL errors were reported. The floor ROI
remains lit after shader reload (within 3 RGB levels); this is not a claim of
pixel-identical whole-frame output. Package CRCs, binary hashes, and matching
symbols are recorded in `validation/portal-black/report.json`. The packaged
game DLL matches the prior save-load-crash fix byte for byte.

AllFixes, AlienText, TranslatedText, and 144FPS launchers now use this build.
