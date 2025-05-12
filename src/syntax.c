#include "axcode.h"

// File type definitions
char *C_HL_extensions[] = { ".c", ".h", ".cpp", NULL };
char *C_HL_keywords[] = {
    "switch", "if", "while", "for", "break", "continue", "return", "else",
    "struct", "union", "typedef", "static", "enum", "class", "case", "int|",
    "long|", "double|", "float|", "char|", "unsigned|", "signed|", "void|", NULL
};

// AxScript language definitions
char *AXSCRIPT_HL_extensions[] = { ".axp", NULL };

// Regular keywords for AxScript
char *AXSCRIPT_HL_keywords[] = {
    "print", "input", "loop", "break", "continue", "return", "fun", "to", "step", "down", NULL
};

// Type-related keywords for AxScript
char *AXSCRIPT_HL_types[] = {
    "var", NULL
};

// Control flow keywords for AxScript
char *AXSCRIPT_HL_control[] = {
    "compeq", "compneq", "compge", "comple", "compg", "compl", 
    "if", "else", NULL
};

// Boolean and logical operators
char *AXSCRIPT_HL_operators[] = {
    "and", "or", "not", "true|", "false|", NULL
};

// Array of supported file types
struct editorSyntax HLDB[] = {
    {
        "c",
        C_HL_extensions,
        C_HL_keywords,
        NULL, // No type keywords for C
        NULL, // No control keywords for C
        NULL, // No operator patterns for C
        "//",
        "/*",
        "*/",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_MULTILINE_COMMENT
    },
    {
        "axscript",
        AXSCRIPT_HL_extensions,
        AXSCRIPT_HL_keywords,
        AXSCRIPT_HL_types,
        AXSCRIPT_HL_control,
        AXSCRIPT_HL_operators,
        "//",
        "/*",
        "*/",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_MULTILINE_COMMENT
    }
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

void editorUpdateSyntax(erow *row) {
    if (row->hl) free(row->hl);
    row->hl = malloc(row->rsize);
    memset(row->hl, COLOR_DEFAULT, row->rsize);
    
    if (E.syntax == NULL) return;
    
    char **keywords = E.syntax->keywords;
    char **type_keywords = E.syntax->type_keywords;
    char **control_keywords = E.syntax->control_keywords;
    char **operator_patterns = E.syntax->operator_patterns;
    char *singleline_comment_start = E.syntax->singleline_comment_start;
    char *multiline_comment_start = E.syntax->multiline_comment_start;
    char *multiline_comment_end = E.syntax->multiline_comment_end;
    
    int prev_sep = 1; // True if previous character was a separator
    int in_string = 0; // Inside a string
    int in_comment = row->hl_open_comment; // Inside a comment that continues from previous line
    
    // If this row continues a comment from the previous line
    if (row->hl_open_comment && multiline_comment_end) {
        row->hl_open_comment = 0;
        int in_ml_comment = 1;
        
        for (int i = 0; i < row->rsize; i++) {
            if (in_ml_comment) {
                row->hl[i] = COLOR_COMMENT;
                
                // Check for end of multiline comment
                if (strncmp(&row->render[i], multiline_comment_end, 
                          strlen(multiline_comment_end)) == 0) {
                    for (unsigned int j = 0; j < strlen(multiline_comment_end); j++) {
                        row->hl[i+j] = COLOR_COMMENT;
                    }
                    i += strlen(multiline_comment_end) - 1;
                    in_ml_comment = 0;
                    prev_sep = 1;
                    continue;
                }
            } else {
                // Process other syntax elements...
                break;
            }
        }
        
        // If we reached end of line and still in comment
        if (in_ml_comment) {
            row->hl_open_comment = 1;
        }
    }
    
    int i = 0;
    while (i < row->rsize) {
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->hl[i-1] : COLOR_DEFAULT;
        
        // Handle comments (skip if we're in a string)
        if (singleline_comment_start && !in_string && !in_comment &&
            strncmp(&row->render[i], singleline_comment_start, 
                   strlen(singleline_comment_start)) == 0) {
            // This is a comment until end of line
            memset(&row->hl[i], COLOR_COMMENT, row->rsize - i);
            break;
        }
        
        // Handle multiline comments
        if (multiline_comment_start && multiline_comment_end && !in_string && !in_comment) {
            if (strncmp(&row->render[i], multiline_comment_start,
                       strlen(multiline_comment_start)) == 0) {
                in_comment = 1;
                for (unsigned int j = 0; j < strlen(multiline_comment_start); j++) {
                    row->hl[i+j] = COLOR_COMMENT;
                }
                i += strlen(multiline_comment_start);
                continue;
            }
        }
        
        if (in_comment) {
            row->hl[i] = COLOR_COMMENT;
            
            if (multiline_comment_end && strncmp(&row->render[i], multiline_comment_end,
                                            strlen(multiline_comment_end)) == 0) {
                for (unsigned int j = 0; j < strlen(multiline_comment_end); j++) {
                    row->hl[i+j] = COLOR_COMMENT;
                }
                i += strlen(multiline_comment_end);
                in_comment = 0;
                prev_sep = 1;
                continue;
            }
            i++;
            continue;
        }
        
        // Handle strings
        if (in_string) {
            row->hl[i] = COLOR_STRING;
            if (c == '\\' && i + 1 < row->rsize) {
                row->hl[i+1] = COLOR_STRING;
                i += 2;
                continue;
            }
            if (c == in_string) in_string = 0;
            i++;
            prev_sep = 1;
            continue;
        } else if (c == '"' || c == '\'') {
            in_string = c;
            row->hl[i] = COLOR_STRING;
            i++;
            continue;
        }
        
        // Handle numbers (if flag is set)
        if ((E.syntax->flags & HL_HIGHLIGHT_NUMBERS) &&
            (isdigit(c) || (c == '.' && i+1 < row->rsize && isdigit(row->render[i+1]))) &&
            (prev_sep || prev_hl == COLOR_NUMBER)) {
            row->hl[i] = COLOR_NUMBER;
            i++;
            prev_sep = 0;
            continue;
        }
        
        // Check if current character is a separator
        if (strchr(",.()+-/*=~%<>[];{}", c) != NULL || isspace(c)) {
            prev_sep = 1;
        } else {
            prev_sep = 0;
        }
        
        // Check for operator patterns (AxScript specific)
        if (operator_patterns) {
            int is_op = 0;
            for (int j = 0; operator_patterns[j]; j++) {
                if (strncmp(&row->render[i], operator_patterns[j], strlen(operator_patterns[j])) == 0 &&
                    (i == 0 || strchr(",.()+-/*=~%<>[];{} \t\n", row->render[i-1]) != NULL)) {
                    
                    int len = strlen(operator_patterns[j]);
                    int is_boolean = 0;
                    
                    if (len > 0 && operator_patterns[j][len-1] == '|') {
                        is_boolean = 1;
                        len--;
                    }
                    
                    for (int k = 0; k < len; k++) {
                        row->hl[i+k] = is_boolean ? COLOR_BOOLEAN : COLOR_OPERATOR;
                    }
                    
                    i += len;
                    is_op = 1;
                    break;
                }
            }
            if (is_op) continue;
        }
        
        // Check for keywords, types, and control keywords
        if (prev_sep) {
            // Regular keywords
            if (keywords) {
                for (int j = 0; keywords[j]; j++) {
                    int klen = strlen(keywords[j]);
                    int kw2 = 0;
                    
                    if (keywords[j][klen-1] == '|') {
                        kw2 = 1;
                        klen--;
                    }
                    
                    if (strncmp(&row->render[i], keywords[j], klen) == 0 &&
                        (i + klen == row->rsize || 
                         strchr(",.()+-/*=~%<>[];{} \t\n", row->render[i+klen]) != NULL)) {
                        
                        for (int k = 0; k < klen; k++) {
                            row->hl[i+k] = kw2 ? COLOR_TYPE : COLOR_KEYWORD;
                        }
                        i += klen;
                        break;
                    }
                }
            }
            
            // Type keywords
            if (type_keywords) {
                for (int j = 0; type_keywords[j]; j++) {
                    int klen = strlen(type_keywords[j]);
                    
                    if (strncmp(&row->render[i], type_keywords[j], klen) == 0 &&
                        (i + klen == row->rsize || 
                         strchr(",.()+-/*=~%<>[];{} \t\n", row->render[i+klen]) != NULL)) {
                        
                        for (int k = 0; k < klen; k++) {
                            row->hl[i+k] = COLOR_TYPE;
                        }
                        i += klen;
                        break;
                    }
                }
            }
            
            // Control keywords
            if (control_keywords) {
                for (int j = 0; control_keywords[j]; j++) {
                    int klen = strlen(control_keywords[j]);
                    
                    if (strncmp(&row->render[i], control_keywords[j], klen) == 0 &&
                        (i + klen == row->rsize || 
                         strchr(",.()+-/*=~%<>[];{} \t\n", row->render[i+klen]) != NULL)) {
                        
                        for (int k = 0; k < klen; k++) {
                            row->hl[i+k] = COLOR_CONTROL;
                        }
                        i += klen;
                        break;
                    }
                }
            }
            
            if (i < row->rsize) {
                row->hl[i] = COLOR_DEFAULT;
                i++;
                continue;
            }
        }
        
        // Default coloring for other characters
        row->hl[i] = COLOR_DEFAULT;
        i++;
    }
    
    // Update open comment status for the next row
    if (in_comment && multiline_comment_end) {
        row->hl_open_comment = 1;
    } else {
        row->hl_open_comment = 0;
    }
}

void editorSelectSyntaxHighlight() {
    E.syntax = NULL;
    if (E.filename == NULL) return;
    
    char *ext = strrchr(E.filename, '.');
    
    for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
        struct editorSyntax *s = &HLDB[j];
        unsigned int i = 0;
        
        while (s->filematch[i]) {
            int is_ext = (s->filematch[i][0] == '.');
            
            if ((is_ext && ext && strcmp(ext, s->filematch[i]) == 0) ||
                (!is_ext && strstr(E.filename, s->filematch[i]))) {
                E.syntax = s;
                
                // Apply syntax highlighting to all rows
                for (int filerow = 0; filerow < E.numrows; filerow++) {
                    editorUpdateRow(&E.row[filerow]);
                }
                return;
            }
            i++;
        }
    }
}