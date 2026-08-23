#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

struct ZyncConfig {
    std::string name = "";
    std::string entry = "main.zy";
    std::vector<std::string> linkLibs;
    std::vector<std::string> includeDirs;
    std::vector<std::string> libDirs;
    bool loaded = false;
};

std::string trimString(const std::string& s);
std::string stripQuotes(const std::string& s);
std::vector<std::string> parseTomlArray(const std::string& val);
ZyncConfig loadZyncToml(const fs::path& tomlPath = "zync.toml");
void appendLibToToml(const std::string& libName);
bool handleAddDependency(const std::string& depName);