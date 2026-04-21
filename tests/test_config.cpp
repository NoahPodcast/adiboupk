#include "doctest.h"
#include "adiboupk/config.hpp"
#include "adiboupk/utils.hpp"

#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("config::save and load round-trip") {
    fs::path tmp_dir = fs::temp_directory_path() / "adiboupk_test_config";
    fs::create_directories(tmp_dir);

    // Create a config
    adiboupk::Config cfg;
    cfg.project_root = tmp_dir;
    cfg.venvs_dir = tmp_dir / ".venvs";

    adiboupk::Group g1;
    g1.name = "GroupA";
    g1.directory = tmp_dir / "GroupA";
    g1.requirements_path = tmp_dir / "GroupA" / "requirements.txt";
    g1.requirements_hash = "abc123";
    cfg.groups.push_back(g1);

    // Save
    CHECK(adiboupk::config::save(cfg));

    // Verify file exists
    CHECK(fs::exists(tmp_dir / adiboupk::config::CONFIG_FILE));

    // Load back
    auto loaded = adiboupk::config::load(tmp_dir);
    REQUIRE(loaded.groups.size() == 1);
    CHECK(loaded.groups[0].name == "GroupA");
    CHECK(loaded.groups[0].directory == tmp_dir / "GroupA");

    // Cleanup
    fs::remove_all(tmp_dir);
}

TEST_CASE("config::load returns default for missing file") {
    fs::path tmp_dir = fs::temp_directory_path() / "adiboupk_test_noconfig";
    fs::create_directories(tmp_dir);

    auto cfg = adiboupk::config::load(tmp_dir);
    CHECK(cfg.groups.empty());
    CHECK(cfg.venvs_dir == tmp_dir / ".venvs");

    fs::remove_all(tmp_dir);
}

TEST_CASE("lock file round-trip") {
    fs::path tmp_dir = fs::temp_directory_path() / "adiboupk_test_lock";
    fs::create_directories(tmp_dir);

    adiboupk::LockFile lock;
    adiboupk::LockEntry entry;
    entry.name = "GroupA";
    entry.requirements_hash = "deadbeef";
    entry.installed = true;
    lock.entries["GroupA"] = entry;

    CHECK(adiboupk::config::save_lock(lock, tmp_dir));

    auto loaded = adiboupk::config::load_lock(tmp_dir);
    REQUIRE(loaded.entries.count("GroupA") == 1);
    CHECK(loaded.entries["GroupA"].requirements_hash == "deadbeef");
    CHECK(loaded.entries["GroupA"].installed == true);

    fs::remove_all(tmp_dir);
}

TEST_CASE("config::needs_install") {
    adiboupk::Group group;
    group.name = "Test";
    group.requirements_hash = "hash_v2";

    SUBCASE("no lock entry -> needs install") {
        adiboupk::LockFile lock;
        CHECK(adiboupk::config::needs_install(group, lock));
    }

    SUBCASE("hash mismatch -> needs install") {
        adiboupk::LockFile lock;
        adiboupk::LockEntry entry;
        entry.name = "Test";
        entry.requirements_hash = "hash_v1";
        entry.installed = true;
        lock.entries["Test"] = entry;
        CHECK(adiboupk::config::needs_install(group, lock));
    }

    SUBCASE("hash match -> up to date") {
        adiboupk::LockFile lock;
        adiboupk::LockEntry entry;
        entry.name = "Test";
        entry.requirements_hash = "hash_v2";
        entry.installed = true;
        lock.entries["Test"] = entry;
        CHECK_FALSE(adiboupk::config::needs_install(group, lock));
    }

    SUBCASE("not installed -> needs install") {
        adiboupk::LockFile lock;
        adiboupk::LockEntry entry;
        entry.name = "Test";
        entry.requirements_hash = "hash_v2";
        entry.installed = false;
        lock.entries["Test"] = entry;
        CHECK(adiboupk::config::needs_install(group, lock));
    }
}
