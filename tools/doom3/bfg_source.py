"""Reads a Doom 3: BFG Edition installation as if it were the original Doom 3.

The BFG Edition ships the original game's art, compiled for its own engine:
models and animations as .bmd5mesh/.bmd5anim/.blwo, textures as .bimage,
sounds as .idwav and fonts in a new atlas. This module presents those files
under their original names and formats (.md5mesh, .md5anim, .lwo, .tga, .wav,
classic font pages), converting each one on first read, so the Doom 3
importers work unchanged. Text declarations are used as shipped.

Formats follow the Doom 3 BFG Edition GPL source (renderer/Model.cpp,
Model_md5.cpp, BinaryImage.cpp, Font.cpp, d3xp/anim/Anim.cpp and
sound/XAudio2/XA2_SoundSample.cpp). Nothing converted here is distributable.
"""
from pathlib import Path
import io
import math
import re
import struct

import numpy as np
from PIL import Image


# ---------------------------------------------------------------------------
# .resources archives

def resources_toc(path):
    with open(path, 'rb') as f:
        magic, offset, _ = struct.unpack('>III', f.read(12))
        if magic != 0xD000000D:
            raise ValueError(f'{path}: not a BFG resources archive')
        f.seek(offset)
        count = struct.unpack('>I', f.read(4))[0]
        entries = []
        for _ in range(count):
            length = struct.unpack('<I', f.read(4))[0]
            name = f.read(length).decode('latin1').replace(chr(92), '/').lower()
            start, size = struct.unpack('>II', f.read(8))
            entries.append((name, start, size))
        return entries


def is_bfg_install(folder):
    return (Path(folder)/'base'/'_common.resources').is_file()


class Reader:
    """Little/big endian cursor over bytes (BFG mixes both)."""
    def __init__(self, data):
        self.data, self.pos = data, 0

    def take(self, fmt):
        values = struct.unpack_from(fmt, self.data, self.pos)
        self.pos += struct.calcsize(fmt)
        return values if len(values) > 1 else values[0]

    def big(self, fmt): return self.take('>'+fmt)
    def little(self, fmt): return self.take('<'+fmt)

    def string(self):
        length = self.little('i')
        text = self.data[self.pos:self.pos+length].decode('latin1')
        self.pos += length
        return text

    def raw(self, size):
        chunk = self.data[self.pos:self.pos+size]
        self.pos += size
        return chunk


# ---------------------------------------------------------------------------
# Math helpers (id conventions: joint matrices are 3x4, column-vector)

def quat_from_matrix(m):
    """Unit quaternion (x, y, z, w) for a rotation matrix m[row][col]."""
    trace = m[0][0] + m[1][1] + m[2][2]
    if trace > 0:
        s = math.sqrt(trace + 1.0) * 2
        w, x, y, z = 0.25*s, (m[2][1]-m[1][2])/s, (m[0][2]-m[2][0])/s, (m[1][0]-m[0][1])/s
    elif m[0][0] > m[1][1] and m[0][0] > m[2][2]:
        s = math.sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]) * 2
        w, x, y, z = (m[2][1]-m[1][2])/s, 0.25*s, (m[0][1]+m[1][0])/s, (m[0][2]+m[2][0])/s
    elif m[1][1] > m[2][2]:
        s = math.sqrt(1.0 + m[1][1] - m[0][0] - m[2][2]) * 2
        w, x, y, z = (m[0][2]-m[2][0])/s, (m[0][1]+m[1][0])/s, 0.25*s, (m[1][2]+m[2][1])/s
    else:
        s = math.sqrt(1.0 + m[2][2] - m[0][0] - m[1][1]) * 2
        w, x, y, z = (m[1][0]-m[0][1])/s, (m[0][2]+m[2][0])/s, (m[1][2]+m[2][1])/s, 0.25*s
    return (x, y, z, w)


def canonical(q):
    """Doom 3 text formats store x, y, z and rebuild a non-negative w."""
    return tuple(-c for c in q) if q[3] < 0 else tuple(q)


def num(value):
    text = f'{value:.10f}'.rstrip('0').rstrip('.')
    return '0' if text in ('-0', '') else text


# ---------------------------------------------------------------------------
# Models

BRM_MAGIC = (ord('B') << 24) | (ord('R') << 16) | (ord('M') << 8) | 108
MD5B_MAGIC = (ord('5') << 24) | (ord('D') << 16) | (ord('M') << 8) | 106
ANIM_MAGIC = (ord('B') << 24) | (ord('M') << 16) | (ord('D') << 8) | 101


def read_static_model(r):
    """idRenderModelStatic::LoadBinaryModel. Returns [(material, verts, indexes)]."""
    if r.big('I') != BRM_MAGIC:
        raise ValueError('Not a BFG render model')
    r.big('q')
    surfaces = []
    for _ in range(r.big('i')):
        r.big('i')
        material = r.string()
        if not r.big('?'):
            surfaces.append((material, [], []))
            continue
        r.little('6f'); r.big('i'); r.big('?'); r.big('?'); r.big('?'); r.big('?')
        num_verts = r.big('i')
        verts = []
        if r.big('i') > 0:
            for _ in range(num_verts):
                xyz = r.little('3f')
                st = np.frombuffer(r.raw(4), '>f2').astype(float)
                r.raw(16)
                verts.append((xyz, tuple(st)))
        shadow = r.big('i')
        r.raw(16*shadow)
        num_indexes = r.big('i')
        indexes = list(struct.unpack(f'>{num_indexes}H', r.raw(2*num_indexes))) if num_indexes else []
        if r.big('i') > 0:
            r.raw(2*num_indexes)
        r.raw(4*r.big('i'))		# mirrored verts
        r.raw(8*r.big('i'))		# dup verts
        r.raw(8*r.big('i'))		# sil edges (4 x 16-bit)
        if r.big('?'):
            r.raw(16*num_verts)
        r.big('3i')
        surfaces.append((material, verts, indexes))
    r.little('6f')
    r.big('i'); r.big('i'); r.big('i'); r.string(); r.raw(9)
    return surfaces


def bmd5mesh_to_md5mesh(data, source=''):
    r = Reader(data)
    read_static_model(r)
    if r.big('I') != MD5B_MAGIC:
        raise ValueError('Not a BFG MD5 model')
    joints = []
    for _ in range(r.big('i')):
        name = r.string()
        joints.append((name, r.big('i')))
    for _ in range(r.big('i')):
        r.big('4f'); r.little('3f')
    inverted = [struct.unpack('>12f', r.raw(48)) for _ in range(r.big('i'))]
    poses = []
    for m in inverted:
        # inverse of [R|t] is [R^T | -R^T t]
        ri = [[m[0], m[1], m[2]], [m[4], m[5], m[6]], [m[8], m[9], m[10]]]
        ti = (m[3], m[7], m[11])
        rot = [[ri[c][row] for c in range(3)] for row in range(3)]
        pos = tuple(-sum(rot[row][k]*ti[k] for k in range(3)) for row in range(3))
        poses.append((rot, pos))
    meshes = []
    for _ in range(r.big('i')):
        material = r.string()
        r.big('i'); r.big('i')
        r.raw(r.big('i'))		# mesh joints
        r.big('f')
        source_verts, output_verts, num_indexes, mirrored, dups, sil = r.big('6i')
        verts = []
        for _ in range(output_verts):
            xyz = r.big('3f')
            st = tuple(np.frombuffer(r.raw(4), '>f2').astype(float))
            r.raw(8)
            color, color2 = r.raw(4), r.raw(4)
            verts.append((xyz, st, color, color2))
        indexes = struct.unpack(f'>{num_indexes}H', r.raw(2*num_indexes))
        r.raw(2*num_indexes)
        r.raw(4*mirrored); r.raw(8*dups); r.raw(8*sil)
        r.big('i')
        meshes.append((material, verts, indexes))

    out = ['MD5Version 10', f'commandline "converted from Doom 3 BFG Edition {source}"', '',
           f'numJoints {len(joints)}', f'numMeshes {len(meshes)}', '', 'joints {']
    for (name, parent), (rot, pos) in zip(joints, poses):
        # Text joints are model space: pos, and the orientation whose matrix
        # (id row-vector convention) is the transpose of the joint rotation.
        q = canonical(quat_from_matrix([[rot[c][row] for c in range(3)] for row in range(3)]))
        out.append(f'\t"{name}"\t{parent} ( {num(pos[0])} {num(pos[1])} {num(pos[2])} ) '
                   f'( {num(q[0])} {num(q[1])} {num(q[2])} )')
    out.append('}')
    for material, verts, indexes in meshes:
        weights, lines = [], []
        for i, (xyz, st, color, color2) in enumerate(verts):
            first = len(weights)
            used = [(color[k], color2[k]) for k in range(4) if color2[k]]
            total = sum(w for _, w in used)
            for joint, w in used:
                rot, pos = poses[joint]
                d = [xyz[k]-pos[k] for k in range(3)]
                offset = [sum(rot[k][row]*d[k] for k in range(3)) for row in range(3)]
                weights.append((joint, w/total, offset))
            lines.append(f'\tvert {i} ( {num(st[0])} {num(st[1])} ) {first} {len(used)}')
        out += ['', 'mesh {', f'\tshader "{material}"', '', f'\tnumverts {len(verts)}'] + lines
        tris = len(indexes)//3
        out += ['', f'\tnumtris {tris}']
        out += [f'\ttri {t} {indexes[3*t]} {indexes[3*t+1]} {indexes[3*t+2]}' for t in range(tris)]
        out += ['', f'\tnumweights {len(weights)}']
        out += [f'\tweight {i} {j} {num(w)} ( {num(o[0])} {num(o[1])} {num(o[2])} )'
                for i, (j, w, o) in enumerate(weights)]
        out.append('}')
    return ('\n'.join(out)+'\n').encode()


def bmd5anim_to_md5anim(data, source=''):
    r = Reader(data)
    if r.big('I') != ANIM_MAGIC:
        raise ValueError('Not a BFG MD5 animation')
    r.big('q')
    frames, rate, _, num_joints, components = r.big('5i')
    bounds = [r.big('6f') for _ in range(r.big('i'))]
    info = []
    for _ in range(r.big('i')):
        name = r.string()
        info.append((name, *r.big('3i')))
    base = [(r.big('4f'), r.little('3f')) for _ in range(r.big('i'))]
    count = r.big('i')
    values = struct.unpack(f'<{count}f', r.raw(4*count))
    out = ['MD5Version 10', f'commandline "converted from Doom 3 BFG Edition {source}"', '',
           f'numFrames {frames}', f'numJoints {num_joints}', f'frameRate {rate}',
           f'numAnimatedComponents {components}', '', 'hierarchy {']
    out += [f'\t"{name}"\t{parent} {bits} {first}' for name, parent, bits, first in info]
    out += ['}', '', 'bounds {']
    out += [f'\t( {num(b[0])} {num(b[1])} {num(b[2])} ) ( {num(b[3])} {num(b[4])} {num(b[5])} )' for b in bounds]
    out += ['}', '', 'baseframe {']
    for q, t in base:
        q = canonical(q)
        out.append(f'\t( {num(t[0])} {num(t[1])} {num(t[2])} ) ( {num(q[0])} {num(q[1])} {num(q[2])} )')
    out.append('}')
    for f in range(frames):
        row = values[f*components:(f+1)*components]
        out += ['', f'frame {f} {{', '\t'+' '.join(num(v) for v in row) if row else '', '}']
    return ('\n'.join(out)+'\n').encode()


def blwo_to_lwo(data):
    """Static model -> minimal LightWave object (points, triangles, UVs, surfaces)."""
    surfaces = [s for s in read_static_model(Reader(data)) if s[1]]
    def chunk(tag, body):
        return tag + struct.pack('>I', len(body)) + body + (b'\0' if len(body) % 2 else b'')
    def sub(tag, body):
        return tag + struct.pack('>H', len(body)) + body
    def name(text):
        b = text.encode('latin1') + b'\0'
        return b + (b'\0' if len(b) % 2 else b'')
    def vx(i):
        return struct.pack('>H', i) if i < 0xFF00 else struct.pack('>I', 0xFF000000 | i)
    points, pols, ptag, uvs = [], [], [], []
    for s, (material, verts, indexes) in enumerate(surfaces):
        base = len(points)
        for xyz, st in verts:
            uvs.append(vx(len(points)) + struct.pack('>2f', st[0], 1.0-st[1]))
            points.append(struct.pack('>3f', xyz[0], xyz[2], xyz[1]))	# LightWave is Y-up
        for t in range(len(indexes)//3):
            a, b, c = (base+indexes[3*t+k] for k in range(3))
            ptag.append(vx(len(pols)) + struct.pack('>H', s))
            pols.append(struct.pack('>H', 3) + vx(a) + vx(c) + vx(b))
    body = b'LWO2'
    body += chunk(b'TAGS', b''.join(name(m) for m, _, _ in surfaces))
    body += chunk(b'LAYR', struct.pack('>HH3f', 0, 0, 0, 0, 0) + name(''))
    body += chunk(b'PNTS', b''.join(points))
    body += chunk(b'VMAP', b'TXUV' + struct.pack('>H', 2) + name('txuv') + b''.join(uvs))
    body += chunk(b'POLS', b'FACE' + b''.join(pols))
    body += chunk(b'PTAG', b'SURF' + b''.join(ptag))
    for m, _, _ in surfaces:
        body += chunk(b'SURF', name(m) + name('') + sub(b'COLR', struct.pack('>3fH', 1, 1, 1, 0)))
    return b'FORM' + struct.pack('>I', len(body)) + body


# ---------------------------------------------------------------------------
# Images

FMT_RGBA8, FMT_XRGB8, FMT_ALPHA, FMT_L8A8, FMT_LUM8, FMT_INT8, FMT_DXT1, FMT_DXT5 = 1, 2, 3, 4, 5, 6, 7, 8
CFM_DEFAULT, CFM_NORMAL_DXT5, CFM_YCOCG_DXT5, CFM_GREEN_ALPHA = 0, 1, 2, 3


def _dxt(data, width, height, fourcc):
    bw, bh = max(1, (width+3)//4), max(1, (height+3)//4)
    size = bw*bh*(8 if fourcc == b'DXT1' else 16)
    header = struct.pack('<7I', 124, 0x81007, bh*4, bw*4, size, 0, 1) + bytes(44)
    header += struct.pack('<II4s5I', 32, 4, fourcc, 0, 0, 0, 0, 0) + struct.pack('<5I', 0x1000, 0, 0, 0, 0)
    image = Image.open(io.BytesIO(b'DDS ' + header + data[:size])).convert('RGBA')
    return np.array(image.crop((0, 0, width, height)))


def bimage_decode(data):
    """Largest mip level as an RGBA array, in the original (source) colors."""
    ts, magic, kind, fmt, color, width, height, levels = struct.unpack_from('>qiiiiiii', data, 0)
    if magic & 0xFFFFFF != 0x4D4942:	# 'BIM'
        raise ValueError('Not a BFG image')
    level, _, w, h, size = struct.unpack_from('>5i', data, 36)
    pixels = data[56:56+size]
    if fmt == FMT_DXT1:
        rgba = _dxt(pixels, w, h, b'DXT1')
    elif fmt == FMT_DXT5:
        rgba = _dxt(pixels, w, h, b'DXT5')
    elif fmt in (FMT_RGBA8, FMT_XRGB8):
        rgba = np.frombuffer(pixels, np.uint8)[:w*h*4].reshape(h, w, 4).copy()
    elif fmt == FMT_L8A8:
        la = np.frombuffer(pixels, np.uint8)[:w*h*2].reshape(h, w, 2)
        rgba = np.dstack([la[..., 0]]*3 + [la[..., 1]])
    elif fmt in (FMT_ALPHA, FMT_LUM8, FMT_INT8):
        v = np.frombuffer(pixels, np.uint8)[:w*h].reshape(h, w)
        full = np.full_like(v, 255)
        rgba = np.dstack([full]*3 + [v]) if fmt == FMT_ALPHA else np.dstack([v]*3 + [v if fmt == FMT_INT8 else full])
    else:
        raise ValueError(f'Unsupported BFG image format {fmt}')
    rgba = rgba.astype(np.float32)
    if color == CFM_YCOCG_DXT5:
        # idColorSpace::ConvertCoCgSYToRGB; the scale factor is in blue.
        scale = 1.0 / (1.0 + rgba[..., 2]*(31.875/255.0))
        co, cg, y = (rgba[..., 0]-128)*scale, (rgba[..., 1]-128)*scale, rgba[..., 3]
        rgba = np.dstack([y+co-cg, y+cg, y-co-cg, np.full_like(y, 255)])
    elif color == CFM_NORMAL_DXT5:
        x, y = rgba[..., 3]/127.5-1, rgba[..., 1]/127.5-1
        z = np.sqrt(np.clip(1-x*x-y*y, 0, 1))
        rgba = np.dstack([(x+1)*127.5, (y+1)*127.5, (z+1)*127.5, np.full_like(x, 255)])
    elif color == CFM_GREEN_ALPHA:
        rgba = np.dstack([np.full_like(rgba[..., 1], 255)]*3 + [rgba[..., 1]])
    return np.clip(np.rint(rgba), 0, 255).astype(np.uint8)


def tga_bytes(rgba):
    out = io.BytesIO()
    Image.fromarray(rgba, 'RGBA').save(out, 'TGA')
    return out.getvalue()


# ---------------------------------------------------------------------------
# Sounds

def _msadpcm(block, channels, coefs, samples_per_block):
    adapt = [230, 230, 230, 230, 307, 409, 512, 614, 768, 614, 512, 409, 307, 230, 230, 230]
    r = Reader(block)
    pred = [r.little('B') for _ in range(channels)]
    delta = [r.little('h') for _ in range(channels)]
    s1 = [r.little('h') for _ in range(channels)]
    s2 = [r.little('h') for _ in range(channels)]
    out = [[s2[c], s1[c]] for c in range(channels)]
    nibbles = []
    for byte in block[r.pos:]:
        nibbles += [byte >> 4, byte & 15]
    for i, n in enumerate(nibbles[:(samples_per_block-2)*channels]):
        c = i % channels
        c1, c2 = coefs[min(pred[c], len(coefs)-1)]
        signed = n - 16 if n & 8 else n
        value = (s1[c]*c1 + s2[c]*c2) // 256 + signed*delta[c]
        value = max(-32768, min(32767, value))
        out[c].append(value)
        s2[c], s1[c] = s1[c], value
        delta[c] = max(16, adapt[n]*delta[c] // 256)
    return out


def idwav_to_wav(data):
    r = Reader(data)
    r.big('I'); r.big('q'); r.big('?'); r.big('i'); r.big('i')
    tag, channels, rate, _, block_align, bits = r.little('HHIIHH')
    coefs, per_block = [], 0
    if tag == 2:
        r.little('H')
        per_block, count = r.little('HH')
        coefs = [r.little('hh') for _ in range(7)][:count]
    elif tag != 1:
        raise ValueError(f'Unsupported BFG sound format {tag}')
    r.raw(r.big('i'))				# amplitude table
    r.big('i')
    payload = b''
    for _ in range(r.big('i')):
        _, size = r.big('2i')
        payload += r.raw(size)
    if tag == 1:
        pcm = payload
    else:
        chans = [[] for _ in range(channels)]
        for start in range(0, len(payload), block_align):
            block = payload[start:start+block_align]
            if len(block) < 7*channels:
                break
            for c, samples in enumerate(_msadpcm(block, channels, coefs, per_block)):
                chans[c] += samples
        pcm = np.array(chans, np.int16).T.astype('<i2').tobytes()
        bits = 16
    # BFG nudged rates to fit whole ADPCM blocks (e.g. 22066 Hz); the Doom 3
    # engine accepts only 11025, 22050 and 44100 Hz. Resample to the nearest.
    standard = min((11025, 22050, 44100), key=lambda s: abs(s-rate))
    if standard != rate:
        samples = np.frombuffer(pcm, '<i2' if bits == 16 else np.uint8).astype(np.float64)
        if bits == 8:
            samples = (samples-128)*256
        samples = samples.reshape(-1, channels)
        count = max(1, round(len(samples)*standard/rate))
        source = np.arange(len(samples))*(1.0/rate)
        target = np.arange(count)*(1.0/standard)
        pcm = np.stack([np.interp(target, source, samples[:, c]) for c in range(channels)], 1)
        pcm = np.clip(np.rint(pcm), -32768, 32767).astype('<i2').tobytes()
        rate, bits = standard, 16
    fmt = struct.pack('<HHIIHH', 1, channels, rate, rate*channels*bits//8, channels*bits//8, bits)
    body = b'WAVE' + b'fmt ' + struct.pack('<I', 16) + fmt + b'data' + struct.pack('<I', len(pcm)) + pcm
    return b'RIFF' + struct.pack('<I', len(body)) + body


# ---------------------------------------------------------------------------
# Fonts: classic Doom 3 fonts are 256x256 glyph pages at 12, 24 and 48 points.
# BFG keeps the classic glyph metrics (old_<size>.dat, identical to the
# original fontimage_<size>.dat) and draws from one 48-point atlas.

def classic_font_pages(view, font, size):
    old = view.raw(f'newfonts/{font}/old_{size}.dat')
    new = Reader(view.raw(f'newfonts/{font}/48.dat'))
    new.big('I'); new.big('h'); new.big('h'); new.big('h')
    count = new.big('h')
    glyphs = [struct.unpack('<BBbbBxHH', new.raw(10)) for _ in range(count)]
    chars = struct.unpack(f'<{count}I', new.raw(4*count))
    by_char = dict(zip(chars, glyphs))
    atlas_name = next(n for n in view.entries if n.startswith(f'generated/images/newfonts/{font}/48#'))
    atlas = bimage_decode(view.raw(atlas_name))
    alpha = atlas[..., 3]
    k = size/48.0
    pages = {}
    for c in range(256):
        height, top, bottom, pitch, xskip, width, iheight, s, t, s2, t2, _, material = struct.unpack_from('<7i4fi32s', old, c*80)
        material = material.split(b'\0')[0].decode('latin1')
        if not material or width <= 0 or iheight <= 0 or c not in by_char:
            continue
        page = pages.setdefault(material, np.zeros((256, 256), np.float32))
        gw, gh, gtop, _, _, gs, gt = by_char[c]
        if gw == 0 or gh == 0:
            continue
        glyph = Image.fromarray(alpha[gt:gt+gh, gs:gs+gw])
        sw, sh = max(1, round(gw*k)), max(1, round(gh*k))
        glyph = np.asarray(glyph.resize((sw, sh), Image.LANCZOS), np.float32)
        # Classic pages hold tightly cropped glyphs (no side bearing): center
        # horizontally in the original box and keep the original baseline.
        x0, y0 = int(round(s*256)) + max(0, (width-sw)//2), int(round(t*256)) + max(0, top - round(gtop*k))
        x1, y1 = min(x0+sw, int(round(s*256))+width, 256), min(y0+sh, int(round(t*256))+iheight, 256)
        if x1 > x0 and y1 > y0:
            page[y0:y1, x0:x1] = np.maximum(page[y0:y1, x0:x1], glyph[:y1-y0, :x1-x0])
    out = {}
    for material, page in pages.items():
        a = np.clip(np.rint(page), 0, 255).astype(np.uint8)
        out[material] = np.dstack([np.full_like(a, 255)]*3 + [a])
    return out


# ---------------------------------------------------------------------------
# Classic view

class ClassicView:
    """Index compatible with the Doom 3 importers: {name: (self, name)}; read converts."""

    def __init__(self, install):
        self.install = Path(install)
        base = self.install/'base'
        archives = sorted(base.rglob('*.resources'), key=lambda p: (p.name != '_common.resources', p.name != '_ordered.resources', str(p).lower()))
        self.entries = {}
        for archive in archives:
            for name, start, size in resources_toc(archive):
                self.entries.setdefault(name, (archive, start, size))
        self.loose = {p.relative_to(base).as_posix().lower(): p for p in base.rglob('*')
                      if p.is_file() and p.suffix.lower() not in ('.resources', '.pat', '.cfg', '.dll', '.exe')}
        self.names = {}		# classic name -> (kind, source)
        for name in self.entries:
            if not name.startswith('generated/'):
                self.names.setdefault(name, ('raw', name))
        for name in self.loose:
            self.names.setdefault(name, ('loose', name))
        for name in self.entries:
            m = re.fullmatch(r'generated/rendermodels/(.+)\.bmd5mesh', name)
            if m: self.names.setdefault(m[1]+'.md5mesh', ('md5mesh', name))
            m = re.fullmatch(r'generated/rendermodels/(.+)\.blwo', name)
            if m: self.names.setdefault(m[1]+'.lwo', ('lwo', name))
            m = re.fullmatch(r'generated/anim/(.+)\.bmd5anim', name)
            if m: self.names.setdefault(m[1]+'.md5anim', ('md5anim', name))
            m = re.fullmatch(r'generated/sound/(.+)\.idwav', name)
            if m:
                self.names.setdefault('sound/'+m[1]+'.wav', ('wav', name))
                self.names.setdefault('sound/'+m[1]+'.ogg', ('wav', name))
        # Images: plain ones first, then parts of combined image programs
        # (BFG bakes e.g. addnormals(local, heightmap(h, 5)) into one image).
        images = sorted(n for n in self.entries if n.startswith('generated/images/') and n.endswith('.bimage'))
        for name in images:
            path = name[len('generated/images/'):name.rindex('#')]
            if not path.startswith(('addnormals/', 'heightmap/')):
                self.names.setdefault(path+'.tga', ('image', name))
        for name in images:
            path = name[len('generated/images/'):name.rindex('#')]
            m = re.fullmatch(r'addnormals/(.+?)/heightmap/(.+)/[\d.]+', path)
            if m:
                self.names.setdefault(m[1]+'.tga', ('image', name))
                self.names.setdefault(m[2]+'.tga', ('flat', name))
            m = re.fullmatch(r'heightmap/(.+)/[\d.]+', path)
            if m:
                self.names.setdefault(m[1]+'.tga', ('image', name))
            # makealpha(x): alpha from x's brightness; makeintensity(x): all
            # channels from x's red. Rebuild x as the grey source image.
            m = re.fullmatch(r'makealpha/(.+)', path)
            if m:
                self.names.setdefault(m[1]+'.tga', ('fromalpha', name))
            m = re.fullmatch(r'makeintensity/(.+)', path)
            if m:
                self.names.setdefault(m[1]+'.tga', ('image', name))
        # BFG dropped images that materials name only as editor previews.
        for name in [n for n in self.entries if n.startswith('materials/') and n.endswith('.mtr')]:
            for image in re.findall(r'(?im)^\s*qer_editorimage\s+"?([^\s"]+)', self.raw(name).decode('latin1')):
                image = image.replace(chr(92), '/').lower()
                image = image if image.endswith('.tga') else image+'.tga'
                self.names.setdefault(image, ('editor', image))
        for font in {n.split('/')[1] for n in self.entries if n.startswith('newfonts/')}:
            if font != 'bankgothic_md_bt':
                continue
            for size in (12, 24, 48):
                self.names[f'fonts/english/bank/fontimage_{size}.dat'] = ('raw', f'newfonts/{font}/old_{size}.dat')
                for material in {struct.unpack_from('<32s', self.raw(f'newfonts/{font}/old_{size}.dat'), c*80+48)[0].split(b'\0')[0].decode('latin1')
                                 for c in range(256)} - {''}:
                    page = material.rsplit('/', 1)[-1].lower()
                    self.names[f'fonts/english/bank/{page}'] = ('font', (font, size, material))
        self.cache = {}

    def raw(self, name):
        if name in self.loose and name not in self.entries:
            return self.loose[name].read_bytes()
        archive, start, size = self.entries[name]
        with open(archive, 'rb') as f:
            f.seek(start)
            return f.read(size)

    def index(self):
        return {name: (self, name) for name in self.names}

    def read(self, name):
        name = name.lower()
        if name in self.cache:
            return self.cache[name]
        kind, source = self.names[name]
        if kind in ('raw', 'loose'):
            data = self.raw(source)
        elif kind == 'md5mesh':
            data = bmd5mesh_to_md5mesh(self.raw(source), source)
        elif kind == 'md5anim':
            data = bmd5anim_to_md5anim(self.raw(source), source)
        elif kind == 'lwo':
            data = blwo_to_lwo(self.raw(source))
        elif kind == 'wav':
            data = idwav_to_wav(self.raw(source))
        elif kind == 'image':
            data = tga_bytes(bimage_decode(self.raw(source)))
        elif kind == 'flat':
            # The height detail is already baked into the combined normal map.
            w, h = struct.unpack_from('>ii', self.raw(source), 20)
            data = tga_bytes(np.full((h, w, 4), (128, 128, 128, 255), np.uint8))
        elif kind == 'fromalpha':
            a = bimage_decode(self.raw(source))[..., 3]
            data = tga_bytes(np.dstack([a, a, a, np.full_like(a, 255)]))
        elif kind == 'editor':
            data = tga_bytes(np.full((8, 8, 4), (128, 128, 128, 255), np.uint8))
        elif kind == 'font':
            font, size, material = source
            data = tga_bytes(classic_font_pages(self, font, size)[material])
        else:
            raise KeyError(name)
        if kind != 'raw' or len(data) < 1 << 20:
            self.cache[name] = data
        return data
