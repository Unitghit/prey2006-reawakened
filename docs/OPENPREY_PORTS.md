# Selected openPREY compatibility ports

Base: FriskTheFallenHuman/Prey2006 `983b171cfb76793c9842a1071472e94c1a808164`.
Local branch: `compat/openprey-fixes`. Implemented and checked on Windows x64, 2026-09-20.

These changes adapt selected fixes from themuffinator/openPREY, rather than replace Prey2006 subsystems wholesale. Existing source license headers remain in place. Credit for the upstream implementations belongs to their original authors, principally themuffinator; the adaptations below account for the different engine interfaces.

## Provenance and scope

- [fce5d2bc37f3c0e7ef715bd1f8006b97700496f2](https://github.com/themuffinator/openPREY/commit/fce5d2bc37f3c0e7ef715bd1f8006b97700496f2): portal camera transforms, viewer-side gate, portal/sky render-to-texture stages, credit/spline text presentation and event handling.
- [b6c2cde239353e0d093e42bff5003fe8dce619ae](https://github.com/themuffinator/openPREY/commit/b6c2cde239353e0d093e42bff5003fe8dce619ae): remote portal eye offset and evaluated-expression distance culling.
- [870ae4054d9a7e6af32bba2d3a884f3dcde6d230](https://github.com/themuffinator/openPREY/commit/870ae4054d9a7e6af32bba2d3a884f3dcde6d230), with source inspected at `c9601e08c08f5fe66b2e96095d4b201ea8391126`: glow-only rendering/blur/compositing and framed-window geometry.

Only the listed features are ported, not the entirety of these commits. This is not a claim of exact retail visual matching or full campaign compatibility. No game tick rate, input cadence, physics or FPS-unlock behavior was changed.

## Changes

### Portals

`renderer/tr_subview.cpp` now transforms the viewer's position and orientation through the portal instead of computing the transformed position but discarding it. It retains the source-surface scissor, uses the remote camera for PVS lookup, and rejects viewpoints behind the portal's allowed viewing plane.

Direct portal distance is read from the material's evaluated shader registers, with bounds/null checks. A negative result suppresses the view; zero leaves it unlimited. The value remains floating point instead of truncating fractional expressions.

`hhPortal::GetRenderView()` supplies the remote eye offset based on the portal collision bounds. The inherited implementation rebuilds the view each call, so the offset does not accumulate.

The already-declared `DI_PORTAL_RENDER` and `DI_SKYBOX_RENDER` types now have material parsing and stage dispatch for `portalRenderMap` / `skyboxRenderMap`. Nonpositive capture sizes are rejected. Existing spirit-visibility filtering and mirror handling are retained.

### GUI frames and text

Prey2006 already has `hhSuperWindow`; it previously discarded `leftMat`. That class now loads and draws both sides, mirrors the appropriate corners/edges, and applies background margins. Prey2006's material tint/alpha remains active. It does not replace the tab or window system.

Credit/spline window identity is preserved through parsing and rebuilds. Added presentation supports `onStart`, `onCallEnable`, `onCallDisable`, `onCallReset`, credit activation and the upstream deterministic glyph-scatter/trail effect. Existing `splineIn` / `trailOffset` registers are reused. Trails are bounded to 64 rather than the upstream 1,000. Drawing does not lazily create additional serialized GUI variables. Transient trail state is reset on save loading; it is not added to the save format.

The upstream glyph effect is a reconstruction, not a verified copy of the retail algorithm.

### Glow

The renderer now captures authored `glowStage` surfaces, blurs horizontally and vertically, then adds the result to the saved scene before scene postprocessing. Menus, editor views and subviews skip this overlay. Existing `r_skipGlowOverlay` disables it.

Adaptations:

- Use dhewm3's `qgl` entry points and Prey2006's `isGlow` stage flag.
- Reuse scene depth to keep occluded glow occluded, without rerunning the game or clearing depth/stencil.
- Use dedicated scene/glow images rather than borrowing `_accum` from unrelated effects.
- Account for power-of-two texture padding and use the correct dimension for each blur axis.
- Preserve modelview/projection/texture matrices across fullscreen passes.
- Bypass `g_lowresFullscreenFX`'s capture-size override for internal glow images, while retaining existing behavior for all old capture callers.
- Skip the extra captures when no enabled glow stage is present.

Controls retain openPREY's defaults:

| Cvar | Default |
|---|---:|
| `r_glowAlpha` | 0.55 |
| `r_glowAlphaChange` | 0.85 |
| `r_glowSteps` | 8 |
| `r_glowStrength` | 0.5 |

This is the authored glow overlay. openPREY's separate HDR, generic bloom, SSAO and CRT pipelines are outside this port.

## Build and validation

Built the unmodified baseline and modified executable/game DLL with MSVC 19.44 and the Windows SDK:

```powershell
cmake -S neo -B build/compat -G "Visual Studio 17 2022" -A x64 -DTOOLS=OFF -DDEDICATED=OFF
cmake --build build/compat --config Release --parallel 8
```

The resulting executable is `output/windows/prey06.exe`. Tests use a separate copy of retail `pak000.pk4` through `pak006.pk4`, the port's generated `pak007.pk4` and game module, and isolated save/config folders. No retail assets are included in this source patch.

Checked:

- Windows x64 Release compilation and linking.
- Retail main-menu rendering.
- `tests/fixtures/compat_ui.gui`: missing baseline side frames become visible, background margins work, and spline text animates and settles. Includes a credit activation callback.
- Roadhouse scene and neon sign at 1280x720: baseline lacks the added halo; the port renders it without sampling padded texture areas across the scene.
- Active portal in `game/dmescher2`, viewed from `setviewpos 290 -1440 800 0`: scene and portal render with the new camera transform at 1024x768.
- A new save and reload in `game/dmescher2` using the modified build.
- Git whitespace-error check.

Local reproduction runner is `../run_validation.ps1` in the containing workspace, with `-Version baseline|candidate` and `-Scene ui|scene|portal|glow|glowoff|menu|save`. It opens windowed test instances and exits after capturing a screenshot and console dump. Captures are under `../validation/<version>/profile/base/`. The runner and asset copies are local validation tools, not distributable game assets.

Known limits:

- No full campaign playthrough or multiplayer session has been tested.
- Main-menu success does not validate every menu, HUD widget or language.
- Portal texture-stage captures, negative-distance culling, the opposite side of a portal, and nested sky views still need targeted visual tests.
- The save test covers saves created by this build; existing retail or older-port save compatibility is not established.
- Initial black captures during level startup were not accepted as visual validation; subsequent checks waited for the scene and used explicit test viewpoints. Feeding Tower's opening sequence overrode the requested camera position, so the portal comparison uses dmescher2 instead.
- Baseline warnings about `_menushot`, `_default.wav`, missing retail reverb files and some map location overlaps remain. They are not treated as fixed by this port.
- Glow intensity and glyph motion have not been matched pixel-for-pixel against retail. Cross-platform builds and performance profiling remain untested.
