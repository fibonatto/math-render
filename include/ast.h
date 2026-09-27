#ifndef MATH_RENDER_AST_H
#define MATH_RENDER_AST_H

#include <stddef.h>

typedef enum {
    AST_TEXT,
    AST_GROUP,
    AST_FRACTION,
    AST_BINOM,
    AST_SUPERSCRIPT,
    AST_SUBSCRIPT,
    AST_SUBSUP,
    AST_SUM,
    AST_INT,
    AST_SQRT,
    AST_SEQUENCE
} AstType;

typedef struct Ast Ast;

struct Ast {
    AstType type;

    char *text;

    Ast *left;
    Ast *right;

    Ast **children;
    size_t child_count;
};

Ast *ast_new(AstType type);
Ast *ast_text(const char *text);
Ast *ast_binary(AstType type, Ast *left, Ast *right);
Ast *ast_unary(AstType type, Ast *child);
Ast *ast_sequence(void);

void ast_add(Ast *node, Ast *child);
void ast_free(Ast *node);

#endif
