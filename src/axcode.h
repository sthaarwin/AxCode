#ifndef AXCODE_H
#define AXCODE_H

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
#include <stdarg.h>

#define CTRL_KEY(k) ((k) & 0x1f)
#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)
#define HL_HIGHLIGHT_MULTILINE_COMMENT (1<<2)
#define VERSION "0.1"

// Define color pairs
enum editorColors {
    COLOR_DEFAULT = 1,
    COLOR_COMMENT,
    COLOR_KEYWORD,
    COLOR_TYPE,      // For type keywords like "var", "fun", etc.
    COLOR_CONTROL,   // For control flow keywords
    COLOR_NUMBER,
    COLOR_STRING,
    COLOR_MATCH,
    COLOR_BOOLEAN,   // For boolean values (true/false)
    COLOR_OPERATOR,  // For operators
    COLOR_STATUS,
    COLOR_STATUS_MSG,
    COLOR_LINE_NUMBER
};

// Editor modes
enum editorMode {
    MODE_NORMAL,
    MODE_INSERT,
    MODE_COMMAND
};

// Syntax highlighting definitions
struct editorSyntax {
    char *filetype;
    char **filematch;
    char **keywords;
    char **type_keywords;     // For highlighting type-related keywords 
    char **control_keywords;  // For highlighting control flow keywords
    char **operator_patterns; // For highlighting operators
    char *singleline_comment_start;
    char *multiline_comment_start;
    char *multiline_comment_end;
    int flags;
};

typedef struct erow {
    int size;
    char *chars;
    int rsize;
    char *render;
    unsigned char *hl;  // Highlight array
    int hl_open_comment; // Flag for open multiline comments
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
    int mode;           // Editor mode
    WINDOW *win;        // ncurses window
    int dirty;          // Flag to indicate if file has been modified
    int showLineNumbers; // Flag to show line numbers
    struct editorSyntax *syntax; // Current syntax highlight
};

// Global editor state
extern struct editorConfig E;

// Function prototypes

// Editor operations
void initEditor();
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
void editorProcessKeypress();
void editorMoveCursor(int key);
void editorScroll();
void editorProcessCommand(char *command);

// Display functions
void editorDrawRows();
void editorDrawStatusBar();

// File operations
void editorOpen(char *filename);
void editorSave();
char *editorPrompt(char *prompt);

// Row operations
void editorUpdateRow(erow *row);
void editorInsertRow(int at, char *s, size_t len);
void editorAppendRow(char *s, size_t len);
void editorFreeRow(erow *row);
void editorDelRow(int at);
void editorRowInsertChar(erow *row, int at, int c);
void editorRowAppendString(erow *row, char *s, size_t len);
void editorRowDelChar(erow *row, int at);

// Editor actions
void editorInsertChar(int c);
void editorInsertNewline();
void editorDeleteChar();

// Syntax highlighting
void editorUpdateSyntax(erow *row);
void editorSelectSyntaxHighlight();

#endif /* AXCODE_H */