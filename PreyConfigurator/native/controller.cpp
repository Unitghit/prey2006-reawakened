#include "controller.h"
#include <commctrl.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#define SDL_MAIN_HANDLED
#include "SDL.h"

// ---------------------------------------------------------------------------
// Bindings in the game configuration
// ---------------------------------------------------------------------------

struct PadAction { const wchar_t* label; const char* command; };
static const PadAction Actions[] = {
    {L"Move forward","_forward"},{L"Move back","_back"},{L"Move left","_moveleft"},{L"Move right","_moveright"},
    {L"Look up","_lookup"},{L"Look down","_lookdown"},{L"Turn left","_left"},{L"Turn right","_right"},
    {L"Attack","_attack"},{L"Alternate attack","_attackalt"},{L"Jump","_moveup"},{L"Crouch","_movedown"},
    {L"Reload","_impulse13"},{L"Toggle lighter","_impulse16"},{L"Spirit walk","_impulse54"},{L"Throw crawler grenade","_impulse25"},
    {L"Previous weapon","_impulse15"},{L"Next weapon","_impulse14"},
    {L"Weapon slot 1","_impulse1"},{L"Weapon slot 2","_impulse2"},{L"Weapon slot 3","_impulse3"},{L"Weapon slot 4","_impulse4"},
    {L"Weapon slot 5","_impulse5"},{L"Weapon slot 6","_impulse6"},{L"Weapon slot 7","_impulse7"},{L"Weapon slot 8","_impulse8"},
    {L"Weapon slot 9","_impulse9"},{L"Center view","_impulse18"},{L"Quick save","savegame quick"},{L"Quick load","loadgame quick"},
};

// Engine key names in SDL game controller button order (SDL_GameControllerButton);
// nullptr marks buttons that cannot be bound (Guide, Start opens the menu, Touchpad).
static const char* ButtonKeys[] = {
    "JOY_BTN_SOUTH","JOY_BTN_EAST","JOY_BTN_WEST","JOY_BTN_NORTH","JOY_BTN_BACK",nullptr,nullptr,
    "JOY_BTN_LSTICK","JOY_BTN_RSTICK","JOY_BTN_LSHOULDER","JOY_BTN_RSHOULDER",
    "JOY_DPAD_UP","JOY_DPAD_DOWN","JOY_DPAD_LEFT","JOY_DPAD_RIGHT",
    "JOY_BTN_MISC1","JOY_BTN_RPADDLE1","JOY_BTN_LPADDLE1","JOY_BTN_RPADDLE2","JOY_BTN_LPADDLE2",nullptr,
};

PadBindings DefaultPadBindings() {
    return {
        {"JOY_STICK1_UP","_forward"},{"JOY_STICK1_DOWN","_back"},{"JOY_STICK1_LEFT","_moveleft"},{"JOY_STICK1_RIGHT","_moveright"},
        {"JOY_STICK2_UP","_lookup"},{"JOY_STICK2_DOWN","_lookdown"},{"JOY_STICK2_LEFT","_left"},{"JOY_STICK2_RIGHT","_right"},
        {"JOY_TRIGGER2","_attack"},{"JOY_TRIGGER1","_attackalt"},
        {"JOY_BTN_SOUTH","_moveup"},{"JOY_BTN_EAST","_movedown"},{"JOY_BTN_RSTICK","_movedown"},
        {"JOY_BTN_WEST","_impulse16"},{"JOY_BTN_NORTH","_impulse54"},{"JOY_BTN_LSTICK","_impulse25"},
        {"JOY_BTN_LSHOULDER","_impulse15"},{"JOY_BTN_RSHOULDER","_impulse14"},
        {"JOY_DPAD_UP","_impulse1"},{"JOY_DPAD_RIGHT","_impulse2"},{"JOY_DPAD_DOWN","_impulse3"},{"JOY_DPAD_LEFT","_impulse4"},
    };
}

static fs::path ConfigFile(const fs::path& root) { return root/L"userdata/base/prey06.cfg"; }

static std::string Lower(std::string s) { std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return (char)tolower(c);}); return s; }

// One quoted or bare token from a configuration line.
static bool Token(const std::string& line, size_t& pos, std::string& out) {
    while (pos < line.size() && (line[pos]==' ' || line[pos]=='\t')) ++pos;
    if (pos >= line.size()) return false;
    out.clear();
    if (line[pos]=='"') { size_t end=line.find('"',pos+1); if(end==std::string::npos) return false; out=line.substr(pos+1,end-pos-1); pos=end+1; }
    else { size_t end=line.find_first_of(" \t\r",pos); if(end==std::string::npos) end=line.size(); out=line.substr(pos,end-pos); pos=end; }
    return true;
}

// A controller binding line: bind "JOY_..." "command"; key is upper case.
static bool PadBindLine(const std::string& line, std::string* key, std::string* command) {
    size_t pos=0; std::string word,k,c;
    if (!Token(line,pos,word) || Lower(word)!="bind" || !Token(line,pos,k)) return false;
    std::transform(k.begin(),k.end(),k.begin(),[](unsigned char ch){return (char)toupper(ch);});
    if (k.rfind("JOY_",0)!=0) return false;
    Token(line,pos,c);
    if (key) *key=k;
    if (command) *command=c;
    return true;
}

static std::vector<std::string> ConfigLines(const fs::path& root) {
    std::vector<std::string> lines;
    std::ifstream in(LongPath(ConfigFile(root)),std::ios::binary);
    std::string line;
    while (std::getline(in,line)) { if(!line.empty() && line.back()=='\r') line.pop_back(); lines.push_back(line); }
    return lines;
}

PadBindings ReadPadBindings(const fs::path& root, bool* isDefault) {
    PadBindings bindings;
    for (const auto& line : ConfigLines(root)) {
        std::string key,command;
        if (PadBindLine(line,&key,&command) && !command.empty()) bindings[key]=command;
    }
    if (isDefault) *isDefault=bindings.empty();
    // The game applies the default layout to a configuration with no controller bindings.
    return bindings.empty() ? DefaultPadBindings() : bindings;
}

void WritePadBindings(const fs::path& root, const PadBindings& bindings) {
    std::string out;
    for (const auto& line : ConfigLines(root)) if (!PadBindLine(line,nullptr,nullptr)) out+=line+"\r\n";
    for (const auto& b : bindings) if (!b.second.empty()) out+="bind \""+b.first+"\" \""+b.second+"\"\r\n";
    fs::create_directories(LongPath(ConfigFile(root).parent_path()));
    Atomic(ConfigFile(root),out);
}

// ---------------------------------------------------------------------------
// Controller input through the game's own SDL2.dll
// ---------------------------------------------------------------------------

struct Sdl {
    HMODULE module{};
    int (SDLCALL *Init)(Uint32){};
    void (SDLCALL *QuitSubSystem)(Uint32){};
    SDL_bool (SDLCALL *SetHint)(const char*,const char*){};
    int (SDLCALL *NumJoysticks)(void){};
    SDL_bool (SDLCALL *IsGameController)(int){};
    SDL_GameController* (SDLCALL *Open)(int){};
    void (SDLCALL *Close)(SDL_GameController*){};
    const char* (SDLCALL *Name)(SDL_GameController*){};
    SDL_GameControllerType (SDLCALL *Type)(SDL_GameController*){};
    SDL_GameController* (SDLCALL *FromInstance)(SDL_JoystickID){};
    int (SDLCALL *PollEvent)(SDL_Event*){};
    SDL_RWops* (SDLCALL *RWFromFile)(const char*,const char*){};
    int (SDLCALL *AddMappingsFromRW)(SDL_RWops*,int){};
    bool ready=false;

    bool Load(const fs::path& root) {
        module=LoadLibraryExW(LongPath(root/EngineDirectory/L"SDL2.dll").c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!module) return false;
        auto get=[&](auto& fn, const char* name){ fn=reinterpret_cast<std::remove_reference_t<decltype(fn)>>(GetProcAddress(module,name)); return fn!=nullptr; };
        if (!(get(Init,"SDL_Init") && get(QuitSubSystem,"SDL_QuitSubSystem") && get(SetHint,"SDL_SetHint") && get(NumJoysticks,"SDL_NumJoysticks") &&
              get(IsGameController,"SDL_IsGameController") && get(Open,"SDL_GameControllerOpen") && get(Close,"SDL_GameControllerClose") &&
              get(Name,"SDL_GameControllerName") && get(Type,"SDL_GameControllerGetType") && get(FromInstance,"SDL_GameControllerFromInstanceID") &&
              get(PollEvent,"SDL_PollEvent"))) return false;
        // Same button positions as the game; input arrives while the launcher has focus.
        SetHint("SDL_GAMECONTROLLER_USE_BUTTON_LABELS","0");
        SetHint("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS","1");
        ready = Init(SDL_INIT_GAMECONTROLLER)==0;
        // The same community mappings the game loads (SDL_GameControllerDB), so the
        // window recognises every controller the game does.
        if (ready && get(RWFromFile,"SDL_RWFromFile") && get(AddMappingsFromRW,"SDL_GameControllerAddMappingsFromRW"))
            if (SDL_RWops* db=RWFromFile(Utf8((root/EngineDirectory/L"gamecontrollerdb.txt").wstring()).c_str(),"rb")) AddMappingsFromRW(db,1);
        return ready;
    }
    ~Sdl() { if(ready) QuitSubSystem(SDL_INIT_GAMECONTROLLER); if(module) FreeLibrary(module); }
};

enum class PadStyle { Xbox, PlayStation, Nintendo };

static PadStyle StyleOf(SDL_GameControllerType type) {
    switch ((int)type) {
        case SDL_CONTROLLER_TYPE_PS3: case SDL_CONTROLLER_TYPE_PS4: case SDL_CONTROLLER_TYPE_PS5: return PadStyle::PlayStation;
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO: case 11: case 12: case 13: return PadStyle::Nintendo; // Joy-Cons (SDL 2.24)
        default: return PadStyle::Xbox;
    }
}

// Player-facing name of an engine key for the connected controller's style; the
// same names as the game's binding menus (Sys_GetLocalizedJoyKeyName).
static std::wstring KeyLabel(const std::string& key, PadStyle style) {
    static const std::map<std::string, const wchar_t*> common = {
        {"JOY_DPAD_UP",L"D-pad up"},{"JOY_DPAD_DOWN",L"D-pad down"},{"JOY_DPAD_LEFT",L"D-pad left"},{"JOY_DPAD_RIGHT",L"D-pad right"},
        {"JOY_STICK1_UP",L"L stick up"},{"JOY_STICK1_DOWN",L"L stick down"},{"JOY_STICK1_LEFT",L"L stick left"},{"JOY_STICK1_RIGHT",L"L stick right"},
        {"JOY_STICK2_UP",L"R stick up"},{"JOY_STICK2_DOWN",L"R stick down"},{"JOY_STICK2_LEFT",L"R stick left"},{"JOY_STICK2_RIGHT",L"R stick right"},
        {"JOY_BTN_RPADDLE1",L"Paddle P1"},{"JOY_BTN_LPADDLE1",L"Paddle P3"},{"JOY_BTN_RPADDLE2",L"Paddle P2"},{"JOY_BTN_LPADDLE2",L"Paddle P4"},
    };
    static const std::map<std::string, const wchar_t*> xbox = {
        {"JOY_BTN_SOUTH",L"A button"},{"JOY_BTN_EAST",L"B button"},{"JOY_BTN_WEST",L"X button"},{"JOY_BTN_NORTH",L"Y button"},{"JOY_BTN_BACK",L"View button"},
        {"JOY_BTN_LSTICK",L"L stick click"},{"JOY_BTN_RSTICK",L"R stick click"},{"JOY_BTN_LSHOULDER",L"LB"},{"JOY_BTN_RSHOULDER",L"RB"},
        {"JOY_TRIGGER1",L"LT"},{"JOY_TRIGGER2",L"RT"},{"JOY_BTN_MISC1",L"Share button"},
    };
    static const std::map<std::string, const wchar_t*> playstation = {
        {"JOY_BTN_SOUTH",L"Cross"},{"JOY_BTN_EAST",L"Circle"},{"JOY_BTN_WEST",L"Square"},{"JOY_BTN_NORTH",L"Triangle"},{"JOY_BTN_BACK",L"Share"},
        {"JOY_BTN_LSTICK",L"L3"},{"JOY_BTN_RSTICK",L"R3"},{"JOY_BTN_LSHOULDER",L"L1"},{"JOY_BTN_RSHOULDER",L"R1"},
        {"JOY_TRIGGER1",L"L2"},{"JOY_TRIGGER2",L"R2"},{"JOY_BTN_MISC1",L"Mute"},
    };
    static const std::map<std::string, const wchar_t*> nintendo = {
        {"JOY_BTN_SOUTH",L"B button"},{"JOY_BTN_EAST",L"A button"},{"JOY_BTN_WEST",L"Y button"},{"JOY_BTN_NORTH",L"X button"},{"JOY_BTN_BACK",L"- button"},
        {"JOY_BTN_LSTICK",L"L stick click"},{"JOY_BTN_RSTICK",L"R stick click"},{"JOY_BTN_LSHOULDER",L"L"},{"JOY_BTN_RSHOULDER",L"R"},
        {"JOY_TRIGGER1",L"ZL"},{"JOY_TRIGGER2",L"ZR"},{"JOY_BTN_MISC1",L"Capture"},
    };
    const auto& names = style==PadStyle::PlayStation ? playstation : style==PadStyle::Nintendo ? nintendo : xbox;
    if (auto it=names.find(key); it!=names.end()) return it->second;
    if (auto it=common.find(key); it!=common.end()) return it->second;
    return Wide(key);
}

// ---------------------------------------------------------------------------
// Window
// ---------------------------------------------------------------------------

// IDOK (1) and IDCANCEL (2) are reserved for OK, Cancel, Enter and Esc.
static constexpr int ListId=10, ChangeId=11, ClearId=12, ResetId=13, OkId=IDOK, CancelId=IDCANCEL;
static constexpr UINT_PTR PollTimer=1;
static constexpr int AxisRest=12000, AxisPressed=22000;   // of 32767
static constexpr wchar_t DialogClass[]=L"PreyControllerDialog";

struct ControllerDialog {
    fs::path root;
    HWND window{},list{},device{},hint{},change{},clear{},reset{},ok{},cancel{};
    HFONT font{};
    Sdl sdl;
    std::vector<SDL_GameController*> pads;
    SDL_GameController* active{};
    PadStyle style=PadStyle::Xbox;
    PadBindings bindings;
    int listening=-1;                 // action index waiting for a controller input
    DWORD listenStart=0;
    Sint16 axes[SDL_CONTROLLER_AXIS_MAX]{};    // latest value of every axis
    bool armed[SDL_CONTROLLER_AXIS_MAX]{};     // axis returned near its rest position since listening began
    bool saved=false,done=false,changed=false;
    UINT dpi=96;
    int S(int v) const { return MulDiv(v,(int)dpi,96); }

    std::wstring Keys(const char* command) const {
        std::wstring text;
        for (const auto& b : bindings) if (b.second==command) { if(!text.empty()) text+=L", "; text+=KeyLabel(b.first,style); }
        return text;
    }
    void Fill() {
        for (int i=0;i<(int)(sizeof(Actions)/sizeof(Actions[0]));++i) {
            std::wstring keys = i==listening ? L"Press a button..." : Keys(Actions[i].command);
            if (keys.empty()) keys=L"-";
            ListView_SetItemText(list,i,1,const_cast<wchar_t*>(keys.c_str()));
        }
    }
    void ShowDevice() {
        std::wstring text;
        if (!sdl.ready) text=L"Controller support could not be loaded (engine\\SDL2.dll). Bindings can still be cleared or reset.";
        else if (!active) text=L"No controller detected. Connect one; it appears here automatically.";
        else {
            const char* name=sdl.Name(active);
            text=L"Controller: "+(name?Wide(name):std::wstring(L"Game controller"))+
                (style==PadStyle::PlayStation?L"  (PlayStation buttons)":style==PadStyle::Nintendo?L"  (Nintendo buttons)":L"  (Xbox buttons)");
        }
        SetWindowTextW(device,text.c_str());
    }
    void Use(SDL_GameController* pad) {
        active=pad;
        style = pad ? StyleOf(sdl.Type(pad)) : PadStyle::Xbox;
        ShowDevice(); Fill();
    }
    void OpenPads() {
        for (int i=0;i<sdl.NumJoysticks();++i) if (sdl.IsGameController(i)) if (auto pad=sdl.Open(i)) if (std::find(pads.begin(),pads.end(),pad)==pads.end()) pads.push_back(pad);
        Use(pads.empty()?nullptr:pads.back());
    }
    void Assign(const std::string& key) {
        const char* command=Actions[listening].command;
        // One action per input, and the pressed input becomes this action's only binding.
        for (auto it=bindings.begin();it!=bindings.end();) it = (it->first==key || it->second==command) ? bindings.erase(it) : std::next(it);
        bindings[key]=command;
        int done_=listening; listening=-1; changed=true;
        Fill(); ListView_SetItemState(list,done_,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
        SetHint(L"Assigned. Select another action to change it, or click OK to save.");
        EnableButtons();
    }
    void SetHint(const wchar_t* text) { SetWindowTextW(hint,text); }
    int Selected() const { return ListView_GetNextItem(list,-1,LVNI_SELECTED); }
    void EnableButtons() {
        bool any=Selected()>=0;
        EnableWindow(change,any && sdl.ready && active && listening<0);
        EnableWindow(clear,any && listening<0);
    }
    void Listen() {
        int i=Selected(); if (i<0 || !sdl.ready || !active) return;
        listening=i; listenStart=GetTickCount();
        // A stick or trigger is accepted once it moves from near rest past the threshold;
        // one already held when Change is clicked must be released first.
        for (int a=0;a<SDL_CONTROLLER_AXIS_MAX;++a) armed[a]=std::abs((int)axes[a])<AxisRest;
        Fill(); EnableButtons();
        SetHint(L"Press a button, trigger or stick direction on the controller. Esc cancels.");
    }
    void StopListening() { if(listening<0) return; listening=-1; Fill(); EnableButtons(); SetHint(L"Cancelled."); }
    void Poll() {
        if (!sdl.ready) return;
        SDL_Event ev;
        while (sdl.PollEvent(&ev)) {
            if (ev.type==SDL_CONTROLLERDEVICEADDED) {
                if (auto pad=sdl.Open(ev.cdevice.which)) { if(std::find(pads.begin(),pads.end(),pad)==pads.end()) pads.push_back(pad); Use(pad); }
            } else if (ev.type==SDL_CONTROLLERDEVICEREMOVED) {
                if (auto pad=sdl.FromInstance(ev.cdevice.which)) { pads.erase(std::remove(pads.begin(),pads.end(),pad),pads.end()); sdl.Close(pad); if(active==pad) { if(listening>=0) StopListening(); Use(pads.empty()?nullptr:pads.back()); } }
            } else if (ev.type==SDL_CONTROLLERBUTTONDOWN) {
                auto pad=sdl.FromInstance(ev.cbutton.which);
                if (pad && pad!=active) Use(pad);           // the controller in hand becomes the one shown
                if (listening>=0 && GetTickCount()-listenStart>150 && ev.cbutton.button<sizeof(ButtonKeys)/sizeof(ButtonKeys[0]) && ButtonKeys[ev.cbutton.button])
                    Assign(ButtonKeys[ev.cbutton.button]);
            } else if (ev.type==SDL_CONTROLLERAXISMOTION && ev.caxis.axis<SDL_CONTROLLER_AXIS_MAX) {
                // Axis motion arrives in many small steps, so the threshold is crossed
                // gradually; compare against the rest state, not the previous step.
                const int a=ev.caxis.axis; const Sint16 value=ev.caxis.value; axes[a]=value;
                if (std::abs((int)value)<AxisRest) { armed[a]=true; continue; }
                if (listening<0 || !armed[a] || std::abs((int)value)<AxisPressed) continue;
                armed[a]=false;
                const bool neg=value<0;
                switch (ev.caxis.axis) {
                    case SDL_CONTROLLER_AXIS_LEFTX: Assign(neg?"JOY_STICK1_LEFT":"JOY_STICK1_RIGHT"); break;
                    case SDL_CONTROLLER_AXIS_LEFTY: Assign(neg?"JOY_STICK1_UP":"JOY_STICK1_DOWN"); break;
                    case SDL_CONTROLLER_AXIS_RIGHTX: Assign(neg?"JOY_STICK2_LEFT":"JOY_STICK2_RIGHT"); break;
                    case SDL_CONTROLLER_AXIS_RIGHTY: Assign(neg?"JOY_STICK2_UP":"JOY_STICK2_DOWN"); break;
                    case SDL_CONTROLLER_AXIS_TRIGGERLEFT: if(!neg) Assign("JOY_TRIGGER1"); break;
                    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: if(!neg) Assign("JOY_TRIGGER2"); break;
                }
            }
        }
    }
    void Command(int id) {
        if (id==ChangeId) Listen();
        else if (id==ClearId) {
            int i=Selected(); if(i<0) return;
            for (auto it=bindings.begin();it!=bindings.end();) it = it->second==Actions[i].command ? bindings.erase(it) : std::next(it);
            changed=true; Fill(); SetHint(L"Cleared.");
        } else if (id==ResetId) { StopListening(); bindings=DefaultPadBindings(); changed=true; Fill(); SetHint(L"Default layout selected. Click OK to save."); }
        else if (id==OkId) {
            if (changed) {
                // An empty set would make the game apply the default layout again.
                if (bindings.empty()) { MessageBoxW(window,L"Assign at least one controller button, or click Reset to default.",L"Controller",MB_OK|MB_ICONINFORMATION); return; }
                WritePadBindings(root,bindings); saved=true;
            }
            done=true;
        } else if (id==CancelId) { if(listening>=0) { StopListening(); return; } done=true; }
    }
    void Layout() {
        RECT c; GetClientRect(window,&c);
        const int m=S(12), bh=S(28), bw=S(130), w=c.right-2*m;
        int y=m;
        SIZE s{}; HDC dc=GetDC(window); auto old=SelectObject(dc,font); GetTextExtentPoint32W(dc,L"Ag",2,&s); SelectObject(dc,old); ReleaseDC(window,dc);
        const int line=s.cy+4;
        MoveWindow(device,m,y,w,line*2,TRUE); y+=line*2+4;
        int bottom=c.bottom-m-bh, hintTop=bottom-line*2-6;
        MoveWindow(list,m,y,w-bw-m,std::max(S(60),hintTop-y-S(6)),TRUE);
        int bx=c.right-m-bw;
        MoveWindow(change,bx,y,bw,bh,TRUE); MoveWindow(clear,bx,y+bh+S(6),bw,bh,TRUE); MoveWindow(reset,bx,y+2*(bh+S(6)),bw,bh,TRUE);
        MoveWindow(hint,m,hintTop,w,line*2,TRUE);
        MoveWindow(cancel,c.right-m-S(100),bottom,S(100),bh,TRUE); MoveWindow(ok,c.right-m-S(206),bottom,S(100),bh,TRUE);
        ListView_SetColumnWidth(list,0,LVSCW_AUTOSIZE_USEHEADER);
        RECT lr; GetClientRect(list,&lr); int first=std::max(S(160),(int)(lr.right*0.5)); ListView_SetColumnWidth(list,0,first); ListView_SetColumnWidth(list,1,std::max(80,(int)lr.right-first));
    }
};

static LRESULT CALLBACK DialogProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    auto d=reinterpret_cast<ControllerDialog*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if (msg==WM_NCCREATE) { d=(ControllerDialog*)((CREATESTRUCTW*)lp)->lpCreateParams; d->window=h; SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)d); }
    if (!d) return DefWindowProcW(h,msg,wp,lp);
    try {
        switch (msg) {
            case WM_SIZE: if(d->list) d->Layout(); return 0;
            case WM_TIMER: if(wp==PollTimer) d->Poll(); return 0;
            case WM_COMMAND: if(HIWORD(wp)==BN_CLICKED) d->Command(LOWORD(wp)); return 0;
            case WM_NOTIFY: {
                auto nm=(NMHDR*)lp;
                if (nm->idFrom==ListId && nm->code==LVN_ITEMCHANGED) d->EnableButtons();
                if (nm->idFrom==ListId && nm->code==NM_DBLCLK) d->Listen();
                return 0;
            }
            case WM_GETMINMAXINFO: ((MINMAXINFO*)lp)->ptMinTrackSize={d->S(520),d->S(480)}; return 0;
            case WM_CTLCOLORSTATIC: SetBkMode((HDC)wp,TRANSPARENT); return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
            case WM_CLOSE: d->listening=-1; d->done=true; return 0;
        }
    } catch (const std::exception& e) { MessageBoxW(h,ErrorText(e.what()).c_str(),L"Controller",MB_OK|MB_ICONERROR); }
    return DefWindowProcW(h,msg,wp,lp);
}

bool RunControllerDialog(HINSTANCE instance, HWND owner, HFONT font, const fs::path& root) {
    static bool registered=false;
    if (!registered) {
        WNDCLASSEXW wc{sizeof(wc)}; wc.lpfnWndProc=DialogProc; wc.hInstance=instance; wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        wc.hbrBackground=GetSysColorBrush(COLOR_BTNFACE); wc.lpszClassName=DialogClass;
        wc.hIcon=(HICON)LoadImageW(instance,MAKEINTRESOURCEW(1),IMAGE_ICON,0,0,LR_DEFAULTSIZE);
        RegisterClassExW(&wc); registered=true;
    }
    ControllerDialog d; d.root=root; d.bindings=ReadPadBindings(root);
    d.font=font;
    using DpiFn=UINT(WINAPI*)(HWND);
    auto dpiFn=reinterpret_cast<DpiFn>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow"));
    d.dpi=dpiFn?dpiFn(owner):96;
    RECT o; GetWindowRect(owner,&o); const int w=d.S(640), hgt=d.S(600);
    HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME|WS_EX_CONTROLPARENT,DialogClass,L"Controller",WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_CLIPCHILDREN,
        o.left+((o.right-o.left)-w)/2,o.top+((o.bottom-o.top)-hgt)/2,w,hgt,owner,nullptr,instance,&d);
    if (!h) throw std::runtime_error("Cannot create controller window");
    auto make=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int id){ HWND c=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,0,0,1,1,h,(HMENU)(INT_PTR)id,instance,nullptr); SendMessageW(c,WM_SETFONT,(WPARAM)d.font,TRUE); return c; };
    d.device=make(L"STATIC",L"",SS_NOPREFIX,0);
    d.list=make(WC_LISTVIEWW,L"",WS_TABSTOP|WS_BORDER|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS|LVS_NOSORTHEADER,ListId);
    ListView_SetExtendedListViewStyle(d.list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
    LVCOLUMNW col{LVCF_TEXT|LVCF_WIDTH}; col.cx=220; col.pszText=const_cast<wchar_t*>(L"Action"); ListView_InsertColumn(d.list,0,&col);
    col.pszText=const_cast<wchar_t*>(L"Controller"); ListView_InsertColumn(d.list,1,&col);
    for (int i=0;i<(int)(sizeof(Actions)/sizeof(Actions[0]));++i) { LVITEMW it{LVIF_TEXT}; it.iItem=i; it.pszText=const_cast<wchar_t*>(Actions[i].label); ListView_InsertItem(d.list,&it); }
    d.change=make(L"BUTTON",L"Change...",WS_TABSTOP|BS_PUSHBUTTON,ChangeId);
    d.clear=make(L"BUTTON",L"Clear",WS_TABSTOP|BS_PUSHBUTTON,ClearId);
    d.reset=make(L"BUTTON",L"Reset to default",WS_TABSTOP|BS_PUSHBUTTON,ResetId);
    d.hint=make(L"STATIC",L"Select an action and click Change (or double-click it), then press a button on the controller. Start opens the game menu and cannot be assigned.",SS_NOPREFIX,0);
    d.ok=make(L"BUTTON",L"OK",WS_TABSTOP|BS_DEFPUSHBUTTON,OkId);
    d.cancel=make(L"BUTTON",L"Cancel",WS_TABSTOP|BS_PUSHBUTTON,CancelId);
    d.Fill(); d.Layout();
    if (d.sdl.Load(root)) d.OpenPads(); else d.ShowDevice();
    d.EnableButtons();
    SetTimer(h,PollTimer,20,nullptr);
    EnableWindow(owner,FALSE); ShowWindow(h,SW_SHOW); SetFocus(d.list);
    MSG msg;
    while (!d.done && GetMessageW(&msg,nullptr,0,0)>0) {
        if (msg.message==WM_KEYDOWN && msg.wParam==VK_ESCAPE && d.listening>=0) { d.StopListening(); continue; }
        if (!IsDialogMessageW(h,&msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (msg.message==WM_QUIT) PostQuitMessage((int)msg.wParam);
    KillTimer(h,PollTimer);
    for (auto pad : d.pads) d.sdl.Close(pad);
    EnableWindow(owner,TRUE); DestroyWindow(h); SetForegroundWindow(owner);
    return d.saved;
}

void VerifyControllerBindings(const fs::path& output) {
    auto require=[](bool ok,const char* message){ if(!ok) throw std::runtime_error(message); };
    const fs::path root=output/(L"controller Unicode 雪 "+std::to_wstring(GetTickCount64()));
    fs::create_directories(root/L"userdata/base");
    bool isDefault=false;
    require(ReadPadBindings(root,&isDefault)==DefaultPadBindings() && isDefault,"Missing configuration did not give the default layout");
    Atomic(ConfigFile(root),"unbindall\r\nbind \"w\" \"_forward\"\r\nbind \"JOY_BTN_SOUTH\" \"_impulse18\"\r\nbind \"joy_trigger2\" \"_attack\"\r\nseta s_volume_dB \"0\"\r\n");
    auto b=ReadPadBindings(root,&isDefault);
    require(!isDefault && b.size()==2 && b["JOY_BTN_SOUTH"]=="_impulse18" && b["JOY_TRIGGER2"]=="_attack","Configured controller bindings not read");
    b.erase("JOY_TRIGGER2"); b["JOY_BTN_EAST"]="savegame quick";
    WritePadBindings(root,b);
    std::ifstream in(LongPath(ConfigFile(root)),std::ios::binary); std::string text((std::istreambuf_iterator<char>(in)),{});
    require(text.find("bind \"w\" \"_forward\"")!=std::string::npos && text.find("seta s_volume_dB")!=std::string::npos,"Other configuration lines lost");
    require(text.find("joy_trigger2")==std::string::npos && text.find("bind \"JOY_BTN_EAST\" \"savegame quick\"")!=std::string::npos,"Controller bindings not replaced");
    require(ReadPadBindings(root)==b,"Written controller bindings differ");
    require(KeyLabel("JOY_BTN_SOUTH",PadStyle::PlayStation)==L"Cross" && KeyLabel("JOY_BTN_SOUTH",PadStyle::Nintendo)==L"B button" && KeyLabel("JOY_TRIGGER2",PadStyle::Xbox)==L"RT","Button labels wrong");
    std::error_code ec; fs::remove_all(root,ec);
    Atomic(output/L"controller-pass.txt","PASS: default layout without bindings, reading/replacing controller bindings while keeping other lines, labels per controller style.\n");
}
