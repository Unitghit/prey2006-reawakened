"""Build a local Jen seam repair from the user's retail Prey assets.

Requires Pillow and NumPy. Only this script is distributable; the generated PK4
contains derived retail assets and must not be committed or redistributed.
Use with the renderer's Jen chest tangent-frame correction.
"""
import argparse
import io
import re
import zipfile
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = 'models/characters/girlfriend/'
OUTPUT_IMAGE = 'models/reawakened/jen_chest_local.tga'
# Bind-pose exposed chest centerline UVs, in descending anatomical order.
SEAM = np.array([[.932395, .660884], [.980317, .730054],
                 [.983228, .773651], [.985082, .801420]])
EXPRESSION = re.compile(
    r'addnormals\(models/characters/girlfriend/girlfriend_liquid_local.tga,\s*'
    r'heightmap\(models/characters/girlfriend/girlfriend_liquid_h.tga,\s*7\s*\)\s*\)')


def normalize(v):
    return v / np.maximum(np.linalg.norm(v, axis=2, keepdims=True), 1e-15)


def combine_normal(normal, height):
    """Match the engine's heightmap(7), dropsample, addnormals operations."""
    h = height.astype(float).mean(2).astype(np.uint8).astype(float)
    down = np.roll(h, -1, 0)
    one = np.ones_like(h)
    a = normalize(np.stack((-(np.roll(h, -1, 1)-h)*7/256,
                            -(down-h)*7/256, one), 2))
    b = normalize(np.stack((-(np.roll(down, -1, 1)-down)*7/256,
                            -(down-h)*7/256, one), 2))
    bump = (normalize(a+b)*127+128).astype(np.uint8)
    ny, nx = normal.shape[:2]
    by, bx = bump.shape[:2]
    # R_Dropsample uses a quarter-pixel offset for rows and integer columns.
    ys = ((np.arange(ny)+.25)*by/ny).astype(int)
    xs = (np.arange(nx)*bx/nx).astype(int)
    bump = (bump[ys[:, None], xs].astype(float)-128)/127
    n = (normal.astype(float)-128)/127
    n[:, :, 2] = np.where(np.linalg.norm(n, axis=2) < 1,
        np.sqrt(np.maximum(0, 1-(n[:, :, :2]**2).sum(2))), n[:, :, 2])
    n[:, :, :2] += bump[:, :, :2]
    return (normalize(n)*127+128).clip(0, 255).astype(np.uint8)


def repair(normal):
    """Neutralize only the across-seam component in a feathered UV strip."""
    height, width = normal.shape[:2]
    if (width, height) != (1024, 1024):
        raise ValueError('Expected the retail 1024x1024 body normal map')
    points = SEAM*np.array([width, height])
    y, x = np.mgrid[:height, :width]
    xy = np.stack((x+.5, y+.5), 2)
    best = np.full((height, width), np.inf)
    perpendicular = np.zeros((height, width, 2))
    for start, end in zip(points[:-1], points[1:]):
        d = end-start
        t = np.clip(((xy-start)*d).sum(2)/(d*d).sum(), 0, 1)
        distance = np.linalg.norm(xy-(start+t[:, :, None]*d), axis=2)
        use = distance < best
        best[use] = distance[use]
        perpendicular[use] = np.array([d[1], -d[0]])/np.linalg.norm(d)
    blend = np.clip((best-2)/12, 0, 1)
    blend = blend*blend*(3-2*blend)
    n = (normal.astype(float)-128)/127
    across = (n[:, :, :2]*perpendicular).sum(2)
    n[:, :, :2] -= (1-blend)[:, :, None]*across[:, :, None]*perpendicular
    result = (normalize(n)*127+128).clip(0, 255).astype(np.uint8)
    result[blend == 1] = normal[blend == 1]
    assert np.array_equal(result[blend == 1], normal[blend == 1])
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', type=Path, required=True, help='Retail base folder containing pak*.pk4')
    parser.add_argument('--output', type=Path, required=True, help='Local output PK4 (outside source checkout)')
    args = parser.parse_args()
    wanted = {'materials/characters.mtr', ROOT+'girlfriend_liquid_local.tga',
              ROOT+'girlfriend_liquid_h.tga'}
    data = {}
    for path in sorted(args.base.glob('pak*.pk4')):
        with zipfile.ZipFile(path) as archive:
            for name in archive.namelist():
                if name.lower() in wanted:
                    data[name.lower()] = archive.read(name)
    missing = wanted-data.keys()
    if missing:
        raise ValueError('Missing retail assets: '+', '.join(sorted(missing)))
    def rgb(name):
        return np.array(Image.open(io.BytesIO(data[ROOT+name])).convert('RGB'))
    normal = rgb('girlfriend_liquid_local.tga')
    height = rgb('girlfriend_liquid_h.tga')
    if normal.shape != (1024, 1024, 3) or height.shape != (512, 512, 3):
        raise ValueError('Unsupported replacement texture dimensions')
    source = data['materials/characters.mtr'].decode('latin1')
    material, count = EXPRESSION.subn(OUTPUT_IMAGE, source)
    if count == 0:
        raise ValueError('Retail body normal-map expression was not found')
    image = io.BytesIO()
    Image.fromarray(repair(combine_normal(normal, height))).save(image, format='TGA')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_suffix('.pk4.tmp')
    with zipfile.ZipFile(temporary, 'w', zipfile.ZIP_DEFLATED) as archive:
        archive.writestr(OUTPUT_IMAGE, image.getvalue())
        archive.writestr('materials/characters.mtr', material.encode('latin1'))
    temporary.replace(args.output)
    print(f'Wrote {args.output}; updated {count} body-map references')


if __name__ == '__main__':
    main()
