"""Local Portal viewmodel importer. Requires numpy, Pillow, and Crowbar CLI.

python import_viewmodel.py PORTAL_INSTALL CROWBAR_EXE OUTPUT_BASE WORK_DIRECTORY
No Valve assets are distributed with this script.
"""
from pathlib import Path
import io
import shutil
import struct
import subprocess
import sys
from PIL import Image
from source_assets import VPK
from smd_to_md5 import convert


def vtf_image(data):
    if data[:4] != b'VTF\0': raise ValueError('Not VTF')
    major, minor, header = struct.unpack_from('<III', data, 4)
    w,h = struct.unpack_from('<HH', data,16)
    flags,frames = struct.unpack_from('<IH',data,20)
    fmt = struct.unpack_from('<I',data,52)[0]
    mipcount = data[56]
    if frames != 1 or flags & 0x4000: raise ValueError('Animated/cube VTF unsupported')
    def size(w,h,f):
        if f in (13,20): return max(1,(w+3)//4)*max(1,(h+3)//4)*8
        if f in (14,15): return max(1,(w+3)//4)*max(1,(h+3)//4)*16
        raise ValueError('Unsupported VTF pixel format '+str(f))
    lowfmt = struct.unpack_from('<I',data,57)[0]
    offset = header + (size(data[61],data[62],lowfmt) if data[61] and data[62] else 0)
    if minor >= 3:
        count = struct.unpack_from('<I',data,68)[0]
        for i in range(count):
            tag = data[80+i*8:84+i*8]
            if tag[:3] == b'\x30\0\0': offset = struct.unpack_from('<I',data,84+i*8)[0]
    offset += sum(size(max(1,w>>i),max(1,h>>i),fmt) for i in range(mipcount-1,0,-1))
    fourcc = {13:b'DXT1',20:b'DXT1',14:b'DXT3',15:b'DXT5'}[fmt]
    header = struct.pack('<7I',124,0x81007,h,w,size(w,h,fmt),0,1)+bytes(44)
    header += struct.pack('<II4s5I',32,4,fourcc,0,0,0,0,0)+struct.pack('<5I',0x1000,0,0,0,0)
    return Image.open(io.BytesIO(b'DDS '+header+data[offset:offset+size(w,h,fmt)])).convert('RGBA')


def main():
    install, crowbar, destination, work = map(Path,sys.argv[1:])
    # Complete conversion before touching the user's installed files.
    output = work/'generated-viewmodel'
    vpk = VPK(install/'portal/portal_pak_dir.vpk')
    source, decompiled = work/'source', work/'decompiled'
    for name in vpk.entries:
        if name.startswith('models/weapons/v_portalgun.'):
            vpk.extract(name, source)
    creationflags = subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0
    subprocess.run([str(crowbar.resolve()),'-p',str((source/'models/weapons/v_portalgun.mdl').resolve()),
                    '-o',str(decompiled.resolve())],check=True,creationflags=creationflags,timeout=120)
    models = output/'models/reawakened/portalgun/view'
    print('Converted joints/triangles:',convert(decompiled, models))
    textures=output/'textures/reawakened/portalgun'
    textures.mkdir(parents=True,exist_ok=True)
    for name in ('bluelight','orangelight','portalgun_effects'):
        vtf_image(vpk.read('materials/sprites/'+name+'.vtf')).save(textures/(name+'.tga'))
    shared = VPK(install/'hl2/hl2_textures_dir.vpk')
    for name in ('energyball','portal_1_particle','portal_2_particle'):
        asset='materials/effects/'+name+'.vtf'
        vtf_image((vpk if asset in vpk.entries else shared).read(asset)).save(textures/(name+'.tga'))
    materials=output/'materials';materials.mkdir(parents=True,exist_ok=True)
    (materials/'portalgun_shots.mtr').write_text('\n'.join(
        f'reawakened/portalgun/shot_{kind} {{\n translucent\n noShadows\n'
        f' {{ blend add map textures/reawakened/portalgun/{texture}.tga vertexColor glowStage }}\n}}'
        for kind,texture in [('ball','energyball'),('blue','portal_1_particle'),('orange','portal_2_particle')])+'\n')
    base='materials/models/weapons/v_models/v_portalgun/'
    for name in ('v_portalgun','v_portalgun_glass','v_portalgun_normal','v_portalgun_exponent'):
        vtf_image(vpk.read(base+name+'.vtf')).save(textures/(name+'.tga'))
    # This installation's VMT has a stale shared-hands path; use the texture
    # actually shipped alongside the portal gun instead.
    vtf_image(vpk.read(base+'v_hands.vtf')).save(textures/'v_hands.tga')
    # Source stores specular strength in the normal-map alpha for this model.
    normal=vtf_image(vpk.read(base+'v_portalgun_normal.vtf'))
    alpha=normal.getchannel('A')
    Image.merge('RGB',(alpha,alpha,alpha)).save(textures/'specular.tga')
    # A separate RGB map keeps the mask/exponent intact even when the engine
    # compresses specular maps without alpha. The dedicated interaction shader
    # reads strength from R and the Source exponent texture from G.
    exponent=vtf_image(vpk.read(base+'v_portalgun_exponent.vtf')).getchannel('R')
    Image.merge('RGB',(alpha,exponent,alpha)).save(textures/'phong.tga')
    for relative in ('def/portalgun_view.def','script/reawakened/weapon_portalgun_v1.script','materials/portalgun_view.mtr'):
        dest=output/relative;dest.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(Path(__file__).with_name(Path(relative).name),dest)
    for color,sound in (('blue','portalgun_shoot_blue1'),('orange','portalgun_shoot_red1')):
        dest=output/f'sound/reawakened/portalgun/{color}.wav';dest.parent.mkdir(parents=True,exist_ok=True)
        dest.write_bytes(vpk.read('sound/weapons/portalgun/'+sound+'.wav'))
    dest=output/'sound/portalgun_view.sndshd'
    dest.write_text('\n'.join(f'rw_portalgun_{c} {{\n volume -5\n sound/reawakened/portalgun/{c}.wav\n}}' for c in ('blue','orange'))+'\n')
    for path in sorted(output.rglob('*')):
        if path.is_file():
            target=destination/path.relative_to(output)
            target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(path,target)
    print('Imported Portal first-person assets into',destination)


if __name__=='__main__':
    if len(sys.argv)!=5: raise SystemExit(__doc__)
    main()
