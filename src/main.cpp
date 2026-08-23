#include "../include/builder.hpp"
#include "../include/config.hpp"
#include <iostream>
#include <string>

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
            std::cerr << "\033[1;31m[Error]\033[0m Missing argument. Usage: zync create <project_name> OR zync create test <name> OR zync create native <name>" << std::endl;
            return 1;
        }
        std::string arg2 = argv[2];
        if (arg2 == "test") {
            if (argc < 4) {
                std::cerr << "\033[1;31m[Error]\033[0m Missing test suite name. Usage: zync create test <name>" << std::endl;
                return 1;
            }
            return handleCreateTest(argv[3]) ? 0 : 1;
        }
        if (arg2 == "native" || arg2 == "cpp") {
            if (argc < 4) {
                std::cerr << "\033[1;31m[Error]\033[0m Missing module name. Usage: zync create native <name>" << std::endl;
                return 1;
            }
            return handleCreateNative(argv[3]) ? 0 : 1;
        }
        return handleCreate(arg2) ? 0 : 1;
    }

    if (command == "add") {
        if (argc < 3) {
            std::cerr << "\033[1;31m[Error]\033[0m Missing argument. Usage: zync add pkg <name> OR zync add test <name> OR zync add dep <name> OR zync add native <name>" << std::endl;
            return 1;
        }
        std::string subCmd = argv[2];
        if (subCmd == "pkg" || subCmd == "package") {
            if (argc < 4) {
                std::cerr << "\033[1;31m[Error]\033[0m Missing package name. Usage: zync add pkg <name>" << std::endl;
                return 1;
            }
            return handleAddPackage(argv[3]) ? 0 : 1;
        } else if (subCmd == "test") {
            if (argc < 4) {
                std::cerr << "\033[1;31m[Error]\033[0m Missing test name. Usage: zync add test <name>" << std::endl;
                return 1;
            }
            return handleCreateTest(argv[3]) ? 0 : 1;
        } else if (subCmd == "dep" || subCmd == "dependency") {
            if (argc < 4) {
                std::cerr << "\033[1;31m[Error]\033[0m Missing dependency name. Usage: zync add dep <name>" << std::endl;
                return 1;
            }
            return handleAddDependency(argv[3]) ? 0 : 1;
        } else if (subCmd == "native" || subCmd == "cpp") {
            if (argc < 4) {
                std::cerr << "\033[1;31m[Error]\033[0m Missing native module name. Usage: zync add native <name>" << std::endl;
                return 1;
            }
            return handleCreateNative(argv[3]) ? 0 : 1;
        }
        return handleAddPackage(subCmd) ? 0 : 1;
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
        std::string defaultOutName = fs::path(targetFile).stem().string();
        std::string customOutput = "";
        std::string customFlags = "";
        std::string userLinkFlags = "";
        std::string userIncludeFlags = "";

        ZyncConfig toml = loadZyncToml();
        if (toml.loaded) {
            if (!toml.name.empty()) {
                customOutput = toml.name;
            }
            for (const auto& lib : toml.linkLibs) userLinkFlags += " -l" + lib;
            for (const auto& ldir : toml.libDirs) userLinkFlags += " -L" + ldir;
            for (const auto& idir : toml.includeDirs) userIncludeFlags += " -I" + idir;
        }

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
            } else if (arg.rfind("-l", 0) == 0 || arg.rfind("-L", 0) == 0) {
                userLinkFlags += " " + arg;
            } else if (arg == "--framework" && i + 1 < argc) {
                userLinkFlags += " -framework " + std::string(argv[++i]);
            } else if (arg.rfind("-I", 0) == 0) {
                userIncludeFlags += " " + arg;
            } else if (arg == "--dce") {
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
            } else if (arg == "-O1") {
                opt = OptLevel::O1;
            } else if (arg == "-O2") {
                opt = OptLevel::O2;
            } else if (arg == "-O3") {
                opt = OptLevel::O3;
            } else if (arg == "-O4") {
                opt = OptLevel::O4;
            } else if (arg == "-O5") {
                opt = OptLevel::O5;
            } else if (arg == "-Os") {
                sizeProf = SizeProfile::SMALL;
            } else if (arg == "-g" || arg == "--gimple") {
                dumpGimple = true;
            } else if (arg == "-wasm" || arg == "--wasm") {
                target = BuildTarget::WASM;
            }
        }

        if (customOutput.empty()) {
            customOutput = defaultOutName;
        }

        return handleBuild(targetFile, customOutput, opt, customFlags, granular, sizeProf, dumpGimple, target, forceRebuild, false, jobs, userLinkFlags, userIncludeFlags) ? 0 : 1;
    }

    if (command == "run") {
        if (argc < 3) {
            std::cerr << "\033[1;31m[Error]\033[0m Missing target name. Usage: zync run <name> [options]" << std::endl;
            return 1;
        }

        std::string target = argv[2];
        bool isWasm = false;
        for (int i = 3; i < argc; ++i) {
            if (std::string(argv[i]) == "-wasm" || std::string(argv[i]) == "--wasm") isWasm = true;
        }
        return handleRun(target, isWasm) ? 0 : 1;
    }

    std::cerr << "\033[1;31m[Error]\033[0m Unknown command: " << command << std::endl;
    printUsage();
    return 1;
}