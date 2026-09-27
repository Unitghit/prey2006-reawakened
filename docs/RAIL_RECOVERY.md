# Rail collision precision and shallow overlap recovery

The `railstuck` save is in `game/shuttleb`, around world coordinates (24462, 1305, 23472). World-space Pluecker line construction multiplies large coordinates and subtracts almost equal products. That loses precision on small nearby collision edges. Contents, contact and movement queries could disagree, repeatedly lifting the player or producing unstable contacts on the rail.

Collision contents and translation now construct both sets of Pluecker lines relative to the same trace start. This includes point rays, hull edges, model edges, moving model-vertex rays and epsilon-expanded edges. Stored geometry, planes, contacts, portal clipping and trace results remain in their existing coordinate spaces. Rotation traces are unchanged. No save or network layout changes are needed.

The previous ground-normal selection workaround is removed. Shallow overlap recovery still repairs an already embedded single-player hull, but removes only downward velocity, preserving lateral momentum. It requires a clear candidate within two units against gravity and a reverse sweep to walkable world geometry within 0.75 units. It does not permit escape through walls or deep overlaps. Spirit/deathwalk, vehicle and portal axis-transition paths remain excluded.

## Validation

Muted tests run on a private Windows desktop. The actual save is tested directly, including its existing accumulated sideways velocity. The movement and jumping fixtures additionally reset the player just above the rail with zero velocity, so they measure new contact behavior rather than the old saved impulse.

An A/B run uses the same revised game DLL and movement fixture with the old versus revised engine. Old world-space collision math repeatedly triggers recovery during movement (8 corrections in the positive-X section), changes the standing height, and interrupts acceleration. Revised math triggers zero recoveries in all four movement sections. Movement along the rail holds Z=23472.35 and reaches 180 units/second. Stepping off either side remains possible. Three jumps land at exactly Z=23472.35 with zero resting velocity each time.

The deep-floor and embedded-wall fixtures remain blocked, and the sloped portal exit regression reaches approximately (-235.67, -116.25, 0.25) at 180 units/second. Both executable and game DLL must be updated together. The retail executable was not compared; this does not establish whether retail has the same precision issue.
