#include "lexer.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *copy_range(const char *start, size_t length)
{
    char *result = malloc(length + 1);

    if (!result)
        return NULL;

    memcpy(result, start, length);
    result[length] = '\0';

    return result;
}

void lexer_init(Lexer *lexer, const char *input)
{
    lexer->input = input;
    lexer->position = 0;
}

static const int char_to_token[256] = {
    ['{'] = TOKEN_LBRACE,
    ['}'] = TOKEN_RBRACE,
    ['('] = TOKEN_LPAREN,
    [')'] = TOKEN_RPAREN,
    ['^'] = TOKEN_CARET,
    ['_'] = TOKEN_UNDERSCORE,
    ['+'] = TOKEN_OPERATOR,
    ['-'] = TOKEN_OPERATOR,
    ['='] = TOKEN_OPERATOR,
    ['*'] = TOKEN_OPERATOR,
    ['/'] = TOKEN_OPERATOR,
    ['<'] = TOKEN_OPERATOR,
    ['>'] = TOKEN_OPERATOR,
    [','] = TOKEN_TEXT,
};

Token lexer_next(Lexer *lexer)
{
    const char *input = lexer->input;
    size_t *position = &lexer->position;

    while (isspace((unsigned char)input[*position]))
        (*position)++;

    unsigned char c = input[*position];

    if (!c)
        return (Token){ TOKEN_EOF, NULL };

    if (c == '\\') {
        size_t start = ++(*position);

        while (isalpha((unsigned char)input[*position]))
            (*position)++;

        return (Token){
            TOKEN_COMMAND,
            copy_range(input + start, *position - start)
        };
    }

    int token_type = char_to_token[c];

    if (token_type != 0) {
        (*position)++;

        char value[2] = { c, '\0' };

        return (Token){
            token_type,
            strdup(value)
        };
    }

    size_t start = *position;

    while (
        input[*position] &&
        !isspace((unsigned char)input[*position]) &&
        input[*position] != '\\' &&
        !char_to_token[(unsigned char)input[*position]]
    ) {
        (*position)++;
    }

    return (Token){
        TOKEN_TEXT,
        copy_range(input + start, *position - start)
    };
}

void token_free(Token *token)
{
    free(token->value);
    token->value = NULL;
}
