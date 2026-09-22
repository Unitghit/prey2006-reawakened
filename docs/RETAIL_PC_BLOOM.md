# Retail PC bloom with scalable resolution

`r_glowMode 2` is the default. It implements the retail Windows sequence observed
in the user's original executable with apitrace: a separate glow-only geometry
view with its own depth buffer, a horizontal then vertical blur, and a 0.5-strength
additive overlay after the scene's material postprocessing. It reuses the existing
submitted geometry; it does not run simulation or animation again.

`r_glowResolution` controls this mode:

- **0 (default):** follow the internal view width and height automatically.
- **256:** use the captured 256x256 reference and its original repeated draws.
- **512, 1024, etc.:** a fixed square budget, capped separately to the view width
  and height. These settings use the smooth resampled kernel when not 256x256.

`r_glowMode 1` retains the experimental reconstructed Xbox kernel. Mode 0 retains
the earlier port blur. The combined AllFixes launcher selects mode 2, resolution 0,
alpha 0.55, alpha change 0.85, steps 8, and strength 0.5.

## Reference path

The 256-square path copies the center once, then blends shifted textured quads at
plus/minus i/256 for i=1..7 in each axis. Each quad's RGBA color is
`c_i = 0.55 * 0.85^(i-1)` and its blend is SRC_ALPHA, ONE. The RGB8 source has
implicit alpha 1, so each RGB contribution has weight `c_i*c_i`. The horizontal
result is retained while the vertical pairs are added. Each draw rounds/clamps
through the framebuffer. The result is enlarged with linear filtering and added
to the scene. Geometry shifts preserve the captured behavior at screen edges;
they do not stretch the texture coordinates.

## Higher-resolution path

Merely increasing the mask resolution with the same sparse offsets makes thin
neon tubes appear as repeated outlines. Instead, the reference weights are
linearly resampled onto the target texel grid and normalized to their original
total gain. The offsets remain in reference screen units, preserving halo width
as resolution changes. Adjacent taps share a bilinear texture fetch.

Generated ARB fragment programs perform the two separable passes. Programs are
cached by axis size and blur settings, regenerated after shader reload or context
restart, and checked against driver instruction, texture-fetch and parameter
limits. Unsupported programs fall back to the 256-square reference. Sampling
clamps to the copied view's texel centers, including non-power-of-two targets.

This is a quality enhancement, not bit-identical retail rendering: interpolation
fills the gaps between the original taps, accumulation rounds once per shader
pass rather than per quad, and border samples clamp to the visible mask. Native
resolution uses more GPU time and texture memory, especially at 4K. A fixed 512
or 1024 budget is available for a performance/quality compromise.

## State and scope

The low-resolution view uses copied draw-surface descriptions with outward-rounded
scissors. It does not mutate the frontend's surface list. The main view's existing
color is saved and restored around the glow prepass, preserving previously drawn
direct subviews; its depth is rebuilt normally. The final overlay does not write
depth. Menus and editor views remain excluded. Retail-mode portal views now
receive bloom; see [PORTAL_BLOOM.md](PORTAL_BLOOM.md). Other subview types retain
their previous behavior.

Captured evidence is local to `validation/retail-bloom-capture/FINDINGS.md` in the
workspace, with the original API trace and intermediate PNGs. Retail shader code
and game assets are not embedded in this source change. The implementation is
derived from the observed draw operations and blend math.

## Validation (2026-09-21)

- Release build and whitespace checks passed. Updated binaries/packages and
  matching linker maps were deployed to `validation/presentation-build`; the
  previous build is preserved in `validation/presentation-before-retail-bloom`.
- An apitrace capture of the port verified that all 14 RGBA blur weights in the
  256-square path match the retail capture. An independent numerical check of
  its intermediate images found maximum errors of 4/255 and mean errors below
  0.3/255 over active pixels, consistent with framebuffer blend precision.
- The captured native-resolution shaders preserve the reference kernel gain
  (2.956124 per axis). Their intermediate images match independent CPU evaluation
  of the captured shader constants within 1/255 per channel. At 1280x720 the
  bilinear-paired shader uses 40 horizontal and 23 vertical texture fetches.
- Muted playtests at 1280x720/144, 1920x1080/60 and 3840x2160/144 exercised mode 0,
  mode 1, mode 2 at native/256/512 resolution, and strength/alpha/skip controls.
  Native 4K output was visually inspected. These are configured FPS caps, not
  benchmark claims that each scene sustains those rates.
- Ten menu cycles, shader reload, renderer restart, map transition, portal views,
  and save/teleport/load completed. The portal save restored the expected
  `(290 -1440 800.25)`, yaw 0. Restart and portal screenshots were inspected.
- Reports: `validation/retail-bloom-report.json` and
  `validation/retail-bloom-capture/port-verification.json`. Trace replay reported
  an SDL startup-window size mismatch (32x32 versus Windows' 120x32 minimum);
  the analyzed gameplay images are the correct size. Full campaign coverage,
  hardware fallback behavior on other GPUs and performance benchmarking remain
  outside these targeted checks.
