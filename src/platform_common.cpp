#include "adiboupk/platform.hpp"
#include <algorithm>

namespace adiboupk {
namespace platform {

fs::path normalize_path(const fs::path& p) {
    std::error_code ec;
    auto canonical = fs::weakly_canonical(p, ec);
    if (ec) {
        // Fallback: just normalize lexically
        return p.lexically_normal();
    }
    return canonical;
}

bool is_under(const fs::path& path, const fs::path& base) {
    auto norm_path = normalize_path(path);
    auto norm_base = normalize_path(base);

    auto rel = norm_path.lexically_relative(norm_base);
    if (rel.empty()) return false;

    // If the relative path starts with "..", it's not under base
    auto first = *rel.begin();
    return first != "..";
}

} // namespace platform
} // namespace adiboupk
