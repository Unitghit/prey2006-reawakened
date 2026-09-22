# Image-program argument truncation

The rifle's reduced surface detail was separate from the retail interaction
shader differences. `ImageFromFile` treated a complete image program as a
filename and called `StripFileExtension` on it. For the rifle this changed:

```
addnormals( models/weapons/rifle/rifle_local.tga, heightmap( models/weapons/rifle/rifle_h.tga, 7))
```

into an expression ending at `rifle_h`, losing the strength and closing
parentheses. The permissive heightmap parser then used its default strength of
1 instead of 7. Full texture dimensions therefore did not guarantee correct
generated texture content.

Image programs now remove embedded `.tga` suffixes without truncating their
arguments, matching the original game's observed canonical rifle program.
Ordinary filenames retain their existing extension handling. This applies to
all image programs, including other weapons and world materials.

Validation (2026-09-21):

- Read-only inspection of both running games confirmed the original retained
  strength 7 while the port had a truncated rifle program.
- Release build succeeded. A separate muted session of the fixed build had a
  single complete rifle normal-map program with strength 7 at 1024 by 1024.
- Matching static captures using the same shader and camera restored the
  scope rim relief; see `validation/weapon-detail/comparison.png`.
- Muted black_floor, portal_bloom and multi_portals loads, repeated save loading,
  menu transitions, and spirit/third-person views completed without engine or
  OpenGL errors. `validation/weapon-detail/verify.py` passed.
- Validation scripts and logs are in `validation/weapon-detail` in the parent
  workspace. Existing user game sessions were left running.

An initial diagnostic `listImages touched` invocation hit the previously known
unloaded-image format error after the captures. The diagnostic was removed;
the complete capture run was repeated successfully. Its initial log is retained
as `initial-diagnostic-console.log`.

This verifies the argument-loss fix, not pixel-identical rendering of every
material relative to the original executable.
