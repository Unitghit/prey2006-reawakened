"""Flatten the retail portal's translation-only opening animation, locally."""
import math
import re
import numpy as np


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
    def fmt(v):return ' '.join(f'{x:.8f}' for x in v)
    models=output/'models/reawakened/portalgun';models.mkdir(parents=True,exist_ok=True)
    definitions=[]
    blocks=[b for b in mesh.split('mesh {')[1:] if re.search(r'shader "[^"]+/(portal|portal_fx|portal_innerwarp)"',b)]
    for color in ('blue','orange'):
        for closed in (False,True):
            name=color+('_closed' if closed else '')+'_opening'
            lines=['MD5Version 10','commandline "local flattened retail portal animation"',f'numJoints {len(joints)}',f'numMeshes {len(blocks)}','joints {']
            lines += [f' "{n}" {(-1 if i==0 else 0)} ( {fmt(projected[i])} ) ( 0 0 0 )' for i,(n,p,r) in enumerate(joints)]+['}']
            for block in blocks:
                material=re.search(r'shader "([^"]+)"',block)[1].rsplit('/',1)[1]
                inner=material=='portal_innerwarp'
                fixed=inner or material=='portal'
                if inner and closed:material='portal_back'
                if color=='blue' and not(inner and not closed):material=material.replace('portal','superportal',1)
                depth=.06 if inner else (.25 if material.endswith('_fx') else .125)
                block=re.sub(r'shader "[^"]+"',f'shader "reawakened/portalgun/retail_{material}"',block)
                def weight(m):
                    i=int(m[2]);p=joints[i][1]+joints[i][2]@np.array(list(map(float,m[4].split())))
                    # Animate the outer energy layer. Keep a complete rim
                    # around the already-open aperture throughout the effect.
                    if fixed:i=0
                    p=flat(p)-projected[i];p[0]=depth
                    return f'weight {m[1]} {i} {m[3]} ( {fmt(p)} )'
                block=re.sub(r'weight\s+(\d+)\s+(\d+)\s+([\d.]+)\s+\( ([^)]*) \)',weight,block)
                lines.append('mesh {'+block)
            (models/(name+'.md5mesh')).write_text('\n'.join(lines)+'\n')
            definitions.append(f'model rw_portal_{name} {{\n mesh models/reawakened/portalgun/{name}.md5mesh\n anim open models/reawakened/portalgun/open_flat.md5anim\n}}')
    # Settle into the existing flattened portal's bind shape instead of
    # snapping from the retail animation's larger final pose to our mesh.
    for i in range(max(0,len(frames)-8),len(frames)):
        t=(i-(len(frames)-8))/7.;t=t*t*(3-2*t)
        frames[i]=[p*(1-t)+target*t for p,target in zip(frames[i],projected)]
    rate=int(re.search(r'frameRate\s+(\d+)',animation)[1])
    lines=['MD5Version 10','commandline "local flattened retail opening"',f'numFrames {len(frames)}',f'numJoints {len(joints)}',f'frameRate {rate}',f'numAnimatedComponents {3*len(joints)}','hierarchy {']
    lines += [f' "{n}" {(-1 if i==0 else 0)} 7 {i*3}' for i,(n,p,r) in enumerate(joints)]+['}','bounds {']
    # Conservative bounds cover the retail overshoot and the flattened artwork.
    lines += [' ( -1 -256 -256 ) ( 1 256 256 )' for _ in frames]+['}','baseframe {']
    lines += [f' ( {fmt(p)} ) ( 0 0 0 )' for p in frames[0]]+['}']
    lines += [f'frame {i} {{\n '+ ' '.join(fmt(p) for p in frame)+'\n}' for i,frame in enumerate(frames)]
    (models/'open_flat.md5anim').write_text('\n'.join(lines)+'\n')
    (output/'def').mkdir(parents=True,exist_ok=True)
    (output/'def/reawakened_portalgun_opening.def').write_text('\n'.join(definitions)+'\n')
