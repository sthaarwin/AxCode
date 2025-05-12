#define _GNU_SOURCE
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <stdarg.h>  // Added for va_list support

#define CTRL_KEY(k) ((k) & 0x1f)
#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

// Define color pairs
enum editorColors {
    COLOR_DEFAULT = 1,
    COLOR_COMMENT,
    COLOR_KEYWORD,
    COLOR_NUMBER,
    COLOR_STRING,
    COLOR_MATCH,
    COLOR_STATUS,
    COLOR_STATUS_MSG,
    COLOR_LINE_NUMBER
};

// Syntax highlighting definitions
struct editorSyntax {
    char *filetype;
    char **filematch;
    char **keywords;
    char *singleline_comment_start;
    int flags;
};

// File type definitions
char *C_HL_extensions[] = { ".c", ".h", ".cpp", NULL };
char *C_HL_keywords[] = {
    "switch", "if", "while", "for", "break", "continue", "return", "else",
    "struct", "union", "typedef", "static", "enum", "class", "case", "int|",
    "long|", "double|", "float|", "char|", "unsigned|", "signed|", "void|", NULL
};

// Array of supported file types
struct editorSyntax HLDB[] = {
    {
        "c",
        C_HL_extensions,
        C_HL_keywords,
        "//",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

typedef struct erow {
    int size;
    char *chars;
    int rsize;
    char *render;
    unsigned char *hl;  // Highlight array
} erow;

struct editorConfig {
    int cx, cy;         // Cursor position
    int rx;             // Render position (for tabs)
    int rowoff;         // Row offset for scrolling
    int coloff;         // Column offset for scrolling
    int screenrows;     // Number of rows visible in the terminal
    int screencols;     // Number of columns visible in the terminal
    int numrows;        // Number of rows in the file
    erow *row;          // Array of rows
    char *filename;     // Current filename
    char statusmsg[80]; // Status message
    time_t statusmsg_time; // Time when the status message was set
    int mode;           // Editor mode (0 = normal, 1 = insert)
    WINDOW *win;        // ncurses window
    int dirty;          // Flag to indicate if file has been modified
    int showLineNumbers; // Flag to show line numbers
    struct editorSyntax *syntax; // Current syntax highlight
};

struct editorConfig E;

enum editorMode {
    MODE_NORMAL,
    MODE_INSERT,
    MODE_COMMAND
};

// Function prototypes
void initEditor();
void editorRefreshScreen();
void editorProcessKeypress();
void editorOpen(char *filename);
void editorSave();
void editorInsertChar(int c);
void editorInsertNewline();
void editorDeleteChar();
void editorMoveCursor(int key);
char *editorPrompt(char *prompt);
void editorSetStatusMessage(const char *fmt, ...);
void editorAppendRow(char *s, size_t len);
void editorFreeRow(erow *row);
void editorDelRow(int at);
void editorRowInsertChar(erow *row, int at, int c);
void editorRowDelChar(erow *row, int at);
void editorInsertRow(int at, char *s, size_t len);
void editorScroll();
void editorProcessCommand(char *command);
void editorUpdateSyntax(erow *row);
void editorSelectSyntaxHighlight();

// Syntax highlighting functions
void editorUpdateSyntax(erow *row) {
    if (row->hl) free(row->hl);
    row->hl = malloc(row->rsize);
    memset(row->hl, COLOR_DEFAULT, row->rsize);
    
    if (E.syntax == NULL) return;
    
    // Check for numbers
    if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
        for (int i = 0; i < row->rsize; i++) {
            if (isdigit(row->render[i])) {
                row->hl[i] = COLOR_NUMBER;
            }
        }
    }
    
    // Check for strings
    if (E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
        int in_string = 0;
        char string_char = 0;
        
        for (int i = 0; i < row->rsize; i++) {
            if (in_string) {
                row->hl[i] = COLOR_STRING;
                
                if (row->render[i] == '\\' && i + 1 < row->rsize) {
                    row->hl[i + 1] = COLOR_STRING;
                    i++;
                    continue;
                }
                
                if (row->render[i] == string_char) in_string = 0;
                continue;
            } else {
                if (row->render[i] == '"' || row->render[i] == '\'') {
                    in_string = 1;
                    string_char = row->render[i];
                    row->hl[i] = COLOR_STRING;
                }
            }
        }
    }
    
    // Check for comments
    if (E.syntax->singleline_comment_start) {
        int comment_len = strlen(E.syntax->singleline_comment_start);
        
        for (int i = 0; i < row->rsize - comment_len + 1; i++) {
            if (strncmp(&row->render[i], E.syntax->singleline_comment_start, comment_len) == 0) {
                memset(&row->hl[i], COLOR_COMMENT, row->rsize - i);
                break;
            }
        }
    }
    
    // Check for keywords
    if (E.syntax->keywords) {
        char **keywords = E.syntax->keywords;
        int i = 0;
        
        while (keywords[i]) {
            int klen = strlen(keywords[i]);
            int kw2 = 0;
            
            if (keywords[i][klen-1] == '|') {
                kw2 = 1;
                klen--;
            }
            
            for (int j = 0; j < row->rsize - klen + 1; j++) {
                if ((j == 0 || !isalnum(row->render[j-1])) && 
                    !isalnum(row->render[j+klen]) &&
                    strncmp(&row->render[j], keywords[i], klen) == 0) {
                    
                    memset(&row->hl[j], kw2 ? COLOR_KEYWORD : COLOR_KEYWORD, klen);
                    j += klen;
                }
            }
            i++;
        }
    }
}

void editorSelectSyntaxHighlight() {
    E.syntax = NULL;
    if (E.filename == NULL) return;
    
    char *ext = strrchr(E.filename, '.');
    if (ext) {
        for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
            struct editorSyntax *s = &HLDB[j];
            unsigned int i = 0;
            while (s->filematch[i]) {
                if (strcmp(ext, s->filematch[i]) == 0) {
                    E.syntax = s;
                    
                    // Apply syntax highlighting to all rows
                    for (int filerow = 0; filerow < E.numrows; filerow++) {
                        editorUpdateSyntax(&E.row[filerow]);
                    }
                    return;
                }
                i++;
            }
        }
    }
}

// Row operations
void editorUpdateRow(erow *row) {
    free(row->render);
    row->render = malloc(row->size + 1);
    
    int j = 0;
    for (int i = 0; i < row->size; i++) {
        row->render[j++] = row->chars[i];
    }
    
    row->render[j] = '\0';
    row->rsize = j;
    
    editorUpdateSyntax(row);
}

void editorInsertRow(int at, char *s, size_t len) {
    if (at < 0 || at > E.numrows) return;
    
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
    
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    
    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    editorUpdateRow(&E.row[at]);
    
    E.numrows++;
    E.dirty = 1;
}

void editorAppendRow(char *s, size_t len) {
    editorInsertRow(E.numrows, s, len);
}

void editorFreeRow(erow *row) {
    free(row->render);
    free(row->chars);
}

void editorDelRow(int at) {
    if (at < 0 || at >= E.numrows) return;
    
    editorFreeRow(&E.row[at]);
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
    E.numrows--;
    E.dirty = 1;
}

void editorRowInsertChar(erow *row, int at, int c) {
    if (at < 0 || at > row->size) at = row->size;
    
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;
    
    editorUpdateRow(row);
    E.dirty = 1;
}

void editorRowAppendString(erow *row, char *s, size_t len) {
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    
    editorUpdateRow(row);
    E.dirty = 1;
}

void editorRowDelChar(erow *row, int at) {
    if (at < 0 || at >= row->size) return;
    
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    
    editorUpdateRow(row);
    E.dirty = 1;
}

// Editor operations
void editorInsertChar(int c) {
    if (E.cy == E.numrows) {
        editorAppendRow("", 0);
    }
    
    editorRowInsertChar(&E.row[E.cy], E.cx, c);
    E.cx++;
}

void editorInsertNewline() {
    if (E.cx == 0) {
        editorInsertRow(E.cy, "", 0);
    } else {
        erow *row = &E.row[E.cy];
        editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
        row = &E.row[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
    }
    
    E.cy++;
    E.cx = 0;
}

void editorDeleteChar() {
    if (E.cy == E.numrows) return;
    if (E.cx == 0 && E.cy == 0) return;
    
    erow *row = &E.row[E.cy];
    if (E.cx > 0) {
        editorRowDelChar(row, E.cx - 1);
        E.cx--;
    } else {
        E.cx = E.row[E.cy - 1].size;
        editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
        editorDelRow(E.cy);
        E.cy--;
    }
}

// Editor initialization
void initEditor() {
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 0;
    E.row = NULL;
    E.filename = NULL;
    E.statusmsg[0] = '\0';
    E.mode = MODE_NORMAL;
    E.dirty = 0;
    E.showLineNumbers = 1; // Enable line numbers by default
    E.syntax = NULL;

    // Initialize ncurses
    E.win = initscr();
    raw();
    keypad(E.win, TRUE);
    noecho();
    start_color();
    
    // Set up color pairs
    init_pair(COLOR_DEFAULT, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLOR_COMMENT, COLOR_BLUE, COLOR_BLACK);
    init_pair(COLOR_KEYWORD, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(COLOR_NUMBER, COLOR_CYAN, COLOR_BLACK);
    init_pair(COLOR_STRING, COLOR_GREEN, COLOR_BLACK);
    init_pair(COLOR_MATCH, COLOR_RED, COLOR_BLACK);
    init_pair(COLOR_STATUS, COLOR_BLACK, COLOR_WHITE);
    init_pair(COLOR_STATUS_MSG, COLOR_YELLOW, COLOR_BLACK);
    init_pair(COLOR_LINE_NUMBER, COLOR_BLACK, COLOR_WHITE);
    
    // Get terminal size
    getmaxyx(E.win, E.screenrows, E.screencols);
    E.screenrows -= 2; // Make room for status bar
}

// Display functions
void editorDrawRows() {
    int y;
    for (y = 0; y < E.screenrows; y++) {
        int filerow = y + E.rowoff;
        
        // Line numbers display (if enabled)
        int lineNumWidth = 0;
        if (E.showLineNumbers && E.numrows > 0) {
            lineNumWidth = 6; // Width for line numbers: 4 digits + 1 space + 1 separator
            if (filerow < E.numrows) {
                wattron(E.win, COLOR_PAIR(COLOR_LINE_NUMBER));
                mvwprintw(E.win, y, 0, "%4d ", filerow + 1);
                wattroff(E.win, COLOR_PAIR(COLOR_LINE_NUMBER));
                mvwaddch(E.win, y, 5, '|');
            } else {
                mvwprintw(E.win, y, 0, "     |");
            }
        }
        
        if (filerow >= E.numrows) {
            if (E.numrows == 0 && y == E.screenrows / 3) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome),
                    "AxCode editor -- version 0.1");
                if (welcomelen > E.screencols - lineNumWidth) 
                    welcomelen = E.screencols - lineNumWidth;
                
                // Center the welcome message
                int padding = (E.screencols - lineNumWidth - welcomelen) / 2;
                if (padding) {
                    if (!E.showLineNumbers) {
                        mvwaddch(E.win, y, 0, '~');
                        padding--;
                    }
                }
                while (padding-- > 0) 
                    mvwaddch(E.win, y, lineNumWidth + padding, ' ');
                
                mvwprintw(E.win, y, lineNumWidth + (E.screencols - lineNumWidth - welcomelen) / 2, "%s", welcome);
            } else {
                if (!E.showLineNumbers)
                    mvwaddch(E.win, y, 0, '~');
            }
        } else {
            int len = E.row[filerow].size - E.coloff;
            if (len < 0) len = 0;
            if (len > E.screencols - lineNumWidth) 
                len = E.screencols - lineNumWidth;
            
            // Apply syntax highlighting based on file type
            if (E.syntax) {
                unsigned char *hl = E.row[filerow].hl;
                int current_color = COLOR_DEFAULT;
                
                for (int j = 0; j < len; j++) {
                    char c = E.row[filerow].render[j + E.coloff];
                    unsigned char color = hl ? hl[j + E.coloff] : COLOR_DEFAULT;
                    
                    if (color != current_color) {
                        current_color = color;
                        wattron(E.win, COLOR_PAIR(current_color));
                    }
                    
                    mvwaddch(E.win, y, j + lineNumWidth, c);
                }
                wattroff(E.win, COLOR_PAIR(current_color));
            } else {
                mvwprintw(E.win, y, lineNumWidth, "%.*s", len, &E.row[filerow].render[E.coloff]);
            }
        }
        wclrtoeol(E.win);
    }
}

void editorDrawStatusBar() {
    wattron(E.win, A_REVERSE);
    
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
        E.filename ? E.filename : "[No Name]", E.numrows,
        E.mode == MODE_INSERT ? "[INSERT]" : E.mode == MODE_COMMAND ? "[COMMAND]" : "[NORMAL]");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d,%d", E.cy + 1, E.cx + 1);
    
    if (len > E.screencols) len = E.screencols;
    mvwprintw(E.win, E.screenrows, 0, "%s", status);
    
    while (len < E.screencols) {
        if (E.screencols - len == rlen) {
            mvwprintw(E.win, E.screenrows, len, "%s", rstatus);
            break;
        } else {
            mvwaddch(E.win, E.screenrows, len, ' ');
            len++;
        }
    }
    
    wattroff(E.win, A_REVERSE);
    mvwprintw(E.win, E.screenrows + 1, 0, "%s", E.statusmsg);
    wclrtoeol(E.win);
}

// Scrolling functions
void editorScroll() {
    // Vertical scrolling
    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenrows) {
        E.rowoff = E.cy - E.screenrows + 1;
    }
    
    // Horizontal scrolling
    if (E.cx < E.coloff) {
        E.coloff = E.cx;
    }
    if (E.cx >= E.coloff + E.screencols) {
        E.coloff = E.cx - E.screencols + 1;
    }
}

void editorRefreshScreen() {
    editorScroll();
    
    wclear(E.win);
    editorDrawRows();
    editorDrawStatusBar();
    wmove(E.win, E.cy - E.rowoff, E.cx - E.coloff);
    wrefresh(E.win);
}

// Editor movement
void editorMoveCursor(int key) {
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    
    switch(key) {
        case 'h':
            if (E.cx > 0) {
                E.cx--;
            } else if (E.cy > 0) {
                // Move to end of previous line
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;
        case 'j':
            if (E.cy < E.numrows - 1) {
                E.cy++;
                // Ensure cursor doesn't go beyond the end of the line
                if (row && E.cx > E.row[E.cy].size) {
                    E.cx = E.row[E.cy].size;
                }
            }
            break;
        case 'k':
            if (E.cy > 0) {
                E.cy--;
                // Ensure cursor doesn't go beyond the end of the line
                if (row && E.cx > E.row[E.cy].size) {
                    E.cx = E.row[E.cy].size;
                }
            }
            break;
        case 'l':
            if (row && E.cx < row->size) {
                E.cx++;
            } else if (row && E.cx == row->size && E.cy < E.numrows - 1) {
                // Move to beginning of next line
                E.cy++;
                E.cx = 0;
            }
            break;
    }
    
    // If we moved to a different line, update row
    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    
    // Make sure E.cx is within bounds for the new row
    int rowlen = row ? row->size : 0;
    if (E.cx > rowlen) {
        E.cx = rowlen;
    }
}

// Command handling
void editorProcessCommand(char *command) {
    if (strcmp(command, "w") == 0) {
        // Write/save command
        editorSave();
    } else if (strcmp(command, "q") == 0) {
        // Quit command
        if (E.dirty) {
            editorSetStatusMessage("WARNING: File has unsaved changes. Use :q! to force quit.");
            return;
        }
        endwin();
        exit(0);
    } else if (strcmp(command, "q!") == 0) {
        // Force quit command
        endwin();
        exit(0);
    } else if (strcmp(command, "wq") == 0) {
        // Write and quit command
        editorSave();
        if (!E.dirty) {
            endwin();
            exit(0);
        }
    } else {
        editorSetStatusMessage("Unknown command: %s", command);
    }
}

// Basic operations
void editorProcessKeypress() {
    static char cmdBuffer[128] = {0};
    static int cmdPos = 0;
    
    int c = wgetch(E.win);
    
    switch (E.mode) {
        case MODE_NORMAL:
            switch (c) {
                case 'i':
                    E.mode = MODE_INSERT;
                    break;
                case ':':
                    E.mode = MODE_COMMAND;
                    cmdBuffer[0] = ':';
                    cmdBuffer[1] = '\0';
                    cmdPos = 1;
                    editorSetStatusMessage("%s", cmdBuffer);
                    break;
                case 'h':
                case 'j':
                case 'k':
                case 'l':
                    editorMoveCursor(c);
                    break;
                case 'x':
                    // Delete character under cursor (like 'x' in vi)
                    if (E.cy < E.numrows && E.cx < E.row[E.cy].size) {
                        editorRowDelChar(&E.row[E.cy], E.cx);
                    }
                    break;
                case 'A':
                    // Append at end of line
                    if (E.cy < E.numrows)
                        E.cx = E.row[E.cy].size;
                    E.mode = MODE_INSERT;
                    break;
                case 'I':
                    // Insert at beginning of line
                    E.cx = 0;
                    E.mode = MODE_INSERT;
                    break;
                case 'o':
                    // Open new line below
                    if (E.cy < E.numrows)
                        E.cx = E.row[E.cy].size;
                    else
                        E.cx = 0;
                    editorInsertNewline();
                    E.mode = MODE_INSERT;
                    break;
                case 'O':
                    // Open new line above
                    E.cx = 0;
                    editorInsertRow(E.cy, "", 0);
                    E.mode = MODE_INSERT;
                    break;
                case CTRL_KEY('q'):
                    // Clean up and exit
                    if (E.dirty) {
                        editorSetStatusMessage("WARNING: File has unsaved changes. Press Ctrl-Q again to quit.");
                        E.dirty = 2;  // Special value to indicate we've warned once
                        break;
                    }
                    if (E.dirty == 2) {
                        endwin();
                        exit(0);
                    }
                    endwin();
                    exit(0);
                    break;
                case CTRL_KEY('s'):
                    // Save file
                    editorSave();
                    break;
            }
            break;
            
        case MODE_INSERT:
            if (c == 27) {  // ESC key
                E.mode = MODE_NORMAL;
            } else if (c == '\r') {
                editorInsertNewline();
            } else if (c == 127 || c == KEY_BACKSPACE) {  // Backspace key
                editorDeleteChar();
            } else if (!iscntrl(c)) {
                editorInsertChar(c);
            }
            break;
            
        case MODE_COMMAND:
            if (c == 27) {  // ESC key
                E.mode = MODE_NORMAL;
                editorSetStatusMessage("");
                cmdBuffer[0] = '\0';
                cmdPos = 0;
            } else if (c == '\r') {  // Enter key
                cmdBuffer[cmdPos] = '\0';
                if (cmdPos > 1) {  // We have a command (more than just ':')
                    editorProcessCommand(cmdBuffer + 1);  // Skip the ':' character
                }
                E.mode = MODE_NORMAL;
                cmdBuffer[0] = '\0';
                cmdPos = 0;
            } else if (c == 127 || c == KEY_BACKSPACE) {  // Backspace key
                if (cmdPos > 1) {  // Don't delete the ':' character
                    cmdPos--;
                    cmdBuffer[cmdPos] = '\0';
                    editorSetStatusMessage("%s", cmdBuffer);
                } else if (cmdPos == 1) {
                    E.mode = MODE_NORMAL;
                    cmdBuffer[0] = '\0';
                    cmdPos = 0;
                    editorSetStatusMessage("");
                }
            } else if (!iscntrl(c) && cmdPos < sizeof(cmdBuffer) - 1) {
                cmdBuffer[cmdPos++] = c;
                cmdBuffer[cmdPos] = '\0';
                editorSetStatusMessage("%s", cmdBuffer);
            }
            break;
    }
}

// Main function
int main(int argc, char *argv[]) {
    initEditor();
    
    if (argc >= 2) {
        editorOpen(argv[1]);
    }
    
    // Set initial status message
    editorSetStatusMessage("HELP: Ctrl-Q = quit");
    
    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    
    return 0;
}

// File operations
void editorOpen(char *filename) {
    FILE *fp;
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    E.filename = strdup(filename);
    
    fp = fopen(filename, "r");
    if (!fp) {
        editorSetStatusMessage("Cannot open file");
        return;
    }
    
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
            linelen--;
        editorAppendRow(line, linelen);
    }
    free(line);
    fclose(fp);
    E.dirty = 0;
    editorSelectSyntaxHighlight();
}

void editorSave() {
    if (E.filename == NULL) {
        E.filename = editorPrompt("Save as: ");
        if (E.filename == NULL) {
            editorSetStatusMessage("Save aborted");
            return;
        }
    }
    
    FILE *fp = fopen(E.filename, "w");
    if (!fp) {
        editorSetStatusMessage("Cannot save file");
        return;
    }
    
    for (int i = 0; i < E.numrows; i++) {
        fwrite(E.row[i].chars, 1, E.row[i].size, fp);
        fwrite("\n", 1, 1, fp);
    }
    
    fclose(fp);
    E.dirty = 0;
    editorSetStatusMessage("File saved");
}

char *editorPrompt(char *prompt) {
    size_t bufsize = 128;
    char *buf = malloc(bufsize);
    size_t buflen = 0;

    editorSetStatusMessage("%s", prompt);
    while (1) {
        editorRefreshScreen();
        int c = wgetch(E.win);
        if (c == '\r') {
            if (buflen != 0) {
                buf[buflen] = '\0';
                editorSetStatusMessage("");
                return buf;
            }
        } else if (c == 27) {
            editorSetStatusMessage("");
            free(buf);
            return NULL;
        } else if (!iscntrl(c) && c < 128) {
            if (buflen == bufsize - 1) {
                bufsize *= 2;
                buf = realloc(buf, bufsize);
            }
            buf[buflen++] = c;
        }
    }
}

void editorSetStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}