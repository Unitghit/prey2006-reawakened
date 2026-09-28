"""Compares Doom 3 files read from a BFG Edition installation (bfg_source) with
the original game's files. Local verification only.

python compare_bfg_classic.py CLASSIC_INSTALL BFG_INSTALL mesh|anim|image|sound NAME...
"""
import sys, re, zipfile, glob, math, io
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
import numpy as np
from PIL import Image
import bfg_source

CLASSIC, BFG = sys.argv[1], sys.argv[2]

classic = {}
for folder in ('base', 'd3xp'):
    for p in sorted(glob.glob(f'{CLASSIC}/{folder}/pak*.pk4')):
        z = zipfile.ZipFile(p)
        for n in z.namelist():
            classic[n.lower()] = (z, n)
def cread(n):
    z, o = classic[n.lower()]; return z.read(o)

view = bfg_source.ClassicView(BFG)

def quat_mat(x, y, z):
    w = math.sqrt(max(0, 1-x*x-y*y-z*z))
    return np.array([[1-2*(y*y+z*z), 2*(x*y-w*z), 2*(x*z+w*y)],
                     [2*(x*y+w*z), 1-2*(x*x+z*z), 2*(y*z-w*x)],
                     [2*(x*z-w*y), 2*(y*z+w*x), 1-2*(x*x+y*y)]])

def parse_mesh(text):
    text = re.sub(r'//[^\n]*', '', text)
    joints = [(float(a), float(b), float(c), float(d), float(e), float(f)) for a, b, c, d, e, f in
              re.findall(r'"[^"]*"\s+-?\d+\s+\(\s*(\S+)\s+(\S+)\s+(\S+)\s*\)\s*\(\s*(\S+)\s+(\S+)\s+(\S+)\s*\)', re.search(r'joints\s*\{(.*?)\}', text, re.S)[1])]
    names = re.findall(r'"([^"]*)"\s+-?\d+\s+\(', re.search(r'joints\s*\{(.*?)\}', text, re.S)[1])
    meshes = []
    for body in re.findall(r'mesh\s*\{(.*?)\n\}', text, re.S):
        shader = re.search(r'shader\s+"([^"]*)"', body)[1]
        verts = [(float(s), float(t), int(a), int(b)) for s, t, a, b in re.findall(r'vert\s+\d+\s+\(\s*(\S+)\s+(\S+)\s*\)\s+(\d+)\s+(\d+)', body)]
        tris = [tuple(map(int, t)) for t in re.findall(r'tri\s+\d+\s+(\d+)\s+(\d+)\s+(\d+)', body)]
        weights = [(int(j), float(w), np.array([float(x), float(y), float(z)])) for j, w, x, y, z in
                   re.findall(r'weight\s+\d+\s+(\d+)\s+(\S+)\s+\(\s*(\S+)\s+(\S+)\s+(\S+)\s*\)', body)]
        pos = []
        for s, t, a, b in verts:
            p = np.zeros(3)
            for j, w, o in weights[a:a+b]:
                jx, jy, jz, qx, qy, qz = joints[j]
                # classic loader: joint mat = transpose(q.ToMat3()); id ToMat3 is the standard matrix
                p += w*(quat_mat(qx, qy, qz).T @ o + np.array([jx, jy, jz]))
            pos.append(p)
        meshes.append((shader, np.array([(s, t) for s, t, _, _ in verts]), np.array(pos), tris))
    return names, joints, meshes

def compare_mesh(name):
    a = parse_mesh(cread(name).decode('latin1'))
    b = parse_mesh(view.read(name).decode('latin1'))
    report = [name]
    if a[0] != b[0]:
        report.append(f'  joint names differ: {len(a[0])} vs {len(b[0])}')
    jd = max(abs(x-y) for ja, jb in zip(a[1], b[1]) for x, y in zip(ja[:3], jb[:3]))
    qd = max(abs(x-y) for ja, jb in zip(a[1], b[1]) for x, y in zip(ja[3:], jb[3:]))
    report.append(f'  joints {len(a[1])}: max pos diff {jd:.5f}, max quat diff {qd:.5f}')
    for (sa, sta, pa, ta), (sb, stb, pb, tb) in zip(a[2], b[2]):
        # each classic vertex: nearest converted vertex with same uv
        worst = 0
        for p, st in zip(pa, sta):
            d = np.linalg.norm(pb-p, axis=1) + 100*np.abs(stb-st).sum(axis=1)
            worst = max(worst, d.min())
        # triangle orientation: compare face normals of first matching tris via positions
        def normals(P, T):
            return {tuple(np.round(sorted(map(tuple, np.round(P[list(t)], 2))), 2).ravel()): np.cross(P[t[1]]-P[t[0]], P[t[2]]-P[t[0]]) for t in T}
        na, nb = normals(pa, ta), normals(pb, tb)
        common = set(na) & set(nb)
        agree = sum(1 for k in common if np.dot(na[k], nb[k]) > 0)
        report.append(f'  mesh {sa} / {sb}: verts {len(pa)}->{len(pb)} tris {len(ta)}->{len(tb)} worst vertex {worst:.4f}; winding agrees {agree}/{len(common)}')
    return '\n'.join(report)

def parse_anim(text):
    frames = [list(map(float, f.split())) for f in re.findall(r'frame\s+\d+\s*\{([^}]*)\}', text)]
    base = [list(map(float, b)) for b in re.findall(r'\(\s*(\S+)\s+(\S+)\s+(\S+)\s*\)\s*\(\s*(\S+)\s+(\S+)\s+(\S+)\s*\)', re.search(r'baseframe\s*\{(.*?)\}', text, re.S)[1])]
    hier = re.findall(r'"([^"]*)"\s+(-?\d+)\s+(\d+)\s+(\d+)', re.search(r'hierarchy\s*\{(.*?)\}', text, re.S)[1])
    head = {k: re.search(k+r'\s+(\S+)', text)[1] for k in ('numFrames', 'numJoints', 'frameRate', 'numAnimatedComponents')}
    return head, hier, base, frames

def compare_anim(name):
    a, b = parse_anim(cread(name).decode('latin1')), parse_anim(view.read(name).decode('latin1'))
    fd = max((abs(x-y) for fa, fb in zip(a[3], b[3]) for x, y in zip(fa, fb)), default=0)
    bd = max((abs(x-y) for fa, fb in zip(a[2], b[2]) for x, y in zip(fa, fb)), default=0)
    return f'{name}: head {"same" if a[0]==b[0] else (a[0], b[0])}, hierarchy {"same" if a[1]==b[1] else "DIFF"}, base max diff {bd:.5f}, frames {len(a[3])}/{len(b[3])} max diff {fd:.5f}'

def compare_image(name):
    a = np.asarray(Image.open(io.BytesIO(cread(name))).convert('RGBA'), np.float32)
    b = np.asarray(Image.open(io.BytesIO(view.read(name))).convert('RGBA'), np.float32)
    kind = view.names[name.lower()]
    if a.shape != b.shape:
        b = np.asarray(Image.fromarray(b.astype(np.uint8)).resize((a.shape[1], a.shape[0]), Image.BILINEAR), np.float32)
        note = ' (resized)'
    else:
        note = ''
    err = np.abs(a-b).mean(axis=(0, 1))
    return f'{name} [{kind[0]} {str(kind[1]).split("/")[-1][:40]}]{note}: mean abs diff RGBA {np.round(err, 1)}'

def compare_sound(name):
    import wave
    def load(data):
        w = wave.open(io.BytesIO(data))
        d = np.frombuffer(w.readframes(w.getnframes()), '<i2' if w.getsampwidth() == 2 else np.uint8).astype(np.float32)
        if w.getsampwidth() == 1: d = (d-128)*256
        return w.getnchannels(), w.getframerate(), d
    try:
        a = load(cread(name))
    except Exception as e:
        return f'{name}: classic not PCM wav ({e})'
    b = load(view.read(name))
    n = min(len(a[2]), len(b[2]))
    corr = np.corrcoef(a[2][:n], b[2][:n])[0, 1] if n > 10 else 0
    return f'{name}: ch {a[0]}/{b[0]} rate {a[1]}/{b[1]} samples {len(a[2])}/{len(b[2])} correlation {corr:.4f}'

if __name__ == '__main__':
    kind = sys.argv[3]
    for n in (x.strip() for x in sys.argv[4:]):
        try:
            print({'mesh': compare_mesh, 'anim': compare_anim, 'image': compare_image, 'sound': compare_sound}[kind](n))
        except Exception as e:
            import traceback; traceback.print_exc()
