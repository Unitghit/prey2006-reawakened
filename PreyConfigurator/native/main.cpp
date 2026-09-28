#include "settings.h"
#include "setup.h"
#include "games.h"
#include <commctrl.h>
#include <shellapi.h>
#include <algorithm>
#include <fstream>
#include <stdexcept>

static HINSTANCE instance;
static constexpr int RestoreId=2001, SaveId=2002, PlayId=2003, ContentId=2004, AdvancedId=2005;
static constexpr wchar_t MainClass[]=L"PreySettingsNative", PageClass[]=L"PreySettingsPage";
static UINT WindowDpi(HWND window) {
    using Fn=UINT(WINAPI*)(HWND);
    auto fn=reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow"));
    if (fn) return fn(window);
    HDC dc=GetDC(window); UINT dpi=GetDeviceCaps(dc,LOGPIXELSX); ReleaseDC(window,dc); return dpi?dpi:96;
}
// Hide a window from the screen while it still paints normally (DWM cloaking), so
// it can be shown and fully drawn first, then revealed in one step. dwmapi is a
// system library; if it is unavailable the window simply shows as before.
static void Cloak(HWND window,BOOL cloak) {
    using Fn=HRESULT(WINAPI*)(HWND,DWORD,LPCVOID,DWORD);
    static Fn fn=[]{HMODULE m=LoadLibraryW(L"dwmapi.dll");return m?reinterpret_cast<Fn>(GetProcAddress(m,"DwmSetWindowAttribute")):nullptr;}();
    if(fn) fn(window,13/*DWMWA_CLOAK*/,&cloak,sizeof(cloak));
}
static RECT WorkArea(HWND window) {
    MONITORINFO info{sizeof(info)};
    GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&info);
    return info.rcWork;
}
struct Row { HWND label{},combo{}; RECT labelRect{},comboRect{}; };
struct Group { std::wstring name; HWND title{}; RECT rect{}; };
struct App {
    fs::path root;
    HWND window{},page{},title{},status{},tooltip{},buttons[4]{},advanced{};
    bool showAdvanced=false;
    HFONT font{},bold{},heading{};
    UINT dpi=96;
    std::vector<Row> rows;
    std::vector<Group> groups;
    std::vector<RECT> buttonRects;
    int minWidth=680,minHeight=450;
    int scroll=0,contentHeight=0,pageHeight=0,wheelRemainder=0,footerHeight=0;
    bool layingOut=false;
    std::wstring statusText=L"Latest improvements selected by default.";
    explicit App(fs::path r):root(std::move(r)){}
    ~App() { DeleteObject(font); DeleteObject(bold); DeleteObject(heading); }
    // Match the legacy form: fonts follow DPI, while layout gutters stay compact.
    int D(int v) const { return v; }
    SIZE Measure(const std::wstring& text, HFONT f, int width=0) const {
        HDC dc=GetDC(window); auto old=SelectObject(dc,f);
        RECT r{0,0,width,0};
        DrawTextW(dc,text.c_str(),(int)text.size(),&r,DT_CALCRECT|DT_NOPREFIX|(width?DT_WORDBREAK:DT_SINGLELINE));
        SelectObject(dc,old); ReleaseDC(window,dc); return {r.right-r.left,r.bottom-r.top};
    }
    HWND Control(const wchar_t* cls,const std::wstring& text,DWORD style,HWND parent,int id=0) {
        HWND h=CreateWindowExW(0,cls,text.c_str(),WS_CHILD|WS_VISIBLE|style,0,0,1,1,parent,reinterpret_cast<HMENU>((INT_PTR)id),instance,nullptr);
        if (!h) throw std::runtime_error("Cannot create settings control");
        return h;
    }
    void Fonts(UINT newDpi) {
        dpi=newDpi;
        NONCLIENTMETRICSW metrics{sizeof(metrics)};
        using Fn=BOOL(WINAPI*)(UINT,UINT,PVOID,UINT,UINT);
        auto fn=reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"SystemParametersInfoForDpi"));
        LOGFONTW lf{};
        if (fn && fn(SPI_GETNONCLIENTMETRICS,sizeof(metrics),&metrics,0,dpi)) lf=metrics.lfMessageFont;
        else {
            SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(metrics),&metrics,0);
            lf=metrics.lfMessageFont; HDC dc=GetDC(nullptr); int systemDpi=GetDeviceCaps(dc,LOGPIXELSY); ReleaseDC(nullptr,dc);
            lf.lfHeight=MulDiv(lf.lfHeight,(int)dpi,systemDpi?systemDpi:96);
        }
        if (!lf.lfHeight) { lf.lfHeight=-MulDiv(9,(int)dpi,72); wcscpy_s(lf.lfFaceName,L"Segoe UI"); }
        HFONT oldFont=font,oldBold=bold,oldHeading=heading;
        font=CreateFontIndirectW(&lf); lf.lfWeight=FW_BOLD; bold=CreateFontIndirectW(&lf);
        lf.lfHeight=-MulDiv(16,(int)dpi,72); heading=CreateFontIndirectW(&lf);
        if (!font || !bold || !heading) throw std::runtime_error("Cannot create settings font");
        auto set=[](HWND h,HFONT f) { if(h) SendMessageW(h,WM_SETFONT,(WPARAM)f,TRUE); };
        set(title,heading); set(status,font);
        for (auto h:buttons) set(h,font);
        set(advanced,font);
        for (auto& row:rows) {set(row.label,font);set(row.combo,font);}
        for (auto& group:groups) set(group.title,bold);
        DeleteObject(oldFont);DeleteObject(oldBold);DeleteObject(oldHeading);
    }
    void Tip(HWND h,const std::wstring& text) {
        if(text.empty()) return;
        TOOLINFOW ti{sizeof(ti)}; ti.uFlags=TTF_IDISHWND|TTF_SUBCLASS;ti.hwnd=window;ti.uId=(UINT_PTR)h;ti.lpszText=const_cast<wchar_t*>(text.c_str());
        SendMessageW(tooltip,TTM_ADDTOOLW,0,(LPARAM)&ti);
    }
    void Create();
    bool Visible(size_t i) const { return !Options()[i].advanced || showAdvanced; }
    bool GroupVisible(const Group& g) const {
        for (size_t i=0;i<rows.size();++i) if(Options()[i].group==g.name && Visible(i)) return true;
        return false;
    }
    void ShowAdvanced(bool show) {
        showAdvanced=show;SendMessageW(advanced,BM_SETCHECK,show?BST_CHECKED:BST_UNCHECKED,0);
        for (size_t i=0;i<rows.size();++i) {int cmd=Visible(i)?SW_SHOWNA:SW_HIDE;ShowWindow(rows[i].label,cmd);ShowWindow(rows[i].combo,cmd);}
        for (auto& g:groups) ShowWindow(g.title,GroupVisible(g)?SW_SHOWNA:SW_HIDE);
        Layout();
    }
    void SetValues(const Values& v) {
        auto shown=v.find(L"showAdvanced");ShowAdvanced(shown!=v.end() && shown->second==L"1");
        for (size_t i=0;i<rows.size();++i) {
            const auto& s=Options()[i];
            auto it=std::find_if(s.choices.begin(),s.choices.end(),[&](const Choice& c){return c.value==v.at(s.key);});
            SendMessageW(rows[i].combo,CB_SETCURSEL,it-s.choices.begin(),0);
        }
        Dependencies();
    }
    Values Read() const {
        Values v;
        for (size_t i=0;i<rows.size();++i) {
            int selection=(int)SendMessageW(rows[i].combo,CB_GETCURSEL,0,0);
            if(selection<0 || selection>=(int)Options()[i].choices.size()) throw std::runtime_error("Missing settings choice");
            v[Options()[i].key]=Options()[i].choices[selection].value;
        }
        if(showAdvanced) v[L"showAdvanced"]=L"1";
        return v;
    }
    void Dependencies() {
        Values v=Read();
        for (size_t i=0;i<rows.size();++i) {
            const auto& key=Options()[i].key; bool enabled=true;
            if(key==L"resolution") enabled=v[L"r_fullscreen"]!=L"desktop";
            if(key==L"joy_invertLook" || key==L"joy_deadZone") enabled=v[L"in_useGamepad"]==L"1";
            if(key==L"g_portalGunReticle") enabled=v[L"g_portalGun"]==L"1";
            if(key==L"g_halfLifeAutoHop") enabled=v[L"g_bunnyHop"]==L"3" || v[L"g_bunnyHop"]==L"4";
            // Needs the optional Doom 3 content; offered again once it is installed.
            if(key==L"weaponPack" && !ExtraInstalled(root,Extra::Doom3)) {enabled=false;SendMessageW(rows[i].combo,CB_SETCURSEL,0,0);}
            EnableWindow(rows[i].combo,enabled);
        }
    }
    void Status(const std::wstring& text) { statusText=text;SetWindowTextW(status,text.c_str());Layout(); }
    int Content(int width,bool place) {
        int y=0,inner=std::max(D(100),width-D(24));
        int headerHeight=Measure(L"Prey2006 Reawakened Launcher",heading,width).cy;
        if(place) MoveWindow(title,0,y,width,headerHeight,TRUE);
        y+=headerHeight+D(12);
        size_t i=0;
        for (auto& group:groups) {
            if(!GroupVisible(group)) {
                while(i<rows.size() && Options()[i].group==group.name) ++i;
                group.rect={0,y,width,y};continue;
            }
            int top=y; y+=D(12);
            int titleHeight=Measure(group.name,bold).cy;
            if(place) MoveWindow(group.title,D(12),y,inner,titleHeight,TRUE);
            y+=titleHeight+D(10);
            while(i<rows.size() && Options()[i].group==group.name) {
                if(!Visible(i)) {++i;continue;}
                auto& row=rows[i];
                int leftWidth=(int)(inner*.46),labelWidth=std::max(1,leftWidth-D(15));
                int labelHeight=Measure(Options()[i].label,font,labelWidth).cy;
                int comboHeight=Measure(L"Ag",font).cy+D(8);
                int rowHeight=std::max(labelHeight+D(16),comboHeight+D(8));
                row.labelRect={D(15),y+(rowHeight-labelHeight)/2,D(15)+labelWidth,y+(rowHeight+labelHeight)/2};
                row.comboRect={D(12)+leftWidth+D(3),y+(rowHeight-comboHeight)/2,width-D(15),y+(rowHeight+comboHeight)/2};
                if(place) {
                    const auto& a=row.labelRect; const auto& b=row.comboRect;
                    MoveWindow(row.label,a.left,a.top-scroll,a.right-a.left,a.bottom-a.top,TRUE);
                    MoveWindow(row.combo,b.left,b.top-scroll,std::max<int>(1,b.right-b.left),D(260),TRUE);
                }
                y+=rowHeight; ++i;
            }
            y+=D(12);group.rect={0,top,width,y};
            if(place) { RECT r;GetWindowRect(group.title,&r);MapWindowPoints(nullptr,page,(POINT*)&r,2);MoveWindow(group.title,r.left,r.top-scroll,r.right-r.left,r.bottom-r.top,TRUE); }
            y+=D(14);
        }
        if(place) MoveWindow(title,0,-scroll,width,headerHeight,TRUE);
        return y;
    }
    int Footer(int width,bool place,int top) {
        int statusHeight=Measure(statusText,font,std::max(1,width)).cy;
        int y=statusHeight+D(3),x=0,rowHeight=0;
        const wchar_t* texts[]={L"Restore defaults",L"Save settings",L"Save & Play",L"Game content..."};
        buttonRects.clear();
        {
            SIZE size=Measure(L"Show advanced options",font);int w=size.cx+MulDiv(28,(int)dpi,96),h=size.cy+D(18);
            if(place) MoveWindow(advanced,D(18),top+y,w,h,TRUE);
            x=w+D(10);rowHeight=h;
        }
        for(int i=0;i<4;++i) {
            SIZE size=Measure(texts[i],font);int w=size.cx+D(22),h=size.cy+D(18);
            if(x && x+w>width) {x=0;y+=rowHeight+D(3);rowHeight=0;}
            buttonRects.push_back({D(18)+x,top+y,D(18)+x+w,top+y+h});
            if(place) MoveWindow(buttons[i],D(18)+x,top+y,w,h,TRUE);
            x+=w+D(4);rowHeight=std::max(rowHeight,h);
        }
        if(place) MoveWindow(status,D(18),top,width,statusHeight,TRUE);
        return y+rowHeight;
    }
    void Layout() {
        if(layingOut || !page) return;layingOut=true;
        RECT client;GetClientRect(window,&client);
        int width=std::max<int>(D(120),client.right-D(36));
        footerHeight=Footer(width,false,0);
        pageHeight=std::max<int>(1,client.bottom-D(36)-D(6)-footerHeight);
        MoveWindow(page,D(18),D(18),width,pageHeight,TRUE);
        // Moving ~40 controls one by one repainted each immediately, so the list
        // visibly filled in row by row. Suspend page drawing while placing them and
        // repaint once. Only after the page itself is sized (a redraw-suspended window
        // counts as hidden, so resizing it then would leave the uncovered area stale),
        // and only while visible: WM_SETREDRAW TRUE would show a hidden window.
        const bool batch=IsWindowVisible(window)!=FALSE;
        if(batch) SendMessageW(page,WM_SETREDRAW,FALSE,0);
        // Measure once without a scroll bar, then account for its width if needed.
        ShowScrollBar(page,SB_VERT,FALSE);
        int contentWidth=width;
        contentHeight=Content(contentWidth,false);
        if(contentHeight>pageHeight) {
            ShowScrollBar(page,SB_VERT,TRUE);RECT area;GetClientRect(page,&area);contentWidth=area.right;
            contentHeight=Content(contentWidth,false);
        }
        scroll=std::clamp(scroll,0,std::max(0,contentHeight-pageHeight));
        SCROLLINFO si{sizeof(si),SIF_RANGE|SIF_PAGE|SIF_POS};si.nMax=std::max(0,contentHeight-1);si.nPage=pageHeight;si.nPos=scroll;
        SetScrollInfo(page,SB_VERT,&si,TRUE);
        Content(contentWidth,true);
        Footer(width,true,D(18)+pageHeight+D(6));
        if(batch) {
            SendMessageW(page,WM_SETREDRAW,TRUE,0);
            RedrawWindow(page,nullptr,nullptr,RDW_ERASE|RDW_FRAME|RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
        } else InvalidateRect(page,nullptr,TRUE);
        layingOut=false;
    }
    void ScrollTo(int value) {
        scroll=std::clamp(value,0,std::max(0,contentHeight-pageHeight));
        Layout();
    }
    void Wheel(WPARAM wp) {
        wheelRemainder+=GET_WHEEL_DELTA_WPARAM(wp);
        UINT lines=3;SystemParametersInfoW(SPI_GETWHEELSCROLLLINES,0,&lines,0);
        int ticks=wheelRemainder/WHEEL_DELTA;wheelRemainder%=WHEEL_DELTA;
        if(ticks) ScrollTo(scroll-ticks*(lines==WHEEL_PAGESCROLL?pageHeight:(int)lines*D(16)));
    }
    void Reveal(size_t i) {
        if(layingOut || i>=rows.size()) return;
        auto r=rows[i].comboRect;
        if(r.top<scroll) ScrollTo(r.top-D(6));
        else if(r.bottom>scroll+pageHeight) ScrollTo(r.bottom-pageHeight+D(6));
    }
    void Fit(const RECT& work) {
        minWidth=std::min(680,(int)(work.right-work.left)); minHeight=std::min(450,(int)(work.bottom-work.top));
        int label=0,choice=0;
        for(const auto& s:Options()) {label=std::max(label,(int)Measure(s.label,font).cx);for(const auto& c:s.choices) choice=std::max(choice,(int)Measure(c.label,font).cx);}
        RECT outer,client;GetWindowRect(window,&outer);GetClientRect(window,&client);
        int chromeX=outer.right-outer.left-client.right,chromeY=outer.bottom-outer.top-client.bottom;
        int width=std::max(680,(int)std::max((label+D(45))/.46,(choice+D(41))/.54)+D(84)+chromeX);
        width=std::min(width,(int)(work.right-work.left));
        int inner=std::max(1,width-chromeX-D(36));
        int height=Content(inner,false)+Footer(inner,false,0)+D(42)+chromeY;
        height=std::min(height,(int)(work.bottom-work.top));scroll=0;
        SetWindowPos(window,nullptr,work.left+(work.right-work.left-width)/2,work.top+(work.bottom-work.top-height)/2,width,height,SWP_NOZORDER|SWP_NOACTIVATE);
        Layout();
    }
    void Action(int id) {
        if(id==RestoreId) {auto d=Defaults();if(showAdvanced)d[L"showAdvanced"]=L"1";SetValues(d);Status(L"Defaults selected. Click Save to apply.");return;}
        if(id==ContentId) {
            if(!RunSetup(instance,root,window)) {DestroyWindow(window);return;}
            Dependencies();Status(ExtraInstalled(root,Extra::Doom3)?L"Game content updated.":L"Game content updated. Doom 3 weapons need the optional Doom 3 content.");return;
        }
        Values v=Read();Save(root,v);Status(L"Saved. Use Play-Prey2006-Custom.bat or Save & Play.");
        if(id==PlayId) {Launch(root/EngineDirectory/L"prey06.exe",Arguments(root,v));DestroyWindow(window);}
    }
    void PaintPage(HDC dc) {
        RECT client;GetClientRect(page,&client);FillRect(dc,&client,GetSysColorBrush(COLOR_BTNFACE));
        for(auto& group:groups) {RECT r=group.rect;OffsetRect(&r,0,-scroll);FrameRect(dc,&r,GetSysColorBrush(COLOR_3DSHADOW));}
    }
    void Verify(const fs::path& output);
};

static void Report(HWND owner,const std::exception& e) {MessageBoxW(owner,Wide(e.what()).c_str(),L"Prey2006 Reawakened Launcher",MB_OK|MB_ICONERROR);}
static LRESULT CALLBACK ComboProc(HWND h,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR data) {
    auto app=reinterpret_cast<App*>(data);
    if(msg==WM_MOUSEWHEEL) {SendMessageW(h,CB_SHOWDROPDOWN,FALSE,0);app->Wheel(wp);return 0;}
    if(msg==WM_NCDESTROY) RemoveWindowSubclass(h,ComboProc,1);
    return DefSubclassProc(h,msg,wp,lp);
}
static LRESULT CALLBACK PageProc(HWND h,UINT msg,WPARAM wp,LPARAM lp) {
    auto app=reinterpret_cast<App*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(msg==WM_NCCREATE) {app=(App*)((CREATESTRUCTW*)lp)->lpCreateParams;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)app);}
    if(!app) return DefWindowProcW(h,msg,wp,lp);
    switch(msg) {
        case WM_COMMAND: return SendMessageW(app->window,msg,wp,lp);
        case WM_MOUSEWHEEL: app->Wheel(wp);return 0;
        case WM_VSCROLL: {
            SCROLLINFO si{sizeof(si),SIF_ALL};GetScrollInfo(h,SB_VERT,&si);int pos=app->scroll;
            switch(LOWORD(wp)) {
                case SB_LINEUP:pos-=app->D(16);break;case SB_LINEDOWN:pos+=app->D(16);break;
                case SB_PAGEUP:pos-=app->pageHeight;break;case SB_PAGEDOWN:pos+=app->pageHeight;break;
                case SB_THUMBTRACK:case SB_THUMBPOSITION:pos=si.nTrackPos;break;
                case SB_TOP:pos=0;break;case SB_BOTTOM:pos=app->contentHeight;break;
            }
            app->ScrollTo(pos);return 0;
        }
        case WM_PAINT: {PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);app->PaintPage(dc);EndPaint(h,&ps);return 0;}
        case WM_PRINTCLIENT:app->PaintPage((HDC)wp);return 0;
        case WM_CTLCOLORSTATIC:SetBkMode((HDC)wp,TRANSPARENT);SetTextColor((HDC)wp,GetSysColor(COLOR_BTNTEXT));return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
    }
    return DefWindowProcW(h,msg,wp,lp);
}
static LRESULT CALLBACK MainProc(HWND h,UINT msg,WPARAM wp,LPARAM lp) {
    auto app=reinterpret_cast<App*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(msg==WM_NCCREATE) {app=(App*)((CREATESTRUCTW*)lp)->lpCreateParams;app->window=h;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)app);}
    if(!app) return DefWindowProcW(h,msg,wp,lp);
    try {
        switch(msg) {
            case WM_SIZE:app->Layout();return 0;
            case WM_MOUSEWHEEL:app->Wheel(wp);return 0;
            case WM_COMMAND: {
                int id=LOWORD(wp),event=HIWORD(wp);
                if(id>=100 && id<100+(int)app->rows.size()) {
                    if(event==CBN_SELCHANGE) {app->Dependencies();app->Status(L"Unsaved changes");}
                    if(event==CBN_SETFOCUS) app->Reveal(id-100);
                } else if(id==AdvancedId && event==BN_CLICKED) {
                    app->ShowAdvanced(SendMessageW(app->advanced,BM_GETCHECK,0,0)==BST_CHECKED);app->Status(L"Unsaved changes");
                } else if(id>=RestoreId && id<=ContentId && event==BN_CLICKED) app->Action(id);
                return 0;
            }
            case WM_DPICHANGED: {
                app->Fonts(HIWORD(wp));auto r=(RECT*)lp;
                SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);app->Layout();return 0;
            }
            case WM_GETMINMAXINFO: {
                auto limits=(MINMAXINFO*)lp;RECT work=WorkArea(h);
                limits->ptMinTrackSize.x=std::min((LONG)app->minWidth,work.right-work.left);
                limits->ptMinTrackSize.y=std::min((LONG)app->minHeight,work.bottom-work.top);return 0;
            }
            case WM_SETTINGCHANGE: if(app->page){app->Fonts(WindowDpi(h));app->Layout();}break;
            case WM_CTLCOLORSTATIC:SetBkMode((HDC)wp,TRANSPARENT);return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
            case WM_CLOSE:DestroyWindow(h);return 0;
            case WM_DESTROY:PostQuitMessage(0);return 0;
        }
    } catch(const std::exception& e) {Report(h,e);}
    return DefWindowProcW(h,msg,wp,lp);
}
void App::Create() {
    window=CreateWindowExW(WS_EX_CONTROLPARENT,MainClass,L"Prey2006 Reawakened Launcher",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,760,780,nullptr,nullptr,instance,this);
    if(!window) throw std::runtime_error("Cannot create settings window");
    page=CreateWindowExW(WS_EX_CONTROLPARENT,PageClass,L"Settings",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_VSCROLL,0,0,1,1,window,nullptr,instance,this);
    if(!page) throw std::runtime_error("Cannot create settings page");
    tooltip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,nullptr,WS_POPUP|TTS_ALWAYSTIP,0,0,0,0,window,nullptr,instance,nullptr);
    SendMessageW(tooltip,TTM_SETMAXTIPWIDTH,0,500);SendMessageW(tooltip,TTM_SETDELAYTIME,TTDT_AUTOPOP,20000);
    title=Control(L"STATIC",L"Prey2006 Reawakened Launcher",SS_NOPREFIX,page);
    for(size_t i=0;i<Options().size();++i) {
        const auto& s=Options()[i];
        if(groups.empty() || groups.back().name!=s.group) groups.push_back({s.group,Control(L"STATIC",s.group,SS_NOPREFIX,page),{}});
        Row row;row.label=Control(L"STATIC",s.label,SS_NOPREFIX,page);
        row.combo=Control(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST|CBS_HASSTRINGS|WS_VSCROLL,page,100+(int)i);
        for(const auto& choice:s.choices) SendMessageW(row.combo,CB_ADDSTRING,0,(LPARAM)choice.label.c_str());
        SetWindowSubclass(row.combo,ComboProc,1,(DWORD_PTR)this);Tip(row.label,s.hint);Tip(row.combo,s.hint);rows.push_back(row);
    }
    status=Control(L"STATIC",statusText,SS_NOPREFIX,window);
    buttons[0]=Control(L"BUTTON",L"Restore defaults",WS_TABSTOP|BS_PUSHBUTTON,window,RestoreId);
    buttons[1]=Control(L"BUTTON",L"Save settings",WS_TABSTOP|BS_PUSHBUTTON,window,SaveId);
    buttons[2]=Control(L"BUTTON",L"Save && Play",WS_TABSTOP|BS_PUSHBUTTON,window,PlayId);
    buttons[3]=Control(L"BUTTON",L"Game content...",WS_TABSTOP|BS_PUSHBUTTON,window,ContentId);
    Tip(buttons[3],L"Add or remove the optional Doom 3 and Portal content.");
    advanced=Control(L"BUTTON",L"Show advanced options",WS_TABSTOP|BS_AUTOCHECKBOX,window,AdvancedId);
    Tip(advanced,L"Shadow, portal rendering and dependent options. Hidden options keep their saved values.");
    Fonts(WindowDpi(window));
    Values values=Defaults();
    try {values=Load(root);if(fs::exists(root/L"Prey-settings.json")) statusText=L"Saved settings loaded.";}
    catch(...) {statusText=L"Could not read saved settings. Defaults loaded; original file unchanged.";}
    SetValues(values);SetWindowTextW(status,statusText.c_str());
    for(int pass=0;pass<3;++pass) Fit(WorkArea(window));
}

// App-owned offscreen rendering for layout QA; never captures the user's desktop.
static void Snapshot(HWND h,const fs::path& path) {
    RECT r;GetWindowRect(h,&r);r.right-=r.left;r.bottom-=r.top;r.left=r.top=0;HDC dc=GetDC(h),memory=CreateCompatibleDC(dc);
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=r.right;info.bmiHeader.biHeight=-r.bottom;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* pixels=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);auto old=SelectObject(memory,bitmap);
    SendMessageW(h,WM_PRINT,(WPARAM)memory,PRF_NONCLIENT|PRF_CLIENT|PRF_CHILDREN|PRF_ERASEBKGND);
    BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info.bmiHeader);file.bfSize=file.bfOffBits+r.right*r.bottom*4;
    std::ofstream out(path,std::ios::binary);out.write((char*)&file,sizeof(file));out.write((char*)&info.bmiHeader,sizeof(info.bmiHeader));out.write((char*)pixels,(std::streamsize)r.right*r.bottom*4);
    SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(h,dc);
}
void App::Verify(const fs::path& output) {
    auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    auto initial=Read();RECT work=WorkArea(window);Snapshot(window,output/L"startup.bmp");
    Atomic(output/L"metrics.txt", "dpi="+std::to_string(dpi)+" work="+std::to_string(work.right-work.left)+"x"+std::to_string(work.bottom-work.top)+" label="+std::to_string(Measure(L"Flashlight dynamic shadows",font).cx)+" rowwidth="+std::to_string(rows[6].labelRect.right-rows[6].labelRect.left)+"\n");
    minWidth=480;
    for(bool shown:{false,true}) {
    ShowAdvanced(shown);
    for(UINT scale:{96u,120u,144u,192u}) {
        Fonts(scale);
        for(int width:{640,850,1100}) {
            SetWindowPos(window,nullptr,0,0,width,780,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);Layout();
            int last=0;
            for(const auto& g:groups){require(g.rect.top>=last,"Overlapping sections");last=g.rect.bottom;}
            size_t visible=0;
            for(size_t i=0;i<rows.size();++i) {
                if(!Visible(i)) {require(!IsWindowVisible(rows[i].combo),"Hidden option still shown");continue;}
                ++visible;
                const auto& a=rows[i].labelRect;const auto& b=rows[i].comboRect;
                require(a.right<=b.left,"Overlapping label/dropdown");
                auto g=std::find_if(groups.begin(),groups.end(),[&](const Group& x){return x.name==Options()[i].group;});
                require(a.top>=g->rect.top && a.bottom<=g->rect.bottom && b.bottom<=g->rect.bottom,"Clipped section row");
                size_t n=i+1;while(n<rows.size() && !Visible(n))++n;
                if(n<rows.size())require(std::max(a.bottom,b.bottom)<=std::min(rows[n].labelRect.top,rows[n].comboRect.top),"Overlapping rows");
            }
            require(visible==(shown?rows.size():15),"Wrong number of visible options");
            ScrollTo(contentHeight);require(scroll+pageHeight>=contentHeight,"Cannot scroll to last setting");
            auto current=Read();current.erase(L"showAdvanced");auto expected=initial;expected.erase(L"showAdvanced");
            require(current==expected,"Layout changed a setting");
        }
    }
    }
    ShowAdvanced(false);
    Fonts(WindowDpi(window));RECT small{work.left,work.top,work.left+640,work.top+480};Fit(small);
    RECT bounds;GetWindowRect(window,&bounds);require(bounds.right-bounds.left<=640 && bounds.bottom-bounds.top<=480 && contentHeight>pageHeight,"Small-screen layout failed");
    Snapshot(window,output/L"small-top.bmp");ScrollTo(contentHeight);Snapshot(window,output/L"small-bottom.bmp");
    auto v=Defaults();v[L"r_fullscreen"]=L"desktop";v[L"in_useGamepad"]=L"0";v[L"g_portalGun"]=L"0";v[L"g_bunnyHop"]=L"1";v[L"showAdvanced"]=L"1";SetValues(v);
    for (size_t i=0; i<rows.size(); ++i) {
        const auto& key=Options()[i].key;
        if (key==L"resolution" || key==L"joy_invertLook" || key==L"joy_deadZone" || key==L"g_portalGunReticle" || key==L"g_halfLifeAutoHop")
            require(!IsWindowEnabled(rows[i].combo),"Dependency state failed");
    }
    v[L"g_portalGun"]=L"1";v[L"g_bunnyHop"]=L"3";SetValues(v);
    for (size_t i=0; i<rows.size(); ++i) {
        const auto& key=Options()[i].key;
        if (key==L"g_portalGunReticle" || key==L"g_halfLifeAutoHop") require(IsWindowEnabled(rows[i].combo),"Dependency enable failed");
    }
    auto before=Read();SendMessageW(rows[0].combo,WM_MOUSEWHEEL,MAKEWPARAM(0,(WORD)-WHEEL_DELTA),0);require(Read()==before,"Mouse wheel changed a choice");
    SetValues(initial);Fit(work);Snapshot(window,output/L"settings.bmp");
    Atomic(output/L"layout-pass.txt","PASS: all controls with advanced options hidden and shown, row/group containment, small-screen scrolling, 96/120/144/192 DPI at 640/850/1100 widths, wheel forwarding and dependency state.\n");

    // Exercise the actual button handlers against an isolated root and child
    // probe, including spaces/Unicode in paths. Never launches the real game.
    auto originalRoot=root;
    root=output/(L"actions Unicode 雪 "+std::to_wstring(GetTickCount64()));
    fs::create_directories(root/EngineDirectory);
    Action(RestoreId);require(!fs::exists(root/L"Prey-settings.json"),"Restore defaults wrote settings before Save");
    SetValues(initial);Action(SaveId);require(Load(root)==initial,"Save button changed choices");
    bool rejected=false;try {Action(PlayId);}catch(const std::exception&){rejected=true;}
    require(rejected && IsWindow(window),"Failed launch closed the settings window");
    wchar_t self[32768];GetModuleFileNameW(nullptr,self,32768);
    auto probe=fs::path(self).parent_path()/L"PreySettingsTestChild.exe";
    if(!fs::exists(probe)) probe=originalRoot/L"PreyConfigurator/native/build/Release/PreySettingsTestChild.exe";
    fs::copy_file(probe,root/EngineDirectory/L"prey06.exe",fs::copy_options::overwrite_existing);
    if(initial.at(L"weaponPack")==L"doom3shotgun") {
        // The child only records arguments; no retail assets are needed here.
        auto mod=root/EngineDirectory/L"base";
        fs::create_directories(mod);
        Atomic(mod/L"doom3-import-manifest.json","{}\n");
        fs::create_directories(mod/L"def");
        Atomic(mod/L"def/doom3_machinegun.def","// Verification-only import marker\n");
        Atomic(mod/L"def/doom3_chaingun.def","// Verification-only import marker\n");
        Atomic(mod/L"def/doom3_plasmagun.def","// Verification-only import marker\n");
        Atomic(mod/L"def/doom3_rocketlauncher.def","// Verification-only import marker\n");
    }
    auto expected=Arguments(root,initial);Action(PlayId);require(!IsWindow(window),"Successful Save & Play did not close");
    auto resultFile=root/EngineDirectory/L"launch-result.json";
    for(int i=0;i<100 && !fs::exists(resultFile);++i) Sleep(50);
    std::ifstream in(resultFile,std::ios::binary);require((bool)in,"Child process did not produce a report");
    auto report=ParseJson(std::string(std::istreambuf_iterator<char>(in),{}));
    require(report[L"count"]==std::to_wstring(expected.size()) && report[L"console"]==L"no","Child args or console flags failed");
    for(size_t i=0;i<expected.size();++i) require(report[L"arg"+std::to_wstring(i+1)]==expected[i],"Child argument mismatch");
    require(fs::equivalent(report[L"cwd"],root/EngineDirectory),"Wrong game working directory");
    Atomic(output/L"launch-pass.txt","PASS: Restore is unsaved; Save preserves choices; failed launch stays open; Save & Play launches with exact args, Unicode/spaced paths, correct cwd, no console, and closes Settings.\n");
    root=originalRoot;
}

int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR,int show) {
    instance=inst;int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    std::vector<std::wstring> args;for(int i=1;i<argc;++i)args.emplace_back(argv[i]);LocalFree(argv);
    wchar_t module[32768];GetModuleFileNameW(nullptr,module,32768);fs::path root=fs::path(module).parent_path();
    bool verify=false,migrate=false,setup=false;
    for(size_t i=0;i<args.size();++i) {
        if(args[i]==L"--root" && i+1<args.size())root=fs::absolute(args[++i]);
        else if(args[i]==L"--verify")verify=true;
        else if(args[i]==L"--migrate")migrate=true;
        else if(args[i]==L"--setup")setup=true;
        else if(args[i]==L"--list-games" && i+1<args.size()) {
            // Test hook: detected installations per game, then how each extra
            // folder argument is judged for every game.
            const fs::path out=fs::absolute(args[++i]);std::string report;
            for(Game g:AllGames) {
                report+=Utf8(GameName(g))+":\n";
                for(const auto& p:FindGame(g)) report+="  found "+Utf8(p.wstring())+(CheckGame(g,p).expansion?" (+expansion)":"")+"\n";
            }
            for(++i;i<args.size();++i) {
                report+="check "+Utf8(args[i])+"\n";
                for(Game g:AllGames) {
                    const fs::path folder=NormalizeGameFolder(g,args[i]);const auto c=CheckGame(g,folder);
                    report+="  "+Utf8(GameName(g))+": "+(c.problem.empty()?"OK"+std::string(c.expansion?" (+expansion)":"")+" at "+Utf8(folder.wstring()):Utf8(c.problem))+"\n";
                }
            }
            Atomic(out,report);return 0;
        }
    }
    try {
        if(migrate){Save(root,Load(root));return 0;}
        INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_STANDARD_CLASSES|ICC_WIN95_CLASSES};InitCommonControlsEx(&controls);
        WNDCLASSEXW wc{sizeof(wc)};wc.hInstance=instance;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=GetSysColorBrush(COLOR_BTNFACE);
        wc.hIcon=(HICON)LoadImageW(instance,MAKEINTRESOURCEW(1),IMAGE_ICON,0,0,LR_DEFAULTSIZE);wc.hIconSm=(HICON)LoadImageW(instance,MAKEINTRESOURCEW(1),IMAGE_ICON,16,16,0);
        wc.lpfnWndProc=MainProc;wc.lpszClassName=MainClass;RegisterClassExW(&wc);
        wc.lpfnWndProc=PageProc;wc.lpszClassName=PageClass;wc.hIcon=nullptr;wc.hIconSm=nullptr;RegisterClassExW(&wc);
        if(verify) {
            auto output=root/L"validation/configurator-native";VerifyConfiguration(output);VerifyRetailSetup(output);VerifyGames(output);
            App app(root);app.Create();app.Verify(output);DestroyWindow(app.window);return 0;
        }
        // First run: bring in the player's retail Prey data (and any optional
        // content) before showing settings. --setup opens it even when installed.
        if((setup || !RetailReady(root)) && !RunSetup(instance,root)) return 0;
        // Show while cloaked and paint every control synchronously, then uncloak:
        // the window appears complete instead of as an empty frame or row by row.
        App app(root);app.Create();Cloak(app.window,TRUE);ShowWindow(app.window,show);
        RedrawWindow(app.window,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_FRAME|RDW_ALLCHILDREN|RDW_UPDATENOW);
        Cloak(app.window,FALSE);
        MSG message;while(GetMessageW(&message,nullptr,0,0)>0) {if(!IsDialogMessageW(app.window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}
        return (int)message.wParam;
    } catch(const std::exception& e) {
        if(verify) {fs::create_directories(root/L"validation/configurator-native");Atomic(root/L"validation/configurator-native/error.txt",e.what());}
        else Report(nullptr,e);
        return 1;
    }
}
