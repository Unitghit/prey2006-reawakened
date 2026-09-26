"""Check combat, edge-case, and level-transition BFG fixture logs."""
import sys
from pathlib import Path
combat, edges, transition = [Path(p).read_text(errors="replace") for p in sys.argv[1:]]
checks = {
    "single and four-cell charge": "BFG launch power=1 targets=2" in combat and "BFG launch power=4 targets=2" in combat,
    "scaled direct and burst damage": "BFGTARGET name=bfg_target health=9580" in combat and "BFGTARGET name=bfg_target health=7880" in combat,
    "wall blocks beams": combat.count("BFGTARGET name=bfg_hidden health=10000") >= 2,
    "save and pack toggle preserve ammo": "BFGSTATE held=0 ammo=7 clip=0" in combat and "BFGSTATE held=1 ammo=7 clip=0" in combat,
    "rare pickup and overcharge": "BFGSTATE held=1 ammo=9 clip=4" in edges and "BFGSTATE held=1 ammo=5 clip=0" in edges,
    "native portal traversal": "BFG portalled via=rw_gun_blue" in edges,
    "level transition and restore": "Map: game/spindleb" in transition and transition.count("BFGSTATE held=1 ammo=11 clip=3") >= 4,
    "no engine errors": all("ERROR:" not in s for s in (combat, edges, transition)),
}
for name, ok in checks.items():
    print(("PASS: " if ok else "FAIL: ") + name)
sys.exit(0 if all(checks.values()) else 1)
