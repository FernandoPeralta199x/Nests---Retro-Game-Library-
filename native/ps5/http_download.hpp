#pragma once

#include "storage_manager.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace nests {

enum class DownloadState : int {
    idle = 0,
    preparing,
    connecting,
    receiving,
    verifying,
    complete,
    cancelled,
    failed
};

struct DownloadSpec {
    char url[2048]{};
    char trusted_host[128]{};
    char console_slug[64]{};
    char file_name[128]{};
    char sha256[65]{};
    std::uint64_t expected_size = 0;
    bool redistribution_allowed = false;
    bool license_evidence_verified = false;
    bool catalog_signature_verified = false;
};

struct DownloadProgress {
    std::atomic<DownloadState> state{DownloadState::idle};
    std::atomic<std::uint64_t> received_bytes{0};
    std::atomic<std::uint64_t> expected_bytes{0};
};

bool validate_download_spec(const DownloadSpec &spec,
                            char *error, std::size_t error_capacity) noexcept;

// Runs on a worker thread. This first native transport intentionally accepts
// direct HTTPS URLs only (HTTP 200, no redirects) so the final host remains
// exactly the host approved by the signed catalog.
bool download_https_to_storage(const DownloadSpec &spec,
                               const StorageVolume &volume,
                               const StorageSettings &settings,
                               const std::atomic<bool> &cancel_requested,
                               DownloadProgress &progress,
                               char *error, std::size_t error_capacity) noexcept;

} // namespace nests
