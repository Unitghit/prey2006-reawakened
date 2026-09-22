# Framerate follow-up completion audit

Scope: the five items in FRAMERATE_WORK_PLAN.md. Gameplay continues at the
original 16 ms tick; this work decouples and smooths presentation. Evidence reports and test-profile folders below are under the workspace
validation/ directory; source paths refer to Prey2006/neo/.

| Requirement | Evidence inspected | Result |
| --- | --- | --- |
| Resolve the captured menu deadlock | menu-hang/FINDINGS.md, sound lock guards and area validation; 160 menu cycles across feedingtowera/b; recursive-lock exception CTest | Passed |
| Smooth world transforms and skeletal poses | game_presentation.h; world-144-1 traces and shadow-report.json compare enabled/disabled rendering | Passed |
| Reject stale history after lifecycle changes | History stores generation-checked idEntityPtr, adjacent tick, model and renderer handle; teleport and portal-transfer invalidation; map/shutdown/restore reset; save/teleport/load playtest | Passed |
| Preserve platform contact, collision and ragdoll physics | platform-report.json: four rates and disabled control, 111 matching trajectory samples, no per-draw physics changes; ragdoll-pair-report.json: 123 matching physics samples from ragdoll-world-60/144; Physics_AF.cpp diff is diagnostic only | Passed |
| Keep visible models and shadows in agreement | shadow-report.json: matching geometry/transform hashes at ambient and shadow submission, with construction checks; screenshot inspected | Passed |
| Smooth weapon animation, bob/recoil and attached effects | Camera-local transforms and local skeletal joints; rifle/wrench 60/144 comparison; attached FX/light captures; beam-report.json covers canisters, world beams and leech light | Passed |
| Preserve firing, reload and effect timing | effects-report.json: identical state at shared simulation times across 30/60/144/uncapped; beam-report.json: sunbeam ammo remains one unit per 16 ms | Passed |
| Provide optional late mouse look without consuming input or changing aim | late-* traces: byte-for-byte input state checks, normal/inverted input, smoothing/menu fallback and identical final authoritative aim; authoritative trace drives the reticle; beams-144-1-late verifies attachment and unchanged far endpoint | Passed |
| Handle special view transitions | view-report.json: spirit/body, third-person and sideways/down gravity at 30/144; captured transition frames reject blending; supported normal view recovers | Passed |
| Audit particles, trails, fades and overlays | Tick-owned emissions, beam evaluation and trail creation; renderer-time smoke/material display; simulated overlay expiry and health smoothing; effects-report.json verifies durations and repeated-draw invariants | Passed |
| Preserve normal audio settings and mute tests | Root AGENTS.md, isolated runner profiles, s_volume_dB=-60 in completed runtime logs; no normal-profile or system-volume writes | Passed |
| Deliver matching tested executable/packages and usable launchers | release-smoke and release-final-transitions logs/screenshots; package CRCs and binary/source/evidence hashes in presentation-build/build-manifest.json | Passed |

The implementation is deliberately single-player presentation work. Multiplayer,
demos, altered simulation clocks and unsupported camera modes retain their
existing paths. Late mouse preview defaults off. An interpolated world is
approximately one simulation tick behind gameplay; collision and aiming use
authoritative state. These are documented design constraints, not a claim of
universal game behavior equivalence.

Coverage is targeted: representative combat, portals, platforms, skeletal shadows
and view transitions. It is not a complete campaign playthrough, an end-to-end
physical mouse/monitor latency measurement, or testing of every vehicle,
deathwalk sequence, rotating platform and custom renderer callback.
