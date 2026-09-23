"""Adapt the user's retail Prey portal geometry for wall-mounted portal-gun openings.

Usage: python build_assets.py RETAIL_BASE OUTPUT_BASE
Only this importer is distributable; its generated meshes derive from retail data.
"""
from pathlib import Path
import math
import re
import sys
import zipfile
from portal_opening import energy_polygon


def remove_blue_lightning(body):
    """Keep the retail blue mist/glow, omitting its four flashing arc stages."""
    return re.sub(
        r'\{[^{}]*\bmap\s+models/mapobjects/superportal/superportal_lightning[1-4]\.tga\b[^{}]*\}',
        '', body, flags=re.IGNORECASE)


def cross(a, b):
    return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]


def read_asset(base, name):
    loose = base / name
    if loose.is_file():
        return loose.read_text()
    for archive in sorted(base.glob("*.pk4"), reverse=True):
        with zipfile.ZipFile(archive) as pak:
            if name in pak.namelist():
                return pak.read(name).decode("ascii")
    raise SystemExit("Retail Prey asset not found: " + name + " in " + str(base))


def read_mesh(base):
    return read_asset(base, "models/mapobjects/portal/portal.md5mesh")


def material_body(text, name):
    start = re.search(r"(?m)^" + re.escape(name) + r"\s*\{", text)
    if not start:
        raise ValueError("Missing retail material: " + name)
    opening = text.index("{", start.start())
    depth = 0
    # Ignore braces in comments and quoted strings when extracting the declaration.
    for token in re.finditer(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|[{}]', text[opening:], re.S):
        if token[0] == "{": depth += 1
        elif token[0] == "}":
            depth -= 1
            if depth == 0:
                return text[opening+1:opening+token.start()]
    raise ValueError("Unterminated retail material: " + name)


def parse_mesh(text):
    joints = []
    for m in re.finditer(r'"[^\"]+"\s+-?\d+\s+\( ([^)]*) \)\s+\( ([^)]*) \)', text):
        pos = list(map(float, m[1].split()))
        q = list(map(float, m[2].split()))
        q += [-math.sqrt(max(0, 1-sum(v*v for v in q)))]
        joints.append((pos, q))
    surfaces = []
    for mesh in text.split("mesh {")[1:]:
        material = re.search(r'shader "([^\"]+)"', mesh)[1]
        weights = []
        for w in re.finditer(r'weight\s+\d+\s+(\d+)\s+([\d.]+)\s+\( ([^)]*) \)', mesh):
            pos, q = joints[int(w[1])]
            v = list(map(float, w[3].split()))
            uv = cross(q, v)
            uuv = cross(q, uv)
            weights.append([float(w[2])*(pos[k]+v[k]+2*(q[3]*uv[k]+uuv[k])) for k in range(3)])
        vertices, uv = [], []
        for v in re.finditer(r'vert\s+\d+\s+\( ([^)]*) \)\s+(\d+)\s+(\d+)', mesh):
            first, count = int(v[2]), int(v[3])
            vertices.append([sum(w[k] for w in weights[first:first+count]) for k in range(3)])
            uv.append(list(map(float, v[1].split())))
        faces = [tuple(map(int, m.groups())) for m in re.finditer(r'tri\s+\d+\s+(\d+)\s+(\d+)\s+(\d+)', mesh)]
        surfaces.append((material, vertices, uv, faces))
    return surfaces


def ase_surface(index, vertices, uv, faces):
    lines = [f'*GEOMOBJECT {{', f'*NODE_NAME "portal_{index}"',
             '*NODE_TM { *TM_ROW0 1 0 0 *TM_ROW1 0 1 0 *TM_ROW2 0 0 1 *TM_ROW3 0 0 0 }',
             '*MESH {', '*TIMEVALUE 0', f'*MESH_NUMVERTEX {len(vertices)}', f'*MESH_NUMFACES {len(faces)}', '*MESH_VERTEX_LIST {']
    lines += [f'*MESH_VERTEX {i} {p[0]:.6f} {p[1]:.6f} {p[2]:.6f}' for i, p in enumerate(vertices)]
    lines += ['}', '*MESH_FACE_LIST {']
    lines += [f'*MESH_FACE {i}: A: {a} B: {c} C: {b} AB: 1 BC: 1 CA: 1 *MESH_SMOOTHING 1 *MESH_MTLID 0' for i, (a,b,c) in enumerate(faces)]
    lines += ['}', f'*MESH_NUMTVERTEX {len(uv)}', '*MESH_TVERTLIST {']
    lines += [f'*MESH_TVERT {i} {u:.8f} {1-v:.8f} 0' for i,(u,v) in enumerate(uv)]
    lines += ['}', f'*MESH_NUMTVFACES {len(faces)}', '*MESH_TFACELIST {']
    lines += [f'*MESH_TFACE {i} {a} {c} {b}' for i,(a,b,c) in enumerate(faces)]
    return '\n'.join(lines + ['}', '}', f'*MATERIAL_REF {index}', '}'])



def energy_material(color, closed):
    """Original stage setup; art is resolved only from the user's retail data."""
    rgb = "0.012, 0.04, 0.13" if color == "blue" else "0.13, 0.027, 0.004"
    mist = "0.025, 0.10, 0.32" if color == "blue" else "0.32, 0.085, 0.012"
    art = "superportal/superportal" if color == "blue" else "portal/portal"
    alpha = "1" if closed else "rw_portal_energy_fade[(time - parm8) * 4]"
    condition = "" if closed else "if (time < parm8 + 0.25)"
    baseblend = "gl_one, gl_zero" if closed else "blend"
    name = f"reawakened/portalgun/energy_{color}" + ("_closed" if closed else "")
    return f"""{name} {{
 polygonOffset 1
 noshadows
 noselfshadow
 nooverlays
 twoSided
 {{
  {condition}
  blend {baseblend}
  map _white
  color {rgb}, {alpha}
 }}
 {{
  {condition}
  blend gl_src_alpha, gl_one
  map textures/liquid/noise.tga
  scale 1.5, 1.5
  scroll time * 0.035, time * -0.021
  color {mist}, {alpha}
 }}
 {{
  {condition}
  blend gl_src_alpha, gl_one
  map textures/particles/energy_cloud.tga
  rotate time * -0.04
  color {mist}, {alpha}
 }}
 {{
  {condition}
  blend gl_src_alpha, gl_one
  map models/mapobjects/{art}_back_add.tga
  rotate time * 0.025
  rgb 0.65
  alpha {alpha}
 }}
 {{
  {condition}
  blend gl_src_alpha, gl_one
  map models/mapobjects/{art}_back_add.tga
  rotate time * -0.017
  scale 1.18, 1.18
  translate -0.09, -0.09
  rgb 0.4
  alpha {alpha}
 }}
}}
"""


def build(retail, output):
    surfaces = parse_mesh(read_mesh(retail))
    opening = next(s[1] for s in surfaces if s[0].endswith('/portal_innerwarp'))
    ymin, ymax = min(p[1] for p in opening), max(p[1] for p in opening)
    zmin, zmax = min(p[2] for p in opening), max(p[2] for p in opening)
    cy, cz = (ymin+ymax)/2, (zmin+zmax)/2
    # Preserve the original energy portal's visual size/proportions (roughly
    # 76 x 93 units at the opening), rather than stretching it to the generous
    # 96 x 144 traversal envelope. Physics clearance is defined independently
    # in portalgun.inl and intentionally remains unchanged.
    sy, sz = 1.0, 1.0
    # Keep original UVs and artwork. Flatten the retail funnel against its wall;
    # omit backside and outer refraction, which would sample the supporting wall.
    surfaces = [s for s in surfaces if s[0].endswith(('/portal', '/portal_fx', '/portal_innerwarp'))]
    retail_materials = read_asset(retail, "materials/portals.mtr")
    adapted_materials = {}
    models = output/'models/reawakened/portalgun'
    models.mkdir(parents=True, exist_ok=True)
    for color in ('blue', 'orange'):
        for closed in (False, True):
            materials, geometry = [], []
            for index, (material, vertices, uv, faces) in enumerate(surfaces):
                inner = material.endswith('/portal_innerwarp')
                if inner and closed:
                    material = 'models/mapobjects/portal/portal_back'
                if color == 'blue' and not (inner and not closed):
                    material = material.replace('models/mapobjects/portal/portal', 'models/mapobjects/superportal/superportal')
                source_material = material
                material = "reawakened/portalgun/retail_" + material.rsplit("/", 1)[1]
                if material not in adapted_materials:
                    body = material_body(retail_materials, source_material)
                    if source_material == 'models/mapobjects/superportal/superportal_fx':
                        body = remove_blue_lightning(body)
                    # Wall-mounted portal passes must win coplanar depth tests,
                    # including against decals. Retail level portals stay unchanged.
                    adapted_materials[material] = material + " {\n polygonOffset 1\n" + body + "}\n"
                if inner and closed:
                    material = f"reawakened/portalgun/energy_{color}_closed"
                    vertices = [vertices[k] for k in energy_polygon(vertices)]
                    faces = [(0,i+1,i) for i in range(1,len(vertices)-1)]
                    uv = [((p[1]-ymin)/(ymax-ymin), (p[2]-zmin)/(zmax-zmin)) for p in vertices]
                materials.append(material)
                depth = 0.06 if inner else (0.25 if source_material.endswith('_fx') else 0.125)
                transformed = [(depth, (p[1]-cy)*sy, (p[2]-cz)*sz) for p in vertices]
                geometry.append(ase_surface(index, transformed, uv, faces))
            if not closed:
                _, vertices, _, faces = next(s for s in surfaces if s[0].endswith('/portal_innerwarp'))
                materials.append(f"reawakened/portalgun/energy_{color}")
                vertices = [vertices[k] for k in energy_polygon(vertices)]
                faces = [(0,i+1,i) for i in range(1,len(vertices)-1)]
                uv = [((p[1]-ymin)/(ymax-ymin), (p[2]-zmin)/(zmax-zmin)) for p in vertices]
                geometry.append(ase_surface(len(materials)-1, [(0.08,p[1]-cy,p[2]-cz) for p in vertices], uv, faces))
            lines = ['*3DSMAX_ASCIIEXPORT 200', '*MATERIAL_LIST {', f'*MATERIAL_COUNT {len(materials)}']
            lines += [f'*MATERIAL {i} {{ *MATERIAL_NAME "{m}" *MAP_DIFFUSE {{ *BITMAP "C:/base/{m}.tga" }} }}' for i,m in enumerate(materials)]
            lines += ['}'] + geometry
            (models/(color+('_closed' if closed else '')+'.ase')).write_text('\n'.join(lines)+'\n')
    (models/'closed.ase').write_bytes((models/'blue_closed.ase').read_bytes())
    (output/'materials').mkdir(parents=True, exist_ok=True)
    (output/'materials/reawakened_portalgun_retail.mtr').write_text('\n'.join(adapted_materials.values()))
    energy = "table rw_portal_energy_fade { clamp { 1, 0 } }\n"
    energy += '\n'.join(energy_material(c, closed) for c in ('blue','orange') for closed in (False,True))
    (output/'materials/reawakened_portalgun_energy.mtr').write_text(energy)

    from portal_opening import build_opening
    build_opening(read_mesh(retail), read_asset(retail, 'models/mapobjects/portal/anim/open.md5anim'), output, cy, cz)
    print('Adapted retail blue/orange portal meshes:', models)


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    build(Path(sys.argv[1]), Path(sys.argv[2]))
