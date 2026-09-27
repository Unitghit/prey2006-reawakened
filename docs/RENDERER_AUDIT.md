# Material and model-loading audit

Scope: source inspection and read-only retail asset inventory, 2026-09-27,
after d2214d3. This is a focused first pass, not an assertion that the rest of
the engine is complete or a history-based attribution to upstream developers.
No runtime changes or installation are part of this audit.

## Inventory and method

Run `python tools/audit-render-assets.py <retail-base> --output <report.json>`.
The tool reads archives without extracting or modifying assets. Later archive
filenames override earlier copies of the same file. It inventories declarations,
not live scene instances or proof that every material is used in the campaign.
Loose overrides and mod packages must be audited separately.

Local retail set: 7 archives, 90 material files, 5,510 non-table declarations;
604 ASE, 838 LWO and 99 MD5 mesh files. No MA or FLT models in this set.
Source references below are repository-relative. Assets and personal saves are
not included in this report or repository.

## Findings and recommended order

### 1. ASE vertex-color alpha is uninitialized (confirmed code defect)

`neo/renderer/Model_ase.cpp`, ASE_KeyCFACE_LIST writes RGB but not the fourth
byte of vertexColors. Face storage comes from Mem_Alloc, not cleared storage.
`neo/renderer/Model.cpp`, ConvertASEToModelSurfaces compares and copies all four
bytes, so alpha and vertex deduplication can depend on uninitialized data.

250 retail ASE files contain vertex colors. A further material-table cross-check
found 239 referencing vertexColor materials; this is not proof that their visible
passes consume alpha. Opaque RGB-only lighting may conceal the defect.

Recommended fix: set alpha to 255 for ASE RGB colors. Verify with an authored
colored-mesh fixture using alpha blending and inverse vertex color, repeated
loads and different allocation histories. Keep painted RGB untouched. Do not
infer that every one of these models currently looks wrong.

Related validation defect: the MESH_NUMCVFACES branch checks numTVFaces rather
than numCVFaces. Several parser bounds checks also happen after writes. Treat
these as a separate loader-validation change with malformed fixtures; there is
no evidence from this pass of a retail crash caused by them.

### 2. View-dependent beam snapshots use frame-based caching (confirmed mismatch;
visual impact needs reproduction)

`neo/renderer/Model_beam.cpp` builds its ribbon from view origin.
`neo/renderer/Model_hhBeam.cpp` uses current_view axes to build ribbon geometry.
Both are continuous models. `neo/renderer/tr_light.cpp`, R_EntityDefDynamicModel
uses a per-view generation only for .prt; other continuous models use frameCount.
A second portal view within a frame can therefore reuse a ribbon facing another
camera unless an entity callback invalidates it.

Recommended test: one beam seen directly and through two portals from different
angles in a single frame. A fix must preserve queued draw geometry as well as
change the cache key, as with the previous particle fix. Do not simply regenerate
and overwrite a mesh that an earlier queued view still references.

### 3. Corona and jitter deformation are accepted but not implemented

`neo/renderer/Material.cpp`, ParseDeform skips the arguments and sets two-sided /
no-shadow flags for corona and jitter. It does not assign a functioning deform.
`neo/renderer/tr_deform.cpp` has no corresponding implementation in its dispatch.

Retail inventory: 22 corona materials and 9 jitter materials. Examples include
textures/sfx/corona/corona_a, spiritbridge_fadeon_jitter1 and turbine_hologram1.
Their underlying geometry, textures and animated color stages can still render;
this does not mean 31 effects are entirely invisible.

Recommended work: isolated fixtures and retail comparison to establish billboard,
visibility/occlusion, displacement and timing semantics. Do not replace corona
with a guessed flare or animate jitter by arbitrary amounts.

### 4. Custom light programs remain an incomplete renderer feature

114 materials use blend shader. Programs include skin (88 occurrences), cloth
(22), interactionMasked (27), hair (16), parallax (6), interactionexp (4), and
single occurrences of liquid, interactionLiquid and atmosphere. Occurrences are
program stages, not unique materials, so they do not sum to 114.

The priestess fix correctly prevents unsupported light programs from running as
ambient overlays. 113 materials have conventional lit stages and now retain
those fallbacks. The program-only outro atmosphere is preserved on its legacy
path; it is a specific remaining fidelity concern, not validated as retail-correct.

shaderLevel1/2/3 and shaderFallback1/2/3 are still no-ops in the parser. Global
quality controls disable some broad lighting features, but do not implement the
retail per-stage selection. Full implementation needs stage selection, correct
per-light shader bindings, normal-map encoding, projected-light clipping, shadows
and ambient-light handling together. Enabling these programs alone would revive
the orange-body bug or double the light contribution.

This is the largest fidelity project identified here. Implement and compare one
shader family at a time, beginning with skin/cloth. Keep working fallbacks until
that path is validated, including nested portals and the lighter.

### 5. highres is ignored, with limited impact at current texture settings

315 materials mention highres. ParseStage consumes it without changing image
loading policy. With downsizing disabled this may have no visible effect. Test
under reduced texture settings before deciding how it maps to allowPicmip or
other image policy; do not assume its exact retail semantics from the name.

## Checked and not counted as new defects

- growIn/growOut: 48 / 51 materials use these names as table expressions. No
  standalone directives were found. ParseTerm resolves named declaration tables,
  so the unused parser branches do not establish broken growth animation.
- deform beam: 63 materials request it, but .beam models have a separate
  hhRenderModelBeam implementation. The skipped keyword is not evidence that
  those beam effects are absent. The multi-view cache concern above is separate.
- ASE color parsing uses atof(token), which looks suspicious, but token aliases
  the mutable ase.token buffer. It is not evidence that RGB always parses as zero.
- The recent merged-normal fix reaches all three ASE/LWO/MA merge call sites.
  All-explicit-normal inputs keep generateNormals false. MD5 skinning follows
  a different path and is not evidence of the same merge bug.
- Scope, shuttle and spirit-view flags have ambient rendering checks; their
  presence in parser code is not itself an unimplemented-feature finding.
- Old exporter path warnings can come from unused material-table entries. The
  hidercage CAGE_BAS entry was not the monitor's actual material. Avoid automatic
  filename substitutions without checking referenced mesh material IDs.

## Validation boundary

This pass reviewed source and asset usage and checked the inventory lexer with
small synthetic comment/string/table cases. It did not reproduce new runtime
failures or change a running game. Existing colorproblem and monitor regressions
remain documented in tools/tests. The next safe implementation is the small ASE
alpha initialization fix with an independent fixture, followed by beam multi-view
reproduction. Larger effect work should stay separate from those repairs.

## First implementation pass

Implemented ASE RGB-to-RGBA alpha initialization and per-view snapshots for
both built-in `_beam` and `.beam` ribbon models. Their existing snapshot
replacement allocates fresh triangles; static-model destruction calls
R_FreeStaticTriSurf, which defers disposal while frame data exists. Earlier
queued views therefore retain their geometry. Other continuous model types
keep their previous cache cadence.

Validation: Release engine build passed. `python tools/tests/ase_vertex_alpha.py`
compiles the actual color-face parser against an independent RGB/winding fixture
and verifies identical opaque RGBA for all 256 poisoned allocation patterns.
A hidden, muted engine run loaded colorproblem, saved/reloaded it, and loaded
multiportal2 and captured its nested views without a fatal error. This is a
runtime regression test, not yet an isolated visual reproduction of a beam
facing the wrong portal camera. The beam change is based on the explicit
view-dependent geometry and verified deferred lifetime path.

Expected benefits: deterministic vertex transparency and vertex deduplication
for colored ASE models; correctly oriented beam ribbons in distinct views.
These are correctness changes, not a demonstrated FPS improvement. Beam-heavy
portal scenes can do more geometry work because each camera needs its own mesh.

Still pending: malformed ASE validation, dedicated multi-view beam comparison,
retail-matched corona/jitter deformation, full custom lighting programs and
quality-stage selection, and verified highres semantics. Existing lighting
fallbacks remain active. No claim is made that all missing renderer features
identified in this audit are implemented.
