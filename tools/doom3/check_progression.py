"""Check the hidden-run log produced by tests/progression.cfg."""
import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(errors="replace").replace("\r", "").replace("\n", "")
assert "ERROR:" not in text and "Unknown command" not in text
assert "Real Script checksum didn't match" not in text
addons = {8, 10, 11, 13, 14, 15}
expected = {
    "START": set(), "RIFLE": {8, 10}, "LEECH": {8, 10, 11},
    "OFF": set(), "RESTORED": {8, 10, 11}, "AUTO": {8, 10, 11, 15},
    "ACID": {8, 10, 11, 13, 15}, "ROCKET": addons,
}
for phase, held in expected.items():
    section = text.split("PROGRESSION_" + phase, 1)[1].split("AUTOSUPPLY", 1)[0]
    states = {int(s): int(h) for s, h in re.findall(r"WEAPONUNLOCK slot=(\d+) held=(\d+)", section)}
    assert {s for s in addons if states[s]} == held, (phase, states)
    if phase in ("RIFLE", "LEECH", "RESTORED"):
        assert states[5] == 0, "AutoCannon must not be granted by the addon"
assert "WEAPONGROUP current=10 ideal=10 group=4 variant=1" in text
assert "WEAPONGROUP current=11 ideal=11 group=5 variant=1" in text
assert text.count("WEAPONGROUP current=15 ideal=15 group=2 variant=2") >= 2
assert "AMMOCABINET name=before_cab" not in text
for slot in range(3):
    assert f"AMMOCABINET name=after_cab slot={slot} item=ammo_autocannon" in text
for marker, value in [("SUPPLY_FULL", "1.000"), ("SUPPLY_EMPTY", "0.000")]:
    section = text.split(marker, 1)[1].split("WEAPONGROUP", 1)[0]
    assert f"AUTOSUPPLY eligible=1 percent={value}" in section
section = text.split("PROGRESSION_OFF", 1)[1].split("PROGRESSION_RESTORED", 1)[0]
assert "AUTOSUPPLY eligible=0" in section
assert "CHAINAMMO auto=200 belt=30" in text, "AutoCannon pickup must replenish independent belt ammo"
section = text.split("SHARED_PICKUP", 1)[1]
assert "AMMOPOOL rifle=10 shells=12" in section
assert section.count("SHAREDSHELLS total=12 reserve=2 shotgun=8 super=2") >= 2
print("PASS: unlock milestones, early selection, purple slot-2 SSG, dynamic cabinet eligibility/demand, independent ammo, shared shells and save/toggle round trips")
