#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {
constexpr unsigned char payload[] = "NESTS test payload\n";
std::size_t cursor = 0;
int response_status = 200;
}

extern "C" {
void nests_test_http_reset(int status) {
    cursor = 0;
    response_status = status;
}

int sceNetPoolCreate(const char *, int, int) { return 1; }
int sceNetPoolDestroy(int) { return 0; }
int sceSslInit(std::size_t) { return 2; }
int sceSslTerm(int) { return 0; }
int sceHttpInit(int, int, std::size_t) { return 3; }
int sceHttpTerm(int) { return 0; }
int sceHttpCreateTemplate(int, const char *, int, int) { return 4; }
int sceHttpDeleteTemplate(int) { return 0; }
int sceHttpCreateConnectionWithURL(int, const char *, int) { return 5; }
int sceHttpDeleteConnection(int) { return 0; }
int sceHttpCreateRequestWithURL(int, int, const char *, std::uint64_t) { return 6; }
int sceHttpDeleteRequest(int) { return 0; }
int sceHttpSetAutoRedirect(int, int enabled) { return enabled == 0 ? 0 : -1; }
int sceHttpSetResolveTimeOut(int, std::uint32_t) { return 0; }
int sceHttpSetConnectTimeOut(int, std::uint32_t) { return 0; }
int sceHttpSetSendTimeOut(int, std::uint32_t) { return 0; }
int sceHttpSetRecvTimeOut(int, std::uint32_t) { return 0; }
int sceHttpsEnableOption(int, std::uint32_t flags) {
    constexpr std::uint32_t required = 0x01U | 0x04U | 0x08U | 0x10U | 0x20U | 0x80U;
    return (flags & required) == required ? 0 : -1;
}
int sceHttpSendRequest(int, const void *, std::size_t) { cursor = 0; return 0; }
int sceHttpGetStatusCode(int, int *status) {
    if (status == nullptr) return -1;
    *status = response_status;
    return 0;
}
int sceHttpReadData(int, void *buffer, std::size_t capacity) {
    if (buffer == nullptr) return -1;
    const std::size_t length = sizeof(payload) - 1;
    if (cursor >= length) return 0;
    const std::size_t count = std::min(capacity, length - cursor);
    std::memcpy(buffer, payload + cursor, count);
    cursor += count;
    return static_cast<int>(count);
}
}
