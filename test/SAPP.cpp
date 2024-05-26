#include <gtest/gtest.h>

#include <iostream>

#include <SAPP/SAPP.h>

using namespace sapp;

TEST(SAPP, list_installed_games) {
	SAPP sapp;

	const auto& allGameDetails = sapp.getAllGameDetails();
	ASSERT_FALSE(allGameDetails.empty());

	for (const auto& [appID, gameDetails] : allGameDetails) {
		std::cout << gameDetails.name << " (" << appID << "): " << gameDetails.installDirectory << std::endl;
	}
}
