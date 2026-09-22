# Experimental weapon lighting across portals

Launch `Play-Prey2006-PortalLighting.bat` from the workspace root. It includes the
existing bloom, framerate, portal alignment and 3x portal distance settings.
The regular launchers still use the previous build. The new option is
`g_portalWeaponLighting 1`; set it to `0` in the console for an immediate comparison.
It defaults off and is not archived in the player configuration.

## Implementation

Within 24 map units of an open teleport portal, the rendered eye's distance to
its plane controls a smoothstep lighting blend. The other room contributes zero
at 24 units and 50% at the plane. After crossing, the complementary blend fades
the previous room out. Selection uses the interpolated render camera, including
the source-side frames of an interpolated teleport.

Nearby idLight render definitions are transformed through the portal into the
current room. The renderer restricts these temporary lights to first-person
weapon-depth-hack models and the primary view. Diffuse and specular interaction
colors are weighted after shader evaluation. The weapon mesh is drawn normally;
there is no second transparent weapon image. Normal world lighting, player
physics, muzzle lights and emissive weapon effects are not blended.

All changes are renderer copies, restored after scene submission. Clone handles
are disabled between draws, reused, and freed at map shutdown. View-light frame
data retains the blend weight for the backend after restoration. New transient
renderLight fields are initialized when reading existing saves; the save layout
is unchanged. GAME_API_VERSION is 16, so this build needs its matching engine
and game DLL. Original launchers retain the previous matched pair.

## Limits

This is a light-contribution approximation, not two fully shadowed renderings.
The secondary room's geometry is not transformed, so its copied lights do not
cast shadows. Remote occlusion and light visibility can differ from the real
room, and some shadow-related changes can still occur at crossing. Render-demo
fidelity of the transient light fields has not been implemented or validated.
The prototype excludes multiplayer, cinematics, vehicles, third person and
spirit/death walking. Aperture-edge selection and unusual overlapping portals
need broader gameplay testing. Extra light interactions have a performance cost;
no general performance guarantee is made.

## Validation

Muted isolated profiles and reproducible scripts are in
`validation/portal-weapon-lighting`. Static captures compare off/on/off at 24,
12 and 1 units on both sides of portal_bloom, with bloom disabled only in these
comparison captures. Moving tests capture both crossing directions. The smoke
script reloads saves after blend activation and exercises menus and alternate
views. See the validation report for measured results.
