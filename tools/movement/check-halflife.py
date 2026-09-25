"""Check actual engine movement traces, rather than a duplicate physics model."""
import math
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])
pattern = re.compile(r"MOVEMENT (\d+) mode=(\d+) dt=(\d+) ground=(\d+) origin=(.*?)velocity=(.*?)gravity=(.*?)jump=(\d)")

def samples(text, phase):
    text = (root / f"movement{mode}_{phase}.log").read_text(errors="replace").replace("\n", "")
    part = text.split(phase + "_BEGIN", 1)[1].split(phase + "_END", 1)[0]
    result = []
    for t, sample_mode, dt, ground, pos, vel, gravity, jump in pattern.findall(part):
        result.append(dict(t=int(t), mode=int(sample_mode), dt=int(dt), ground=int(ground),
                           pos=tuple(map(float, pos.split())), vel=tuple(map(float, vel.split())),
                           gravity=tuple(map(float, gravity.split())), jump=int(jump)))
    assert result, f"Missing samples: {phase}"
    assert all(math.isfinite(v) for r in result for k in ("pos", "vel", "gravity") for v in r[k])
    return result

def lateral(row):
    return math.hypot(*row['vel'][:2])

for mode, height, gravity in [(0, 64, 1066), (3, 45, 800), (4, 160**2/1200, 600)]:
    # Console dumps wrap text at screen columns, sometimes inside numbers.
    text = (root / f"movement{mode}.log").read_text(errors="replace").replace("\n", "")
    assert not re.search(r"ERROR:|shutting down:|Unknown command", text)
    stationary = samples(text, "STATIONARY")
    assert sum(r['jump'] for r in stationary) == 1, "Manual mode must require jump release"
    rise = max(r['pos'][2] for r in stationary) - stationary[0]['pos'][2]
    assert abs(rise-height) < 0.3, (mode, "jump height", rise, height)
    assert all(abs(r['gravity'][2] + gravity) < 0.01 for r in stationary)
    automatic = samples(text, "AUTO")
    assert sum(r['jump'] for r in automatic) >= (4 if mode else 1)
    if not mode:
        assert sum(r['jump'] for r in automatic) == 1, "HL auto-hop changed original Prey"
    boost = samples(text, "BOOST")
    jumps = [i for i, r in enumerate(boost) if r['jump']]
    assert jumps and jumps[0] > 0
    gain = lateral(boost[jumps[0]]) - lateral(boost[jumps[0]-1])
    assert abs(gain - (95 if mode == 4 else 0)) < 0.1, (mode, "jump boost", gain)
    if mode == 4:
        assert len(jumps) >= 2 and lateral(boost[jumps[1]]) > lateral(boost[jumps[0]]) + 90
    sideways = samples(text, "SIDEWAYS")
    assert any(abs(r['gravity'][0]+gravity) < 0.01 and abs(r['gravity'][2]) < 0.01 for r in sideways)
    restore = samples(text, "RESTORE")
    assert all(r['mode'] == 0 and abs(r['gravity'][2]+1066) < 0.01 for r in restore)
    restored_rise = max(r['pos'][2] for r in restore) - min(r['pos'][2] for r in restore)
    assert abs(restored_rise-64) < 0.3, (mode, "restore", restored_rise)
    air = samples(text, "AIR")
    if mode:
        assert max(lateral(r) for r in air) > 400.5, "Air strafe did not gain speed"
        assert all(abs(r['vel'][0]-400) < 0.1 for r in air), "Air steering discarded momentum"
        assert max(abs(r['vel'][1]) for r in air) <= 30.1, "Air wish cap lost"
    friction = samples(text, "FRICTION")
    assert lateral(friction[-1]) < 1, "Ground friction did not stop the player"
    portal = text.split('PORTAL_BEGIN', 1)[1].split('PORTAL_END', 1)[0]
    assert 'PORTAL_EXIT ' in portal and 'blocked exit' not in portal, "Floor portal crossing failed"
    print(f"PASS mode {mode}: jump {rise:.2f}, boost {gain:.1f}, manual/auto-hop, air strafe, friction, gravity, save/reload, original restore, floor portal")
