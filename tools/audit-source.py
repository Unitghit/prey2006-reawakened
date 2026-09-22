"""Audit tracked/staged source paths before private pushes or public packaging."""
from pathlib import Path
import re
import hashlib
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
names = subprocess.check_output(['git', '-C', str(root), 'ls-files', '-z']).decode().split('\0')
errors = []
secrets = re.compile(rb'(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{50,}|-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----)')
for name in filter(None, names):
    p = root / name
    if p.suffix.lower() in {'.iso', '.pk4', '.save', '.dmp', '.pdb', '.exe', '.dll', '.lib'} or p.name.lower() in {'preykey', 'xpkey', '.env'}:
        errors.append(f'Forbidden file: {name}')
    if name.split('/')[0] in {'validation', 'userdata', 'engine', 'dist', 'build', 'output'}:
        errors.append(f'Runtime/build directory tracked: {name}')
    if p.stat().st_size > 50 * 1024 * 1024:
        errors.append(f'Unexpected large source file: {name}')
    data = p.read_bytes()
    # Unmodified public curl example, not a developer credential. Pin its
    # normalized contents so the exception cannot hide an actual new key.
    example = name == 'neo/libs/Curl/docs/examples/usercertinmem.c' and hashlib.sha256(data.replace(b'\r\n', b'\n')).hexdigest() == '3b06cbbd39991def62288509ad72741982ded786dc1e8978afdc0734d35094fa'
    if secrets.search(data) and not example:
        errors.append(f'Credential-like content: {name}')
if errors:
    print('\n'.join(errors))
    sys.exit(1)
print(f'PASS: {len(list(filter(None, names)))} tracked source files; no forbidden runtime files or detected credentials.')
