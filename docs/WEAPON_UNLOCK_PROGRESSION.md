# Doom weapon unlock progression

With the Doom weapon pack enabled, campaign ownership determines unlocks on pickup and when loading an existing save. Selection groups and ammo sources are independent of the unlock milestone.

| Campaign pickup | Additions unlocked | Selection group | Ammo source |
| --- | --- | --- | --- |
| Hunter Rifle | Shotgun; Machine Gun | 2; 4 | Rifle pickups, independent reserves |
| Leech Gun | Chaingun | 5 | AutoCannon bullets |
| AutoCannon | Super Shotgun | 2, third variant (purple) | Shared shotgun shells |
| Acid Sprayer | Plasma Gun | 6 | Acid pickups |
| Rocket Launcher | Doom Rocket Launcher; BFG 9000 | 7, second and third variants | Rockets; separate rare BFG cells |

Slot 2 cycles Rifle, Shotgun, Super Shotgun. Shared shells remain 16 in reserve plus loaded shells (8 and 2). Pickup rates and capacities are unchanged.

Dynamic cabinets and automatic ammo spawners accept chaingun ownership as eligibility for AutoCannon bullet pickups. Grenade pickups still require the actual AutoCannon. Before that native weapon is owned, bullet demand follows only the chaingun reserve; afterward it uses the existing combined demand. Random weighting, empty chances and map-authored fixed contents remain intact. Already spawned pickups are not rerolled. Disabling the pack removes addon-only eligibility without deleting ammunition.

The weapon HUD shows owned addons even before the native gun in their selection group. Grants, normal selection and forced spirit/vehicle restoration share one unlock mapping. Existing inventory indices and fixed legacy save arrays are unchanged. The BFG uses index 16 with a tagged inventory extension; old saves remain loadable.

Validation: run tools/doom3/tests/progression.cfg in an isolated portal laboratory profile with the input save, muted using s_volume_dB -60. Use a hidden desktop runner for complete window isolation. The fixture checks early grants, key selection, an actual dynamic cabinet before/after unlock, fullness weighting, independent pickup supply, shared shells, equipped save restoration and pack off/on restoration. Check its progression.log with tools/doom3/check_progression.py. Screenshots record early slots and purple Super Shotgun selection.
