# Doom 3 BFG 9000

The optional Doom weapon pack adds the BFG as the third (purple) weapon in slot 7. Owning the native Rocket Launcher unlocks it, including when loading an existing save. Press 7 repeatedly or cycle weapons to select it. Hold primary fire to charge, release to fire, and use the normal reload binding. Holding primary fire automatically fires at full charge after two seconds. Release the button before charging the next shot; early release still fires a partial charge.

The BFG and Plasma Gun share one 150-unit plasma pool, including loaded ammunition. Each BFG charge costs 37.5 units: four ordinary shots or one four-charge shot consume exactly 150. Fractional credit is preserved across saves and level transitions. The BFG holds up to four charge levels; firing either gun reduces the common total and clamps the other's usable magazine to the remaining supply. Magazine indicators are not additional ammo pools.

Supply comes from the existing slot-6 acid/plasma pickups at their existing rates. BFG ownership also enables that supply in dynamic lockers if the Acid Sprayer is not owned. No new rare BFG cells are added to the random pool. Previously saved separate BFG ammunition converts into plasma once, capped at 150; already-spawned legacy cells remain usable. Unlocking the BFG does not refill the shared pool.

## Combat behavior

The implementation follows the retail Doom 3 definitions and id Software's GPL Projectile.cpp, adapted to Prey's projectile and portal physics. Charge selects one through four cells and scales damage by cells consumed. Direct impact uses 200 damage, acquired visible targets receive five-damage tendril ticks every 333 ms, and the explosion applies a further 200-damage burst to visible acquired targets. Damageable impacts also produce a short-range 100-damage radius component, excluding the direct target. Thus a normal direct hit can receive 400 damage plus flight ticks, before target-specific damage scaling. These constituent values differ from the supplied video's simplified description.

Tendrils check line of sight; targets refresh after portal traversal. The projectile, second rotating shell, tendrils, charging animation, screen, sound assets, pickup model and explosion assets are imported from the user's Doom installation. Retail assets are generated locally and excluded from source control.

## Compatibility and validation

Existing fixed save arrays remain 16 entries; the additional ammo and clip values live in a tagged inventory dictionary record. Loading an older save initializes the extra entry safely. Level transitions preserve extra ammo, clip and the one-time grant flag. The feature is single-player only; multiplayer snapshot layouts retain the legacy count. Disabling the pack hides the weapon while retaining its saved ammo.

The original muted hidden-desktop combat fixtures cover charged damage, obstruction, overcharge, portal traversal, save/load and pack toggles. Their independent-cell balance is superseded by bfg-shared.cfg and bfg-shared-migration.cfg, checked with check_bfg_shared.py. The migration fixture additionally needs the legacy bfg_ready save from the original BFG tests. They require the existing isolated portal laboratory input save. Additional campaign screenshots check the model and charging display. The existing progression fixture covers the other Doom weapons.

Reference: https://github.com/id-Software/DOOM-3/blob/master/neo/game/Projectile.cpp


## Separate loaded charges

BFG reload now transfers 37.5 plasma per charge out of ammo[13] into clip[16].
Firing consumes only loaded charges. The Plasma Gun cannot spend BFG-loaded
ammo; BFG loading also excludes plasma already in the Plasma Gun magazine.
The normal 150-round plasma pool can refill while up to four BFG charges remain
loaded. The Plasma Gun retains its existing total-including-magazine accounting.

Weapon reconstruction restores the cached BFG clip without charging reserve
again; fresh first equip still loads from reserve. A persistent rw_weapon_ flag
converts older shared-pool saves once, paying for their loaded charges from
available plasma and limiting overlapping claims to actual available ammo.
No fixed save layout changes. The marker and clip survive campaign transitions.

Hidden muted tests: full load 150 -> 0 with four charges; ammo pickup refills to
150 while retaining four charges; Plasma Gun firing/reloading retains the BFG
clip; BFG firing does not spend reserve twice; save/reload and level transition;
fully charged shot; older bfg_half save converts to three charges and zero
reserve, unchanged on reload; loading from 38 leaves one charge and 0.5 plasma,
unchanged after save/reload and another reload attempt.
