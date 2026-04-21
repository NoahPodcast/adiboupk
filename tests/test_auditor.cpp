#include "doctest.h"
#include "adiboupk/auditor.hpp"
#include "adiboupk/utils.hpp"

#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("auditor::audit detects conflicts") {
    fs::path tmp_dir = fs::temp_directory_path() / "adiboupk_test_auditor";
    fs::remove_all(tmp_dir);
    fs::create_directories(tmp_dir / "GroupA");
    fs::create_directories(tmp_dir / "GroupB");

    adiboupk::utils::write_file(
        tmp_dir / "GroupA" / "requirements.txt",
        "requests==2.28.0\nprotobuf==4.0\nflask==2.0\n"
    );
    adiboupk::utils::write_file(
        tmp_dir / "GroupB" / "requirements.txt",
        "requests==2.32.5\nprotobuf==6.33\nflask==2.0\n"
    );

    std::vector<adiboupk::Group> groups;
    adiboupk::Group g1;
    g1.name = "GroupA";
    g1.directory = tmp_dir / "GroupA";
    g1.requirements_path = tmp_dir / "GroupA" / "requirements.txt";
    groups.push_back(g1);

    adiboupk::Group g2;
    g2.name = "GroupB";
    g2.directory = tmp_dir / "GroupB";
    g2.requirements_path = tmp_dir / "GroupB" / "requirements.txt";
    groups.push_back(g2);

    auto conflicts = adiboupk::auditor::audit(groups);

    // requests and protobuf have different versions, flask is the same
    CHECK(conflicts.size() == 2);

    // Verify conflict details
    bool found_requests = false;
    bool found_protobuf = false;
    for (const auto& c : conflicts) {
        if (c.package_name == "requests") {
            found_requests = true;
            REQUIRE(c.versions.size() == 2);
        }
        if (c.package_name == "protobuf") {
            found_protobuf = true;
            REQUIRE(c.versions.size() == 2);
        }
    }
    CHECK(found_requests);
    CHECK(found_protobuf);

    fs::remove_all(tmp_dir);
}

TEST_CASE("auditor::audit no conflicts when versions match") {
    fs::path tmp_dir = fs::temp_directory_path() / "adiboupk_test_auditor_ok";
    fs::remove_all(tmp_dir);
    fs::create_directories(tmp_dir / "GroupA");
    fs::create_directories(tmp_dir / "GroupB");

    adiboupk::utils::write_file(
        tmp_dir / "GroupA" / "requirements.txt",
        "requests==2.28.0\n"
    );
    adiboupk::utils::write_file(
        tmp_dir / "GroupB" / "requirements.txt",
        "requests==2.28.0\n"
    );

    std::vector<adiboupk::Group> groups;
    adiboupk::Group g1;
    g1.name = "GroupA";
    g1.requirements_path = tmp_dir / "GroupA" / "requirements.txt";
    groups.push_back(g1);

    adiboupk::Group g2;
    g2.name = "GroupB";
    g2.requirements_path = tmp_dir / "GroupB" / "requirements.txt";
    groups.push_back(g2);

    auto conflicts = adiboupk::auditor::audit(groups);
    CHECK(conflicts.empty());

    fs::remove_all(tmp_dir);
}

TEST_CASE("auditor::format_conflicts") {
    SUBCASE("no conflicts") {
        std::vector<adiboupk::auditor::Conflict> empty;
        auto msg = adiboupk::auditor::format_conflicts(empty);
        CHECK(msg.find("No conflicts") != std::string::npos);
    }

    SUBCASE("with conflicts") {
        adiboupk::auditor::Conflict c;
        c.package_name = "requests";
        c.versions.push_back({"GroupA", "requests==2.28.0", "==2.28.0"});
        c.versions.push_back({"GroupB", "requests==2.32.5", "==2.32.5"});

        auto msg = adiboupk::auditor::format_conflicts({c});
        CHECK(msg.find("1 conflict") != std::string::npos);
        CHECK(msg.find("requests") != std::string::npos);
        CHECK(msg.find("GroupA") != std::string::npos);
        CHECK(msg.find("GroupB") != std::string::npos);
    }
}
