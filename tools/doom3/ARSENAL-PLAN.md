# Doom 3 weapon modes

Agreed arsenal design. The save-compatibility foundation is implemented using
the shotgun. Shared selection and HUD colors are implemented for the rifle /
shotgun / Machine Gun group, with independent reserves and three-way pickup allocation. The remaining
weapons and full modes are planned. See README.md for installation and compatibility details.

## Launcher modes

- Original Prey (default).
- Prey + Doom 3 weapons.
- Doom 3 weapons.

Keep the lighter and Spirit Bow in every mode. Do not add the pistol, BFG or
Soul Cube. Expose the modes in the native launcher; leave old preset BAT files
unchanged. Import retail assets locally, never into the source repository.

## Groups and presentation

| Key | Original weapon (blue) | First Doom weapon (orange) | Second Doom weapon (red) |
| --- | --- | --- | --- |
| 1 | Wrench | Chainsaw | |
| 2 | Hunter Rifle | Shotgun | |
| 3 | Crawler Grenades | Grenades | |
| 4 | Leech Gun | Machine Gun | |
| 5 | Autocannon | Chaingun | Plasma Gun (planned) |
| 6 | Acid Sprayer | Super Shotgun, Doom-only mode | |
| 7 | Rocket Launcher | Rocket Launcher | |

The Super Shotgun uses Resurrection of Evil assets. Its inclusion in the
mixed mode has not been requested. Colors identify variants consistently;
do not renumber colors when hiding Prey weapons in Doom-only mode.

Repeated presses of a number key cycle the unlocked weapons in that group.
Mouse wheel and controller cycling visit all selectable weapons in group order.
Preserve the existing HUD layout and weapon-change behavior. Only change the
selected slot's filled highlight color; no new weapon-name popup.

## Unlocks and ammunition

The first pickup of the original weapon unlocks all counterparts in its group.
In Doom-only mode, the same world pickups grant the Doom weapons. Repeated
pickups replenish ammunition rather than re-granting weapons. Synchronize
counterparts for weapons already owned when enabling a mode on an existing save.

Each acquired, enabled weapon gets an equal share of its group's supply budget,
expressed relative to that weapon's full reserve, not equal bullet counts.
Use fixed thirds for rifle-group pickups and fixed halves for slot-5 pickups,
including ammo-only entities and dropped weapons. Pickup rates are independent
of capacity: keep the shotgun capped at 16 total shells and Machine Gun at 180 rounds.
The Machine Gun unlocks with the Leech Gun (also on existing saves), but still
receives rifle-group pickup supply. Leech Gun energy absorption is unchanged.
The Chaingun unlocks with the Autocannon and uses slot 5's primary supply only.
Its independent reserve is capped at 300 including 60 loaded rounds; Autocannon
primary capacity is 200 while paired. Secondary grenades remain unchanged.
Preserve fractional pickup credit. Full or locked reserves do not donate their
unused pickup share to another gun. Once acquired,
ammunition belongs exclusively to its gun: firing/reloading must never consume
another gun's ammo. Mode toggles retain dormant reserves without conversion or
refills. Existing shared-ammo prototype saves receive a one-time normalized split.
Retain legitimate over-cap ammunition after mode changes until it is spent.
Track magazines correctly so switching weapons cannot duplicate loaded ammo.

Route authored ammo pickups and dynamic dispenser supplies through the same
allocation logic. Account for Prey's secondary ammo types and Leech energy modes
explicitly. Preserve energy-node interactions required for campaign progression
in Doom-only mode. Preserve native alternate fire where the imported weapon
actually has it; do not invent alternate attacks.

## Save requirements and investigation

Users must be able to select another mode in the launcher and continue their
campaign. Disabling a mode is different from deleting the installed weapon
assets: retained definitions and scripts may be needed to deserialize a save
before safely switching its active weapon.

Current source findings:

- The prototype modifies script/prey_main.script and def/player.def. Its
  fs_game directory separates saves from the ordinary campaign.
- idProgram::Save records files compiled after the baseline plus script globals
  and a checksum. Restore recompiles those files before restoring objects.
- Investigate loading versioned addon scripts after the unchanged baseline;
  never bypass script checksum validation or reinterpret saved script offsets.
- MAX_WEAPONS and AMMO_NUMTYPES are both 16. Inventory serializers write fixed
  arrays and weapon ownership is a bit mask. Increasing either constant alone
  would corrupt old save decoding. Account for Spirit Bow and quick grenades.
- Use explicit stable weapon identities. Never reinterpret a stored slot as a
  different gun merely because the launcher mode changed.

Persist unlocks, each weapon's supply and magazine, fractional credits and mode
transition state. Carry these through level transitions as well as save files.
On disabling a mode, switch away from an added weapon safely while preserving
campaign state. Define and validate a non-destructive path for legacy shotgun
prototype saves separately from normal campaign saves. Keep originals intact.

Do not promise compatibility with an unmodified retail executable or an older
Reawakened binary that lacks the added weapon classes.

## Implementation and validation sequence

1. Completed: additive shotgun scripts, normal and legacy saves, mode toggles,
   magazine preservation, spirit restoration and level-transition persistence.
2. Completed for rifle / shotgun / Machine Gun: group metadata, rifle-pickup unlock, number-key
   and wheel/controller selection, original HUD with colored highlights and silhouettes.
   Extend the same registry/unlock rules as each remaining weapon is imported.
3. Implemented for rifle / shotgun / Machine Gun: independent reserves, normalized pickups,
   fractional credit, overflow and cabinet demand, migration and persistence.
   Extend allocation metadata when importing each remaining gun.
4. Machine Gun and slot-5 Chaingun completed. Import and adapt each remaining gun, including expansion Super Shotgun.
5. Complete Doom-only campaign interactions and native launcher modes.
6. Validate old normal saves, legacy prototype saves, new saves with an added
   weapon equipped, mode changes both ways, death/spirit walking, and transitions
   between levels. Test toggles for ammunition duplication and loss.
7. Check every gun's first-person/world models, animations, sound, ammo, firing,
   reload, projectile collision, portals and intended alternate fire.

All automated playtests must be hidden, isolated and muted with
`+set s_volume_dB -60`, without taking desktop focus or changing user settings.
Keep the working launcher build in place until the replacement passes validation.
