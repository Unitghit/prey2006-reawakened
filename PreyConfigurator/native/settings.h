#pragma once
#include <windows.h>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using Values = std::map<std::wstring, std::wstring>;
struct Choice { std::wstring label, value; };
struct Setting { std::wstring group, label, key, initial, hint; std::vector<Choice> choices; };
const std::vector<Setting>& Options();
// Single location for the engine build used by Save & Play and the batch file.
inline constexpr wchar_t EngineDirectory[] = L"engine";
std::wstring Wide(const std::string& text);
std::string Utf8(const std::wstring& text);
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
