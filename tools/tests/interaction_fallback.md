# Custom interaction fallback regression

`blend shader` was parsed as an additive ambient stage. Retail skin, cloth,
hair and masked interaction programs expect per-light origins, projection,
falloff textures and the interaction vertex layout. The ambient path supplies
none of that state reliably, so unrelated draws can tint an entire character.

Classify these stages as SL_SHADER. The current backend uses the material's
existing diffuse/specular/bump fallback instead of executing the unsupported
program a second time as an overlay. This is a fallback correctness fix, not
an implementation of retail custom skin/cloth lighting. Program-only effects
retain their previous behavior so this change does not hide the outro atmosphere.
Normal ambient/additive/glow stages are not disabled.

Run interaction_fallback.cfg with the private colorproblem save, isolated
profile and hidden desktop, muted with s_volume_dB -60. It samples seven view
angles with the lighter on. Also check without the lighter, save/reload, and
nested portals. Existing specular, normal maps, metal highlights and glow
should remain visible without a view-dependent whole-body color overlay.

For deterministic fault injection, put this original diagnostic program at
base/glprogs/skin.vfp in the isolated profile only:

```
!!ARBvp1.0
OPTION ARB_position_invariant;
END
!!ARBfp1.0
MOV result.color, {1, 0.35, 0, 1};
END
```

The previous engine paints the priestess orange; the corrected engine keeps
her lit fallback material. Remove the diagnostic override before ordinary
visual checks or deployment. This tests the erroneous overlay path independently
of whichever stale OpenGL light state a particular view happens to leave behind.
The reported saturated retail artifact was not reproduced at every test angle;
the injected test establishes that the bad pass can no longer tint the surface.

Retail material audit: 114 definitions use blend shader; 113 also have
conventional lit stages. The remaining outro atmosphere has no such fallback
and is deliberately preserved. No retail assets or user saves are committed.
