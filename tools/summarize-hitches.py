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
    frames = []
    for line in text.splitlines():
        marker = re.search(r'HITCH_MAP wall=(\d+) phase=(\w+) map=(.*)', line)
        if marker:
            ready = int(marker[1]) if marker[2] == 'ready' else None
            operations.clear()
            continue
        op = re.search(r'HITCH_OP wall=(\d+) category=(\S+) ms=([\d.]+) asset=(.*)', line)
        if op and ready is not None:
            operations.append(dict(wall=int(op[1]), category=op[2], ms=float(op[3]), asset=op[4]))
            operations = operations[-512:]
        frame = re.search(r'HITCH_FRAME wall=(\d+) frame=(\d+) total_ms=([\d.]+) events_ms=([\d.]+) session_ms=([\d.]+) render_ms=([\d.]+) map=(.*)', line)
        if frame and ready is not None:
            end, total = int(frame[1]), float(frame[3])
            # Exclude map loading and the first second of settling after map-ready.
            if end - total < ready + 1000:
                continue
            overlapping = [op for op in operations if end-total-1 <= op['wall'] <= end+1]
            frames.append(dict(wall=end, frame=int(frame[2]), total_ms=total,
                               events_ms=float(frame[4]), session_ms=float(frame[5]),
                               render_ms=float(frame[6]), map=frame[7],
                               operations=sorted(overlapping, key=lambda op: op['ms'], reverse=True)[:8]))
    return dict(gameplay_hitches=len(frames), worst_ms=max((f['total_ms'] for f in frames), default=0),
                worst_frames=sorted(frames, key=lambda f: f['total_ms'], reverse=True)[:20])

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    print(json.dumps(summarize(args.log.read_text(encoding='utf-8', errors='replace')), indent=2))
