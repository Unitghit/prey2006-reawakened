"""Flatten the retail portal's translation-only opening animation, locally."""
import math
import re
import numpy as np


def energy_polygon(points):
    """Convex perimeter of the flattened aperture, as source vertex indices."""
    unique = {}
    for i,p in enumerate(points):
        unique.setdefault((round(float(p[1]),4),round(float(p[2]),4)),i)
    ordered = sorted(unique)
    def turn(a,b,c): return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
    lower, upper = [], []
    for chain, sequence in ((lower,ordered),(upper,reversed(ordered))):
        for point in sequence:
            while len(chain)>=2 and turn(chain[-2],chain[-1],point)<=0: chain.pop()
            chain.append(point)
    return [unique[p] for p in lower[:-1]+upper[:-1]]


def matrix(q):
    x,y,z=q;w=-math.sqrt(max(0,1-x*x-y*y-z*z))
    return np.array([[1-2*y*y-2*z*z,2*x*y-2*z*w,2*x*z+2*y*w],
                     [2*x*y+2*z*w,1-2*x*x-2*z*z,2*y*z-2*x*w],
                     [2*x*z-2*y*w,2*y*z+2*x*w,1-2*x*x-2*y*y]])


def build_opening(mesh, animation, output, cy, cz):
    joints=[]
    for m in re.finditer(r'"([^"\n]+)"\s+(-?\d+)\s+\( ([^)]*) \)\s+\( ([^)]*) \)',mesh):
        joints.append((m[1],np.array(list(map(float,m[3].split()))),matrix(list(map(float,m[4].split())))))
    def flat(p): return np.array([0,p[1]-cy,p[2]-cz])
    projected=[flat(p) for n,p,r in joints];projected[0]=np.zeros(3)
    hierarchy=re.findall(r'"([^"\n]+)"\s+(-?\d+)\s+(\d+)\s+(\d+)',animation.split('hierarchy {')[1].split('}')[0])
    base=[list(map(float,m[1].split()+m[2].split())) for m in re.finditer(r'\( ([^)]*) \)\s+\( ([^)]*) \)',animation.split('baseframe {')[1].split('}')[0])]
    frames=[]
    for block in re.findall(r'frame\s+\d+\s*\{([^}]*)\}',animation):
        values=list(map(float,block.split()));pose=[]
        for i,(_,parent,flags,first) in enumerate(hierarchy):
            row=base[i][:];cursor=int(first)
            for component in range(6):
                if int(flags)&(1<<component):row[component]=values[cursor];cursor+=1
            p=np.array(row[:3]);r=matrix(row[3:]);parent=int(parent)
            if parent>=0:p,r=pose[parent][0]+pose[parent][1]@p,pose[parent][1]@r
            if not np.allclose(r,joints[i][2],atol=.005):
                raise ValueError('Portal animation rotates joints; cannot flatten without distortion')
            pose.append((p,r))
        frames.append([np.zeros(3) if i==0 else flat(p) for i,(p,r) in enumerate(pose)])
    # Scripted openings have a stationary lead-in for their accompanying FX.
    # A gun impact already supplies that cue: retain only the last held pose
    # so the full duration belongs to the visible expansion, not the pre-roll.
    while len(frames) > 2 and np.allclose(frames[0], frames[1], atol=1e-5):
        frames.pop(0)
    extent=np.array([np.linalg.norm(f) for f in frames])
    peak=int(np.argmax(extent))
    growth=np.maximum.accumulate((extent[:peak+1]-extent[0])/(extent[peak]-extent[0]))
    scales=.01+.99*growth
    def fmt(v):return ' '.join(f'{x:.8f}' for x in v)
    models=output/'models/reawakened/portalgun';models.mkdir(parents=True,exist_ok=True)
    definitions=[]
    blocks=[b for b in mesh.split('mesh {')[1:] if re.search(r'shader "[^"]+/(portal|portal_fx|portal_innerwarp)"',b)]
    # Translate each weighted point together, preserving the oval proportions.
    # Only the expanding retail timing envelope is retained; no recoil/settle.
    positions=[np.zeros(3)];converted=[]
    for block in blocks:
        def weight(m):
            joint=int(m[2]);p=joints[joint][1]+joints[joint][2]@np.array(list(map(float,m[4].split())))
            positions.append(flat(p))
            return f'weight {m[1]} {len(positions)-1} {m[3]} ( DEPTH 0 0 )'
        converted.append(re.sub(r'weight\s+(\d+)\s+(\d+)\s+([\d.]+)\s+\( ([^)]*) \)',weight,block))
    projected=positions
    joints=[('root' if i==0 else f'weight_{i}',p,None) for i,p in enumerate(positions)]
    for color in ('blue','orange'):
        for closed in (False,True):
            name=color+('_closed' if closed else '')+'_opening'
            lines=['MD5Version 10','commandline "local flattened retail portal animation"',f'numJoints {len(joints)}',f'numMeshes {len(converted)}','joints {']
            lines += [f' "{n}" {(-1 if i==0 else 0)} ( {fmt(p)} ) ( 0 0 0 )' for i,(n,p,r) in enumerate(joints)]+['}']
            for block in converted:
                material=re.search(r'shader "([^"]+)"',block)[1].rsplit('/',1)[1]
                inner=material=='portal_innerwarp'
                if inner and closed:material='portal_back'
                if color=='blue' and not(inner and not closed):material=material.replace('portal','superportal',1)
                depth=.06 if inner else (.25 if material.endswith('_fx') else .125)
                block=re.sub(r'shader "[^"]+"',f'shader "reawakened/portalgun/retail_{material}"',block)
                if inner and closed:
                    # The retail aperture UVs are a strip, not a planar disk.
                    # Give only our energy layer planar UVs; preserve the live view.
                    weights = {int(m[1]): (int(m[2]), float(m[3])) for m in re.finditer(r'weight\s+(\d+)\s+(\d+)\s+([\d.]+)', block)}
                    verts = list(re.finditer(r'vert\s+(\d+)\s+\( ([^)]*) \)\s+(\d+)\s+(\d+)', block))
                    points = [sum((projected[weights[w][0]] * weights[w][1] for w in range(int(v[3]),int(v[3])+int(v[4]))), np.zeros(3)) for v in verts]
                    hull = energy_polygon(points)
                    ymin,ymax = min(p[1] for p in points),max(p[1] for p in points)
                    zmin,zmax = min(p[2] for p in points),max(p[2] for p in points)
                    energy = '\n shader "reawakened/portalgun/energy_'+color+('_closed' if closed else '')+'"\n'
                    energy += f' numverts {len(hull)}\n'
                    for i,k in enumerate(hull):
                        v,point=verts[k],points[k]
                        energy += f' vert {i} ( {(point[1]-ymin)/(ymax-ymin):.8f} {(point[2]-zmin)/(zmax-zmin):.8f} ) {v[3]} {v[4]}\n'
                    energy += f' numtris {len(hull)-2}\n'
                    for i in range(1,len(hull)-1): energy += f' tri {i-1} 0 {i+1} {i}\n'
                    energy += f' numweights {len(weights)}\n'
                    energy += '\n'.join(re.findall(r'weight\s+\d+\s+\d+\s+[\d.]+\s+\( [^)]* \)',block))+'\n}\n'
                    lines.append('mesh {'+energy.replace('DEPTH',str(depth)))
                else:
                    lines.append('mesh {'+block.replace('DEPTH',str(depth)))
            (models/(name+'.md5mesh')).write_text('\n'.join(lines)+'\n')
            definitions.append(f'model rw_portal_{name} {{\n mesh models/reawakened/portalgun/{name}.md5mesh\n anim open models/reawakened/portalgun/open_flat.md5anim\n}}')
    frames=[[p*scale for p in projected] for scale in scales]
    rate=round((len(frames)-1)/.45)
    lines=['MD5Version 10','commandline "local flattened retail opening"',f'numFrames {len(frames)}',f'numJoints {len(joints)}',f'frameRate {rate}',f'numAnimatedComponents {3*len(joints)}','hierarchy {']
    lines += [f' "{n}" {(-1 if i==0 else 0)} 7 {i*3}' for i,(n,p,r) in enumerate(joints)]+['}','bounds {']
    # Conservative bounds cover the retail overshoot and the flattened artwork.
    lines += [' ( -1 -256 -256 ) ( 1 256 256 )' for _ in frames]+['}','baseframe {']
    lines += [f' ( {fmt(p)} ) ( 0 0 0 )' for p in frames[0]]+['}']
    lines += [f'frame {i} {{\n '+ ' '.join(fmt(p) for p in frame)+'\n}' for i,frame in enumerate(frames)]
    (models/'open_flat.md5anim').write_text('\n'.join(lines)+'\n')
    (output/'def').mkdir(parents=True,exist_ok=True)
    (output/'def/reawakened_portalgun_opening.def').write_text('\n'.join(definitions)+'\n')
