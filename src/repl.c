#ifndef BST_PLATFORM_WINDOWS
#define _POSIX_C_SOURCE 200809L
#endif

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<signal.h>
#ifndef BST_PLATFORM_WINDOWS
#include<setjmp.h>
#include<sys/select.h>
#include<sys/ioctl.h>
#include<termios.h>
#include<unistd.h>
#endif
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "eval.h"
#include "line_input.h"
#include "script.h"
#include "value.h"

/* ---------- Bestiary REPL: lex -> parse -> eval ---------- */

static int opt_dump_tokens = 0;
static int opt_dump_ast    = 0;
static int opt_plain_input = 0;

typedef enum {
    REPL_STATE_IDLE = 0,
    REPL_STATE_READLINE,
    REPL_STATE_EXEC
} ReplState;

static volatile sig_atomic_t g_sigint_seen = 0;
static volatile sig_atomic_t g_repl_state = REPL_STATE_IDLE;
#ifndef BST_PLATFORM_WINDOWS
static volatile sig_atomic_t g_sigint_from_readline = 0;
static sigjmp_buf g_repl_jmp;
#endif

static char* g_active_line = NULL;
static Token* g_active_toks = NULL;
static size_t g_active_ntok = 0;
static AstNode* g_active_ast = NULL;

/* ---------- Helper methods ---------- */

// Map a TokenKind to a printable tag
static const char* kindName(TokenKind k) {
    switch (k) {
        case TOK_IDENT:        return "IDENT";
        case TOK_NUMBER:       return "NUMBER";
        case TOK_DECIMAL:      return "DECIMAL";
        case TOK_EQUALS:       return "EQUALS";
        case TOK_LBRACE:       return "LBRACE";
        case TOK_RBRACE:       return "RBRACE";
        case TOK_COMMAND:      return "COMMAND";
        case TOK_AMP:          return "AMP";
        case TOK_DBLBACKSLASH: return "DBLBACKSLASH";
        case TOK_PLUS:         return "PLUS";
        case TOK_MINUS:        return "MINUS";
        case TOK_STAR:         return "STAR";
        case TOK_SLASH:        return "SLASH";
        case TOK_LPAREN:       return "LPAREN";
        case TOK_RPAREN:       return "RPAREN";
        case TOK_COMMA:        return "COMMA";
        case TOK_DOT:          return "DOT";
        case TOK_CARET:        return "CARET";
        case TOK_UNDERSCORE:   return "UNDERSCORE";
        case TOK_COLON:        return "COLON";
        case TOK_SEMICOLON:    return "SEMICOLON";
        case TOK_PIPE:         return "PIPE";
        case TOK_LESS:         return "LESS";
        case TOK_GREATER:      return "GREATER";
        case TOK_PRIME:        return "PRIME";
        case TOK_DOLLAR:       return "DOLLAR";
        case TOK_PERCENT:      return "PERCENT";
        case TOK_TILDE:        return "TILDE";
        case TOK_HASH:         return "HASH";
        case TOK_LBRACK:       return "LBRACK";
        case TOK_RBRACK:       return "RBRACK";
        case TOK_EOF:          return "EOF";
        default:               return "?";
    }
}

// Print the token stream in a readable form
static void dumpTokens(Token* toks, size_t n) {
    for (size_t i = 0; i < n; i++) {
        const char* text = toks[i].text ? toks[i].text : "";
        printf("  [%2zu] %-13s line=%zu col=%-3zu text=\"%s\"\n",
               i, kindName(toks[i].kind), toks[i].line, toks[i].col, text);
    }
}

// Free token strings and the token array itself
static void freeTokens(Token* toks, size_t n) {
    for (size_t i = 0; i < n; i++) free(toks[i].text);
    free(toks);
}

static void clearActiveObjects(void) {
    if (g_active_ast) {
        astFree(g_active_ast);
        g_active_ast = NULL;
    }
    if (g_active_toks) {
        freeTokens(g_active_toks, g_active_ntok);
        g_active_toks = NULL;
        g_active_ntok = 0;
    }
    if (g_active_line) {
        free(g_active_line);
        g_active_line = NULL;
    }
}

static int updateBraceDepth(const char* line, size_t* depth) {
    int inString = 0;
    for (const char* p = line; p && *p; p++) {
        if (*p == '"') {
            inString = !inString;
            continue;
        }
        if (inString) continue;
        if (*p == '{') {
            (*depth)++;
        } else if (*p == '}') {
            if (*depth == 0) return 0;
            (*depth)--;
        }
    }
    return 1;
}

static int appendText(char** text, size_t* len, size_t* cap, const char* add) {
    size_t n = strlen(add);
    if (*len + n + 1 > *cap) {
        size_t nextCap = *cap ? *cap * 2 : 256;
        while (nextCap < *len + n + 1) nextCap *= 2;
        char* next = realloc(*text, nextCap);
        if (!next) return 0;
        *text = next;
        *cap = nextCap;
    }
    memcpy(*text + *len, add, n);
    *len += n;
    (*text)[*len] = '\0';
    return 1;
}

static char* indentationText(size_t depth) {
    size_t spaces = depth * 4;
    char* indent = malloc(spaces + 1);
    if (!indent) return NULL;
    memset(indent, ' ', spaces);
    indent[spaces] = '\0';
    return indent;
}

typedef struct {
    char** lines;
    size_t len;
    size_t cap;
} ReplLineBlock;

static char* dupLine(const char* line) {
    size_t n = strlen(line ? line : "");
    char* out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, line ? line : "", n + 1);
    return out;
}

static void freeLineBlock(ReplLineBlock* block) {
    if (!block) return;
    for (size_t i = 0; i < block->len; i++) free(block->lines[i]);
    free(block->lines);
    block->lines = NULL;
    block->len = 0;
    block->cap = 0;
}

static int pushBlockLine(ReplLineBlock* block, const char* line) {
    if (block->len == block->cap) {
        size_t nextCap = block->cap ? block->cap * 2 : 4;
        char** next = realloc(block->lines, nextCap * sizeof(char*));
        if (!next) return 0;
        block->lines = next;
        block->cap = nextCap;
    }
    block->lines[block->len] = dupLine(line);
    if (!block->lines[block->len]) return 0;
    block->len++;
    return 1;
}

static int insertBlockLine(ReplLineBlock* block, size_t index, const char* line) {
    if (!block || index > block->len) return 0;
    if (block->len == block->cap) {
        size_t nextCap = block->cap ? block->cap * 2 : 4;
        char** next = realloc(block->lines, nextCap * sizeof(char*));
        if (!next) return 0;
        block->lines = next;
        block->cap = nextCap;
    }
    char* copy = dupLine(line);
    if (!copy) return 0;
    memmove(block->lines + index + 1, block->lines + index,
            (block->len - index) * sizeof(char*));
    block->lines[index] = copy;
    block->len++;
    return 1;
}

static int replaceBlockLine(ReplLineBlock* block, size_t index, const char* line) {
    if (!block || index >= block->len) return 0;
    char* copy = dupLine(line);
    if (!copy) return 0;
    free(block->lines[index]);
    block->lines[index] = copy;
    return 1;
}

static int resizeBlockLine(ReplLineBlock* block, size_t index, size_t len) {
    if (!block || index >= block->len) return 0;
    char* line = block->lines[index];
    char* resized = realloc(line, len + 1);
    if (!resized) return 0;
    resized[len] = '\0';
    block->lines[index] = resized;
    return 1;
}

static int insertCharInBlockLine(ReplLineBlock* block, size_t index, size_t col, char ch) {
    if (!block || index >= block->len) return 0;
    char* line = block->lines[index];
    size_t len = strlen(line);
    if (col > len) col = len;
    char* resized = realloc(line, len + 2);
    if (!resized) return 0;
    memmove(resized + col + 1, resized + col, len - col + 1);
    resized[col] = ch;
    block->lines[index] = resized;
    return 1;
}

static int deleteCharInBlockLine(ReplLineBlock* block, size_t index, size_t col) {
    if (!block || index >= block->len) return 0;
    char* line = block->lines[index];
    size_t len = strlen(line);
    if (col >= len) return 0;
    memmove(line + col, line + col + 1, len - col);
    return 1;
}

static int recomputeBlockDepth(const ReplLineBlock* block, size_t* depth) {
    size_t nextDepth = 0;
    for (size_t i = 0; block && i < block->len; i++) {
        if (!updateBraceDepth(block->lines[i], &nextDepth)) return 0;
    }
    if (depth) *depth = nextDepth;
    return 1;
}

static char* joinLineBlock(const ReplLineBlock* block) {
    char* text = NULL;
    size_t len = 0;
    size_t cap = 0;
    for (size_t i = 0; block && i < block->len; i++) {
        if (i > 0 && !appendText(&text, &len, &cap, "\n")) {
            free(text);
            return NULL;
        }
        if (!appendText(&text, &len, &cap, block->lines[i])) {
            free(text);
            return NULL;
        }
    }
    return text ? text : dupLine("");
}

static size_t leadingSpaceLen(const char* line) {
    size_t n = 0;
    while (line && line[n] == ' ') n++;
    return n;
}

static int lineIsOnlySpaces(const char* line) {
    for (const char* p = line; p && *p; p++) {
        if (*p != ' ') return 0;
    }
    return 1;
}

#ifndef BST_PLATFORM_WINDOWS
typedef struct {
    ReplLineBlock block;
    size_t current;
    size_t cursor;
    size_t lastRows;
    size_t lastCursorRow;
    int cols;
} ReplBlockEditor;

typedef enum {
    EDIT_KEY_NONE = 0,
    EDIT_KEY_CTRL_C,
    EDIT_KEY_CTRL_D,
    EDIT_KEY_ENTER,
    EDIT_KEY_BACKSPACE,
    EDIT_KEY_DELETE,
    EDIT_KEY_UP,
    EDIT_KEY_DOWN,
    EDIT_KEY_RIGHT,
    EDIT_KEY_LEFT,
    EDIT_KEY_HOME,
    EDIT_KEY_END,
    EDIT_KEY_PRINTABLE
} ReplEditKey;

typedef struct {
    ReplEditKey kind;
    char ch;
} ReplEditInput;

static void writeAll(int fd, const char* text, size_t len) {
    while (len > 0) {
        ssize_t n = write(fd, text, len);
        if (n <= 0) return;
        text += n;
        len -= (size_t)n;
    }
}

static void writeStr(const char* text) {
    writeAll(STDOUT_FILENO, text, strlen(text));
}

static void cursorUp(size_t n) {
    if (n == 0) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "\033[%zuA", n);
    writeStr(buf);
}

static void cursorDown(size_t n) {
    if (n == 0) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "\033[%zuB", n);
    writeStr(buf);
}

static void cursorRight(size_t n) {
    if (n == 0) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "\033[%zuC", n);
    writeStr(buf);
}

static int terminalColumns(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) return ws.ws_col;
    return 80;
}

static size_t visualRows(size_t width, int cols) {
    if (cols < 1) cols = 80;
    return width / (size_t)cols + 1;
}

static size_t editorLineRows(const char* line, int cols) {
    return visualRows(3 + strlen(line ? line : ""), cols);
}

static size_t editorTotalRows(const ReplBlockEditor* editor) {
    size_t rows = 0;
    for (size_t i = 1; i < editor->block.len; i++) {
        rows += editorLineRows(editor->block.lines[i], editor->cols);
    }
    return rows ? rows : 1;
}

static size_t editorCursorRow(const ReplBlockEditor* editor) {
    size_t row = 0;
    for (size_t i = 1; i < editor->current; i++) {
        row += editorLineRows(editor->block.lines[i], editor->cols);
    }
    row += (3 + editor->cursor) / (size_t)editor->cols;
    return row;
}

static size_t editorCursorCol(const ReplBlockEditor* editor) {
    return (3 + editor->cursor) % (size_t)editor->cols;
}

static void renderBlockEditor(ReplBlockEditor* editor) {
    editor->cols = terminalColumns();

    if (editor->lastRows > 0) {
        writeStr("\r");
        cursorUp(editor->lastCursorRow);
        writeStr("\033[J");
    }

    for (size_t i = 1; i < editor->block.len; i++) {
        if (i > 1) writeStr("\r\n");
        writeStr("...");
        writeStr(editor->block.lines[i] ? editor->block.lines[i] : "");
    }

    size_t totalRows = editorTotalRows(editor);
    size_t cursorRow = editorCursorRow(editor);
    size_t cursorCol = editorCursorCol(editor);
    if (totalRows > cursorRow + 1) cursorUp(totalRows - cursorRow - 1);
    writeStr("\r");
    cursorRight(cursorCol);
    editor->lastRows = totalRows;
    editor->lastCursorRow = cursorRow;
    fflush(stdout);
}

static int readByteWithTimeout(unsigned char* out, int timeoutMs) {
    fd_set set;
    struct timeval tv;
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    int ready = select(STDIN_FILENO + 1, &set, NULL, NULL, &tv);
    if (ready <= 0) return 0;
    return read(STDIN_FILENO, out, 1) == 1;
}

static ReplEditInput readEditInput(void) {
    unsigned char c = 0;
    ReplEditInput input = { EDIT_KEY_NONE, 0 };
    if (read(STDIN_FILENO, &c, 1) != 1) return input;

    if (c == 3) {
        input.kind = EDIT_KEY_CTRL_C;
    } else if (c == 4) {
        input.kind = EDIT_KEY_CTRL_D;
    } else if (c == '\r' || c == '\n') {
        input.kind = EDIT_KEY_ENTER;
    } else if (c == 127 || c == '\b') {
        input.kind = EDIT_KEY_BACKSPACE;
    } else if (c == 1) {
        input.kind = EDIT_KEY_HOME;
    } else if (c == 5) {
        input.kind = EDIT_KEY_END;
    } else if (c == 0x1b) {
        unsigned char a = 0;
        unsigned char b = 0;
        if (!readByteWithTimeout(&a, 30)) return input;
        if (a == '[' || a == 'O') {
            if (!readByteWithTimeout(&b, 30)) return input;
            if (b == 'A') input.kind = EDIT_KEY_UP;
            else if (b == 'B') input.kind = EDIT_KEY_DOWN;
            else if (b == 'C') input.kind = EDIT_KEY_RIGHT;
            else if (b == 'D') input.kind = EDIT_KEY_LEFT;
            else if (b == 'H') input.kind = EDIT_KEY_HOME;
            else if (b == 'F') input.kind = EDIT_KEY_END;
            else if (b == '3') {
                unsigned char tilde = 0;
                if (readByteWithTimeout(&tilde, 30) && tilde == '~') input.kind = EDIT_KEY_DELETE;
            }
        }
    } else if (c >= 32) {
        input.kind = EDIT_KEY_PRINTABLE;
        input.ch = (char)c;
    }
    return input;
}

static size_t protectedLineLen(const ReplBlockEditor* editor) {
    if (!editor || editor->current == 0 || editor->current >= editor->block.len) return 0;
    return leadingSpaceLen(editor->block.lines[editor->current]);
}

static void clampEditorCursor(ReplBlockEditor* editor) {
    size_t len = strlen(editor->block.lines[editor->current]);
    size_t protectedLen = protectedLineLen(editor);
    if (editor->cursor > len) editor->cursor = len;
    if (editor->cursor < protectedLen) editor->cursor = protectedLen;
}

static void editorBell(void) {
    writeStr("\a");
}

static int editorInsertPrintable(ReplBlockEditor* editor, char ch) {
    char* line = editor->block.lines[editor->current];
    if (ch == '}' && lineIsOnlySpaces(line)) {
        if (!replaceBlockLine(&editor->block, editor->current, "}")) return 0;
        editor->cursor = 1;
        return 1;
    }
    if (!insertCharInBlockLine(&editor->block, editor->current, editor->cursor, ch)) return 0;
    editor->cursor++;
    return 1;
}

static int editorBackspace(ReplBlockEditor* editor) {
    size_t protectedLen = protectedLineLen(editor);
    if (editor->cursor > protectedLen) {
        if (!deleteCharInBlockLine(&editor->block, editor->current, editor->cursor - 1)) return 0;
        editor->cursor--;
        return 1;
    }
    if (editor->cursor == 0 && editor->current > 1) {
        size_t prev = editor->current - 1;
        size_t prevLen = strlen(editor->block.lines[prev]);
        char* merged = malloc(prevLen + strlen(editor->block.lines[editor->current]) + 1);
        if (!merged) return 0;
        strcpy(merged, editor->block.lines[prev]);
        strcat(merged, editor->block.lines[editor->current]);
        if (!replaceBlockLine(&editor->block, prev, merged)) {
            free(merged);
            return 0;
        }
        free(merged);
        free(editor->block.lines[editor->current]);
        memmove(editor->block.lines + editor->current,
                editor->block.lines + editor->current + 1,
                (editor->block.len - editor->current - 1) * sizeof(char*));
        editor->block.len--;
        editor->current = prev;
        editor->cursor = prevLen;
        return 1;
    }
    editorBell();
    return 1;
}

static int editorDelete(ReplBlockEditor* editor) {
    size_t len = strlen(editor->block.lines[editor->current]);
    if (editor->cursor < len) {
        return deleteCharInBlockLine(&editor->block, editor->current, editor->cursor);
    }
    if (editor->current + 1 < editor->block.len) {
        size_t nextLen = strlen(editor->block.lines[editor->current + 1]);
        char* merged = malloc(len + nextLen + 1);
        if (!merged) return 0;
        strcpy(merged, editor->block.lines[editor->current]);
        strcat(merged, editor->block.lines[editor->current + 1]);
        if (!replaceBlockLine(&editor->block, editor->current, merged)) {
            free(merged);
            return 0;
        }
        free(merged);
        free(editor->block.lines[editor->current + 1]);
        memmove(editor->block.lines + editor->current + 1,
                editor->block.lines + editor->current + 2,
                (editor->block.len - editor->current - 2) * sizeof(char*));
        editor->block.len--;
        return 1;
    }
    editorBell();
    return 1;
}

static int editorNewline(ReplBlockEditor* editor) {
    size_t depth = 0;
    if (!recomputeBlockDepth(&editor->block, &depth)) {
        editorBell();
        return 1;
    }
    if (depth == 0) return 2;

    char* line = editor->block.lines[editor->current];
    size_t len = strlen(line);
    if (editor->cursor > len) editor->cursor = len;

    char* indent = indentationText(depth);
    if (!indent) return 0;
    size_t indentLen = strlen(indent);
    size_t tailLen = len - editor->cursor;
    char* next = malloc(indentLen + tailLen + 1);
    if (!next) {
        free(indent);
        return 0;
    }
    memcpy(next, indent, indentLen);
    memcpy(next + indentLen, line + editor->cursor, tailLen + 1);
    if (!resizeBlockLine(&editor->block, editor->current, editor->cursor)) {
        free(indent);
        free(next);
        return 0;
    }
    int ok = insertBlockLine(&editor->block, editor->current + 1, next);
    free(indent);
    free(next);
    if (!ok) return 0;
    editor->current++;
    editor->cursor = indentLen;
    return 1;
}

static char* readBlockWithEditor(const ReplLineBlock* initialBlock, size_t depth) {
    ReplBlockEditor editor = {0};
    struct termios oldTerm;
    struct termios rawTerm;

    if (!initialBlock || initialBlock->len == 0) return NULL;
    for (size_t i = 0; i < initialBlock->len; i++) {
        if (!pushBlockLine(&editor.block, initialBlock->lines[i])) {
            freeLineBlock(&editor.block);
            return NULL;
        }
    }

    char* indent = indentationText(depth);
    if (!indent || !pushBlockLine(&editor.block, indent)) {
        free(indent);
        freeLineBlock(&editor.block);
        return NULL;
    }
    free(indent);

    editor.current = editor.block.len - 1;
    editor.cursor = strlen(editor.block.lines[editor.current]);
    editor.cols = terminalColumns();

    if (tcgetattr(STDIN_FILENO, &oldTerm) != 0) {
        freeLineBlock(&editor.block);
        return NULL;
    }
    rawTerm = oldTerm;
    rawTerm.c_lflag &= (tcflag_t)~(ICANON | ECHO | ISIG);
    rawTerm.c_iflag &= (tcflag_t)~(IXON | ICRNL);
    rawTerm.c_cc[VMIN] = 1;
    rawTerm.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &rawTerm) != 0) {
        freeLineBlock(&editor.block);
        return NULL;
    }

    renderBlockEditor(&editor);
    for (;;) {
        ReplEditInput input = readEditInput();
        int changed = 1;

        if (input.kind == EDIT_KEY_CTRL_C) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldTerm);
            writeStr("\r\033[J\n");
            freeLineBlock(&editor.block);
            return dupLine("");
        } else if (input.kind == EDIT_KEY_CTRL_D) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldTerm);
            freeLineBlock(&editor.block);
            return NULL;
        } else if (input.kind == EDIT_KEY_ENTER) {
            int status = editorNewline(&editor);
            if (status == 0) {
                tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldTerm);
                freeLineBlock(&editor.block);
                return NULL;
            }
            if (status == 2) break;
        } else if (input.kind == EDIT_KEY_BACKSPACE) {
            if (!editorBackspace(&editor)) {
                tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldTerm);
                freeLineBlock(&editor.block);
                return NULL;
            }
        } else if (input.kind == EDIT_KEY_DELETE) {
            if (!editorDelete(&editor)) {
                tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldTerm);
                freeLineBlock(&editor.block);
                return NULL;
            }
        } else if (input.kind == EDIT_KEY_UP) {
            if (editor.current > 1) {
                editor.current--;
                clampEditorCursor(&editor);
            } else {
                editorBell();
            }
        } else if (input.kind == EDIT_KEY_DOWN) {
            if (editor.current + 1 < editor.block.len) {
                editor.current++;
                clampEditorCursor(&editor);
            } else {
                editorBell();
            }
        } else if (input.kind == EDIT_KEY_LEFT) {
            size_t protectedLen = protectedLineLen(&editor);
            if (editor.cursor > protectedLen) {
                editor.cursor--;
            } else if (editor.current > 1) {
                editor.current--;
                editor.cursor = strlen(editor.block.lines[editor.current]);
            } else {
                editorBell();
            }
        } else if (input.kind == EDIT_KEY_RIGHT) {
            size_t len = strlen(editor.block.lines[editor.current]);
            if (editor.cursor < len) {
                editor.cursor++;
            } else if (editor.current + 1 < editor.block.len) {
                editor.current++;
                editor.cursor = protectedLineLen(&editor);
            } else {
                editorBell();
            }
        } else if (input.kind == EDIT_KEY_HOME) {
            editor.cursor = protectedLineLen(&editor);
        } else if (input.kind == EDIT_KEY_END) {
            editor.cursor = strlen(editor.block.lines[editor.current]);
        } else if (input.kind == EDIT_KEY_PRINTABLE) {
            if (!editorInsertPrintable(&editor, input.ch)) {
                tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldTerm);
                freeLineBlock(&editor.block);
                return NULL;
            }
        } else {
            changed = 0;
        }

        if (changed) renderBlockEditor(&editor);
    }

    size_t totalRows = editorTotalRows(&editor);
    size_t cursorRow = editorCursorRow(&editor);
    if (totalRows > cursorRow + 1) cursorDown(totalRows - cursorRow - 1);

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldTerm);
    writeStr("\r\n");
    char* text = joinLineBlock(&editor.block);
    freeLineBlock(&editor.block);
    return text;
}
#endif

static char* readBalancedInput(void) {
    ReplLineBlock block = {0};
    size_t depth = 0;

    char* line = bstReadLine("> ");
    if (!line) return NULL;
    if (!pushBlockLine(&block, line)) {
        free(line);
        return NULL;
    }
    free(line);
    if (!recomputeBlockDepth(&block, &depth)) {
        freeLineBlock(&block);
        return dupLine("");
    }

#ifndef BST_PLATFORM_WINDOWS
    if (depth > 0 && !opt_plain_input && isatty(STDIN_FILENO) && isatty(STDOUT_FILENO)) {
        char* text = readBlockWithEditor(&block, depth);
        freeLineBlock(&block);
        return text;
    }
#endif

    while (depth > 0) {
        char* indent = indentationText(depth);
        BestiaryLineContext context = {
            .initial = indent ? indent : "",
            .protectedLen = indent ? strlen(indent) : 0,
            .blockLines = (const char* const*)block.lines,
            .blockLineCount = block.len
        };
        int selected = -1;
        line = bstReadLineWithContext("...", &context, &selected);
        free(indent);
        if (!line) {
            freeLineBlock(&block);
            return NULL;
        }

        int ok = selected >= 0 && (size_t)selected < block.len
            ? replaceBlockLine(&block, (size_t)selected, line)
            : pushBlockLine(&block, line);
        free(line);
        if (!ok) {
            freeLineBlock(&block);
            return NULL;
        }

        if (!recomputeBlockDepth(&block, &depth)) {
            freeLineBlock(&block);
            return NULL;
        }
    }

    char* text = joinLineBlock(&block);
    freeLineBlock(&block);
    return text;
}

static void handleSigint(int signo) {
    (void)signo;
    g_sigint_seen = 1;
#ifndef BST_PLATFORM_WINDOWS
    if (g_repl_state == REPL_STATE_READLINE) {
        g_sigint_from_readline = 1;
        siglongjmp(g_repl_jmp, 1);
    }
    write(STDOUT_FILENO, "\n", 1);
    siglongjmp(g_repl_jmp, 1);
#else
    putchar('\n');
    fflush(stdout);
#endif
}

/* ---------- Main ---------- */

int main(int argc, char** argv) {
#ifndef BST_PLATFORM_WINDOWS
    struct sigaction sa;
#endif
    const char* script_filename = NULL;

    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "--tokens") == 0) opt_dump_tokens = 1;
        else if (strcmp(argv[i], "--ast")    == 0) opt_dump_ast    = 1;
        else if (strcmp(argv[i], "--plain")  == 0) opt_plain_input = 1;
        else if (!script_filename) script_filename = argv[i];
        else {
            fprintf(stderr, "usage: %s [--tokens] [--ast] [--plain] [script-file]\n", argv[0]);
            return 2;
        }
    }

#ifndef BST_PLATFORM_WINDOWS
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handleSigint;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
#else
    signal(SIGINT, handleSigint);
#endif

    registerBuiltins();
    EvalContext* ctx = evalCtxNew();

    if (script_filename) {
        ScriptRunOptions options = {opt_dump_tokens, opt_dump_ast, 1};
        ScriptRunResult result = {0, 0, 0};
        char error[512] = {0};
        int ok = bstRunScriptFile(ctx, script_filename, &options, &result, error, sizeof(error));
        if (!ok) {
            fprintf(stderr, "%s\n", error[0] ? error : "failed to run script");
            evalCtxFree(ctx);
            return 1;
        }
        evalCtxFree(ctx);
        return result.errors ? 1 : 0;
    }

    bstLineInputInit(NULL);
    bstLineInputSetEvalContext(ctx);
    bstLineInputSetPlainMode(opt_plain_input);

    puts("Bestiary v1.0.2 -- Ctrl+C cancels, Ctrl+D quits, Ctrl+L clears screen");
    puts("\\help lists commands, \\help{commandName} displays command info");
    for (;;) {
#ifndef BST_PLATFORM_WINDOWS
        if (sigsetjmp(g_repl_jmp, 1) != 0) {
            if (g_sigint_from_readline) {
                bstLineInputRecoverInterrupt();
                g_sigint_from_readline = 0;
            }
            clearActiveObjects();
            g_repl_state = REPL_STATE_IDLE;
            g_sigint_seen = 0;
        }
#else
        if (g_sigint_seen) {
            clearActiveObjects();
            g_repl_state = REPL_STATE_IDLE;
            g_sigint_seen = 0;
        }
#endif

        g_repl_state = REPL_STATE_READLINE;
        char* line = readBalancedInput();
        g_active_line = line;
        g_repl_state = REPL_STATE_IDLE;

        if (!line) { putchar('\n'); break; }     // Ctrl+D
        if (*line == '\0') {
            clearActiveObjects();
            continue;
        }
        bstAddHistory(line);

        g_repl_state = REPL_STATE_EXEC;
        size_t ntok = 0;
        Token* toks = bstLex(line, &ntok);
        g_active_toks = toks;
        g_active_ntok = ntok;
        if (opt_dump_tokens) { puts("-- tokens --"); dumpTokens(toks, ntok); }

        ParseError perr = {0};
        AstNode* ast = bstParse(toks, ntok, &perr);
        g_active_ast = ast;
        if (!ast) {
            fprintf(stderr, "parse error at %zu:%zu: %s\n",
                    perr.line, perr.col, perr.msg ? perr.msg : "unknown");
            clearActiveObjects();
            g_repl_state = REPL_STATE_IDLE;
            continue;
        }
        if (opt_dump_ast) { puts("-- ast --"); astPrint(ast, 2); }

        Value r = eval(ctx, ast);
        g_repl_state = REPL_STATE_IDLE;
        fputs("=> ", stdout); valPrint(r); putchar('\n');
        valFree(r);

        clearActiveObjects();
    }

    clearActiveObjects();
    bstLineInputSetEvalContext(NULL);
    bstLineInputShutdown();
    evalCtxFree(ctx);
    return 0;
}
