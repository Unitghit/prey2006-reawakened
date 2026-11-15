"""Check owner routing through the base-entity sound API, including old saved IDs."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[2]
s=(r/'neo/Prey/prey_baseweapons.cpp').read_text()
a=s.index('void hhWeapon::UpdateSound( void )');b=s.index('\n}',a)+2
method=s[a:b]
entity_decl=next(line for line in (r/'neo/game/Entity.h').read_text().splitlines() if 'UpdateSound( void );' in line)
weapon_decl=next(line for line in (r/'neo/Prey/prey_baseweapons.h').read_text().splitlines() if 'UpdateSound( void ) override;' in line)
code='''
#include <string>
struct { bool GetBool() { return false; } } g_debugWeapon;
struct { void Printf(const char*,...) {} } gameLocal;
struct Player { int listener; int GetListenerId() { return listener; } };
struct Owner { Player* p=nullptr; bool IsValid() { return p!=nullptr; } Player* operator->() {return p;} };
struct idEntity { int entityNumber=2024; struct {int listenerId=2025;} refSound; int emittedListener=-1;
'''+entity_decl+'''
};
void idEntity::UpdateSound() { emittedListener=refSound.listenerId; }
struct hhWeapon: idEntity { Owner owner; std::string name="rifle";
'''+weapon_decl+'''\n};\n'''+method+'''
int main() {
 hhWeapon weapon; Player local{1},remote{2}; idEntity* base=&weapon;
 weapon.owner.p=&local; base->UpdateSound(); if(weapon.emittedListener!=1) return 1;
 weapon.refSound.listenerId=2025; base->UpdateSound(); if(weapon.emittedListener!=1) return 2;
 weapon.owner.p=&remote; base->UpdateSound(); if(weapon.emittedListener!=2) return 3;
 weapon.owner.p=nullptr; base->UpdateSound(); if(weapon.emittedListener!=2025) return 4;
 return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'test.cpp';exe=Path(tmp)/'test.exe';p.write_text(code)
 subprocess.run(['clang++',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: owned weapon, old save, remote owner, unowned emitter, base API dispatch')
