"""Generate an original convex ASE box for the in-engine pusher test."""
from pathlib import Path

vertices = [(-64,-64,0),(64,-64,0),(64,64,0),(-64,64,0),
            (-64,-64,16),(64,-64,16),(64,64,16),(-64,64,16)]
faces = [(0,2,1),(0,3,2),(4,5,6),(4,6,7),(0,1,5),(0,5,4),
         (1,2,6),(1,6,5),(2,3,7),(2,7,6),(3,0,4),(3,4,7)]
lines = ['*3DSMAX_ASCIIEXPORT 200', '*MATERIAL_LIST {', '*MATERIAL_COUNT 1',
         '*MATERIAL 0 {', '*MATERIAL_NAME "_default"', '*MAP_DIFFUSE {',
         '*BITMAP "_default"', '}', '}', '}', '*GEOMOBJECT {',
         '*NODE_NAME "presentation_platform"', '*MESH {', '*TIMEVALUE 0',
         '*MESH_NUMVERTEX 8', '*MESH_NUMFACES 12', '*MESH_VERTEX_LIST {']
lines += [f'*MESH_VERTEX {i} {x} {y} {z}' for i,(x,y,z) in enumerate(vertices)]
lines += ['}', '*MESH_FACE_LIST {']
lines += [f'*MESH_FACE {i}: A: {a} B: {b} C: {c} AB: 1 BC: 1 CA: 1 *MESH_SMOOTHING 0 *MESH_MTLID 0' for i,(a,b,c) in enumerate(faces)]
lines += ['}', '*MESH_NUMTVERTEX 3', '*MESH_TVERTLIST {', '*MESH_TVERT 0 0 0 0',
          '*MESH_TVERT 1 1 0 0', '*MESH_TVERT 2 1 1 0', '}', '*MESH_NUMTVFACES 12', '*MESH_TFACELIST {']
lines += [f'*MESH_TFACE {i} 0 1 2' for i in range(12)]
lines += ['}', '}', '*MATERIAL_REF 0', '}']
out = Path(__file__).parent / 'fixtures/presentation_platform.ase'
out.parent.mkdir(exist_ok=True)
out.write_text('\n'.join(lines) + '\n', encoding='ascii')
print(out)
target_lines = []
for line in lines:
    fields = line.split()
    if fields and fields[0] == '*MESH_VERTEX':
        fields[2] = str(int(fields[2]) // 4)
        fields[3] = str(int(fields[3]) // 4)
        if int(fields[1]) >= 4: fields[-1] = '128'
        line = ' '.join(fields)
    target_lines.append(line)
(out.parent/'presentation_target.ase').write_text('\n'.join(target_lines)+'\n',encoding='ascii')
