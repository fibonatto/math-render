#include "parser.h"
#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    Lexer lexer;
    Token current;
} Parser;

static void advance(Parser *parser)
{
    token_free(&parser->current);
    parser->current = lexer_next(&parser->lexer);
}

static Ast *parse_expression(Parser *parser);

static Ast *parse_atom(Parser *parser)
{
    Token *token = &parser->current;

    if (token->type == TOKEN_TEXT) {
        Ast *node = ast_text(token->value);
        advance(parser);
        return node;
    }

	if (token->type == TOKEN_OPERATOR) {
		Ast *node = ast_text(token->value);
		advance(parser);
		return node;
	}

    if (token->type == TOKEN_LBRACE) {
        advance(parser);

        Ast *node = parse_expression(parser);

        if (parser->current.type == TOKEN_RBRACE)
            advance(parser);

        return node;
    }

    if (token->type == TOKEN_LPAREN) {
        Ast *node = ast_sequence();

        ast_add(node, ast_text("("));

        advance(parser);

        Ast *inside = parse_expression(parser);
        ast_add(node, inside);

        if (parser->current.type == TOKEN_RPAREN) {
            ast_add(node, ast_text(")"));
            advance(parser);
        }

        return node;
    }

    if (token->type == TOKEN_COMMAND) {
        const char *command = token->value;

        if (strcmp(command, "frac") == 0) {
            advance(parser);

            Ast *numerator = parse_atom(parser);
            Ast *denominator = parse_atom(parser);

            return ast_binary(
                AST_FRACTION,
                numerator,
                denominator
            );
        }

        if (strcmp(command, "sum") == 0) {
            advance(parser);

            return ast_new(AST_SUM);
        }

        if (strcmp(command, "int") == 0) {
            advance(parser);

            return ast_new(AST_INT);
        }

        if (strcmp(command, "alpha") == 0) {
            advance(parser);
            return ast_text("α");
        }

        if (strcmp(command, "beta") == 0) {
            advance(parser);
            return ast_text("β");
        }

        if (strcmp(command, "gamma") == 0) {
            advance(parser);
            return ast_text("γ");
        }

        if (strcmp(command, "delta") == 0) {
            advance(parser);
            return ast_text("δ");
        }

        if (strcmp(command, "pi") == 0) {
            advance(parser);
            return ast_text("π");
        }

        if (strcmp(command, "infty") == 0) {
            advance(parser);
            return ast_text("∞");
        }

        if (strcmp(command, "leq") == 0) {
            advance(parser);
            return ast_text("≤");
        }

        if (strcmp(command, "geq") == 0) {
            advance(parser);
            return ast_text("≥");
        }

        if (strcmp(command, "neq") == 0) {
            advance(parser);
            return ast_text("≠");
        }

        if (strcmp(command, "times") == 0) {
            advance(parser);
            return ast_text("×");
        }

        if (strcmp(command, "cdot") == 0) {
            advance(parser);
            return ast_text("·");
        }

        if (strcmp(command, "rightarrow") == 0) {
            advance(parser);
            return ast_text("→");
        }

        Ast *node = ast_text(command);
        advance(parser);
        return node;
    }

    return NULL;
}

static Ast *parse_expression(Parser *parser)
{
    Ast *sequence = ast_sequence();

    while (
        parser->current.type != TOKEN_EOF &&
        parser->current.type != TOKEN_RBRACE &&
        parser->current.type != TOKEN_RPAREN
    ) {
        Ast *node = parse_atom(parser);

        if (!node) {
            advance(parser);
            continue;
        }

        if (parser->current.type == TOKEN_CARET) {
            advance(parser);

            Ast *exponent = parse_atom(parser);

            node = ast_binary(
                AST_SUPERSCRIPT,
                node,
                exponent
            );
        }

        if (parser->current.type == TOKEN_UNDERSCORE) {
            advance(parser);

            Ast *subscript = parse_atom(parser);

            node = ast_binary(
                AST_SUBSCRIPT,
                node,
                subscript
            );
        }

        ast_add(sequence, node);
    }

    return sequence;
}

Ast *parse(const char *input)
{
    Parser parser;

    lexer_init(&parser.lexer, input);
    parser.current = lexer_next(&parser.lexer);

    Ast *root = parse_expression(&parser);

    token_free(&parser.current);

    return root;
}
