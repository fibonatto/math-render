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

Token lexer_next(Lexer *lexer)
{
    const char *input = lexer->input;
    size_t *pos = &lexer->position;

    while (isspace((unsigned char) input[*pos]))
        (*pos)++;

    char c = input[*pos];

    if (!c)
        return (Token) { TOKEN_EOF, NULL };

    if (c == '{') {
        (*pos)++;
        return (Token) { TOKEN_LBRACE, strdup("{") };
    }

    if (c == '}') {
        (*pos)++;
        return (Token) { TOKEN_RBRACE, strdup("}") };
    }

    if (c == '(') {
        (*pos)++;
        return (Token) { TOKEN_LPAREN, strdup("(") };
    }

    if (c == ')') {
        (*pos)++;
        return (Token) { TOKEN_RPAREN, strdup(")") };
    }

    if (c == '^') {
        (*pos)++;
        return (Token) { TOKEN_CARET, strdup("^") };
    }

    if (c == '_') {
        (*pos)++;
        return (Token) { TOKEN_UNDERSCORE, strdup("_") };
    }

    if (strchr("+-=*/<>", c)) {
        (*pos)++;
        char value[2] = { c, '\0' };
        return (Token) { TOKEN_OPERATOR, strdup(value) };
    }

    if (c == '\\') {
        size_t start = ++(*pos);

        while (isalpha((unsigned char) input[*pos]))
            (*pos)++;

        return (Token) {
            TOKEN_COMMAND,
            copy_range(input + start, *pos - start)
        };
    }

    size_t start = *pos;

    while (
        input[*pos] &&
        !isspace((unsigned char) input[*pos]) &&
        !strchr("{}()^_\\+-=*/<>", input[*pos])
    ) {
        (*pos)++;
    }

    return (Token) {
        TOKEN_TEXT,
        copy_range(input + start, *pos - start)
    };
}

void token_free(Token *token)
{
    free(token->value);
    token->value = NULL;
}
