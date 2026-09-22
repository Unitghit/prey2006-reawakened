"""Import a user's retail Doom 3 shotgun assets into a local Prey mod.

Only this importer is distributable. Generated files contain retail assets.
No archives from Doom 3 are mounted wholesale or copied into Prey's base folder.
"""
import argparse
import hashlib
import json
import re
import zipfile
from pathlib import Path


def block(text, name):
    match = re.search(r'(?m)^\s*(?:model\s+|table\s+)?' + re.escape(name) + r'\s*\{', text)
    if not match:
        return None
    end, depth = match.end(), 1
    while depth and end < len(text):
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    if depth:
        raise ValueError('Unterminated declaration: ' + name)
    return text[match.start():end].strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('install', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--prey-base', action='append', type=Path, required=True,
                        help='Prey base directories in ascending precedence order')
    parser.add_argument('--save-compatible', action='store_true',
                        help='Install additive assets in base, retaining the normal campaign script baseline')
    args = parser.parse_args()
    index, archives = {}, []
    for archive in sorted((args.install / 'base').glob('pak*.pk4')):
        z = zipfile.ZipFile(archive)
        archives.append(z)
        for name in z.namelist():
            if not name.endswith('/'):
                index[name.lower()] = (z, name)
    def read(name):
        z, original = index[name.lower()]
        return z.read(original)
    def text(name):
        return re.sub(r'/\*.*?\*/|//[^\n]*', '', read(name).decode('latin1').replace('\r', ''), flags=re.S)
    files = {}
    prefix = 'doom3/'
    shader_names = set()
    for name in sorted(index):
        if name.startswith(('models/md5/weapons/shotgun_view/', 'models/md5/weapons/shotgun_world/')):
            if name.endswith('.md5mesh'):
                mesh = text(name)
                shader_names.update(re.findall(r'shader\s+"([^"]+)"', mesh))
                mesh = re.sub(r'(shader\s+")([^"]+)', lambda m: m[1]+prefix+m[2], mesh)
                files[prefix+name] = mesh.encode()
            elif name.endswith('.md5anim'):
                files[prefix+name] = read(name)
    if not shader_names:
        raise ValueError('Original Doom 3 shotgun assets not found')
    materials = [text(n) for n in index if n.endswith('.mtr')]
    declarations = []
    for shader in sorted(shader_names):
        decl = next((b for t in materials if (b := block(t, shader))), None)
        if decl is None:
            raise ValueError('Missing material: ' + shader)
        # Export-only high-poly references are not shipped and are unnecessary.
        decl = re.sub(r'(?m)^\s*renderbump[^\n]*', '', decl)
        decl = re.sub(r'(?m)^\s*ricochet\s*$', '', decl)
        for asset in re.findall(r'[\w/.-]+\.(?:tga|dds)', decl):
            files[prefix+asset] = read(asset)
        decl = re.sub(r'(?<![\w/])((?:models|textures)/[\w/.-]+)', lambda m: prefix+m[1], decl)
        for table in ('table8','table32','rotate90'):
            decl = re.sub(r'\b'+table+r'\b', 'd3_'+table, decl)
        declarations.append(decl)
    for table in ('table8','table32','rotate90'):
        decl = next((b for t in materials if (b := block(t, table))), None)
        if decl is None:
            raise ValueError('Missing material table: '+table)
        declarations.append(re.sub(r'\b'+table+r'\b', 'd3_'+table, decl))
    files['materials/doom3_shotgun.mtr'] = ('\n\n'.join(declarations)+'\n').encode()
    for name in index:
        if name.startswith('sound/weapons/shotgun/'):
            files[prefix+name] = read(name)
    # Preserve original animation frame sound events, with namespaced shaders.
    weapon_def = text('def/weapon_shotgun.def')
    models = []
    for name in ('viewmodel_shotgun','worldmodel_shotgun'):
        decl = block(weapon_def, name)
        decl = decl.replace(name, 'd3_'+name)
        decl = re.sub(r'(?<![\w/])(models/[\w/.-]+)', lambda m: prefix+m[1], decl)
        decl = re.sub(r'\bplayer_shotgun_', 'd3_player_shotgun_', decl)
        if name == 'viewmodel_shotgun':
            aliases = {'initialPickup':'raise', 'raise_mp':'raise', 'fire':'fire1',
                       'reload_loop':'reload_loop', 'down':'lower', 'putaway_mp':'lower',
                       'put_aside':'lower', 'aside':'lower', 'upright':'raise'}
            extra = '\n'
            for alias, animation in aliases.items():
                extra += f' anim {alias} doom3/models/md5/weapons/shotgun_view/{animation}.md5anim'
                if alias == 'fire':
                    extra += ' { frame 1 sound_weapon d3_player_shotgun_fire\n frame 17 sound_voice d3_player_shotgun_pump }'
                if alias == 'reload_loop':
                    extra += ' { frame 4 sound d3_player_shotgun_reload }'
                extra += '\n'
            pos = decl.index('\n', decl.index('mesh')) + 1
            decl = decl[:pos] + extra + decl[pos:]
        models.append(decl)
    files['def/doom3_shotgun_models.def'] = ('\n\n'.join(models)+'\n').encode()
    sounds = []
    sound_texts = [text(n) for n in index if n.endswith('.sndshd')]
    for name in sorted(set(re.findall(r'\bplayer_shotgun_\w+', weapon_def))):
        decl = next((b for t in sound_texts if (b := block(t, name))), None)
        if decl is None:
            raise ValueError('Missing sound shader: '+name)
        for asset in re.findall(r'sound/[\w/.-]+\.(?:wav|ogg)',decl):
            files[prefix+asset] = read(asset)
        decl = decl.replace(name, 'd3_'+name)
        decl = re.sub(r'(?<![\w/])(sound/[\w/.-]+)', lambda m: prefix+m[1], decl)
        sounds.append(decl)
    files['sound/doom3_shotgun.sndshd'] = ('\n\n'.join(sounds)+'\n').encode()
    prey = {}
    for base in args.prey_base:
        for archive in sorted(base.glob('*.pk4')):
            with zipfile.ZipFile(archive) as z:
                for name in ('script/prey_main.script', 'def/player.def'):
                    if name in z.namelist():
                        prey[name] = z.read(name).decode('latin1')
        for name in ('script/prey_main.script', 'def/player.def'):
            if (base/name).is_file():
                prey[name] = (base/name).read_text()
    main_script = prey['script/prey_main.script']
    files['script/prey_main.script'] = (main_script+'\n#include "script/weapon_d3shotgun.script"\n').encode()
    player = prey['def/player.def']
    if not args.save_compatible:
        player, count = re.subn(r'("def_weapon8"\s+)""', r'\1"weaponobj_d3shotgun"', player)
        if count != 1:
            raise ValueError('Expected exactly one unused player weapon slot 8')
        for key in ('weapon8_cycle','weapon8_best','weapon8_allowempty'):
            player = re.sub(r'("'+key+r'"\s+)"0"', r'\1"1"', player)
    # Reuse Tommy's rifle poses for the new world weapon, rather than falling
    # back to missing unprefixed fire/reload animations.
    player = re.sub(r'(?m)^(\s*anim )rifle_(\w+)([^\r\n]*)',
                    lambda m: m[0]+'\n'+m[1]+'d3shotgun_'+m[2]+m[3], player)
    marker = 'anim d3shotgun_raise'
    pos = player.index(marker)
    player = player[:pos] + 'anim d3shotgun_reload models/player/anim/th_rifle_raise.md5anim\n\t' + player[pos:]
    files['def/player.def'] = player.encode()
    files['script/weapon_d3shotgun.script'] = Path(__file__).with_name('weapon_d3shotgun.script').read_bytes()
    files['def/doom3_shotgun.def'] = Path(__file__).with_name('shotgun.def').read_bytes()
    files['description.txt'] = b'Doom 3 shotgun prototype'
    if args.save_compatible:
        # Never change the baseline script. Keep the old script path byte-for-
        # byte for prototype saves, and a versioned path for new additive saves.
        del files['script/prey_main.script']
        del files['description.txt']
        files['script/reawakened/weapon_d3shotgun_v1.script'] = Path(__file__).with_name('weapon_d3shotgun_v1.script').read_bytes()
        definition = files['def/doom3_shotgun.def'].decode()
        definition = definition.replace('entityDef weaponobj_d3shotgun {',
            'entityDef weaponobj_d3shotgun {\n    "rw_saveCompatible" "1"\n'
            '    "rw_addonScript" "script/reawakened/weapon_d3shotgun_v1.script"')
        files['def/doom3_shotgun.def'] = definition.encode()

    # Validate everything before writing; no path may escape the output folder.
    for name in files:
        if Path(name).is_absolute() or '..' in Path(name).parts:
            raise ValueError('Unsafe asset path: '+name)
    for name, data in files.items():
        dest = args.output/name
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)
    manifest = {n: {'bytes':len(d), 'sha256':hashlib.sha256(d).hexdigest()} for n,d in sorted(files.items())}
    (args.output/('doom3-import-manifest.json' if args.save_compatible else 'import-manifest.json')).write_text(json.dumps(manifest,indent=2))
    print(f'Imported {len(files)} files, {sum(map(len,files.values()))/1048576:.1f} MiB to {args.output}')
    for z in archives:
        z.close()


if __name__ == '__main__':
    main()
