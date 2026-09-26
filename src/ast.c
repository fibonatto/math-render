#include "ast.h"

#include <stdlib.h>
#include <string.h>

Ast *ast_new(AstType type)
{
    Ast *node = calloc(1, sizeof(Ast));

    if (!node)
        return NULL;

    node->type = type;
    return node;
}

Ast *ast_text(const char *text)
{
    Ast *node = ast_new(AST_TEXT);

    if (!node)
        return NULL;

    node->text = strdup(text);

    if (!node->text) {
        free(node);
        return NULL;
    }

    return node;
}

Ast *ast_binary(AstType type, Ast *left, Ast *right)
{
    Ast *node = ast_new(type);

    if (!node)
        return NULL;

    node->left = left;
    node->right = right;

    return node;
}

Ast *ast_unary(AstType type, Ast *child)
{
    /* Same shape as ast_binary, just documents that this construct
     * (currently only \sqrt) has one operand, not two -- it's stored
     * in `left`, `right` stays NULL. */
    return ast_binary(type, child, NULL);
}

Ast *ast_sequence(void)
{
    return ast_new(AST_SEQUENCE);
}

void ast_add(Ast *node, Ast *child)
{
    Ast **children = realloc(
        node->children,
        sizeof(Ast *) * (node->child_count + 1)
    );

    if (!children)
        return;

    node->children = children;
    node->children[node->child_count++] = child;
}

void ast_free(Ast *node)
{
    if (!node)
        return;

    free(node->text);

    ast_free(node->left);
    ast_free(node->right);

    for (size_t i = 0; i < node->child_count; i++)
        ast_free(node->children[i]);

    free(node->children);
    free(node);
}
