#include "linenoise.h"

#include<ctype.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#ifdef _WIN32
#include<conio.h>
#include<io.h>
#include<windows.h>
#endif

#define LINENOISE_MAX_LINE 4096
#define LINENOISE_MAX_HISTORY 128

static linenoiseCompletionCallback* g_completionCallback = NULL;
static char* g_history[LINENOISE_MAX_HISTORY];
static size_t g_historyLen = 0;

static char* lnStrdup(const char* s) {
    size_t len = strlen(s);
    char* out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len + 1);
    return out;
}

static void setLine(char* buf, size_t* len, size_t* pos, const char* value) {
    size_t nextLen = strlen(value);
    if (nextLen >= LINENOISE_MAX_LINE) nextLen = LINENOISE_MAX_LINE - 1;
    memcpy(buf, value, nextLen);
    buf[nextLen] = '\0';
    *len = nextLen;
    *pos = nextLen;
}

static void refreshLine(const char* prompt, const char* buf, size_t len, size_t pos) {
    fputc('\r', stdout);
    fputs(prompt, stdout);
    fwrite(buf, 1, len, stdout);
    fputs("  ", stdout);
    fputc('\r', stdout);
    fputs(prompt, stdout);
    fwrite(buf, 1, pos, stdout);
    fflush(stdout);
}

#ifdef _WIN32
static void clearScreen(void) {
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    DWORD cells = 0;
    DWORD written = 0;
    COORD home = {0, 0};

    if (console == INVALID_HANDLE_VALUE || !GetConsoleScreenBufferInfo(console, &info)) {
        fputs("\033[2J\033[H", stdout);
        fflush(stdout);
        return;
    }

    cells = (DWORD)(info.dwSize.X * info.dwSize.Y);
    FillConsoleOutputCharacterA(console, ' ', cells, home, &written);
    FillConsoleOutputAttribute(console, info.wAttributes, cells, home, &written);
    SetConsoleCursorPosition(console, home);
}
#endif

static void freeCompletions(linenoiseCompletions* lc) {
    for (size_t i = 0; i < lc->len; i++) free(lc->cvec[i]);
    free(lc->cvec);
    lc->cvec = NULL;
    lc->len = 0;
}

static size_t commonPrefixLen(linenoiseCompletions* lc) {
    if (!lc->len) return 0;
    size_t shared = strlen(lc->cvec[0]);
    for (size_t i = 1; i < lc->len; i++) {
        size_t j = 0;
        while (j < shared && lc->cvec[i][j] && lc->cvec[0][j] == lc->cvec[i][j]) j++;
        shared = j;
    }
    return shared;
}

static void completeLine(const char* prompt, char* buf, size_t* len, size_t* pos) {
    if (!g_completionCallback) return;

    linenoiseCompletions lc = {0, NULL};
    g_completionCallback(buf, &lc);
    if (!lc.len) {
        return;
    }

    if (lc.len == 1) {
        setLine(buf, len, pos, lc.cvec[0]);
        refreshLine(prompt, buf, *len, *pos);
        freeCompletions(&lc);
        return;
    }

    size_t shared = commonPrefixLen(&lc);
    if (shared > *len) {
        size_t nextLen = shared;
        if (nextLen >= LINENOISE_MAX_LINE) nextLen = LINENOISE_MAX_LINE - 1;
        memcpy(buf, lc.cvec[0], nextLen);
        buf[nextLen] = '\0';
        *len = nextLen;
        *pos = nextLen;
    }

    fputc('\n', stdout);
    for (size_t i = 0; i < lc.len; i++) puts(lc.cvec[i]);
    refreshLine(prompt, buf, *len, *pos);
    freeCompletions(&lc);
}

static char* readLinePlain(const char* prompt) {
    char stackBuf[1024];
    char* out = NULL;
    size_t len = 0;
    size_t cap = 0;

    if (prompt) {
        fputs(prompt, stdout);
        fflush(stdout);
    }

    for (;;) {
        if (!fgets(stackBuf, sizeof(stackBuf), stdin)) {
            free(out);
            return NULL;
        }

        size_t chunkLen = strlen(stackBuf);
        if (len + chunkLen + 1 > cap) {
            size_t nextCap = cap ? cap * 2 : 1024;
            while (nextCap < len + chunkLen + 1) nextCap *= 2;
            char* next = realloc(out, nextCap);
            if (!next) {
                free(out);
                return NULL;
            }
            out = next;
            cap = nextCap;
        }

        memcpy(out + len, stackBuf, chunkLen);
        len += chunkLen;
        out[len] = '\0';

        if (len > 0 && out[len - 1] == '\n') {
            out[--len] = '\0';
            if (len > 0 && out[len - 1] == '\r') out[--len] = '\0';
            return out;
        }

        if (chunkLen < sizeof(stackBuf) - 1) return out;
    }
}

void linenoiseSetCompletionCallback(linenoiseCompletionCallback* fn) {
    g_completionCallback = fn;
}

void linenoiseAddCompletion(linenoiseCompletions* lc, const char* str) {
    char** next = realloc(lc->cvec, sizeof(char*) * (lc->len + 1));
    if (!next) return;
    lc->cvec = next;
    lc->cvec[lc->len] = lnStrdup(str);
    if (lc->cvec[lc->len]) lc->len++;
}

char* linenoise(const char* prompt) {
    char buf[LINENOISE_MAX_LINE];
    size_t len = 0;
    size_t pos = 0;
    size_t historyPos = g_historyLen;
    prompt = prompt ? prompt : "";
    buf[0] = '\0';

#ifdef _WIN32
    if (!_isatty(_fileno(stdin))) return readLinePlain(prompt);
#else
    return readLinePlain(prompt);
#endif

    fputs(prompt, stdout);
    fflush(stdout);

    for (;;) {
        int c = _getch();
        if (c == 3) return NULL;
        if (c == 4 && len == 0) return NULL;
        if (c == 12) {
            clearScreen();
            refreshLine(prompt, buf, len, pos);
            continue;
        }
        if (c == '\r' || c == '\n') {
            fputc('\n', stdout);
            return lnStrdup(buf);
        }
        if (c == '\t') {
            completeLine(prompt, buf, &len, &pos);
            continue;
        }
        if (c == '\b' || c == 127) {
            if (pos > 0) {
                memmove(buf + pos - 1, buf + pos, len - pos + 1);
                pos--;
                len--;
                refreshLine(prompt, buf, len, pos);
            }
            continue;
        }
        if (c == 0 || c == 224) {
            int key = _getch();
            if (key == 75 && pos > 0) {
                pos--;
            } else if (key == 77 && pos < len) {
                pos++;
            } else if (key == 71) {
                pos = 0;
            } else if (key == 79) {
                pos = len;
            } else if (key == 83 && pos < len) {
                memmove(buf + pos, buf + pos + 1, len - pos);
                len--;
                refreshLine(prompt, buf, len, pos);
                continue;
            } else if (key == 72 && historyPos > 0) {
                historyPos--;
                setLine(buf, &len, &pos, g_history[historyPos]);
            } else if (key == 80 && historyPos < g_historyLen) {
                historyPos++;
                if (historyPos == g_historyLen) setLine(buf, &len, &pos, "");
                else setLine(buf, &len, &pos, g_history[historyPos]);
            }
            refreshLine(prompt, buf, len, pos);
            continue;
        }
        if (isprint((unsigned char)c) && len + 1 < LINENOISE_MAX_LINE) {
            memmove(buf + pos + 1, buf + pos, len - pos + 1);
            buf[pos++] = (char)c;
            len++;
            refreshLine(prompt, buf, len, pos);
        }
    }
}

int linenoiseHistoryAdd(const char* line) {
    if (!line || !*line) return 0;
    if (g_historyLen && strcmp(g_history[g_historyLen - 1], line) == 0) return 1;
    char* copy = lnStrdup(line);
    if (!copy) return 0;
    if (g_historyLen == LINENOISE_MAX_HISTORY) {
        free(g_history[0]);
        memmove(g_history, g_history + 1, sizeof(char*) * (LINENOISE_MAX_HISTORY - 1));
        g_historyLen--;
    }
    g_history[g_historyLen++] = copy;
    return 1;
}

void linenoiseFree(void* ptr) {
    free(ptr);
}
