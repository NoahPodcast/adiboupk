#include "doctest.h"
#include "adiboupk/runner.hpp"
#include "adiboupk/utils.hpp"
#include "adiboupk/venv.hpp"

#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("runner::resolve_python returns empty for unknown script") {
    fs::path tmp_dir = fs::temp_directory_path() / "adiboupk_test_runner";
    fs::remove_all(tmp_dir);
    fs::create_directories(tmp_dir / "Enrichments");

    adiboupk::Config cfg;
    cfg.project_root = tmp_dir;
    cfg.venvs_dir = tmp_dir / ".venvs";

    adiboupk::Group g;
    g.name = "Enrichments";
    g.directory = tmp_dir / "Enrichments";
    cfg.groups.push_back(g);

    // Script not under any group
    auto python = adiboupk::runner::resolve_python(
        tmp_dir / "other" / "script.py", cfg);
    CHECK(python.empty());

    fs::remove_all(tmp_dir);
}

TEST_CASE("runner::resolve_python finds correct venv") {
    fs::path tmp_dir = fs::temp_directory_path() / "adiboupk_test_runner_resolve";
    fs::remove_all(tmp_dir);
    fs::create_directories(tmp_dir / "Enrichments");

    // Create a fake venv with a python binary
    auto venv_dir = tmp_dir / ".venvs" / "Enrichments";
#ifdef _WIN32
    auto python_path = venv_dir / "Scripts" / "python.exe";
#else
    auto python_path = venv_dir / "bin" / "python";
#endif
    fs::create_directories(python_path.parent_path());
    adiboupk::utils::write_file(python_path, "fake python");

    // Create dummy script
    adiboupk::utils::write_file(tmp_dir / "Enrichments" / "test.py", "print('hi')");

    adiboupk::Config cfg;
    cfg.project_root = tmp_dir;
    cfg.venvs_dir = tmp_dir / ".venvs";

    adiboupk::Group g;
    g.name = "Enrichments";
    g.directory = tmp_dir / "Enrichments";
    cfg.groups.push_back(g);

    auto resolved = adiboupk::runner::resolve_python(
        tmp_dir / "Enrichments" / "test.py", cfg);
    CHECK(!resolved.empty());
    CHECK(resolved == python_path);

    fs::remove_all(tmp_dir);
}
