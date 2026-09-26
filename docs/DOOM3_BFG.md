# Doom 3 BFG 9000

The optional Doom weapon pack adds the BFG as the third (purple) weapon in slot 7. Owning the native Rocket Launcher unlocks it, including when loading an existing save. Press 7 repeatedly or cycle weapons to select it. Hold primary fire to charge, release to fire, and use the normal reload binding. Holding past the full charge and grace period overloads the weapon.

Capacity is 12 cells total, including four loaded. Initial unlock grants four cells once. Each rare cell pickup adds one. Dynamic cabinets and automatic ammo spawners become eligible after BFG ownership; their BFG demand weight is multiplied by 0.08. This is a relative weight, not a fixed eight-percent probability. Full ammunition produces no BFG demand. Authored fixed cabinet contents and rocket pickup amounts stay unchanged.

## Combat behavior

The implementation follows the retail Doom 3 definitions and id Software's GPL Projectile.cpp, adapted to Prey's projectile and portal physics. Charge selects one through four cells and scales damage by cells consumed. Direct impact uses 200 damage, acquired visible targets receive five-damage tendril ticks every 333 ms, and the explosion applies a further 200-damage burst to visible acquired targets. Damageable impacts also produce a short-range 100-damage radius component, excluding the direct target. Thus a normal direct hit can receive 400 damage plus flight ticks, before target-specific damage scaling. These constituent values differ from the supplied video's simplified description.

Tendrils check line of sight; targets refresh after portal traversal. The projectile, second rotating shell, tendrils, charging animation, screen, sound assets, pickup model and explosion assets are imported from the user's Doom installation. Retail assets are generated locally and excluded from source control.

## Compatibility and validation

Existing fixed save arrays remain 16 entries; the additional ammo and clip values live in a tagged inventory dictionary record. Loading an older save initializes the extra entry safely. Level transitions preserve extra ammo, clip and the one-time grant flag. The feature is single-player only; multiplayer snapshot layouts retain the legacy count. Disabling the pack hides the weapon while retaining its saved ammo.

The muted hidden-desktop fixtures in tools/doom3/tests/bfg-combat.cfg and bfg-edges.cfg cover charged damage, obstruction, overcharge, ammo pickups, cabinet eligibility, portal traversal, save/load and pack toggles. They require the existing isolated portal laboratory input save. Additional campaign screenshots check the model and charging display. The existing progression fixture covers the other Doom weapons.

Reference: https://github.com/id-Software/DOOM-3/blob/master/neo/game/Projectile.cpp
