#include "adiboupk/isolator.hpp"
#include "adiboupk/platform.hpp"
#include "adiboupk/utils.hpp"
#include "adiboupk/venv.hpp"
#include "nlohmann/json.hpp"

#include <iostream>

using json = nlohmann::json;

namespace adiboupk {
namespace isolator {

bool install_isolated(const fs::path& deps_dir,
                      const fs::path& requirements_path,
                      const std::string& python_cmd) {
    std::string content = utils::read_file(requirements_path);
    if (content.empty()) {
        std::cerr << "adiboupk: empty or missing " << requirements_path.string() << std::endl;
        return false;
    }

    auto reqs = utils::parse_requirements(content);
    if (reqs.empty()) {
        std::cerr << "adiboupk: no packages found in " << requirements_path.string() << std::endl;
        return false;
    }

    // Create deps directory
    std::error_code ec;
    fs::create_directories(deps_dir, ec);

    json package_map;
    bool all_ok = true;

    for (const auto& [pkg_name, req_line] : reqs) {
        fs::path pkg_dir = deps_dir / pkg_name;

        // Clean and recreate package directory
        if (fs::exists(pkg_dir)) {
            fs::remove_all(pkg_dir, ec);
        }
        fs::create_directories(pkg_dir, ec);

        std::cout << "      " << req_line << " -> " << pkg_name << "/" << std::endl;

        // pip install --target=<dir> <package>
        int rc = platform::run_process(
            python_cmd,
            {"-m", "pip", "install", "--target", pkg_dir.string(),
             req_line, "--quiet", "--no-warn-conflicts"},
            false, nullptr
        );

        if (rc != 0) {
            std::cerr << "      Failed to install " << req_line << std::endl;
            all_ok = false;
            continue;
        }

        package_map[pkg_name] = pkg_dir.string();
    }

    // Write package_map.json
    fs::path map_path = deps_dir / "package_map.json";
    if (!utils::write_file(map_path, package_map.dump(2) + "\n")) {
        std::cerr << "adiboupk: failed to write " << map_path.string() << std::endl;
        return false;
    }

    return all_ok;
}

fs::path deps_dir_for(const Config& cfg, const Group& group) {
    std::string safe_name = group.name;
    for (auto& c : safe_name) {
        if (c == '/') c = '_';
    }
    return cfg.venvs_dir / (safe_name + "_isolated");
}

fs::path package_map_for(const Config& cfg, const Group& group) {
    return deps_dir_for(cfg, group) / "package_map.json";
}

bool exists(const Config& cfg, const Group& group) {
    return fs::exists(package_map_for(cfg, group));
}

} // namespace isolator
} // namespace adiboupk
