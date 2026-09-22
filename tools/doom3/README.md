# Doom 3 weapon prototype

Optional additional Shotgun and Machine Gun for Prey2006 Reawakened. Both share
HUD slot 2 with the Hunter Rifle. Press 2 repeatedly to cycle Rifle (blue),
Shotgun (orange), Machine Gun (red). Both the silhouette and surrounding
highlight change color. Wheel/controller cycling visits all available guns.
This is single-player experimental work, not the completed arsenal conversion.
The expansion's Super Shotgun and Doom-only mode are not implemented yet.

The local importer reads an original Doom 3 installation; BFG assets are not
supported. It imports namespaced models, animations, materials, normal/specular
maps, sounds and the Machine Gun's ammo-counter GUI. Generated retail assets
must stay local and must not be included in source commits or releases.

From a packaged Reawakened installation:

```powershell
python tools/doom3/import_shotgun.py "D:/Games/Doom 3" engine/base --prey-base engine/base --save-compatible
```

The historical command name is retained. Compatible imports now install both
weapons. For development, pass the retail Prey base directory followed by the
active engine's base directory using repeated `--prey-base` arguments. The
importer validates dependencies before writing and records hashes in
`base/doom3-import-manifest.json`. Default non-compatible mode still imports
only the historical standalone shotgun prototype.

Enable **Gameplay > Doom 3 weapons > Enabled (prototype)** in the launcher.
Owning the Hunter Rifle unlocks both additions, including on existing saves.
Normal campaign saves use the same folder with the option enabled or disabled.
Keep imported assets installed when disabling the option.

Rifle pickups split into three independent reserves:

| Weapon | Total ammunition, including loaded rounds | Magazine |
| --- | --- | --- |
| Hunter Rifle | 50 | Original behavior |
| Shotgun | **32 shells** | 8 |
| Machine Gun | 200 rounds | 60 |

The shotgun's cap remains 32 with either two or three weapons installed. The
Machine Gun's original 600-round capacity is divided by three. A 30-round rifle
pickup gives 10 rifle rounds, 6.4 shells and 40 Machine Gun rounds when all
three reserves have room. Fractional credit survives pickups and saves. Incoming
overflow is redistributed to reserves with room; acquired ammunition never
transfers between guns. Reloading only moves rounds into the magazine.

Original shared-ammo saves receive a one-time normalized split. Upgrading an
already independent shotgun save preserves its bullets and shells; the new
Machine Gun starts empty and receives its share from subsequent pickups.
Existing shell totals above 32 are clamped to 32, while legitimate rifle ammo
above its reduced cap remains until spent. Disabling/re-enabling preserves
reserves and magazines without splitting or refilling them again. Rifle sniper
ammo and the authored low-ammo puzzle recharge remain rifle-only. Dynamic
cabinets consider all three primary reserves.

Stable inventory indices remain 8 for the Shotgun, 9 for the Spirit Bow and 10
for the Machine Gun. Independent ammo indices are 10 and 11. Array sizes and
binary save layouts are unchanged. Exact script weapon selection still uses
internal indices. HUD bindings preserve the retail GUI window tree and events.

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

`weapon_d3shotgun.script` (legacy), `weapon_d3shotgun_v1.script`, and
`weapon_d3machinegun_v1.script` are save ABI files: preserve their contents and line numbers. Future incompatible
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
rifle-ammo items. A shotgun blast spent only shells; a rifle shot spent only bullets. Reloading
changed the magazine without changing either total. A one-round legacy save
migrated to a one-shell magazine, not eight free shells. All tests were muted.

## Machine Gun validation

The adapter uses a 60-round magazine, 0.1-second minimum firing interval,
one projectile per shot, spread 1, damage 9 and knockback 2. It has no invented
alternate fire or shotgun concentrated-blast gib logic. Imported animations
retain their original sound events; projectile impacts use Prey's effects.

Hidden, muted tests verified the model and working ammo display, red group-2
selection, automatic firing, reload without ammo creation, separate spending,
three-way fractional allocation and overflow, and a fixed 32-shell shotgun cap.
Saving/restoring while equipped or disabled preserves ammunition. Spirit walking
and a real level transition retained a 58-round magazine and 198 total rounds.
An older split save retained 74 rifle rounds and its capped 32 shells, with the
new Machine Gun empty. `test_machinegun_ammo.ps1` covers the three-pool arithmetic;
`test_weapon_ammo.ps1` remains the historical two-pool regression.

The Machine Gun counter uses live magazine/reserve values and the imported
retail warning/background animation. Implicit GUI materials must retain
`colored` and `clamp`, matching the engine-generated material: without `colored`,
the dark translucent test-digit backing becomes opaque white "88" over the
live counter. This material-only correction preserves the GUI window tree and
versioned script, so existing weapon saves receive it without migration.
