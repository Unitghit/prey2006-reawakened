# Recovered glow kernel (experimental)

`r_glowMode 1` selects a portable two-axis reconstruction using the decoded
Xbox 360 Prey glow shader. `r_glowMode 0` preserves the previous port effect
and remains available for comparison. The default is now mode 2, the captured
retail-PC sequence with scalable resolution; see `RETAIL_PC_BLOOM.md`.
This does not change gamma, brightness, lighting, physics, or simulation timing.

## Verified shader

The user's running Xbox title 545407E0 v0.0.0.3 was inspected read-only under
Xenia Canary 0bd090d. Vertex and pixel microcode read from the live D3D shader
objects matched the cached GPU shaders byte for byte:

- Vertex cache hash: d0ffd8aed9d2c23e
- Pixel cache hash: 7eadfcd4945aaafc

For each direction the pixel shader computes:

    sum(i=1..7, a^i * (sample(uv + i*d) + sample(uv - i*d)))

There is no center sample, normalization, threshold extraction, or intermediate
ALU saturation. The CPU sets a = r_glowAlpha (default 0.55) and d to 1/512
in one axis. r_glowSteps and r_glowAlphaChange are not read by that Xbox routine;
they affect only mode 0 here. Final overlay strength defaults to 0.5.

A local numerical interpreter checked the decoded pixel ALU against this formula
for 500 random sets of RGBA samples at five alpha values, including zero.
Executable/shader dumps remain in the local validation directory; no original
binary code or game assets are added to this source implementation.

## Portable implementation

The existing depth-tested material glow mask is captured at the view resolution,
downsampled with bilinear filtering to 256x256, and processed by glow.fp in two
axes. Intermediate RGB output is clamped by the 8-bit render target. Texture
sampling clamps at the image edges. Offsets remain 1/512 in normalized UVs,
independent of screen size and framerate. The result is added to the saved scene.
A dedicated low-resolution texture avoids reallocating the full-size mask every
frame. Views smaller than 256 in either dimension and hardware without ARB
fragment programs fall back to mode 0.

## Fidelity boundary

This is NOT yet a verified reproduction of the complete Xbox pipeline, nor proof
of how the retail Windows executable implements bloom. Xbox creates a 256x256
scratch surface and texture (initialization at 0x82746df8), but also resolves
through a full-screen texture. Its helper at 0x8274e780 binds ordinary image
shaders and performs an additional image draw before the purported vertical
pass. The caller uploads vertical constants but does not rebind the dedicated
glow shaders afterward. Live pointers confirm the ordinary and glow shaders
are different objects; the ordinary pixel shader is a single sample multiplied
by interpolated color. A GPU frame trace is needed to settle the final effective
blend/viewport sequence. Mode 1 deliberately applies the recovered kernel in
both axes; it does not silently claim to emulate this console state behavior.

Xbox output gamma conversion is intentionally excluded. The user's preferred
PC brightness is independent of the kernel reconstruction.
