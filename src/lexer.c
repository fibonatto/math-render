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

// ASCII to TokenType mapping. Unmapped chars default to 0.
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
};

Token lexer_next(Lexer *lexer)
{
    const char *input = lexer->input;
    size_t *pos = &lexer->position;

    while (isspace((unsigned char) input[*pos]))
        (*pos)++;

    unsigned char c = input[*pos];

    if (!c)
        return (Token) { TOKEN_EOF, NULL };

    // Handle commands
    if (c == '\\') {
        size_t start = ++(*pos);
        
        while (isalpha((unsigned char) input[*pos]))
            (*pos)++;
            
        return (Token) {
            TOKEN_COMMAND,
            copy_range(input + start, *pos - start)
        };
    }

    // Handle single-character tokens
    int token_type = char_to_token[c];
    if (token_type != 0) {
        (*pos)++;
        char value[2] = { c, '\0' };
        return (Token) { token_type, strdup(value) };
    }

    // Handle text tokens
    size_t start = *pos;
    
    while (
        input[*pos] &&
        !isspace((unsigned char) input[*pos]) &&
        input[*pos] != '\\' &&
        !char_to_token[(unsigned char) input[*pos]]
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
