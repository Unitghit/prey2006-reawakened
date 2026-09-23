"""Import namespaced retail Rocket Launcher projectile art into a local install."""
import re
import struct


def import_rocket_fx(index, read, text, block, files, shaders):
    path = 'models/weapons/rocketlauncher/rocket.lwo'
    data = read(path)
    if data[:4] != b'FORM' or data[8:12] != b'LWO2':
        raise ValueError('Expected LWO2 rocket')

    def string_at(buf, start):
        end = buf.index(b'\0', start)
        return buf[start:end], end + 1 + ((end + 1 - start) % 2)

    def name_bytes(name):
        name = name.replace(b'\\', b'/')
        shaders.add(name.decode('ascii'))
        result = b'doom3/' + name + b'\0'
        return result + b'\0' * (len(result) % 2)

    chunks = []
    offset = 12
    while offset < len(data):
        kind = data[offset:offset+4]
        size = struct.unpack('>I', data[offset+4:offset+8])[0]
        payload = data[offset+8:offset+8+size]
        if kind == b'TAGS':
            names, pos = [], 0
            while pos < len(payload):
                name, pos = string_at(payload, pos)
                names.append(name_bytes(name))
            payload = b''.join(names)
        elif kind == b'SURF':
            name, pos = string_at(payload, 0)
            payload = name_bytes(name) + payload[pos:]
        chunks.append(kind + struct.pack('>I', len(payload)) + payload + b'\0' * (len(payload) % 2))
        offset += 8 + size + size % 2
    body = b'LWO2' + b''.join(chunks)
    files['doom3/'+path] = b'FORM' + struct.pack('>I', len(body)) + body

    sources = [text(n) for n in index if n.endswith(('.prt', '.def'))]
    particles = []
    for name in ('rockettrail', 'rocketExplosion'):
        decl = next((d for src in sources if (d := block(re.sub(r'\bparticle\s+', 'table ', src), name))), None)
        if not decl:
            raise ValueError('Missing particle '+name)
        decl = decl.replace('table '+name, 'particle d3_'+name, 1)
        shaders.update(re.findall(r'\bmaterial\s+"?([\w/.-]+)', decl))
        decl = re.sub(r'(\bmaterial\s+"?)([\w/.-]+)', r'\1doom3/\2', decl)
        particles.append(decl)
    files['particles/doom3_rocket.prt'] = ('\n\n'.join(particles)+'\n').encode()
    files['fx/doom3_rocket.fx'] = b'''fx fx/d3_rockettrail {
 { name "trail" particle "d3_rockettrail.prt" duration 1000000 restart 1 trackOrigin 1 useAxis normal }
}
fx fx/d3_rocketExplosion {
 { name "impact" model "d3_rocketExplosion.prt" duration 3 restart 0 }
}
'''
