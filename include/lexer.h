#ifndef MATH_RENDER_LEXER_H
#define MATH_RENDER_LEXER_H

#include <stddef.h>

typedef enum {
    TOKEN_EOF,
    TOKEN_TEXT,
    TOKEN_COMMAND,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_CARET,
    TOKEN_UNDERSCORE,
    TOKEN_OPERATOR
} TokenType;

typedef struct {
    TokenType type;
    char *value;
} Token;

typedef struct {
    const char *input;
    size_t position;
} Lexer;

void lexer_init(Lexer *lexer, const char *input);
Token lexer_next(Lexer *lexer);
void token_free(Token *token);

#endif
