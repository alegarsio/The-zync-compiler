#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <filesystem>
#include "ast.hpp"

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

bool parseFileRecursive(const fs::path& filePath, ProgramNode* mergedProgram, std::unordered_set<std::string>& visitedFiles, std::vector<fs::path>& allDepFiles);
bool handleBuild(const std::string& inputPath, const std::string& customOutputName, OptLevel opt, const std::string& customFlags, const GranularOptFlags& granular, SizeProfile sizeProf, bool dumpGimple, BuildTarget target = BuildTarget::NATIVE, bool forceRebuild = false, bool isQuietMode = false, unsigned int jobs = 0, const std::string& userLinkFlags = "", const std::string& userIncludeFlags = "", bool isTestBuild = false);
bool handleRun(const std::string& targetName, bool isWasm = false, bool isTestRun = false);
bool handleEval(const std::string& codeSnippet);
void handleRepl();
bool handleServe(const std::string& targetName);
bool handleTest(const std::string& targetFile);
bool handleCreate(const std::string& projectName);
bool handleAddPackage(const std::string& pkgName);
bool handleCreateTest(const std::string& rawTestName);
bool handleCreateNative(const std::string& rawName);
void printUsage();