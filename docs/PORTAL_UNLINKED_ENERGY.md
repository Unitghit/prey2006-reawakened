# Unlinked portal energy

Portal-gun endpoints with no partner now use an opaque, colored interior with
slowly moving retail noise and energy artwork. The blue/orange rim and existing
opening timing remain unchanged. Pairing swaps to the live aperture and fades
the matching energy overlay out over 250 ms using shader time, not simulation
steps. Replacing a linked endpoint also uses this short reveal.

The imported aperture was a recessed funnel with strip UVs. Flattening it does
not produce a usable filled disk. The importer creates a convex perimeter fan
for the energy layer, with planar UVs and the same weighted opening pose.
Its winding matches the retail surfaces in both ASE and MD5 formats. The live
portal aperture is retained independently. The filled material uses an explicit
GL_ONE/GL_ZERO base stage (Prey's blend-none syntax means no color draw).

No renderer-wide exceptions, collision changes or new launcher option are
needed. This is part of the existing optional portal gun. Material definitions
and generated geometry are rebuilt by tools/portalgun/build_assets.py from the
user's retail installation; generated retail-derived files are not distributed
in the source repository.

Link state and fade timing use existing spawnArgs and shader parameters, so the
save format does not change. Removing an endpoint clears the surviving gun
portal's raw camera target and renderer view pointer before they can dangle.
The survivor switches back to its filled model. Pair changes present the new
model immediately to avoid a missing render definition for one simulation tick.

Reference investigation: Portal-Base's portalrenderable_flatbasic.cpp uses
portalstaticoverlay_1/2 when no linked portal exists, skips the remote render in
that state, and layers its static overlay over the linked view. This implementation
uses the same general layering approach with Prey materials; no Valve renderer
code or art was copied for this effect.

Validation uses hidden, muted isolated profiles. The energy fixture exercises
both colors, single-endpoint save/reload, staged link-fade captures, replacement
and partner removal. Retail multiportal4 comparison captures check wall masking.
The body split, deep views, decal mask, replacement and through-portal shot
fixtures also passed, as did development/private builds and launcher verification.
