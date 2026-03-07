#ifndef FREELANG_TOKEN_H
#define FREELANG_TOKEN_H

#include <stddef.h>

typedef enum {
    // Literals
    TOK_NUMBER = 0,
    TOK_STRING,
    TOK_FSTRING,
    TOK_IDENT,

    // Keywords (28)
    TOK_LET,
    TOK_CONST,
    TOK_VAR,
    TOK_FN,
    TOK_IF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_FOR,
    TOK_IN,
    TOK_RETURN,
    TOK_BREAK,
    TOK_CONTINUE,
    TOK_TRUE,
    TOK_FALSE,
    TOK_NULL,
    TOK_STRUCT,
    TOK_ENUM,
    TOK_TRY,
    TOK_CATCH,
    TOK_FINALLY,
    TOK_THROW,
    TOK_IMPORT,
    TOK_FROM,
    TOK_EXPORT,
    TOK_DEFAULT,
    TOK_AS,
    TOK_ASYNC,
    TOK_AWAIT,
    TOK_MATCH,
    TOK_REACTIVE,       // "reactive" keyword (Phase 4)
    TOK_WATCH,          // "watch" keyword/decorator (Phase 4)
    TOK_TRANSACTION,    // "transaction" keyword/decorator (Phase 4)

    // Operators (26)
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PERCENT,
    TOK_POWER,          // **
    TOK_EQ,             // =
    TOK_EQEQ,           // ==
    TOK_EQEQEQ,         // ===
    TOK_NOT,            // !
    TOK_NOTEQ,          // !=
    TOK_NOTEQUALEQ,     // !==
    TOK_LT,
    TOK_LTEQ,
    TOK_GT,
    TOK_GTEQ,
    TOK_AND,            // &&
    TOK_OR,             // ||
    TOK_AMPERSAND,      // &
    TOK_PIPE,           // |
    TOK_CARET,          // ^
    TOK_TILDE,          // ~
    TOK_LSHIFT,         // <<
    TOK_RSHIFT,         // >>
    TOK_ARROW,          // =>
    TOK_QUESTION,       // ?

    // Punctuation (13)
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_COMMA,
    TOK_DOT,
    TOK_SEMICOLON,
    TOK_COLON,
    TOK_AT,
    TOK_HASH,
    TOK_DOLLAR,

    // Compound assignments
    TOK_PLUS_EQ,
    TOK_MINUS_EQ,
    TOK_STAR_EQ,
    TOK_SLASH_EQ,

    // Special
    TOK_NEWLINE,
    TOK_EOF
} TokenType;

#define TOK_KEYWORD_COUNT 31
#define TOK_TOTAL_COUNT 65

typedef struct {
    TokenType type;
    char *value;        // lexeme text (owned)
    double num_value;   // for TOK_NUMBER
    int line;
    int col;
} Token;

// Token utilities
Token *token_new(TokenType type, const char *value, int line, int col);
void   token_free(Token *tok);
const char *token_type_name(TokenType type);
void   token_print(Token *tok);

#endif // FREELANG_TOKEN_H
