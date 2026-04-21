#include "doctest.h"
#include "adiboupk/discovery.hpp"
#include "adiboupk/utils.hpp"

#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("discovery::scan finds groups with requirements.txt") {
    fs::path tmp_dir = fs::temp_directory_path() / "adiboupk_test_discovery";
    fs::remove_all(tmp_dir);
    fs::create_directories(tmp_dir);

    // Create two module directories with requirements.txt
    fs::create_directories(tmp_dir / "ModuleA");
    adiboupk::utils::write_file(tmp_dir / "ModuleA" / "requirements.txt", "requests==2.28.0\n");

    fs::create_directories(tmp_dir / "ModuleB");
    adiboupk::utils::write_file(tmp_dir / "ModuleB" / "requirements.txt", "flask==2.0\n");

    // Create a directory without requirements.txt (should be ignored)
    fs::create_directories(tmp_dir / "NoReqs");

    // Create a hidden directory (should be ignored)
    fs::create_directories(tmp_dir / ".hidden");
    adiboupk::utils::write_file(tmp_dir / ".hidden" / "requirements.txt", "bad\n");

    auto groups = adiboupk::discovery::scan(tmp_dir);
    REQUIRE(groups.size() == 2);
    CHECK(groups[0].name == "ModuleA");
    CHECK(groups[1].name == "ModuleB");
    CHECK(!groups[0].requirements_hash.empty());
    CHECK(!groups[1].requirements_hash.empty());

    fs::remove_all(tmp_dir);
}

TEST_CASE("discovery::scan ignores special directories") {
    fs::path tmp_dir = fs::temp_directory_path() / "adiboupk_test_discovery_ignore";
    fs::remove_all(tmp_dir);
    fs::create_directories(tmp_dir);

    // These should all be ignored
    for (const auto& name : {"node_modules", "__pycache__", ".git", "build"}) {
        fs::create_directories(tmp_dir / name);
        adiboupk::utils::write_file(fs::path(tmp_dir / name / "requirements.txt"), "pkg==1.0\n");
    }

    auto groups = adiboupk::discovery::scan(tmp_dir);
    CHECK(groups.empty());

    fs::remove_all(tmp_dir);
}

TEST_CASE("discovery::find_group_for_script") {
    fs::path tmp_dir = fs::temp_directory_path() / "adiboupk_test_find_group";
    fs::remove_all(tmp_dir);
    fs::create_directories(tmp_dir / "Enrichments");
    fs::create_directories(tmp_dir / "Responses");

    // Create dummy files so paths resolve
    adiboupk::utils::write_file(tmp_dir / "Enrichments" / "script.py", "");
    adiboupk::utils::write_file(tmp_dir / "Responses" / "script.py", "");

    std::vector<adiboupk::Group> groups;
    adiboupk::Group g1;
    g1.name = "Enrichments";
    g1.directory = tmp_dir / "Enrichments";
    groups.push_back(g1);

    adiboupk::Group g2;
    g2.name = "Responses";
    g2.directory = tmp_dir / "Responses";
    groups.push_back(g2);

    auto* found = adiboupk::discovery::find_group_for_script(
        groups, tmp_dir / "Enrichments" / "script.py");
    REQUIRE(found != nullptr);
    CHECK(found->name == "Enrichments");

    found = adiboupk::discovery::find_group_for_script(
        groups, tmp_dir / "Responses" / "script.py");
    REQUIRE(found != nullptr);
    CHECK(found->name == "Responses");

    found = adiboupk::discovery::find_group_for_script(
        groups, tmp_dir / "other" / "script.py");
    CHECK(found == nullptr);

    fs::remove_all(tmp_dir);
}
