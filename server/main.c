/* Read-only, loopback-only server for the bundled Game3 HTML. */
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
    char header[512];
    int len = snprintf(header, sizeof header,
        "HTTP/1.1 %s\r\nContent-Type: %s; charset=utf-8\r\nContent-Length: %zu\r\n"
        "Connection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n"
        "X-Frame-Options: DENY\r\nReferrer-Policy: no-referrer\r\n\r\n", status, type, size);
    if (len > 0 && (size_t)len < sizeof header && send_all(client, header, (size_t)len)) {
        (void)send_all(client, data, size);
    }
}

static void serve_client(sock client, const char *html, size_t html_size) {
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
        reply(client, "200 OK", "text/html", html, html_size);
    } else {
        reply(client, "404 Not Found", "text/plain", "Not found", 9);
    }
}

static int host_catalogue(FILE *file) {
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
    for (;;) {
        sock client = accept(server, NULL, NULL);
        if (client == BAD_SOCKET) break;
        serve_client(client, html, size);
        close_socket(client);
    }
    close_socket(server);
    free(html);
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
    int result = host_catalogue(_wfopen(path, L"rb"));
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
