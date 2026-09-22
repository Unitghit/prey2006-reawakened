"""Summarize a profile's diagnostics/hitches.log; requires only Python 3.

CPU timings include nested work and presentation waits. They are not GPU timings.
Only slow frames are logged, so this cannot calculate average FPS or percentiles.
"""
import argparse
import json
import re
from pathlib import Path

def summarize(text):
    ready = None
    operations = []
    ui_events = []
    frames = []
    for line in text.splitlines():
        marker = re.search(r'HITCH_MAP wall=(\d+) phase=(\w+) map=(.*)', line)
        if marker:
            ready = int(marker[1]) if marker[2] == 'ready' else None
            operations.clear()
            ui_events.clear()
            continue
        ui = re.search(r'HITCH_UI wall=(\d+) state=(\w+)', line)
        if ui:
            ui_events.append((int(ui[1]), ui[2]))
            ui_events = ui_events[-128:]
        op = re.search(r'HITCH_OP wall=(\d+) category=(\S+) ms=([\d.]+) asset=(.*)', line)
        if op:
            operations.append(dict(wall=int(op[1]), category=op[2], ms=float(op[3]), asset=op[4]))
            operations = operations[-512:]
        frame = re.search(r'HITCH_FRAME wall=(\d+) frame=(\d+) total_ms=([\d.]+) events_ms=([\d.]+) session_ms=([\d.]+) render_ms=([\d.]+)(?: state=(\w+) start_state=(\w+))? map=(.*)', line)
        if not frame:
            continue
        end, total = int(frame[1]), float(frame[3])
        state, start_state = frame[7], frame[8]
        # Older logs did not record UI state: do not assert those were gameplay.
        classification = 'unclassified' if state is None else state
        if state and state != start_state:
            classification = 'transition'
        elif state == 'gameplay' and any(end-total-1 <= t <= end+1 and mode == 'menu' for t, mode in ui_events):
            classification = 'transition'
        if state == 'loading' or start_state == 'loading':
            continue
        if ready is not None:
            if end - total < ready + 1000:
                continue  # loading and the first second after map-ready
        elif classification not in ('menu', 'transition'):
            continue
        overlapping = [op for op in operations if end-total-1 <= op['wall'] <= end+1]
        frames.append(dict(wall=end, frame=int(frame[2]), total_ms=total,
                           classification=classification,
                           events_ms=float(frame[4]), session_ms=float(frame[5]),
                           render_ms=float(frame[6]), map=frame[9],
                           operations=sorted(overlapping, key=lambda op: op['ms'], reverse=True)[:8]))
    gameplay = [f for f in frames if f['classification'] == 'gameplay']
    return dict(gameplay_hitches=len(gameplay),
                menu_hitches=sum(f['classification'] == 'menu' for f in frames),
                transition_hitches=sum(f['classification'] == 'transition' for f in frames),
                unclassified_hitches=sum(f['classification'] == 'unclassified' for f in frames),
                worst_ms=max((f['total_ms'] for f in frames), default=0),
                worst_gameplay_ms=max((f['total_ms'] for f in gameplay), default=0),
                worst_frames=sorted(frames, key=lambda f: f['total_ms'], reverse=True)[:20])

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    print(json.dumps(summarize(args.log.read_text(encoding='utf-8', errors='replace')), indent=2))
