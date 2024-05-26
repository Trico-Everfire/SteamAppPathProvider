#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class SAPP {
public:
	using AppID = std::uint32_t;

	SAPP();

	[[nodiscard]] std::string_view getSteamInstallDir() const;

	[[nodiscard]] const std::vector<std::string>& getSteamLibraryDirs() const;

	[[nodiscard]] std::string getSteamSourceModDir() const;

	[[nodiscard]] std::vector<AppID> getInstalledApps() const;

	[[nodiscard]] bool isAppInstalled(AppID appID) const;

	[[nodiscard]] std::string_view getAppName(AppID appID) const;

	[[nodiscard]] std::string getAppInstallDir(AppID appID) const;

	[[nodiscard]] std::string getAppIconPath(AppID appID) const;

	[[nodiscard]] std::string getAppLogoPath(AppID appID) const;

	[[nodiscard]] std::string getAppBoxArtPath(AppID appID) const;

	[[nodiscard]] std::string getAppStoreArtPath(AppID appID) const;

	[[nodiscard]] bool isAppUsingSourceEngine(AppID appID) const;

	[[nodiscard]] bool isAppUsingSource2Engine(AppID appID) const;

	[[nodiscard]] explicit operator bool() const;

private:
	struct GameInfo {
		std::string name;
		std::string installDir;
		std::size_t libraryInstallDirsIndex;
	};

	std::unordered_map<AppID, GameInfo> gameDetails;
	std::string steamInstallDir;
	std::vector<std::string> libraryDirs;

	static std::string readTextFile(const std::string& path);
};
