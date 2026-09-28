#pragma once
#include "settings.h"
#include <set>

// Source games Reawakened imports from. Folders are validated by the data the
// import uses (never by an executable), so Steam, GOG, retail and copied
// installations all work the same way.
enum class Game { Prey, Doom3, Doom3BFG, Portal };
inline constexpr Game AllGames[] = { Game::Prey, Game::Doom3, Game::Doom3BFG, Game::Portal };

struct GameCheck {
	std::wstring problem;		// empty when the folder is usable
	bool expansion = false;		// Doom 3: Resurrection of Evil (Super Shotgun) present
};

const wchar_t* GameName(Game game);
bool GameRequired(Game game);
GameCheck CheckGame(Game game, const fs::path& folder);
// Maps a chosen folder (a subfolder such as base\ or portal\, or a parent that
// contains the game) to the game's installation folder.
fs::path NormalizeGameFolder(Game game, const fs::path& chosen);
// Detected installations that pass CheckGame, most likely first.
std::vector<fs::path> FindGame(Game game);

// Archive directory readers (names only, lowercase, '/' separated).
std::set<std::string> ZipNames(const fs::path& archive);
std::set<std::string> ResourcesNames(const fs::path& archive);
std::set<std::string> VpkNames(const fs::path& directoryFile);
// Steam library folders and an app's install folder (empty when not installed).
std::vector<fs::path> SteamLibraries();
fs::path SteamAppFolder(int appId);

void VerifyGames(const fs::path& output);
// Minimal stored-entry zip for self-tests.
void WriteTestZip(const fs::path& file, const std::vector<std::string>& names);
