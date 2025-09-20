#ifndef PREFS_HPP
#define PREFS_HPP

#include <string>

int readPreferredGraphicsAPI(const std::string& cfgPath = "editor_prefs.cfg");
bool writePreferredGraphicsAPI(int api, const std::string& cfgPath = "editor_prefs.cfg");

#endif