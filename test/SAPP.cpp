#include <gtest/gtest.h>

#include <iostream>

#include <SAPP/SAPP.h>

TEST(SAPP, list_installed_games) {
	SAPP sapp;
	ASSERT_TRUE(sapp);

	std::cout << sapp.getSteamInstallDir() << std::endl;

	for (SAPP::AppID appID : sapp.getInstalledApps()) {
		std::cout << sapp.getAppName(appID) << " (" << appID << "): " << sapp.getAppInstallDir(appID) << std::endl;
	}
}
