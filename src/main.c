#include "ast.h"
#include "layout.h"
#include "parser.h"
#include "renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_stdin(void)
{
    size_t capacity = 4096;
    size_t length = 0;

    char *buffer = malloc(capacity);

    if (!buffer)
        return NULL;

    int c;

    while ((c = getchar()) != EOF) {
        if (length + 1 >= capacity) {
            capacity *= 2;

            char *tmp = realloc(buffer, capacity);

            if (!tmp) {
                free(buffer);
                return NULL;
            }

            buffer = tmp;
        }

        buffer[length++] = (char)c;
    }

    buffer[length] = '\0';

    return buffer;
}

static char *read_file(const char *path)
{
    FILE *file = fopen(path, "rb");

    if (!file)
        return NULL;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size < 0) {
        fclose(file);
        return NULL;
    }

    char *buffer = malloc((size_t)size + 1);

    if (!buffer) {
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, (size_t)size, file);
    buffer[size] = '\0';

    fclose(file);

    return buffer;
}

int main(int argc, char **argv)
{
    char *input = NULL;

    if (argc > 2) {
        fprintf(stderr, "usage: math-render [file]\n");
        return 1;
    }

    if (argc == 2)
        input = read_file(argv[1]);
    else
        input = read_stdin();

    if (!input) {
        fprintf(stderr, "math-render: failed to read input\n");
        return 1;
    }

    Ast *ast = parse(input);

    if (!ast) {
        fprintf(stderr, "math-render: parse error\n");
        free(input);
        return 1;
    }

    Box *box = layout(ast);

    if (!box) {
        fprintf(stderr, "math-render: layout error\n");
        ast_free(ast);
        free(input);
        return 1;
    }

    render(box);

    box_free(box);
    ast_free(ast);
    free(input);

    return 0;
}
