#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace adiboupk {
namespace utils {

namespace fs = std::filesystem;

// Compute SHA-256 hash of a file's contents. Returns hex string.
std::string sha256_file(const fs::path& filepath);

// Compute SHA-256 hash of a string. Returns hex string.
std::string sha256_string(const std::string& input);

// Read entire file into a string. Returns empty string on failure.
std::string read_file(const fs::path& filepath);

// Write string to file. Returns true on success.
bool write_file(const fs::path& filepath, const std::string& content);

// Trim whitespace from both ends
std::string trim(const std::string& s);

// Split string by delimiter
std::vector<std::string> split(const std::string& s, char delimiter);

// Parse a requirements.txt file into package_name -> version_spec pairs.
// Handles lines like "requests==2.28.0", "protobuf>=4.0", "flask", comments, -r includes.
// Returns map of lowercase package name -> original line (for pip install).
std::map<std::string, std::string> parse_requirements(const std::string& content);

// Extract package name from a requirement line (e.g., "requests==2.28.0" -> "requests")
std::string extract_package_name(const std::string& requirement_line);

// Extract version spec from a requirement line (e.g., "requests==2.28.0" -> "==2.28.0")
std::string extract_version_spec(const std::string& requirement_line);

} // namespace utils
} // namespace adiboupk
