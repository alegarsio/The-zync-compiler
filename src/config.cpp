#include "../include/config.hpp"
#include "../include/dependencies/crow.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cctype>

std::string trimString(const std::string& s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(static_cast<unsigned char>(*start))) start++;
    auto end = s.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(static_cast<unsigned char>(*end)));
    return std::string(start, end + 1);
}

std::string stripQuotes(const std::string& s) {
    std::string t = trimString(s);
    if (t.size() >= 2 && ((t.front() == '"' && t.back() == '"') || (t.front() == '\'' && t.back() == '\''))) {
        return t.substr(1, t.size() - 2);
    }
    return t;
}

std::vector<std::string> parseTomlArray(const std::string& val) {
    std::vector<std::string> result;
    size_t openBracket = val.find('[');
    size_t closeBracket = val.find(']');
    if (openBracket == std::string::npos || closeBracket == std::string::npos) return result;

    std::string inner = val.substr(openBracket + 1, closeBracket - openBracket - 1);
    std::stringstream ss(inner);
    std::string item;
    while (std::getline(ss, item, ',')) {
        std::string cleaned = stripQuotes(item);
        if (!cleaned.empty()) {
            result.push_back(cleaned);
        }
    }
    return result;
}

ZyncConfig loadZyncToml(const fs::path& tomlPath) {
    ZyncConfig config;
    if (!fs::exists(tomlPath)) return config;

    std::ifstream in(tomlPath);
    if (!in.is_open()) return config;

    std::string line;
    std::string currentSection = "";

    while (std::getline(in, line)) {
        std::string t = trimString(line);
        if (t.empty() || t.front() == '#') continue;

        if (t.front() == '[' && t.back() == ']') {
            currentSection = t.substr(1, t.size() - 2);
            continue;
        }

        size_t eqPos = t.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = trimString(t.substr(0, eqPos));
        std::string val = trimString(t.substr(eqPos + 1));

        if (currentSection == "package") {
            if (key == "name") config.name = stripQuotes(val);
            else if (key == "entry") config.entry = stripQuotes(val);
        } else if (currentSection == "dependencies") {
            if (key == "link") {
                config.linkLibs = parseTomlArray(val);
            } else if (key == "include_dirs" || key == "includes") {
                config.includeDirs = parseTomlArray(val);
            } else if (key == "lib_dirs" || key == "libs") {
                config.libDirs = parseTomlArray(val);
            }
        }
    }

    if (config.entry.empty()) {
        config.entry = "main.zy";
    }

    config.loaded = true;
    return config;
}

void appendLibToToml(const std::string& libName) {
    fs::path tomlPath = "zync.toml";
    if (!fs::exists(tomlPath)) return;

    std::ifstream in(tomlPath);
    if (!in.is_open()) return;

    std::stringstream buffer;
    std::string line;
    bool inDeps = false;
    bool foundLink = false;

    while (std::getline(in, line)) {
        std::string t = trimString(line);
        if (t == "[dependencies]") {
            inDeps = true;
            buffer << line << "\n";
            continue;
        }

        if (inDeps && t.rfind("link", 0) == 0 && t.find('=') != std::string::npos) {
            foundLink = true;
            size_t closeBracket = line.rfind(']');
            if (closeBracket != std::string::npos) {
                std::string before = line.substr(0, closeBracket);
                std::string after = line.substr(closeBracket);
                std::string trimmedBefore = trimString(before);

                if (trimmedBefore.find("\"" + libName + "\"") == std::string::npos &&
                    trimmedBefore.find("'" + libName + "'") == std::string::npos) {
                    if (trimmedBefore.back() == '[') {
                        line = before + "\"" + libName + "\"" + after;
                    } else {
                        line = before + ", \"" + libName + "\"" + after;
                    }
                }
            }
        }
        buffer << line << "\n";
    }
    in.close();

    if (!foundLink && inDeps) {
        std::string content = buffer.str();
        size_t depPos = content.find("[dependencies]");
        if (depPos != std::string::npos) {
            size_t insertPos = content.find('\n', depPos);
            if (insertPos != std::string::npos) {
                content.insert(insertPos + 1, "link = [\"" + libName + "\"]\n");
                std::ofstream out(tomlPath);
                out << content;
                out.close();
                return;
            }
        }
    }

    std::ofstream out(tomlPath);
    out << buffer.str();
    out.close();
}

bool handleAddDependency(const std::string& depName) {
    std::string name = depName;
    if (name.length() >= 4 && name.substr(name.length() - 4) == ".hpp") {
        name = name.substr(0, name.length() - 4);
    } else if (name.length() >= 2 && name.substr(name.length() - 2) == ".h") {
        name = name.substr(0, name.length() - 2);
    }

    fs::path depDir = "wrapper";
    if (!fs::exists(depDir)) {
        try {
            fs::create_directories(depDir);
        } catch (const std::exception& e) {
            std::cerr << "\033[1;31m[Error]\033[0m Failed to create wrapper directory: " << e.what() << std::endl;
            return false;
        }
    }

    if (name == "crow") {
        fs::path crowHpp = depDir / "crow.hpp";
        std::ofstream out(crowHpp);
        if (!out.is_open()) {
            std::cerr << "\033[1;31m[Error]\033[0m Failed to create " << crowHpp.string() << std::endl;
            return false;
        }

        out << ZyncDependencies::getCrowTemplate();
        out.close();

        appendLibToToml("pthread");

        std::cout << "\033[1;32m[Zync Wrapper Added]\033[0m Crow HTTP wrapper generated: '\033[1m" << crowHpp.string() << "\033[0m'" << std::endl;
        std::cout << "\033[1;36m[Zync Config]\033[0m Added 'pthread' link dependency to zync.toml" << std::endl;
        std::cout << "\nImport in your Zync code with:\n";
        std::cout << "  import @CPPheader \"wrapper/crow\"\n";
        std::cout << "  var app = ZyncCrow::App()\n" << std::endl;
        return true;
    }

    std::cerr << "\033[1;31m[Error]\033[0m Unknown predefined wrapper: " << depName << std::endl;
    return false;
}