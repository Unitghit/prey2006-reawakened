# Skybox bloom

The retail bloom eligibility gate allowed SC_PORTAL and DI_PORTAL_RENDER views
but rejected SC_PORTAL_SKYBOX. Main-view glow cannot see the sky room's surfaces,
so skies lost their authored bloom entirely.

Allow direct skybox backgrounds to run the existing retail bloom pass. This
uses the same glow stages, kernel, resolution and strength as other scenes.
The existing global bloom toggle still applies. Sky bloom is independent of
the portal bloom toggle, because a main camera's sky is not a portal effect.
Other subview types retain their previous eligibility rules.

Release build and configurator verification passed. Muted sky_no_bloom capture
before/after shows restored blue-white cloud/horizon bloom with unchanged
foreground structure. Toggle-off capture removes the bloom. boxrender regression
renders the remote sky through a portal. Evidence: validation/sky-bloom.
The running retail executable was not captured or modified in this pass; this
fix restores a demonstrably excluded render pass, not a pixel-exact retail match.
Settings/custom launcher target validation/sky-bloom-build. All user choices,
including forced Cherokee on save load, are retained.

## Parent composition correction

The standalone sky overlay above was superseded by sun-bloom-build. Read-only
inspection of running E:/1 Games/PC/Prey 2006/PREY.exe (PID 42376) found:
- 0x4e3340 constructs the glow view, 256 square, with glow flag at +0x183.
- 0x481ba4/0x481bad filters non-glow stages from glow views.
- 0x483caa excludes subviews from the final overlay; 0x483d79 reads strength.
- Live strength .5, alpha .55, change .85, steps 8, skip 0 matched the running
  port. Sky/cloud/sun image dimensions and formats also matched.
Disassembly and live-value/image reports are in validation/sun-bloom.

The port now retains a frame-owned sky-view pointer on its parent, draws its
emission geometry into the parent's bloom mask, masks it with parent geometry,
then blurs and overlays once after scene rendering. Standalone sky bloom is
removed. This restores halo spread across sky/rock boundaries without reducing
global bloom or changing authored textures. Each portal parent owns its own sky
reference, reset on view generation; no persistent entity pointers are added.

Release build passed. Muted sunbright native/256/off captures, boxrender and
threeportals regression passed. Compared before/after captures: edge halo is
restored; the sun core remains bright. Exact matching of the user's perceived
sun intensity is not established. Do not claim a complete intensity fix.
A separate original-engine capture attempt was inconclusive (one had stale
bloom-off settings, another showed an authorization overlay); those captures
are not reference evidence. The user-provided original screenshot and read-only
live code/settings are the reference for this correction. The existing original
process was not modified. The Windows capture helper failed with 0x80004002.

The remaining giant sun hotspot was subsequently reproduced with view effects enabled and fixed separately; see SUN_CORONA.md. Earlier captures disabled the offending legacy overlay.
