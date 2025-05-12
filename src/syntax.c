#include "axcode.h"

// File type definitions
char *C_HL_extensions[] = { ".c", ".h", ".cpp", NULL };
char *C_HL_keywords[] = {
    "print", "if", "while", "for", "break", "continue", "return", "else",
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
                    (j + klen == row->rsize || !isalnum(row->render[j+klen])) &&
                    strncmp(&row->render[j], keywords[i], klen) == 0) {
                    
                    memset(&row->hl[j], kw2 ? COLOR_KEYWORD : COLOR_KEYWORD, klen);
                    j += klen - 1;
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