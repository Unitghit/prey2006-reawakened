# Extended nested portal views

Implemented 2026-09-23. Independently implemented in Prey's renderer; no Source
engine code or assets are included.

## Settings

The native launcher adds **Nested portal rendering**:

- Standard: existing three real portal layers.
- Extended: six real layers, first two at full resolution.
- Extended + repeating views (default): the same six layers, plus a restricted
  previous-frame image fallback at the cutoff.

`r_portalDeepViews` selects 0/1/2. The launcher maps the depth to 3/6 using
`r_portalMaxDepth`. The renderer's standalone defaults and older BAT presets are
unchanged. Portal render distance is independent.

Each deeper viewport halves both dimensions, down to a 16-pixel floor (never
larger than its parent). Its scissor follows the projected opening. At 1280x720,
layers 3 through 6 use 640x360, 320x180, 160x90 and 80x45. These are whole-view
coordinates; only the visible footprint is drawn. Linear filtering is used when
compositing. Distant edges consequently have less detail and do not receive the
main framebuffer's MSAA samples.

## Isolation and work limits

Optional framebuffer objects give each deep subtree its own color, depth and
stencil storage. The completed image is composited only inside the actual portal
aperture. An existing framebuffer-copy/restore path supports systems without the
required functions; `r_portalDeepTargets 0` forces that diagnostic path. Failed
framebuffer completeness checks also fall back to save/restore for that view.
Storage includes any larger texture-camera scratch viewports in the subtree.

There are at most 12 real reduced-resolution views per render frame, plus the
first two full-resolution layers. This bounds branching work as well as depth.
There are 32 reusable history slots. Framebuffer storage is released before GL
context destruction. Texture copies read the active framebuffer's color attachment.

Bloom, fog and skybox processing still runs through the normal view renderer.
The existing aperture masking and decal clipping remain in place. Nested siblings
have separate path histories; they do not share one global last-portal image.

## Repeating fallback and limits

This is an approximation, not additional simulation or unlimited fresh cameras.
At a real-view cutoff, an earlier occurrence of the same portal may supply its
previous-frame view, cropped to that occurrence's footprint and mapped into the
terminal opening. It requires:

- a terminal footprint no larger than 96 pixels in the root view;
- almost identical view orientation and no more than one unit of lateral
  displacement between the repeated cameras;
- a matching portal ancestry/endpoint-pose key and viewport size;
- immediately preceding-frame history, no backward time jump, and at most 100 ms
  of scene time difference;
- unchanged FOV, root camera movement of at most four units and small rotation.

Map/save initialization and GL restart invalidate history. Changed endpoint
transforms select fresh history. Ineligible views retain the existing terminal
appearance. Fast motion can therefore reveal the real cutoff; this intentionally
avoids displaying a stale, unrelated room. Reused distant motion can lag a frame
and perspective is approximate. This is not an exact port of Valve's shader.

## Verification

- Hidden, muted tests (`s_volume_dB -60`) using `multiportal2` confirmed all six
  layers and decreasing target sizes.
- A controlled facing pair exercised the repeating fallback. `deep_views.cfg`
  checks that Extended disables reuse and Standard restores the three-layer cap.
- Retail saves `threeportals` and `boxrender` checked sibling and skybox views.
- `straferightclip` with scripted right movement logged an actual portal exit.
- The native launcher's verification checks every choice, migration, layout at
  multiple DPIs, small-screen scrolling and Save & Play argument handling.

`benchmark view` measures the current frozen render scene for one second without
changing resolution or running the unrelated GL context benchmark. In the hidden
1280x720 `multiportal2` test, six full-resolution layers measured about 7.6 ms,
the isolated-target hybrid about 7.7 ms, and three layers about 6.9 ms. The earlier
copy/restore prototype was about 8.3 ms. These are local render-throughput samples,
not a guarantee of gameplay FPS gains: small portal views can be CPU/geometry
limited, and extra real layers still cost work.
