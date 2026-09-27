"""Locally import retail Super Shotgun assets; generated assets are not distributable."""
import re
from pathlib import Path


def import_supershotgun(index, read, text, block, files):
    prefix = 'doom3/'
    shaders = set()
    for name in sorted(index):
        if name.startswith(('models/md5/weapons/doublebarrel_view/', 'models/md5/weapons/shotgun_double_world/')):
            if name.endswith('.md5mesh'):
                mesh = text(name)
                shaders.update(re.findall(r'shader\s+"([^"]+)"', mesh))
                files[prefix+name] = re.sub(r'(shader\s+")([^"]+)', lambda m: m[1]+prefix+m[2], mesh).encode()
            elif name.endswith('.md5anim'):
                files[prefix+name] = read(name)
    if not shaders:
        raise ValueError('Original Doom 3 Super Shotgun assets not found')
    materials = [text(n) for n in index if n.endswith('.mtr')]
    declarations = []
    tables = set()
    for shader in sorted(shaders):
        if any(block(data.decode(), prefix+shader) for path,data in files.items() if path.endswith('.mtr')):
            continue
        decl = next((b for t in materials if (b := block(t, shader))), None)
        if decl is None:
            asset = shader if shader.endswith('.tga') else shader+'.tga'
            files[prefix+asset] = read(asset)
            # Match an implicit GUI material: honor window tint and alpha.
            decl = shader+' {\n { blend blend\n map '+asset+'\n colored\n clamp\n }\n}'
        decl = re.sub(r'(?m)^\s*(?:renderbump[^\n]*|ricochet\s*)$', '', decl)
        for asset in re.findall(r'(?:models|textures|guis|gui)/[\w/.-]+', decl):
            candidate = asset if asset.lower() in index else asset+'.tga'
            if candidate.lower() in index:
                files[prefix+candidate] = read(candidate)
        decl = re.sub(r'(?<![\w/])((?:models|textures|guis|gui)/[\w/.-]+)', lambda m: prefix+m[1], decl)
        for table in sorted(set(re.findall(r'\b(\w+)\s*\[', decl))):
            if re.search(r'\b'+table+r'\b', decl):
                tables.add(table)
                decl = re.sub(r'\b'+table+r'\b', 'd3ssg_'+table, decl)
        declarations.append(decl)
    for table in sorted(tables):
        decl = next((b for t in materials if (b := block(t, table))), None)
        if decl is None:
            raise ValueError('Missing table: '+table)
        declarations.append(re.sub(r'\b'+table+r'\b', 'd3ssg_'+table, decl, flags=re.I))
    files['materials/doom3_supershotgun.mtr'] = ('\n\n'.join(declarations)+'\n').encode()
    weapon = text('def/weapon_shotgun_double.def')
    models = []
    for name in ('viewmodel_shotgun_double','worldmodel_shotgun_double'):
        decl = block(weapon, name).replace(name, 'd3_'+name)
        decl = re.sub(r'(?<![\w/])(models/[\w/.-]+)', lambda m: prefix+m[1], decl)
        decl = re.sub(r'\b(?:player_shotgun_\w+|ssg_\w+)', lambda m: 'd3_'+m[0], decl)
        if name == 'viewmodel_shotgun_double':
            # GUI aside is a held pose, not a looping holster animation.
            # Tommy's independent GUI hand still handles screen presses.
            aliases = {'initialPickup':'raise', 'raise_mp':'raise', 'fire':'fire1', 'reload':'reload',
                       'down':'lower', 'putaway_mp':'lower', 'put_aside':'idle',
                       'aside':'idle', 'upright':'idle'}
            extra = '\n'
            for alias, anim in aliases.items():
                extra += f' anim {alias} doom3/models/md5/weapons/doublebarrel_view/new/{anim}.md5anim'
                if alias == 'fire':
                    extra += ' { frame 1 sound_weapon d3_ssg_fire }'
                if alias == 'reload':
                    extra += ' { frame 6 sound_body3 d3_ssg_click\n frame 23 sound_voice d3_ssg_shell_insert\n frame 32 sound_voice2 d3_ssg_shell_insert\n frame 45 sound_body2 d3_ssg_clack }'
                extra += '\n'
            pos = decl.index('\n', decl.index('mesh'))+1
            decl = decl[:pos]+extra+decl[pos:]
        decl = re.sub(r'(?m)^\s*frame \d+\s+object_call EjectBrass\s*$', '', decl)
        models.append(decl)
    files['def/doom3_supershotgun_models.def'] = ('\n\n'.join(models)+'\n').encode()
    sound_texts = [text(n) for n in index if n.endswith('.sndshd')]
    sounds = []
    for name in sorted(set(re.findall(r'\b(?:player_shotgun_\w+|ssg_\w+)', weapon))):
        if any(block(data.decode(), 'd3_'+name) for path,data in files.items() if path.endswith('.sndshd')):
            continue
        decl = next((b for t in sound_texts if (b := block(t, name))), None)
        if decl is None:
            raise ValueError('Missing sound: '+name)
        decl = re.sub(r'//[^\n]*', '', decl)
        for asset in re.findall(r'sound/[\w/.-]+\.(?:wav|ogg)', decl):
            candidate = asset if asset.lower() in index else str(Path(asset).with_suffix('.ogg')).replace('\\','/')
            files[prefix+candidate] = read(candidate)
            decl = decl.replace(asset, candidate)
        decl = re.sub(r'\b'+re.escape(name)+r'\b', 'd3_'+name, decl, count=1)
        sounds.append(re.sub(r'(?<![\w/])(sound/[\w/.-]+)', lambda m: prefix+m[1], decl))
    files['sound/doom3_supershotgun.sndshd'] = ('\n\n'.join(sounds)+'\n').encode()
    player = files['def/player.def'].decode()
    player = re.sub(r'(?m)^\s*anim d3supershotgun_\w+[^\r\n]*[\r\n]+', '', player)
    player = re.sub(r'(?m)^(\s*anim )rifle_(\w+)([^\r\n]*)',
                    lambda m: m[0]+'\n'+m[1]+'d3supershotgun_'+m[2]+m[3], player)
    pos = player.index('anim d3supershotgun_raise')
    player = player[:pos]+'anim d3supershotgun_reload models/player/anim/th_rifle_raise.md5anim\n\t'+player[pos:]
    files['def/player.def'] = player.encode()
    files['def/doom3_supershotgun.def'] = Path(__file__).with_name('supershotgun.def').read_bytes()
    files['script/reawakened/weapon_d3supershotgun_v1.script'] = Path(__file__).with_name('weapon_d3supershotgun_v1.script').read_bytes()
    files['script/reawakened/weapon_d3supershotgun_v2.script'] = Path(__file__).with_name('weapon_d3supershotgun_v2.script').read_bytes()
    files['script/reawakened/weapon_d3supershotgun_v3.script'] = Path(__file__).with_name('weapon_d3supershotgun_v3.script').read_bytes()
