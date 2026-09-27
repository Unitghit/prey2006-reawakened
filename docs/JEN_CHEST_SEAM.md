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
