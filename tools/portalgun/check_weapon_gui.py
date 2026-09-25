"""Validate weapon_gui.cfg against the privately supplied gui_create save."""
import pathlib
import sys

text = pathlib.Path(sys.argv[1]).read_text(errors="replace").replace("\n", "")
assert not any(error in text for error in ("ERROR:", "shutting down:", "Unknown command"))
assert "PORTALGUN_VIEW selected weaponobj_portalgun" in text
assert "WEAPONGROUP current=1 ideal=1 group=1 variant=1" in text
press = text.split("SCREEN_PRESS_BEGIN", 1)[1].split("SCREEN_PRESS_END", 1)[0]
assert "GUI_CLICK object_itemcabinet_6 command opencabinet" in press, "Cabinet did not open"
assert press.count("GUI_CLICK") >= 2, "Missing GUI press or release"
held = text.split("SCREEN_HOLD_BEGIN", 1)[1].split("SCREEN_HOLD_END", 1)[0]
assert "GUI_CLICK" in held, "Held-input check did not start at an active GUI"
assert "PORTALGUN_SHOT launch" not in press + held, "Screen click leaked into portal firing"
firing = text.split("FIRE_BEGIN", 1)[1].split("FIRE_END", 1)[0]
assert firing.count("PORTALGUN_SHOT launch blue") == 1
assert firing.count("PORTALGUN_SHOT launch orange") == 1
assert "WEAPONGROUP current=2 ideal=2 group=2 variant=0" in text.split("FIRE_END", 1)[1]
print("PASS: portal gun opens cabinet, delivers GUI press/release, suppresses held-click shots, fires both colors afterward, and switches weapons")
