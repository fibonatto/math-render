CC = cc

CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS = -Iinclude

TARGET = math-render
# TARGET = build/math-render
OBJDIR = build/obj

SRC = \
	src/main.c \
	src/lexer.c \
	src/parser.c \
	src/ast.c \
	src/layout.c \
	src/renderer.c

OBJ = $(SRC:src/%.c=$(OBJDIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@

$(OBJDIR)/%.o: src/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build

install: $(TARGET)
	install -d $(HOME)/.local/bin
	install -m 755 $(TARGET) $(HOME)/.local/bin/math-render

uninstall:
	rm -f $(HOME)/.local/bin/math-render

.PHONY: all clean install uninstall
