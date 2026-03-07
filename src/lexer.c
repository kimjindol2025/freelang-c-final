#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "lexer.h"

#define INITIAL_TOKEN_CAPACITY 256

typedef struct {
    const char *keyword;
    TokenType type;
} KeywordEntry;

static KeywordEntry keywords[] = {
    { "let", TOK_LET },
    { "const", TOK_CONST },
    { "var", TOK_VAR },
    { "fn", TOK_FN },
    { "if", TOK_IF },
    { "else", TOK_ELSE },
    { "while", TOK_WHILE },
    { "for", TOK_FOR },
    { "in", TOK_IN },
    { "return", TOK_RETURN },
    { "break", TOK_BREAK },
    { "continue", TOK_CONTINUE },
    { "true", TOK_TRUE },
    { "false", TOK_FALSE },
    { "null", TOK_NULL },
    { "struct", TOK_STRUCT },
    { "enum", TOK_ENUM },
    { "try", TOK_TRY },
    { "catch", TOK_CATCH },
    { "finally", TOK_FINALLY },
    { "throw", TOK_THROW },
    { "import", TOK_IMPORT },
    { "from", TOK_FROM },
    { "export", TOK_EXPORT },
    { "default", TOK_DEFAULT },
    { "as", TOK_AS },
    { "async", TOK_ASYNC },
    { "await", TOK_AWAIT },
    { "match", TOK_MATCH },
    { "reactive", TOK_REACTIVE },    // Phase 4: MOSS-State
    { "watch", TOK_WATCH },          // Phase 4: MOSS-State
    { "transaction", TOK_TRANSACTION }, // Phase 4: MOSS-State
    { NULL, 0 }
};

static TokenType lookup_keyword(const char *str, size_t len) {
    char buffer[256];
    if (len >= 256) return TOK_IDENT;

    strncpy(buffer, str, len);
    buffer[len] = '\0';

    for (int i = 0; keywords[i].keyword != NULL; i++) {
        if (strcmp(buffer, keywords[i].keyword) == 0) {
            return keywords[i].type;
        }
    }
    return TOK_IDENT;
}

Lexer *lexer_new(const char *source) {
    if (!source) return NULL;

    Lexer *l = (Lexer *)malloc(sizeof(Lexer));
    if (!l) return NULL;

    l->src = source;
    l->src_len = strlen(source);
    l->pos = 0;
    l->line = 1;
    l->col = 1;

    l->tokens = (Token *)malloc(sizeof(Token) * INITIAL_TOKEN_CAPACITY);
    if (!l->tokens) {
        free(l);
        return NULL;
    }

    l->token_count = 0;
    l->token_capacity = INITIAL_TOKEN_CAPACITY;
    l->keyword_map = NULL;  // Not using for now, using linear search

    return l;
}

void lexer_free(Lexer *l) {
    if (!l) return;

    for (size_t i = 0; i < l->token_count; i++) {
        if (l->tokens[i].value) {
            free(l->tokens[i].value);
        }
    }
    free(l->tokens);
    free(l);
}

static void lexer_add_token(Lexer *l, Token *tok) {
    if (l->token_count >= l->token_capacity) {
        l->token_capacity *= 2;
        Token *new_tokens = (Token *)realloc(l->tokens, sizeof(Token) * l->token_capacity);
        if (!new_tokens) return;
        l->tokens = new_tokens;
    }

    // Copy token struct (value pointer is now owned by lexer)
    l->tokens[l->token_count++] = *tok;
    // Free the temporary token struct (but NOT its value, which lexer now owns)
    free(tok);
}

static char lexer_current(Lexer *l) {
    if (l->pos >= l->src_len) return '\0';
    return l->src[l->pos];
}

static char lexer_peek(Lexer *l, int offset) {
    if (l->pos + offset >= l->src_len) return '\0';
    return l->src[l->pos + offset];
}

static void lexer_advance(Lexer *l) {
    if (l->pos < l->src_len) {
        if (l->src[l->pos] == '\n') {
            l->line++;
            l->col = 1;
        } else {
            l->col++;
        }
        l->pos++;
    }
}

static void lexer_skip_whitespace(Lexer *l) {
    while (lexer_current(l) && isspace(lexer_current(l))) {
        if (lexer_current(l) == '\n') {
            // Could emit NEWLINE token here if needed
        }
        lexer_advance(l);
    }
}

static void lexer_skip_line_comment(Lexer *l) {
    // Skip //
    lexer_advance(l);
    lexer_advance(l);
    while (lexer_current(l) && lexer_current(l) != '\n') {
        lexer_advance(l);
    }
}

static void lexer_skip_block_comment(Lexer *l) {
    // Skip /*
    lexer_advance(l);
    lexer_advance(l);

    while (l->pos < l->src_len) {
        if (lexer_current(l) == '*' && lexer_peek(l, 1) == '/') {
            lexer_advance(l);
            lexer_advance(l);
            break;
        }
        lexer_advance(l);
    }
}

static Token *lexer_read_string(Lexer *l, char quote_char) {
    int start_line = l->line;
    int start_col = l->col;
    lexer_advance(l);  // skip opening quote

    char buffer[4096];
    size_t buffer_len = 0;

    while (lexer_current(l) && lexer_current(l) != quote_char) {
        if (lexer_current(l) == '\\' && lexer_peek(l, 1)) {
            lexer_advance(l);
            char escaped = lexer_current(l);
            switch (escaped) {
                case 'n': buffer[buffer_len++] = '\n'; break;
                case 't': buffer[buffer_len++] = '\t'; break;
                case 'r': buffer[buffer_len++] = '\r'; break;
                case '\\': buffer[buffer_len++] = '\\'; break;
                case '"': buffer[buffer_len++] = '"'; break;
                case '\'': buffer[buffer_len++] = '\''; break;
                default: buffer[buffer_len++] = escaped;
            }
        } else {
            buffer[buffer_len++] = lexer_current(l);
        }
        lexer_advance(l);
    }
    buffer[buffer_len] = '\0';

    if (lexer_current(l) == quote_char) {
        lexer_advance(l);  // skip closing quote
    }

    Token *tok = token_new(TOK_STRING, buffer, start_line, start_col);
    return tok;
}

static Token *lexer_read_fstring(Lexer *l) {
    int start_line = l->line;
    int start_col = l->col;
    lexer_advance(l);  // skip 'f'
    lexer_advance(l);  // skip opening quote

    char buffer[4096];
    size_t buffer_len = 0;

    while (lexer_current(l) && lexer_current(l) != '"') {
        if (lexer_current(l) == '{') {
            buffer[buffer_len++] = '{';
            lexer_advance(l);
            // Read expression until }
            int depth = 1;
            while (depth > 0 && lexer_current(l)) {
                if (lexer_current(l) == '{') depth++;
                else if (lexer_current(l) == '}') depth--;
                buffer[buffer_len++] = lexer_current(l);
                lexer_advance(l);
            }
        } else {
            buffer[buffer_len++] = lexer_current(l);
            lexer_advance(l);
        }
    }
    buffer[buffer_len] = '\0';

    if (lexer_current(l) == '"') {
        lexer_advance(l);
    }

    Token *tok = token_new(TOK_FSTRING, buffer, start_line, start_col);
    return tok;
}

static Token *lexer_read_number(Lexer *l) {
    int start_line = l->line;
    int start_col = l->col;

    char buffer[256];
    size_t len = 0;

    while (isdigit(lexer_current(l))) {
        buffer[len++] = lexer_current(l);
        lexer_advance(l);
    }

    if (lexer_current(l) == '.' && isdigit(lexer_peek(l, 1))) {
        buffer[len++] = '.';
        lexer_advance(l);
        while (isdigit(lexer_current(l))) {
            buffer[len++] = lexer_current(l);
            lexer_advance(l);
        }
    }

    buffer[len] = '\0';

    Token *tok = token_new(TOK_NUMBER, buffer, start_line, start_col);
    tok->num_value = atof(buffer);
    return tok;
}

static Token *lexer_read_ident(Lexer *l) {
    int start_line = l->line;
    int start_col = l->col;

    char buffer[256];
    size_t len = 0;

    while (isalnum(lexer_current(l)) || lexer_current(l) == '_') {
        buffer[len++] = lexer_current(l);
        lexer_advance(l);
    }
    buffer[len] = '\0';

    TokenType type = lookup_keyword(buffer, len);
    Token *tok = token_new(type, buffer, start_line, start_col);
    return tok;
}

int lexer_scan_all(Lexer *l) {
    if (!l) return -1;

    while (l->pos < l->src_len) {
        lexer_skip_whitespace(l);

        if (l->pos >= l->src_len) break;

        // Comments
        if (lexer_current(l) == '/' && lexer_peek(l, 1) == '/') {
            lexer_skip_line_comment(l);
            continue;
        }
        if (lexer_current(l) == '/' && lexer_peek(l, 1) == '*') {
            lexer_skip_block_comment(l);
            continue;
        }

        int start_line = l->line;
        int start_col = l->col;
        char ch = lexer_current(l);

        // Numbers
        if (isdigit(ch)) {
            Token *tok = lexer_read_number(l);
            lexer_add_token(l, tok);
            continue;
        }

        // Strings
        if (ch == '"' || ch == '\'') {
            Token *tok = lexer_read_string(l, ch);
            lexer_add_token(l, tok);
            continue;
        }

        // F-strings
        if (ch == 'f' && (lexer_peek(l, 1) == '"' || lexer_peek(l, 1) == '\'')) {
            Token *tok = lexer_read_fstring(l);
            lexer_add_token(l, tok);
            continue;
        }

        // Identifiers and keywords
        if (isalpha(ch) || ch == '_') {
            Token *tok = lexer_read_ident(l);
            lexer_add_token(l, tok);
            continue;
        }

        // Multi-character operators
        if (ch == '=' && lexer_peek(l, 1) == '=' && lexer_peek(l, 2) == '=') {
            Token *tok = token_new(TOK_EQEQEQ, "===", start_line, start_col);
            lexer_add_token(l, tok);
            lexer_advance(l); lexer_advance(l); lexer_advance(l);
            continue;
        }
        if (ch == '!' && lexer_peek(l, 1) == '=' && lexer_peek(l, 2) == '=') {
            Token *tok = token_new(TOK_NOTEQUALEQ, "!==", start_line, start_col);
            lexer_add_token(l, tok);
            lexer_advance(l); lexer_advance(l); lexer_advance(l);
            continue;
        }
        if (ch == '=' && lexer_peek(l, 1) == '=') {
            Token *tok = token_new(TOK_EQEQ, "==", start_line, start_col);
            lexer_add_token(l, tok);
            lexer_advance(l); lexer_advance(l);
            continue;
        }
        if (ch == '!' && lexer_peek(l, 1) == '=') {
            Token *tok = token_new(TOK_NOTEQ, "!=", start_line, start_col);
            lexer_add_token(l, tok);
            lexer_advance(l); lexer_advance(l);
            continue;
        }
        if (ch == '<' && lexer_peek(l, 1) == '=') {
            Token *tok = token_new(TOK_LTEQ, "<=", start_line, start_col);
            lexer_add_token(l, tok);
            lexer_advance(l); lexer_advance(l);
            continue;
        }
        if (ch == '>' && lexer_peek(l, 1) == '=') {
            Token *tok = token_new(TOK_GTEQ, ">=", start_line, start_col);
            lexer_add_token(l, tok);
            lexer_advance(l); lexer_advance(l);
            continue;
        }
        if (ch == '<' && lexer_peek(l, 1) == '<') {
            Token *tok = token_new(TOK_LSHIFT, "<<", start_line, start_col);
            lexer_add_token(l, tok);
            lexer_advance(l); lexer_advance(l);
            continue;
        }
        if (ch == '>' && lexer_peek(l, 1) == '>') {
            Token *tok = token_new(TOK_RSHIFT, ">>", start_line, start_col);
            lexer_add_token(l, tok);
            lexer_advance(l); lexer_advance(l);
            continue;
        }
        if (ch == '&' && lexer_peek(l, 1) == '&') {
            Token *tok = token_new(TOK_AND, "&&", start_line, start_col);
            lexer_add_token(l, tok);
            lexer_advance(l); lexer_advance(l);
            continue;
        }
        if (ch == '|' && lexer_peek(l, 1) == '|') {
            Token *tok = token_new(TOK_OR, "||", start_line, start_col);
            lexer_add_token(l, tok);
            lexer_advance(l); lexer_advance(l);
            continue;
        }
        if (ch == '*' && lexer_peek(l, 1) == '*') {
            Token *tok = token_new(TOK_POWER, "**", start_line, start_col);
            lexer_add_token(l, tok);
            lexer_advance(l); lexer_advance(l);
            continue;
        }
        if (ch == '=' && lexer_peek(l, 1) == '>') {
            Token *tok = token_new(TOK_ARROW, "=>", start_line, start_col);
            lexer_add_token(l, tok);
            lexer_advance(l); lexer_advance(l);
            continue;
        }
        if (ch == '+' && lexer_peek(l, 1) == '=') {
            Token *tok = token_new(TOK_PLUS_EQ, "+=", start_line, start_col);
            lexer_add_token(l, tok);
            lexer_advance(l); lexer_advance(l);
            continue;
        }
        if (ch == '-' && lexer_peek(l, 1) == '=') {
            Token *tok = token_new(TOK_MINUS_EQ, "-=", start_line, start_col);
            lexer_add_token(l, tok);
            lexer_advance(l); lexer_advance(l);
            continue;
        }
        if (ch == '*' && lexer_peek(l, 1) == '=') {
            Token *tok = token_new(TOK_STAR_EQ, "*=", start_line, start_col);
            lexer_add_token(l, tok);
            lexer_advance(l); lexer_advance(l);
            continue;
        }
        if (ch == '/' && lexer_peek(l, 1) == '=') {
            Token *tok = token_new(TOK_SLASH_EQ, "/=", start_line, start_col);
            lexer_add_token(l, tok);
            lexer_advance(l); lexer_advance(l);
            continue;
        }

        // Single-character tokens
        TokenType type = TOK_EOF;
        switch (ch) {
            case '+': type = TOK_PLUS; break;
            case '-': type = TOK_MINUS; break;
            case '*': type = TOK_STAR; break;
            case '/': type = TOK_SLASH; break;
            case '%': type = TOK_PERCENT; break;
            case '=': type = TOK_EQ; break;
            case '!': type = TOK_NOT; break;
            case '<': type = TOK_LT; break;
            case '>': type = TOK_GT; break;
            case '&': type = TOK_AMPERSAND; break;
            case '|': type = TOK_PIPE; break;
            case '^': type = TOK_CARET; break;
            case '~': type = TOK_TILDE; break;
            case '?': type = TOK_QUESTION; break;
            case '(': type = TOK_LPAREN; break;
            case ')': type = TOK_RPAREN; break;
            case '{': type = TOK_LBRACE; break;
            case '}': type = TOK_RBRACE; break;
            case '[': type = TOK_LBRACKET; break;
            case ']': type = TOK_RBRACKET; break;
            case ',': type = TOK_COMMA; break;
            case '.': type = TOK_DOT; break;
            case ';': type = TOK_SEMICOLON; break;
            case ':': type = TOK_COLON; break;
            case '@': type = TOK_AT; break;
            case '#': type = TOK_HASH; break;
            case '$': type = TOK_DOLLAR; break;
            default:
                // Unknown character, skip
                fprintf(stderr, "Unexpected character: %c at %d:%d\n", ch, l->line, l->col);
                lexer_advance(l);
                continue;
        }

        char buf[2] = { ch, '\0' };
        Token *tok = token_new(type, buf, start_line, start_col);
        lexer_add_token(l, tok);
        lexer_advance(l);
    }

    // Add EOF token
    Token *eof = token_new(TOK_EOF, "", l->line, l->col);
    lexer_add_token(l, eof);

    return 0;
}

Token *lexer_get_token(Lexer *l, size_t index) {
    if (!l || index >= l->token_count) return NULL;
    return &l->tokens[index];  // Return pointer to token in array
}

size_t lexer_token_count(Lexer *l) {
    return l ? l->token_count : 0;
}

void lexer_print_tokens(Lexer *l) {
    if (!l) return;
    printf("=== Tokens (%zu total) ===\n", l->token_count);
    for (size_t i = 0; i < l->token_count; i++) {
        token_print(&l->tokens[i]);  // Pass pointer to token
    }
}
