#pragma once
#include "settings.h"
#include "games.h"
#include <atomic>
#include <cstdint>
#include <functional>

// First-run setup: brings the player's retail Prey (2006) data archives into
// this installation's engine/base. Only pak000-pak006.pk4 are used; the retail
// executable, CD key, configuration and saves are never read or copied.
extern const wchar_t* const RetailPaks[7];

bool RetailReady(const fs::path& root);
// Setup runs on an installation's first launch until the player completes it once
// (a marker in userdata). Installs from before the marker count once they have
// retail data and saved settings.
bool SetupComplete(const fs::path& root);
void MarkSetupComplete(const fs::path& root);
// Why Prey's engine cannot run from this installation folder (path too long for its
// file functions), or an empty string.
std::wstring InstallPathProblem(const fs::path& root);
// A Prey installation folder containing base/pak000..pak006.pk4 (valid archives).
// Returns an empty string when valid, otherwise a player-readable reason.
std::wstring RetailProblem(const fs::path& preyFolder);
// Accepts either the Prey folder or its base folder; returns the Prey folder.
fs::path NormalizeRetailFolder(const fs::path& chosen);
// Detected installation folders, most likely first (registry, moved drives, common paths).
std::vector<fs::path> FindRetailCandidates();
// Same path on every other fixed drive (installations moved to a new drive letter).
std::vector<fs::path> DriveVariants(const fs::path& path);
// Links (same volume) or copies the archives; each file appears only when complete.
void ImportRetail(const fs::path& preyFolder, const fs::path& root,
	const std::function<void(uint64_t done, uint64_t total)>& progress, const std::atomic<bool>* cancel);
// Files Reawakened derives from the imported Prey archives (the portal gun's
// openings, the Jen seam repair). They are never shipped: the bundled converter
// builds them after the retail import, or on the next setup of an installation
// that lacks them. Needed only when this installation has the converter.
bool PreyFilesNeeded(const fs::path& root);
void BuildPreyFiles(const fs::path& root,
	const std::function<void(uint64_t done, uint64_t total)>& progress, const std::atomic<bool>* cancel);

// Optional content. Reawakened (frame rate, fixes, portal tool mechanics) works
// without either; each adds assets converted from the player's own copy.
enum class Extra { Doom3, Portal };
inline constexpr Extra AllExtras[] = { Extra::Doom3, Extra::Portal };
const wchar_t* ExtraName(Extra extra);
bool ExtraInstalled(const fs::path& root, Extra extra);
// Doom 3 content comes from the original game or the BFG Edition; Portal from Portal.
struct ExtraSource { Game game = Game::Doom3; fs::path folder; GameCheck check; };
ExtraSource CheckExtraSource(Extra extra, const fs::path& chosen);
std::vector<fs::path> FindExtraSources(Extra extra);
// Installs into engine/base and records the installed files for removal.
// Doom 3 without Resurrection of Evil installs everything except the Super Shotgun.
void ImportExtra(Extra extra, const fs::path& gameFolder, const fs::path& root,
	const std::function<void(uint64_t done, uint64_t total)>& progress, const std::atomic<bool>* cancel);
void RemoveExtra(Extra extra, const fs::path& root);

// Setup window: Prey (required when missing) plus the optional games. Also opened
// from the launcher to add or remove optional content. Returns true when the
// retail data is in place afterwards.
bool RunSetup(HINSTANCE instance, const fs::path& root, HWND owner = nullptr);
// Self-test for --verify, using small stand-in archives in a temporary folder.
void VerifyRetailSetup(const fs::path& output);
