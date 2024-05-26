#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#ifdef _WIN32
#include <Windows.h>
#endif

#include <KeyValue.h>

namespace sapp {

using AppID = std::uint32_t;

struct GameDetails {
	std::string name;
	std::string installDirectory;
};

class SAPP {
public:
	SAPP() {
		std::filesystem::path steamLocation;

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
			steamLocation = steamLocationData.get();
		}
#else
		{
			std::string home = std::getenv("HOME");
#ifdef __APPLE__
			steamLocation = std::filesystem::path{home} / "Library" / "Application Support" / "Steam";
#else
			steamLocation = std::filesystem::path{home} / ".steam" / "steam";
#endif
		}

		std::error_code ec;
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

		this->libraryCacheDirectory = steamLocation / "appcache" / "librarycache";

		auto libraryFoldersData = readTextFile((steamLocation / "steamapps" / "libraryfolders.vdf").string());
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

			if (!std::filesystem::exists(libraryFolderPath)) {
				continue;
			}

			for (const auto& entry : std::filesystem::directory_iterator{libraryFolderPath, std::filesystem::directory_options::skip_permission_denied}) {
				auto entryName = entry.path().filename().string();
				if (!entryName.starts_with("appmanifest_") || !entryName.ends_with(".acf")) {
					continue;
				}

				auto appManifestData = readTextFile(entry.path().string());
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
				auto& appInstallDirectory = appState.Get("installdir");
				if (!appInstallDirectory.IsValid()) {
					continue;
				}
				auto& appID = appState.Get("appid");
				if (!appID.IsValid()) {
					continue;
				}

				this->gameDetails[std::stoi(appID.Value().string)] = GameDetails{
					.name = appName.Value().string,
					.installDirectory = (libraryFolderPath / "common" / appInstallDirectory.Value().string).string(),
				};
			}
		}
	}

	[[nodiscard]] std::vector<AppID> getInstalledGameIDs() const {
		auto keys = std::views::keys(this->gameDetails);
		return {keys.begin(), keys.end()};
	}

	[[nodiscard]] bool isGameInstalled(AppID appID) const {
		return this->gameDetails.contains(appID);
	}

	[[nodiscard]] std::string_view getGameName(AppID appID) const {
		if (!this->gameDetails.contains(appID)) {
			return "";
		}
		return this->gameDetails.at(appID).name;
	}

	[[nodiscard]] std::string_view getGameInstallDirectory(AppID appID) const {
		if (!this->gameDetails.contains(appID)) {
			return "";
		}
		return this->gameDetails.at(appID).installDirectory;
	}

	[[nodiscard]] std::string getGameIconPath(AppID appID) const {
		if (!this->gameDetails.contains(appID)) {
			return "";
		}
		return (this->libraryCacheDirectory / (std::to_string(appID) + "_icon.jpg")).string();
	}

	[[nodiscard]] std::string getGameLogoPath(AppID appID) const {
		if (!this->gameDetails.contains(appID)) {
			return "";
		}
		return (this->libraryCacheDirectory / (std::to_string(appID) + "_logo.png")).string();
	}

	[[nodiscard]] std::string getGameBoxArtPath(AppID appID) const {
		if (!this->gameDetails.contains(appID)) {
			return "";
		}
		return (this->libraryCacheDirectory / (std::to_string(appID) + "_library_600x900.jpg")).string();
	}

	[[nodiscard]] std::string getGameStoreArtPath(AppID appID) const {
		if (!this->gameDetails.contains(appID)) {
			return "";
		}
		return (this->libraryCacheDirectory / (std::to_string(appID) + "_header.jpg")).string();
	}

	[[nodiscard]] bool isGameUsingSourceEngine(AppID appID) const {
		static std::unordered_set<AppID> cache;
		if (cache.contains(appID)) {
			return true;
		}

		if (!this->isGameInstalled(appID)) {
			return false;
		}

		auto installDir = this->getGameInstallDirectory(appID);
		if (!std::filesystem::exists(installDir)) {
			return false;
		}

		std::filesystem::directory_iterator dirIterator{installDir, std::filesystem::directory_options::skip_permission_denied};
		if (std::any_of(std::filesystem::begin(dirIterator), std::filesystem::end(dirIterator), [](const auto& entry){
			return std::filesystem::exists(entry.path() / "gameinfo.txt");
		})) {
			cache.emplace(appID);
			return true;
		}
		return false;
	}

	[[nodiscard]] bool isGameUsingSource2Engine(AppID appID) const {
		static std::unordered_set<AppID> cache;
		if (cache.contains(appID)) {
			return true;
		}

		if (!this->isGameInstalled(appID)) {
			return false;
		}

		auto installDir = this->getGameInstallDirectory(appID);
		if (!std::filesystem::exists(installDir)) {
			return false;
		}

		std::filesystem::directory_iterator dirIterator{installDir, std::filesystem::directory_options::skip_permission_denied};
		if (std::any_of(std::filesystem::begin(dirIterator), std::filesystem::end(dirIterator), [](const auto& entry) {
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
		})) {
			cache.emplace(appID);
			return true;
		}
		return false;
	}

	[[nodiscard]] explicit operator bool() const {
		return !this->gameDetails.empty();
	}

	[[nodiscard]] const GameDetails& operator[](AppID appID) const {
		return this->gameDetails.at(appID);
	}

private:
	std::unordered_map<AppID, GameDetails> gameDetails;
	std::filesystem::path libraryCacheDirectory;

	static std::string readTextFile(const std::string& path) {
		std::ifstream file{path};
		auto size = std::filesystem::file_size(path);
		std::string out;
		out.resize(size);
		file.read(out.data(), static_cast<std::streamsize>(size));
		return out;
	}
};

} // namespace sapp
