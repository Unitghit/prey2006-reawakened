"""Locally import retail Rocket Launcher assets; generated assets are not distributable."""
import re
from pathlib import Path


def import_rocketlauncher(index, read, text, block, files):
    prefix = 'doom3/'
    shaders = set()
    for name in sorted(index):
        if name.startswith(('models/md5/weapons/rocketlauncher_view/', 'models/md5/weapons/rocketlauncher_world/')):
            if name.endswith('.md5mesh'):
                mesh = text(name)
                shaders.update(re.findall(r'shader\s+"([^"]+)"', mesh))
                files[prefix+name] = re.sub(r'(shader\s+")([^"]+)', lambda m: m[1]+prefix+m[2], mesh).encode()
            elif name.endswith('.md5anim'):
                files[prefix+name] = read(name)
    if not shaders:
        raise ValueError('Original Doom 3 Rocket Launcher assets not found')
    from import_rocket_fx import import_rocket_fx
    import_rocket_fx(index, read, text, block, files, shaders)
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
                decl = re.sub(r'\b'+table+r'\b', 'd3rl_'+table, decl)
        declarations.append(decl)
    for table in sorted(tables):
        decl = next((b for t in materials if (b := block(t, table))), None)
        if decl is None:
            raise ValueError('Missing table: '+table)
        declarations.append(re.sub(r'\b'+table+r'\b', 'd3rl_'+table, decl, flags=re.I))
    files['materials/doom3_rocketlauncher.mtr'] = ('\n\n'.join(declarations)+'\n').encode()
    weapon = text('def/weapon_rocketlauncher.def')
    models = []
    for name in ('viewmodel_rocketlauncher','worldmodel_rocketlauncher'):
        decl = block(weapon, name).replace(name, 'd3_'+name)
        decl = re.sub(r'(?<![\w/])(models/[\w/.-]+)', lambda m: prefix+m[1], decl)
        decl = re.sub(r'\b(?:player_rocketlauncher_\w+|rocket_\w+)', lambda m: 'd3_'+m[0], decl)
        if name == 'viewmodel_rocketlauncher':
            aliases = {'initialPickup':'raise', 'raise_mp':'raise',
                       'down':'lower', 'putaway_mp':'lower', 'put_aside':'lower',
                       'aside':'lower', 'upright':'raise'}
            extra = '\n'
            for alias, anim in aliases.items():
                extra += f' anim {alias} doom3/models/md5/weapons/rocketlauncher_view/{anim}.md5anim'
                extra += '\n'
            pos = decl.index('\n', decl.index('mesh'))+1
            decl = decl[:pos]+extra+decl[pos:]
        models.append(decl)
    files['def/doom3_rocketlauncher_models.def'] = ('\n\n'.join(models)+'\n').encode()
    sound_texts = [text(n) for n in index if n.endswith('.sndshd')]
    sounds = []
    for name in sorted(set(re.findall(r'\b(?:player_rocketlauncher_\w+|rocket_(?:flight|impact))', weapon))):
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
    files['sound/doom3_rocketlauncher.sndshd'] = ('\n\n'.join(sounds)+'\n').encode()
    player = files['def/player.def'].decode()
    player = re.sub(r'(?m)^\s*anim d3rocketlauncher_\w+[^\r\n]*[\r\n]+', '', player)
    player = re.sub(r'(?m)^(\s*anim )rifle_(\w+)([^\r\n]*)',
                    lambda m: m[0]+'\n'+m[1]+'d3rocketlauncher_'+m[2]+m[3], player)
    pos = player.index('anim d3rocketlauncher_raise')
    player = player[:pos]+'anim d3rocketlauncher_reload models/player/anim/th_rifle_raise.md5anim\n\t'+player[pos:]
    files['def/player.def'] = player.encode()
    files['def/doom3_rocketlauncher.def'] = Path(__file__).with_name('rocketlauncher.def').read_bytes()
    files['script/reawakened/weapon_d3rocketlauncher_v1.script'] = Path(__file__).with_name('weapon_d3rocketlauncher_v1.script').read_bytes()

    skins = []
    sources = [text(n) for n in index if n.endswith('.skin')]
    for count in range(6):
        name = f'skins/models/weapons/{count}rox'
        decl = next((b for src in sources if (b := block(re.sub(r'\bskin\s+', 'table ', src), name))), None)
        if not decl:
            raise ValueError('Missing rocket ammo skin '+name)
        decl = decl.replace('table '+name, 'skin d3_rocket_'+str(count), 1)
        decl = re.sub(r'(?<![\w/])(models/[\w/.-]+)', r'doom3/\1', decl)
        skins.append(decl)
    files['skins/doom3_rocketlauncher.skin'] = ('\n\n'.join(skins)+'\n').encode()
