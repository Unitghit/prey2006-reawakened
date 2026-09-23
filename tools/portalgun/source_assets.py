"""Read selected assets from a user's Source 1 VPK (no assets bundled)."""
from pathlib import Path
import struct
import zlib


class VPK:
    def __init__(self, path):
        self.path = Path(path)
        data = self.path.read_bytes()
        signature, version, tree_size = struct.unpack_from('<III', data)
        if signature != 0x55AA1234 or version not in (1, 2):
            raise ValueError('Unsupported VPK')
        header = 12 if version == 1 else 28
        self.data_start = header + tree_size
        self.entries = {}
        cursor = header
        def string():
            nonlocal cursor
            end = data.index(b'\0', cursor)
            value = data[cursor:end].decode('utf8')
            cursor = end + 1
            return value
        while (ext := string()):
            while (folder := string()):
                while (name := string()):
                    crc, pre, archive, offset, length, term = struct.unpack_from('<IHHIIH', data, cursor)
                    cursor += 18
                    if term != 0xffff:
                        raise ValueError('Invalid VPK entry')
                    preload = data[cursor:cursor+pre]
                    cursor += pre
                    key = ((folder+'/' if folder != ' ' else '')+name+'.'+ext).lower()
                    self.entries[key] = (crc, archive, offset, length, preload)

    def read(self, name):
        crc, archive, offset, length, preload = self.entries[name.lower()]
        path = self.path if archive == 0x7fff else self.path.with_name(self.path.name.replace('_dir.vpk', f'_{archive:03d}.vpk'))
        with path.open('rb') as file:
            file.seek(offset + (self.data_start if archive == 0x7fff else 0))
            result = preload + file.read(length)
        if zlib.crc32(result) != crc:
            raise ValueError('VPK checksum mismatch: '+name)
        return result

    def extract(self, name, output):
        dest = Path(output)/name
        if not dest.resolve().is_relative_to(Path(output).resolve()):
            raise ValueError('Unsafe asset path')
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(self.read(name))
        return dest
