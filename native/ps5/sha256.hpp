#pragma once

#include <cstddef>
#include <cstdint>

namespace nests {

class Sha256 {
public:
    Sha256() noexcept;
    void update(const void *data, std::size_t length) noexcept;
    void finish(std::uint8_t digest[32]) noexcept;
private:
    void transform(const std::uint8_t block[64]) noexcept;
    std::uint32_t state_[8]{};
    std::uint64_t total_bytes_ = 0;
    std::uint8_t buffer_[64]{};
    std::size_t buffered_ = 0;
    bool finished_ = false;
};

void sha256_hex(const std::uint8_t digest[32], char output[65]) noexcept;
bool sha256_matches_hex(const std::uint8_t digest[32], const char *expected) noexcept;

} // namespace nests
