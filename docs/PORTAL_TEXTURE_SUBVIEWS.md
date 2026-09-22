# Texture subview scratch ordering

The weirdartifact save (salvage) reproduced a rectangular image from another
view over the lower-left portion of the moving doorway portal. Disabling bloom,
postprocessing, or additional portal nesting did not remove it; disabling subviews did.

R_GenerateSubViews previously rendered direct portals before later-sorted
texture subviews. Texture subviews crop the shared framebuffer, render into its
lower-left corner and capture the result to a texture. That scratch draw could
therefore overwrite a completed direct portal image before its parent consumed it.

Generate texture subviews first, then the direct sky background, then direct
portals. Preserve the one-background-per-parent rule and existing recursion guards.
No movement, collision, portal transforms or save format changes.

Validation: Release build, muted weirdartifact captures with bloom on/off and
three portal layers, movement captures, threeportals and boxrender regression
loads/captures, and configurator --verify passed. The captured rectangle is gone.
The short movement sequence checks nearby movement; it is not exhaustive testing
of every crossing direction or moving portal configuration.
Evidence: validation/weird-artifact. Deployment: validation/portal-scratch-build;
Prey Settings and the custom launcher updated, existing user settings retained.
