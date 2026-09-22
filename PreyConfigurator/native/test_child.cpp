#include "settings.h"
#include <shellapi.h>

int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    try {
        Values report;int count;auto args=CommandLineToArgvW(GetCommandLineW(),&count);
        for(int i=1;i<count;++i) report[L"arg"+std::to_wstring(i)]=args[i];
        LocalFree(args);report[L"count"]=std::to_wstring(count-1);
        report[L"console"]=GetConsoleWindow()?L"yes":L"no";
        report[L"cwd"]=fs::current_path().wstring();
        Atomic(fs::current_path()/L"launch-result.json",Json(report));return 0;
    } catch(...) {return 1;}
}
