#include "storage_manager.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

namespace nests {
namespace {

bool copy_text(char *out, std::size_t capacity, const char *value) noexcept {
    if (out == nullptr || capacity == 0 || value == nullptr) return false;
    const std::size_t length = std::strlen(value);
    if (length >= capacity) return false;
    std::memcpy(out, value, length + 1);
    return true;
}

bool ensure_directory(const char *path) noexcept {
    if (::mkdir(path, 0700) == 0) return true;
    if (errno != EEXIST) return false;
    struct stat info {};
    return ::lstat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

bool ensure_child_directory(const char *parent, const char *child,
                            char *out, std::size_t capacity) noexcept {
    const int written = std::snprintf(out, capacity, "%s/%s", parent, child);
    if (written <= 0 || static_cast<std::size_t>(written) >= capacity) return false;
    return ensure_directory(out);
}

bool writable_probe(const char *directory) noexcept {
    char path[320]{};
    for (int attempt = 0; attempt < 16; ++attempt) {
        const int written = std::snprintf(path, sizeof(path),
            "%s/.nests-write-probe-%ld-%d", directory,
            static_cast<long>(::getpid()), attempt);
        if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(path)) return false;
        const int fd = ::open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd < 0) {
            if (errno == EEXIST) continue;
            return false;
        }
        static constexpr char marker[] = "NESTS";
        const ssize_t stored = ::write(fd, marker, sizeof(marker) - 1);
        const bool synced = stored == static_cast<ssize_t>(sizeof(marker) - 1) &&
                            ::fsync(fd) == 0;
        const bool closed = ::close(fd) == 0;
        const bool removed = ::unlink(path) == 0;
        return synced && closed && removed;
    }
    return false;
}

std::uint64_t free_bytes(const char *path) noexcept {
    struct statvfs stats {};
    if (::statvfs(path, &stats) != 0) return 0;
    const std::uint64_t blocks = static_cast<std::uint64_t>(stats.f_bavail);
    const std::uint64_t size = static_cast<std::uint64_t>(stats.f_frsize);
    if (size != 0 && blocks > UINT64_MAX / size) return UINT64_MAX;
    return blocks * size;
}

bool fill_volume(const char *id, const char *label, const char *mount_root,
                 bool removable, StorageVolume *out) noexcept {
    if (id == nullptr || label == nullptr || mount_root == nullptr || out == nullptr) return false;
    struct stat root_info {};
    if (::lstat(mount_root, &root_info) != 0 || !S_ISDIR(root_info.st_mode)) return false;
    if (::access(mount_root, R_OK | X_OK) != 0) return false;

    char nests_dir[256]{};
    char roms_dir[320]{};
    if (!ensure_child_directory(mount_root, "nests", nests_dir, sizeof(nests_dir))) return false;
    if (!ensure_child_directory(nests_dir, "roms", roms_dir, sizeof(roms_dir))) return false;
    if (!writable_probe(roms_dir)) return false;

    StorageVolume candidate{};
    if (!copy_text(candidate.id, sizeof(candidate.id), id) ||
        !copy_text(candidate.label, sizeof(candidate.label), label) ||
        !copy_text(candidate.mount_root, sizeof(candidate.mount_root), mount_root) ||
        !copy_text(candidate.roms_path, sizeof(candidate.roms_path), roms_dir)) return false;
    candidate.free_bytes = free_bytes(roms_dir);
    candidate.removable = removable;
    *out = candidate;
    return true;
}

bool write_all(int fd, const char *data, std::size_t length) noexcept {
    std::size_t offset = 0;
    while (offset < length) {
        const ssize_t stored = ::write(fd, data + offset, length - offset);
        if (stored < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (stored == 0) return false;
        offset += static_cast<std::size_t>(stored);
    }
    return true;
}

bool fsync_parent(const char *path) noexcept {
    char parent[320]{};
    if (!copy_text(parent, sizeof(parent), path)) return false;
    char *slash = std::strrchr(parent, '/');
    if (slash == nullptr) return false;
    if (slash == parent) slash[1] = '\0';
    else *slash = '\0';
    const int fd = ::open(parent, O_RDONLY);
    if (fd < 0) return false;
    const bool ok = ::fsync(fd) == 0;
    (void)::close(fd);
    return ok;
}

} // namespace

bool is_safe_path_segment(const char *value) noexcept {
    if (value == nullptr || value[0] == '\0') return false;
    const std::size_t length = std::strlen(value);
    if (length > 63 || value[0] == '.') return false;
    for (std::size_t i = 0; i < length; ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    }
    return true;
}

bool is_safe_download_filename(const char *value) noexcept {
    if (value == nullptr || value[0] == '\0') return false;
    const std::size_t length = std::strlen(value);
    if (length > 127 || value[0] == '.' || std::strstr(value, "..") != nullptr) return false;
    bool dot_seen = false;
    for (std::size_t i = 0; i < length; ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (c == '.') {
            if (dot_seen || i == 0 || i + 1 == length) return false;
            dot_seen = true;
            continue;
        }
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    }
    return dot_seen;
}

bool scan_storage_roots(const char *internal_root,
                        const char *const *usb_roots, std::size_t usb_count,
                        StorageVolume *volumes, std::size_t capacity,
                        std::size_t *count) noexcept {
    if (volumes == nullptr || count == nullptr || capacity == 0 || internal_root == nullptr) return false;
    *count = 0;

    StorageVolume internal{};
    if (fill_volume("internal", "ARMAZENAMENTO INTERNO", internal_root, false, &internal))
        volumes[(*count)++] = internal;

    for (std::size_t i = 0; i < usb_count && *count < capacity; ++i) {
        if (usb_roots == nullptr || usb_roots[i] == nullptr) continue;
        char id[16]{};
        char label[32]{};
        if (std::snprintf(id, sizeof(id), "usb%u", static_cast<unsigned>(i)) <= 0 ||
            std::snprintf(label, sizeof(label), "USB %u", static_cast<unsigned>(i)) <= 0) continue;
        StorageVolume usb{};
        if (fill_volume(id, label, usb_roots[i], true, &usb))
            volumes[(*count)++] = usb;
    }
    return *count > 0;
}

bool scan_default_storage(StorageVolume *volumes, std::size_t capacity,
                          std::size_t *count) noexcept {
    static constexpr const char *usb_roots[] = {
        "/usb0", "/usb1", "/usb2", "/usb3",
        "/usb4", "/usb5", "/usb6", "/usb7"
    };
    return scan_storage_roots("/download0", usb_roots,
                              sizeof(usb_roots) / sizeof(usb_roots[0]),
                              volumes, capacity, count);
}

bool load_storage_settings(const char *settings_path,
                           StorageSettings *settings) noexcept {
    if (settings_path == nullptr || settings == nullptr) return false;
    StorageSettings loaded{};
    std::FILE *file = std::fopen(settings_path, "rb");
    if (file == nullptr) {
        *settings = loaded;
        return errno == ENOENT;
    }
    char line[96]{};
    bool valid = true;
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        line[std::strcspn(line, "\r\n")] = '\0';
        if (std::strncmp(line, "volume=", 7) == 0) {
            const char *value = line + 7;
            if (std::strcmp(value, "internal") == 0) {
                (void)copy_text(loaded.volume_id, sizeof(loaded.volume_id), "internal");
            } else if (std::strncmp(value, "usb", 3) == 0 && is_safe_path_segment(value)) {
                (void)copy_text(loaded.volume_id, sizeof(loaded.volume_id), value);
            } else {
                valid = false;
            }
        } else if (std::strncmp(line, "layout=", 7) == 0) {
            const char *value = line + 7;
            if (std::strcmp(value, "by_console") == 0) loaded.separate_by_console = true;
            else if (std::strcmp(value, "single") == 0) loaded.separate_by_console = false;
            else valid = false;
        }
        // Legacy 0.6 records only contained layout=..., which remains accepted.
    }
    if (std::ferror(file)) valid = false;
    (void)std::fclose(file);
    if (valid) *settings = loaded;
    return valid;
}

bool save_storage_settings(const char *settings_path,
                           const StorageSettings &settings) noexcept {
    if (settings_path == nullptr || settings_path[0] == '\0') return false;
    if (std::strcmp(settings.volume_id, "internal") != 0 &&
        !(std::strncmp(settings.volume_id, "usb", 3) == 0 && is_safe_path_segment(settings.volume_id)))
        return false;

    char temporary[320]{};
    const int temp_len = std::snprintf(temporary, sizeof(temporary), "%s.tmp", settings_path);
    if (temp_len <= 0 || static_cast<std::size_t>(temp_len) >= sizeof(temporary)) return false;

    const int fd = ::open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return false;
    char content[128]{};
    const int length = std::snprintf(content, sizeof(content),
        "version=2\nvolume=%s\nlayout=%s\n",
        settings.volume_id, settings.separate_by_console ? "by_console" : "single");
    bool ok = length > 0 && static_cast<std::size_t>(length) < sizeof(content) &&
              write_all(fd, content, static_cast<std::size_t>(length));
    if (ok) ok = ::fsync(fd) == 0;
    if (::close(fd) != 0) ok = false;
    if (!ok) {
        (void)::unlink(temporary);
        return false;
    }
    if (::rename(temporary, settings_path) != 0) {
        (void)::unlink(temporary);
        return false;
    }
    (void)fsync_parent(settings_path);
    return true;
}

bool find_storage_volume(const StorageVolume *volumes, std::size_t count,
                         const char *volume_id, StorageVolume *out) noexcept {
    if (volumes == nullptr || volume_id == nullptr || out == nullptr) return false;
    for (std::size_t i = 0; i < count; ++i) {
        if (std::strcmp(volumes[i].id, volume_id) == 0) {
            *out = volumes[i];
            return true;
        }
    }
    return false;
}

bool build_download_paths(const StorageVolume &volume,
                          const StorageSettings &settings,
                          const char *console_slug,
                          const char *file_name,
                          char *final_path, std::size_t final_capacity,
                          char *partial_path, std::size_t partial_capacity) noexcept {
    if (final_path == nullptr || partial_path == nullptr ||
        final_capacity == 0 || partial_capacity == 0 ||
        !is_safe_download_filename(file_name)) return false;

    char directory[320]{};
    if (settings.separate_by_console) {
        if (!is_safe_path_segment(console_slug)) return false;
        const int written = std::snprintf(directory, sizeof(directory), "%s/%s",
                                          volume.roms_path, console_slug);
        if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(directory)) return false;
        if (!ensure_directory(directory)) return false;
    } else {
        if (!copy_text(directory, sizeof(directory), volume.roms_path)) return false;
    }

    const int final_len = std::snprintf(final_path, final_capacity, "%s/%s", directory, file_name);
    if (final_len <= 0 || static_cast<std::size_t>(final_len) >= final_capacity) return false;
    const int part_len = std::snprintf(partial_path, partial_capacity, "%s.part", final_path);
    return part_len > 0 && static_cast<std::size_t>(part_len) < partial_capacity;
}

} // namespace nests
