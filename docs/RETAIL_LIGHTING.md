# Retail normal-map and specular rendering

The shared ARB2 light-interaction path now uses the original Prey shader family
for world geometry, characters, first-person weapons and portal views. This
restores material lighting behavior; it does not replace or upscale textures.

## Recovered behavior

Read-only inspection of the user's original E: installation established:
- `r_normalizeBumpmap=1`, `r_correctSpecular=1`, `r_cubemapNormalize=0`.
- Those settings select `interactionNormBumpH.vfp` in retail shader selection
  at 0x47f954..0x47fa41, rather than the port's unconditional `interaction.vfp`.
- Retail ClearStage at 0x498d80 sets specular exponent 22 and brightness 1.4
  (constants at 0x7be1fc and 0x7be1f8).
- The `specularEXP` parser at 0x49b1de reads two floats separated by a comma.
- Live rifle color/specular uploads are 512x512 RGBA8, and the normal-map upload
  is 1024x1024 RGBA8 in both games. Texture dimensions were not the difference.

The original executable's SHA-256 and preceding audit are recorded in
validation/retail-bloom-capture/FINDINGS.md and validation/rifle-quality/FINDINGS.md.
Addresses apply to that executable, not arbitrary releases.

## Changes

1. Register and select matching vertex/fragment pairs from the existing retail
   assets: interaction, interactionUpdate, interactionCubeMaps,
   interactionNormBump, interactionNormBumpH and interactionAll.
2. Make the existing normal-map and corrected-specular controls select their
   actual shader paths. Normal-map renormalization defaults on. Restore the
   optional cube-normalization selector with the original off default.
3. Preserve `specularEXP exponent, brightness` in the material stage, use the
   recovered default values, carry them through each specular interaction, and
   upload them to fragment program.env[2].
4. Turning corrected specular off selects the old specular calculation; it no
   longer removes all specular lighting. `r_skipSpecular` remains the explicit
   off control. Existing shader-detail, ambient-light and noSpecular exclusions
   remain in effect.
5. Preserve the previously corrected vertex-color packing, decal depth offset,
   portal light weights, gamma/bloom handling and animation interpolation.

No save format or engine/game ABI change is required for this renderer-only
change. The deployment still includes the matching game package with all prior
fixes. The launchers explicitly enable normalized bump maps and corrected
specular so older saved video settings cannot silently disable the restoration.
Texture mip bias remains the user's setting; it is not used to fake extra detail.

## Validation

Release compilation and shader compilation passed. At a frozen portal_bloom
view, all six shader choices were captured with the same mip bias, and additional
captures tested -1 mip bias and bloom separately. The API trace confirms all six
program pairs drawing, and specular parameter uploads of (22,1.4), (300,1),
(60,1.5) and (340,20.2), including authored values previously discarded.

Muted isolated smoke tests cover black_floor, portal_bloom, multi_portals,
blended portal lighting, save reload, menus, third-person and spirit views.
Both-direction portal crossing is checked with the new shaders and blend active.
Evidence and reproducible scripts: validation/retail-lighting.

This restores the audited common light-interaction behavior. It is not a claim
that every renderer feature or every level now matches retail pixel-for-pixel.
The user screenshots use different viewpoints/lighting; the local A/B captures
isolate the shader changes but are not matched retail/port scene captures.
The optional portal light blend retains its documented secondary-shadow limit.
