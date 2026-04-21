#include "adiboupk/auditor.hpp"
#include "adiboupk/installer.hpp"
#include "adiboupk/utils.hpp"
#include "adiboupk/venv.hpp"

#include <map>
#include <sstream>

namespace adiboupk {
namespace auditor {

std::vector<Conflict> audit(const std::vector<Group>& groups) {
    // package_name -> list of (group_name, requirement_line, version_spec)
    std::map<std::string, std::vector<Conflict::GroupVersion>> package_map;

    for (const auto& group : groups) {
        std::string content = utils::read_file(group.requirements_path);
        if (content.empty()) continue;

        auto reqs = utils::parse_requirements(content);
        for (const auto& [pkg_name, req_line] : reqs) {
            std::string version = utils::extract_version_spec(req_line);
            package_map[pkg_name].push_back({group.name, req_line, version});
        }
    }

    std::vector<Conflict> conflicts;

    for (auto& [pkg_name, versions] : package_map) {
        if (versions.size() < 2) continue;

        // Check if all version specs are identical
        bool all_same = true;
        for (size_t i = 1; i < versions.size(); i++) {
            if (versions[i].version_spec != versions[0].version_spec) {
                all_same = false;
                break;
            }
        }

        if (!all_same) {
            Conflict c;
            c.package_name = pkg_name;
            c.versions = std::move(versions);
            conflicts.push_back(std::move(c));
        }
    }

    return conflicts;
}

std::string format_conflicts(const std::vector<Conflict>& conflicts) {
    if (conflicts.empty()) {
        return "No conflicts detected.\n";
    }

    std::ostringstream out;
    out << "Found " << conflicts.size() << " conflict(s):\n\n";

    for (const auto& c : conflicts) {
        out << "  " << c.package_name << ":\n";
        for (const auto& v : c.versions) {
            out << "    " << v.group_name << " requires: " << v.requirement_line << "\n";
        }
        out << "\n";
    }

    out << "These conflicts are safely isolated by adiboupk (each group has its own venv).\n"
        << "This audit is informational — no action required unless you want to unify versions.\n";

    return out.str();
}

bool has_conflicts(const std::vector<Group>& groups) {
    return !audit(groups).empty();
}

std::string audit_transitive(const Config& cfg) {
    std::ostringstream out;
    bool any_issues = false;

    for (const auto& group : cfg.groups) {
        auto vdir = venv::venv_dir_for(cfg, group);
        if (!venv::exists(vdir)) continue;

        std::string check_output = installer::pip_check(vdir);
        if (check_output.empty()) continue;

        // "No broken requirements found." means all good
        if (check_output.find("No broken requirements found") != std::string::npos) {
            continue;
        }

        if (!any_issues) {
            out << "Transitive dependency conflicts (pip check):\n\n";
            any_issues = true;
        }

        out << "  " << group.name << ":\n";
        std::istringstream stream(check_output);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty()) {
                out << "    " << line << "\n";
            }
        }
        out << "\n";
    }

    if (!any_issues) {
        out << "No transitive dependency conflicts detected.\n";
    }

    return out.str();
}

} // namespace auditor
} // namespace adiboupk
