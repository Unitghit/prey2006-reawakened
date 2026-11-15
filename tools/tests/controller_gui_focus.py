"""Compile the actual input routing and focus-transition code with small test doubles."""
from pathlib import Path
import subprocess, tempfile
root = Path(__file__).resolve().parents[2]
events = (root / 'neo/sys/events.cpp').read_text()
route = events[events.index('static int guiButtonRoute'):events.index('bool D3_IN_interactiveIngameGuiActive')]
player = (root / 'neo/Prey/game_player.cpp').read_text()
start = player.index('\t// Balance activation when moving directly')
focus = player[start:player.index('\n}\n', start)]
code = route + '''
struct UI { bool active; int changes; const char* Activate(bool a,int) { active=a; ++changes; return ""; } };
struct Entity {};
struct { int time; } gameLocal;
void HandleGuiCommands(Entity*,const char*) {}
void transition(Entity* oldFocus, UI* oldUI, Entity* focusGUIent, UI* focusUI) {
const char* command;
''' + focus + '''
}
#define CHECK(x) if (!(x)) return __LINE__;
int main() {
// Screen closes while A is held: release must still release mouse1.
CHECK(RouteGuiButton(0,true,true)==1);
CHECK(RouteGuiButton(0,false,false)==2);
// Gameplay press followed by GUI focus: release must stay on joystick.
CHECK(RouteGuiButton(1,true,false)==0);
CHECK(RouteGuiButton(1,false,true)==0);
// Two GUI buttons represent one mouse button; only last release clears it.
CHECK(RouteGuiButton(0,true,true)==1);
CHECK(RouteGuiButton(1,true,true)==-1);
CHECK(RouteGuiButton(0,true,false)==-1);
CHECK(RouteGuiButton(0,false,false)==-1);
CHECK(RouteGuiButton(1,false,false)==2);
CHECK(RouteGuiButton(0,true,false)==0);
CHECK(RouteGuiButton(0,false,false)==0);
Entity a,b; UI first={false,0},second={false,0};
transition(0,0,&a,&first); CHECK(first.active);
transition(&a,&first,&b,&second); CHECK(!first.active && second.active);
transition(&b,&second,&b,&first); CHECK(first.active && !second.active);
int changes=first.changes;
transition(&b,&first,&b,&first); CHECK(first.changes==changes);
transition(&b,&first,0,0); CHECK(!first.active && !second.active);
return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    source=Path(tmp)/'test.cpp'; exe=Path(tmp)/'test.exe'
    source.write_text(code)
    subprocess.run(['clang++',str(source),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: focus transitions and controller press/release routing')
