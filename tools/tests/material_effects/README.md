# Material effect fixtures

These meshes, materials, skins, and room are authored test assets, not extracted
retail content. The shader programs themselves come from the user's retail data.

Copy this directory's contents into an isolated profile's base directory. Compile
`dmap maps/audit_materials.map`, then run `exec material_effects.cfg`. Run on a
private desktop with `+set s_volume_dB -60`; do not use the player's profile.
Use a 1280x720 window with MSAA disabled for the measured reference results.

Verified in the Release build:

- Jitter bit 128 stretches the quad's local Y axis: width 154 -> 168 pixels;
  height stays 154. Bit 4096 doubles the selected translation.
- Corona expression 0.25 -> 0.5 changes a 48x48 square into 96x96.
- Moving 100 units farther away leaves that corona 96x96.
- Skin, cloth, hair, masked interaction, parallax, variable specular exponent,
  liquid interaction, and atmosphere programs all compile and draw without ARB
  errors. They use different colors/reflections with the same authored quad.
- Switching shader quality from 3 to 0 selects the white conventional fallback;
  restoring 3 restores the colored atmosphere program.

Additional private-save tests cover colorproblem with changing camera angles,
save/reload, multiportal2 with three projected lighter lights, and sunbright.
The screenshots and personal saves are intentionally outside the source tree.
This validates the renderer paths, not a pixel-exact retail comparison for every
campaign material. Ambient lights and special diagnostic overrides deliberately
retain conventional light stages.
