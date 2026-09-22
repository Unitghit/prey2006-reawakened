"""Locally import retail Machine Gun assets; generated assets are not distributable."""
import re
from pathlib import Path


def import_machinegun(index, read, text, block, files):
    prefix = 'doom3/'
    shaders = set()
    for name in sorted(index):
        if name.startswith(('models/md5/weapons/machinegun_view/', 'models/md5/weapons/machinegun_world/')):
            if name.endswith('.md5mesh'):
                mesh = text(name)
                shaders.update(re.findall(r'shader\s+"([^"]+)"', mesh))
                files[prefix+name] = re.sub(r'(shader\s+")([^"]+)', lambda m: m[1]+prefix+m[2], mesh).encode()
            elif name.endswith('.md5anim'):
                files[prefix+name] = read(name)
    if not shaders:
        raise ValueError('Original Doom 3 Machine Gun assets not found')
    gui = text('guis/weapons/machinegun.gui')
    shaders.update(re.findall(r'background\s+"([^"]+)"', gui))
    materials = [text(n) for n in index if n.endswith('.mtr')]
    declarations = []
    tables = set()
    for shader in sorted(shaders):
        if block(files['materials/doom3_shotgun.mtr'].decode(), prefix+shader):
            continue
        decl = next((b for t in materials if (b := block(t, shader))), None)
        if decl is None:
            asset = shader if shader.endswith('.tga') else shader+'.tga'
            files[prefix+asset] = read(asset)
            # Match an implicit GUI material: honor window tint and alpha.
            decl = shader+' {\n { blend blend\n map '+asset+'\n colored\n clamp\n }\n}'
        decl = re.sub(r'(?m)^\s*(?:renderbump[^\n]*|ricochet\s*)$', '', decl)
        for asset in re.findall(r'[\w/.-]+\.(?:tga|dds)', decl):
            files[prefix+asset] = read(asset)
        decl = re.sub(r'(?<![\w/])((?:models|textures|guis|gui)/[\w/.-]+)', lambda m: prefix+m[1], decl)
        for table in sorted(set(re.findall(r'\b(\w+)\s*\[', decl))):
            if re.search(r'\b'+table+r'\b', decl):
                tables.add(table)
                decl = re.sub(r'\b'+table+r'\b', 'd3mg_'+table, decl)
        declarations.append(decl)
    for table in sorted(tables):
        decl = next((b for t in materials if (b := block(t, table))), None)
        if decl is None:
            raise ValueError('Missing table: '+table)
        declarations.append(re.sub(r'\b'+table+r'\b', 'd3mg_'+table, decl, flags=re.I))
    files['materials/doom3_machinegun.mtr'] = ('\n\n'.join(declarations)+'\n').encode()
    gui = re.sub(r'(background\s+")([^"]+)', lambda m: m[1]+prefix+m[2], gui)
    # State names are provided by the engine for this additive weapon only.
    files['guis/reawakened/machinegun_v1.gui'] = gui.encode()
    weapon = text('def/weapon_machinegun.def')
    models = []
    for name in ('viewmodel_machinegun','worldmodel_machinegun'):
        decl = block(weapon, name).replace(name, 'd3_'+name)
        decl = re.sub(r'(?<![\w/])(models/[\w/.-]+)', lambda m: prefix+m[1], decl)
        decl = re.sub(r'\bplayer_machinegun_', 'd3_player_machinegun_', decl)
        if name == 'viewmodel_machinegun':
            aliases = {'initialPickup':'pullup', 'raise_mp':'pullup', 'fire':'fire4',
                       'down':'putaway', 'putaway_mp':'putaway', 'put_aside':'putaway',
                       'aside':'putaway', 'upright':'pullup'}
            extra = '\n'
            for alias, anim in aliases.items():
                extra += f' anim {alias} doom3/models/md5/weapons/machinegun_view/{anim}.md5anim'
                if alias == 'fire':
                    extra += ' { frame 1 sound_weapon d3_player_machinegun_fire\n frame 1 sound_voice2 d3_player_machinegun_mech }'
                extra += '\n'
            pos = decl.index('\n', decl.index('mesh'))+1
            decl = decl[:pos]+extra+decl[pos:]
        models.append(decl)
    files['def/doom3_machinegun_models.def'] = ('\n\n'.join(models)+'\n').encode()
    sound_texts = [text(n) for n in index if n.endswith('.sndshd')]
    sounds = []
    for name in sorted(set(re.findall(r'\bplayer_machinegun_\w+', weapon))):
        decl = next((b for t in sound_texts if (b := block(t, name))), None)
        if decl is None:
            raise ValueError('Missing sound: '+name)
        for asset in re.findall(r'sound/[\w/.-]+\.(?:wav|ogg)', decl):
            files[prefix+asset] = read(asset)
        decl = decl.replace(name, 'd3_'+name)
        sounds.append(re.sub(r'(?<![\w/])(sound/[\w/.-]+)', lambda m: prefix+m[1], decl))
    files['sound/doom3_machinegun.sndshd'] = ('\n\n'.join(sounds)+'\n').encode()
    player = files['def/player.def'].decode()
    player = re.sub(r'(?m)^\s*anim d3machinegun_\w+[^\r\n]*[\r\n]+', '', player)
    player = re.sub(r'(?m)^(\s*anim )rifle_(\w+)([^\r\n]*)',
                    lambda m: m[0]+'\n'+m[1]+'d3machinegun_'+m[2]+m[3], player)
    pos = player.index('anim d3machinegun_raise')
    player = player[:pos]+'anim d3machinegun_reload models/player/anim/th_rifle_raise.md5anim\n\t'+player[pos:]
    files['def/player.def'] = player.encode()
    files['def/doom3_machinegun.def'] = Path(__file__).with_name('machinegun.def').read_bytes()
    files['script/reawakened/weapon_d3machinegun_v1.script'] = Path(__file__).with_name('weapon_d3machinegun_v1.script').read_bytes()
