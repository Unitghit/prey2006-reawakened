#include "settings.h"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <shellapi.h>

#include "options.inc"

std::wstring Wide(const std::string& text) {
    if (text.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), (int)text.size(), nullptr, 0);
    if (!n) throw std::runtime_error("Invalid UTF-8 text");
    std::wstring result(n, 0);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), (int)text.size(), result.data(), n);
    return result;
}
std::string Utf8(const std::wstring& text) {
    if (text.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    if (!n) throw std::runtime_error("Invalid Unicode text");
    std::string result(n, 0);
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), (int)text.size(), result.data(), n, nullptr, nullptr);
    return result;
}
std::wstring ErrorText(const char* message) {
    try { return Wide(message); } catch (...) {}
    const int n = MultiByteToWideChar(CP_ACP, 0, message, -1, nullptr, 0);
    if (n <= 1) return L"Unknown error";
    std::wstring result(n - 1, 0);
    MultiByteToWideChar(CP_ACP, 0, message, -1, result.data(), n);
    return result;
}
fs::path LongPath(const fs::path& path) {
    std::wstring text = fs::absolute(path).lexically_normal().make_preferred().wstring();
    if (text.rfind(L"\\\\?\\", 0) == 0) return text;
    if (text.rfind(L"\\\\", 0) == 0) return L"\\\\?\\UNC\\" + text.substr(2);
    return L"\\\\?\\" + text;
}
static bool InCodePage(const std::wstring& text) {
    BOOL lost = FALSE;
    WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, text.c_str(), -1, nullptr, 0, nullptr, &lost);
    return !lost;
}
fs::path EnginePath(const fs::path& path) {
    if (InCodePage(path.wstring())) return path;
    std::error_code ec; fs::create_directories(path, ec);	// short names exist only for existing folders
    std::wstring name(32768, L'\0');
    const DWORD n = GetShortPathNameW(path.c_str(), name.data(), (DWORD)name.size());
    if (n && n < name.size()) { name.resize(n); if (InCodePage(name)) return name; }
    return path;
}
bool EngineCanUse(const fs::path& path) { return InCodePage(EnginePath(path).wstring()); }
static const wchar_t* Smooth[] = {L"g_interpolatePortals", L"g_interpolateView", L"g_interpolateWorld", L"g_interpolateWeapons", L"g_interpolateEffects"};
static std::wstring Get(const Values& v, const std::wstring& key, const std::wstring& fallback = L"") {
    auto it = v.find(key); return it == v.end() ? fallback : it->second;
}
Values Defaults() {
    Values v; for (const auto& s : Options()) v[s.key] = s.initial; return v;
}
Values Migrate(const Values& saved) {
    Values v = Defaults();
    for (const auto& s : Options()) {
        auto it = saved.find(s.key);
        if (it != saved.end() && std::any_of(s.choices.begin(), s.choices.end(), [&](const Choice& c) { return c.value == it->second; })) v[s.key] = it->second;
    }
    if (!saved.count(L"bloom")) v[L"bloom"] = Get(saved,L"r_skipGlowOverlay") == L"1" ? L"off" : Get(saved,L"r_glowResolution") == L"256" ? L"original" : L"enhanced";
    // The two former muzzle flash switches became one choice.
    if (!saved.count(L"muzzleShadows") && (saved.count(L"g_muzzleFlashShadows") || saved.count(L"g_npcMuzzleFlashShadows"))) {
        const bool own = Get(saved,L"g_muzzleFlashShadows",L"1") == L"1", enemies = Get(saved,L"g_npcMuzzleFlashShadows",L"1") == L"1";
        v[L"muzzleShadows"] = !own && !enemies ? L"off" : !enemies ? L"player" : L"all";
    }
    // Launcher preference, not a game setting.
    if (Get(saved,L"showAdvanced") == L"1") v[L"showAdvanced"] = L"1";
    return v;
}
void Validate(const Values& v) {
    for (const auto& s : Options()) {
        auto it = v.find(s.key);
        if (it == v.end() || !std::any_of(s.choices.begin(), s.choices.end(), [&](const Choice& c) { return c.value == it->second; }))
            throw std::runtime_error("Invalid setting: " + Utf8(s.label));
    }
}

// The settings format is a JSON object with string keys and string values.
// Parse strictly, including escapes, so corrupt files cannot silently lose choices.
Values ParseJson(const std::string& bytes) {
    auto text = Wide(bytes); size_t pos = 0;
    if (!text.empty() && text[0] == 0xfeff) ++pos;
    auto ws = [&] { while (pos < text.size() && (text[pos] == L' ' || text[pos] == L'\t' || text[pos] == L'\r' || text[pos] == L'\n')) ++pos; };
    auto take = [&](wchar_t ch) { ws(); if (pos >= text.size() || text[pos++] != ch) throw std::runtime_error("Invalid settings JSON"); };
    auto str = [&]() {
        take(L'"'); std::wstring result;
        while (pos < text.size()) {
            wchar_t c = text[pos++];
            if (c == L'"') { (void)Utf8(result); return result; }
            if (c < 32) throw std::runtime_error("Invalid JSON string");
            if (c != L'\\') { result += c; continue; }
            if (pos == text.size()) throw std::runtime_error("Incomplete JSON escape");
            c = text[pos++];
            switch (c) {
                case L'"': case L'\\': case L'/': result += c; break;
                case L'b': result += L'\b'; break; case L'f': result += L'\f'; break;
                case L'n': result += L'\n'; break; case L'r': result += L'\r'; break; case L't': result += L'\t'; break;
                case L'u': {
                    unsigned value = 0;
                    for (int i = 0; i < 4; ++i) {
                        if (pos == text.size()) throw std::runtime_error("Incomplete Unicode escape");
                        wchar_t h = text[pos++]; unsigned digit;
                        if (h >= L'0' && h <= L'9') digit = h - L'0';
                        else if (h >= L'a' && h <= L'f') digit = h - L'a' + 10;
                        else if (h >= L'A' && h <= L'F') digit = h - L'A' + 10;
                        else throw std::runtime_error("Invalid Unicode escape");
                        value = value * 16 + digit;
                    }
                    result += (wchar_t)value; break;
                }
                default: throw std::runtime_error("Invalid JSON escape");
            }
        }
        throw std::runtime_error("Unterminated JSON string");
    };
    Values result; take(L'{'); ws();
    if (pos < text.size() && text[pos] != L'}') {
        for (;;) {
            auto key = str(); take(L':'); auto value = str();
            if (!result.emplace(key,value).second) throw std::runtime_error("Duplicate settings key");
            ws(); if (pos < text.size() && text[pos] == L',') { ++pos; continue; } break;
        }
    }
    take(L'}'); ws(); if (pos != text.size()) throw std::runtime_error("Trailing settings data");
    return result;
}
static std::string JsonString(const std::wstring& text) {
    std::string result = "\"";
    for (unsigned char c : Utf8(text)) {
        if (c == '"' || c == '\\') { result += '\\'; result += (char)c; }
        else if (c < 32) { const char* h = "0123456789abcdef"; result += "\\u00"; result += h[c >> 4]; result += h[c & 15]; }
        else result += (char)c;
    }
    return result + '"';
}
std::string Json(const Values& v) {
    std::string result = "{\n"; bool first = true;
    for (const auto& pair : v) { if (!first) result += ",\n"; first = false; result += "  " + JsonString(pair.first) + ": " + JsonString(pair.second); }
    return result + "\n}\n";
}
Values Load(const fs::path& root) {
    auto file = root / L"Prey-settings.json";
    if (!fs::exists(file)) return Defaults();
    if (fs::file_size(file) > 1024 * 1024) throw std::runtime_error("Settings file is unexpectedly large");
    std::ifstream in(file,std::ios::binary);
    if (!in) throw std::runtime_error("Cannot read settings file");
    return Migrate(ParseJson(std::string(std::istreambuf_iterator<char>(in), {})));
}
std::vector<std::pair<std::wstring,std::wstring>> Variables(const Values& v) {
    Validate(v); std::vector<std::pair<std::wstring,std::wstring>> result;
    for (const auto& s : Options()) if (s.key != L"resolution" && s.key != L"bloom" && s.key != L"muzzleShadows" && s.key != L"r_fullscreen" && s.key != L"weaponPack") result.emplace_back(s.key,v.at(s.key));
    const auto& size = v.at(L"resolution"); auto x = size.find(L'x');
    const std::vector<std::pair<std::wstring,std::wstring>> fixed = {
        {L"fs_game",L""},{L"g_doom3Shotgun",v.at(L"weaponPack")==L"doom3shotgun"?L"1":L"0"},
        {L"r_mode",L"-1"},{L"r_customWidth",size.substr(0,x)},{L"r_customHeight",size.substr(x+1)},
        {L"gui_translateAlienFont",L"fonts"},{L"g_stopTime",L"0"},
        {L"r_fullscreen",v.at(L"r_fullscreen")==L"0"?L"0":L"1"},{L"r_fullscreenDesktop",v.at(L"r_fullscreen")==L"desktop"?L"1":L"0"},
        {L"com_unlockedFPS",L"1"},{L"r_glowMode",L"2"},{L"r_skipGlowOverlay",v.at(L"bloom")==L"off"?L"1":L"0"},{L"r_glowResolution",v.at(L"bloom")==L"original"?L"256":L"0"},
        {L"r_glowStrength",L"0.5"},{L"r_glowAlpha",L"0.55"},{L"r_glowAlphaChange",L"0.85"},{L"r_glowSteps",L"8"},
        {L"r_correctspecular",L"1"},{L"r_normalizebumpmap",L"1"},{L"r_cubemapNormalize",L"0"},{L"r_portalMaxDepth",v.at(L"r_portalDeepViews")==L"0"?L"3":L"6"},
        {L"r_glowPortals",L"1"},{L"g_portalLighter",L"1"},{L"g_portalMuzzleFlash",L"1"},{L"g_portalWeaponLighting",L"1"},{L"g_portalPreserveMotion",L"1"},{L"g_nightmare",L"1"},{L"g_lateMouse",L"0"},
        {L"g_muzzleFlashShadows",v.at(L"muzzleShadows")==L"off"?L"0":L"1"},{L"g_npcMuzzleFlashShadows",v.at(L"muzzleShadows")==L"all"?L"1":L"0"},
        // Formerly launcher options; now always at their recommended values.
        {L"image_threadedDecode",L"1"},{L"com_assetPreload",L"1"},{L"com_hitchTrace",L"0"}
    };
    result.insert(result.end(),fixed.begin(),fixed.end());
    for (auto key : Smooth) result.emplace_back(key,L"1");
    return result;
}
std::string Launcher(const Values& v) {
    std::string build = Utf8(EngineDirectory); std::replace(build.begin(),build.end(),'/','\\');
    std::string result = "@echo off\r\nsetlocal\r\npushd \"%~sdp0" + build + "\" || exit /b 1\r\nprey06.exe +set fs_basepath \"%~sdp0" + build + "\" +set fs_cdpath \"%~sdp0engine\" +set fs_savepath \"%~sdp0userdata\" +set fs_configpath \"%~sdp0userdata\" ";
    bool first = true; for (const auto& p : Variables(v)) { if (!first) result += ' '; first=false; result += "+set " + Utf8(p.first) + " " + (p.second.empty()?"\"\"":Utf8(p.second)); }
    return result + "\r\nset \"preyExitCode=%errorlevel%\"\r\npopd\r\nexit /b %preyExitCode%\r\n";
}
void Atomic(const fs::path& file, const std::string& contents) {
    auto tmp = file; tmp += L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
    { std::ofstream out(tmp,std::ios::binary|std::ios::trunc); out.write(contents.data(),(std::streamsize)contents.size()); out.close();
      if (!out) { std::error_code ec; fs::remove(tmp,ec); throw std::runtime_error("Cannot write " + Utf8(file.wstring())); } }
    if (!MoveFileExW(tmp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        std::error_code ec; fs::remove(tmp,ec); throw std::runtime_error("Cannot replace " + Utf8(file.wstring()));
    }
}
void Save(const fs::path& root, const Values& v) {
    Validate(v); Atomic(root/L"Prey-settings.json",Json(v)); Atomic(root/L"Play-Prey2006-Custom.bat",Launcher(v));
    // Copy legacy prototype saves into the shared campaign list once. The
    // originals and any existing destination saves are never overwritten.
    if (fs::exists(root/EngineDirectory/L"base/doom3-import-manifest.json")) {
        const auto source = root/L"userdata/doom3shotgun/savegames";
        const auto target = root/L"userdata/base/savegames";
        if (fs::is_directory(source)) {
            fs::create_directories(target);
            for (const auto& entry : fs::directory_iterator(source)) {
                if (!entry.is_regular_file() || entry.path().extension()!=L".save") continue;
                const auto name=L"D3Legacy_"+entry.path().stem().wstring();
                const auto dest=target/(name+L".save");
                if (fs::exists(dest)) continue;
                // Publish the save last so an interrupted copy can be retried.
                for (const auto* ext : {L".txt",L".tga"}) {
                    auto side=entry.path(); side.replace_extension(ext);
                    auto to=target/(name+ext);
                    if (fs::exists(side) && !fs::exists(to)) {
                        if (side.extension()==L".txt") {
                            std::ifstream in(side,std::ios::binary);
                            std::string title(std::istreambuf_iterator<char>(in), {});
                            auto quote=title.find('"');
                            if (quote!=std::string::npos) title.insert(quote+1,"Doom 3 legacy: ");
                            Atomic(to,title);
                        } else fs::copy_file(side,to);
                    }
                }
                auto tmp=dest; tmp+=L".importing";
                fs::copy_file(entry.path(),tmp,fs::copy_options::overwrite_existing);
                fs::rename(tmp,dest);
            }
        }
    }
}
std::vector<std::wstring> Arguments(const fs::path& root, const Values& v) {
    if (v.at(L"weaponPack") == L"doom3shotgun" &&
        (!fs::exists(root/EngineDirectory/L"base/doom3-import-manifest.json") ||
         !fs::exists(root/EngineDirectory/L"base/def/doom3_machinegun.def") ||
         !fs::exists(root/EngineDirectory/L"base/def/doom3_chaingun.def") ||
         !fs::exists(root/EngineDirectory/L"base/def/doom3_plasmagun.def") ||
         !fs::exists(root/EngineDirectory/L"base/def/doom3_rocketlauncher.def"))) {
        throw std::runtime_error("The Doom 3 weapons need the optional Doom 3 content. Add it with Import games... first.");
    }
    std::vector<std::wstring> args;
    auto set = [&](const std::wstring& key, const std::wstring& value) { args.insert(args.end(),{L"+set",key,value}); };
    set(L"fs_basepath",EnginePath(root/EngineDirectory).wstring()); set(L"fs_cdpath",EnginePath(root/L"engine").wstring());
    set(L"fs_savepath",EnginePath(root/L"userdata").wstring()); set(L"fs_configpath",EnginePath(root/L"userdata").wstring());
    for (const auto& p : Variables(v)) set(p.first,p.second);
    return args;
}
// Windows argv quoting, including embedded quotes and trailing backslashes.
std::wstring QuoteArgument(const std::wstring& value) {
    std::wstring out = L"\""; size_t slashes = 0;
    for (wchar_t c : value) {
        if (c == L'\\') { ++slashes; continue; }
        if (c == L'"') { out.append(slashes*2+1,L'\\'); out += c; }
        else { out.append(slashes,L'\\'); out += c; }
        slashes = 0;
    }
    out.append(slashes*2,L'\\'); return out + L'"';
}
void Launch(const fs::path& exe, const std::vector<std::wstring>& args) {
    if (!fs::is_regular_file(exe)) throw std::runtime_error("Game build missing: " + Utf8(exe.wstring()));
    std::wstring command = QuoteArgument(exe.wstring());
    for (const auto& arg : args) command += L" " + QuoteArgument(arg);
    STARTUPINFOW si{}; si.cb = sizeof(si); PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,EnginePath(exe.parent_path()).c_str(),&si,&pi))
        throw std::runtime_error("Could not launch game (Windows error " + std::to_string(GetLastError()) + ")");
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
}

void VerifyConfiguration(const fs::path& output) {
    fs::create_directories(output);
    auto require = [](bool ok) { if (!ok) throw std::runtime_error("Configuration verification failed"); };
    auto defaults = Defaults(); require(defaults.size()==22 && defaults.at(L"g_weaponUnlockTips")==L"1" && defaults.at(L"muzzleShadows")==L"all" && defaults.at(L"g_noSpiritResurrections")==L"0" && defaults.at(L"g_portalGun")==L"0" && defaults.at(L"g_portalGunReticle")==L"1" && defaults.at(L"g_bunnyHop")==L"0" && defaults.at(L"g_halfLifeAutoHop")==L"0" && defaults.at(L"com_maxFPS")==L"-1");
    Save(output,defaults); require(Load(output)==defaults);
    Atomic(output/L"default-launcher.bat",Launcher(defaults));
    for (const auto& s : Options()) for (const auto& c : s.choices) {
        auto v = defaults; v[s.key]=c.value; Validate(v); require(Migrate(ParseJson(Json(v)))==v);
        auto pairs = Variables(v); Values vars(pairs.begin(),pairs.end());
        require(vars.at(L"fs_game").empty() && vars.at(L"g_doom3Shotgun")==(v[L"weaponPack"]==L"doom3shotgun"?L"1":L"0"));
        require(vars.at(L"g_portalGun")==v[L"g_portalGun"] && vars.at(L"g_portalGunReticle")==v[L"g_portalGunReticle"]);
        require(vars.at(L"g_bunnyHop")==v[L"g_bunnyHop"] && vars.at(L"g_halfLifeAutoHop")==v[L"g_halfLifeAutoHop"]);
        require(vars.at(L"image_threadedDecode")==L"1" && vars.at(L"com_assetPreload")==L"1" && vars.at(L"com_hitchTrace")==L"0");
        require(vars.at(L"g_muzzleFlashShadows")==(v[L"muzzleShadows"]==L"off"?L"0":L"1") && vars.at(L"g_npcMuzzleFlashShadows")==(v[L"muzzleShadows"]==L"all"?L"1":L"0"));
        require(vars.at(L"g_noSpiritResurrections")==v[L"g_noSpiritResurrections"]);
        require(vars.at(L"r_portalMaxDepth")== (v[L"r_portalDeepViews"]==L"0"?L"3":L"6") && vars.at(L"r_correctspecular")==L"1");
        require(vars.at(L"r_fullscreen")== (v[L"r_fullscreen"]==L"0"?L"0":L"1"));
        require(vars.at(L"r_fullscreenDesktop")== (v[L"r_fullscreen"]==L"desktop"?L"1":L"0"));
        require(vars.at(L"r_skipGlowOverlay")== (v[L"bloom"]==L"off"?L"1":L"0"));
        require(vars.at(L"r_glowResolution")== (v[L"bloom"]==L"original"?L"256":L"0"));
        for (auto key : Smooth) require(vars.at(key)==L"1");
        Atomic(output/(s.key+L"-"+c.value+L".bat"),Launcher(v));
    }
    Values old{{L"r_skipGlowOverlay",L"1"},{L"gui_translateAlienFontDistance",L"200"},{L"g_forceCherokee",L"1"}};
    auto migrated=Migrate(old); require(migrated[L"bloom"]==L"off" && migrated[L"gui_translateAlienFontDistance"]==L"200" && migrated[L"g_forceCherokee"]==L"1");
    require(Migrate({{L"g_muzzleFlashShadows",L"0"},{L"g_npcMuzzleFlashShadows",L"0"}})[L"muzzleShadows"]==L"off");
    require(Migrate({{L"g_muzzleFlashShadows",L"1"},{L"g_npcMuzzleFlashShadows",L"0"}})[L"muzzleShadows"]==L"player");
    require(Migrate({{L"g_muzzleFlashShadows",L"0"},{L"g_npcMuzzleFlashShadows",L"1"}})[L"muzzleShadows"]==L"all");
    require(Migrate({{L"smoothMotion",L"0"},{L"com_hitchTrace",L"1"}})==defaults);
    auto shown=defaults; shown[L"showAdvanced"]=L"1"; require(Migrate(ParseJson(Json(shown)))==shown);
    require(ParseJson("{\"x\":\"\\u0041\\uD83D\\uDE00\"}")[L"x"]==L"A\U0001F600");
    for (const char* bad : {"{} junk","{\"x\":1}","{\"x\":\"a\",}","{\"x\":\"a\",\"x\":\"b\"}","{\"x\":\"\\uD800\"}"}) {
        bool rejected=false; try { ParseJson(bad); } catch (...) { rejected=true; } require(rejected);
    }
    std::vector<std::wstring> testArgs{L"C:\\Folder with spaces\\",L"hello\"world",L"",L"Unicode 雪",L"%&!"};
    std::wstring command=L"test.exe"; for (const auto& a : testArgs) command+=L" "+QuoteArgument(a);
    int argc; auto argv=CommandLineToArgvW(command.c_str(),&argc); require(argv && argc==(int)testArgs.size()+1);
    for (int i=1;i<argc;++i) require(argv[i]==testArgs[i-1]); LocalFree(argv);
    auto migrationRoot=output/(L"save-migration-"+std::to_wstring(GetCurrentProcessId()));
    auto legacy=migrationRoot/L"userdata/doom3shotgun/savegames";
    auto shared=migrationRoot/L"userdata/base/savegames";
    fs::create_directories(legacy);
    fs::create_directories(migrationRoot/EngineDirectory/L"base");
    Atomic(migrationRoot/EngineDirectory/L"base/doom3-import-manifest.json","{}");
    Atomic(legacy/L"example.save","legacy-save");
    Atomic(legacy/L"example.txt","\"example\"\n\"Ascent\"\n\"\"\n");
    Save(migrationRoot,defaults); // Import is also available with the addon off.
    require(fs::exists(shared/L"D3Legacy_example.save"));
    auto read=[](const fs::path& path) {
        std::ifstream in(path,std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), {});
    };
    require(read(legacy/L"example.save")=="legacy-save");
    require(read(shared/L"D3Legacy_example.save")=="legacy-save");
    require(read(shared/L"D3Legacy_example.txt").find("Doom 3 legacy: example")!=std::string::npos);
    Atomic(shared/L"D3Legacy_example.save","user-progress");
    Save(migrationRoot,defaults);
    require(read(shared/L"D3Legacy_example.save")=="user-progress");
    require(read(legacy/L"example.save")=="legacy-save");
    Atomic(output/L"configuration-pass.txt","PASS: all choices, bundles, defaults, migration, JSON validation, argument quoting and disk round trips.\n");
}
