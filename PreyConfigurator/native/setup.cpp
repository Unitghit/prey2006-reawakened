#include "setup.h"
#include "games.h"
#include <commctrl.h>
#include <shobjidl.h>
#include <algorithm>
#include <fstream>
#include <set>
#include <stdexcept>
#include <thread>

const wchar_t* const RetailPaks[7] = { L"pak000.pk4", L"pak001.pk4", L"pak002.pk4", L"pak003.pk4",
	L"pak004.pk4", L"pak005.pk4", L"pak006.pk4" };

static fs::path RetailTarget(const fs::path& root) { return root/EngineDirectory/L"base"; }

std::wstring InstallPathProblem(const fs::path& root) {
	// Prey's file functions stop at 260 characters; the deepest installed file
	// (converted Doom 3 content) adds 86 after the installation folder.
	if (!EngineCanUse(root))
		return L"This folder's path has characters that Prey's engine cannot use with this Windows language setting, "
			L"and the drive has no short names for it. Move the Reawakened folder to a path with plain letters, "
			L"such as C:\\Games\\Prey2006 Reawakened, and start it from there.";
	const size_t length = fs::absolute(EnginePath(root)).wstring().size();
	if (length > 170)
		return L"This folder's path is " + std::to_wstring(length) + L" characters long, too long for Prey's engine (170 at most). "
			L"Move the Reawakened folder somewhere shorter, such as C:\\Games\\Prey2006 Reawakened, and start it from there.";
	return L"";
}

static fs::path SetupMarker(const fs::path& root) { return root/L"userdata/setup-complete"; }
bool SetupComplete(const fs::path& root) {
	std::error_code ec;
	return fs::exists(SetupMarker(root), ec) || (RetailReady(root) && fs::exists(root/L"Prey-settings.json", ec));
}
void MarkSetupComplete(const fs::path& root) {
	fs::create_directories(SetupMarker(root).parent_path());
	Atomic(SetupMarker(root), "Setup completed.\n");
}

bool RetailReady(const fs::path& root) {
	std::error_code ec;
	for (auto name : RetailPaks) if (!fs::is_regular_file(RetailTarget(root)/name, ec)) return false;
	return true;
}

static bool IsArchive(const fs::path& file) {
	std::ifstream in(file, std::ios::binary);
	char magic[4] = {};
	return in.read(magic, 4) && magic[0] == 'P' && magic[1] == 'K' && magic[2] == 3 && magic[3] == 4;
}

std::wstring RetailProblem(const fs::path& preyFolder) {
	std::error_code ec;
	if (preyFolder.empty() || !fs::is_directory(preyFolder, ec)) return L"That folder does not exist.";
	for (auto name : RetailPaks) {
		const auto file = preyFolder/L"base"/name;
		if (!fs::is_regular_file(file, ec)) return L"This is not a Prey (2006) installation: base\\" + std::wstring(name) + L" is missing.";
		if (!IsArchive(file)) return L"base\\" + std::wstring(name) + L" is damaged or not a Prey data archive.";
	}
	// Other id Tech 4 games (Doom 3) use the same archive names; require Prey's own content.
	const auto names = ZipNames(preyFolder/L"base/pak000.pk4");
	if (!names.count("maps/game/roadhouse.map") || !names.count("script/prey_util2.script"))
		return L"This is not Prey (2006): base\\pak000.pk4 does not contain Prey's data.";
	return L"";
}

fs::path NormalizeRetailFolder(const fs::path& chosen) {
	fs::path folder = chosen;
	while (!folder.empty() && !folder.has_filename() && folder != folder.root_path()) folder = folder.parent_path();
	std::wstring leaf = folder.filename().wstring();
	std::transform(leaf.begin(), leaf.end(), leaf.begin(), ::towlower);
	return leaf == L"base" ? folder.parent_path() : folder;
}

static std::vector<fs::path> FixedDrives() {
	std::vector<fs::path> drives;
	const DWORD mask = GetLogicalDrives();
	for (int i = 0; i < 26; ++i) {
		if (!(mask & (1u << i))) continue;
		const std::wstring root = std::wstring(1, (wchar_t)(L'A' + i)) + L":\\";
		const UINT type = GetDriveTypeW(root.c_str());
		if (type == DRIVE_FIXED || type == DRIVE_REMOVABLE) drives.emplace_back(root);
	}
	return drives;
}

std::vector<fs::path> DriveVariants(const fs::path& path) {
	std::vector<fs::path> variants;
	if (!path.has_root_name()) return variants;
	const fs::path relative = path.relative_path();
	for (const auto& drive : FixedDrives()) {
		if (_wcsicmp(drive.root_name().c_str(), path.root_name().c_str()) != 0) variants.push_back(drive/relative);
	}
	return variants;
}

std::vector<fs::path> FindRetailCandidates() { return FindGame(Game::Prey); }

struct CopyState { uint64_t base, total; const std::function<void(uint64_t, uint64_t)>* progress; const std::atomic<bool>* cancel; };

static DWORD CALLBACK CopyProgress(LARGE_INTEGER, LARGE_INTEGER copied, LARGE_INTEGER, LARGE_INTEGER, DWORD, DWORD, HANDLE, HANDLE, LPVOID data) {
	auto state = static_cast<CopyState*>(data);
	if (state->cancel && state->cancel->load()) return PROGRESS_CANCEL;
	(*state->progress)(state->base + (uint64_t)copied.QuadPart, state->total);
	return PROGRESS_CONTINUE;
}

void ImportRetail(const fs::path& preyFolder, const fs::path& root,
		const std::function<void(uint64_t, uint64_t)>& progress, const std::atomic<bool>* cancel) {
	const auto problem = RetailProblem(preyFolder);
	if (!problem.empty()) throw std::runtime_error(Utf8(problem));
	const auto target = RetailTarget(root);
	fs::create_directories(target);
	uint64_t total = 0, done = 0;
	for (auto name : RetailPaks) total += fs::file_size(preyFolder/L"base"/name);
	for (auto name : RetailPaks) {
		const auto source = preyFolder/L"base"/name, dest = target/name;
		const uint64_t size = fs::file_size(source);
		std::error_code ec;
		if (fs::is_regular_file(dest, ec) && fs::file_size(dest, ec) == size) { done += size; progress(done, total); continue; }
		const fs::path part = dest.wstring() + L".part";
		fs::remove(part, ec);
		// Same volume: a hard link shares the data instead of copying 1.7 GB.
		if (!CreateHardLinkW(part.c_str(), source.c_str(), nullptr)) {
			CopyState state{ done, total, &progress, cancel };
			BOOL cancelFlag = FALSE;
			if (!CopyFileExW(source.c_str(), part.c_str(), CopyProgress, &state, &cancelFlag, COPY_FILE_FAIL_IF_EXISTS)) {
				const DWORD error = GetLastError();
				fs::remove(part, ec);
				if (error == ERROR_REQUEST_ABORTED) throw std::runtime_error("Import cancelled.");
				throw std::runtime_error("Could not copy " + Utf8(name) + " (Windows error " + std::to_string(error) + "). Check free disk space.");
			}
		}
		fs::rename(part, dest);
		done += size; progress(done, total);
		if (cancel && cancel->load()) throw std::runtime_error("Import cancelled.");
	}
}

// ---------------------------------------------------------------------------
// Optional content

const wchar_t* ExtraName(Extra extra) { return extra == Extra::Doom3 ? L"Doom 3 weapons" : L"Portal gun model"; }
static const wchar_t* ExtraKey(Extra extra) { return extra == Extra::Doom3 ? L"doom3" : L"portal"; }
static fs::path ExtraRecord(const fs::path& root, Extra extra) {
	return root/EngineDirectory/L"base"/(std::wstring(L"reawakened-") + ExtraKey(extra) + L"-files.txt");
}
// Converted content for this test build. The bundled converter replaces this
// folder as the source; the installed result and its record stay the same.
static fs::path ExtraStage(const fs::path& root, Extra extra) { return root/L"setup-content"/ExtraKey(extra); }

bool ExtraInstalled(const fs::path& root, Extra extra) {
	std::error_code ec;
	const auto base = root/EngineDirectory/L"base";
	if (fs::is_regular_file(ExtraRecord(root, extra), ec)) return true;
	// Installations made before the record existed.
	return extra == Extra::Doom3 ? fs::is_regular_file(base/L"doom3-import-manifest.json", ec)
		: fs::is_regular_file(base/L"models/reawakened/portalgun/view/view.md5mesh", ec);
}

ExtraSource CheckExtraSource(Extra extra, const fs::path& chosen) {
	ExtraSource result;
	if (extra == Extra::Portal) {
		result.game = Game::Portal; result.folder = NormalizeGameFolder(Game::Portal, chosen);
		result.check = CheckGame(Game::Portal, result.folder);
		return result;
	}
	// One row accepts either Doom 3 edition.
	const fs::path classic = NormalizeGameFolder(Game::Doom3, chosen);
	const GameCheck classicCheck = CheckGame(Game::Doom3, classic);
	if (classicCheck.problem.empty()) return { Game::Doom3, classic, classicCheck };
	const fs::path bfg = NormalizeGameFolder(Game::Doom3BFG, chosen);
	const GameCheck bfgCheck = CheckGame(Game::Doom3BFG, bfg);
	std::error_code ec;
	if (bfgCheck.problem.empty() || fs::is_regular_file(bfg/L"base/_common.resources", ec)) return { Game::Doom3BFG, bfg, bfgCheck };
	result.folder = classic; result.check = classicCheck;
	if (classicCheck.problem.find(L"BFG") != std::wstring::npos) result.check.problem = L"This Doom 3: BFG Edition installation is incomplete or modified.";
	return result;
}

std::vector<fs::path> FindExtraSources(Extra extra) {
	if (extra == Extra::Portal) return FindGame(Game::Portal);
	// Prefer an original Doom 3 that includes Resurrection of Evil, then any
	// installation with it, then the rest.
	std::vector<fs::path> all, best;
	for (Game game : { Game::Doom3, Game::Doom3BFG }) for (const auto& p : FindGame(game)) all.push_back(p);
	for (const auto& p : all) if (CheckExtraSource(extra, p).check.expansion) best.push_back(p);
	for (const auto& p : all) if (std::find(best.begin(), best.end(), p) == best.end()) best.push_back(p);
	return best;
}

static bool SuperShotgunFile(const std::wstring& relative) {
	std::wstring lower = relative;
	std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
	return lower.find(L"supershotgun") != std::wstring::npos || lower.find(L"doublebarrel") != std::wstring::npos
		|| lower.find(L"shotgun_double") != std::wstring::npos;
}

static std::vector<std::wstring> StagedFiles(const fs::path& stage) {
	std::vector<std::wstring> files;
	std::error_code ec;
	const fs::path folder = LongPath(stage);
	if (!fs::is_directory(folder, ec)) return files;
	for (auto it = fs::recursive_directory_iterator(folder, ec); it != fs::recursive_directory_iterator(); it.increment(ec))
		if (it->is_regular_file(ec)) files.push_back(it->path().lexically_relative(folder).generic_wstring());
	std::sort(files.begin(), files.end());
	return files;
}

// The bundled converter (tools/importer, built with PyInstaller) and the model
// decompiler it uses for Portal. Absent in development builds, which then use
// pre-converted content from setup-content.
static fs::path ImporterExe(const fs::path& root) { return root/EngineDirectory/L"importer/reawakened-import.exe"; }
static fs::path CrowbarExe(const fs::path& root) { return root/EngineDirectory/L"importer/crowbar/Crowbar.exe"; }
static fs::path ImportWork(const fs::path&) {
	// A short temporary folder: conversion uses deep paths, and the model
	// decompiler does not accept extended-length ones.
	wchar_t temp[MAX_PATH + 1] = {};
	GetTempPathW(MAX_PATH + 1, temp);
	return fs::path(temp)/L"Reawakened-import";
}
// Shipped files an import replaced, restored when the content is removed.
static fs::path ReplacedFolder(const fs::path& root, Extra extra) {
	return root/EngineDirectory/L"replaced"/ExtraKey(extra);
}

// Runs the converter hidden, forwarding its "PROGRESS <0-100>" lines. Output
// is logged next to the work folder for diagnosis.
static void RunImporter(const fs::path& root, const std::vector<std::wstring>& args,
		const std::function<void(uint64_t, uint64_t)>& progress, const std::atomic<bool>* cancel, const fs::path& log) {
	std::wstring command = QuoteArgument(ImporterExe(root).wstring());
	for (const auto& a : args) command += L" " + QuoteArgument(a);
	SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
	HANDLE readPipe = nullptr, writePipe = nullptr;
	if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) throw std::runtime_error("Cannot start the content converter.");
	SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
	HANDLE logFile = CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	STARTUPINFOW si{ sizeof(si) };
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdOutput = writePipe;
	si.hStdError = logFile != INVALID_HANDLE_VALUE ? logFile : writePipe;
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	PROCESS_INFORMATION pi{};
	std::vector<wchar_t> buffer(command.begin(), command.end()); buffer.push_back(0);
	const BOOL started = CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
		nullptr, ImporterExe(root).parent_path().c_str(), &si, &pi);
	CloseHandle(writePipe);
	if (logFile != INVALID_HANDLE_VALUE) CloseHandle(logFile);
	if (!started) { CloseHandle(readPipe); throw std::runtime_error("Cannot start the content converter."); }
	// A watcher ends the converter on cancel; its closed pipe ends the read loop.
	std::atomic<bool> finished{ false };
	std::thread watcher([&]() {
		while (!finished) {
			if (WaitForSingleObject(pi.hProcess, 100) == WAIT_OBJECT_0) break;
			if (cancel && cancel->load()) { TerminateProcess(pi.hProcess, 1); break; }
		}
	});
	std::string pending, error;
	char chunk[512]; DWORD got = 0;
	while (ReadFile(readPipe, chunk, sizeof(chunk), &got, nullptr) && got) {
		pending.append(chunk, got);
		for (size_t end; (end = pending.find('\n')) != std::string::npos; pending.erase(0, end + 1)) {
			std::string line = pending.substr(0, end);
			if (!line.empty() && line.back() == '\r') line.pop_back();
			if (line.rfind("PROGRESS ", 0) == 0) progress((uint64_t)std::max(0, std::min(100, atoi(line.c_str() + 9))), 100);
			else if (line.rfind("ERROR ", 0) == 0) error = line.substr(6);
		}
	}
	WaitForSingleObject(pi.hProcess, INFINITE);
	finished = true; watcher.join();
	DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code);
	CloseHandle(pi.hThread); CloseHandle(pi.hProcess); CloseHandle(readPipe);
	if (cancel && cancel->load()) throw std::runtime_error("Import cancelled.");
	if (code != 0) throw std::runtime_error(error.empty() ? "The content conversion failed." : error);
}

// Extended-length paths accept only backslashes; records use '/'.
static fs::path Under(const fs::path& folder, const std::wstring& relative) {
	fs::path r(relative); r.make_preferred(); return folder/r;
}

void ImportExtra(Extra extra, const fs::path& gameFolder, const fs::path& root,
		const std::function<void(uint64_t, uint64_t)>& progress, const std::atomic<bool>* cancel) {
	const auto source = CheckExtraSource(extra, gameFolder);
	if (!source.check.problem.empty()) throw std::runtime_error(Utf8(source.check.problem));
	const auto base = LongPath(root/EngineDirectory/L"base");
	fs::path stage;
	std::error_code ec;
	// Conversion is 80% of the progress bar, installing the result the rest.
	auto scaled = [&](uint64_t done, uint64_t total, uint64_t from, uint64_t span) {
		progress(from + (total ? done * span / total : span), 1000);
	};
	if (fs::is_regular_file(ImporterExe(root), ec)) {
		const auto work = ImportWork(root);
		fs::create_directories(work);
		stage = work/(std::wstring(ExtraKey(extra)) + L"-output");
		std::vector<std::wstring> args;
		if (extra == Extra::Doom3)
			args = { L"doom3", L"--game", source.folder.wstring(), L"--prey-base", (root/EngineDirectory/L"base").wstring(), L"--output", stage.wstring() };
		else
			args = { L"portal", L"--game", source.folder.wstring(), L"--crowbar", CrowbarExe(root).wstring(),
				L"--output", stage.wstring(), L"--work", (work/L"portal-work").wstring() };
		RunImporter(root, args, [&](uint64_t d, uint64_t t) { scaled(d, t, 0, 800); }, cancel,
			work/(std::wstring(ExtraKey(extra)) + L".log"));
	} else {
		stage = ExtraStage(root, extra);
	}
	auto files = StagedFiles(stage);
	stage = LongPath(stage);
	if (files.empty()) throw std::runtime_error(Utf8(std::wstring(L"This build cannot convert the ") + ExtraName(extra) + L"."));
	if (extra == Extra::Doom3 && !source.check.expansion)
		files.erase(std::remove_if(files.begin(), files.end(), SuperShotgunFile), files.end());
	// Files this content installed before are replaced freely; any other
	// existing file is a shipped one, kept aside for removal.
	std::set<std::wstring> ours;
	{
		std::ifstream in(ExtraRecord(root, extra), std::ios::binary);
		std::string line; while (std::getline(in, line)) if (!line.empty()) ours.insert(Wide(line));
	}
	const auto replaced = LongPath(ReplacedFolder(root, extra));
	uint64_t total = 0, done = 0;
	for (const auto& f : files) total += fs::file_size(Under(stage, f));
	std::string record;
	for (const auto& f : files) {
		if (cancel && cancel->load()) throw std::runtime_error("Import cancelled.");
		const auto dest = Under(base, f);
		if (fs::is_regular_file(dest, ec) && !ours.count(f) && !fs::exists(Under(replaced, f), ec)) {
			fs::create_directories((Under(replaced, f)).parent_path());
			fs::copy_file(dest, Under(replaced, f));
		}
		fs::create_directories(dest.parent_path());
		const fs::path part = dest.wstring() + L".part";
		fs::copy_file(Under(stage, f), part, fs::copy_options::overwrite_existing);
		fs::rename(part, dest);
		record += Utf8(f) + "\n";
		done += fs::file_size(dest); scaled(done, total, 800, 200);
	}
	// Written last: content counts as installed only once every file is in place.
	Atomic(ExtraRecord(root, extra), record);
	if (stage.parent_path() == LongPath(ImportWork(root))) fs::remove_all(stage, ec);
}

void RemoveExtra(Extra extra, const fs::path& root) {
	const auto base = LongPath(root/EngineDirectory/L"base");
	std::vector<std::wstring> files;
	std::ifstream in(ExtraRecord(root, extra), std::ios::binary);
	if (in) { std::string line; while (std::getline(in, line)) if (!line.empty()) files.push_back(Wide(line)); }
	else files = StagedFiles(ExtraStage(root, extra));
	if (files.empty() && ExtraInstalled(root, extra)) throw std::runtime_error("Cannot determine which files to remove.");
	std::error_code ec;
	for (const auto& f : files) {
		const auto path = (Under(base, f)).lexically_normal();
		// Records are plain text; never follow one outside engine/base.
		if (path.wstring().rfind(base.lexically_normal().wstring(), 0) != 0) continue;
		fs::remove(path, ec);
		for (auto dir = path.parent_path(); dir != base && fs::is_empty(dir, ec); dir = dir.parent_path()) fs::remove(dir, ec);
	}
	in.close();
	// Put back the shipped files the import replaced.
	const auto replaced = LongPath(ReplacedFolder(root, extra));
	for (const auto& f : StagedFiles(replaced)) {
		fs::create_directories((Under(base, f)).parent_path());
		fs::copy_file(Under(replaced, f), Under(base, f), fs::copy_options::overwrite_existing);
	}
	fs::remove_all(replaced, ec);
	if (fs::is_empty(replaced.parent_path(), ec)) fs::remove(replaced.parent_path(), ec);
	fs::remove(ExtraRecord(root, extra), ec);
}

// ---------------------------------------------------------------------------
// Setup window

namespace {
constexpr UINT WM_SETUP_PROGRESS = WM_APP + 1, WM_SETUP_DONE = WM_APP + 2;
constexpr int InstallId = 3002, QuitId = 3003, RowBase = 3100;	// row r: +10r +0 edit, +1 browse, +2 include
enum RowKind { PreyRow, Doom3Row, PortalRow, RowCount };
struct SetupRow {
	HWND include{}, path{}, browse{}, status{};
	bool installed = false;		// content already present
	bool valid = false;
	std::wstring message;
};
struct SetupUi {
	fs::path root;
	HWND window{}, owner{}, status{}, progress{}, install{}, quit{};
	SetupRow rows[RowCount];
	HFONT font{}, bold{}, heading{};
	std::thread worker;
	std::atomic<bool> cancel{ false };
	bool busy = false, manage = false;
	std::wstring error, done;
};

std::wstring Text(HWND h) { wchar_t text[32768]; GetWindowTextW(h, text, 32768); return text; }
bool Included(const SetupUi& ui, int r) { return r == PreyRow || SendMessageW(ui.rows[r].include, BM_GETCHECK, 0, 0) == BST_CHECKED; }
Extra RowExtra(int r) { return r == Doom3Row ? Extra::Doom3 : Extra::Portal; }

// Actions the Install button performs, in order.
struct Step { int row; bool remove; fs::path folder; };
std::vector<Step> Plan(const SetupUi& ui) {
	std::vector<Step> steps;
	for (int r = 0; r < RowCount; ++r) {
		const auto& row = ui.rows[r];
		if (Included(ui, r) && !row.installed) steps.push_back({ r, false, r == PreyRow ? NormalizeRetailFolder(Text(row.path)) : CheckExtraSource(RowExtra(r), Text(row.path)).folder });
		if (!Included(ui, r) && row.installed) steps.push_back({ r, true, {} });
	}
	return steps;
}

void Refresh(SetupUi& ui) {
	bool ready = true;
	for (int r = 0; r < RowCount; ++r) {
		auto& row = ui.rows[r];
		const bool include = Included(ui, r);
		const bool needsFolder = include && !row.installed;
		std::wstring message;
		if (row.installed) message = include ? L"Installed." : L"Will be removed.";
		else if (!include) message = L"Not installed. Reawakened works without it; you can add it later from the launcher.";
		else if (Text(row.path).empty()) { message = L"Not found automatically. Click Browse and select the game's installation folder."; row.valid = false; }
		else if (r == PreyRow) {
			const auto problem = RetailProblem(NormalizeRetailFolder(Text(row.path)));
			row.valid = problem.empty(); message = row.valid ? L"Prey (2006) found." : problem;
		} else {
			const auto source = CheckExtraSource(RowExtra(r), Text(row.path));
			row.valid = source.check.problem.empty();
			if (!row.valid) message = source.check.problem;
			else if (r == PortalRow) message = L"Portal found.";
			else message = std::wstring(GameName(source.game)) + L" found" + (source.check.expansion
				? L", with Resurrection of Evil (all seven weapons)."
				: L". Resurrection of Evil was not found, so the Super Shotgun is not included.");
		}
		if (needsFolder && !row.valid) ready = false;
		SetWindowTextW(row.status, message.c_str());
		EnableWindow(row.path, needsFolder && !ui.busy); EnableWindow(row.browse, needsFolder && !ui.busy);
		if (row.include) EnableWindow(row.include, !ui.busy);
	}
	const bool changes = !Plan(ui).empty();
	const auto pathProblem = InstallPathProblem(ui.root);
	// First run can finish with nothing to change (Prey already imported).
	EnableWindow(ui.install, ready && (changes || !ui.manage) && !ui.busy && pathProblem.empty());
	if (!ui.busy) SetWindowTextW(ui.status, !pathProblem.empty() ? pathProblem.c_str() : !ready ? L"" : changes ? (ui.manage ? L"Ready. Click Apply." : L"Ready. Click Install.") : ui.manage ? L"No changes." : L"Ready. Click Install to continue.");
}

void Browse(SetupUi& ui, int r) {
	IFileOpenDialog* dialog = nullptr;
	if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return;
	DWORD options = 0; dialog->GetOptions(&options); dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
	dialog->SetTitle(r == PreyRow ? L"Select your Prey (2006) installation folder"
		: r == Doom3Row ? L"Select your Doom 3 or Doom 3: BFG Edition installation folder" : L"Select your Portal installation folder");
	if (SUCCEEDED(dialog->Show(ui.window))) {
		IShellItem* item = nullptr;
		if (SUCCEEDED(dialog->GetResult(&item))) {
			PWSTR chosen = nullptr;
			if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &chosen))) {
				const fs::path folder = r == PreyRow ? NormalizeRetailFolder(chosen) : CheckExtraSource(RowExtra(r), chosen).folder;
				SetWindowTextW(ui.rows[r].path, folder.c_str());
				CoTaskMemFree(chosen);
			}
			item->Release();
		}
	}
	dialog->Release();
	Refresh(ui);
}

void StartInstall(SetupUi& ui) {
	const auto steps = Plan(ui);
	// Saves made with an optional weapon selected need its files to load.
	std::wstring removing;
	for (const auto& s : steps) if (s.remove) removing += std::wstring(removing.empty() ? L"" : L" and the ") + ExtraName(RowExtra(s.row));
	if (!removing.empty()) {
		const std::wstring text = L"Remove the " + removing + L"?\n\nSaves made while using them will not load until the content "
			L"is added again. Adding it again, from any copy of the game, makes those saves work as before.";
		if (MessageBoxW(ui.window, text.c_str(), L"Prey2006 Reawakened", MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2) != IDOK) return;
	}
	ui.busy = true; ui.cancel = false; ui.error.clear();
	Refresh(ui);
	EnableWindow(ui.install, FALSE);
	SetWindowTextW(ui.quit, L"Cancel");
	HWND window = ui.window;
	const fs::path root = ui.root;
	ui.worker = std::thread([&ui, window, root, steps]() {
		size_t index = 0;
		auto report = [&](uint64_t done, uint64_t total) {
			const uint64_t part = total ? done * 1000 / total : 1000;
			PostMessageW(window, WM_SETUP_PROGRESS, (WPARAM)((index * 1000 + part) / steps.size()), (LPARAM)steps[index].row);
		};
		try {
			for (; index < steps.size(); ++index) {
				const auto& s = steps[index];
				report(0, 1);
				if (s.row == PreyRow) ImportRetail(s.folder, root, report, &ui.cancel);
				else if (s.remove) RemoveExtra(RowExtra(s.row), root);
				else ImportExtra(RowExtra(s.row), s.folder, root, report, &ui.cancel);
			}
			PostMessageW(window, WM_SETUP_DONE, 1, 0);
		} catch (const std::exception& e) {
			ui.error = ErrorText(e.what());
			PostMessageW(window, WM_SETUP_DONE, 0, 0);
		}
	});
}

LRESULT CALLBACK SetupProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
	auto ui = reinterpret_cast<SetupUi*>(GetWindowLongPtrW(h, GWLP_USERDATA));
	if (msg == WM_NCCREATE) { ui = (SetupUi*)((CREATESTRUCTW*)lp)->lpCreateParams; ui->window = h; SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)ui); }
	if (!ui) return DefWindowProcW(h, msg, wp, lp);
	switch (msg) {
	case WM_COMMAND: {
		const int id = LOWORD(wp);
		if (id == InstallId) StartInstall(*ui);
		else if (id == QuitId) SendMessageW(h, WM_CLOSE, 0, 0);
		else if (id >= RowBase && id < RowBase + 10 * RowCount && !ui->busy) {
			const int r = (id - RowBase) / 10, part = (id - RowBase) % 10;
			if (part == 1) Browse(*ui, r);
			else if ((part == 0 && HIWORD(wp) == EN_CHANGE) || part == 2) Refresh(*ui);
		}
		return 0;
	}
	case WM_SETUP_PROGRESS: {
		SendMessageW(ui->progress, PBM_SETPOS, wp, 0);
		const int r = (int)lp;
		const bool removing = ui->rows[r].installed;
		SetWindowTextW(ui->status, (std::wstring(removing ? L"Removing " : L"Installing ") + (r == PreyRow ? L"Prey data" : ExtraName(RowExtra(r))) + L"...").c_str());
		return 0;
	}
	case WM_SETUP_DONE:
		if (ui->worker.joinable()) ui->worker.join();
		ui->busy = false;
		for (int r = 0; r < RowCount; ++r) ui->rows[r].installed = r == PreyRow ? RetailReady(ui->root) : ExtraInstalled(ui->root, RowExtra(r));
		if (wp) { ui->done = L"ok"; DestroyWindow(h); return 0; }
		SendMessageW(ui->progress, PBM_SETPOS, 0, 0);
		SetWindowTextW(ui->quit, ui->manage ? L"Close" : L"Quit");
		Refresh(*ui); SetWindowTextW(ui->status, ui->error.c_str());
		return 0;
	case WM_CLOSE:
		if (ui->busy) { ui->cancel = true; SetWindowTextW(ui->status, L"Cancelling..."); return 0; }
		DestroyWindow(h); return 0;
	case WM_CTLCOLORSTATIC: SetBkMode((HDC)wp, TRANSPARENT); return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
	case WM_DESTROY: if (ui->owner) { EnableWindow(ui->owner, TRUE); SetActiveWindow(ui->owner); } PostQuitMessage(0); return 0;
	}
	return DefWindowProcW(h, msg, wp, lp);
}
}

bool RunSetup(HINSTANCE instance, const fs::path& root, HWND owner) {
	const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	WNDCLASSEXW wc{ sizeof(wc) };
	wc.hInstance = instance; wc.lpfnWndProc = SetupProc; wc.lpszClassName = L"PreySettingsSetup";
	wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
	wc.hIcon = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
	RegisterClassExW(&wc);

	SetupUi ui; ui.root = root; ui.owner = owner; ui.manage = RetailReady(root) && SetupComplete(root);
	const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	HWND window = CreateWindowExW(WS_EX_CONTROLPARENT, wc.lpszClassName, ui.manage ? L"Prey2006 Reawakened Import Games" : L"Prey2006 Reawakened Setup",
		style, CW_USEDEFAULT, CW_USEDEFAULT, 10, 10, owner, nullptr, instance, &ui);
	if (!window) throw std::runtime_error("Cannot create setup window");

	const UINT dpi = GetDpiForWindow(window);
	auto S = [dpi](int v) { return MulDiv(v, (int)dpi, 96); };
	NONCLIENTMETRICSW metrics{ sizeof(metrics) };
	SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, dpi);
	ui.font = CreateFontIndirectW(&metrics.lfMessageFont);
	LOGFONTW lf = metrics.lfMessageFont; lf.lfWeight = FW_BOLD; ui.bold = CreateFontIndirectW(&lf);
	lf.lfHeight = -MulDiv(13, (int)dpi, 72); ui.heading = CreateFontIndirectW(&lf);

	const int width = S(620), margin = S(18), inner = width - 2 * margin;
	int y = margin;
	auto add = [&](const wchar_t* cls, const wchar_t* text, DWORD st, int x, int yy, int w, int hgt, int id, HFONT f) {
		HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | st, x, yy, w, hgt, window, (HMENU)(INT_PTR)id, instance, nullptr);
		SendMessageW(c, WM_SETFONT, (WPARAM)f, TRUE); return c;
	};
	add(L"STATIC", ui.manage ? L"Import games" : L"Set up Prey2006 Reawakened", SS_NOPREFIX, margin, y, inner, S(28), 0, ui.heading); y += S(34);
	add(L"STATIC", L"Uses your own copies of the games. Steam and non-Steam both work, and your installed games are never changed.",
		SS_NOPREFIX, margin, y, inner, S(36), 0, ui.font); y += S(44);

	const wchar_t* titles[RowCount] = { L"Prey (2006)  (required)", L"Doom 3 weapons  (optional)", L"Portal gun model  (optional)" };
	const wchar_t* hints[RowCount] = { L"Its seven data archives (base\\pak000.pk4 to pak006.pk4) are copied into Reawakened.",
		L"From Doom 3 or Doom 3: BFG Edition. Resurrection of Evil adds the Super Shotgun.",
		L"From Portal. Without it the portal tool still works, shown without a gun model." };
	for (int r = 0; r < RowCount; ++r) {
		auto& row = ui.rows[r];
		const int id = RowBase + 10 * r;
		if (r == PreyRow) add(L"STATIC", titles[r], SS_NOPREFIX, margin, y, inner, S(20), 0, ui.bold);
		else row.include = add(L"BUTTON", titles[r], WS_TABSTOP | BS_AUTOCHECKBOX, margin, y, inner, S(20), id + 2, ui.bold);
		y += S(22);
		add(L"STATIC", hints[r], SS_NOPREFIX, margin + S(18), y, inner - S(18), S(18), 0, ui.font); y += S(22);
		row.path = add(L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, margin + S(18), y, inner - S(18) - S(96), S(24), id, ui.font);
		row.browse = add(L"BUTTON", L"Browse...", WS_TABSTOP | BS_PUSHBUTTON, width - margin - S(88), y - S(1), S(88), S(26), id + 1, ui.font); y += S(30);
		row.status = add(L"STATIC", L"", SS_NOPREFIX, margin + S(18), y, inner - S(18), S(34), 0, ui.font); y += S(44);

		// Current state and auto-detection. Optional content is offered when found.
		row.installed = r == PreyRow ? RetailReady(root) : ExtraInstalled(root, RowExtra(r));
		const auto found = r == PreyRow ? FindRetailCandidates() : FindExtraSources(RowExtra(r));
		if (!found.empty()) SetWindowTextW(row.path, found.front().c_str());
		if (row.include) SendMessageW(row.include, BM_SETCHECK, row.installed || (!ui.manage && !found.empty()) ? BST_CHECKED : BST_UNCHECKED, 0);
	}
	ui.status = add(L"STATIC", L"", SS_NOPREFIX, margin, y, inner, S(52), 0, ui.font); y += S(56);
	ui.progress = add(PROGRESS_CLASSW, L"", 0, margin, y, inner, S(16), 0, ui.font); y += S(28);
	SendMessageW(ui.progress, PBM_SETRANGE32, 0, 1000);
	ui.install = add(L"BUTTON", ui.manage ? L"Apply" : L"Install", WS_TABSTOP | BS_DEFPUSHBUTTON, width - margin - S(196), y, S(96), S(28), InstallId, ui.font);
	ui.quit = add(L"BUTTON", ui.manage ? L"Close" : L"Quit", WS_TABSTOP | BS_PUSHBUTTON, width - margin - S(96), y, S(96), S(28), QuitId, ui.font);
	y += S(28) + margin;

	RECT frame{ 0, 0, width, y };
	AdjustWindowRectExForDpi(&frame, style, FALSE, WS_EX_CONTROLPARENT, dpi);
	RECT work; SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
	const int w = frame.right - frame.left, hgt = frame.bottom - frame.top;
	SetWindowPos(window, nullptr, work.left + (work.right - work.left - w) / 2, work.top + std::max(0, (int)(work.bottom - work.top - hgt) / 2), w, hgt, SWP_NOZORDER);
	Refresh(ui);

	if (owner) EnableWindow(owner, FALSE);
	ShowWindow(window, SW_SHOWNORMAL);
	MSG message;
	while (GetMessageW(&message, nullptr, 0, 0) > 0) {
		if (!IsDialogMessageW(window, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
	}
	if (ui.worker.joinable()) { ui.cancel = true; ui.worker.join(); }
	DeleteObject(ui.font); DeleteObject(ui.bold); DeleteObject(ui.heading);
	UnregisterClassW(wc.lpszClassName, instance);
	if (SUCCEEDED(com)) CoUninitialize();
	// First run completes only through Install; Quit leaves the launcher.
	const bool complete = RetailReady(root) && (ui.manage || ui.done == L"ok");
	if (complete) MarkSetupComplete(root);
	return complete;
}

// ---------------------------------------------------------------------------
// Self-test

void VerifyRetailSetup(const fs::path& output) {
	auto require = [](bool ok, const char* message) { if (!ok) throw std::runtime_error(message); };
	const fs::path work = output/(L"retail setup Unicode 雪 " + std::to_wstring(GetTickCount64()));
	const fs::path prey = work/L"Prey 2006", root = work/L"Reawakened";
	fs::create_directories(prey/L"base");
	for (auto name : RetailPaks) {
		std::ofstream out(prey/L"base"/name, std::ios::binary);
		out.write("PK\x03\x04", 4);
		out << Utf8(name) << std::string(4096, 'x');
	}
	WriteTestZip(prey/L"base/pak000.pk4", { "maps/game/roadhouse.map", "script/prey_util2.script" });
	require(RetailProblem(prey).empty(), "Valid retail folder rejected");
	WriteTestZip(work/L"Doom 3/base/pak000.pk4", { "def/player.def" });
	for (int k = 1; k < 7; ++k) { std::ofstream d3(work/L"Doom 3/base"/RetailPaks[k], std::ios::binary); d3.write("PK", 4); }
	require(!RetailProblem(work/L"Doom 3").empty(), "Doom 3 accepted as Prey");
	require(NormalizeRetailFolder(prey/L"base") == prey, "Base folder not normalized to the Prey folder");
	require(NormalizeRetailFolder(prey.wstring() + L"\\") == prey, "Trailing separator not normalized");
	fs::rename(prey/L"base/pak003.pk4", prey/L"base/pak003.bak");
	require(!RetailProblem(prey).empty(), "Missing archive accepted");
	fs::rename(prey/L"base/pak003.bak", prey/L"base/pak003.pk4");
	{ std::ofstream bad(prey/L"base/pak005.pk4", std::ios::binary | std::ios::trunc); bad << "not an archive"; }
	require(!RetailProblem(prey).empty(), "Damaged archive accepted");
	{ std::ofstream good(prey/L"base/pak005.pk4", std::ios::binary | std::ios::trunc); good.write("PK\x03\x04", 4); good << std::string(64, 'y'); }

	require(!RetailReady(root), "Empty installation reported ready");
	uint64_t last = 0, total = 0;
	ImportRetail(prey, root, [&](uint64_t d, uint64_t t) { last = d; total = t; }, nullptr);
	require(RetailReady(root) && last == total && total > 0, "Import incomplete");
	for (auto name : RetailPaks) {
		require(fs::file_size(root/EngineDirectory/L"base"/name) == fs::file_size(prey/L"base"/name), "Imported size differs");
		require(!fs::exists(root/EngineDirectory/L"base"/(std::wstring(name) + L".part")), "Partial file left behind");
	}
	ImportRetail(prey, root, [](uint64_t, uint64_t) {}, nullptr);	// already present: no-op
	std::atomic<bool> cancel{ true };
	fs::remove(root/EngineDirectory/L"base/pak000.pk4");
	bool cancelled = false;
	try { ImportRetail(prey, root, [](uint64_t, uint64_t) {}, &cancel); } catch (const std::exception&) { cancelled = true; }
	require(cancelled, "Cancelled import did not stop");

	// Optional content: install from the staged conversion, Super Shotgun only with
	// Resurrection of Evil, record-driven removal. Retail data stays untouched.
	const fs::path doom = work/L"Doom 3 Ωmega";
	WriteTestZip(doom/L"base/pak000.pk4", { "models/md5/weapons/shotgun_view/viewshotgun.md5mesh", "models/md5/weapons/machinegun_view/viewmachinegun.md5mesh",
		"models/md5/weapons/chaingun_view/viewchaingun.md5mesh", "models/md5/weapons/plasmagun_view/viewplasmagun.md5mesh",
		"models/md5/weapons/rocketlauncher_view/viewrocketlauncher.md5mesh", "models/md5/weapons/bfg_view/viewbfg.md5mesh" });
	require(CheckExtraSource(Extra::Doom3, doom/L"base").folder == doom, "Doom 3 base folder not normalized");
	require(!CheckExtraSource(Extra::Doom3, prey).check.problem.empty(), "Prey accepted as Doom 3");
	const auto stage = root/L"setup-content/doom3";
	for (auto name : { L"def/doom3_shotgun.def", L"def/doom3_supershotgun.def", L"models/w/doublebarrel_view/v.md5mesh", L"doom3-import-manifest.json", L"guis/hud/icons.gui" }) {
		fs::create_directories((stage/name).parent_path()); std::ofstream(stage/name) << "x";
	}
	ImportRetail(prey, root, [](uint64_t, uint64_t) {}, nullptr);
	require(!ExtraInstalled(root, Extra::Doom3), "Doom 3 content reported before install");
	const auto base = root/EngineDirectory/L"base";
	auto contents = [](const fs::path& f) { std::ifstream in(f, std::ios::binary); return std::string(std::istreambuf_iterator<char>(in), {}); };
	fs::create_directories(base/L"guis/hud"); std::ofstream(base/L"guis/hud/icons.gui") << "shipped";
	ImportExtra(Extra::Doom3, doom, root, [](uint64_t, uint64_t) {}, nullptr);
	require(contents(base/L"guis/hud/icons.gui") == "x", "Imported file did not replace the shipped one");
	require(ExtraInstalled(root, Extra::Doom3) && fs::exists(base/L"def/doom3_shotgun.def"), "Doom 3 content not installed");
	require(!fs::exists(base/L"def/doom3_supershotgun.def") && !fs::exists(base/L"models/w"), "Super Shotgun installed without the expansion");
	WriteTestZip(doom/L"d3xp/pak000.pk4", { "models/md5/weapons/doublebarrel_view/new/dbviewmesh.md5mesh" });
	ImportExtra(Extra::Doom3, doom, root, [](uint64_t, uint64_t) {}, nullptr);
	require(fs::exists(base/L"def/doom3_supershotgun.def"), "Super Shotgun missing with the expansion");
	RemoveExtra(Extra::Doom3, root);
	require(!ExtraInstalled(root, Extra::Doom3) && !fs::exists(base/L"def/doom3_shotgun.def") && !fs::exists(base/L"models"), "Doom 3 content not removed");
	require(contents(base/L"guis/hud/icons.gui") == "shipped" && !fs::exists(root/EngineDirectory/L"replaced"), "Shipped file not restored after removal");
	require(RetailReady(root), "Removing optional content touched the retail data");
	bool noConverter = false;
	try { ImportExtra(Extra::Portal, doom, root, [](uint64_t, uint64_t) {}, nullptr); } catch (const std::exception&) { noConverter = true; }
	require(noConverter && !ExtraInstalled(root, Extra::Portal), "Invalid Portal source accepted");

	require(InstallPathProblem(L"C:\\Games\\Prey2006 Reawakened").empty(), "Short installation path rejected");
	require(!InstallPathProblem(fs::path(L"C:\\") / std::wstring(180, L'x')).empty(), "Over-long installation path accepted");
	require(RetailReady(root) && !SetupComplete(root), "Setup counted complete before the player finished it");
	MarkSetupComplete(root);
	require(SetupComplete(root), "Completed setup not recorded");
	const auto variants = DriveVariants(L"Q:\\Games\\Prey 2006");
	require(!variants.empty() && std::all_of(variants.begin(), variants.end(), [](const fs::path& p) {
		return p.relative_path() == fs::path(L"Games\\Prey 2006"); }), "Drive variants wrong");
	(void)FindRetailCandidates();	// must not throw on this machine
	std::error_code ec; fs::remove_all(work, ec);
	Atomic(output/L"setup-pass.txt", "PASS: retail validation (missing/damaged archives, base folder, trailing separator), import with progress, "
		"no partial files, repeat import, cancellation, optional content install/expansion/removal with shipped-file restore, installation path length, first-run completion, drive-letter variants, detection.\n");
}
