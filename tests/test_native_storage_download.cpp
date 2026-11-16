#include "http_download.hpp"
#include "sha256.hpp"
#include "storage_manager.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

extern "C" void nests_test_http_reset(int status);

namespace {
int failures = 0;
void check(bool condition, const char *message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}
bool exists(const char *path) {
    struct stat info {};
    return ::lstat(path, &info) == 0;
}
std::string read_file(const char *path) {
    std::FILE *file = std::fopen(path, "rb");
    if (file == nullptr) return {};
    std::string data;
    char buffer[128]{};
    std::size_t got = 0;
    while ((got = std::fread(buffer, 1, sizeof(buffer), file)) > 0)
        data.append(buffer, got);
    std::fclose(file);
    return data;
}
bool make_dir(const char *path) {
    return ::mkdir(path, 0700) == 0;
}
nests::DownloadSpec make_spec() {
    nests::DownloadSpec spec{};
    std::snprintf(spec.url, sizeof(spec.url), "%s", "https://downloads.example/test.nes");
    std::snprintf(spec.trusted_host, sizeof(spec.trusted_host), "%s", "downloads.example");
    std::snprintf(spec.console_slug, sizeof(spec.console_slug), "%s", "nes");
    std::snprintf(spec.file_name, sizeof(spec.file_name), "%s", "nests-test.nes");
    std::snprintf(spec.sha256, sizeof(spec.sha256), "%s",
        "779d53b409ef2acda246f2dfdbfb1dfc2b325187dcf32dadd2c9c884af77ab48");
    spec.expected_size = sizeof("NESTS test payload\n") - 1;
    spec.redistribution_allowed = true;
    spec.license_evidence_verified = true;
    spec.catalog_signature_verified = true;
    return spec;
}
}

int main() {
    char temp_template[] = "/tmp/nests-native-test-XXXXXX";
    char *root = ::mkdtemp(temp_template);
    check(root != nullptr, "temporary test root created");
    if (root == nullptr) return 1;

    char internal_root[256]{};
    char usb_root[256]{};
    char settings_path[320]{};
    std::snprintf(internal_root, sizeof(internal_root), "%s/internal", root);
    std::snprintf(usb_root, sizeof(usb_root), "%s/usb0", root);
    std::snprintf(settings_path, sizeof(settings_path), "%s/internal/nests/settings.txt", root);
    check(make_dir(internal_root), "internal test mount created");
    check(make_dir(usb_root), "USB test mount created");

    const char *usb_roots[] = {usb_root};
    nests::StorageVolume volumes[nests::kMaxStorageVolumes]{};
    std::size_t count = 0;
    check(nests::scan_storage_roots(internal_root, usb_roots, 1,
        volumes, nests::kMaxStorageVolumes, &count), "storage scan finds writable mounts");
    check(count == 2, "internal and USB mount discovered");
    check(std::strcmp(volumes[0].id, "internal") == 0, "internal mount is first");
    check(std::strcmp(volumes[1].id, "usb0") == 0 && volumes[1].removable,
          "USB mount has stable ID and removable flag");

    nests::StorageSettings settings{};
    std::snprintf(settings.volume_id, sizeof(settings.volume_id), "%s", "usb0");
    settings.separate_by_console = false;
    check(nests::save_storage_settings(settings_path, settings), "storage settings saved atomically");
    nests::StorageSettings loaded{};
    check(nests::load_storage_settings(settings_path, &loaded), "storage settings reloaded");
    check(std::strcmp(loaded.volume_id, "usb0") == 0 && !loaded.separate_by_console,
          "storage target and layout persist");

    char final_path[320]{};
    char part_path[336]{};
    check(nests::build_download_paths(volumes[1], loaded, "nes", "game.nes",
        final_path, sizeof(final_path), part_path, sizeof(part_path)),
        "flat layout builds download paths");
    check(std::strstr(final_path, "/nests/roms/game.nes") != nullptr,
          "flat layout writes under NESTS ROM directory");
    check(!nests::build_download_paths(volumes[1], loaded, "../etc", "game.nes",
        final_path, sizeof(final_path), part_path, sizeof(part_path)),
        "path traversal via console slug rejected");
    check(!nests::build_download_paths(volumes[1], loaded, "nes", "../game.nes",
        final_path, sizeof(final_path), part_path, sizeof(part_path)),
        "path traversal via filename rejected");

    std::uint8_t digest[32]{};
    nests::Sha256 sha;
    static constexpr char payload[] = "NESTS test payload\n";
    sha.update(payload, sizeof(payload) - 1);
    sha.finish(digest);
    check(nests::sha256_matches_hex(digest,
        "779d53b409ef2acda246f2dfdbfb1dfc2b325187dcf32dadd2c9c884af77ab48"),
        "SHA-256 implementation matches known digest");

    char error[256]{};
    nests::DownloadSpec spec = make_spec();
    check(nests::validate_download_spec(spec, error, sizeof(error)),
          "authorized direct HTTPS record validates");
    nests::DownloadSpec invalid = spec;
    std::snprintf(invalid.url, sizeof(invalid.url), "%s", "http://downloads.example/test.nes");
    check(!nests::validate_download_spec(invalid, error, sizeof(error)),
          "plain HTTP is blocked");
    invalid = spec;
    std::snprintf(invalid.trusted_host, sizeof(invalid.trusted_host), "%s", "other.example");
    check(!nests::validate_download_spec(invalid, error, sizeof(error)),
          "host outside allowlist is blocked");
    invalid = spec;
    invalid.redistribution_allowed = false;
    check(!nests::validate_download_spec(invalid, error, sizeof(error)),
          "missing redistribution permission blocks download");
    invalid = spec;
    invalid.catalog_signature_verified = false;
    check(!nests::validate_download_spec(invalid, error, sizeof(error)),
          "unsigned catalog item blocks download");

    nests::StorageSettings usb_settings{};
    std::snprintf(usb_settings.volume_id, sizeof(usb_settings.volume_id), "%s", "usb0");
    usb_settings.separate_by_console = true;
    std::atomic<bool> cancel{false};
    nests::DownloadProgress progress{};

    nests_test_http_reset(200);
    check(nests::download_https_to_storage(spec, volumes[1], usb_settings, cancel,
        progress, error, sizeof(error)), "valid HTTPS payload downloads and verifies");
    check(progress.state.load() == nests::DownloadState::complete,
          "successful transfer ends in complete state");
    char expected_path[320]{};
    std::snprintf(expected_path, sizeof(expected_path), "%s/nests/roms/nes/nests-test.nes",
                  usb_root);
    check(exists(expected_path), "verified file is atomically promoted");
    check(read_file(expected_path) == payload, "downloaded bytes match source fixture");

    spec = make_spec();
    std::snprintf(spec.sha256, sizeof(spec.sha256), "%s",
        "0000000000000000000000000000000000000000000000000000000000000000");
    std::snprintf(spec.file_name, sizeof(spec.file_name), "%s", "wrong-hash.nes");
    nests_test_http_reset(200);
    check(!nests::download_https_to_storage(spec, volumes[1], usb_settings, cancel,
        progress, error, sizeof(error)), "wrong SHA-256 is rejected");
    char wrong_final[320]{};
    std::snprintf(wrong_final, sizeof(wrong_final), "%s/nests/roms/nes/wrong-hash.nes", usb_root);
    char wrong_part[336]{};
    std::snprintf(wrong_part, sizeof(wrong_part), "%s.part", wrong_final);
    check(!exists(wrong_final) && !exists(wrong_part),
          "hash mismatch removes partial file and never publishes it");

    spec = make_spec();
    std::snprintf(spec.file_name, sizeof(spec.file_name), "%s", "redirect.nes");
    nests_test_http_reset(302);
    check(!nests::download_https_to_storage(spec, volumes[1], usb_settings, cancel,
        progress, error, sizeof(error)), "HTTP redirects are rejected");
    char redirect_final[320]{};
    std::snprintf(redirect_final, sizeof(redirect_final), "%s/nests/roms/nes/redirect.nes", usb_root);
    char redirect_part[336]{};
    std::snprintf(redirect_part, sizeof(redirect_part), "%s.part", redirect_final);
    check(!exists(redirect_final) && !exists(redirect_part),
          "redirect rejection cleans temporary file");

    spec = make_spec();
    std::snprintf(spec.file_name, sizeof(spec.file_name), "%s", "cancelled.nes");
    cancel.store(true);
    nests_test_http_reset(200);
    check(!nests::download_https_to_storage(spec, volumes[1], usb_settings, cancel,
        progress, error, sizeof(error)), "pre-cancelled download is stopped");
    check(progress.state.load() == nests::DownloadState::cancelled,
          "cancel state is reported");
    cancel.store(false);

    // A mount that has disappeared or lost write access is not selected as available.
    nests::StorageVolume missing_volume{};
    check(!nests::find_storage_volume(volumes, count, "usb9", &missing_volume),
          "unknown USB device does not resolve");

    // A device that vanishes after the scan must fail before a new transfer starts.
    char removable_rom[320]{};
    std::snprintf(removable_rom, sizeof(removable_rom), "%s/nests/roms/nes", usb_root);
    check(::rmdir(removable_rom) == 0, "remove test USB console directory");
    char removable_root[320]{};
    std::snprintf(removable_root, sizeof(removable_root), "%s/nests/roms", usb_root);
    check(::rmdir(removable_root) == 0, "remove test USB ROM directory");
    char removable_nests[320]{};
    std::snprintf(removable_nests, sizeof(removable_nests), "%s/nests", usb_root);
    check(::rmdir(removable_nests) == 0, "remove test USB NESTS directory");
    check(::rmdir(usb_root) == 0, "unmount test USB volume");
    spec = make_spec();
    std::snprintf(spec.file_name, sizeof(spec.file_name), "%s", "removed-usb.nes");
    nests_test_http_reset(200);
    check(!nests::download_https_to_storage(spec, volumes[1], usb_settings, cancel,
        progress, error, sizeof(error)), "a removed USB volume is rejected");
    check(progress.state.load() == nests::DownloadState::failed,
          "removed USB volume reports failed state");

    (void)::unlink(expected_path);
    (void)::unlink(settings_path);
    char settings_temporary[360]{};
    std::snprintf(settings_temporary, sizeof(settings_temporary), "%s.tmp", settings_path);
    (void)::unlink(settings_temporary);
    char internal_roms[320]{};
    std::snprintf(internal_roms, sizeof(internal_roms), "%s/nests/roms", internal_root);
    (void)::rmdir(internal_roms);
    char internal_nests[320]{};
    std::snprintf(internal_nests, sizeof(internal_nests), "%s/nests", internal_root);
    (void)::rmdir(internal_nests);
    (void)::rmdir(internal_root);
    (void)::rmdir(root);

    if (failures != 0) {
        std::fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    std::puts("All native storage/download tests passed.");
    return 0;
}
