# Merged prop normal generation

The hidercage_base.ASE in shuttleb contains six monitor housings sharing the
cage_monitor material. Its renderbump declaration makes the importer deliberately
discard explicit normals and set generateNormals for FinishSurfaces. Merging
those mesh pieces allocated a new surface but lost this flag, so its zero normals
were preserved and the casing received no lighting. Textures and UVs were intact.

R_MergeSurfaceList now carries the normal-generation requirement from any input
into the result. Repeated merges retain it; meshes whose inputs all have explicit
normals remain unchanged. This applies to ASE, LWO and MA import paths using the
shared helper. Geometry, UVs, materials and save formats do not change.

Use merged_prop_normals.cfg with the private updated colorproblem save looking
at the monitors. Run in an isolated hidden desktop, with s_volume_dB -60. Verify
textured metal casings under room lighting and the lighter, specular highlights,
animated screen overlays, and save/reload. No material overrides are required.

Diagnosis: shadows/culling/bump toggles did not restore the original casing;
an unlit texture diagnostic proved the UVs and image were present. Temporarily
removing renderbump restored explicit normals and lighting. The final code fix
restores the original material without that override. Hidden tests confirmed
textured casings with the normal map and specular map intact, followed by
save/reload and nested portal regression checks. No retail assets are included.
