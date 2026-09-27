# Adaptive difficulty overrides

The native launcher's Difficulty override offers Adaptive Hard (50-100%) and
Adaptive Cherokee (75-100%). Respect saved difficulty remains the default.
`g_forceCherokee` retains 0/1/2 for respect/Cherokee/Normal and adds 3/4 for the
adaptive choices. Overrides apply before spawning a new level and after reading
the difficulty from a save.

`g_adaptiveDifficulty` is active session state, not an archived user preference:
0 original behavior, 1 adaptive Hard, 2 adaptive Cherokee. Adaptive modes use the
existing enemy-performance DDA calculation regardless of g_useDDA or a restored
debug force flag. The consumer-facing value is clamped, not remapped: Hard to
0.5-1, Cherokee to 0.75-1. That covers damage, AI and script consumers, including
the first frame after loading. Standard Cherokee still forces 1; Normal retains
its previous DDA setting.

Adaptive Cherokee sets g_wicked, retaining removal of health spores and basins
at spawn. Adaptive Hard clears it. Saved worlds retain their already-spawned or
removed pickups; changing mode does not reconstruct collected or absent items.

The existing saved difficulty integer encodes 0 Normal, 1 Cherokee, 3 Adaptive
Hard, 4 Adaptive Cherokee. No fields are inserted and old saves remain readable.
Older builds do not understand the new modes and interpret nonzero values as
Cherokee. New saves should be played with the updated build. Session state is
retained across level transitions and cleared on fresh map startup with no
persistent player information, before applying the launcher override.

Campaign transitions explicitly carry `rw_campaignDifficulty` in persistent
player information. The destination restores both adaptive mode and g_wicked
before map entities spawn, even with the launcher returned to respect-save.
The target_endLevel regression covers a saved Adaptive Cherokee transition and
an Adaptive Hard transition: scales remain 0.75/0.5, and health spore/basin
entities are respectively removed/retained.

`difficultyInfo` reports the active flags and effective scale. In developer mode
its optional numeric argument injects a DDA test value; ordinary gameplay
recalculates adaptive difficulty on the next update.

Validation: hidden muted tests cover low/mid/high scaler values, both adaptive
save round trips under respect-save, both existing forced modes and fresh-map
startup. Native launcher option/configuration/launch verification passed for the
source and development launcher builds. The aggregate private packaging script
was blocked by its running-process guard (PID 24052); the separately built and
tested development DLL and launcher were installed without modifying packages
in that guarded output directory.
