#pragma once

#include "adiboupk/config.hpp"
#include <string>
#include <vector>

namespace adiboupk {
namespace auditor {

// A conflict: same package required by multiple groups with different version specs
struct Conflict {
    std::string package_name;
    struct GroupVersion {
        std::string group_name;
        std::string requirement_line;  // Full line from requirements.txt
        std::string version_spec;      // e.g., "==2.28.0"
    };
    std::vector<GroupVersion> versions;
};

// Audit all groups for cross-group dependency conflicts.
// Returns list of conflicts found.
std::vector<Conflict> audit(const std::vector<Group>& groups);

// Format conflicts for display
std::string format_conflicts(const std::vector<Conflict>& conflicts);

// Check if there are any conflicts (convenience)
bool has_conflicts(const std::vector<Group>& groups);

} // namespace auditor
} // namespace adiboupk
