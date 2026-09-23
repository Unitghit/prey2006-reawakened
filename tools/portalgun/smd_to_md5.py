"""Convert Crowbar SMD meshes/animations to MD5, preserving skeletal weights.

Requires numpy. Source files and generated files stay outside the source repo.
"""
import math
import re
from pathlib import Path
import numpy as np

# SMD viewmodel forward is -Y; Prey viewmodel forward is +X.
BASIS = np.array([[0., -1, 0], [1, 0, 0], [0, 0, 1]])


def rotation(angles):
    x, y, z = angles
    cx, cy, cz = map(math.cos, angles)
    sx, sy, sz = map(math.sin, angles)
    return np.array([[cz*cy, cz*sy*sx-sz*cx, cz*sy*cx+sz*sx],
                     [sz*cy, sz*sy*sx+cz*cx, sz*sy*cx-cz*sx],
                     [-sy, cy*sx, cy*cx]])


def quaternion(m):
    # Symmetric eigenproblem, stable for rotations near 180 degrees.
    xx, xy, xz = m[0]; yx, yy, yz = m[1]; zx, zy, zz = m[2]
    k = np.array([[xx-yy-zz, yx+xy, zx+xz, zy-yz],
                  [yx+xy, yy-xx-zz, zy+yz, xz-zx],
                  [zx+xz, zy+yz, zz-xx-yy, yx-xy],
                  [zy-yz, xz-zx, yx-xy, xx+yy+zz]]) / 3
    values, vectors = np.linalg.eigh(k)
    q = vectors[:, np.argmax(values)]
    return -q if q[3] > 0 else q  # MD5 reconstructs a negative W.


def read_smd(path):
    text = Path(path).read_text()
    nodes = [(m[2], int(m[3])) for m in re.finditer(r'^\s*(\d+) "([^"]+)" (-?\d+)', text, re.M)]
    section = text.split('skeleton', 1)[1].split('end', 1)[0]
    frames = []
    for frame in re.split(r'\btime \d+\s*', section)[1:]:
        rows = [list(map(float, line.split())) for line in frame.splitlines() if line.strip()]
        if len(rows) != len(nodes):
            raise ValueError('Incomplete skeleton frame')
        frames.append([(np.array(row[1:4]), rotation(row[4:7])) for row in rows])
    triangles = {}
    if '\ntriangles' in text:
        lines = text.split('\ntriangles', 1)[1].strip().splitlines()
        for i in range(0, len(lines)-1, 4):
            material = lines[i].strip()
            tri = []
            for line in lines[i+1:i+4]:
                row = line.split()
                count = int(row[9]) if len(row) > 9 else 0
                links = [(int(row[10+j*2]), float(row[11+j*2])) for j in range(count)] or [(int(row[0]), 1.)]
                total = sum(w for _, w in links)
                tri.append((np.array(list(map(float, row[1:4]))), tuple(map(float, row[7:9])), [(j,w/total) for j,w in links]))
            triangles.setdefault(material, []).append(tri)
    return nodes, frames, triangles


def global_pose(nodes, frame, convert=False):
    result = []
    for (_, parent), (p, r) in zip(nodes, frame):
        if parent >= 0:
            pp, pr = result[parent]
            p, r = pp+pr@p, pr@r
        result.append((p, r))
    return [(BASIS@p, BASIS@r) for p,r in result] if convert else result


def fmt(v):
    return ' '.join(f'{x:.8f}' for x in v)


def with_origin(nodes, frames):
    # idTech strips movement from joint zero. Keep the Source camera-relative
    # animation offset on a child joint instead of losing it during playback.
    return [('origin', -1)]+[(n,p+1) for n,p in nodes], [[(np.zeros(3),np.eye(3))]+f for f in frames]


def convert(source, output):
    source, output = Path(source), Path(output)
    output.mkdir(parents=True, exist_ok=True)
    nodes, frames, surfaces = read_smd(source/'portalgun_reference.smd')
    nodes, frames = with_origin(nodes, frames)
    surfaces = {m:[[(p,uv,[(j+1,w) for j,w in links]) for p,uv,links in tri] for tri in tris] for m,tris in surfaces.items()}
    bind = global_pose(nodes, frames[0])
    joints = global_pose(nodes, frames[0], True)
    lines = ['MD5Version 10', 'commandline "local Portal asset conversion"',
             f'numJoints {len(nodes)}', f'numMeshes {len(surfaces)+3}', 'joints {']
    lines += [f' "{name}" {parent} ( {fmt(p)} ) ( {fmt(quaternion(r)[:3])} )'
              for (name,parent),(p,r) in zip(nodes,joints)]
    lines += ['}']
    # Keep vertex UV seams, but merge matching corners for smooth normals.
    all_weights = []
    for material, triangles in surfaces.items():
        verts, weights, faces, lookup = [], [], [], {}
        for triangle in triangles:
            face = []
            for p, uv, links in triangle:
                key = (*p, *uv, *sum(([j,w] for j,w in links), []))
                if key not in lookup:
                    lookup[key] = len(verts)
                    verts.append((uv, len(weights), len(links)))
                    for joint, weight in links:
                        bp, br = bind[joint]
                        weights.append((joint, weight, br.T@(p-bp)))
                face.append(lookup[key])
            faces.append(face)
        lines += ['mesh {', f' shader "reawakened/portalgun/view/{material}"', f' numverts {len(verts)}']
        lines += [f' vert {i} ( {u:.8f} {1-v:.8f} ) {first} {count}' for i,((u,v),first,count) in enumerate(verts)]
        lines += [f' numtris {len(faces)}']
        # MD5/idTech uses clockwise front faces.
        lines += [f' tri {i} {a} {c} {b}' for i,(a,b,c) in enumerate(faces)]
        lines += [f' numweights {len(weights)}']
        lines += [f' weight {i} {joint} {weight:.8f} ( {fmt(p)} )' for i,(joint,weight,p) in enumerate(weights)]
        lines += ['}']
        all_weights.extend(weights)
    # Recover attachment positions from the user's decompiled QC. Skin the
    # glow quads to those same joints, then let Prey's sprite deform face them
    # toward the camera. They share the weapon's visibility and depth rules.
    qc = (source/'v_portalgun.qc').read_text()
    attachments = {m[1]:(m[2],np.array(list(map(float,m[3].split())))) for m in
                   re.finditer(r'\$attachment "([^"]+)" "([^"]+)" ([\d.\- ]+) rotate',qc)}
    for material, sprites in (
        ('indicator_blue',[('Body_light',2.304)]),
        ('indicator_orange',[('Body_light',2.304)]),
        ('core_glow',[('Inside_effects',3.5)]+[(f'Beam_point{i}',5.12) for i in range(1,6)])):
        weights = []
        for attachment,size in sprites:
            bone, pos = attachments[attachment]
            # Keep the soft sprite center just above the housing when viewed
            # with Prey's weapon projection; depth testing still clips the rim.
            pos = pos + np.array([0, 0.35, 0])
            joint = next(i for i,(n,p) in enumerate(nodes) if n == bone)
            for y,z in ((-1,1),(1,1),(1,-1),(-1,-1)):
                weights.append((joint,1.,pos+np.array([0,y*size/2,z*size/2])))
        lines += ['mesh {', f' shader "reawakened/portalgun/view/{material}"', f' numverts {len(weights)}']
        lines += [f' vert {i} ( {((0,0),(1,0),(1,1),(0,1))[i%4][0]} {((0,0),(1,0),(1,1),(0,1))[i%4][1]} ) {i} 1' for i in range(len(weights))]
        lines += [f' numtris {len(sprites)*2}']
        for i in range(len(sprites)):
            lines += [f' tri {i*2} {i*4} {i*4+1} {i*4+2}',f' tri {i*2+1} {i*4} {i*4+2} {i*4+3}']
        lines += [f' numweights {len(weights)}']
        lines += [f' weight {i} {joint} 1 ( {fmt(p)} )' for i,(joint,w,p) in enumerate(weights)]+['}']
        all_weights.extend(weights)
    (output/'view.md5mesh').write_text('\n'.join(lines)+'\n')
    for name in ('idle','draw','fire1','holster','fizzle','pickup','release','lowidle','idletolow','lowtoidle'):
        anim_nodes, anim_frames, _ = read_smd(source/'v_portalgun_anims'/(name+'.smd'))
        anim_nodes, anim_frames = with_origin(anim_nodes, anim_frames)
        if anim_nodes != nodes:
            raise ValueError('Animation skeleton differs: '+name)
        # A stationary pose still needs two frames for the MD5 animation loader.
        if len(anim_frames) == 1:
            anim_frames *= 2
        poses, bounds = [], []
        for frame in anim_frames:
            values = []
            for (_,parent),(p,r) in zip(nodes,frame):
                if parent < 0: p,r = BASIS@p, BASIS@r
                values.extend([*p,*quaternion(r)[:3]])
            poses.append(values)
            pose = global_pose(nodes,frame,True)
            # Bound every weight endpoint, which also bounds their convex blend.
            points = np.array([pose[j][0]+pose[j][1]@p for j,w,p in all_weights])
            bounds.append((points.min(axis=0)-1, points.max(axis=0)+1))
        lines = ['MD5Version 10','commandline "local Portal animation conversion"',f'numFrames {len(poses)}',
                 f'numJoints {len(nodes)}','frameRate 30',f'numAnimatedComponents {len(nodes)*6}', 'hierarchy {']
        lines += [f' "{n}" {p} 63 {i*6}' for i,(n,p) in enumerate(nodes)]
        lines += ['}', 'bounds {']+[f' ( {fmt(a)} ) ( {fmt(b)} )' for a,b in bounds]+['}', 'baseframe {']
        lines += [f' ( {fmt(poses[0][i:i+3])} ) ( {fmt(poses[0][i+3:i+6])} )' for i in range(0,len(nodes)*6,6)]
        lines += ['}']+[f'frame {i} {{\n {fmt(v)}\n}}' for i,v in enumerate(poses)]
        (output/(name+'.md5anim')).write_text('\n'.join(lines)+'\n')
    return len(nodes), sum(len(t) for t in surfaces.values())
