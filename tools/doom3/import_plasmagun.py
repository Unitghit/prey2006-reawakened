"""Locally import retail Plasma Gun assets; generated assets are not distributable."""
import re
from pathlib import Path


def import_plasmagun(index, read, text, block, files):
    prefix = 'doom3/'
    shaders = set()
    for name in sorted(index):
        if name.startswith(('models/md5/weapons/plasmagun_view/', 'models/md5/weapons/plasmagun_world/')):
            if name.endswith('.md5mesh'):
                mesh = text(name)
                shaders.update(re.findall(r'shader\s+"([^"]+)"', mesh))
                files[prefix+name] = re.sub(r'(shader\s+")([^"]+)', lambda m: m[1]+prefix+m[2], mesh).encode()
            elif name.endswith('.md5anim'):
                files[prefix+name] = read(name)
    if not shaders:
        raise ValueError('Original Doom 3 Plasma Gun assets not found')
    gui = text('guis/weapons/plasmagun.gui')
    # This GUI uses Doom's Bank face; retain the paths embedded in its glyph data.
    for name in index:
        if name.startswith('fonts/english/bank/') and name.endswith(('.dat', '.tga')):
            files[name] = read(name)
    shaders.update(re.findall(r'background\s+"([^"]+)"', gui))
    from import_plasma_fx import import_plasma_fx
    import_plasma_fx(index, read, text, block, files, shaders)
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
                decl = re.sub(r'\b'+table+r'\b', 'd3pg_'+table, decl)
        declarations.append(decl)
    for table in sorted(tables):
        decl = next((b for t in materials if (b := block(t, table))), None)
        if decl is None:
            raise ValueError('Missing table: '+table)
        declarations.append(re.sub(r'\b'+table+r'\b', 'd3pg_'+table, decl, flags=re.I))
    files['materials/doom3_plasmagun.mtr'] = ('\n\n'.join(declarations)+'\n').encode()
    gui = re.sub(r'(background\s+")([^"]+)', lambda m: m[1]+prefix+m[2], gui)
    # State names are provided by the engine for this additive weapon only.
    files['guis/reawakened/plasmagun_v1.gui'] = gui.encode()
    weapon = text('def/weapon_plasmagun.def')
    models = []
    for name in ('viewmodel_plasmagun','worldmodel_plasmagun'):
        decl = block(weapon, name).replace(name, 'd3_'+name)
        decl = re.sub(r'(?<![\w/])(models/[\w/.-]+)', lambda m: prefix+m[1], decl)
        decl = re.sub(r'\b(?:player_plasma_\w+|plasma_\w+)', lambda m: 'd3_'+m[0], decl)
        if name == 'viewmodel_plasmagun':
            # GUI aside is a held pose, not a looping holster animation.
            # Tommy's independent GUI hand still handles screen presses.
            aliases = {'initialPickup':'raise', 'raise_mp':'raise', 'fire':'fire1',
                       'down':'lower', 'putaway_mp':'lower', 'put_aside':'idle',
                       'aside':'idle', 'upright':'idle'}
            extra = '\n'
            for alias, anim in aliases.items():
                extra += f' anim {alias} doom3/models/md5/weapons/plasmagun_view/{anim}.md5anim'
                extra += ' { frame 1 sound_weapon d3_plasma_fire }' if alias == 'fire' else ''
                extra += '\n'
            pos = decl.index('\n', decl.index('mesh'))+1
            decl = decl[:pos]+extra+decl[pos:]
        models.append(decl)
    files['def/doom3_plasmagun_models.def'] = ('\n\n'.join(models)+'\n').encode()
    sound_texts = [text(n) for n in index if n.endswith('.sndshd')]
    sounds = []
    for name in sorted(set(re.findall(r'\b(?:player_plasma_\w+|plasma_(?:fire|reload|dryfire|flight|impact))', weapon))):
        if any(block(data.decode(), 'd3_'+name) for path,data in files.items() if path.endswith('.sndshd')):
            continue
        decl = next((b for t in sound_texts if (b := block(t, name))), None)
        if decl is None:
            raise ValueError('Missing sound: '+name)
        for asset in re.findall(r'sound/[\w/.-]+\.(?:wav|ogg)', decl):
            files[prefix+asset] = read(asset)
        decl = re.sub(r'\b'+re.escape(name)+r'\b', 'd3_'+name, decl, count=1)
        sounds.append(re.sub(r'(?<![\w/])(sound/[\w/.-]+)', lambda m: prefix+m[1], decl))
    files['sound/doom3_plasmagun.sndshd'] = ('\n\n'.join(sounds)+'\n').encode()
    player = files['def/player.def'].decode()
    player = re.sub(r'(?m)^\s*anim d3plasmagun_\w+[^\r\n]*[\r\n]+', '', player)
    player = re.sub(r'(?m)^(\s*anim )rifle_(\w+)([^\r\n]*)',
                    lambda m: m[0]+'\n'+m[1]+'d3plasmagun_'+m[2]+m[3], player)
    pos = player.index('anim d3plasmagun_raise')
    player = player[:pos]+'anim d3plasmagun_reload models/player/anim/th_rifle_raise.md5anim\n\t'+player[pos:]
    files['def/player.def'] = player.encode()
    files['def/doom3_plasmagun.def'] = Path(__file__).with_name('plasmagun.def').read_bytes()
    files['script/reawakened/weapon_d3plasmagun_v1.script'] = Path(__file__).with_name('weapon_d3plasmagun_v1.script').read_bytes()
