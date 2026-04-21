#include "adiboupk/discovery.hpp"
#include "adiboupk/platform.hpp"
#include "adiboupk/utils.hpp"

#include <algorithm>
#include <set>

namespace adiboupk {
namespace discovery {

static const std::set<std::string> IGNORED_DIRS = {
    ".venvs", "node_modules", "__pycache__", ".git", ".github",
    "build", "dist", "third_party", ".tox", ".pytest_cache"
};

static bool should_ignore(const fs::path& dir_name) {
    std::string name = dir_name.filename().string();
    if (name.empty() || name[0] == '.') return true;
    return IGNORED_DIRS.count(name) > 0;
}

std::vector<Group> scan(const fs::path& project_root) {
    std::vector<Group> groups;
    std::error_code ec;

    for (auto& entry : fs::directory_iterator(project_root, ec)) {
        if (!entry.is_directory()) continue;
        if (should_ignore(entry.path())) continue;

        fs::path req_path = entry.path() / "requirements.txt";
        if (!fs::exists(req_path)) continue;

        Group group;
        group.name = entry.path().filename().string();
        group.directory = entry.path();
        group.requirements_path = req_path;
        group.requirements_hash = utils::sha256_file(req_path);

        groups.push_back(std::move(group));
    }

    // Sort by name for deterministic output
    std::sort(groups.begin(), groups.end(),
              [](const Group& a, const Group& b) { return a.name < b.name; });

    return groups;
}

const Group* find_group_for_script(const std::vector<Group>& groups,
                                    const fs::path& script_path) {
    auto norm_script = platform::normalize_path(script_path);

    for (const auto& group : groups) {
        auto norm_dir = platform::normalize_path(group.directory);
        if (platform::is_under(norm_script, norm_dir)) {
            return &group;
        }
    }
    return nullptr;
}

} // namespace discovery
} // namespace adiboupk
