#ifdef _WIN32

#include "adiboupk/platform.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <windows.h>

namespace adiboupk {
namespace platform {

std::string python_command() {
    return "python";
}

fs::path venv_python(const fs::path& venv_dir) {
    return venv_dir / "Scripts" / "python.exe";
}

fs::path venv_pip(const fs::path& venv_dir) {
    return venv_dir / "Scripts" / "pip.exe";
}

// Helper: convert UTF-8 string to wide string
static std::wstring utf8_to_wide(const std::string& str) {
    if (str.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], size);
    return result;
}

// Helper: convert wide string to UTF-8
static std::string wide_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(),
                                    nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(),
                        &result[0], size, nullptr, nullptr);
    return result;
}

// Build a command line string from executable + args, properly quoting
static std::wstring build_command_line(const std::string& executable,
                                        const std::vector<std::string>& args) {
    std::wstring cmdline;

    auto quote = [](const std::string& s) -> std::wstring {
        std::wstring ws = utf8_to_wide(s);
        // If contains spaces or special chars, quote it
        if (ws.find(L' ') != std::wstring::npos ||
            ws.find(L'\t') != std::wstring::npos ||
            ws.find(L'"') != std::wstring::npos) {
            std::wstring quoted = L"\"";
            for (auto c : ws) {
                if (c == L'"') quoted += L"\\\"";
                else quoted += c;
            }
            quoted += L"\"";
            return quoted;
        }
        return ws;
    };

    cmdline = quote(executable);
    for (const auto& arg : args) {
        cmdline += L" ";
        cmdline += quote(arg);
    }
    return cmdline;
}

int run_process(const std::string& executable,
                const std::vector<std::string>& args,
                bool capture_output,
                std::string* stdout_buf) {
    HANDLE hReadPipe = NULL, hWritePipe = NULL;
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    if (capture_output && stdout_buf) {
        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
            return -1;
        }
        SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
    }

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    if (capture_output && stdout_buf) {
        si.hStdOutput = hWritePipe;
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.dwFlags |= STARTF_USESTDHANDLES;
    }

    PROCESS_INFORMATION pi = {};
    std::wstring cmdline = build_command_line(executable, args);

    BOOL ok = CreateProcessW(
        nullptr,
        &cmdline[0],
        nullptr, nullptr,
        capture_output ? TRUE : FALSE,
        0,
        nullptr, nullptr,
        &si, &pi
    );

    if (!ok) {
        if (hReadPipe) CloseHandle(hReadPipe);
        if (hWritePipe) CloseHandle(hWritePipe);
        return -1;
    }

    if (capture_output && stdout_buf) {
        CloseHandle(hWritePipe);
        char buffer[4096];
        DWORD bytesRead;
        while (ReadFile(hReadPipe, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
            stdout_buf->append(buffer, bytesRead);
        }
        CloseHandle(hReadPipe);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return static_cast<int>(exitCode);
}

[[noreturn]] void exec_replace(const std::string& executable,
                               const std::vector<std::string>& args) {
    // On Windows, we can't truly replace the process. We create a child and exit.
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    std::wstring cmdline = build_command_line(executable, args);

    BOOL ok = CreateProcessW(
        nullptr,
        &cmdline[0],
        nullptr, nullptr,
        TRUE,  // Inherit handles so child gets stdin/stdout/stderr
        0,
        nullptr, nullptr,
        &si, &pi
    );

    if (!ok) {
        std::cerr << "adiboupk: failed to exec " << executable << std::endl;
        ExitProcess(127);
    }

    // Wait for child to finish, then exit with its exit code
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    ExitProcess(exitCode);
}

bool remove_directory(const fs::path& dir) {
    // On Windows, files may be read-only. We need to clear that flag before removing.
    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec)) {
        if (entry.is_regular_file()) {
            fs::permissions(entry.path(), fs::perms::owner_write, fs::perm_options::add, ec);
        }
    }
    fs::remove_all(dir, ec);
    return !ec;
}

std::string get_env(const std::string& name) {
    std::wstring wname = utf8_to_wide(name);
    DWORD size = GetEnvironmentVariableW(wname.c_str(), nullptr, 0);
    if (size == 0) return {};
    std::wstring wval(size, 0);
    GetEnvironmentVariableW(wname.c_str(), &wval[0], size);
    wval.resize(size - 1); // Remove trailing null
    return wide_to_utf8(wval);
}

} // namespace platform
} // namespace adiboupk

#endif // _WIN32
