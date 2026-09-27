CC = cc

CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS = -Iinclude
SANITIZERS = -fsanitize=address,undefined

TARGET = math-render
TEST_TARGET = build/test_main
OBJDIR = build/obj

SRC = \
	src/main.c \
	src/lexer.c \
	src/parser.c \
	src/ast.c \
	src/layout.c \
	src/renderer.c

TEST_SRC = \
	src/lexer.c \
	src/parser.c \
	src/ast.c \
	src/layout.c \
	src/renderer.c \
	test_main.c

OBJ = $(SRC:src/%.c=$(OBJDIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@

$(OBJDIR)/%.o: src/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZERS) \
		-o $@ $(TEST_SRC)

clean:
	rm -rf build

install: $(TARGET)
	install -d $(HOME)/.local/bin
	install -m 755 $(TARGET) $(HOME)/.local/bin/math-render

uninstall:
	rm -f $(HOME)/.local/bin/math-render

.PHONY: all test clean install uninstall
