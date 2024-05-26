#include <gtest/gtest.h>

#include <iostream>

#include <SAPP/SAPP.h>

TEST(SAPP, list_installed_apps) {
	SAPP sapp;
	ASSERT_TRUE(sapp);

	std::cout << "Steam install directory: " << sapp.getSteamInstallDir() << std::endl;

	for (auto appID : sapp.getInstalledApps()) {
		std::cout << sapp.getAppName(appID) << " (" << appID << "): " << sapp.getAppInstallDir(appID) << std::endl;
	}
}

TEST(SAPP, search_for_apps_using_engine) {
	SAPP sapp;
	ASSERT_TRUE(sapp);

	for (auto appID : sapp.getInstalledApps()) {
		if (sapp.isAppUsingSourceEngine(appID) || sapp.isAppUsingSource2Engine(appID)) {
			std::cout << sapp.getAppName(appID) << " (" << appID << "): " << sapp.getAppInstallDir(appID) << std::endl;
		}
	}
}
