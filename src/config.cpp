#include "adiboupk/config.hpp"
#include "adiboupk/utils.hpp"
#include "nlohmann/json.hpp"

#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace adiboupk {
namespace config {

Config load(const fs::path& project_root) {
    Config cfg;
    cfg.project_root = project_root;
    cfg.venvs_dir = project_root / DEFAULT_VENVS_DIR;

    fs::path config_path = project_root / CONFIG_FILE;
    if (!fs::exists(config_path)) {
        return cfg;
    }

    std::string content = utils::read_file(config_path);
    if (content.empty()) return cfg;

    try {
        auto j = json::parse(content);

        if (j.contains("venvs_dir")) {
            cfg.venvs_dir = project_root / j["venvs_dir"].get<std::string>();
        }

        if (j.contains("python")) {
            cfg.python_path = j["python"].get<std::string>();
        }

        if (j.contains("groups") && j["groups"].is_array()) {
            for (const auto& g : j["groups"]) {
                Group group;
                group.name = g["name"].get<std::string>();
                group.directory = project_root / g["directory"].get<std::string>();

                if (g.contains("requirements")) {
                    group.requirements_path = project_root / g["requirements"].get<std::string>();
                } else {
                    group.requirements_path = group.directory / "requirements.txt";
                }

                // Compute hash of current requirements.txt
                if (fs::exists(group.requirements_path)) {
                    group.requirements_hash = utils::sha256_file(group.requirements_path);
                }

                cfg.groups.push_back(std::move(group));
            }
        }
    } catch (const json::exception& e) {
        std::cerr << "adiboupk: error parsing " << config_path.string()
                  << ": " << e.what() << std::endl;
    }

    return cfg;
}

bool save(const Config& cfg) {
    json j;
    j["venvs_dir"] = fs::relative(cfg.venvs_dir, cfg.project_root).string();

    if (!cfg.python_path.empty()) {
        j["python"] = cfg.python_path;
    }

    json groups_arr = json::array();
    for (const auto& g : cfg.groups) {
        json gj;
        gj["name"] = g.name;
        gj["directory"] = fs::relative(g.directory, cfg.project_root).string();

        auto default_req = g.directory / "requirements.txt";
        if (g.requirements_path != default_req) {
            gj["requirements"] = fs::relative(g.requirements_path, cfg.project_root).string();
        }

        groups_arr.push_back(gj);
    }
    j["groups"] = groups_arr;

    fs::path config_path = cfg.project_root / CONFIG_FILE;
    return utils::write_file(config_path, j.dump(2) + "\n");
}

LockFile load_lock(const fs::path& project_root) {
    LockFile lock;
    fs::path lock_path = project_root / LOCK_FILE;

    if (!fs::exists(lock_path)) return lock;

    std::string content = utils::read_file(lock_path);
    if (content.empty()) return lock;

    try {
        auto j = json::parse(content);
        if (j.contains("groups") && j["groups"].is_object()) {
            for (auto& [key, val] : j["groups"].items()) {
                LockEntry entry;
                entry.name = key;
                entry.requirements_hash = val.value("requirements_hash", "");
                entry.installed = val.value("installed", false);
                lock.entries[key] = entry;
            }
        }
    } catch (const json::exception& e) {
        std::cerr << "adiboupk: error parsing lock file: " << e.what() << std::endl;
    }

    return lock;
}

bool save_lock(const LockFile& lock, const fs::path& project_root) {
    json j;
    json groups_obj = json::object();

    for (const auto& [name, entry] : lock.entries) {
        json ej;
        ej["requirements_hash"] = entry.requirements_hash;
        ej["installed"] = entry.installed;
        groups_obj[name] = ej;
    }

    j["groups"] = groups_obj;

    fs::path lock_path = project_root / LOCK_FILE;
    return utils::write_file(lock_path, j.dump(2) + "\n");
}

bool needs_install(const Group& group, const LockFile& lock) {
    auto it = lock.entries.find(group.name);
    if (it == lock.entries.end()) return true;
    if (!it->second.installed) return true;
    return it->second.requirements_hash != group.requirements_hash;
}

Config init(const fs::path& project_root, const std::vector<Group>& groups) {
    Config cfg;
    cfg.project_root = project_root;
    cfg.venvs_dir = project_root / DEFAULT_VENVS_DIR;
    cfg.groups = groups;
    return cfg;
}

} // namespace config
} // namespace adiboupk
