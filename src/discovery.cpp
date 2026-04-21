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

// Extract subgroup name from "requirements-foo.txt" -> "foo"
static std::string extract_subgroup_name(const std::string& filename) {
    // "requirements-foo.txt" -> "foo"
    if (filename.size() <= 17) return {}; // "requirements-.txt" = 17 chars minimum
    if (filename.substr(0, 13) != "requirements-") return {};
    if (filename.substr(filename.size() - 4) != ".txt") return {};
    return filename.substr(13, filename.size() - 17);
}

std::vector<Group> scan(const fs::path& project_root) {
    std::vector<Group> groups;
    std::error_code ec;

    for (auto& entry : fs::directory_iterator(project_root, ec)) {
        if (!entry.is_directory()) continue;
        if (should_ignore(entry.path())) continue;

        std::string dir_name = entry.path().filename().string();
        bool has_default = false;

        // Check for requirements-*.txt (subgroups) first
        std::vector<Group> subgroups;
        for (auto& file : fs::directory_iterator(entry.path(), ec)) {
            if (!file.is_regular_file()) continue;
            std::string fname = file.path().filename().string();

            if (fname == "requirements.txt") {
                has_default = true;
                continue;
            }

            std::string sub_name = extract_subgroup_name(fname);
            if (sub_name.empty()) continue;

            Group sg;
            sg.name = dir_name + "/" + sub_name;
            sg.directory = entry.path();
            sg.requirements_path = file.path();
            sg.requirements_hash = utils::sha256_file(file.path());
            subgroups.push_back(std::move(sg));
        }

        if (!subgroups.empty()) {
            // Auto-map scripts to subgroups by naming convention:
            // "script_vt.py" or "vt_lookup.py" matches subgroup "vt"
            // if the filename contains the subgroup suffix
            for (auto& file : fs::directory_iterator(entry.path(), ec)) {
                if (!file.is_regular_file()) continue;
                std::string fname = file.path().filename().string();
                if (fname.size() < 4 || fname.substr(fname.size() - 3) != ".py") continue;

                for (auto& sg : subgroups) {
                    // Extract suffix from group name: "Enrichments/vt" -> "vt"
                    auto slash_pos = sg.name.rfind('/');
                    if (slash_pos == std::string::npos) continue;
                    std::string suffix = sg.name.substr(slash_pos + 1);

                    // Match if filename contains the suffix
                    // e.g., "script_vt.py" contains "vt", "vt_lookup.py" contains "vt"
                    std::string fname_lower = fname;
                    std::transform(fname_lower.begin(), fname_lower.end(),
                                   fname_lower.begin(), ::tolower);
                    std::string suffix_lower = suffix;
                    std::transform(suffix_lower.begin(), suffix_lower.end(),
                                   suffix_lower.begin(), ::tolower);

                    if (fname_lower.find(suffix_lower) != std::string::npos) {
                        sg.scripts.push_back(fname);
                        break; // Each script maps to at most one subgroup
                    }
                }
            }

            // Add subgroups
            for (auto& sg : subgroups) {
                groups.push_back(std::move(sg));
            }
            // Also add the default requirements.txt as a fallback group if it exists
            if (has_default) {
                Group fallback;
                fallback.name = dir_name;
                fallback.directory = entry.path();
                fallback.requirements_path = entry.path() / "requirements.txt";
                fallback.requirements_hash = utils::sha256_file(fallback.requirements_path);
                groups.push_back(std::move(fallback));
            }
        } else if (has_default) {
            // No subgroups, just the default requirements.txt
            Group group;
            group.name = dir_name;
            group.directory = entry.path();
            group.requirements_path = entry.path() / "requirements.txt";
            group.requirements_hash = utils::sha256_file(group.requirements_path);
            groups.push_back(std::move(group));
        }
    }

    // Sort by name for deterministic output
    std::sort(groups.begin(), groups.end(),
              [](const Group& a, const Group& b) { return a.name < b.name; });

    return groups;
}

const Group* find_group_for_script(const std::vector<Group>& groups,
                                    const fs::path& script_path) {
    auto norm_script = platform::normalize_path(script_path);
    std::string script_filename = script_path.filename().string();

    // Pass 1: Check explicit script mappings (most specific match)
    for (const auto& group : groups) {
        if (group.scripts.empty()) continue;
        auto norm_dir = platform::normalize_path(group.directory);
        if (!platform::is_under(norm_script, norm_dir)) continue;

        for (const auto& pattern : group.scripts) {
            if (script_filename == pattern) {
                return &group;
            }
        }
    }

    // Pass 2: Check subgroups (groups with "/" in name) by directory
    // These are more specific than plain directory groups
    for (const auto& group : groups) {
        if (group.name.find('/') == std::string::npos) continue;
        if (!group.scripts.empty()) continue; // Already checked in pass 1
        auto norm_dir = platform::normalize_path(group.directory);
        if (platform::is_under(norm_script, norm_dir)) {
            return &group;
        }
    }

    // Pass 3: Fall back to directory-level groups (no "/" in name)
    for (const auto& group : groups) {
        if (group.name.find('/') != std::string::npos) continue;
        auto norm_dir = platform::normalize_path(group.directory);
        if (platform::is_under(norm_script, norm_dir)) {
            return &group;
        }
    }

    return nullptr;
}

} // namespace discovery
} // namespace adiboupk
