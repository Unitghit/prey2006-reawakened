"""Convert the locally owned Portal world gun; no retail data is distributed."""
import re
import subprocess
import sys
from pathlib import Path
import numpy as np
from PIL import Image
from smd_to_md5 import read_smd, fmt

# The world gun's muzzle is -Z and its top indicator is -X. Tommy expects
# +X forward, +Y left, +Z up. This is a rigid, handedness-preserving transform.
WORLD_BASIS = np.array([[0., 0, -1], [0, -1, 0], [-1, 0, 0]])


def import_worldmodel(vpk, crowbar, output, work, decode_vtf):
    source, decompiled = work/'world-source', work/'world-decompiled'
    for name in vpk.entries:
        if name.startswith('models/weapons/w_portalgun.'):
            vpk.extract(name, source)
    subprocess.run([str(crowbar.resolve()), '-p', str((source/'models/weapons/w_portalgun.mdl').resolve()),
                    '-o', str(decompiled.resolve())], check=True, timeout=120,
                   creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0)
    nodes, frames, surfaces = read_smd(decompiled/'w_portalgun_reference.smd')
    if nodes != [('weapon_bone', -1)] or not np.allclose(frames[0][0][0], 0) or not np.allclose(frames[0][0][1], np.eye(3)):
        raise ValueError('Unexpected Portal world-model bind skeleton')
    meshes = []
    for material, triangles in surfaces.items():
        vertices, faces, lookup = [], [], {}
        for triangle in triangles:
            face = []
            for position, uv, links in triangle:
                if links != [(0, 1.0)]:
                    raise ValueError('Unexpected world-model skin weights')
                key = (*position, *uv)
                if key not in lookup:
                    lookup[key] = len(vertices)
                    vertices.append((WORLD_BASIS@position, (uv[0], 1-uv[1])))
                face.append(lookup[key])
            faces.append((face[0], face[2], face[1]))
        meshes.append(('reawakened/portalgun/world/'+material, vertices, faces))
    qc = (decompiled/'w_portalgun.qc').read_text()
    attachments = {m[1]: np.array(list(map(float, m[2].split()))) for m in
                   re.finditer(r'\$attachment "([^"]+)" "weapon_bone" ([\d.\- ]+) rotate', qc)}
    # Use the world model's own sockets, not the viewmodel's camera offsets.
    for material, sockets in [('indicator_blue', [('Body_light', 2.304)]),
                              ('indicator_orange', [('Body_light', 2.304)]),
                              ('core_glow', [('Inside_effects', 3.5)]+[(f'Beam_point{i}', 5.12) for i in range(1, 6)])]:
        vertices, faces = [], []
        for socket, size in sockets:
            center = WORLD_BASIS@attachments[socket]
            center[2] += 0.15
            start = len(vertices)
            for (y,z), uv in zip(((-1,1),(1,1),(1,-1),(-1,-1)), ((0,0),(1,0),(1,1),(0,1))):
                vertices.append((center+np.array([0,y*size/2,z*size/2]), uv))
            faces += [(start,start+1,start+2), (start,start+2,start+3)]
        meshes.append(('reawakened/portalgun/view/'+material, vertices, faces))
    lines = ['MD5Version 10', 'commandline "local Portal world conversion"', 'numJoints 1',
             f'numMeshes {len(meshes)}', 'joints { "origin" -1 ( 0 0 0 ) ( 0 0 0 ) }']
    points = []
    for material, vertices, faces in meshes:
        lines += ['mesh {', f' shader "{material}"', f' numverts {len(vertices)}']
        lines += [f' vert {i} ( {fmt(uv)} ) {i} 1' for i,(p,uv) in enumerate(vertices)]
        lines += [f' numtris {len(faces)}']+[f' tri {i} {a} {b} {c}' for i,(a,b,c) in enumerate(faces)]
        lines += [f' numweights {len(vertices)}']+[f' weight {i} 0 1 ( {fmt(p)} )' for i,(p,uv) in enumerate(vertices)]+['}']
        points.extend(p for p,uv in vertices)
    models = output/'models/reawakened/portalgun/world'
    models.mkdir(parents=True, exist_ok=True)
    (models/'world.md5mesh').write_text('\n'.join(lines)+'\n')
    bounds = f'( {fmt(np.min(points,axis=0)-1)} ) ( {fmt(np.max(points,axis=0)+1)} )'
    (models/'idle.md5anim').write_text('MD5Version 10\ncommandline "local Portal world pose"\nnumFrames 2\nnumJoints 1\nframeRate 30\nnumAnimatedComponents 0\n'
        'hierarchy { "origin" -1 0 0 }\nbounds { '+bounds+'\n'+bounds+' }\n'
        'baseframe { ( 0 0 0 ) ( 0 0 0 ) }\nframe 0 { }\nframe 1 { }\n')
    textures = output/'textures/reawakened/portalgun'
    textures.mkdir(parents=True, exist_ok=True)
    base = 'materials/models/weapons/w_models/portalgun/'
    for name in ('w_portalgun', 'w_portalgun_normal'):
        decode_vtf(vpk.read(base+name+'.vtf')).save(textures/(name+'.tga'))
    alpha = decode_vtf(vpk.read(base+'w_portalgun_normal.vtf')).getchannel('A')
    exponent = decode_vtf(vpk.read(base+'w_portalgun_exponent.vtf')).getchannel('R')
    Image.merge('RGB', (alpha,exponent,alpha)).save(textures/'phong_world.tga')
