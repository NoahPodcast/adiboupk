#include "adiboupk/utils.hpp"
#include "picosha2.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace adiboupk {
namespace utils {

std::string sha256_file(const fs::path& filepath) {
    std::ifstream f(filepath, std::ios::binary);
    if (!f.is_open()) return {};

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    return sha256_string(content);
}

std::string sha256_string(const std::string& input) {
    std::string hash_hex;
    picosha2::hash256_hex_string(input, hash_hex);
    return hash_hex;
}

std::string read_file(const fs::path& filepath) {
    std::ifstream f(filepath, std::ios::binary);
    if (!f.is_open()) return {};

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    return content;
}

bool write_file(const fs::path& filepath, const std::string& content) {
    // Ensure parent directory exists
    std::error_code ec;
    if (filepath.has_parent_path()) {
        fs::create_directories(filepath.parent_path(), ec);
    }

    std::ofstream f(filepath, std::ios::binary);
    if (!f.is_open()) return false;

    f.write(content.data(), content.size());
    return f.good();
}

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::istringstream stream(s);
    std::string token;
    while (std::getline(stream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string extract_package_name(const std::string& requirement_line) {
    std::string line = trim(requirement_line);
    if (line.empty() || line[0] == '#' || line[0] == '-') return {};

    // Find first version specifier character
    auto pos = line.find_first_of("=<>!~[;@");
    std::string name;
    if (pos == std::string::npos) {
        name = line;
    } else {
        name = line.substr(0, pos);
    }

    name = trim(name);
    // Normalize: lowercase, replace - with _
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::replace(name.begin(), name.end(), '-', '_');
    return name;
}

std::string extract_version_spec(const std::string& requirement_line) {
    std::string line = trim(requirement_line);
    if (line.empty() || line[0] == '#' || line[0] == '-') return {};

    auto pos = line.find_first_of("=<>!~");
    if (pos == std::string::npos) return {};

    // Find end of version spec (before ; or [ or @)
    auto end_pos = line.find_first_of(";[@", pos);
    if (end_pos == std::string::npos) {
        return trim(line.substr(pos));
    }
    return trim(line.substr(pos, end_pos - pos));
}

std::map<std::string, std::string> parse_requirements(const std::string& content) {
    std::map<std::string, std::string> result;
    auto lines = split(content, '\n');

    for (const auto& raw_line : lines) {
        std::string line = trim(raw_line);

        // Skip empty lines, comments, and options
        if (line.empty() || line[0] == '#' || line[0] == '-') continue;

        std::string name = extract_package_name(line);
        if (!name.empty()) {
            result[name] = line;
        }
    }

    return result;
}

} // namespace utils
} // namespace adiboupk
