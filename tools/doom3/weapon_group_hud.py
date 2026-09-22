"""Generate local HUD variants from the user's Prey assets (stdlib only)."""
import re


def recolor_tga(data, variant):
    """Preserve TGA compression, orientation and alpha; recolor only RGB."""
    out = bytearray(data)
    if len(out) < 18 or out[1] != 0 or out[2] not in (2, 10) or out[16] not in (24, 32):
        raise ValueError('Expected a true-color TGA icon')
    stride = out[16] // 8
    remaining = int.from_bytes(out[12:14], 'little') * int.from_bytes(out[14:16], 'little')
    offset = 18 + out[0]

    def pixel(pos):
        if pos + stride > len(out):
            raise ValueError('Truncated TGA icon')
        blue, green, red = out[pos:pos+3]
        # Shift the baked blue fill, retaining pale outlines and alpha.
        if blue > red:
            out[pos:pos+3] = bytes((red, green if variant == 1 else
                                   min(green, red + max(0, green-red)//5), blue))

    while remaining:
        if out[2] == 10:
            if offset >= len(out):
                raise ValueError('Truncated TGA packet')
            packet = out[offset]
            offset += 1
            count = (packet & 127) + 1
            stored = 1 if packet & 128 else count
        else:
            count = stored = remaining
        if count > remaining:
            raise ValueError('Invalid TGA packet length')
        for _ in range(stored):
            pixel(offset)
            offset += stride
        remaining -= count
    return bytes(out)


def generate_hud(prey, files):
    name = 'guis/hud/hud_weaponswitchicons.guifragment'
    gui = prey[name].decode('latin1')
    # The exact window tree, expression registers and events are save ABI.
    # Only change existing background bindings.
    pattern = r'windowDef WeaponSelectionOverlay\s*\{[^{}]*\}'
    match = re.search(pattern, gui)
    if not match:
        raise ValueError('Missing HUD weapon selection overlay')
    window = re.sub(r'(background\s+)"[^"]+"',
                    r'\1"gui::rw_weaponSelectionMaterial"', match[0])
    gui = gui[:match.start()] + window + gui[match.end():]
    source = prey['guis/assets/hud/sw_weapon_selector.tga']
    materials = []
    for variant in (1, 2):
        material = f'reawakened/hud/selection_variant{variant}'
        files[material + '.tga'] = recolor_tga(source, variant)
        materials.append(f'{material}\n{{\n noshadows\n {{\n blend blend\n map {material}.tga\n colored\n }}\n}}')
    icons = ('wrench', 'rifle', 'sw_crawler', 'soulstripper', 'autocannon', 'acidsprayer', 'rocketlauncher')
    for slot, icon in enumerate(icons, 1):
        pattern = rf'windowDef SelectedWeaponGraphic{slot}\s*\{{[^{{}}]*\}}'
        match = re.search(pattern, gui)
        if not match:
            raise ValueError(f'Missing selected HUD weapon {slot}')
        window = re.sub(r'(background\s+)"[^"]+"',
                        rf'\1"gui::rw_weapon{slot}_material"', match[0])
        gui = gui[:match.start()] + window + gui[match.end():]
        source = prey[f'textures/interface/icons/{icon}.tga']
        for variant in (1, 2):
            material = f'reawakened/hud/weapon{slot}_variant{variant}'
            files[material + '.tga'] = recolor_tga(source, variant)
            materials.append(f'{material}\n{{\n noshadows\n {{\n blend blend\n map {material}.tga\n linear\n colored\n }}\n}}')
    files[name] = gui.encode('latin1')
    files['materials/reawakened_weapon_groups.mtr'] = ('\n\n'.join(materials)+'\n').encode()
