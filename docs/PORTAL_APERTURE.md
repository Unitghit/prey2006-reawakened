# Energy portal aperture at grazing angles

The portalrenderover save demonstrates destination pixels protruding past the thin orange rim. The retail portal.md5mesh innerwarp surface is a bowl: the inner ring is about 20 local units behind the outer ring. Allowing visible triangles at grazing angles exposes that depth.

The renderer projects only models/mapobjects/portal/portal_innerwarp, when used as an SC_PORTAL surface, onto the portal entity local X=0 crossing plane. A frame-owned vertex copy supplies both precise subview bounds and the parent depth aperture. UVs, indices, animation in the plane, and the separate energy/distortion meshes remain intact. Shared assets and simulation geometry are not modified. Other portal materials are unchanged.

Direct portal recursion is now bounded by the nesting limit, including repeated views of the same portal. See PORTAL_SIBLINGS_AND_CROSSING.md for the follow-up near-plane correction. Distance limits remain unchanged.

Validation: Release engine build succeeded. Muted isolated portalrenderover captures show the destination confined to the edge-on opening; a subviews-disabled control identifies the surrounding local geometry. portalside retains its oblique destination view; multiportal still renders the nested destination. Loading portal_bloom afterward completed cleanly. Additional displaced-camera captures also show an open energy portal rendering within its rim. Captures and scripts are in validation/portal-aperture.

Deployment: Prey Settings and the custom launcher use validation/portal-aperture-build. User settings are preserved; no new option is required for this rendering correction.
