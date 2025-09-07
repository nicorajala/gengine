#include "Engine/prefs.hpp"
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

int readPreferredGraphicsAPI(const std::string& cfgPath) {
	std::ifstream cfgFile(cfgPath);
	if (!cfgFile.is_open()) return 0;							// Config file doesn't exist, return default (0 = OpenGL)

	std::string line;
	while (std::getline(cfgFile, line)) {
		if (line.rfind("graphics_api=", 0) == 0) {
			std::string val = line.substr(strlen("graphics_api="));
			if (val == "dx11" || val == "d3d11" || val == "directx") return 1;
			return 0;
		}
	}

	// If we reach here, the setting wasn't found or was invalid, return default
	return 0;
}

bool writePreferredGraphicsAPI(int api, const std::string& cfgPath) {
	std::ofstream cfgFile(cfgPath, std::ios::out | std::ios::trunc);
	if (!cfgFile.is_open()) return false;						// Failed to open file for writing

	cfgFile << "graphics_api=" << (api == 1 ? "dx11" : "opengl") << "\n";
	cfgFile.close();
	return true;
}