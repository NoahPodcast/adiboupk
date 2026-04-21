#include "doctest.h"
#include "adiboupk/platform.hpp"
#include "adiboupk/utils.hpp"

#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("platform::normalize_path") {
    auto cwd = fs::current_path();
    auto norm = adiboupk::platform::normalize_path(cwd);
    CHECK(!norm.empty());
    // Normalized path should be absolute
    CHECK(norm.is_absolute());
}

TEST_CASE("platform::is_under") {
    fs::path tmp_dir = fs::temp_directory_path() / "adiboupk_test_platform";
    fs::remove_all(tmp_dir);
    fs::create_directories(tmp_dir / "sub" / "deep");

    // Create files so paths can be resolved
    adiboupk::utils::write_file(tmp_dir / "sub" / "deep" / "file.txt", "");

    CHECK(adiboupk::platform::is_under(tmp_dir / "sub" / "deep" / "file.txt", tmp_dir));
    CHECK(adiboupk::platform::is_under(tmp_dir / "sub" / "deep", tmp_dir / "sub"));
    CHECK_FALSE(adiboupk::platform::is_under(tmp_dir, tmp_dir / "sub"));

    fs::remove_all(tmp_dir);
}

TEST_CASE("platform::python_command returns non-empty") {
    auto cmd = adiboupk::platform::python_command();
    CHECK(!cmd.empty());
}

TEST_CASE("platform::venv_python path structure") {
    fs::path venv = "/tmp/test_venv";
    auto python = adiboupk::platform::venv_python(venv);

#ifdef _WIN32
    CHECK(python == fs::path("/tmp/test_venv/Scripts/python.exe"));
#else
    CHECK(python == fs::path("/tmp/test_venv/bin/python"));
#endif
}

TEST_CASE("platform::venv_pip path structure") {
    fs::path venv = "/tmp/test_venv";
    auto pip = adiboupk::platform::venv_pip(venv);

#ifdef _WIN32
    CHECK(pip == fs::path("/tmp/test_venv/Scripts/pip.exe"));
#else
    CHECK(pip == fs::path("/tmp/test_venv/bin/pip"));
#endif
}

TEST_CASE("platform::get_env") {
    // PATH should always be set
    auto path = adiboupk::platform::get_env("PATH");
    CHECK(!path.empty());

    // Non-existent variable
    auto missing = adiboupk::platform::get_env("ADIBOUPK_NONEXISTENT_VAR_12345");
    CHECK(missing.empty());
}

TEST_CASE("platform::run_process with capture") {
    std::string output;
    int rc = adiboupk::platform::run_process("echo", {"hello"}, true, &output);
    CHECK(rc == 0);
    CHECK(output.find("hello") != std::string::npos);
}
