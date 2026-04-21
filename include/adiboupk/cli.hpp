#pragma once

#include <string>
#include <vector>

namespace adiboupk {
namespace cli {

enum class Command {
    INIT,       // Initialize config by scanning for groups
    INSTALL,    // Create venvs and install dependencies
    SETUP,      // All-in-one: init + install + audit
    UPDATE,     // Re-scan groups + reinstall changed dependencies
    UPGRADE,    // Self-update the adiboupk binary
    RUN,        // Run a script in the correct venv
    AUDIT,      // Check for cross-group conflicts
    STATUS,     // Show current state (groups, venvs, hashes)
    CLEAN,      // Remove all venvs
    UNINSTALL,  // Remove adiboupk binary and optionally project files
    WHICH,      // Show which python would be used for a script
    HELP,       // Show help
    VERSION,    // Show version
    UNKNOWN
};

struct ParsedArgs {
    Command command = Command::HELP;
    std::string script_path;                // For RUN and WHICH
    std::vector<std::string> script_args;   // For RUN: args passed to the script
    std::string project_root;               // --root flag (default: cwd)
    bool force = false;                     // --force flag (reinstall even if up to date)
    bool verbose = false;                   // --verbose flag
};

// Parse command line arguments
ParsedArgs parse(int argc, char* argv[]);

// Print help message
void print_help();

// Print version
void print_version();

} // namespace cli
} // namespace adiboupk
