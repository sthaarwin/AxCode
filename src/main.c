#include "axcode.h"

int main(int argc, char *argv[]) {
    // Initialize editor
    initEditor();
    
    // If filename provided, open it
    if (argc >= 2) {
        editorOpen(argv[1]);
    }
    
    // Set initial status message
    editorSetStatusMessage("HELP: Ctrl-Q = quit | Ctrl-S = save | ESC = normal mode");
    
    // Main editor loop
    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    
    return 0;
}