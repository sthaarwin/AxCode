#include "axcode.h"

void editorOpen(char *filename) {
    FILE *fp;
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    free(E.filename);
    E.filename = strdup(filename);
    
    fp = fopen(filename, "r");
    if (!fp) {
        editorSetStatusMessage("Cannot open file");
        return;
    }
    
    // Clear existing content
    for (int i = 0; i < E.numrows; i++) {
        editorFreeRow(&E.row[i]);
    }
    free(E.row);
    E.numrows = 0;
    E.row = NULL;
    
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
        editorSelectSyntaxHighlight();
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
    buf[0] = '\0';

    editorSetStatusMessage("%s", prompt);
    
    while (1) {
        editorRefreshScreen();
        int c = wgetch(E.win);
        
        if (c == KEY_ENTER || c == '\n' || c == '\r') {
            if (buflen != 0) {
                editorSetStatusMessage("");
                return buf;
            }
        } else if (c == 27) {  // ESC key
            editorSetStatusMessage("");
            free(buf);
            return NULL;
        } else if (c == 127 || c == KEY_BACKSPACE) {  // Backspace
            if (buflen > 0) {
                buf[--buflen] = '\0';
            }
        } else if (!iscntrl(c) && c < 128) {
            if (buflen == bufsize - 1) {
                bufsize *= 2;
                buf = realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
        }
        
        editorSetStatusMessage("%s%s", prompt, buf);
    }
}