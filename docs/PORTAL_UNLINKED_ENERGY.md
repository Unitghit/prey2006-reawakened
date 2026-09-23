# Unlinked portal center

An unpaired portal gun endpoint replaces only its interior window with an
opaque blue/orange material containing subtle moving retail noise and energy.
The rim artwork, rim geometry, opening skeleton and opening animation are the
original portal-gun versions. Connecting a pair uses the original linked models
and model-switching behavior, without an energy overlay or extra fade.

The first energy implementation added bright backside ring layers, a linked
transition mesh and early model presentation. Those additions were removed
after playtesting showed a thick rim and broken-looking opening transitions.
The original linked ASE/MD5 meshes and shared animation now regenerate byte
for byte against the pre-energy assets. Both unlinked animated models retain
the original skeleton and first two rim meshes exactly; only the interior mesh
and material differ.

The interior uses a convex perimeter fan with planar UVs and the existing
opening joints. Its winding matches the original surfaces in both ASE and MD5
formats. The filled material uses GL_ONE/GL_ZERO for its base stage; Prey's
blend-none syntax means no color draw. There are no added luminous ring stages.

No renderer-wide exceptions, collision changes or new launcher option are
needed. This is part of the existing optional portal gun. The importer rebuilds
the derived models from the user's retail installation. No generated retail
assets are included in the source repository.

Link state uses an existing spawnArg without changing the save format.
Removing an endpoint clears the surviving gun portal's raw camera target and
renderer view pointer before they can dangle; the survivor resumes its filled
model. Shader fade parameters are no longer needed.

Validation: exact-file comparisons against the pre-energy linked assets and
animation; unchanged unlinked rim/skeleton comparisons; hidden, muted captures
of opening, both colors, pairing, single-endpoint save/reload and replacement;
existing replacement and split-player regression fixtures. Development/private
builds and native launcher verification also pass.
