# Jen chest shading seam

The retail body material requests unsmoothed tangents. At the mirrored UV join
on the exposed chest, independently selected dominant triangles create a visible
lighting discontinuity. An unlit clean-diffuse comparison does not show the broad
split; smoothing the entire material also changes clothing and jewelry highlights.

The MD5 loader identifies the retail body by material and source vertex count,
then selects exposed centerline vertices in the bind pose. After tangent derivation,
the renderer averages incident geometric face normals at those vertices and
reprojects their tangents, preserving mirrored bitangent handedness. Geometry,
UVs, animation weights, textures and all other vertices remain unchanged.

This is a scoped retail-asset correction, not global smoothing or vertex welding.
Replacement meshes with the same material and vertex count should be checked if
their bind-pose shape differs. No save format or gameplay changes are involved.

Validation: Release engine build; hidden, muted seam-save captures with normal
materials, fallback lighting and bump/specular isolation; lighter illumination;
save/reload with a different character pose. The broad centerline split is reduced
without globally smoothing folds. This does not claim to remove every authored
normal-map seam on this or other models.

## Follow-up: mirrored tangent direction and local normal-map repair

Averaging vertex normals alone left a fine line. The renderer now builds a
consistent along-seam tangent direction from neighboring centerline vertices and
UVs, keeping the across-seam handedness distinct. This correction remains limited
to the same retail chest vertices.

The matching local asset patch removes the across-seam normal-map component in a
2-pixel strip with a 12-pixel smooth falloff. About 0.5% of the 1024-square map is
inside that footprint; texels outside it are unchanged relative to the generated
combined normal map. Along-seam detail remains. The generator reproduces the
heightmap/addnormals combination (floating-point normalization may differ slightly
from the engine's fast approximation). No diffuse, specular, mask, or mesh assets
are repainted.

Generate locally with Python, Pillow and NumPy:

```powershell
python tools/materials/build_jen_seam.py --base "<retail>/base" --output "<runtime>/base/zz_reawakened_jen_seam.pk4"
```

The generated archive contains derived retail data, so it is excluded from source
and must not be redistributed. It overrides `materials/characters.mtr`, preserving
its other declarations from the supplied retail archives. Regenerate/merge it if
another mod changes that file. It requires this renderer correction; neither
half of the fix alone produces the same result. Removing the generated archive
reverts the texture portion without modifying original archives.

Final-package validation used the real PK4 with loose prototypes removed: the
seam save under normal scene lighting and lighter illumination, followed by
save/reload into a different pose. The long chest line is no longer apparent in
the frontal comparison; this is not a guarantee against every possible seam at
all angles or on replacement assets. Tests remained hidden and muted.
