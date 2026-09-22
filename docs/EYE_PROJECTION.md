# Animated eye texture projection

`R_EyeballDeform` builds connected triangle islands: the visible eye, a helper
triangle defining its origin/orientation, and a target triangle. Island entries
store triangle numbers. The orientation code mistakenly used that number as an
index-buffer offset without multiplying by three. For Tommy's head, the helpers
are triangles 7 and 8, following seven visible-eye triangles. Reading offsets 7
or 8 consequently uses visible-eye vertices rather than the helper triangle.
That incorrect basis moves with the eye mesh and distorts the projected iris.

The orientation lookup now uses `triangleNumber * 3`. Emitted eye vertices also
copy their complete source attributes before replacing the projected UVs, avoiding
uninitialized vertex data. Retail textures, materials, animation and eye-focus
targets are unchanged. This renderer fix applies to materials using `deform
eyeball`, including the opening mirror scene.

Muted intro runs and a saved frozen mirror pose are captured in the parent
workspace's `validation/eye-deform` directory. The baseline uses the preceding
scope-zoom build; the corrected build uses the same retail assets.
