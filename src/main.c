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

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    long size = ftell(file);

    if (size < 0) {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    char *buffer = malloc((size_t)size + 1);

    if (!buffer) {
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, (size_t)size, file);

    if (bytes_read != (size_t)size && ferror(file)) {
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[bytes_read] = '\0';

    fclose(file);

    return buffer;
}

static void usage(const char *program)
{
    fprintf(
        stderr,
        "usage:\n"
        "  %s\n"
        "  %s -f <file>\n"
        "  %s <expression>\n",
        program,
        program,
        program
    );
}

int main(int argc, char **argv)
{
    char *input = NULL;

    if (argc == 1) {
        input = read_stdin();
    } else if (argc == 2) {
        if (strcmp(argv[1], "-f") == 0) {
            fprintf(stderr, "math-render: missing file\n");
            usage(argv[0]);
            return 1;
        }

        if (strcmp(argv[1], "--help") == 0 ||
            strcmp(argv[1], "-h") == 0) {
            usage(argv[0]);
            return 0;
        }

        input = strdup(argv[1]);
    } else if (argc == 3 && strcmp(argv[1], "-f") == 0) {
        input = read_file(argv[2]);
    } else {
        usage(argv[0]);
        return 1;
    }

    if (!input) {
        if (argc >= 2 && strcmp(argv[1], "-f") == 0)
            fprintf(
                stderr,
                "math-render: failed to read file '%s'\n",
                argc >= 3 ? argv[2] : ""
            );
        else
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
