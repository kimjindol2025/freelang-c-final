#ifndef FREELANG_LEXER_H
#define FREELANG_LEXER_H

#include "token.h"

typedef struct {
    const char *src;
    size_t src_len;
    size_t pos;
    int line;
    int col;

    // Token buffer (changed from Token** to Token* - direct array, not pointers)
    Token *tokens;
    size_t token_count;
    size_t token_capacity;

    // Keyword lookup
    void *keyword_map;  // HashMap<string, TokenType>
} Lexer;

// Lexer operations
Lexer  *lexer_new(const char *source);
void    lexer_free(Lexer *l);

// Tokenization
int     lexer_scan_all(Lexer *l);        // Tokenize entire source
Token  *lexer_next_token(Lexer *l);      // Get next single token
Token  *lexer_peek_token(Lexer *l, int offset);  // Look ahead

// Token buffer access
Token  *lexer_get_token(Lexer *l, size_t index);
size_t  lexer_token_count(Lexer *l);

// Utilities
void    lexer_print_tokens(Lexer *l);    // Debug: print all tokens

#endif // FREELANG_LEXER_H
