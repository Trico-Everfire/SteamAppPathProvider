#include <gtest/gtest.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <filesystem>
#include "sapp/SteamAppPathProvider.h"

char SLASH;
[[maybe_unused]] auto SLASH_HELPER = wctomb(&SLASH, std::filesystem::path::preferred_separator);

//Caching only has impact on source engine game fetches, we don't do tests for SAPP usages that don't use it.
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

TEST(SAPP, searchForSourceGamesCached) {
    constexpr bool cacheSource = true;
    CFileSystemSearchProvider provider{cacheSource, cacheSource};
    ASSERT_TRUE(provider.Available());

    auto installedSteamAppCount = provider.GetNumInstalledApps();
    std::unique_ptr<uint32_t[]> steamAppIDs(provider.GetInstalledAppsEX());

    for (int i = 0; i < installedSteamAppCount; i++) {
        if (!(provider.BIsSourceGame(steamAppIDs[i]) || provider.BIsSource2Game(steamAppIDs[i])))
            continue;

        std::unique_ptr<CFileSystemSearchProvider::Game> steamGameInfo(provider.GetAppInstallDirEX(steamAppIDs[i]));
        std::cout << steamAppIDs[i] << ": " << steamGameInfo->library << SLASH << "common" << SLASH << steamGameInfo->installDir << " " << (provider.BIsSourceGame(steamAppIDs[i]) ? 1: 2) << std::endl;
    }
}

TEST(SAPP, searchForSourceGamesUncached) {
    constexpr bool cacheSource = false;
    CFileSystemSearchProvider provider{cacheSource, cacheSource};
    ASSERT_TRUE(provider.Available());

    auto installedSteamAppCount = provider.GetNumInstalledApps();
    std::unique_ptr<uint32_t[]> steamAppIDs(provider.GetInstalledAppsEX());

    for (int i = 0; i < installedSteamAppCount; i++) {
        if (!(provider.BIsSourceGame(steamAppIDs[i]) || provider.BIsSource2Game(steamAppIDs[i])))
            continue;

        std::unique_ptr<CFileSystemSearchProvider::Game> steamGameInfo(provider.GetAppInstallDirEX(steamAppIDs[i]));
        std::cout << steamAppIDs[i] << ": " << steamGameInfo->library << SLASH << "common" << SLASH << steamGameInfo->installDir << " " << (provider.BIsSourceGame(steamAppIDs[i]) ? 1: 2) << std::endl;
    }
}

TEST(SAPP, searchForSourceGamesCachedStressTest) {
    constexpr bool cacheSource = true;
    CFileSystemSearchProvider provider{cacheSource, cacheSource};
    ASSERT_TRUE(provider.Available());

    auto installedSteamAppCount = provider.GetNumInstalledApps();
    std::unique_ptr<uint32_t[]> steamAppIDs(provider.GetInstalledAppsEX());


    for(int j = 0; j < 100; j++) //We repeat the evaluation of "is a source game" 100 times to stress test the caching system over the traditional method.
    {
        for (int i = 0; i < installedSteamAppCount; i++) {
            if (!(provider.BIsSourceGame(steamAppIDs[i]) || provider.BIsSource2Game(steamAppIDs[i])))
                continue;

            std::unique_ptr<CFileSystemSearchProvider::Game> steamGameInfo(provider.GetAppInstallDirEX(steamAppIDs[i]));
            std::cout << steamAppIDs[i] << ": " << steamGameInfo->library << SLASH << "common" << SLASH << steamGameInfo->installDir << " " << (provider.BIsSourceGame(steamAppIDs[i]) ? 1: 2) << std::endl;
        }
    }
}

TEST(SAPP, searchForSourceGamesUncachedStressTest) {
    constexpr bool cacheSource = false;
    CFileSystemSearchProvider provider{cacheSource, cacheSource};
    ASSERT_TRUE(provider.Available());

    auto installedSteamAppCount = provider.GetNumInstalledApps();
    std::unique_ptr<uint32_t[]> steamAppIDs(provider.GetInstalledAppsEX());


    for(int j = 0; j < 100; j++) //We repeat the evaluation of "is a source game" 100 times to stress test the caching system over the traditional method.
    {
        for (int i = 0; i < installedSteamAppCount; i++) {
            if (!(provider.BIsSourceGame(steamAppIDs[i]) || provider.BIsSource2Game(steamAppIDs[i])))
                continue;

            std::unique_ptr<CFileSystemSearchProvider::Game> steamGameInfo(provider.GetAppInstallDirEX(steamAppIDs[i]));
            std::cout << steamAppIDs[i] << ": " << steamGameInfo->library << SLASH << "common" << SLASH << steamGameInfo->installDir << " " << (provider.BIsSourceGame(steamAppIDs[i]) ? 1: 2) << std::endl;
        }
    }
}
