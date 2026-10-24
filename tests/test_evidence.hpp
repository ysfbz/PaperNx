#pragma once
#include <cstdlib>
#include <filesystem>

namespace test_evidence {
inline std::filesystem::path path(const std::filesystem::path &relative) {
    const char *root=std::getenv("PAPERNX_TEST_EVIDENCE_ROOT");
    return (root && *root ? std::filesystem::u8path(root) : std::filesystem::path("verification")) / relative;
}
}
