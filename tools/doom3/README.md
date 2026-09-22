# Doom 3 shotgun prototype

An optional additional weapon for Prey2006 Reawakened. It occupies previously
unused slot 8, selected with the 8 key or weapon cycling. Prey's weapons remain
available. The first prototype shares rifle ammunition and uses placeholder
Prey HUD artwork. It is single-player experimental work, not a finished arsenal
conversion. The expansion's double-barrel shotgun has not been implemented.

The local importer reads an original Doom 3 installation. BFG Edition's compiled
assets are not supported. It imports the shotgun models, animations, materials,
normal/specular maps and sounds into a namespaced mod folder. Imported retail
assets must stay local and must not be included in source commits or releases.

From a packaged Reawakened installation:

```powershell
python tools/doom3/import_shotgun.py "D:/Games/Doom 3" engine/doom3shotgun --prey-base engine/base
```

For development, pass the retail Prey base directory followed by the active
engine's base directory using repeated `--prey-base` arguments. This preserves
the active Prey scripts and player definition as the basis for the generated
mod adapters. The importer checks all required assets before writing and records
sizes and SHA-256 hashes in `import-manifest.json`.

Enable **Gameplay > Doom 3 shotgun > Enabled (prototype)** in the launcher.
This uses `fs_game doom3shotgun`, with a separate save/configuration folder. The
launcher copies normal controls into that folder on first use. Normal campaign
saves are not imported: this mod adds a script object and weapon slot, so loading
base-game saves is not supported. Disabling the option restores the normal game.

Start a new mod game, open the console and enter `give weapon_d3shotgun` to obtain
the shotgun. Use `give ammo` if needed. For the local prototype playtest, a
`Doom3_Shotgun` save is provided separately, outside Git. No automatic item
placement has been added to the campaign.

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
