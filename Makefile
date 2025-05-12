CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99
LDFLAGS = -lncurses

SRC_DIR = src
BIN_DIR = bin
LIB_DIR = lib
TARGET = $(BIN_DIR)/axcode
STATIC_LIB = $(LIB_DIR)/libaxcode.a
SHARED_LIB = $(LIB_DIR)/libaxcode.so

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(SRC:.c=.o)
# Exclude main.o for library builds
LIB_OBJ = $(filter-out $(SRC_DIR)/main.o, $(OBJ))

.PHONY: all clean static shared install distclean

all: $(TARGET)
	@echo "Cleaning object files..."
	@$(MAKE) clean-obj

# Build the executable
$(TARGET): $(OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build static library
static: $(STATIC_LIB)
	@echo "Cleaning object files..."
	@$(MAKE) clean-obj

$(STATIC_LIB): $(LIB_OBJ) | $(LIB_DIR)
	ar rcs $@ $^

# Build shared library
shared: $(SHARED_LIB)
	@echo "Cleaning object files..."
	@$(MAKE) clean-obj

$(SHARED_LIB): $(LIB_OBJ) | $(LIB_DIR)
	$(CC) -shared -o $@ $^

# Common compilation rule for all .c files
%.o: %.c
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(LIB_DIR):
	mkdir -p $(LIB_DIR)

install: all
	install -m 755 $(TARGET) /usr/local/bin/

# Only clean object files
clean-obj:
	rm -f $(SRC_DIR)/*.o
	
# Clean everything (objects and binaries)
clean: clean-obj
	rm -f $(TARGET) $(STATIC_LIB) $(SHARED_LIB)

# Complete cleanup including generated directories
distclean: clean
	rm -rf $(BIN_DIR) $(LIB_DIR)