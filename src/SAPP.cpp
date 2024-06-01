#include <SAPP/SAPP.h>

#include <algorithm>
#include <fstream>
#include <filesystem>
#include <ranges>
#include <unordered_set>

#ifdef _WIN32
#include <memory>
#include <Windows.h>
#endif

#include <KeyValue.h>

namespace {

std::string readTextFile(const std::string& path) {
	std::ifstream file{path};
	auto size = std::filesystem::file_size(path);
	std::string out;
	out.resize(size);
	file.read(out.data(), static_cast<std::streamsize>(size));
	return out;
}

bool isAppUsingSourceEnginePredicate(std::string_view installDir) {
	std::filesystem::directory_iterator dirIterator{installDir, std::filesystem::directory_options::skip_permission_denied};
	return std::any_of(std::filesystem::begin(dirIterator), std::filesystem::end(dirIterator), [](const auto& entry){
		return entry.is_directory() && std::filesystem::exists(entry.path() / "gameinfo.txt");
	});
}

bool isAppUsingSource2EnginePredicate(std::string_view installDir) {
	std::filesystem::directory_iterator dirIterator{installDir, std::filesystem::directory_options::skip_permission_denied};
	return std::any_of(std::filesystem::begin(dirIterator), std::filesystem::end(dirIterator), [](const auto& entry) {
		if (!entry.is_directory()) {
			return false;
		}
		if (std::filesystem::exists(entry.path() / "gameinfo.gi")) {
			return true;
		}
		std::filesystem::directory_iterator subDirIterator{entry.path(), std::filesystem::directory_options::skip_permission_denied};
		return std::any_of(std::filesystem::begin(subDirIterator), std::filesystem::end(subDirIterator), [](const auto& entry) {
			return entry.is_directory() && std::filesystem::exists(entry.path() / "gameinfo.gi");
		});
	});
}

// Note: this can't be a template because gcc threw a fit. No idea why
std::unordered_set<SAPP::AppID> getAppsKnownToUseEngine(bool(*p)(std::string_view)) {
	if (p == &::isAppUsingSourceEnginePredicate) {
		return {
#include "cache/EngineSource.inl"
		};
	} else if (p == &::isAppUsingSource2EnginePredicate) {
		return {
#include "cache/EngineSource2.inl"
		};
	}
	return {};
}

template<bool(*P)(std::string_view)>
bool isAppUsingEngine(const SAPP* sapp, SAPP::AppID appID) {
	static std::unordered_set<SAPP::AppID> knownIs = ::getAppsKnownToUseEngine(P);
	if (knownIs.contains(appID)) {
		return true;
	}

	static std::unordered_set<SAPP::AppID> knownIsNot;
	if (knownIsNot.contains(appID)) {
		return false;
	}

	if (!sapp->isAppInstalled(appID)) {
		return false;
	}

	auto installDir = sapp->getAppInstallDir(appID);
	if (std::error_code ec; !std::filesystem::exists(installDir, ec)) [[unlikely]] {
		return false;
	}

	if (P(installDir)) {
		knownIs.emplace(appID);
		return true;
	}
	knownIsNot.emplace(appID);
	return false;
}

} // namespace

SAPP::SAPP() {
	std::filesystem::path steamLocation;
	std::error_code ec;

#ifdef _WIN32
	{
		// 16383 being the maximum length of a path
		static constexpr DWORD STEAM_LOCATION_MAX_SIZE = 16383;
		std::unique_ptr<char[]> steamLocationData{new char[STEAM_LOCATION_MAX_SIZE]};

		HKEY steam;
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Valve\Steam)", 0, KEY_QUERY_VALUE | KEY_WOW64_32KEY, &steam) != ERROR_SUCCESS) {
			return;
		}

		DWORD steamLocationSize = STEAM_LOCATION_MAX_SIZE;
		if (RegQueryValueExA(steam, "InstallPath", nullptr, nullptr, reinterpret_cast<LPBYTE>(steamLocationData.get()), &steamLocationSize) != ERROR_SUCCESS) {
			return;
		}

		RegCloseKey(steam);
		steamLocation = steamLocationSize > 0 ? std::string(steamLocationData.get(), steamLocationSize - 1) : "";
	}
#else
	{
		std::filesystem::path home{std::getenv("HOME")};
#ifdef __APPLE__
		steamLocation = home / "Library" / "Application Support" / "Steam";
#else
		// Snap install takes priority, the .steam symlink may exist simultaneously with Snap installs
		steamLocation = home / "snap" / "steam" / "common" / ".steam" / "steam";

		if (!std::filesystem::exists(steamLocation, ec)) {
			// Use the regular install path
			steamLocation = home / ".steam" / "steam";
		}
#endif
	}

	if (!std::filesystem::exists(steamLocation, ec)) {
		std::string location;
		std::filesystem::path d{"cwd/steamclient64.dll"};
		for (const auto& entry : std::filesystem::directory_iterator{"/proc/"}) {
			if (std::filesystem::exists(entry / d, ec)) {
				ec.clear();
				const auto s = std::filesystem::read_symlink(entry.path() / "cwd", ec);
				if (ec) {
					continue;
				}
				location = s.string();
				break;
			}
		}
		if (location.empty()) {
			return;
		} else {
			steamLocation = location;
		}
	}
#endif

	if (!std::filesystem::exists(steamLocation.string(), ec)) {
		return;
	}
	this->steamInstallDir = steamLocation.string();

	auto libraryFoldersFilePath = steamLocation / "steamapps" / "libraryfolders.vdf";
	if (!std::filesystem::exists(libraryFoldersFilePath, ec)) {
		return;
	}

	auto libraryFoldersData = ::readTextFile(libraryFoldersFilePath.string());
	KeyValueRoot libraryFolders{libraryFoldersData.c_str()};

	if (!libraryFolders.IsValid()) {
		return;
	}
	libraryFolders.Solidify();

	auto& libraryFoldersValue = libraryFolders.Get("libraryfolders");
	if (!libraryFoldersValue.IsValid()) {
		return;
	}

	for (int i = 0; i < libraryFoldersValue.ChildCount(); i++) {
		auto& folder = libraryFoldersValue.At(i);

		std::string folderName = folder.Key().string;
		if (folderName == "TimeNextStatsReport" || folderName == "ContentStatsID") {
			continue;
		}

		auto& folderPath = folder.Get("path");
		if (!folderPath.IsValid()) {
			continue;
		}

		std::filesystem::path libraryFolderPath = folderPath.Value().string;
		{
			std::string libraryFolderPathProcessedEscapes;
			bool hitBackslash = false;
			for (char c : libraryFolderPath.string()) {
				if (hitBackslash || c != '\\') {
					libraryFolderPathProcessedEscapes += c;
					hitBackslash = false;
				} else {
					hitBackslash = true;
				}
			}
			libraryFolderPath = libraryFolderPathProcessedEscapes;
		}
		libraryFolderPath /= "steamapps";

		this->libraryDirs.push_back(libraryFolderPath.string());

		if (!std::filesystem::exists(libraryFolderPath)) {
			continue;
		}

		for (const auto& entry : std::filesystem::directory_iterator{libraryFolderPath, std::filesystem::directory_options::skip_permission_denied}) {
			auto entryName = entry.path().filename().string();
			if (!entryName.starts_with("appmanifest_") || !entryName.ends_with(".acf")) {
				continue;
			}

			auto appManifestData = ::readTextFile(entry.path().string());
			KeyValueRoot appManifest(appManifestData.c_str());

			if (!appManifest.IsValid()) {
				return;
			}
			appManifest.Solidify();

			auto& appState = appManifest.Get("AppState");
			if (!appState.IsValid()) {
				continue;
			}

			auto& appName = appState.Get("name");
			if (!appName.IsValid()) {
				continue;
			}
			auto& appInstallDir = appState.Get("installdir");
			if (!appInstallDir.IsValid()) {
				continue;
			}
			auto& appID = appState.Get("appid");
			if (!appID.IsValid()) {
				continue;
			}

			this->gameDetails[std::stoi(appID.Value().string)] = GameInfo{
					.name = appName.Value().string,
					.installDir = appInstallDir.Value().string,
					.libraryInstallDirsIndex = this->libraryDirs.size() - 1,
			};
		}
	}
}

std::string_view SAPP::getSteamInstallDir() const {
	return this->steamInstallDir;
}

const std::vector<std::string>& SAPP::getSteamLibraryDirs() const {
	return this->libraryDirs;
}

std::string SAPP::getSteamSourceModDir() const {
	return (std::filesystem::path{this->steamInstallDir} / "steamapps" / "sourcemods").string();
}

std::vector<SAPP::AppID> SAPP::getInstalledApps() const {
	auto keys = std::views::keys(this->gameDetails);
	return {keys.begin(), keys.end()};
}

bool SAPP::isAppInstalled(AppID appID) const {
	return this->gameDetails.contains(appID);
}

std::string_view SAPP::getAppName(AppID appID) const {
	if (!this->gameDetails.contains(appID)) {
		return "";
	}
	return this->gameDetails.at(appID).name;
}

std::string SAPP::getAppInstallDir(AppID appID) const {
	if (!this->gameDetails.contains(appID)) {
		return "";
	}
	return (std::filesystem::path{this->libraryDirs[this->gameDetails.at(appID).libraryInstallDirsIndex]} / "common" / this->gameDetails.at(appID).installDir).string();
}

std::string SAPP::getAppIconPath(AppID appID) const {
	if (!this->gameDetails.contains(appID)) {
		return "";
	}
	auto path = (std::filesystem::path{this->steamInstallDir} / "appcache" / "librarycache" / (std::to_string(appID) + "_icon.jpg")).string();
	if (std::error_code ec; !std::filesystem::exists(path, ec)) {
		return "";
	}
	return path;
}

std::string SAPP::getAppLogoPath(AppID appID) const {
	if (!this->gameDetails.contains(appID)) {
		return "";
	}
	auto path = (std::filesystem::path{this->steamInstallDir} / "appcache" / "librarycache" / (std::to_string(appID) + "_logo.png")).string();
	if (std::error_code ec; !std::filesystem::exists(path, ec)) {
		return "";
	}
	return path;
}

std::string SAPP::getAppBoxArtPath(AppID appID) const {
	if (!this->gameDetails.contains(appID)) {
		return "";
	}
	auto path = (std::filesystem::path{this->steamInstallDir} / "appcache" / "librarycache" / (std::to_string(appID) + "_library_600x900.jpg")).string();
	if (std::error_code ec; !std::filesystem::exists(path, ec)) {
		return "";
	}
	return path;
}

std::string SAPP::getAppStoreArtPath(AppID appID) const {
	if (!this->gameDetails.contains(appID)) {
		return "";
	}
	auto path = (std::filesystem::path{this->steamInstallDir} / "appcache" / "librarycache" / (std::to_string(appID) + "_header.jpg")).string();
	if (std::error_code ec; !std::filesystem::exists(path, ec)) {
		return "";
	}
	return path;
}

bool SAPP::isAppUsingSourceEngine(AppID appID) const {
	return ::isAppUsingEngine<::isAppUsingSourceEnginePredicate>(this, appID);
}

bool SAPP::isAppUsingSource2Engine(AppID appID) const {
	return ::isAppUsingEngine<::isAppUsingSource2EnginePredicate>(this, appID);
}

SAPP::operator bool() const {
	return !this->gameDetails.empty();
}
