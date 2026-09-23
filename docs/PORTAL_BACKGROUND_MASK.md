# Direct portal aperture compositing

`bigblockportal` exposed a rectangular destination image outside the distant
circular portal. Direct subviews render into a scissored part of the shared
framebuffer before their parent. Relying on the parent's depth pass to conceal
that rectangle is insufficient where background or sibling subviews preserve
existing color.

Each generated direct portal now queues a background capture before its entire
subview subtree and a masked restore afterward. The restore draws the actual
parent-space aperture into stencil, including its near-plane depth clamp, then
restores the captured pixels only outside that aperture. This also isolates
nested and overlapping sibling views. Portal contents and bloom inside the
opening remain unchanged; the separate rim still renders normally.

Only the portal scissor is copied. Scratch images are retained per ancestry
level and reused sequentially by siblings. Captures use native pixel dimensions
and nearest filtering, bypassing optional FX and texture downsize controls.
No additional scene render, distance setting, or launcher option is introduced.
The parent begins its own depth/stencil pass normally after the restore.

Validation: Release engine and private launcher build passed. Hidden, muted
before/after captures of `bigblockportal` show the square removed with the
circular destination retained. `threeportals`, `boxrender`, and `portalgunclip`
retain nested views, sky surroundings, and foreground weapon visibility.
1600x900 with an actual four-sample framebuffer also preserves the aperture.
No GL or engine errors were reported. Personal saves and captures remain in
local validation directories, outside the source repository.
