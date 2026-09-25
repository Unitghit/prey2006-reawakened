# Paired portal rim overlap

In the retail `portalflicker` setup, looking through the orange floor portal also rendered the blue destination portal's glow at the exit plane. Offset effect layers could survive that view's clip plane, adding a blue-white band over the orange opening that changed with viewing angle.

During model submission for a surface-mounted portal subview, omit the portal entity whose origin and facing match that view's destination coordinate frame. The entity must itself have a remote portal view. This is a per-view omission: the same portal remains visible in the main view and unrelated recursive views. No global entity visibility flags, material changes, or camera/clip-plane offsets are used. Other geometry, player models, and decals keep their existing treatment.

Validation uses matched camera captures from `portalflicker`, looking around the opening. The destination's blue glow disappears while the orange rim remains. The `exit_rim` regression checks both colors, requiring the paired exit to be omitted and the entrance still submitted in the main view. Opening, linking, replacement, reload, deep recursion, and split-player rendering are checked separately. Retail saves and assets are not distributed.
