/* Loopback-only server; only the static catalogue can be refreshed. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <winhttp.h>
#include <wchar.h>
typedef SOCKET sock;
#define BAD_SOCKET INVALID_SOCKET
#define close_socket closesocket
#define prefix_ignore_case _strnicmp
static unsigned long long now_ms(void) { return GetTickCount64(); }
#else
#include <arpa/inet.h>
#include <signal.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
typedef int sock;
#define BAD_SOCKET (-1)
#define close_socket close
#define prefix_ignore_case strncasecmp
static unsigned long long now_ms(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (unsigned long long)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}
#endif

#define PORT 8765
#define LIMIT (4 * 1024 * 1024)
static const char health[] = "game3-local-catalogue-v1";
#define CATALOGUE_SCRIPT "sha256-Dd8bWVoSx4X1XC03tYNxF1/ygUiLjRAXBYN2C3exbd4="
static const char csp[] = "default-src 'none'; script-src '" CATALOGUE_SCRIPT "'; "
    "style-src 'unsafe-inline'; img-src data:; font-src data:; connect-src 'none'; "
    "form-action 'none'; base-uri 'none'; object-src 'none'; frame-ancestors 'none'";

struct catalogue {
    char *html;
    size_t size;
#ifdef _WIN32
    const wchar_t *path;
    SRWLOCK lock;
#endif
};

#ifdef _WIN32
static int compatible_html(const char *body, size_t size) {
    static const char doctype[] = "<!DOCTYPE html>";
    return size >= sizeof doctype - 1 && size <= LIMIT && !memchr(body, '\0', size) &&
        !memcmp(body, doctype, sizeof doctype - 1) && strstr(body, CATALOGUE_SCRIPT) &&
        strstr(body, "data-game3-local-label=\"v1\"");
}

/* Only this public HTML endpoint is fetched. No redirects, cookies, or executable downloads. */
static size_t fetch_catalogue(char *body) {
    static const wchar_t path[] = L"/repos/VolodyKira/game_3/contents/game3-catalogue.html?ref=main";
    HINTERNET session = NULL, connection = NULL, request = NULL;
    DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER, disabled = WINHTTP_DISABLE_COOKIES;
    DWORD status = 0, status_size = sizeof status, count;
    size_t received = 0, valid = 0;
    const unsigned long long start = now_ms();
    session = WinHttpOpen(L"Game3-Local-Catalogue/1.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session || !WinHttpSetTimeouts(session, 2500, 3000, 3000, 4000)) goto done;
    connection = WinHttpConnect(session, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connection) goto done;
    request = WinHttpOpenRequest(connection, L"GET", path, NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request || !WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirects, sizeof redirects) ||
        !WinHttpSetOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof disabled) ||
        !WinHttpSendRequest(request, L"Accept: application/vnd.github.raw+json\r\nCache-Control: no-cache\r\n",
            (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request, NULL) ||
        !WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX) || status != 200) goto done;
    do {
        if (now_ms() - start >= 12000 ||
            !WinHttpReadData(request, body + received, (DWORD)(LIMIT + 1 - received), &count)) goto done;
        received += count;
        if (received > LIMIT) goto done;
    } while (count);
    body[received] = '\0';
    if (compatible_html(body, received)) valid = received;
done:
    if (request) WinHttpCloseHandle(request);
    if (connection) WinHttpCloseHandle(connection);
    if (session) WinHttpCloseHandle(session);
    return valid;
}

static int save_catalogue(const wchar_t *path, const char *body, size_t size) {
    wchar_t dir[MAX_PATH], temp[MAX_PATH];
    wcscpy(dir, path); // WinMain has already checked the length.
    wchar_t *separator = wcsrchr(dir, L'\\');
    if (!separator) return 0;
    separator[1] = L'\0';
    if (!GetTempFileNameW(dir, L"g3c", 0, temp)) return 0;
    HANDLE file = CreateFileW(temp, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD count = 0;
    int ok = 0;
    if (file != INVALID_HANDLE_VALUE) {
        ok = WriteFile(file, body, (DWORD)size, &count, NULL) && count == size && FlushFileBuffers(file);
        CloseHandle(file);
    }
    if (ok && MoveFileExW(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return 1;
    DeleteFileW(temp);
    return 0;
}

static DWORD WINAPI refresh_catalogue(void *context) {
    struct catalogue *catalogue = context;
    char *latest = malloc(LIMIT + 1);
    if (!latest) return 0;
    size_t size = fetch_catalogue(latest);
    // This worker is the only writer. The listener only reads the current buffer.
    if (size && (size != catalogue->size || memcmp(catalogue->html, latest, size)) &&
        save_catalogue(catalogue->path, latest, size)) {
        AcquireSRWLockExclusive(&catalogue->lock);
        char *previous = catalogue->html;
        catalogue->html = latest;
        catalogue->size = size;
        ReleaseSRWLockExclusive(&catalogue->lock);
        latest = previous;
    }
    free(latest);
    return 0;
}
#endif

static int send_all(sock client, const char *data, size_t size) {
    while (size) {
        int n = send(client, data, (int)size, 0);
        if (n <= 0) return 0;
        data += n;
        size -= (size_t)n;
    }
    return 1;
}

static void reply(sock client, const char *status, const char *type, const char *data, size_t size) {
    char header[768];
    int len = snprintf(header, sizeof header,
        "HTTP/1.1 %s\r\nContent-Type: %s; charset=utf-8\r\nContent-Length: %zu\r\n"
        "Connection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n"
        "X-Frame-Options: DENY\r\nReferrer-Policy: no-referrer\r\nContent-Security-Policy: %s\r\n\r\n",
        status, type, size, csp);
    if (len > 0 && (size_t)len < sizeof header && send_all(client, header, (size_t)len)) {
        (void)send_all(client, data, size);
    }
}

static void serve_client(sock client, struct catalogue *catalogue) {
#ifdef _WIN32
    DWORD timeout = 1000;
#else
    struct timeval timeout = {1, 0};
#endif
    if (setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof timeout) != 0 ||
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof timeout) != 0) return;

    char request[8193] = {0};
    size_t used = 0;
    const unsigned long long start = now_ms();
    while (!strstr(request, "\r\n\r\n")) {
        if (used == sizeof request - 1 || now_ms() - start >= 1500) return;
        int n = recv(client, request + used, (int)(sizeof request - used - 1), 0);
        if (n <= 0 || memchr(request + used, '\0', (size_t)n)) return;
        used += (size_t)n;
    }
    char *line = strstr(request, "\r\n");
    *line = '\0';
    int host_headers = 0, local_host = 0;
    for (line += 2; (line[0] != '\r' || line[1] != '\n'); ) {
        char *next = strstr(line, "\r\n");
        if (!next) return;
        *next = '\0';
        if (prefix_ignore_case(line, "Host:", 5) == 0) {
            ++host_headers;
            line += 5 + strspn(line + 5, " \t");
            local_host = strcmp(line, "127.0.0.1:8765") == 0;
        }
        line = next + 2;
    }
    if (host_headers != 1 || !local_host) {
        reply(client, "400 Bad Request", "text/plain", "Bad request", 11);
    } else if (!strcmp(request, "GET /game3-health HTTP/1.1")) {
        reply(client, "200 OK", "text/plain", health, sizeof health - 1);
    } else if (!strcmp(request, "GET /game3-catalogue.html HTTP/1.1")) {
#ifdef _WIN32
        AcquireSRWLockShared(&catalogue->lock);
#endif
        reply(client, "200 OK", "text/html", catalogue->html, catalogue->size);
#ifdef _WIN32
        ReleaseSRWLockShared(&catalogue->lock);
#endif
    } else {
        reply(client, "404 Not Found", "text/plain", "Not found", 9);
    }
}

static int host_catalogue(FILE *file
#ifdef _WIN32
    , const wchar_t *path
#endif
) {
    if (!file) return 2;
    char *html = malloc(LIMIT + 1);
    if (!html) { fclose(file); return 2; }
    size_t size = fread(html, 1, LIMIT + 1, file);
    int ok = !ferror(file) && size && size <= LIMIT;
    fclose(file);
    if (!ok) { free(html); return 2; }

    sock server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == BAD_SOCKET) { free(html); return 3; }
    int option = 1;
#ifdef _WIN32
    int option_name = SO_EXCLUSIVEADDRUSE;
#else
    int option_name = SO_REUSEADDR;
#endif
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (setsockopt(server, SOL_SOCKET, option_name, (const char *)&option, sizeof option) ||
        bind(server, (const struct sockaddr *)&addr, sizeof addr) || listen(server, 8)) {
        close_socket(server); free(html); return 3;
    }
    struct catalogue catalogue = { .html = html, .size = size };
#ifdef _WIN32
    catalogue.path = path;
    InitializeSRWLock(&catalogue.lock);
    HANDLE refresh = CreateThread(NULL, 0, refresh_catalogue, &catalogue, 0, NULL);
#endif
    for (;;) {
        sock client = accept(server, NULL, NULL);
        if (client == BAD_SOCKET) break;
        serve_client(client, &catalogue);
        close_socket(client);
    }
    close_socket(server);
#ifdef _WIN32
    if (refresh) {
        WaitForSingleObject(refresh, INFINITE);
        CloseHandle(refresh);
    }
#endif
    free(catalogue.html);
    return 4;
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show) {
    (void)instance; (void)previous; (void)command; (void)show;
    wchar_t path[MAX_PATH];
    const wchar_t name[] = L"game3-catalogue.html";
    DWORD length = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (!length || length >= MAX_PATH) return 2;
    wchar_t *separator = wcsrchr(path, L'\\');
    if (!separator || (separator - path + 1) + sizeof name / sizeof *name > MAX_PATH) return 2;
    wcscpy(separator + 1, name);
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa)) return 3;
    int result = host_catalogue(_wfopen(path, L"rb"), path);
    WSACleanup();
    return result;
}
#else
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    signal(SIGPIPE, SIG_IGN);
    return host_catalogue(fopen(argv[1], "rb"));
}
#endif
