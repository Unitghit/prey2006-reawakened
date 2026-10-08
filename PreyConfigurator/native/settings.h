#pragma once
#include <windows.h>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using Values = std::map<std::wstring, std::wstring>;
struct Choice { std::wstring label, value; };
// An action setting is a button (label: initial) that opens a window; it has no saved value.
struct Setting { std::wstring group, label, key, initial, hint; std::vector<Choice> choices; bool advanced = false; bool action = false; };
const std::vector<Setting>& Options();
// Single location for the engine build used by Save & Play and the batch file.
inline constexpr wchar_t EngineDirectory[] = L"engine";
std::wstring Wide(const std::string& text);
std::string Utf8(const std::wstring& text);
// Exception text: UTF-8 when valid, otherwise the Windows code page (filesystem errors).
std::wstring ErrorText(const char* message);
// Extended-length form of an absolute path, for file operations beyond MAX_PATH.
fs::path LongPath(const fs::path& path);
// Prey's engine opens files with the Windows code page. A folder whose name has other
// characters is passed by its short (8.3) name; EngineCanUse is false when there is none.
fs::path EnginePath(const fs::path& path);
bool EngineCanUse(const fs::path& path);
Values Defaults();
Values Migrate(const Values& saved);
Values ParseJson(const std::string& text);
std::string Json(const Values& values);
Values Load(const fs::path& root);
void Validate(const Values& values);
std::vector<std::pair<std::wstring, std::wstring>> Variables(const Values& values);
std::string Launcher(const Values& values);
void Save(const fs::path& root, const Values& values);
std::vector<std::wstring> Arguments(const fs::path& root, const Values& values);
std::wstring QuoteArgument(const std::wstring& value);
void Launch(const fs::path& executable, const std::vector<std::wstring>& args);
void Atomic(const fs::path& file, const std::string& contents);
void VerifyConfiguration(const fs::path& output);
