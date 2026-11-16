#pragma once

#include <cstddef>
#include <cstdint>

namespace nests {

constexpr std::size_t kMaxStorageVolumes = 9;

struct StorageVolume {
    char id[16]{};
    char label[32]{};
    char mount_root[160]{};
    char roms_path[256]{};
    std::uint64_t free_bytes = 0;
    bool removable = false;
};

struct StorageSettings {
    char volume_id[16] = "internal";
    bool separate_by_console = true;
};

bool scan_default_storage(StorageVolume *volumes, std::size_t capacity,
                          std::size_t *count) noexcept;

// Exposed for host tests and for platforms whose mount names differ.
bool scan_storage_roots(const char *internal_root,
                        const char *const *usb_roots, std::size_t usb_count,
                        StorageVolume *volumes, std::size_t capacity,
                        std::size_t *count) noexcept;

bool load_storage_settings(const char *settings_path,
                           StorageSettings *settings) noexcept;
bool save_storage_settings(const char *settings_path,
                           const StorageSettings &settings) noexcept;

bool find_storage_volume(const StorageVolume *volumes, std::size_t count,
                         const char *volume_id, StorageVolume *out) noexcept;

bool build_download_paths(const StorageVolume &volume,
                          const StorageSettings &settings,
                          const char *console_slug,
                          const char *file_name,
                          char *final_path, std::size_t final_capacity,
                          char *partial_path, std::size_t partial_capacity) noexcept;

bool is_safe_path_segment(const char *value) noexcept;
bool is_safe_download_filename(const char *value) noexcept;

} // namespace nests
