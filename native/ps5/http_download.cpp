#include "http_download.hpp"

#include "sha256.hpp"

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

extern "C" {
int sceNetPoolCreate(const char *name, int size, int flags);
int sceNetPoolDestroy(int pool);
int sceSslInit(std::size_t pool_size);
int sceSslTerm(int context);
int sceHttpInit(int net_pool, int ssl_context, std::size_t pool_size);
int sceHttpTerm(int context);
int sceHttpCreateTemplate(int context, const char *user_agent, int version, int auto_proxy);
int sceHttpDeleteTemplate(int template_id);
int sceHttpCreateConnectionWithURL(int template_id, const char *url, int keep_alive);
int sceHttpDeleteConnection(int connection);
int sceHttpCreateRequestWithURL(int connection, int method, const char *url,
                                std::uint64_t content_length);
int sceHttpDeleteRequest(int request);
int sceHttpSetAutoRedirect(int id, int enabled);
int sceHttpSetResolveTimeOut(int id, std::uint32_t usec);
int sceHttpSetConnectTimeOut(int id, std::uint32_t usec);
int sceHttpSetSendTimeOut(int id, std::uint32_t usec);
int sceHttpSetRecvTimeOut(int id, std::uint32_t usec);
int sceHttpsEnableOption(int id, std::uint32_t flags);
int sceHttpSendRequest(int request, const void *data, std::size_t size);
int sceHttpGetStatusCode(int request, int *status);
int sceHttpReadData(int request, void *data, std::size_t size);
}

namespace nests {
namespace {

constexpr std::uint64_t kMaximumDownloadBytes = 2ULL * 1024ULL * 1024ULL * 1024ULL;
constexpr std::size_t kReadBufferSize = 64U * 1024U;
constexpr std::uint32_t kTimeoutUsec = 15000000U;
constexpr int kHttp11 = 2;
constexpr int kHttpGet = 0;
// Server verify | host name | not-after | not-before | known CA | SNI.
constexpr std::uint32_t kHttpsVerifyFlags = 0x01U | 0x04U | 0x08U | 0x10U | 0x20U | 0x80U;

void set_error(char *out, std::size_t capacity, const char *message) noexcept {
    if (out == nullptr || capacity == 0) return;
    (void)std::snprintf(out, capacity, "%s", message != nullptr ? message : "Erro desconhecido");
}

bool is_hex_sha256(const char *value) noexcept {
    if (value == nullptr || std::strlen(value) != 64) return false;
    for (std::size_t i = 0; i < 64; ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (!std::isxdigit(c)) return false;
    }
    return true;
}

bool valid_host_name(const char *host) noexcept {
    if (host == nullptr || host[0] == '\0') return false;
    const std::size_t length = std::strlen(host);
    if (length > 120 || host[0] == '.' || host[length - 1] == '.') return false;
    bool has_dot = false;
    bool previous_dot = false;
    for (std::size_t i = 0; i < length; ++i) {
        const unsigned char c = static_cast<unsigned char>(host[i]);
        if (c == '.') {
            if (previous_dot) return false;
            has_dot = true;
            previous_dot = true;
            continue;
        }
        previous_dot = false;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-')) return false;
    }
    return has_dot;
}

bool extract_https_host(const char *url, char *host, std::size_t capacity) noexcept {
    if (url == nullptr || host == nullptr || capacity == 0) return false;
    static constexpr char prefix[] = "https://";
    if (std::strncmp(url, prefix, sizeof(prefix) - 1) != 0) return false;
    const char *begin = url + sizeof(prefix) - 1;
    const char *end = begin;
    while (*end != '\0' && *end != '/' && *end != '?' && *end != '#' && *end != ':') {
        if (static_cast<unsigned char>(*end) <= 0x20 || *end == '@') return false;
        ++end;
    }
    if (*end == ':' || end == begin) return false; // only the default HTTPS port is accepted
    const std::size_t length = static_cast<std::size_t>(end - begin);
    if (length >= capacity) return false;
    std::memcpy(host, begin, length);
    host[length] = '\0';
    return valid_host_name(host);
}

bool equal_host_case_insensitive(const char *left, const char *right) noexcept {
    if (left == nullptr || right == nullptr) return false;
    while (*left != '\0' && *right != '\0') {
        if (std::tolower(static_cast<unsigned char>(*left)) !=
            std::tolower(static_cast<unsigned char>(*right))) return false;
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

bool write_all(int fd, const std::uint8_t *data, std::size_t length) noexcept {
    std::size_t offset = 0;
    while (offset < length) {
        const ssize_t written = ::write(fd, data + offset, length - offset);
        if (written < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (written == 0) return false;
        offset += static_cast<std::size_t>(written);
    }
    return true;
}

bool available_space(const char *path, std::uint64_t required) noexcept {
    struct statvfs stats {};
    if (::statvfs(path, &stats) != 0) return false;
    const std::uint64_t blocks = static_cast<std::uint64_t>(stats.f_bavail);
    const std::uint64_t block_size = static_cast<std::uint64_t>(stats.f_frsize);
    if (block_size != 0 && blocks > UINT64_MAX / block_size) return true;
    return blocks * block_size >= required;
}

bool sync_parent_directory(const char *path) noexcept {
    char directory[320]{};
    const std::size_t length = std::strlen(path);
    if (length == 0 || length >= sizeof(directory)) return false;
    std::memcpy(directory, path, length + 1);
    char *slash = std::strrchr(directory, '/');
    if (slash == nullptr) return false;
    if (slash == directory) slash[1] = '\0';
    else *slash = '\0';
    const int fd = ::open(directory, O_RDONLY);
    if (fd < 0) return false;
    const bool result = ::fsync(fd) == 0;
    (void)::close(fd);
    return result;
}

struct HttpResources {
    int pool = -1;
    int ssl = -1;
    int http = -1;
    int template_id = -1;
    int connection = -1;
    int request = -1;

    ~HttpResources() {
        if (request >= 0) (void)sceHttpDeleteRequest(request);
        if (connection >= 0) (void)sceHttpDeleteConnection(connection);
        if (template_id >= 0) (void)sceHttpDeleteTemplate(template_id);
        if (http >= 0) (void)sceHttpTerm(http);
        if (ssl >= 0) (void)sceSslTerm(ssl);
        if (pool >= 0) (void)sceNetPoolDestroy(pool);
    }
};

bool open_http(const char *url, HttpResources &resources, char *error,
               std::size_t error_capacity) noexcept {
    int result = resources.pool = sceNetPoolCreate("nests_download", 1024 * 1024, 0);
    if (result >= 0) result = resources.ssl = sceSslInit(304 * 1024);
    if (result >= 0) result = resources.http = sceHttpInit(resources.pool, resources.ssl, 4 * 1024 * 1024);
    if (result >= 0) resources.template_id = sceHttpCreateTemplate(
        resources.http, "NESTS/0.7 (PS5 homebrew)", kHttp11, 0);
    if (resources.template_id >= 0) result = sceHttpSetAutoRedirect(resources.template_id, 0);
    if (resources.template_id >= 0 && result >= 0)
        result = sceHttpSetResolveTimeOut(resources.template_id, kTimeoutUsec);
    if (resources.template_id >= 0 && result >= 0)
        result = sceHttpSetConnectTimeOut(resources.template_id, kTimeoutUsec);
    if (resources.template_id >= 0 && result >= 0)
        result = sceHttpSetSendTimeOut(resources.template_id, kTimeoutUsec);
    if (resources.template_id >= 0 && result >= 0)
        result = sceHttpSetRecvTimeOut(resources.template_id, kTimeoutUsec);
    if (resources.template_id >= 0 && result >= 0)
        result = sceHttpsEnableOption(resources.template_id, kHttpsVerifyFlags);
    if (resources.template_id >= 0 && result >= 0)
        resources.connection = sceHttpCreateConnectionWithURL(resources.template_id, url, 0);
    if (resources.connection >= 0)
        resources.request = sceHttpCreateRequestWithURL(resources.connection, kHttpGet, url, 0);

    if (result < 0 || resources.pool < 0 || resources.ssl < 0 || resources.http < 0 ||
        resources.template_id < 0 || resources.connection < 0 || resources.request < 0) {
        set_error(error, error_capacity, "Falha ao inicializar a conexao HTTPS do PS5.");
        return false;
    }
    return true;
}

bool create_partial_file(const char *path, int *fd, char *error,
                         std::size_t error_capacity) noexcept {
    if (path == nullptr || fd == nullptr) return false;
    struct stat existing {};
    if (::lstat(path, &existing) == 0) {
        if (!S_ISREG(existing.st_mode) || ::unlink(path) != 0) {
            set_error(error, error_capacity, "Arquivo parcial existente nao pode ser removido.");
            return false;
        }
    } else if (errno != ENOENT) {
        set_error(error, error_capacity, "Nao foi possivel verificar o arquivo parcial.");
        return false;
    }

    int flags = O_WRONLY | O_CREAT | O_EXCL;
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    *fd = ::open(path, flags, 0600);
    if (*fd < 0) {
        set_error(error, error_capacity, "Nao foi possivel criar o arquivo temporario no destino.");
        return false;
    }
    return true;
}

} // namespace

bool validate_download_spec(const DownloadSpec &spec,
                            char *error, std::size_t error_capacity) noexcept {
    char host[128]{};
    if (!extract_https_host(spec.url, host, sizeof(host))) {
        set_error(error, error_capacity, "A URL precisa ser HTTPS direto, sem porta personalizada ou credenciais.");
        return false;
    }
    if (!valid_host_name(spec.trusted_host) ||
        !equal_host_case_insensitive(host, spec.trusted_host)) {
        set_error(error, error_capacity, "O host da URL nao corresponde ao host aprovado pelo catalogo.");
        return false;
    }
    if (!is_safe_path_segment(spec.console_slug) ||
        !is_safe_download_filename(spec.file_name)) {
        set_error(error, error_capacity, "Nome de console ou arquivo invalido.");
        return false;
    }
    if (spec.expected_size == 0 || spec.expected_size > kMaximumDownloadBytes) {
        set_error(error, error_capacity, "Tamanho esperado fora do limite permitido.");
        return false;
    }
    if (!is_hex_sha256(spec.sha256)) {
        set_error(error, error_capacity, "SHA-256 ausente ou invalido.");
        return false;
    }
    if (!spec.redistribution_allowed || !spec.license_evidence_verified ||
        !spec.catalog_signature_verified) {
        set_error(error, error_capacity, "Download bloqueado: autorizacao, evidencia e assinatura do catalogo sao obrigatorias.");
        return false;
    }
    return true;
}

bool download_https_to_storage(const DownloadSpec &spec,
                               const StorageVolume &volume,
                               const StorageSettings &settings,
                               const std::atomic<bool> &cancel_requested,
                               DownloadProgress &progress,
                               char *error, std::size_t error_capacity) noexcept {
    progress.state.store(DownloadState::preparing);
    progress.received_bytes.store(0);
    progress.expected_bytes.store(spec.expected_size);
    if (!validate_download_spec(spec, error, error_capacity)) {
        progress.state.store(DownloadState::failed);
        return false;
    }
    if (std::strcmp(volume.id, settings.volume_id) != 0) {
        set_error(error, error_capacity, "O volume escolhido nao corresponde a configuracao atual.");
        progress.state.store(DownloadState::failed);
        return false;
    }

    struct stat root_info {};
    struct stat roms_info {};
    if (::stat(volume.mount_root, &root_info) != 0 || !S_ISDIR(root_info.st_mode) ||
        ::stat(volume.roms_path, &roms_info) != 0 || !S_ISDIR(roms_info.st_mode) ||
        ::access(volume.roms_path, W_OK | X_OK) != 0) {
        set_error(error, error_capacity, "O armazenamento selecionado foi removido ou esta sem permissao de escrita.");
        progress.state.store(DownloadState::failed);
        return false;
    }
    if (!available_space(volume.roms_path, spec.expected_size)) {
        set_error(error, error_capacity, "Espaco livre insuficiente no armazenamento escolhido.");
        progress.state.store(DownloadState::failed);
        return false;
    }

    char final_path[320]{};
    char partial_path[336]{};
    if (!build_download_paths(volume, settings, spec.console_slug, spec.file_name,
                              final_path, sizeof(final_path), partial_path, sizeof(partial_path))) {
        set_error(error, error_capacity, "Nao foi possivel montar o caminho seguro de destino.");
        progress.state.store(DownloadState::failed);
        return false;
    }
    struct stat final_info {};
    if (::lstat(final_path, &final_info) == 0) {
        set_error(error, error_capacity, "Este arquivo ja existe na biblioteca. Remova-o antes de baixar novamente.");
        progress.state.store(DownloadState::failed);
        return false;
    }
    if (errno != ENOENT) {
        set_error(error, error_capacity, "Nao foi possivel verificar o destino final.");
        progress.state.store(DownloadState::failed);
        return false;
    }
    if (cancel_requested.load()) {
        set_error(error, error_capacity, "Download cancelado.");
        progress.state.store(DownloadState::cancelled);
        return false;
    }

    int output_fd = -1;
    if (!create_partial_file(partial_path, &output_fd, error, error_capacity)) {
        progress.state.store(DownloadState::failed);
        return false;
    }

    bool file_ok = false;
    bool success = false;
    HttpResources http{};
    nests::Sha256 hash{};
    std::uint64_t received = 0;

    progress.state.store(DownloadState::connecting);
    if (!open_http(spec.url, http, error, error_capacity)) goto cleanup;

    {
        const int sent = sceHttpSendRequest(http.request, nullptr, 0);
        if (sent < 0) {
            set_error(error, error_capacity, "Falha ao enviar a solicitacao HTTPS.");
            goto cleanup;
        }
        int status = 0;
        const int status_result = sceHttpGetStatusCode(http.request, &status);
        if (status_result < 0 || status != 200) {
            set_error(error, error_capacity, status >= 300 && status < 400
                ? "O servidor redirecionou o download; use uma URL HTTPS direta."
                : "O servidor nao retornou HTTP 200.");
            goto cleanup;
        }
    }

    progress.state.store(DownloadState::receiving);
    {
        std::uint8_t buffer[kReadBufferSize]{};
        for (;;) {
            if (cancel_requested.load()) {
                set_error(error, error_capacity, "Download cancelado pelo usuario.");
                progress.state.store(DownloadState::cancelled);
                goto cleanup;
            }
            const int got = sceHttpReadData(http.request, buffer, sizeof(buffer));
            if (got < 0) {
                set_error(error, error_capacity, "Erro durante a leitura do arquivo HTTPS.");
                goto cleanup;
            }
            if (got == 0) break;
            const std::uint64_t chunk = static_cast<std::uint64_t>(got);
            if (chunk > spec.expected_size - received) {
                set_error(error, error_capacity, "O arquivo recebido excede o tamanho declarado no catalogo.");
                goto cleanup;
            }
            if (!write_all(output_fd, buffer, static_cast<std::size_t>(got))) {
                set_error(error, error_capacity, "Falha ao gravar no armazenamento selecionado.");
                goto cleanup;
            }
            hash.update(buffer, static_cast<std::size_t>(got));
            received += chunk;
            progress.received_bytes.store(received);
        }
    }

    if (received != spec.expected_size) {
        set_error(error, error_capacity, "O tamanho recebido e diferente do tamanho esperado.");
        goto cleanup;
    }

    progress.state.store(DownloadState::verifying);
    {
        std::uint8_t digest[32]{};
        hash.finish(digest);
        if (!sha256_matches_hex(digest, spec.sha256)) {
            set_error(error, error_capacity, "SHA-256 divergente. O arquivo parcial sera descartado.");
            goto cleanup;
        }
    }

    if (::fsync(output_fd) != 0) {
        set_error(error, error_capacity, "Nao foi possivel sincronizar o arquivo baixado.");
        goto cleanup;
    }
    if (::close(output_fd) != 0) {
        output_fd = -1;
        set_error(error, error_capacity, "Falha ao fechar o arquivo temporario.");
        goto cleanup;
    }
    output_fd = -1;
    file_ok = true;

    if (::lstat(final_path, &final_info) == 0) {
        set_error(error, error_capacity, "O arquivo final apareceu durante o download; nada foi sobrescrito.");
        goto cleanup;
    }
    if (errno != ENOENT || ::rename(partial_path, final_path) != 0) {
        set_error(error, error_capacity, "Nao foi possivel publicar o arquivo verificado.");
        goto cleanup;
    }
    (void)sync_parent_directory(final_path);
    success = true;

cleanup:
    if (output_fd >= 0) (void)::close(output_fd);
    if (!success) (void)::unlink(partial_path);
    progress.received_bytes.store(received);
    if (success) {
        progress.state.store(DownloadState::complete);
        set_error(error, error_capacity, "Download concluido e SHA-256 validado.");
        return true;
    }
    if (progress.state.load() != DownloadState::cancelled)
        progress.state.store(DownloadState::failed);
    (void)file_ok;
    return false;
}

} // namespace nests
