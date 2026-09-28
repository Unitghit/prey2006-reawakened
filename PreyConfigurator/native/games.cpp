#include "games.h"
#include "setup.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Archive directories

static std::string Lower(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)(c == '\\' ? '/' : ::tolower(c)); });
	return s;
}

static bool ReadAt(std::ifstream& in, uint64_t offset, void* data, size_t size) {
	in.clear(); in.seekg((std::streamoff)offset);
	return (bool)in.read((char*)data, (std::streamsize)size);
}

std::set<std::string> ZipNames(const fs::path& archive) {
	std::set<std::string> names;
	std::ifstream in(archive, std::ios::binary);
	if (!in) return names;
	in.seekg(0, std::ios::end);
	const uint64_t size = (uint64_t)in.tellg();
	const size_t tail = (size_t)std::min<uint64_t>(size, 65536 + 22);
	std::vector<unsigned char> end(tail);
	if (!ReadAt(in, size - tail, end.data(), tail)) return names;
	for (size_t i = tail - 22 + 1; i-- > 0;) {
		if (end[i] == 0x50 && end[i + 1] == 0x4b && end[i + 2] == 5 && end[i + 3] == 6) {
			uint16_t count; uint32_t dirSize, dirOffset;
			memcpy(&count, &end[i + 10], 2); memcpy(&dirSize, &end[i + 12], 4); memcpy(&dirOffset, &end[i + 16], 4);
			std::vector<unsigned char> dir(dirSize);
			if (!ReadAt(in, dirOffset, dir.data(), dirSize)) return names;
			size_t p = 0;
			for (uint16_t n = 0; n < count && p + 46 <= dir.size(); ++n) {
				if (!(dir[p] == 0x50 && dir[p + 1] == 0x4b && dir[p + 2] == 1 && dir[p + 3] == 2)) break;
				uint16_t nameLength, extra, comment;
				memcpy(&nameLength, &dir[p + 28], 2); memcpy(&extra, &dir[p + 30], 2); memcpy(&comment, &dir[p + 32], 2);
				if (p + 46 + nameLength > dir.size()) break;
				names.insert(Lower(std::string((const char*)&dir[p + 46], nameLength)));
				p += 46 + nameLength + extra + comment;
			}
			break;
		}
	}
	return names;
}

static uint32_t Big32(const unsigned char* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

// Doom 3 BFG Edition .resources: big-endian header and entry fields, with
// little-endian name lengths (as written by the BFG engine).
std::set<std::string> ResourcesNames(const fs::path& archive) {
	std::set<std::string> names;
	std::ifstream in(archive, std::ios::binary);
	unsigned char header[12];
	if (!in || !ReadAt(in, 0, header, 12) || Big32(header) != 0xD000000Du) return names;
	const uint32_t offset = Big32(header + 4), size = Big32(header + 8);
	std::vector<unsigned char> toc(size);
	if (size < 4 || !ReadAt(in, offset, toc.data(), size)) return names;
	const uint32_t count = Big32(toc.data());
	size_t p = 4;
	for (uint32_t i = 0; i < count && p + 4 <= toc.size(); ++i) {
		uint32_t length; memcpy(&length, &toc[p], 4); p += 4;
		if (p + length + 8 > toc.size()) break;
		names.insert(Lower(std::string((const char*)&toc[p], length)));
		p += length + 8;
	}
	return names;
}

// Valve VPK (v1 and v2) directory tree.
std::set<std::string> VpkNames(const fs::path& directoryFile) {
	std::set<std::string> names;
	std::ifstream in(directoryFile, std::ios::binary);
	unsigned char header[28];
	if (!in || !ReadAt(in, 0, header, 12)) return names;
	uint32_t signature, version, treeSize;
	memcpy(&signature, header, 4); memcpy(&version, header + 4, 4); memcpy(&treeSize, header + 8, 4);
	if (signature != 0x55AA1234u || (version != 1 && version != 2)) return names;
	const uint32_t start = version == 1 ? 12 : 28;
	std::vector<char> tree(treeSize);
	if (!ReadAt(in, start, tree.data(), treeSize)) return names;
	size_t p = 0;
	auto next = [&]() -> std::string {
		const size_t begin = p;
		while (p < tree.size() && tree[p]) ++p;
		std::string s(tree.data() + begin, p - begin);
		if (p < tree.size()) ++p;
		return s;
	};
	while (p < tree.size()) {
		const std::string extension = next();
		if (extension.empty()) break;
		while (p < tree.size()) {
			const std::string folder = next();
			if (folder.empty()) break;
			while (p < tree.size()) {
				const std::string file = next();
				if (file.empty()) break;
				if (p + 18 > tree.size()) return names;
				uint16_t preload; memcpy(&preload, &tree[p + 4], 2);
				p += 18 + preload;
				names.insert(Lower((folder == " " ? "" : folder + "/") + file + "." + extension));
			}
		}
	}
	return names;
}

// ---------------------------------------------------------------------------
// Registry, Steam and drives

static std::wstring RegValue(HKEY hive, const std::wstring& key, const wchar_t* value, DWORD view) {
	wchar_t data[4096]; DWORD size = sizeof(data);
	if (RegGetValueW(hive, key.c_str(), value, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | view, nullptr, data, &size) == ERROR_SUCCESS) return data;
	return L"";
}

static std::vector<std::wstring> RegSubkeys(HKEY hive, const std::wstring& key, DWORD view) {
	std::vector<std::wstring> keys;
	HKEY handle;
	const REGSAM sam = KEY_READ | (view == RRF_SUBKEY_WOW6432KEY ? KEY_WOW64_32KEY : KEY_WOW64_64KEY);
	if (RegOpenKeyExW(hive, key.c_str(), 0, sam, &handle) != ERROR_SUCCESS) return keys;
	wchar_t name[512];
	for (DWORD i = 0; ; ++i) {
		DWORD length = 512;
		if (RegEnumKeyExW(handle, i, name, &length, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
		keys.emplace_back(name);
	}
	RegCloseKey(handle);
	return keys;
}

static const DWORD Views[] = { RRF_SUBKEY_WOW6432KEY, RRF_SUBKEY_WOW6464KEY };

std::vector<fs::path> SteamLibraries() {
	std::vector<fs::path> steam, libraries;
	for (DWORD view : Views) {
		auto path = RegValue(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", view);
		if (!path.empty()) steam.emplace_back(path);
		path = RegValue(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam", L"InstallPath", view);
		if (!path.empty()) steam.emplace_back(path);
	}
	auto add = [&](const fs::path& p) {
		const auto norm = fs::path(p).lexically_normal();
		for (const auto& l : libraries) if (_wcsicmp(l.c_str(), norm.c_str()) == 0) return;
		libraries.push_back(norm);
	};
	for (const auto& root : steam) {
		add(root);
		std::ifstream vdf(root/L"steamapps/libraryfolders.vdf");
		std::string line;
		while (std::getline(vdf, line)) {
			const auto key = line.find("\"path\"");
			if (key == std::string::npos) continue;
			const auto open = line.find('"', key + 6), close = line.rfind('"');
			if (open == std::string::npos || close <= open) continue;
			std::string value = line.substr(open + 1, close - open - 1), unescaped;
			for (size_t i = 0; i < value.size(); ++i) {
				if (value[i] == '\\' && i + 1 < value.size() && value[i + 1] == '\\') ++i;
				unescaped += value[i];
			}
			add(Wide(unescaped));
		}
	}
	return libraries;
}

fs::path SteamAppFolder(int appId) {
	for (const auto& library : SteamLibraries()) {
		std::ifstream manifest(library/(L"steamapps/appmanifest_" + std::to_wstring(appId) + L".acf"));
		std::string line;
		while (std::getline(manifest, line)) {
			const auto key = line.find("\"installdir\"");
			if (key == std::string::npos) continue;
			const auto open = line.find('"', key + 12), close = line.rfind('"');
			if (open != std::string::npos && close > open) return library/L"steamapps/common"/Wide(line.substr(open + 1, close - open - 1));
		}
	}
	return {};
}

static std::vector<fs::path> Drives() {
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

// ---------------------------------------------------------------------------
// Games

const wchar_t* GameName(Game game) {
	switch (game) {
	case Game::Prey: return L"Prey (2006)";
	case Game::Doom3: return L"Doom 3";
	case Game::Doom3BFG: return L"Doom 3: BFG Edition";
	case Game::Portal: return L"Portal";
	}
	return L"";
}
bool GameRequired(Game game) { return game == Game::Prey; }

static const char* const Doom3Views[] = { "shotgun_view/viewshotgun", "machinegun_view/viewmachinegun", "chaingun_view/viewchaingun",
	"plasmagun_view/viewplasmagun", "rocketlauncher_view/viewrocketlauncher", "bfg_view/viewbfg" };
static const char* const SuperShotgunView = "doublebarrel_view/new/dbviewmesh";

static std::set<std::string> FolderZipNames(const fs::path& folder) {
	std::set<std::string> names;
	std::error_code ec;
	if (!fs::is_directory(folder, ec)) return names;
	for (const auto& entry : fs::directory_iterator(folder, ec)) {
		std::wstring ext = entry.path().extension().wstring();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
		if (ext == L".pk4") { auto n = ZipNames(entry.path()); names.insert(n.begin(), n.end()); }
	}
	return names;
}

static std::set<std::string> FolderResourcesNames(const fs::path& base) {
	std::set<std::string> names;
	std::error_code ec;
	if (!fs::is_directory(base, ec)) return names;
	for (auto it = fs::recursive_directory_iterator(base, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
		std::wstring ext = it->path().extension().wstring();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
		if (ext == L".resources") { auto n = ResourcesNames(it->path()); names.insert(n.begin(), n.end()); }
	}
	return names;
}

static bool HasBfgResources(const fs::path& folder) {
	std::error_code ec;
	return fs::is_regular_file(folder/L"base/_common.resources", ec);
}

static GameCheck CheckDoom3(const fs::path& folder) {
	GameCheck result;
	std::error_code ec;
	if (!fs::is_directory(folder, ec)) { result.problem = L"That folder does not exist."; return result; }
	const auto base = FolderZipNames(folder/L"base");
	if (base.empty()) {
		result.problem = HasBfgResources(folder)
			? L"This is Doom 3: BFG Edition. Select it in the BFG Edition row instead."
			: L"This is not a Doom 3 installation: no data archives (base\\*.pk4) were found.";
		return result;
	}
	if (base.count("maps/game/roadhouse.map")) { result.problem = L"This is Prey (2006). Select it in the Prey row instead."; return result; }
	for (auto view : Doom3Views) {
		if (!base.count(std::string("models/md5/weapons/") + view + ".md5mesh")) {
			result.problem = L"This Doom 3 installation is incomplete or modified: the " + Wide(std::string(view).substr(0, std::string(view).find('_'))) + L" weapon model is missing.";
			return result;
		}
	}
	const auto expansion = FolderZipNames(folder/L"d3xp");
	result.expansion = expansion.count(std::string("models/md5/weapons/") + SuperShotgunView + ".md5mesh") > 0;
	return result;
}

static GameCheck CheckDoom3BFG(const fs::path& folder) {
	GameCheck result;
	std::error_code ec;
	if (!fs::is_directory(folder, ec)) { result.problem = L"That folder does not exist."; return result; }
	if (!HasBfgResources(folder)) {
		if (CheckDoom3(folder).problem.empty()) result.problem = L"This is the original Doom 3. Select it in the Doom 3 row instead.";
		else if (FolderZipNames(folder/L"base").count("maps/game/roadhouse.map")) result.problem = L"This is Prey (2006). Select it in the Prey row instead.";
		else result.problem = L"This is not a Doom 3: BFG Edition installation: base\\_common.resources was not found.";
		return result;
	}
	const auto names = FolderResourcesNames(folder/L"base");
	for (auto view : Doom3Views) {
		if (!names.count(std::string("generated/rendermodels/models/md5/weapons/") + view + ".bmd5mesh")) {
			result.problem = L"This BFG Edition installation is incomplete or modified: the " + Wide(std::string(view).substr(0, std::string(view).find('_'))) + L" weapon model is missing.";
			return result;
		}
	}
	result.expansion = names.count(std::string("generated/rendermodels/models/md5/weapons/") + SuperShotgunView + ".bmd5mesh") > 0;
	return result;
}

static const char* const PortalFiles[] = {
	"models/weapons/v_portalgun.mdl", "models/weapons/v_portalgun.vvd", "models/weapons/v_portalgun.dx90.vtx",
	"models/weapons/w_portalgun.mdl", "models/weapons/w_portalgun.vvd", "models/weapons/w_portalgun.dx90.vtx",
	"materials/models/weapons/v_models/v_portalgun/v_portalgun.vtf", "materials/models/weapons/v_models/v_portalgun/v_hands.vtf",
	"materials/models/weapons/v_models/v_portalgun/v_portalgun_normal.vtf", "materials/models/weapons/w_models/portalgun/w_portalgun.vtf",
	"materials/sprites/hud/portal_crosshairs.vtf", "sound/weapons/portalgun/portalgun_shoot_blue1.wav" };
static const char* const PortalShared = "materials/effects/energyball.vtf";

// A file is available when packed in the game's VPK or present loose under it.
static bool Available(const std::set<std::string>& packed, const fs::path& looseRoot, const char* name) {
	std::error_code ec;
	return packed.count(name) || fs::is_regular_file(looseRoot/Wide(name), ec);
}

static GameCheck CheckPortal(const fs::path& folder) {
	GameCheck result;
	std::error_code ec;
	if (!fs::is_directory(folder, ec)) { result.problem = L"That folder does not exist."; return result; }
	if (!fs::is_directory(folder/L"portal", ec)) { result.problem = L"This is not a Portal installation: the portal folder was not found."; return result; }
	const auto packed = VpkNames(folder/L"portal/portal_pak_dir.vpk");
	for (auto name : PortalFiles) {
		if (!Available(packed, folder/L"portal", name)) {
			result.problem = L"This Portal installation is incomplete: " + Wide(name) + L" was not found.";
			return result;
		}
	}
	if (!Available(packed, folder/L"portal", PortalShared)) {
		const auto shared = VpkNames(folder/L"hl2/hl2_textures_dir.vpk");
		if (!Available(shared, folder/L"hl2", PortalShared)) {
			result.problem = L"This Portal installation is incomplete: hl2\\hl2_textures_dir.vpk (shared textures) was not found.";
			return result;
		}
	}
	return result;
}

GameCheck CheckGame(Game game, const fs::path& folder) {
	switch (game) {
	case Game::Prey: { GameCheck r; r.problem = RetailProblem(folder); return r; }
	case Game::Doom3: return CheckDoom3(folder);
	case Game::Doom3BFG: return CheckDoom3BFG(folder);
	case Game::Portal: return CheckPortal(folder);
	}
	return GameCheck{ L"Unknown game." };
}

static std::wstring LowerLeaf(const fs::path& p) {
	std::wstring leaf = p.filename().wstring();
	std::transform(leaf.begin(), leaf.end(), leaf.begin(), ::towlower);
	return leaf;
}

fs::path NormalizeGameFolder(Game game, const fs::path& chosen) {
	fs::path folder = chosen;
	while (!folder.empty() && !folder.has_filename() && folder != folder.root_path()) folder = folder.parent_path();
	const std::wstring leaf = LowerLeaf(folder);
	// A subfolder inside the installation.
	if (leaf == L"base" && game != Game::Portal) return folder.parent_path();
	if (leaf == L"d3xp" && game == Game::Doom3) return folder.parent_path();
	if ((leaf == L"portal" || leaf == L"hl2") && game == Game::Portal && !CheckGame(game, folder.parent_path()).problem.size()) return folder.parent_path();
	// A parent folder containing the game (for example steamapps\common).
	if (!CheckGame(game, folder).problem.empty()) {
		const wchar_t* children[] = { L"Prey", L"Prey 2006", L"Doom 3", L"DOOM 3", L"DOOM 3 BFG Edition", L"Doom 3 BFG Edition", L"Portal" };
		for (auto child : children) {
			if (CheckGame(game, folder/child).problem.empty()) return folder/child;
		}
	}
	return folder;
}

std::vector<fs::path> FindGame(Game game) {
	std::vector<fs::path> ordered;
	auto push = [&](const fs::path& p) { if (!p.empty()) ordered.push_back(p); };
	// Steam (every library), including installations on other drives.
	const int steamApps[] = { game == Game::Prey ? 3970 : game == Game::Doom3 ? 9050 : game == Game::Doom3BFG ? 208200 : 400,
		game == Game::Doom3 ? 9070 : 0 };
	for (int app : steamApps) if (app) push(SteamAppFolder(app));
	// Game-specific and generic Windows registrations (retail, GOG, installers).
	const wchar_t* displayPrefix = game == Game::Prey ? L"Prey" : game == Game::Doom3 ? L"Doom 3" : game == Game::Doom3BFG ? L"Doom 3 BFG" : L"Portal";
	for (DWORD view : Views) {
		for (HKEY hive : { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER }) {
			std::vector<std::wstring> keys;
			if (game == Game::Prey) keys = { L"SOFTWARE\\Human Head Studios\\Prey", L"SOFTWARE\\2K Games\\Prey", L"SOFTWARE\\3D Realms\\Prey" };
			if (game == Game::Doom3) keys = { L"SOFTWARE\\id\\Doom 3", L"SOFTWARE\\id Software\\Doom 3", L"SOFTWARE\\Activision\\Doom 3" };
			for (const auto& key : keys) {
				for (const wchar_t* value : { L"InstallPath", L"InstallDir", L"Path" }) push(RegValue(hive, key, value, view));
				const auto exe = RegValue(hive, key, L"InstallExe", view);
				if (!exe.empty()) push(fs::path(exe).parent_path());
			}
		}
		const std::wstring uninstall = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
		for (const auto& sub : RegSubkeys(HKEY_LOCAL_MACHINE, uninstall, view)) {
			const auto display = RegValue(HKEY_LOCAL_MACHINE, uninstall + L"\\" + sub, L"DisplayName", view);
			if (_wcsnicmp(display.c_str(), displayPrefix, wcslen(displayPrefix)) == 0 || (display.find(displayPrefix) != std::wstring::npos && game != Game::Prey))
				push(RegValue(HKEY_LOCAL_MACHINE, uninstall + L"\\" + sub, L"InstallLocation", view));
		}
		const std::wstring gog = L"SOFTWARE\\GOG.com\\Games";
		for (const auto& sub : RegSubkeys(HKEY_LOCAL_MACHINE, gog, view)) {
			const auto name = RegValue(HKEY_LOCAL_MACHINE, gog + L"\\" + sub, L"gameName", view);
			if (name.find(displayPrefix) != std::wstring::npos || _wcsnicmp(name.c_str(), displayPrefix, wcslen(displayPrefix)) == 0)
				push(RegValue(HKEY_LOCAL_MACHINE, gog + L"\\" + sub, L"path", view));
		}
	}
	// Registered folders that moved to another drive letter.
	const size_t registered = ordered.size();
	for (size_t i = 0; i < registered; ++i) for (const auto& moved : DriveVariants(ordered[i])) ordered.push_back(moved);
	// Common folders on every drive, including Steam libraries Steam no longer knows.
	std::vector<const wchar_t*> names;
	if (game == Game::Prey) names = { L"Prey", L"Prey 2006" };
	if (game == Game::Doom3) names = { L"Doom 3", L"DOOM 3" };
	if (game == Game::Doom3BFG) names = { L"DOOM 3 BFG Edition", L"Doom 3 BFG Edition", L"Doom 3 BFG" };
	if (game == Game::Portal) names = { L"Portal" };
	for (const auto& drive : Drives()) {
		for (const wchar_t* parent : { L"", L"Games", L"Program Files (x86)", L"Program Files", L"GOG Games", L"GOG Galaxy\\Games",
				L"Program Files (x86)\\Steam\\steamapps\\common", L"SteamLibrary\\steamapps\\common", L"Steam\\steamapps\\common" }) {
			for (auto name : names) push(drive/parent/name);
		}
	}
	std::vector<fs::path> valid;
	for (const auto& raw : ordered) {
		fs::path path = NormalizeGameFolder(game, fs::path(raw).make_preferred().lexically_normal());
		std::error_code ec;
		const fs::path real = fs::canonical(path, ec);	// actual on-disk spelling and case
		if (!ec) path = real;
		if (std::find_if(valid.begin(), valid.end(), [&](const fs::path& p) { return _wcsicmp(p.c_str(), path.c_str()) == 0; }) != valid.end()) continue;
		if (CheckGame(game, path).problem.empty()) valid.push_back(path);
	}
	return valid;
}

// ---------------------------------------------------------------------------
// Self-test with small stand-in installations

void WriteTestZip(const fs::path& file, const std::vector<std::string>& names) {
	fs::create_directories(file.parent_path());
	std::ofstream out(file, std::ios::binary);
	std::string central; uint32_t offset = 0;
	auto u16 = [](std::string& s, uint16_t v) { s.append((const char*)&v, 2); };
	auto u32 = [](std::string& s, uint32_t v) { s.append((const char*)&v, 4); };
	std::string local;
	for (const auto& name : names) {
		std::string h; u32(h, 0x04034b50); u16(h, 20); u16(h, 0); u16(h, 0); u32(h, 0); u32(h, 0); u32(h, 0); u32(h, 0); u16(h, (uint16_t)name.size()); u16(h, 0);
		h += name;
		std::string c; u32(c, 0x02014b50); u16(c, 20); u16(c, 20); u16(c, 0); u16(c, 0); u32(c, 0); u32(c, 0); u32(c, 0); u32(c, 0);
		u16(c, (uint16_t)name.size()); u16(c, 0); u16(c, 0); u16(c, 0); u16(c, 0); u32(c, 0); u32(c, offset); c += name;
		local += h; central += c; offset += (uint32_t)h.size();
	}
	std::string e; u32(e, 0x06054b50); u16(e, 0); u16(e, 0); u16(e, (uint16_t)names.size()); u16(e, (uint16_t)names.size());
	u32(e, (uint32_t)central.size()); u32(e, offset); u16(e, 0);
	out << local << central << e;
}

static void WriteResources(const fs::path& file, const std::vector<std::string>& names) {
	fs::create_directories(file.parent_path());
	auto be = [](std::string& s, uint32_t v) { char b[4] = { (char)(v >> 24), (char)(v >> 16), (char)(v >> 8), (char)v }; s.append(b, 4); };
	std::string toc; be(toc, (uint32_t)names.size());
	for (const auto& n : names) { uint32_t l = (uint32_t)n.size(); toc.append((const char*)&l, 4); toc += n; be(toc, 0); be(toc, 0); }
	std::string head; be(head, 0xD000000D); be(head, 12); be(head, (uint32_t)toc.size());
	std::ofstream(file, std::ios::binary) << head << toc;
}

static void WriteVpk(const fs::path& file, const std::vector<std::string>& names) {
	fs::create_directories(file.parent_path());
	// Group by extension and folder, as Valve's tree does.
	std::map<std::string, std::map<std::string, std::vector<std::string>>> tree;
	for (const auto& n : names) {
		const auto dot = n.rfind('.'), slash = n.rfind('/');
		tree[n.substr(dot + 1)][slash == std::string::npos ? " " : n.substr(0, slash)].push_back(n.substr(slash + 1, dot - slash - 1));
	}
	std::string t;
	for (const auto& [ext, folders] : tree) {
		t += ext; t += '\0';
		for (const auto& [folder, files] : folders) {
			t += folder; t += '\0';
			for (const auto& f : files) { t += f; t += '\0'; t.append(16, '\0'); t += "\xff\xff"; }
			t += '\0';
		}
		t += '\0';
	}
	t += '\0';
	std::string h; uint32_t v[3] = { 0x55AA1234u, 1, (uint32_t)t.size() }; h.append((const char*)v, 12);
	std::ofstream(file, std::ios::binary) << h << t;
}

void VerifyGames(const fs::path& output) {
	auto require = [](bool ok, const char* message) { if (!ok) throw std::runtime_error(message); };
	// Short temporary root: loose Portal paths are deep and must stay under MAX_PATH.
	(void)output;
	const fs::path work = fs::temp_directory_path()/(L"rw 雪 " + std::to_wstring(GetTickCount64()));
	std::vector<std::string> classic;
	for (auto v : Doom3Views) classic.push_back(std::string("models/md5/weapons/") + v + ".md5mesh");
	WriteTestZip(work/L"Doom 3/base/pak000.pk4", classic);
	auto doom = CheckGame(Game::Doom3, work/L"Doom 3");
	require(doom.problem.empty() && !doom.expansion, "Classic Doom 3 not recognised");
	WriteTestZip(work/L"Doom 3/d3xp/pak000.pk4", { std::string("models/md5/weapons/") + SuperShotgunView + ".md5mesh" });
	require(CheckGame(Game::Doom3, work/L"Doom 3").expansion, "Resurrection of Evil not recognised");
	require(NormalizeGameFolder(Game::Doom3, work/L"Doom 3/base") == work/L"Doom 3", "Doom 3 base folder not normalized");
	require(NormalizeGameFolder(Game::Doom3, work) == work/L"Doom 3", "Doom 3 parent folder not normalized");

	std::vector<std::string> bfg;
	for (auto v : Doom3Views) bfg.push_back(std::string("generated/rendermodels/models/md5/weapons/") + v + ".bmd5mesh");
	WriteResources(work/L"DOOM 3 BFG Edition/base/_common.resources", bfg);
	auto b = CheckGame(Game::Doom3BFG, work/L"DOOM 3 BFG Edition");
	require(b.problem.empty() && !b.expansion, "BFG Edition not recognised");
	WriteResources(work/L"DOOM 3 BFG Edition/base/maps/d3ctf1.resources", { std::string("generated/rendermodels/models/md5/weapons/") + SuperShotgunView + ".bmd5mesh" });
	require(CheckGame(Game::Doom3BFG, work/L"DOOM 3 BFG Edition").expansion, "BFG Super Shotgun in map archives not recognised");
	require(CheckGame(Game::Doom3, work/L"DOOM 3 BFG Edition").problem.find(L"BFG") != std::wstring::npos, "BFG folder in the Doom 3 row not explained");
	require(CheckGame(Game::Doom3BFG, work/L"Doom 3").problem.find(L"original Doom 3") != std::wstring::npos, "Classic folder in the BFG row not explained");

	std::vector<std::string> portal(std::begin(PortalFiles), std::end(PortalFiles));
	WriteVpk(work/L"Portal/portal/portal_pak_dir.vpk", portal);
	require(!CheckGame(Game::Portal, work/L"Portal").problem.empty(), "Portal without shared textures accepted");
	WriteVpk(work/L"Portal/hl2/hl2_textures_dir.vpk", { PortalShared });
	require(CheckGame(Game::Portal, work/L"Portal").problem.empty(), "Steam-style Portal not recognised");
	require(NormalizeGameFolder(Game::Portal, work/L"Portal/portal") == work/L"Portal", "Portal subfolder not normalized");
	// Loose (extracted) files, as some non-Steam copies have.
	for (const auto& name : portal) { fs::create_directories((work/L"Loose Portal/portal"/Wide(name)).parent_path()); std::ofstream(work/L"Loose Portal/portal"/Wide(name)) << "x"; }
	fs::create_directories(work/L"Loose Portal/hl2/materials/effects"); std::ofstream(work/L"Loose Portal/hl2/materials/effects/energyball.vtf") << "x";
	require(CheckGame(Game::Portal, work/L"Loose Portal").problem.empty(), "Loose-file Portal not recognised");
	fs::remove(work/L"Loose Portal/portal/models/weapons/w_portalgun.mdl");
	require(!CheckGame(Game::Portal, work/L"Loose Portal").problem.empty(), "Incomplete Portal accepted");
	require(!CheckGame(Game::Portal, work/L"Doom 3").problem.empty(), "Doom 3 accepted as Portal");

	for (Game g : AllGames) (void)FindGame(g);	// must not throw on this machine
	std::error_code ec; fs::remove_all(work, ec);
	Atomic(output/L"games-pass.txt", "PASS: classic Doom 3 and Resurrection of Evil, BFG Edition incl. map archives, edition mix-ups explained, "
		"Steam-style and loose Portal, shared textures, incomplete installs, folder normalization, detection.\n");
}
