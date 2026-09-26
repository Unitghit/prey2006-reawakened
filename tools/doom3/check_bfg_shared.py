"""Verify shared plasma accounting and migration logs from hidden playtests."""
from pathlib import Path
import sys
shared, migration = [Path(p).read_text(errors="replace") for p in sys.argv[1:]]
checks = {
 "four ordinary shots use 150": all(f"BFGSTATE held=1 ammo={a} clip={c}" in shared for a,c in [(112,3),(75,2),(37,1),(0,0)]),
 "fraction survives save/load": shared.count("BFGSTATE held=1 ammo=112 clip=3") >= 2,
 "fully charged shot": "BFG launch power=4" in shared and shared.count("BFGSTATE held=1 ammo=0 clip=0") >= 2,
 "plasma gun consumes common pool": "BFGSTATE held=1 ammo=148" in shared,
 "legacy ammo merges to capacity": "BFGSTATE held=1 ammo=150 clip=3" in migration,
 "slot 6 pickup supplies BFG": "BFGSTATE held=1 ammo=40" in migration,
 "level transition and save": "Map: game/spindleb" in migration and migration.count("BFGSTATE held=1 ammo=112 clip=3") >= 3,
 "no engine errors": all("ERROR:" not in s for s in (shared,migration)),
}
for name, ok in checks.items(): print(("PASS: " if ok else "FAIL: ")+name)
sys.exit(0 if all(checks.values()) else 1)
