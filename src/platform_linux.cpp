#ifndef _WIN32

#include "adiboupk/platform.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

namespace adiboupk {
namespace platform {

std::string python_command() {
    // Prefer python3, fall back to python
    if (std::system("python3 --version > /dev/null 2>&1") == 0) {
        return "python3";
    }
    return "python";
}

fs::path venv_python(const fs::path& venv_dir) {
    return venv_dir / "bin" / "python";
}

fs::path venv_pip(const fs::path& venv_dir) {
    return venv_dir / "bin" / "pip";
}

int run_process(const std::string& executable,
                const std::vector<std::string>& args,
                bool capture_output,
                std::string* stdout_buf) {
    int pipefd[2] = {-1, -1};
    if (capture_output && stdout_buf) {
        if (pipe(pipefd) == -1) {
            return -1;
        }
    }

    pid_t pid = fork();
    if (pid == -1) {
        return -1;
    }

    if (pid == 0) {
        // Child process
        if (capture_output && stdout_buf) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
        }

        // Build argv
        std::vector<const char*> argv;
        argv.push_back(executable.c_str());
        for (const auto& arg : args) {
            argv.push_back(arg.c_str());
        }
        argv.push_back(nullptr);

        execvp(executable.c_str(), const_cast<char* const*>(argv.data()));
        // If execvp returns, it failed
        _exit(127);
    }

    // Parent process
    if (capture_output && stdout_buf) {
        close(pipefd[1]);

        char buffer[4096];
        ssize_t n;
        while ((n = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            stdout_buf->append(buffer, n);
        }
        close(pipefd[0]);
    }

    int status = 0;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

[[noreturn]] void exec_replace(const std::string& executable,
                               const std::vector<std::string>& args) {
    std::vector<const char*> argv;
    argv.push_back(executable.c_str());
    for (const auto& arg : args) {
        argv.push_back(arg.c_str());
    }
    argv.push_back(nullptr);

    execvp(executable.c_str(), const_cast<char* const*>(argv.data()));

    // execvp only returns on error
    std::cerr << "adiboupk: failed to exec " << executable << ": "
              << strerror(errno) << std::endl;
    _exit(127);
}

bool remove_directory(const fs::path& dir) {
    std::error_code ec;
    fs::remove_all(dir, ec);
    return !ec;
}

std::string get_env(const std::string& name) {
    const char* val = std::getenv(name.c_str());
    return val ? std::string(val) : std::string();
}

} // namespace platform
} // namespace adiboupk

#endif // !_WIN32
