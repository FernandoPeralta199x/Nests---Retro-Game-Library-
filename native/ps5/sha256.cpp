#include "sha256.hpp"

#include <cstring>

namespace nests {
namespace {

constexpr std::uint32_t k[64] = {
    0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
    0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
    0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
    0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
    0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
    0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
    0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
    0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U
};

constexpr std::uint32_t rotate_right(std::uint32_t value, unsigned amount) noexcept {
    return (value >> amount) | (value << (32U - amount));
}
constexpr std::uint32_t load_be32(const std::uint8_t *p) noexcept {
    return (static_cast<std::uint32_t>(p[0]) << 24) |
           (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) |
           static_cast<std::uint32_t>(p[3]);
}
constexpr void store_be32(std::uint8_t *p, std::uint32_t value) noexcept {
    p[0] = static_cast<std::uint8_t>(value >> 24);
    p[1] = static_cast<std::uint8_t>(value >> 16);
    p[2] = static_cast<std::uint8_t>(value >> 8);
    p[3] = static_cast<std::uint8_t>(value);
}

} // namespace

Sha256::Sha256() noexcept
    : state_{0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,
             0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U} {}

void Sha256::transform(const std::uint8_t block[64]) noexcept {
    std::uint32_t words[64]{};
    for (std::size_t i = 0; i < 16; ++i) words[i] = load_be32(block + i * 4);
    for (std::size_t i = 16; i < 64; ++i) {
        const std::uint32_t s0 = rotate_right(words[i - 15], 7) ^
                                 rotate_right(words[i - 15], 18) ^ (words[i - 15] >> 3);
        const std::uint32_t s1 = rotate_right(words[i - 2], 17) ^
                                 rotate_right(words[i - 2], 19) ^ (words[i - 2] >> 10);
        words[i] = words[i - 16] + s0 + words[i - 7] + s1;
    }

    std::uint32_t a=state_[0], b=state_[1], c=state_[2], d=state_[3];
    std::uint32_t e=state_[4], f=state_[5], g=state_[6], h=state_[7];
    for (std::size_t i = 0; i < 64; ++i) {
        const std::uint32_t sum1 = rotate_right(e,6) ^ rotate_right(e,11) ^ rotate_right(e,25);
        const std::uint32_t choice = (e & f) ^ (~e & g);
        const std::uint32_t temp1 = h + sum1 + choice + k[i] + words[i];
        const std::uint32_t sum0 = rotate_right(a,2) ^ rotate_right(a,13) ^ rotate_right(a,22);
        const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temp2 = sum0 + majority;
        h=g; g=f; f=e; e=d+temp1; d=c; c=b; b=a; a=temp1+temp2;
    }
    state_[0]+=a; state_[1]+=b; state_[2]+=c; state_[3]+=d;
    state_[4]+=e; state_[5]+=f; state_[6]+=g; state_[7]+=h;
}

void Sha256::update(const void *data, std::size_t length) noexcept {
    if (finished_ || data == nullptr || length == 0) return;
    const auto *bytes = static_cast<const std::uint8_t *>(data);
    total_bytes_ += static_cast<std::uint64_t>(length);
    while (length > 0) {
        const std::size_t available = sizeof(buffer_) - buffered_;
        const std::size_t take = length < available ? length : available;
        std::memcpy(buffer_ + buffered_, bytes, take);
        buffered_ += take;
        bytes += take;
        length -= take;
        if (buffered_ == sizeof(buffer_)) {
            transform(buffer_);
            buffered_ = 0;
        }
    }
}

void Sha256::finish(std::uint8_t digest[32]) noexcept {
    if (digest == nullptr) return;
    if (!finished_) {
        const std::uint64_t bit_length = total_bytes_ * 8U;
        buffer_[buffered_++] = 0x80U;
        if (buffered_ > 56) {
            while (buffered_ < 64) buffer_[buffered_++] = 0;
            transform(buffer_);
            buffered_ = 0;
        }
        while (buffered_ < 56) buffer_[buffered_++] = 0;
        for (int shift = 56; shift >= 0; shift -= 8)
            buffer_[buffered_++] = static_cast<std::uint8_t>(bit_length >> shift);
        transform(buffer_);
        buffered_ = 0;
        finished_ = true;
    }
    for (std::size_t i = 0; i < 8; ++i) store_be32(digest + i * 4, state_[i]);
}

void sha256_hex(const std::uint8_t digest[32], char output[65]) noexcept {
    static constexpr char hex[] = "0123456789abcdef";
    if (digest == nullptr || output == nullptr) return;
    for (std::size_t i = 0; i < 32; ++i) {
        output[i * 2] = hex[digest[i] >> 4];
        output[i * 2 + 1] = hex[digest[i] & 0x0fU];
    }
    output[64] = '\0';
}

bool sha256_matches_hex(const std::uint8_t digest[32], const char *expected) noexcept {
    if (digest == nullptr || expected == nullptr || std::strlen(expected) != 64) return false;
    char actual[65]{};
    sha256_hex(digest, actual);
    unsigned difference = 0;
    for (std::size_t i = 0; i < 64; ++i) {
        char c = expected[i];
        if (c >= 'A' && c <= 'F') c = static_cast<char>(c - 'A' + 'a');
        difference |= static_cast<unsigned char>(actual[i]) ^ static_cast<unsigned char>(c);
    }
    return difference == 0;
}

} // namespace nests
