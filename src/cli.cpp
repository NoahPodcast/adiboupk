#include "adiboupk/cli.hpp"

#include <iostream>

namespace adiboupk {
namespace cli {

static Command parse_command(const std::string& cmd) {
    if (cmd == "init")    return Command::INIT;
    if (cmd == "install") return Command::INSTALL;
    if (cmd == "setup")   return Command::SETUP;
    if (cmd == "update")  return Command::UPDATE;
    if (cmd == "upgrade") return Command::UPGRADE;
    if (cmd == "run")     return Command::RUN;
    if (cmd == "audit")   return Command::AUDIT;
    if (cmd == "status")  return Command::STATUS;
    if (cmd == "clean")   return Command::CLEAN;
    if (cmd == "which")   return Command::WHICH;
    if (cmd == "help" || cmd == "--help" || cmd == "-h") return Command::HELP;
    if (cmd == "version" || cmd == "--version" || cmd == "-v") return Command::VERSION;
    return Command::UNKNOWN;
}

ParsedArgs parse(int argc, char* argv[]) {
    ParsedArgs args;

    if (argc < 2) {
        args.command = Command::HELP;
        return args;
    }

    int i = 1;

    // Parse global flags before command
    while (i < argc) {
        std::string arg = argv[i];
        if (arg == "--root" && i + 1 < argc) {
            args.project_root = argv[++i];
            i++;
        } else if (arg == "--verbose") {
            args.verbose = true;
            i++;
        } else if (arg == "--force") {
            args.force = true;
            i++;
        } else {
            break;
        }
    }

    if (i >= argc) {
        args.command = Command::HELP;
        return args;
    }

    args.command = parse_command(argv[i]);
    i++;

    // Parse command-specific arguments
    switch (args.command) {
        case Command::RUN:
        case Command::WHICH:
            if (i < argc) {
                args.script_path = argv[i];
                i++;
                // Remaining args are passed to the script
                while (i < argc) {
                    args.script_args.push_back(argv[i]);
                    i++;
                }
            }
            break;

        case Command::INSTALL:
        case Command::SETUP:
        case Command::UPDATE:
        case Command::UPGRADE:
            // Parse remaining flags
            while (i < argc) {
                std::string arg = argv[i];
                if (arg == "--force") {
                    args.force = true;
                }
                i++;
            }
            break;

        default:
            break;
    }

    return args;
}

void print_help() {
    std::cout <<
R"(adiboupk - Python dependency isolation for multi-module projects

USAGE:
    adiboupk <command> [options] [args]

COMMANDS:
    setup             All-in-one: scan, create venvs, install deps, audit
    update            Re-scan groups and reinstall changed dependencies
    upgrade           Update adiboupk itself to the latest version
    init              Scan project and create adiboupk.json config
    install           Create venvs and install dependencies for all groups
    run <script>      Run a Python script using the correct group's venv
    audit             Detect cross-group dependency conflicts
    status            Show groups, venvs, and dependency state
    clean             Remove all managed venvs
    which <script>    Show which python binary would be used for a script
    help              Show this help message
    version           Show version

OPTIONS:
    --root <path>     Project root directory (default: current directory)
    --force           Force reinstall even if dependencies are up to date
    --verbose         Show detailed output

EXAMPLES:
    adiboupk setup
    adiboupk run ./Enrichments/cortex_lookup.py hostname123
    adiboupk audit
    adiboupk which ./Responses/send_email.py
)";
}

void print_version() {
#ifdef ADIBOUPK_VERSION
    std::cout << "adiboupk " << ADIBOUPK_VERSION << std::endl;
#else
    std::cout << "adiboupk (unknown version)" << std::endl;
#endif
}

} // namespace cli
} // namespace adiboupk
