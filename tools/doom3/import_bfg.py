"""Locally import retail BFG assets; generated assets are not distributable."""
import re
from pathlib import Path


def import_bfg(index, read, text, block, files):
    prefix = 'doom3/'
    shaders = set()
    for name in sorted(index):
        if name.startswith(('models/md5/weapons/bfg_view/', 'models/md5/weapons/bfg_world/')):
            if name.endswith('.md5mesh'):
                mesh = text(name)
                shaders.update(re.findall(r'shader\s+"([^"]+)"', mesh))
                files[prefix+name] = re.sub(r'(shader\s+")([^"]+)', lambda m: m[1]+prefix+m[2], mesh).encode()
            elif name.endswith('.md5anim'):
                files[prefix+name] = read(name)
    if not shaders:
        raise ValueError('Original Doom 3 BFG assets not found')
    from import_bfg_fx import import_bfg_fx
    import_bfg_fx(index, read, text, block, files, shaders)
    gui = text('guis/weapons/bfg.gui')
    shaders.update(re.findall(r'background\s+"([^"]+)"', gui))
    materials = [re.sub(r'}(?=[A-Za-z_])', '}\n', text(n)) for n in index if n.endswith('.mtr')]
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
        for asset in re.findall(r'(?:models|textures|guis|gui)/[\w/.-]+', decl):
            candidate = asset if asset.lower() in index else asset+'.tga'
            if candidate.lower() in index: files[prefix+candidate] = read(candidate)
        decl = re.sub(r'(?<![\w/])((?:models|textures|guis|gui)/[\w/.-]+)', lambda m: prefix+m[1], decl)
        for table in sorted(set(re.findall(r'\b(\w+)\s*\[', decl))):
            if re.search(r'\b'+table+r'\b', decl):
                tables.add(table)
                decl = re.sub(r'\b'+table+r'\b', 'd3bfg_'+table, decl)
        declarations.append(decl)
    for table in sorted(tables):
        decl = next((b for t in materials if (b := block(t, table))), None)
        if decl is None:
            raise ValueError('Missing table: '+table)
        declarations.append(re.sub(r'\b'+table+r'\b', 'd3bfg_'+table, decl, flags=re.I))
    files['materials/doom3_bfg.mtr'] = ('\n\n'.join(declarations)+'\n').encode()
    gui = re.sub(r'(background\s+")([^"]+)', lambda m: m[1]+prefix+m[2], gui)
    # State names are provided by the engine for this additive weapon only.
    files['guis/reawakened/bfg_v1.gui'] = gui.encode()
    weapon = text('def/weapon_bfg.def')
    models = []
    for name in ('viewmodel_bfg','worldmodel_bfg'):
        decl = block(weapon, name).replace(name, 'd3_'+name)
        decl = re.sub(r'(?<![\w/])(models/[\w/.-]+)', lambda m: prefix+m[1], decl)
        decl = re.sub(r'\bplayer_bfg_', 'd3_player_bfg_', decl)
        if name == 'viewmodel_bfg':
            # GUI aside is a held pose, not a looping holster animation.
            # Tommy's independent GUI hand still handles screen presses.
            aliases = {'initialPickup':'raise', 'raise_mp':'raise', 'down':'lower',
                       'putaway_mp':'lower', 'put_aside':'idle', 'aside':'idle', 'upright':'idle'}
            extra = ''.join(f' anim {alias} doom3/models/md5/weapons/bfg_view/{anim}.md5anim\n' for alias,anim in aliases.items())
            pos = decl.index('\n', decl.index('mesh'))+1
            decl = decl[:pos]+extra+decl[pos:]
        models.append(decl)
    files['def/doom3_bfg_models.def'] = ('\n\n'.join(models)+'\n').encode()
    sound_texts = [text(n) for n in index if n.endswith('.sndshd')]
    sounds = []
    for name in sorted(set(re.findall(r'\bplayer_bfg_\w+', weapon))):
        decl = next((b for t in sound_texts if (b := block(t, name))), None)
        if decl is None:
            raise ValueError('Missing sound: '+name)
        for asset in re.findall(r'sound/[\w/.-]+\.(?:wav|ogg)', decl):
            files[prefix+asset] = read(asset)
        decl = decl.replace(name, 'd3_'+name)
        sounds.append(re.sub(r'(?<![\w/])(sound/[\w/.-]+)', lambda m: prefix+m[1], decl))
    files['sound/doom3_bfg.sndshd'] = ('\n\n'.join(sounds)+'\n').encode()
    player = files['def/player.def'].decode()
    player = re.sub(r'(?m)^\s*anim d3bfg_\w+[^\r\n]*[\r\n]+', '', player)
    player = re.sub(r'(?m)^(\s*anim )rifle_(\w+)([^\r\n]*)',
                    lambda m: m[0]+'\n'+m[1]+'d3bfg_'+m[2]+m[3], player)
    pos = player.index('anim d3bfg_raise')
    player = player[:pos]+'anim d3bfg_reload models/player/anim/th_rifle_raise.md5anim\n\t'+player[pos:]
    files['def/player.def'] = player.encode()
    files['def/doom3_bfg.def'] = Path(__file__).with_name('bfg.def').read_bytes()
    files['script/reawakened/weapon_d3bfg_v1.script'] = Path(__file__).with_name('weapon_d3bfg_v1.script').read_bytes()
