#include "adiboupk/venv.hpp"
#include "adiboupk/platform.hpp"

#include <iostream>

namespace adiboupk {
namespace venv {

bool create(const fs::path& venv_dir, const std::string& python_cmd) {
    // Ensure parent directory exists
    std::error_code ec;
    fs::create_directories(venv_dir.parent_path(), ec);

    std::vector<std::string> args = {"-m", "venv", venv_dir.string()};
    int rc = platform::run_process(python_cmd, args);
    if (rc != 0) {
        std::cerr << "adiboupk: failed to create venv at " << venv_dir.string()
                  << " (exit code " << rc << ")" << std::endl;
        return false;
    }

    // Verify the venv was created
    if (!venv::exists(venv_dir)) {
        std::cerr << "adiboupk: venv created but python binary not found in "
                  << venv_dir.string() << std::endl;
        return false;
    }

    return true;
}

bool exists(const fs::path& venv_dir) {
    auto python = platform::venv_python(venv_dir);
    return fs::exists(python);
}

bool destroy(const fs::path& venv_dir) {
    if (!fs::exists(venv_dir)) return true;
    return platform::remove_directory(venv_dir);
}

fs::path venv_dir_for(const Config& cfg, const Group& group) {
    return cfg.venvs_dir / group.name;
}

} // namespace venv
} // namespace adiboupk
