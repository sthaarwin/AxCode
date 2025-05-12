# AxCode

A beautiful, lightweight vi-like text editor written in C with ncurses.

## Features

- **Modal Editing**: Normal, Insert, and Command modes like vi
- **Syntax Highlighting**: Support for C/C++ with extensible framework
- **Line Numbers**: Toggle with `:set number` and `:set nonumber`
- **Vi-like Commands**: Movement with h, j, k, l, and more
- **File Operations**: Open, edit, and save files
- **Library Support**: Can be used as both static and shared library

## Keyboard Shortcuts

### Normal Mode
- `i` - Enter insert mode
- `:` - Enter command mode
- `h`, `j`, `k`, `l` - Move cursor left, down, up, right
- `x` - Delete character under cursor
- `A` - Append at end of line
- `I` - Insert at beginning of line
- `o` - Open new line below
- `O` - Open new line above
- `Ctrl+Q` - Quit
- `Ctrl+S` - Save

### Insert Mode
- `ESC` - Return to normal mode
- Standard text editing

### Command Mode
- `w` - Save file
- `q` - Quit (warns on unsaved changes)
- `q!` - Force quit without saving
- `wq` - Save and quit
- `set number` - Show line numbers
- `set nonumber` - Hide line numbers

## Building

### Requirements
- C Compiler (gcc recommended)
- ncurses library
- make

### Compilation Options

#### Build the executable
```
make
```

#### Build the static library
```
make static
```

#### Build the shared library
```
make shared
```

#### Clean object files
```
make clean
```

#### Complete cleanup
```
make distclean
```

#### Install system-wide
```
sudo make install
```

## Using as a Library

### Static Linking
```c
#include "axcode.h"

// Link with -laxcode
```

### Dynamic Linking
```c
#include "axcode.h"

// Link with -laxcode -ldl
```

## Project Structure

- `src/axcode.h` - Main header file
- `src/editor.c` - Core editor functionality
- `src/file.c` - File operations
- `src/row.c` - Text row manipulation
- `src/syntax.c` - Syntax highlighting
- `src/main.c` - Entry point

## License

Open source software - Use at your own risk.
