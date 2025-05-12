#include "axcode.h"

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
    init_pair(COLOR_TYPE, COLOR_GREEN, COLOR_BLACK);
    init_pair(COLOR_CONTROL, COLOR_YELLOW, COLOR_BLACK);
    init_pair(COLOR_NUMBER, COLOR_CYAN, COLOR_BLACK);
    init_pair(COLOR_STRING, COLOR_GREEN, COLOR_BLACK);
    init_pair(COLOR_MATCH, COLOR_RED, COLOR_BLACK);
    init_pair(COLOR_BOOLEAN, COLOR_YELLOW, COLOR_BLACK);
    init_pair(COLOR_OPERATOR, COLOR_MAGENTA, COLOR_BLACK);
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
                    "AxCode editor -- version %s", VERSION);
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
    int len = snprintf(status, sizeof(status), "%.20s %s - %d lines %s",
        E.filename ? E.filename : "[No Name]",
        E.dirty ? "(modified)" : "",
        E.numrows,
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
    
    // Calculate the correct cursor position accounting for line numbers
    int lineNumWidth = (E.showLineNumbers && E.numrows > 0) ? 6 : 0;
    wmove(E.win, E.cy - E.rowoff, E.cx - E.coloff + lineNumWidth);
    
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

// Editor actions
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
    } else if (strcmp(command, "set nonumber") == 0) {
        // Turn off line numbers
        E.showLineNumbers = 0;
        editorSetStatusMessage("Line numbers disabled");
    } else if (strcmp(command, "set number") == 0) {
        // Turn on line numbers
        E.showLineNumbers = 1;
        editorSetStatusMessage("Line numbers enabled");
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
            } else if (c == KEY_ENTER || c == '\n' || c == '\r') {
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
            } else if (c == KEY_ENTER || c == '\n' || c == '\r') {  // Enter key - fixing to detect multiple variants
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
            } else if (!iscntrl(c) && cmdPos < (int)sizeof(cmdBuffer) - 1) {
                cmdBuffer[cmdPos++] = c;
                cmdBuffer[cmdPos] = '\0';
                editorSetStatusMessage("%s", cmdBuffer);
            }
            break;
    }
}

void editorSetStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}