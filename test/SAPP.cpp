#include <gtest/gtest.h>

#include <iostream>

#include <SAPP/SAPP.h>

using namespace sapp;

TEST(SAPP, list_installed_games) {
	SAPP sapp;
	ASSERT_TRUE(sapp);

	for (AppID appID : sapp.getInstalledGameIDs()) {
		std::cout << sapp.getGameName(appID) << " (" << appID << "): " << sapp.getGameInstallDirectory(appID) << std::endl;
	}
}
