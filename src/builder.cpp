#include "../include/builder.hpp"
#include "../include/lexer.hpp"
#include "../include/parser.hpp"
#include "../include/codegen.hpp"
#include "../include/config.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <thread>
#include <future>
#include <mutex>
#include <algorithm>
#include <cctype>
#include <regex>
#include <array>
#include <cstdio>
#include <memory>

static uint64_t computeHash(const std::string& str) {
    uint64_t hash = 14695981039346656037ULL;
    for (char c : str) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

static uint64_t computeFileHash(const fs::path& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return 0;
    std::stringstream buffer;
    buffer << file.rdbuf();
    return computeHash(buffer.str());
}

static bool isCommandAvailable(const std::string& cmd) {
#if defined(_WIN32)
    std::string checkCmd = "where " + cmd + " >nul 2>&1";
#else
    std::string checkCmd = "which " + cmd + " > /dev/null 2>&1";
#endif
    return (std::system(checkCmd.c_str()) == 0);
}

static std::unordered_map<std::string, std::string> loadCacheManifest(const fs::path& manifestPath) {
    std::unordered_map<std::string, std::string> cache;
    if (!fs::exists(manifestPath)) return cache;

    std::ifstream in(manifestPath);
    if (!in.is_open()) return cache;

    std::string line;
    while (std::getline(in, line)) {
        size_t eqPos = line.find('=');
        if (eqPos != std::string::npos) {
            std::string key = line.substr(0, eqPos);
            std::string val = line.substr(eqPos + 1);
            cache[key] = val;
        }
    }
    return cache;
}

static void saveCacheManifest(const fs::path& manifestPath, const std::unordered_map<std::string, std::string>& cache) {
    std::ofstream out(manifestPath);
    if (!out.is_open()) return;

    for (const auto& kv : cache) {
        out << kv.first << "=" << kv.second << "\n";
    }
}

static std::string execCommandCapture(const std::string& cmd, int& exitCode) {
    std::string result = "";
    std::string redirectCmd = cmd + " 2>&1";
    std::array<char, 256> buffer;
    
    FILE* pipe = popen(redirectCmd.c_str(), "r");
    if (!pipe) {
        exitCode = -1;
        return "Failed to run command pipe.";
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    exitCode = pclose(pipe);
    return result;
}

static std::string getSourceLine(const std::string& filePath, int lineNum) {
    std::ifstream file(filePath);
    if (!file.is_open()) return "";
    std::string line;
    int current = 1;
    while (std::getline(file, line)) {
        if (current == lineNum) return line;
        current++;
    }
    return "";
}

static void printZyncDiagnostic(const std::string& rawCppOutput, const std::string& zyFile, const std::string& mainCppFile) {
    (void)mainCppFile;
    std::istringstream stream(rawCppOutput);
    std::string line;
    std::regex errorRegex(R"((?:.*[\/\\])?([^\/\\:]+\.(?:cpp|zy)):(\d+):(\d+):\s*error:\s*(.*))");
    std::smatch match;

    bool foundAny = false;

    while (std::getline(stream, line)) {
        if (std::regex_search(line, match, errorRegex)) {
            foundAny = true;
            std::string matchedFile = match[1].str();
            int errorLine = std::stoi(match[2].str());
            int col = std::stoi(match[3].str());
            std::string errMsg = match[4].str();

            int zyLine = errorLine;
            std::string zySourceSnippet = "";

            if (matchedFile.find(".cpp") != std::string::npos) {
                std::string targetIdentifier = "";
                std::regex identRegex(R"('(.*?)')");
                std::smatch identMatch;
                if (std::regex_search(errMsg, identMatch, identRegex)) {
                    targetIdentifier = identMatch[1].str();
                    if (targetIdentifier.rfind("_", 0) == 0) {
                        targetIdentifier = targetIdentifier.substr(1);
                    }
                }

                std::ifstream zyStream(zyFile);
                if (zyStream.is_open()) {
                    std::string zLine;
                    int zCurrent = 1;
                    int bestLine = 0;
                    std::string bestSnippet = "";

                    while (std::getline(zyStream, zLine)) {
                        if (!targetIdentifier.empty() && zLine.find(targetIdentifier) != std::string::npos) {
                            bestLine = zCurrent;
                            bestSnippet = zLine;
                            break;
                        }
                        zCurrent++;
                    }

                    if (bestLine > 0) {
                        zyLine = bestLine;
                        zySourceSnippet = bestSnippet;
                    }
                }
            }

            if (zySourceSnippet.empty()) {
                zySourceSnippet = getSourceLine(zyFile, zyLine);
            }

            if (!zySourceSnippet.empty()) {
                std::string targetIdentifier = "";
                std::regex identRegex(R"('(.*?)')");
                std::smatch identMatch;
                if (std::regex_search(errMsg, identMatch, identRegex)) {
                    targetIdentifier = identMatch[1].str();
                }
                if (!targetIdentifier.empty()) {
                    size_t pos = zySourceSnippet.find(targetIdentifier);
                    if (pos != std::string::npos) {
                        col = static_cast<int>(pos + 1);
                    }
                }
            }

            errMsg = std::regex_replace(errMsg, std::regex("std::basic_string<char>|std::string"), "string");
            errMsg = std::regex_replace(errMsg, std::regex("const char\\[\\d+\\]"), "string");
            errMsg = std::regex_replace(errMsg, std::regex("std::vector"), "vector");
            errMsg = std::regex_replace(errMsg, std::regex("std::map"), "map");
            errMsg = std::regex_replace(errMsg, std::regex("::_new"), "::new");
            errMsg = std::regex_replace(errMsg, std::regex("_new"), "new");

            std::cerr << "\n\033[1;31m[Zync Error]\033[0m \033[1m" << zyFile << ":" << zyLine << ":" << col << "\033[0m: " << errMsg << "\n";
            if (!zySourceSnippet.empty()) {
                std::cerr << " \033[1;34m" << std::setw(5) << zyLine << " |\033[0m " << zySourceSnippet << "\n";
                std::cerr << " \033[1;34m      |\033[0m \033[1;31m" << std::string(col > 0 ? col - 1 : 0, ' ') << "^~~~~~\033[0m\n";
            }
        }
    }

    if (!foundAny) {
        std::cerr << rawCppOutput << std::endl;
    }
}
bool parseFileRecursive(const fs::path& filePath, ProgramNode* mergedProgram, std::unordered_set<std::string>& visitedFiles, std::vector<fs::path>& allDepFiles) {
    std::string canonicalPath;
    try {
        canonicalPath = fs::canonical(filePath).string();
    } catch (...) {
        canonicalPath = filePath.string();
    }

    if (visitedFiles.find(canonicalPath) != visitedFiles.end()) {
        return true;
    }
    visitedFiles.insert(canonicalPath);
    allDepFiles.push_back(filePath);

    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "\033[1;31m[Error]\033[0m Could not open file " << filePath.string() << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    file.close();

    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenize();

    Parser parser(tokens);
    std::string expectedPkg = filePath.stem().string();
    auto programAST = parser.parseProgram(expectedPkg);

    if (!programAST) {
        return false;
    }

    fs::path currentDir = filePath.parent_path();

    for (auto& imp : programAST->imports) {
        if (imp->kind == ImportKind::ZYNC_FILE) {
            fs::path importedPath = currentDir / imp->target;
            if (!fs::exists(importedPath) && fs::exists(fs::path(imp->target))) {
                importedPath = fs::path(imp->target);
            }
            if (!fs::exists(importedPath) && !importedPath.has_extension()) {
                importedPath += ".zy";
            }
            if (!fs::exists(importedPath)) {
                std::cerr << "\033[1;31m[Error]\033[0m Imported file not found: " << imp->target << " (from " << filePath.string() << ")" << std::endl;
                return false;
            }
            if (!parseFileRecursive(importedPath, mergedProgram, visitedFiles, allDepFiles)) {
                return false;
            }
        } else if (imp->kind == ImportKind::PACKAGE) {
            fs::path autoZyFile = currentDir / (imp->target + ".zy");
            if (!fs::exists(autoZyFile) && fs::exists(fs::path(imp->target + ".zy"))) {
                autoZyFile = fs::path(imp->target + ".zy");
            }
            if (fs::exists(autoZyFile)) {
                if (!parseFileRecursive(autoZyFile, mergedProgram, visitedFiles, allDepFiles)) {
                    return false;
                }
            }
            mergedProgram->imports.push_back(std::move(imp));
        } else {
            mergedProgram->imports.push_back(std::move(imp));
        }
    }

    for (auto& tr : programAST->traits) mergedProgram->traits.push_back(std::move(tr));
    for (auto& rec : programAST->records) mergedProgram->records.push_back(std::move(rec));
    for (auto& im : programAST->impls) mergedProgram->impls.push_back(std::move(im));
    for (auto& pkg : programAST->packages) mergedProgram->packages.push_back(std::move(pkg));
    for (auto& fn : programAST->functions) mergedProgram->functions.push_back(std::move(fn));
    for (auto& t : programAST->tests) mergedProgram->tests.push_back(std::move(t));

    return true;
}

bool handleBuild(const std::string& inputPath, const std::string& customOutputName, OptLevel opt, const std::string& customFlags, const GranularOptFlags& granular, SizeProfile sizeProf, bool dumpGimple, BuildTarget target, bool forceRebuild, bool isQuietMode, unsigned int jobs, const std::string& userLinkFlags, const std::string& userIncludeFlags, bool isTestBuild) {
    auto buildStartTime = std::chrono::high_resolution_clock::now();

    fs::path zyPath(inputPath);
    if (!fs::exists(zyPath)) {
        if (!zyPath.has_extension()) {
            zyPath += ".zy";
        }
        if (!fs::exists(zyPath)) {
            std::cerr << "\033[1;31m[Error]\033[0m File " << inputPath << " not found." << std::endl;
            return false;
        }
    }

    std::string outputName = customOutputName.empty() ? zyPath.stem().string() : customOutputName;

    fs::path buildDir = isTestBuild ? (fs::path("build") / "test") : fs::path("build");
    fs::path objDir = isTestBuild ? (buildDir / "obj") : (fs::path("build") / "obj");
    fs::path gimpleDir = buildDir / "gimple";
    fs::path wasmDir = buildDir / "wasm";
    fs::path cacheManifestPath = objDir / (outputName + ".cache");

    if (!fs::exists(buildDir)) fs::create_directories(buildDir);
    if (!fs::exists(objDir)) fs::create_directories(objDir);
    if (dumpGimple && !fs::exists(gimpleDir)) fs::create_directories(gimpleDir);
    if (target == BuildTarget::WASM && !fs::exists(wasmDir)) fs::create_directories(wasmDir);

    fs::path exePath = (target == BuildTarget::WASM) ? (wasmDir / (outputName + ".wasm")) : (buildDir / outputName);

    std::string optFlags;
    std::string linkFlags = userLinkFlags;
    std::string includeFlags = userIncludeFlags;
    std::string optDescription;

#if defined(__APPLE__)
    if (fs::exists("/opt/homebrew/include")) includeFlags += " -I/opt/homebrew/include";
    if (fs::exists("/opt/homebrew/lib")) linkFlags += " -L/opt/homebrew/lib";
    if (fs::exists("/usr/local/include")) includeFlags += " -I/usr/local/include";
    if (fs::exists("/usr/local/lib")) linkFlags += " -L/usr/local/lib";
#endif

    if (isQuietMode) {
        optFlags = "-O0";
        optDescription = "JIT / Fast Eval";
    } else if (opt == OptLevel::CUSTOM && !customFlags.empty()) {
        optFlags = customFlags;
        optDescription = "Custom (" + customFlags + ")";
    } else if (sizeProf == SizeProfile::SMALL) {
        optFlags = "-Os -ffunction-sections -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables";
        if (target == BuildTarget::NATIVE) {
#if defined(__APPLE__)
            linkFlags += " -Wl,-dead_strip -Wl,-x";
#else
            linkFlags += " -s -Wl,--gc-sections";
#endif
        }
        optDescription = "Size: Small (-Os, Dead Strip, Section GC)";
    } else if (sizeProf == SizeProfile::MEDIUM) {
        optFlags = "-O2 -ffunction-sections -fdata-sections";
        if (target == BuildTarget::NATIVE) {
#if defined(__APPLE__)
            linkFlags += " -Wl,-dead_strip";
#else
            linkFlags += " -s -Wl,--gc-sections";
#endif
        }
        optDescription = "Size: Medium (-O2 Balanced Size & Speed)";
    } else if (sizeProf == SizeProfile::LARGE) {
        optFlags = (target == BuildTarget::WASM)
            ? "-O3 -funroll-loops -DNDEBUG"
            : "-O3 -march=native -funroll-loops -DNDEBUG";
        optDescription = "Size: Large (-O3 Max Performance & Full Inlining)";
    } else {
        switch (opt) {
            case OptLevel::O1:
                optFlags = "-O1";
                optDescription = "Level 1 (Basic Optimization)";
                break;
            case OptLevel::O2:
                optFlags = "-O2";
                optDescription = "Level 2 (Standard Release)";
                break;
            case OptLevel::O3:
                optFlags = (target == BuildTarget::WASM)
                    ? "-O3 -DNDEBUG"
                    : "-O3 -march=native -DNDEBUG";
                optDescription = "Level 3 (High Performance & Vectorization)";
                break;
            case OptLevel::O4:
                optFlags = (target == BuildTarget::WASM)
                    ? "-O3 -flto -funroll-loops -ffast-math -DNDEBUG"
                    : "-O3 -march=native -flto -funroll-loops -ffast-math -DNDEBUG";
                linkFlags += " -flto";
                optDescription = "Level 4 (Ultra Fast: LTO, Loop Unrolling, Fast Math)";
                break;
            case OptLevel::O5:
                optFlags = (target == BuildTarget::WASM)
                    ? "-O3 -flto -funroll-loops -ffast-math -DNDEBUG"
                    : "-Ofast -march=native -mtune=native -flto -funroll-loops -fomit-frame-pointer -ffast-math -ftree-vectorize -DNDEBUG";
                linkFlags += " -flto";
                optDescription = "Level 5 (Maximum Performance: -Ofast, LTO, Loop Unrolling, Vectorization)";
                break;
            case OptLevel::O0:
            default:
                optFlags = "-O0";
                optDescription = "Debug (Fast Build / Unoptimized)";
                break;
        }
    }

    if (granular.dce) {
        optFlags += " -ffunction-sections -fdata-sections";
        if (target == BuildTarget::NATIVE) {
#if defined(__APPLE__)
            linkFlags += " -Wl,-dead_strip";
#else
            linkFlags += " -s -Wl,--gc-sections";
#endif
        }
        optDescription += " + DCE";
    }
    if (granular.lto) {
        optFlags += " -flto";
        linkFlags += " -flto";
        optDescription += " + LTO";
    }
    if (granular.unroll) {
        optFlags += " -funroll-loops";
        optDescription += " + Unroll";
    }
    if (granular.fastMath) {
        optFlags += " -ffast-math";
        optDescription += " + FastMath";
    }
    if (granular.nativeArch && target != BuildTarget::WASM) {
        optFlags += " -march=native -mtune=native";
        optDescription += " + NativeArch";
    }
    if (granular.stripSymbols) {
        if (target == BuildTarget::NATIVE) {
#if defined(__APPLE__)
            linkFlags += " -Wl,-x";
#else
            linkFlags += " -s";
#endif
        }
        optDescription += " + Strip";
    }
    if (!customFlags.empty() && opt != OptLevel::CUSTOM) {
        optFlags += " " + customFlags;
        optDescription += " + Custom(" + customFlags + ")";
    }

    std::vector<fs::path> allDepFiles;
    std::unordered_set<std::string> visitedGlobal;
    auto globalMerged = std::make_unique<ProgramNode>();

    if (!parseFileRecursive(zyPath, globalMerged.get(), visitedGlobal, allDepFiles)) {
        return false;
    }

    uint64_t combinedSourceHash = 0;
    for (const auto& dep : allDepFiles) {
        combinedSourceHash ^= computeFileHash(dep);
    }
    std::string configSignature = optFlags + "|" + linkFlags + "|" + includeFlags + "|" + (target == BuildTarget::WASM ? "wasm" : "native");
    uint64_t totalConfigHash = combinedSourceHash ^ computeHash(configSignature);

    auto oldCache = loadCacheManifest(cacheManifestPath);
    bool isCached = false;

    if (!forceRebuild && fs::exists(exePath) && oldCache.find("config_hash") != oldCache.end()) {
        if (oldCache["config_hash"] == std::to_string(totalConfigHash)) {
            isCached = true;
        }
    }

    if (isCached && !dumpGimple) {
        auto buildEndTime = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(buildEndTime - buildStartTime).count();

        if (!isQuietMode) {
            std::cout << "\033[1;32m[Zync Cache]\033[0m \033[1m" << zyPath.filename().string() 
                      << "\033[0m is up to date (\033[1;33m" << std::fixed << std::setprecision(2) << elapsedMs << " ms\033[0m - unchanged artifact)." << std::endl;
        }
        return true;
    }

    unsigned int detectedCores = (jobs > 0) ? jobs : std::thread::hardware_concurrency();
    if (detectedCores == 0) detectedCores = 2;

    if (!isQuietMode) {
        std::cout << "\033[1;36m[Zync]\033[0m Target: " << zyPath.filename().string() << " -> " << buildDir.string() << "/" << outputName 
                  << " (" << optDescription << ") [Parallel Jobs: " << detectedCores << "]" << std::endl;
    }

    fs::path mainCpp = objDir / (outputName + ".cpp");
    fs::path mainObj = objDir / (outputName + ".o");

    CodeGen codegen(globalMerged.get());
    std::string cppCode = codegen.generate();

    std::ofstream out(mainCpp);
    out << cppCode;
    out.close();

    std::string compileCmd;
    if (target == BuildTarget::WASM) {
        compileCmd = "em++ -std=c++17 -I. -Iinclude -Inative -Idependencies -Idependencies/wrapper " + includeFlags + " " + optFlags + " -c " + mainCpp.string() + " -o " + mainObj.string();
    } else {
        compileCmd = "g++ -std=c++17 -I. -Iinclude -Inative -Idependencies -Idependencies/wrapper " + includeFlags + " " + optFlags + " -c " + mainCpp.string() + " -o " + mainObj.string();
    }

    std::cout << "\033[1;36m[Zync Parallel]\033[0m (1/1) Compiling " << zyPath.filename().string() << "..." << std::endl;
    int exitCode = 0;
    std::string compileOutput = execCommandCapture(compileCmd, exitCode);
    if (exitCode != 0) {
        printZyncDiagnostic(compileOutput, zyPath.string(), mainCpp.string());
        std::cerr << "\n\033[1;31m[Compile Error]\033[0m Failed compiling unit: " << zyPath.filename().string() << std::endl;
        return false;
    }

    if (target == BuildTarget::WASM) {
        fs::path wasmHtmlOut = wasmDir / (outputName + ".html");
        std::string wasmOpt = optFlags.empty() ? "-O3" : optFlags;
        std::string emccCmd = "em++ -std=c++17 -I. -Iinclude -Inative -Idependencies -Idependencies/wrapper " + includeFlags + " " + optFlags + " -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 " + mainObj.string() + " " + linkFlags + " -o " + wasmHtmlOut.string();
        int wasmExit = 0;
        std::string wasmOut = execCommandCapture(emccCmd, wasmExit);
        if (wasmExit != 0) {
            printZyncDiagnostic(wasmOut, zyPath.string(), mainCpp.string());
            std::cerr << "\033[1;31m[Zync Error]\033[0m Linker step for WASM failed." << std::endl;
            return false;
        }
    } else {
        std::string linkCmd = "g++ -std=c++17 -I. -Iinclude -Inative -Idependencies -Idependencies/wrapper " + includeFlags + " " + optFlags + " " + mainObj.string() + " " + linkFlags + " -o " + (buildDir / outputName).string();
        int linkExit = 0;
        std::string linkOut = execCommandCapture(linkCmd, linkExit);
        if (linkExit != 0) {
            printZyncDiagnostic(linkOut, zyPath.string(), mainCpp.string());
            std::cerr << "\033[1;31m[Zync Error]\033[0m Linker step failed." << std::endl;
            return false;
        }
    }

    if (dumpGimple) {
        std::string gccGimpleCmd = "g++ -std=c++17 -I. -Iinclude -Inative -Idependencies -Idependencies/wrapper " + includeFlags + " " + optFlags + " -fdump-tree-gimple -c " + mainCpp.string() + " -o " + mainObj.string() + " 2>/dev/null";
        std::system(gccGimpleCmd.c_str());
        for (const auto& entry : fs::directory_iterator(fs::current_path())) {
            if (entry.is_regular_file() && entry.path().filename().string().find(".gimple") != std::string::npos) {
                fs::path targetPath = gimpleDir / entry.path().filename();
                fs::rename(entry.path(), targetPath);
                std::cout << "\033[1;32m[Zync IR]\033[0m GIMPLE IR generated: " << targetPath.string() << std::endl;
            }
        }
    }

    std::unordered_map<std::string, std::string> newCache;
    newCache["config_hash"] = std::to_string(totalConfigHash);
    saveCacheManifest(cacheManifestPath, newCache);

    auto buildEndTime = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(buildEndTime - buildStartTime).count();

    if (!isQuietMode) {
        std::cout << "\033[1;32m[Zync Finished]\033[0m Binary artifact: " << exePath.string() 
                  << " (took \033[1;33m" << std::fixed << std::setprecision(2) << elapsedMs << " ms\033[0m)" << std::endl;
    }
    return true;
}

bool handleRun(const std::string& targetName, bool isWasm, bool isTestRun) {
    fs::path targetPath(targetName);
    std::string stemName = targetPath.stem().string();

    if (isWasm) {
        if (!isCommandAvailable("node")) {
            std::cerr << "\033[1;31m[Zync Error]\033[0m Node.js runtime not found." << std::endl;
            std::cerr << "\033[1;33m[Requirement]\033[0m To execute WebAssembly files in terminal, Node.js must be installed." << std::endl;
            return false;
        }

        fs::path wasmJsPath = fs::path("build") / "wasm" / (stemName + ".js");
        if (!fs::exists(wasmJsPath)) {
            std::cerr << "\033[1;31m[Error]\033[0m WASM module not found at " << wasmJsPath.string() << "." << std::endl;
            std::cerr << "Run 'zync build " << stemName << ".zy -wasm' first." << std::endl;
            return false;
        }

        std::cout << "\033[1;36m[Zync WASM Runner]\033[0m Executing " << wasmJsPath.string() << " via Node.js...\n" << std::endl;
        std::string runCmd = "node " + wasmJsPath.string();
        int res = std::system(runCmd.c_str());
        return (res == 0);
    }

    fs::path exePath = isTestRun ? (fs::path("build") / "test" / stemName) : (fs::path("build") / stemName);
    if (!fs::exists(exePath)) {
        std::cerr << "\033[1;31m[Error]\033[0m Executable not found at " << exePath.string() << ". Run 'zync build " << targetName << "' first." << std::endl;
        return false;
    }

    std::string runCommand = "./" + exePath.string();
    int runResult = std::system(runCommand.c_str());
    return (runResult == 0);
}

bool handleEval(const std::string& codeSnippet) {
    fs::path buildDir = "build";
    if (!fs::exists(buildDir)) fs::create_directories(buildDir);

    std::string trimmedSnippet = codeSnippet;
    while (!trimmedSnippet.empty() && std::isspace(static_cast<unsigned char>(trimmedSnippet.back()))) {
        trimmedSnippet.pop_back();
    }
    if (!trimmedSnippet.empty() && trimmedSnippet.back() != ';' && trimmedSnippet.back() != '}') {
        trimmedSnippet += ";";
    }

    std::string wrappedCode = 
        "pkg main\n\n"
        "import std\n\n"
        "fn main() -> void {\n"
        "    " + trimmedSnippet + "\n"
        "}\n";

    fs::path tempZy = buildDir / "__zync_eval_temp.zy";
    std::ofstream out(tempZy);
    out << wrappedCode;
    out.close();

    GranularOptFlags granular;
    bool ok = handleBuild(tempZy.string(), "__zync_eval_bin", OptLevel::O0, "", granular, SizeProfile::NONE, false, BuildTarget::NATIVE, true, true);
    
    if (ok) {
        fs::path evalBin = buildDir / "__zync_eval_bin";
        std::string runCmd = "./" + evalBin.string();
        std::system(runCmd.c_str());

        fs::remove(tempZy);
        fs::remove(evalBin);
        fs::path evalCpp = buildDir / "obj" / "__zync_eval_temp.cpp";
        if (fs::exists(evalCpp)) fs::remove(evalCpp);
        fs::path evalObj = buildDir / "obj" / "__zync_eval_temp.o";
        if (fs::exists(evalObj)) fs::remove(evalObj);
    }
    return ok;
}

void handleRepl() {
    std::cout << "\033[1;36mZync Interactive REPL (v1.0)\033[0m" << std::endl;
    std::cout << "Type expressions or statements. Commands: \033[1;33mexit\033[0m, \033[1;33mquit\033[0m, \033[1;33mclear\033[0m.\n" << std::endl;

    std::vector<std::string> sessionStatements;

    while (true) {
        std::cout << "\033[1;32mzy>\033[0m " << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) {
            std::cout << std::endl;
            break;
        }

        std::string trimmed = line;
        while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) trimmed.erase(0, 1);
        while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) trimmed.pop_back();

        if (trimmed.empty()) continue;
        if (trimmed == "exit" || trimmed == "quit") break;
        if (trimmed == "clear" || trimmed == "cls") {
            sessionStatements.clear();
            std::cout << "\033[1;33mSession reset.\033[0m" << std::endl;
            continue;
        }

        std::string activeStatement = trimmed;
        if (activeStatement.rfind("var ", 0) == 0 || activeStatement.rfind("print", 0) == 0 || activeStatement.find('=') != std::string::npos) {
            if (activeStatement.back() != ';' && activeStatement.back() != '}') {
                activeStatement += ";";
            }
        } else {
            activeStatement = "println(" + activeStatement + ");";
        }

        fs::path buildDir = "build";
        if (!fs::exists(buildDir)) fs::create_directories(buildDir);

        std::ostringstream fullSession;
        fullSession << "pkg main\n\nimport std\n\nfn main() -> void {\n";
        for (const auto& s : sessionStatements) {
            fullSession << "    " << s << "\n";
        }
        fullSession << "    " << activeStatement << "\n";
        fullSession << "}\n";

        fs::path tempZy = buildDir / "__zync_repl_session.zy";
        std::ofstream out(tempZy);
        out << fullSession.str();
        out.close();

        GranularOptFlags granular;
        bool ok = handleBuild(tempZy.string(), "__zync_repl_bin", OptLevel::O0, "", granular, SizeProfile::NONE, false, BuildTarget::NATIVE, true, true);

        if (ok) {
            fs::path replBin = buildDir / "__zync_repl_bin";
            std::string runCmd = "./" + replBin.string();
            std::system(runCmd.c_str());

            if (trimmed.rfind("var ", 0) == 0 || (trimmed.find('=') != std::string::npos && trimmed.rfind("print", 0) != 0)) {
                sessionStatements.push_back(activeStatement);
            }

            fs::remove(tempZy);
            fs::remove(replBin);
        }
    }
}

bool handleServe(const std::string& targetName) {
    if (!isCommandAvailable("node")) {
        std::cerr << "\033[1;31m[Zync Error]\033[0m Node.js runtime not found." << std::endl;
        std::cerr << "\033[1;33m[Requirement]\033[0m To serve WebAssembly in local server, Node.js must be installed." << std::endl;
        return false;
    }

    std::string stemName = targetName.empty() ? "main" : fs::path(targetName).stem().string();
    fs::path wasmHtml = fs::path("build") / "wasm" / (stemName + ".html");

    if (!fs::exists(wasmHtml)) {
        std::cerr << "\033[1;33m[Warning]\033[0m HTML artifact " << wasmHtml.string() << " not found." << std::endl;
        std::cerr << "Make sure you have compiled with 'zync build " << stemName << ".zy -wasm'" << std::endl;
    }

    std::cout << "\033[1;32m[Zync Server]\033[0m Starting local WebAssembly server..." << std::endl;
    std::cout << "\033[1;36m[Browser URL]\033[0m http://localhost:8080/build/wasm/" << stemName << ".html" << std::endl;
    std::cout << "\033[1;33m(Press Ctrl+C to stop server)\033[0m\n" << std::endl;

    std::string serverCmd;
    if (isCommandAvailable("npx")) {
        serverCmd = "npx --yes serve -p 8080 .";
    } else if (isCommandAvailable("python3")) {
        serverCmd = "python3 -m http.server 8080";
    } else {
        std::cerr << "\033[1;31m[Error]\033[0m Neither npx nor python3 available to host local server." << std::endl;
        return false;
    }

    int res = std::system(serverCmd.c_str());
    return (res == 0);
}

bool handleTest(const std::string& targetFile) {
    fs::path zyPath(targetFile);
    if (!zyPath.has_extension()) {
        zyPath += ".zy";
    }

    std::cout << "\033[1;36m[Zync Test]\033[0m Compiling test suite: \033[1m" << zyPath.string() << "\033[0m" << std::endl;
    
    GranularOptFlags granular;
    if (!handleBuild(zyPath.string(), "", OptLevel::O0, "", granular, SizeProfile::NONE, false, BuildTarget::NATIVE, true, true, 0, "", "", true)) {
        std::cerr << "\033[1;31m[Zync Test Error]\033[0m Test build pipeline aborted." << std::endl;
        return false;
    }

    bool success = handleRun(zyPath.string(), false, true);
    return success;
}

bool handleCreate(const std::string& projectName) {
    fs::path projectDir = projectName;
    if (fs::exists(projectDir)) {
        std::cerr << "\033[1;31m[Error]\033[0m Directory '" << projectName << "' already exists." << std::endl;
        return false;
    }

    try {
        fs::create_directories(projectDir);

        fs::path mainFile = projectDir / "main.zy";
        std::ofstream mainOut(mainFile);
        if (!mainOut.is_open()) {
            std::cerr << "\033[1;31m[Error]\033[0m Failed to create main.zy in " << projectName << std::endl;
            return false;
        }
        mainOut << "pkg main\n\nimport std\n\n";
        mainOut << "fn main() -> void {\n";
        mainOut << "    println(\"Hello from Zync project: " << projectName << "!\")\n";
        mainOut << "}\n";
        mainOut.close();

        fs::path tomlFile = projectDir / "zync.toml";
        std::ofstream tomlOut(tomlFile);
        if (tomlOut.is_open()) {
            tomlOut << "[package]\n"
                    << "name = \"\"\n"
                    << "entry = \"main.zy\"\n\n"
                    << "[dependencies]\n"
                    << "link = []\n";
            tomlOut.close();
        }

        fs::path gitignoreFile = projectDir / ".gitignore";
        std::ofstream gitOut(gitignoreFile);
        if (gitOut.is_open()) {
            gitOut << "build/\n";
            gitOut.close();
        }

        std::cout << "\033[1;32m[Zync Created]\033[0m Project '\033[1m" << projectName << "\033[0m' initialized successfully!" << std::endl;
        std::cout << "\nGet started with:\n";
        std::cout << "  cd " << projectName << "\n";
        std::cout << "  zync build main.zy\n";
        std::cout << "  zync run main\n" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "\033[1;31m[Error]\033[0m " << e.what() << std::endl;
        return false;
    }
}

bool handleAddPackage(const std::string& pkgName) {
    std::string fileName = pkgName;
    if (fileName.length() >= 3 && fileName.substr(fileName.length() - 3) == ".zy") {
        fileName = fileName.substr(0, fileName.length() - 3);
    }

    fs::path targetFile = fileName + ".zy";
    if (fs::exists(targetFile)) {
        std::cerr << "\033[1;31m[Error]\033[0m Package file '" << targetFile.string() << "' already exists." << std::endl;
        return false;
    }

    std::ofstream out(targetFile);
    if (!out.is_open()) {
        std::cerr << "\033[1;31m[Error]\033[0m Failed to create file " << targetFile.string() << std::endl;
        return false;
    }

    out << "pkg " << fs::path(fileName).filename().string() << "\n\n";
    out.close();

    std::cout << "\033[1;32m[Zync Package Created]\033[0m Created package template '\033[1m" << targetFile.string() << "\033[0m'" << std::endl;
    return true;
}

bool handleCreateTest(const std::string& rawTestName) {
    std::string testName = rawTestName;
    if (testName.length() >= 3 && testName.substr(testName.length() - 3) == ".zy") {
        testName = testName.substr(0, testName.length() - 3);
    }
    if (testName.length() >= 5 && testName.substr(testName.length() - 5) == "_test") {
        testName = testName.substr(0, testName.length() - 5);
    }

    fs::path testsDir = "tests";
    if (!fs::exists(testsDir)) {
        try {
            fs::create_directories(testsDir);
        } catch (const std::exception& e) {
            std::cerr << "\033[1;31m[Error]\033[0m Failed to create tests directory: " << e.what() << std::endl;
            return false;
        }
    }

    std::string fileName = testName + "_test";
    fs::path targetFile = testsDir / (fileName + ".zy");

    if (fs::exists(targetFile)) {
        std::cerr << "\033[1;31m[Error]\033[0m Test file '" << targetFile.string() << "' already exists." << std::endl;
        return false;
    }

    std::ofstream out(targetFile);
    if (!out.is_open()) {
        std::cerr << "\033[1;31m[Error]\033[0m Failed to create test file " << targetFile.string() << std::endl;
        return false;
    }

    out << "pkg " << fileName << "\n\n"
        << "import std\n\n"
        << "test(\"basic_" << testName << "_assertion\") {\n"
        << "    var a = 10\n"
        << "    var b = 20\n"
        << "    assert_eq(a + b, 30)\n"
        << "    assert_ne(a, b)\n"
        << "}\n\n"
        << "test(\"boolean_check\") {\n"
        << "    assert(true)\n"
        << "}\n";
    out.close();

    std::cout << "\033[1;32m[Zync Test Created]\033[0m Test suite created: '\033[1m" << targetFile.string() << "\033[0m'" << std::endl;
    std::cout << "\nRun the test with:\n";
    std::cout << "  zync test " << targetFile.string() << "\n" << std::endl;
    return true;
}

bool handleCreateNative(const std::string& rawName) {
    std::string name = rawName;
    if (name.length() >= 4 && name.substr(name.length() - 4) == ".hpp") {
        name = name.substr(0, name.length() - 4);
    } else if (name.length() >= 4 && name.substr(name.length() - 4) == ".cpp") {
        name = name.substr(0, name.length() - 4);
    }

    fs::path nativeDir = "native";
    fs::path pkgDir = nativeDir / name;
    fs::path hppFile = pkgDir / (name + ".hpp");
    fs::path nativeZyncHpp = nativeDir / "zync.hpp";

    if (fs::exists(hppFile)) {
        std::cerr << "\033[1;31m[Error]\033[0m Native header '" << hppFile.string() << "' already exists." << std::endl;
        return false;
    }

    try {
        fs::create_directories(pkgDir);
    } catch (const std::exception& e) {
        std::cerr << "\033[1;31m[Error]\033[0m Failed to create directory " << pkgDir.string() << ": " << e.what() << std::endl;
        return false;
    }

    if (!fs::exists(nativeZyncHpp)) {
        fs::path srcZyncHpp = "include/zync.hpp";
        if (fs::exists(srcZyncHpp)) {
            try {
                fs::copy_file(srcZyncHpp, nativeZyncHpp, fs::copy_options::overwrite_existing);
            } catch (...) {
                std::ofstream zyncOut(nativeZyncHpp);
                if (zyncOut.is_open()) {
                    zyncOut << "#pragma once\n\n"
                            << "#include <iostream>\n"
                            << "#include <string>\n"
                            << "#include <vector>\n"
                            << "#include <tuple>\n"
                            << "#include <utility>\n\n"
                            << "#define Package(name) namespace name\n"
                            << "#define fn inline auto\n\n"
                            << "template <typename... Args>\n"
                            << "inline void print(Args&&... args) {\n"
                            << "    (std::cout << ... << std::forward<Args>(args));\n"
                            << "}\n\n"
                            << "template <typename... Args>\n"
                            << "inline void println(Args&&... args) {\n"
                            << "    (std::cout << ... << std::forward<Args>(args)) << std::endl;\n"
                            << "}\n";
                    zyncOut.close();
                }
            }
        } else {
            std::ofstream zyncOut(nativeZyncHpp);
            if (zyncOut.is_open()) {
                zyncOut << "#pragma once\n\n"
                        << "#include <iostream>\n"
                        << "#include <string>\n"
                        << "#include <vector>\n"
                        << "#include <tuple>\n"
                        << "#include <utility>\n\n"
                        << "#define Package(name) namespace name\n"
                        << "#define fn inline auto\n\n"
                        << "template <typename... Args>\n"
                        << "inline void print(Args&&... args) {\n"
                        << "    (std::cout << ... << std::forward<Args>(args));\n"
                        << "}\n\n"
                        << "template <typename... Args>\n"
                        << "inline void println(Args&&... args) {\n"
                        << "    (std::cout << ... << std::forward<Args>(args)) << std::endl;\n"
                        << "}\n";
                zyncOut.close();
            }
        }
    }

    std::ofstream out(hppFile);
    if (!out.is_open()) {
        std::cerr << "\033[1;31m[Error]\033[0m Failed to create header file " << hppFile.string() << std::endl;
        return false;
    }

    out << "#pragma once\n\n"
        << "#include \"../zync.hpp\"\n\n"
        << "Package(" << name << ") {\n\n"
        << "    fn init() -> void {\n"
        << "        println(\"Native package [" << name << "] initialized!\");\n"
        << "    }\n\n"
        << "    fn add(int a, int b) -> int {\n"
        << "        return a + b;\n"
        << "    }\n\n"
        << "} // Package(" << name << ")\n";
    out.close();

    std::cout << "\033[1;32m[Zync Native Created]\033[0m Native package '" << name << "' -> " << hppFile.string() << std::endl;
    std::cout << "\nInclude in your Zync code with:\n";
    std::cout << "  import \"" << hppFile.string() << "\"\n";
    std::cout << "  " << name << "::init()\n" << std::endl;
    return true;
}

void printUsage() {
    std::cout << "\033[1;36mZync Compiler & Toolchain CLI\033[0m\n" << std::endl;
    std::cout << "\033[1mUsage:\033[0m" << std::endl;
    std::cout << "  ./zync create <project_name>       Create a new Zync project template" << std::endl;
    std::cout << "  ./zync create test <name>          Generate test suite in tests/<name>_test.zy" << std::endl;
    std::cout << "  ./zync create native <name>        Generate C++ native header in native/<name>/<name>.hpp" << std::endl;
    std::cout << "  ./zync add pkg <pkg_name>          Create new package file <pkg_name>.zy" << std::endl;
    std::cout << "  ./zync add test <name>             Generate test suite in tests/<name>_test.zy" << std::endl;
    std::cout << "  ./zync add wrapper <name>          Generate wrapper for predefined dependency (e.g. crow)" << std::endl;
    std::cout << "  ./zync add native <name>           Generate C++ native binding module in native/<name>/<name>.hpp" << std::endl;
    std::cout << "  ./zync build <file.zy> [options]   Compile source file to binary (default: build/<name>)" << std::endl;
    std::cout << "  ./zync run <name> [options]        Execute compiled native binary from build/<name>" << std::endl;
    std::cout << "  ./zync eval \"<code_snippet>\"       Instant evaluate code snippet" << std::endl;
    std::cout << "  ./zync repl                        Launch interactive REPL session" << std::endl;
    std::cout << "  ./zync serve [name]                Launch local HTTP server for WASM" << std::endl;
    std::cout << "  ./zync test <file.zy>              Build and run test suite with benchmark report (outputs to build/test/)" << std::endl;
    std::cout << "\n\033[1mBinary Size Presets:\033[0m" << std::endl;
    std::cout << "  --size small, -Os, -size small     Smallest binary footprint (-Os, section-GC, stripped tables)" << std::endl;
    std::cout << "  --size medium, -Om, -size medium   Balanced binary size & performance (-O2, section-GC)" << std::endl;
    std::cout << "  --size large, -Ol, -size large     Maximum performance & inlining (large footprint, -O3, loop unroll)" << std::endl;
    std::cout << "\n\033[1mOptimization Presets & Performance Levels:\033[0m" << std::endl;
    std::cout << "  (default)               Debug build (-O0)" << std::endl;
    std::cout << "  -O1                     Basic optimization" << std::endl;
    std::cout << "  -O2                     Standard release optimization" << std::endl;
    std::cout << "  -O3                     High performance & auto-vectorization" << std::endl;
    std::cout << "  -O4                     Ultra performance (LTO, loop unrolling, fast math)" << std::endl;
    std::cout << "  -O5                     Maximum performance (-Ofast, LTO, loop unrolling, loop interchange, auto-vectorization)" << std::endl;
    std::cout << "\n\033[1mGranular & Linker Flags:\033[0m" << std::endl;
    std::cout << "  -o, --output <name>     Custom executable output name" << std::endl;
    std::cout << "  -l<lib>                 Link external library (e.g. -lraylib)" << std::endl;
    std::cout << "  -L<dir>                 Add library search path" << std::endl;
    std::cout << "  -I<dir>                 Add header search path" << std::endl;
    std::cout << "  --framework <name>      Link macOS framework (e.g. --framework IOKit)" << std::endl;
    std::cout << "  --dce                   Dead code elimination" << std::endl;
    std::cout << "  --lto                   Link-Time Optimization" << std::endl;
    std::cout << "  --unroll                Force aggressive loop unrolling" << std::endl;
    std::cout << "  --fast-math             Fast floating point math" << std::endl;
    std::cout << "  --native                Tailor instructions for host CPU architecture" << std::endl;
    std::cout << "  -s, --strip             Strip symbol tables" << std::endl;
    std::cout << "\n\033[1mTargets & Execution:\033[0m" << std::endl;
    std::cout << "  -wasm, --target wasm    Compile to WebAssembly (.wasm, .js, .html)" << std::endl;
    std::cout << "  -j, --jobs <N>          Parallel worker threads" << std::endl;
    std::cout << "  -f, --force             Force recompilation" << std::endl;
    std::cout << "  -g, --gimple            Extract IR representation" << std::endl;
}