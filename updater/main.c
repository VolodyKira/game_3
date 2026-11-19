#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define LIMIT (128 * 1024)
static const wchar_t remote_path[] =
    L"/repos/VolodyKira/game_3/contents/games-redirect.user.js?ref=arena%2F27e65b2f-game-3";
static const wchar_t filename[] = L"games-redirect.user.js";
static char message[256];
static char remote_script[LIMIT + 1];
static char local_script[LIMIT + 1];

/* This is a format check, not a signature. The source is the configured HTTPS endpoint. */
static int is_expected_script(const char *bytes, DWORD size) {
    static const char begin[] = "// ==UserScript==";
    static const char namespace_key[] = "// @namespace";
    static const char namespace_value[] = "urn:volodykira:game-3";
    if (size <= sizeof(begin) - 1 || memcmp(bytes, begin, sizeof(begin) - 1) || memchr(bytes, '\0', size)) return 0;
    const char *end = strstr(bytes, "\n// ==/UserScript==");
    const char *ns = strstr(bytes, "\n// @namespace");
    if (!end || end - bytes >= 4096 || !ns || ns >= end) return 0;
    ns += sizeof(namespace_key);
    if (*ns != ' ' && *ns != '\t') return 0;
    ns += strspn(ns, " \t");
    if (strncmp(ns, namespace_value, sizeof(namespace_value) - 1)) return 0;
    ns += sizeof(namespace_value) - 1;
    ns += strspn(ns, " \t");
    return *ns == '\r' || *ns == '\n';
}

static int fetch_script(DWORD *size) {
    HINTERNET session = NULL, connection = NULL, request = NULL;
    DWORD status = 0, status_size = sizeof(status), count;
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    int ok = 0;
    *size = 0;
    session = WinHttpOpen(L"Game3-Userscript-Updater/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session || !WinHttpSetTimeouts(session, 5000, 5000, 10000, 10000)) goto winhttp_error;
    connection = WinHttpConnect(session, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connection) goto winhttp_error;
    request = WinHttpOpenRequest(connection, L"GET", remote_path, NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request || !WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect)) ||
        !WinHttpSendRequest(request, L"Accept: application/vnd.github.raw+json\r\nCache-Control: no-cache\r\n", (DWORD)-1L,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request, NULL) ||
        !WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX)) goto winhttp_error;
    if (status != 200) {
        snprintf(message, sizeof(message), "GitHub returned HTTP %lu. Check repository access, branch, or rate limits.", (unsigned long)status);
        goto done;
    }
    do {
        if (!WinHttpReadData(request, remote_script + *size, (DWORD)sizeof(remote_script) - *size, &count)) goto winhttp_error;
        *size += count;
        if (*size > LIMIT) { strcpy(message, "The download is too large. No file changed."); goto done; }
    } while (count != 0);
    if (!is_expected_script(remote_script, *size)) {
        strcpy(message, "The response is not the expected userscript. No file changed.");
        goto done;
    }
    ok = 1;
    goto done;
winhttp_error:
    snprintf(message, sizeof(message), "Could not contact GitHub (Windows error %lu).", (unsigned long)GetLastError());
done:
    if (request) WinHttpCloseHandle(request);
    if (connection) WinHttpCloseHandle(connection);
    if (session) WinHttpCloseHandle(session);
    return ok;
}

/* Returns 1 if found, 0 if absent, and -1 if reading/replacing it would be unsafe. */
static int read_local(const wchar_t *path, DWORD *size) {
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_NOT_FOUND ? 0 : -1;
    DWORD count;
    *size = 0;
    do {
        if (!ReadFile(file, local_script + *size, (DWORD)sizeof(local_script) - *size, &count, NULL)) {
            CloseHandle(file);
            return -1;
        }
        *size += count;
        if (*size > LIMIT) { CloseHandle(file); return -1; }
    } while (count != 0);
    CloseHandle(file);
    return is_expected_script(local_script, *size) ? 1 : -1;
}

static int save_script(const wchar_t *folder, const wchar_t *path, DWORD size) {
    wchar_t temp[MAX_PATH];
    DWORD count = 0;
    if (!GetTempFileNameW(folder, L"g3u", 0, temp)) return 0;
    HANDLE file = CreateFileW(temp, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) { DeleteFileW(temp); return 0; }
    int written = WriteFile(file, remote_script, size, &count, NULL) && count == size && FlushFileBuffers(file);
    CloseHandle(file);
    if (written && MoveFileExW(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return 1;
    DeleteFileW(temp);
    return 0;
}

static int update(void) {
    wchar_t folder[MAX_PATH], path[MAX_PATH];
    DWORD length = GetModuleFileNameW(NULL, folder, MAX_PATH);
    if (!length || length >= MAX_PATH) { strcpy(message, "Could not find the program folder."); return 1; }
    wchar_t *separator = wcsrchr(folder, L'\\');
    if (!separator) { strcpy(message, "Could not find the program folder."); return 1; }
    separator[1] = L'\0';
    if (wcslen(folder) + wcslen(filename) >= MAX_PATH) { strcpy(message, "The program path is too long."); return 1; }
    wcscpy(path, folder);
    wcscat(path, filename);

    DWORD remote_size, local_size;
    if (!fetch_script(&remote_size)) return 1;
    int local = read_local(path, &local_size);
    if (local < 0) { strcpy(message, "The existing file could not be read or is not the expected userscript. Left unchanged."); return 1; }
    if (local && local_size == remote_size && !memcmp(local_script, remote_script, remote_size)) {
        strcpy(message, "Already up to date. Nothing changed.");
        return 0;
    }
    if (!save_script(folder, path, remote_size)) {
        strcpy(message, "Could not save the update. Put the program in a writable folder, such as Downloads.");
        return 1;
    }
    strcpy(message, "Saved games-redirect.user.js next to this program. Import it in Tampermonkey if you did not install it from the GitHub link.");
    return 0;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show) {
    (void)instance;
    (void)previous;
    (void)show;
    const int scheduled = !strcmp(command, "--scheduled");
    if (!scheduled && *command) {
        MessageBoxA(NULL, "Unknown argument. Run without arguments for a manual check.",
            "Game3 Userscript Updater", MB_OK | MB_ICONWARNING);
        return 2;
    }
    int result = update();
    if (!scheduled) MessageBoxA(NULL, message, "Game3 Userscript Updater",
        MB_OK | (result ? MB_ICONWARNING : MB_ICONINFORMATION));
    return result;
}
