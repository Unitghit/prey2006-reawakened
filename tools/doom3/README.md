# Doom 3 shotgun prototype

An optional additional weapon for Prey2006 Reawakened. It shares HUD slot 2
with the Hunter Rifle. Press 2 repeatedly to cycle between them. Prey's weapons remain
available. The historical standalone prototype shares rifle ammunition; compatible installs
use independent bullet/shell reserves and Prey HUD artwork
with an orange selection highlight and silhouette for the shotgun. It is single-player experimental work, not a finished arsenal
conversion. The expansion's double-barrel shotgun has not been implemented.

The local importer reads an original Doom 3 installation. BFG Edition's compiled
assets are not supported. It imports the shotgun models, animations, materials,
normal/specular maps and sounds into a namespaced mod folder. Imported retail
assets must stay local and must not be included in source commits or releases.

From a packaged Reawakened installation:

```powershell
python tools/doom3/import_shotgun.py "D:/Games/Doom 3" engine/base --prey-base engine/base --save-compatible
```

For development, pass the retail Prey base directory followed by the active
engine's base directory using repeated `--prey-base` arguments. The importer
checks dependencies before writing and records hashes in
`base/doom3-import-manifest.json`. The default importer mode remains available
for historical standalone prototype installations.

Enable **Gameplay > Doom 3 shotgun > Enabled (prototype)** in the launcher.
The save-compatible build uses the normal campaign save folder in both modes.
Owning the Hunter Rifle makes the shotgun available in group 2. Disabling the
option hides it and returns to an owned Prey weapon, retaining its magazine.
Compatible installs now split rifle-ammo pickups into **independent bullets and
shells**. With both guns available, capacities are 75 rifle rounds and 160
shells (half of their original 150/320 capacities). A 30-round rifle pickup gives
15 bullets plus 32 shells if both reserves have room. This preserves normalized
supply, not equal raw counts. Incoming overflow goes to the other gun if one is
full; fractional rounds carry forward to later pickups and survive saves.

Firing/reloading never draws from the other gun's reserve. Prey's inventory
totals include loaded rounds, so a reload changes the magazine without adding
ammunition. Old shared-ammo saves are migrated once; their existing total is
split proportionally, and loaded magazines are clamped if necessary. Disabling
the shotgun retains its shells and magazine. Re-enabling does not split again,
convert ammo or refill anything. Legitimate rifle ammo above the enabled cap is
retained until spent. Rifle sniper ammunition and the original low-ammo puzzle
recharge remain dedicated to the rifle. Dynamic cabinets consider both primary
reserves when deciding whether to offer rifle-group supplies.

Number keys cycle within groups. Wheel and controller
weapon cycling visit each available weapon in group order. Internal saved
index 8 remains unchanged; scripts still select exact internal weapon indices.
The existing HUD layout, fade timing and absence of weapon-name popups remain.
Orange and red highlight/icon variants are generated locally from retail artwork. The
red variant is reserved for the future third weapon, not an implemented gun.
The HUD window tree must not change: it is part of the binary save layout.

The launcher copies old standalone prototype saves into the shared folder as
`D3Legacy_*`, displayed with a `Doom 3 legacy:` prefix. Originals and existing
destination saves are never overwritten. The original mod folder is retained.
Normal campaign and old shotgun saves can be loaded without starting over.

Keep the imported assets installed when disabling the weapon. A saved weapon or
thread must first be restored before the mode can safely hide it. This is
compatibility between modes in the updated port, not a guarantee that new saves
will work with an older executable or the retail engine.

## Save format and validation

The baseline `prey_main.script` is unchanged in compatible installations.
The weapon compiles its versioned script on first construction; idProgram
records this extra file with the save and restores it before saved objects.
Its globals and checksum must match. The loader also tests the exact historical
shotgun baseline before constructing saved threads, allowing old mod saves to
load without relocating instructions or bypassing validation.

`weapon_d3shotgun.script` (legacy) and `weapon_d3shotgun_v1.script` (additive)
are save ABI files: preserve their contents and line numbers. Future incompatible
script work needs a new versioned file/type with old versions retained.
Inventory array sizes and object save layouts have not changed. Namespaced
`rw_weapon_*` dictionary state persists across saves and level transitions.

Hidden, muted validation covers normal campaign and legacy prototype saves,
enable/disable/re-enable round trips, a partly used magazine, and restoration
from spirit walking. A real end-level target also preserved a seven-shell
magazine and 149 rifle rounds through a map transition and subsequent mode
changes. Saving with a live blast tracker, then restoring with the addon off,
retained the six-shell magazine and 129 rounds. The `weaponPackInfo` console command reports inventory,
magazine, health and position. With developer mode enabled, a slot argument
selects through the usual weapon path; `key2`, `next` and `prev` exercise input
selection; `spirit` exercises spirit walking without
adding an unsaveable console script.

The weapon uses an eight-shell magazine, thirteen pellets, a 1.333-second
minimum firing interval and the original two-shell reload animations. Projectile
collisions and impact effects use Prey's firing system. This is an adapter, not
a claim of identical Doom 3 combat balance. Tommy's third-person rifle poses
are aliased for the added weapon. First-person arms are Doom 3's original arms.

Set `d3_shotgunTrace 1` for firing/reload clip-count diagnostics. Automated tests
must stay hidden and use `s_volume_dB -60` in isolated profiles. The engine's
existing `com_fpsTestAttack` diagnostic runs in its fixed-tic session path;
`com_fixedTic 1` makes rendered-frame waits deterministic for those tests.
Normal launcher play keeps the unlocked-framerate settings.

Do not save after using the engine's `script` console command in a test: it adds
a temporary `console` script source that this build cannot reopen during load.
Use normal game controls or the existing input diagnostics for save/load tests.

## Concentrated lethal blasts

The optional shotgun now uses Prey's authored gib debris for ordinary Hunters,
Fodders and Hounds when at least eight distinct pellets from one blast strike
that enemy and that blast kills it. A fresh shot has fresh counters; corpses
already dead at the first hit cannot qualify. Bosses, story characters,
cinematic actors, `noDamage`, `not_gory`, and `no_shotgun_gib` exclusions remain
protected. Gore-disabled settings take precedence.

Pellets remain physical projectiles and retain normal portal behavior. The
blast collects hits per enemy and applies pending damage after that physics
pass, then decides whether to gib. Counts and kill attribution persist across
physics ticks for the lifetime of the blast and are serialized with saves.
The pellet damage definition explicitly disables individual-pellet gibbing.
Only a qualified blast invokes the native gib path; a special damage marker
avoids Prey's global gib-effect cooldown suppressing a second enemy's debris.

Validation used hidden, muted isolated sessions. Controlled zero-spread shots
exercise the seven/eight-hit boundary without random misses; the installed
weapon retains its original thirteen pellets and spread of 22. The diagnostic
`d3_shotgunTrace 1` logs target, pellet count, kill attribution, eligibility,
gib decision, and resulting health.

Validated cases for the concentrated-blast change:

- Seven pellets, lethal: normal death, no gib.
- Eight pellets, lethal: gib.
- Thirteen pellets, nonlethal: no gib.
- Explicit `no_shotgun_gib` target: normal death.
- Gore disabled: no shotgun-triggered gib.
- Original prototype save, original spread: eleven hits and a lethal gib.
- Save/load with a live blast tracker, followed by unlocked 144 FPS rendering:
  successful restoration and expiration without a new error.


Run the repeatable campaign-save regression in a fresh output directory:

```powershell
./tools/doom3/test_save_compatibility.ps1 -Engine engine -PreyAssets engine -CampaignSave userdata/base/savegames/example.save -Output validation/weapon-save-roundtrip
```

Use a stationary ordinary campaign save with the Hunter Rifle owned. The test
copies its seed, launches hidden and muted, and asserts unchanged ammo, magazine,
health and position across the three mode states. It never runs in the player's
profile. Launcher verification separately checks that legacy save migration
preserves originals and existing destination files.

## Independent-ammo regression

`test_weapon_ammo.ps1` takes the same Engine, PreyAssets, CampaignSave and fresh
Output parameters as the compatibility test. It verifies fractional awards,
independent spending, overflowing pickups, full-reserve rejection, save/load
and repeated mode toggles. Developer-only `weaponAmmoInfo` operations support
these isolated checks. `norecharge` disables the retail rifle safety recharge
only on that test player so it cannot be mistaken for ammunition duplication.

Additional hidden playtests fired both guns, reloaded the shotgun, loaded an
active legacy shotgun, crossed a real end-level target, and activated authored
rifle-ammo items. The rifle remained at 75 while a shotgun blast spent one of
160 shells; a rifle shot spent one bullet without changing shells. Reloading
changed the magazine without changing either total. A one-round legacy save
migrated to a one-shell magazine, not eight free shells. All tests were muted.
