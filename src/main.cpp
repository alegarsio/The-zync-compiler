#include "../include/lexer.hpp"
#include "../include/parser.hpp"
#include "../include/codegen.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <unordered_set>
#include <unordered_map>
#include <cstdlib>
#include <vector>
#include <chrono>
#include <iomanip>
#include <thread>
#include <future>
#include <mutex>
#include <algorithm>

namespace fs = std::filesystem;

enum class OptLevel {
    O0,
    O1,
    O2,
    O3,
    O4,
    O5,
    CUSTOM
};

enum class SizeProfile {
    NONE,
    SMALL,
    MEDIUM,
    LARGE
};

enum class BuildTarget {
    NATIVE,
    WASM
};

struct GranularOptFlags {
    bool dce = false;
    bool lto = false;
    bool unroll = false;
    bool fastMath = false;
    bool nativeArch = false;
    bool stripSymbols = false;
};

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

bool parseFileRecursive(const fs::path& filePath, ProgramNode* mergedProgram, std::unordered_set<std::string>& visitedFiles, std::vector<fs::path>& allDepFiles) {
    std::string canonicalPath = fs::canonical(filePath).string();
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
    auto programAST = parser.parseProgram();

    fs::path currentDir = filePath.parent_path();
    for (auto& imp : programAST->imports) {
        if (imp->kind == ImportKind::ZYNC_FILE) {
            fs::path importedPath = currentDir / imp->target;
            if (!fs::exists(importedPath)) {
                std::cerr << "\033[1;31m[Error]\033[0m Imported file not found: " << imp->target << " (from " << filePath.string() << ")" << std::endl;
                return false;
            }
            if (!parseFileRecursive(importedPath, mergedProgram, visitedFiles, allDepFiles)) {
                return false;
            }
        } else {
            mergedProgram->imports.push_back(std::move(imp));
        }
    }

    for (auto& tr : programAST->traits) {
        mergedProgram->traits.push_back(std::move(tr));
    }

    for (auto& rec : programAST->records) {
        mergedProgram->records.push_back(std::move(rec));
    }

    for (auto& im : programAST->impls) {
        mergedProgram->impls.push_back(std::move(im));
    }

    for (auto& pkg : programAST->packages) {
        mergedProgram->packages.push_back(std::move(pkg));
    }

    for (auto& fn : programAST->functions) {
        mergedProgram->functions.push_back(std::move(fn));
    }

    for (auto& t : programAST->tests) {
        mergedProgram->tests.push_back(std::move(t));
    }

    return true;
}

struct CompileTask {
    std::string srcCpp;
    std::string outObj;
    std::string compileCmd;
    std::string name;
};

bool executeParallelJobs(const std::vector<CompileTask>& tasks, unsigned int maxJobs) {
    if (tasks.empty()) return true;

    unsigned int numThreads = (maxJobs > 0) ? maxJobs : std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 2;

    std::mutex printMutex;
    std::atomic<bool> allSuccess(true);
    std::atomic<size_t> taskIndex(0);

    auto worker = [&]() {
        while (true) {
            size_t idx = taskIndex.fetch_add(1);
            if (idx >= tasks.size()) break;
            if (!allSuccess.load()) break;

            const auto& task = tasks[idx];
            {
                std::lock_guard<std::mutex> lock(printMutex);
                std::cout << "\033[1;36m[Zync Parallel]\033[0m (" << (idx + 1) << "/" << tasks.size() << ") Compiling " << task.name << "..." << std::endl;
            }

            int res = std::system(task.compileCmd.c_str());
            if (res != 0) {
                allSuccess.store(false);
                std::lock_guard<std::mutex> lock(printMutex);
                std::cerr << "\033[1;31m[Compile Error]\033[0m Failed compiling unit: " << task.name << std::endl;
            }
        }
    };

    std::vector<std::thread> threadPool;
    unsigned int actualThreads = std::min<unsigned int>(numThreads, static_cast<unsigned int>(tasks.size()));
    for (unsigned int i = 0; i < actualThreads; ++i) {
        threadPool.emplace_back(worker);
    }

    for (auto& t : threadPool) {
        if (t.joinable()) t.join();
    }

    return allSuccess.load();
}

bool handleBuild(const std::string& inputPath, const std::string& customOutputName, OptLevel opt, const std::string& customFlags, const GranularOptFlags& granular, SizeProfile sizeProf, bool dumpGimple, BuildTarget target = BuildTarget::NATIVE, bool forceRebuild = false, bool isQuietMode = false, unsigned int jobs = 0) {
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

    fs::path buildDir = "build";
    fs::path objDir = buildDir / "obj";
    fs::path gimpleDir = buildDir / "gimple";
    fs::path wasmDir = buildDir / "wasm";
    fs::path cacheManifestPath = objDir / (outputName + ".cache");

    if (!fs::exists(buildDir)) fs::create_directories(buildDir);
    if (!fs::exists(objDir)) fs::create_directories(objDir);
    if (dumpGimple && !fs::exists(gimpleDir)) fs::create_directories(gimpleDir);
    if (target == BuildTarget::WASM && !fs::exists(wasmDir)) fs::create_directories(wasmDir);

    fs::path exePath = (target == BuildTarget::WASM) ? (wasmDir / (outputName + ".wasm")) : (buildDir / outputName);

    std::string optFlags;
    std::string linkFlags;
    std::string optDescription;

    if (isQuietMode) {
        optFlags = "-O0";
        optDescription = "JIT / Fast Eval";
    } else if (opt == OptLevel::CUSTOM && !customFlags.empty()) {
        optFlags = customFlags;
        optDescription = "Custom (" + customFlags + ")";
    } else if (sizeProf == SizeProfile::SMALL) {
        optFlags = "-Os -ffunction-sections -fdata-sections";
        if (target == BuildTarget::NATIVE) {
#if defined(__APPLE__)
            linkFlags += " -Wl,-dead_strip";
#else
            linkFlags += " -s -Wl,--gc-sections";
#endif
        }
        optDescription = "Size: Small (-Os)";
    } else if (sizeProf == SizeProfile::MEDIUM) {
        optFlags = "-O2";
        optDescription = "Size: Medium (-O2 Balanced)";
    } else if (sizeProf == SizeProfile::LARGE) {
        optFlags = (target == BuildTarget::WASM)
            ? "-O3 -funroll-loops -DNDEBUG"
            : "-O3 -march=native -funroll-loops -DNDEBUG";
        optDescription = "Size: Large (-O3 Max Performance & Inlining)";
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
                    : "-Ofast -march=native -mtune=native -flto -funroll-loops -fomit-frame-pointer -ffast-math -DNDEBUG";
                linkFlags += " -flto";
                optDescription = "Level 5 (Extreme Optimization: Max Unrolling, Vectorization, LTO, Fast Math)";
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
    std::string configSignature = optFlags + "|" + linkFlags + "|" + (target == BuildTarget::WASM ? "wasm" : "native");
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
        std::cout << "\033[1;36m[Zync]\033[0m Target: " << zyPath.filename().string() << " -> build/" << outputName 
                  << " (" << optDescription << ") [Parallel Jobs: " << detectedCores << "]" << std::endl;
    }

    std::vector<CompileTask> parallelTasks;
    std::vector<std::string> objectFilesToLink;
    std::unordered_map<std::string, std::string> newCache;

    for (const auto& fileDep : allDepFiles) {
        std::string unitStem = fileDep.stem().string();
        fs::path unitCpp = objDir / (unitStem + ".cpp");
        fs::path unitObj = objDir / (unitStem + ".o");
        fs::path unitCacheFile = objDir / (unitStem + ".cache");

        uint64_t fileH = computeFileHash(fileDep) ^ computeHash(configSignature);
        auto unitCache = loadCacheManifest(unitCacheFile);

        objectFilesToLink.push_back(unitObj.string());

        if (!forceRebuild && fs::exists(unitObj) && unitCache["hash"] == std::to_string(fileH)) {
            continue;
        }

        auto singleProgram = std::make_unique<ProgramNode>();
        std::unordered_set<std::string> tempVisited;
        std::vector<fs::path> tempDeps;
        parseFileRecursive(fileDep, singleProgram.get(), tempVisited, tempDeps);

        CodeGen codegen(singleProgram.get());
        std::string cppCode = codegen.generate();

        std::ofstream out(unitCpp);
        out << cppCode;
        out.close();

        std::string compileCmd;
        if (target == BuildTarget::WASM) {
            compileCmd = "em++ -std=c++17 -I. -Iinclude " + optFlags + " -c " + unitCpp.string() + " -o " + unitObj.string();
        } else {
            compileCmd = "g++ -std=c++17 -I. -Iinclude " + optFlags + " -c " + unitCpp.string() + " -o " + unitObj.string();
        }

        parallelTasks.push_back({unitCpp.string(), unitObj.string(), compileCmd, fileDep.filename().string()});

        unitCache["hash"] = std::to_string(fileH);
        saveCacheManifest(unitCacheFile, unitCache);
    }

    if (!parallelTasks.empty()) {
        if (!executeParallelJobs(parallelTasks, detectedCores)) {
            return false;
        }
    }

    std::ostringstream linkObjs;
    for (const auto& obj : objectFilesToLink) {
        linkObjs << obj << " ";
    }

    if (target == BuildTarget::WASM) {
        fs::path wasmHtmlOut = wasmDir / (outputName + ".html");
        std::string wasmOpt = optFlags.empty() ? "-O3" : optFlags;
        std::string emccCmd = "em++ -std=c++17 -I. -Iinclude " + wasmOpt + " -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 " + linkObjs.str() + linkFlags + " -o " + wasmHtmlOut.string();
        int wasmRes = std::system(emccCmd.c_str());
        if (wasmRes != 0) {
            std::cerr << "\033[1;31m[Zync Error]\033[0m Linker step for WASM failed." << std::endl;
            return false;
        }
    } else {
        std::string linkCmd = "g++ -std=c++17 -I. -Iinclude " + optFlags + " " + linkObjs.str() + linkFlags + " -o " + (buildDir / outputName).string();
        int linkRes = std::system(linkCmd.c_str());
        if (linkRes != 0) {
            std::cerr << "\033[1;31m[Zync Error]\033[0m Linker step failed." << std::endl;
            return false;
        }
    }

    if (dumpGimple) {
        fs::path mainCpp = objDir / (outputName + ".cpp");
        std::string gccGimpleCmd = "g++ -std=c++17 -I. -Iinclude " + optFlags + " -fdump-tree-gimple -c " + mainCpp.string() + " -o " + (objDir / (outputName + ".o")).string() + " 2>/dev/null";
        std::system(gccGimpleCmd.c_str());
        for (const auto& entry : fs::directory_iterator(fs::current_path())) {
            if (entry.is_regular_file() && entry.path().filename().string().find(".gimple") != std::string::npos) {
                fs::path targetPath = gimpleDir / entry.path().filename();
                fs::rename(entry.path(), targetPath);
                std::cout << "\033[1;32m[Zync IR]\033[0m GIMPLE IR generated: " << targetPath.string() << std::endl;
            }
        }
    }

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

bool handleRun(const std::string& targetName, bool isWasm = false) {
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

    fs::path exePath = fs::path("build") / stemName;
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
        fullSession << "import std\n\nfn main() -> void {\n";
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
    if (!handleBuild(zyPath.string(), "", OptLevel::O0, "", granular, SizeProfile::NONE, false, BuildTarget::NATIVE, true, true)) {
        std::cerr << "\033[1;31m[Zync Test Error]\033[0m Test build pipeline aborted." << std::endl;
        return false;
    }

    bool success = handleRun(zyPath.string());
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
        mainOut << "import std\n\n";
        mainOut << "fn main() -> void {\n";
        mainOut << "    println(\"Hello from Zync project: " << projectName << "!\")\n";
        mainOut << "}\n";
        mainOut.close();

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

void printUsage() {
    std::cout << "\033[1;36mZync Compiler & Toolchain CLI\033[0m\n" << std::endl;
    std::cout << "\033[1mUsage:\033[0m" << std::endl;
    std::cout << "  ./zync create <project_name>       Create a new Zync project template" << std::endl;
    std::cout << "  ./zync build <file.zy> [options]   Compile source" << std::endl;
    std::cout << "  ./zync run <name> [options]        Execute binary" << std::endl;
    std::cout << "  ./zync eval \"<code_snippet>\"       Instant evaluate code snippet" << std::endl;
    std::cout << "  ./zync repl                        Launch interactive REPL session" << std::endl;
    std::cout << "  ./zync serve [name]                Launch local HTTP server for WASM" << std::endl;
    std::cout << "  ./zync test <file.zy>              Build and run test suite with benchmark report" << std::endl;
    std::cout << "\n\033[1mOptimization Presets:\033[0m" << std::endl;
    std::cout << "  (default)               Debug build (-O0)" << std::endl;
    std::cout << "  -O1, --opt 1            Level 1: Basic optimization" << std::endl;
    std::cout << "  -O2, --opt 2            Level 2: Standard release optimization" << std::endl;
    std::cout << "  -O3, --opt 3            Level 3: High performance (SIMD, auto-vectorize)" << std::endl;
    std::cout << "  -O4, --opt 4            Level 4: Ultra performance (LTO, fast-math, unroll)" << std::endl;
    std::cout << "  -O5, --opt 5            Level 5: Extreme optimization (Max unrolling, LTO, fast-math)" << std::endl;
    std::cout << "\n\033[1mCustom Granular Optimizations (Mix & Match):\033[0m" << std::endl;
    std::cout << "  --dce, --dead-code      Eliminate unused functions/variables at link time" << std::endl;
    std::cout << "  --lto                   Enable Link-Time Optimization across all units" << std::endl;
    std::cout << "  --unroll                Force aggressive loop unrolling" << std::endl;
    std::cout << "  --fast-math             Allow aggressive non-IEEE floating point math" << std::endl;
    std::cout << "  --native                Generate instructions tailored for host CPU architecture" << std::endl;
    std::cout << "  -s, --strip             Strip all debug and symbol tables from binary" << std::endl;
    std::cout << "  -C, --custom-opt \"...\"  Inject raw backend C++ compiler flags" << std::endl;
    std::cout << "\n\033[1mBinary Size Profiles:\033[0m" << std::endl;
    std::cout << "  -Os, --size small       Smallest binary footprint" << std::endl;
    std::cout << "  --size medium           Balanced size and runtime speed" << std::endl;
    std::cout << "  --size large            Maximum throughput & loop expansion" << std::endl;
    std::cout << "\n\033[1mParallel Build & Caching:\033[0m" << std::endl;
    std::cout << "  -j, --jobs <N>          Set number of parallel worker threads (default: auto CPU cores)" << std::endl;
    std::cout << "  -o, --output <name>     Custom executable artifact name (default: file stem name)" << std::endl;
    std::cout << "  -f, --force             Force recompilation (bypass incremental cache)" << std::endl;
    std::cout << "\n\033[1mTargets & Execution:\033[0m" << std::endl;
    std::cout << "  (default)               Native Executable" << std::endl;
    std::cout << "  -wasm, --target wasm    Compile to WebAssembly (.wasm, .js, .html)" << std::endl;
    std::cout << "  ./zync run <name> -wasm Run WebAssembly file via Node.js" << std::endl;
    std::cout << "  ./zync serve <name>     Host WebAssembly HTML file in browser" << std::endl;
    std::cout << "\n\033[1mDiagnostics & IR:\033[0m" << std::endl;
    std::cout << "  -g, --gimple            Extract IR representation into build/gimple/" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string command = argv[1];

    if (command == "help" || command == "--help" || command == "-h") {
        printUsage();
        return 0;
    }

    if (command == "repl" || command == "-repl" || command == "--repl") {
        handleRepl();
        return 0;
    }

    if (command == "eval" || command == "-eval" || command == "-e") {
        if (argc < 3) {
            std::cerr << "\033[1;31m[Error]\033[0m Missing code snippet. Usage: zync eval \"<code_to_eval>\"" << std::endl;
            return 1;
        }
        std::string snippet = argv[2];
        return handleEval(snippet) ? 0 : 1;
    }

    if (command == "serve" || command == "-serve" || command == "--serve") {
        std::string target = (argc >= 3) ? argv[2] : "main";
        return handleServe(target) ? 0 : 1;
    }

    if (command == "create" || command == "-create" || command == "--create" || command == "new") {
        if (argc < 3) {
            std::cerr << "\033[1;31m[Error]\033[0m Missing project name. Usage: zync create <project_name>" << std::endl;
            return 1;
        }
        std::string projectName = argv[2];
        return handleCreate(projectName) ? 0 : 1;
    }

    if (command == "test") {
        std::string targetFile = (argc >= 3) ? argv[2] : "main.zy";
        return handleTest(targetFile) ? 0 : 1;
    }

    if (command == "build") {
        if (argc < 3) {
            std::cerr << "\033[1;31m[Error]\033[0m Missing target file. Usage: zync build <file.zy> [options]" << std::endl;
            return 1;
        }

        std::string targetFile = argv[2];
        std::string customOutput = fs::path(targetFile).stem().string();
        std::string customFlags = "";
        GranularOptFlags granular;
        OptLevel opt = OptLevel::O0;
        SizeProfile sizeProf = SizeProfile::NONE;
        BuildTarget target = BuildTarget::NATIVE;
        bool dumpGimple = false;
        bool forceRebuild = false;
        unsigned int jobs = 0;

        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
                customOutput = argv[++i];
            } else if ((arg == "-j" || arg == "--jobs") && i + 1 < argc) {
                jobs = static_cast<unsigned int>(std::stoul(argv[++i]));
            } else if ((arg == "-C" || arg == "--custom-opt") && i + 1 < argc) {
                customFlags = argv[++i];
                opt = OptLevel::CUSTOM;
            } else if (arg == "--dce" || arg == "--dead-code" || arg == "--dead-code-elimination") {
                granular.dce = true;
            } else if (arg == "--lto") {
                granular.lto = true;
            } else if (arg == "--unroll") {
                granular.unroll = true;
            } else if (arg == "--fast-math") {
                granular.fastMath = true;
            } else if (arg == "--native") {
                granular.nativeArch = true;
            } else if (arg == "-s" || arg == "--strip") {
                granular.stripSymbols = true;
            } else if (arg == "-f" || arg == "--force") {
                forceRebuild = true;
            } else if (arg == "-O1" || (arg == "--opt" && i + 1 < argc && std::string(argv[i + 1]) == "1")) {
                opt = OptLevel::O1;
            } else if (arg == "-O2" || (arg == "--opt" && i + 1 < argc && std::string(argv[i + 1]) == "2")) {
                opt = OptLevel::O2;
            } else if (arg == "-O3" || (arg == "--opt" && i + 1 < argc && std::string(argv[i + 1]) == "3")) {
                opt = OptLevel::O3;
            } else if (arg == "-O4" || (arg == "--opt" && i + 1 < argc && std::string(argv[i + 1]) == "4")) {
                opt = OptLevel::O4;
            } else if (arg == "-O5" || (arg == "--opt" && i + 1 < argc && std::string(argv[i + 1]) == "5")) {
                opt = OptLevel::O5;
            } else if (arg == "-Os" || (arg == "--size" && i + 1 < argc && std::string(argv[i + 1]) == "small")) {
                sizeProf = SizeProfile::SMALL;
            } else if (arg == "--size" && i + 1 < argc && std::string(argv[i + 1]) == "medium") {
                sizeProf = SizeProfile::MEDIUM;
            } else if (arg == "--size" && i + 1 < argc && std::string(argv[i + 1]) == "large") {
                sizeProf = SizeProfile::LARGE;
            } else if (arg == "-g" || arg == "--gimple") {
                dumpGimple = true;
            } else if (arg == "-wasm" || arg == "--wasm" || (arg == "--target" && i + 1 < argc && std::string(argv[i + 1]) == "wasm")) {
                target = BuildTarget::WASM;
            }
        }

        return handleBuild(targetFile, customOutput, opt, customFlags, granular, sizeProf, dumpGimple, target, forceRebuild, false, jobs) ? 0 : 1;
    }

    if (command == "run") {
        if (argc < 3) {
            std::cerr << "\033[1;31m[Error]\033[0m Missing target name. Usage: zync run <name> [options]" << std::endl;
            return 1;
        }

        std::string target = argv[2];
        bool isWasm = false;

        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-wasm" || arg == "--wasm") {
                isWasm = true;
            }
        }
        return handleRun(target, isWasm) ? 0 : 1;
    }

    std::cerr << "\033[1;31m[Error]\033[0m Unknown command: " << command << std::endl;
    printUsage();
    return 1;
}