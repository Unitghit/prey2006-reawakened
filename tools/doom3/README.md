# Doom 3 weapon prototype

Optional Shotgun, Machine Gun, Chaingun and Plasma Gun for Prey2006 Reawakened.
Press 2 to cycle Hunter Rifle / Shotgun, 4 for Leech Gun / Machine Gun,
5 for Autocannon / Chaingun, or 6 for Acid Sprayer / Plasma Gun.
Original weapons are blue and the first addition in each slot is orange.
Both silhouette and surrounding highlight change color. Wheel/controller
cycling visits all available guns.
This is single-player experimental work, not the completed arsenal conversion.
The expansion's Super Shotgun and Doom-only mode are not implemented yet.

The local importer reads an original Doom 3 installation; BFG assets are not
supported. It imports namespaced models, animations, materials, normal/specular
maps, sounds and the Machine Gun and Chaingun ammo-counter GUIs. Generated retail assets
must stay local and must not be included in source commits or releases.

From a packaged Reawakened installation:

```powershell
python tools/doom3/import_shotgun.py "D:/Games/Doom 3" engine/base --prey-base engine/base --save-compatible
```

The historical command name is retained. Compatible imports now install all four
weapons. For development, pass the retail Prey base directory followed by the
active engine's base directory using repeated `--prey-base` arguments. The
importer validates dependencies before writing and records hashes in
`base/doom3-import-manifest.json`. Default non-compatible mode still imports
only the historical standalone shotgun prototype.

Enable **Gameplay > Doom 3 weapons > Enabled (prototype)** in the launcher.
Owning the Hunter Rifle unlocks the Shotgun. Owning the Leech Gun unlocks the
Machine Gun, including in existing saves. Old rifle-only Machine Gun grants
are hidden until the Leech Gun is acquired; their ammo remains stored.
Owning the Autocannon unlocks the Chaingun, also on existing saves.
Owning the Acid Sprayer unlocks the Plasma Gun, including in existing saves.
Normal campaign saves use the same folder with the option enabled or disabled.
Keep imported assets installed when disabling the option.

Rifle pickups split between the unlocked independent reserves. Leech Gun
energy absorption is unchanged and never supplies Machine Gun bullets:

| Weapon | Total ammunition, including loaded rounds | Magazine |
| --- | --- | --- |
| Hunter Rifle | 75 before Leech unlock, then 50 | Original behavior |
| Shotgun | **16 shells** | 8 |
| Machine Gun | **180 rounds** | 60 |
| Autocannon (when Chaingun unlocked) | 200 primary rounds | Original behavior |
| Chaingun | **300 rounds** | 60 |
| Acid Sprayer (when Plasma Gun unlocked) | 12 acid units | Original behavior |
| Plasma Gun | **250 cells** | 50 |

Primary Autocannon pickups supply only the slot-5 pair. For example, a 40-round
pickup gives 20 Autocannon rounds and 30 Chaingun bullets when both reserves
have room. Acquired rounds stay separate. Autocannon secondary grenades are
unaffected. Existing Autocannon ammunition is retained when adding the Chaingun;
its new reserve receives subsequent pickups, without a free refill.

The shotgun remains capped at 16 shells with either two or three weapons
unlocked; the Machine Gun is capped at 180 total rounds, including its loaded
60. A 30-round rifle pickup gives 10 rifle rounds and 3.2 shells before the
Leech Gun unlock. Afterwards it also gives 24 Machine Gun rounds. Each receives
a fixed third of its original normalized pickup supply; slot 5 receives fixed
halves. Full or locked partners never donate their unused share. Fractional
credit survives pickups and saves; acquired ammunition never
transfers between guns. Reloading only moves rounds into the magazine.

Original shared-ammo saves receive a one-time normalized split. Upgrading an
already independent shotgun save preserves its bullets and shells; the new
Machine Gun starts empty and receives its share from subsequent pickups.
Existing addon totals are clamped to 16 shells and 180 Machine Gun rounds
when enabled. Legitimate rifle ammo
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

## Slot-4 unlock and balance revision

Selection groups and ammo-supply groups are independent. Machine Gun selection
now uses slot 4 and follows the Leech Gun's lock/unlock state, while rifle
pickups still supply its own bullet pool. Existing saves without a Leech Gun
cannot select the earlier prototype grant. Acquiring the Leech Gun or loading
a save that owns it activates the Machine Gun. No extra tooltip was added.
Hidden, muted validation covers both unlock paths, slot-2/slot-4 cycling,
new total caps, fractional pickup allocation, mode toggles, and saved ownership.
Earlier validation figures above describe the historical balance at that time.

Manual reload is bound to **R** by default and can be changed in the game's
**Options > Controls > Combat > Reload** row. Existing profiles receive the
binding once if R is unused and no reload binding exists. Custom assignments
and intentionally cleared bindings are retained. All second weapons (Shotgun
in slot 2, Machine Gun in slot 4 and Chaingun in slot 5) use orange highlights
and icons.

## Chaingun

The Chaingun is the orange second weapon in slot 5. The retail model, ammo GUI,
reload animation and sounds are imported locally. Its adapter retains a
60-round magazine, 0.4-second spin-up, one-second spin-down, seven-frame-at-60-Hz
fire interval, spread 5, damage 20 and knockback 1. Barrel rotation uses elapsed
simulation time. It has no invented alternate fire or shotgun-specific gib rule.
Prey's weapon class now exposes the existing getWorldModel script event so the
first-person and world barrels can rotate together. Older weapon scripts remain
unchanged for save compatibility.

Hidden, muted tests verified visuals and counter updates, separate spending,
reloads, slot-5 selection, fractional supply and overflow, full-reserve rejection,
untouched grenade ammunition, equipped and disabled save/load, repeated toggles,
Spirit Walk and a real level transition. A 56-round magazine and 116 total bullets
survived Spirit Walk, transition and save/load without changing 100 Autocannon
rounds or two grenades. The existing Machine Gun ammo regression still passes.
Use test_chaingun_ammo.ps1 for the repeatable slot-5 arithmetic/save regression.

## Fixed pickup shares and 180-round Machine Gun revision

The Machine Gun capacity increase does not increase its pickup awards. All
rifle ammo and weapon drops use fixed thirds, even before the Machine Gun unlock.
Autocannon ammo and weapon drops use fixed halves. The regular 50-round ammo
box supplies 25 Autocannon rounds and 37.5 Chaingun bullets; a regular weapon
pickup supplies 50 and 75. Full partners no longer transfer their pickup share.
Secondary ammunition remains untouched. Tests cover authored ammo entities,
weapon pickups, dropped weapons, fractional persistence and full reserves.
Historical validation figures above describe earlier revisions.

## Slot-6 Plasma Gun

The Plasma Gun is the orange second weapon in slot 6. It uses independent cells
supplied only by Acid Sprayer ammo boxes and weapon pickups, including dropped
weapons. Fixed half shares apply: the regular four-unit acid pickup supplies
two acid units and 40 cells; the weapon pickup supplies eight acid units and
160 cells. Full reserves never transfer their unused pickup share. Existing acid
ammo is preserved, including amounts above the paired 12-unit cap; a newly
unlocked cell reserve receives subsequent pickups without a free refill.
Slot 5 retains its existing Autocannon/Chaingun pair and ammo rules.

The local adapter imports the retail models, animations, Bank GUI font, sounds,
bolt mesh and trail/impact particles. It retains a 50-cell magazine, 0.125-second
fire interval, zero spread, 700-unit/second bolt speed and 16 direct damage.
Prey's projectile class supplies portal traversal and collision behavior; the
retail particles run through Prey's FX system. It has no invented alternate fire.

Stable inventory index 13 and ammo index 13 leave quick grenades at weapon index
12. Existing weapon scripts and fixed save array sizes are unchanged. Hidden,
muted tests cover unlock/selection, ammo boxes and dropped weapons, independent
spending, reloads, fractional/full pickups, enemy damage, portal shots, equipped
and disabled saves, Spirit Walk and a real level transition. The repeatable
test_plasma_ammo.ps1 checks ammo and save behavior. Set d3_plasmaTrace to 1 only
for diagnostic bolt traversal/impact logs.
