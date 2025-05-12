#include "axcode.h"

// Global editor config
struct editorConfig E;

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
    E.row[at].hl = NULL;
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
    free(row->hl);
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