#include <gtest/gtest.h>
#include <sapp/FilesystemSearchProvider.h>
#include <cstdlib>
#include <iostream>
#include <string>

char SLASH;
[[maybe_unused]] auto SLASH_HELPER = wctomb(&SLASH, std::filesystem::path::preferred_separator);

TEST(SAPP, searchForInstalledGames) {
    CFileSystemSearchProvider provider;
    ASSERT_TRUE(provider.Available());

    auto installedSteamAppCount = provider.GetNumInstalledApps();
    std::unique_ptr<uint32_t[]> steamAppIDs(provider.GetInstalledAppsEX());

    for (int i = 0; i < installedSteamAppCount; i++) {
        std::unique_ptr<CFileSystemSearchProvider::Game> steamGameInfo(provider.GetAppInstallDirEX(steamAppIDs[i]));
        std::cout << steamAppIDs[i] << ": " << steamGameInfo->library << SLASH << "common" << SLASH << steamGameInfo->installDir << std::endl;
    }
}

TEST(SAPP, searchForSourceGames) {
    CFileSystemSearchProvider provider;
    ASSERT_TRUE(provider.Available());

    auto installedSteamAppCount = provider.GetNumInstalledApps();
    std::unique_ptr<uint32_t[]> steamAppIDs(provider.GetInstalledAppsEX());

    for (int i = 0; i < installedSteamAppCount; i++) {
        if (!(provider.BIsSourceGame(steamAppIDs[i]) || provider.BIsSource2Game(steamAppIDs[i])))
            continue;

        std::unique_ptr<CFileSystemSearchProvider::Game> steamGameInfo(provider.GetAppInstallDirEX(steamAppIDs[i]));
        std::cout << steamAppIDs[i] << ": " << steamGameInfo->library << SLASH << "common" << SLASH << steamGameInfo->installDir << std::endl;
    }
}
