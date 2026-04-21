#include "doctest.h"
#include "adiboupk/utils.hpp"

#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("utils::trim") {
    CHECK(adiboupk::utils::trim("  hello  ") == "hello");
    CHECK(adiboupk::utils::trim("\t\nhello\r\n") == "hello");
    CHECK(adiboupk::utils::trim("hello") == "hello");
    CHECK(adiboupk::utils::trim("") == "");
    CHECK(adiboupk::utils::trim("   ") == "");
}

TEST_CASE("utils::split") {
    auto parts = adiboupk::utils::split("a,b,c", ',');
    REQUIRE(parts.size() == 3);
    CHECK(parts[0] == "a");
    CHECK(parts[1] == "b");
    CHECK(parts[2] == "c");

    parts = adiboupk::utils::split("single", ',');
    REQUIRE(parts.size() == 1);
    CHECK(parts[0] == "single");

    parts = adiboupk::utils::split("", ',');
    // std::getline on empty string yields no tokens
    CHECK(parts.empty());
}

TEST_CASE("utils::sha256_string") {
    // Known SHA-256 of empty string
    auto hash = adiboupk::utils::sha256_string("");
    CHECK(hash == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    // Known SHA-256 of "hello"
    hash = adiboupk::utils::sha256_string("hello");
    CHECK(hash == "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
}

TEST_CASE("utils::extract_package_name") {
    CHECK(adiboupk::utils::extract_package_name("requests==2.28.0") == "requests");
    CHECK(adiboupk::utils::extract_package_name("protobuf>=4.0") == "protobuf");
    CHECK(adiboupk::utils::extract_package_name("Flask") == "flask");
    CHECK(adiboupk::utils::extract_package_name("my-package==1.0") == "my_package");
    CHECK(adiboupk::utils::extract_package_name("  requests == 2.28.0  ") == "requests");
    CHECK(adiboupk::utils::extract_package_name("# comment") == "");
    CHECK(adiboupk::utils::extract_package_name("-r other.txt") == "");
    CHECK(adiboupk::utils::extract_package_name("") == "");
    CHECK(adiboupk::utils::extract_package_name("package[extra]>=1.0") == "package");
}

TEST_CASE("utils::extract_version_spec") {
    CHECK(adiboupk::utils::extract_version_spec("requests==2.28.0") == "==2.28.0");
    CHECK(adiboupk::utils::extract_version_spec("protobuf>=4.0") == ">=4.0");
    CHECK(adiboupk::utils::extract_version_spec("Flask") == "");
    CHECK(adiboupk::utils::extract_version_spec("pkg~=1.0") == "~=1.0");
    CHECK(adiboupk::utils::extract_version_spec("pkg!=2.0") == "!=2.0");
    CHECK(adiboupk::utils::extract_version_spec("") == "");
}

TEST_CASE("utils::parse_requirements") {
    std::string content = R"(
# This is a comment
requests==2.28.0
protobuf>=4.0
Flask

-r other.txt
my-package==1.0
)";

    auto reqs = adiboupk::utils::parse_requirements(content);
    CHECK(reqs.size() == 4);
    CHECK(reqs.count("requests") == 1);
    CHECK(reqs.count("protobuf") == 1);
    CHECK(reqs.count("flask") == 1);
    CHECK(reqs.count("my_package") == 1);
    CHECK(reqs["requests"] == "requests==2.28.0");
}

TEST_CASE("utils::read_file and write_file") {
    fs::path tmp = fs::temp_directory_path() / "adiboupk_test_rw.txt";
    std::string content = "hello world\nline 2\n";

    CHECK(adiboupk::utils::write_file(tmp, content));
    CHECK(adiboupk::utils::read_file(tmp) == content);

    fs::remove(tmp);
}

TEST_CASE("utils::sha256_file") {
    fs::path tmp = fs::temp_directory_path() / "adiboupk_test_hash.txt";
    std::string content = "test content";
    adiboupk::utils::write_file(tmp, content);

    auto file_hash = adiboupk::utils::sha256_file(tmp);
    auto str_hash = adiboupk::utils::sha256_string(content);
    CHECK(file_hash == str_hash);
    CHECK(!file_hash.empty());

    fs::remove(tmp);
}
