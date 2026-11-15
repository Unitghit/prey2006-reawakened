"""Regression-test production GUI bookkeeping and SDL controller dispatch in isolation.

No physical controller or game windows required. Compile the same harness against
the pre-fix revision as a negative control: each regression must fail before the pending fixes.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
BASELINE = 'e28aa1a4281b3be9465cb473414e5f7ddeb6f895'

def source(path, old=False):
    if old:
        return subprocess.check_output(['git', 'show', BASELINE + ':' + path], cwd=ROOT, text=True)
    return (ROOT / path).read_text()

def harness(old=False):
    events = source('neo/sys/events.cpp', old)
    player = source('neo/Prey/game_player.cpp', old)
    route_start = events.index('bool D3_IN_interactiveIngameGuiActive') if old else events.index('static int guiButtonRoute')
    registry = events[route_start:events.index('\nstatic void PushButton', route_start)]
    start = events.index('\t\tcase SDL_CONTROLLERBUTTONDOWN:')
    dispatch = events[start:events.index('\n\t\tcase SDL_CONTROLLERAXISMOTION:', start)]
    start = player.index('void hhPlayer::UpdateFocus')
    end = player.index('\n}\n', start)
    chunk = player[start:end]
    start = chunk.index('\tif ( focusGUIent && focusUI ) {') if old else chunk.index('\t// Balance activation')
    focus = chunk[start:]
    return r'''
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
template<class T> struct idList {
    std::vector<T> v;
    int FindIndex(T t) { for(int i=0;i<(int)v.size();++i) if(v[i]==t) return i; return -1; }
    void Append(T t) { v.push_back(t); }
    void RemoveIndex(int i) { v.erase(v.begin()+i); }
    void Clear() { v.clear(); }
    int Num() { return (int)v.size(); }
};
struct idUserInterface;
struct { bool menu=false; void* GetActiveMenu() { return menu ? this : nullptr; } } sessLocal;
''' + registry + r'''
struct idUserInterface {
    bool active=false; int changes=0;
    const char* Activate(bool a,int) {
        active=a; ++changes;
        Sys_SetInteractiveIngameGuiActive(a,this);
        return "";
    }
};
struct Entity {};
struct { int time=0; } gameLocal;
void HandleGuiCommands(Entity*,const char*) {}
void transition(Entity* oldFocus,idUserInterface* oldUI,Entity* focusGUIent,idUserInterface* focusUI) {
    const char* command;
''' + focus + r'''
}
enum { SDL_CONTROLLERBUTTONDOWN=1, SDL_CONTROLLERBUTTONUP,
       SDL_CONTROLLER_BUTTON_A=0, SDL_CONTROLLER_BUTTON_Y=3, SDL_CONTROLLER_BUTTON_START=6,
       SE_KEY=1, K_ESCAPE=27, K_MOUSE1=100, K_FIRST_JOY=200,
       J_ACTION_FIRST=0, J_ACTION_MAX=20, M_ACTION1=0 };
using SDL_GameControllerButton=int;
using sys_jEvents=int;
int mapjoybutton(int b) { return b; }
struct { bool GetBool() { return true; } } in_useGamepad;
struct Common { void Warning(const char*) {} } commonInstance;
Common* common=&commonInstance;
struct Poll { int key,value; Poll(int k,int v):key(k),value(v) {} };
using mouse_poll_t=Poll; using joystick_poll_t=Poll;
idList<Poll> mouse_polls,joystick_polls;
struct sysEvent_t { int evType=0,evValue=0,evValue2=0; };
#define IS_SDL_BTN_DOWN(b) ((b).down)
sysEvent_t button(int b,bool down) {
    struct { int type; struct { int button; bool down; } cbutton; } ev={down?SDL_CONTROLLERBUTTONDOWN:SDL_CONTROLLERBUTTONUP,{b,down}};
    sysEvent_t res; bool isDown;
    for(int once=0;once<1;++once) { switch(ev.type) {
''' + dispatch + r'''
    } }
    return {};
}
#define CHECK(x) do { if(!(x)) { std::printf("FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(int argc,char** argv) {
    int test=argc>1?std::atoi(argv[1]):0;
    Entity a,b; idUserInterface first,second;
    if(test==0 || test==1) {
        transition(nullptr,nullptr,&a,&first);
        CHECK(D3_IN_interactiveIngameGuiActive);
        transition(&a,&first,test==0?&b:&a,&second);
        transition(test==0?&b:&a,&second,nullptr,nullptr);
        CHECK(!first.active && !second.active);
        CHECK(!D3_IN_interactiveIngameGuiActive);
        CHECK(button(SDL_CONTROLLER_BUTTON_A,true).evValue==K_FIRST_JOY+SDL_CONTROLLER_BUTTON_A);
        CHECK(mouse_polls.Num()==0);
    } else if(test==2 || test==3) {
        for(int b : {SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_Y}) {
            if(test==2) first.Activate(true,0);
            auto press=button(b,true);
            first.Activate(test==3,0);
            auto release=button(b,false);
            CHECK(press.evValue==release.evValue);
            CHECK(press.evValue2==1 && release.evValue2==0);
            CHECK(press.evValue==(test==2?K_MOUSE1:K_FIRST_JOY+b));
            first.Activate(false,0);
        }
        CHECK(mouse_polls.Num()==(test==2?4:0));
        CHECK(joystick_polls.Num()==(test==3?4:0));
    } else if(test==4) {
        first.Activate(true,0);
        CHECK(button(SDL_CONTROLLER_BUTTON_A,true).evValue==K_MOUSE1);
        CHECK(button(SDL_CONTROLLER_BUTTON_Y,true).evType==0);
        first.Activate(false,0);
        CHECK(button(SDL_CONTROLLER_BUTTON_A,false).evType==0);
        auto release=button(SDL_CONTROLLER_BUTTON_Y,false);
        CHECK(release.evValue==K_MOUSE1 && release.evValue2==0);
        CHECK(mouse_polls.Num()==2 && joystick_polls.Num()==0);
    } else if(test==5) {
        first.Activate(true,0);
        auto press=button(SDL_CONTROLLER_BUTTON_A,true);
        sessLocal.menu=true;
        auto release=button(SDL_CONTROLLER_BUTTON_A,false);
        CHECK(press.evValue==release.evValue && release.evValue2==0);
        CHECK(mouse_polls.Num()==2 && joystick_polls.Num()==0);
    } else if(test==6) {
        // Repeated interaction never leaves the GUI registry or mouse press active.
        for(int i=0;i<1000;++i) {
            transition(nullptr,nullptr,&a,&first);
            CHECK(button(SDL_CONTROLLER_BUTTON_A,true).evValue==K_MOUSE1);
            transition(&a,&first,&b,&second);
            transition(&b,&second,nullptr,nullptr);
            CHECK(!D3_IN_interactiveIngameGuiActive);
            auto release=button(SDL_CONTROLLER_BUTTON_A,false);
            CHECK(release.evValue==K_MOUSE1 && release.evValue2==0);
            CHECK(button(SDL_CONTROLLER_BUTTON_Y,true).evValue==K_FIRST_JOY+SDL_CONTROLLER_BUTTON_Y);
            CHECK(button(SDL_CONTROLLER_BUTTON_Y,false).evValue==K_FIRST_JOY+SDL_CONTROLLER_BUTTON_Y);
        }
        CHECK(mouse_polls.Num()==2000 && joystick_polls.Num()==2000);
    }
    return 0;
}
'''

names=['screen-to-screen focus','two GUIs on one entity','GUI press/gameplay release',
       'gameplay press/GUI release','overlapping interaction buttons','menu opens while held',
       '1000 repeated interaction cycles']
with tempfile.TemporaryDirectory() as tmp:
    for old in (True,False):
        cpp=Path(tmp)/'regression.cpp'; exe=Path(tmp)/'regression.exe'
        cpp.write_text(harness(old))
        subprocess.run(['clang++',str(cpp),'-o',str(exe)],check=True)
        for number,name in enumerate(names):
            result=subprocess.run([str(exe),str(number)],capture_output=True,text=True)
            expected=1 if old else 0
            if result.returncode != expected:
                raise RuntimeError(f'{name}: unexpected result {result.returncode}\n{result.stdout}')
            print(('CONFIRMED OLD FAILURE: ' if old else 'PASS: ')+name)
