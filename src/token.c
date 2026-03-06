#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "token.h"

Token *token_new(TokenType type, const char *value, int line, int col) {
    Token *tok = (Token *)malloc(sizeof(Token));
    if (!tok) return NULL;

    tok->type = type;
    tok->value = value ? strdup(value) : NULL;
    tok->num_value = 0.0;
    tok->line = line;
    tok->col = col;

    return tok;
}

void token_free(Token *tok) {
    if (!tok) return;
    if (tok->value) {
        free(tok->value);
    }
    free(tok);
}

const char *token_type_name(TokenType type) {
    switch (type) {
        // Literals
        case TOK_NUMBER:     return "NUMBER";
        case TOK_STRING:     return "STRING";
        case TOK_FSTRING:    return "FSTRING";
        case TOK_IDENT:      return "IDENT";

        // Keywords
        case TOK_LET:        return "LET";
        case TOK_CONST:      return "CONST";
        case TOK_VAR:        return "VAR";
        case TOK_FN:         return "FN";
        case TOK_IF:         return "IF";
        case TOK_ELSE:       return "ELSE";
        case TOK_WHILE:      return "WHILE";
        case TOK_FOR:        return "FOR";
        case TOK_IN:         return "IN";
        case TOK_RETURN:     return "RETURN";
        case TOK_BREAK:      return "BREAK";
        case TOK_CONTINUE:   return "CONTINUE";
        case TOK_TRUE:       return "TRUE";
        case TOK_FALSE:      return "FALSE";
        case TOK_NULL:       return "NULL";
        case TOK_STRUCT:     return "STRUCT";
        case TOK_ENUM:       return "ENUM";
        case TOK_TRY:        return "TRY";
        case TOK_CATCH:      return "CATCH";
        case TOK_FINALLY:    return "FINALLY";
        case TOK_THROW:      return "THROW";
        case TOK_IMPORT:     return "IMPORT";
        case TOK_FROM:       return "FROM";
        case TOK_EXPORT:     return "EXPORT";
        case TOK_DEFAULT:    return "DEFAULT";
        case TOK_AS:         return "AS";
        case TOK_ASYNC:      return "ASYNC";
        case TOK_AWAIT:      return "AWAIT";
        case TOK_MATCH:      return "MATCH";

        // Operators
        case TOK_PLUS:       return "PLUS";
        case TOK_MINUS:      return "MINUS";
        case TOK_STAR:       return "STAR";
        case TOK_SLASH:      return "SLASH";
        case TOK_PERCENT:    return "PERCENT";
        case TOK_POWER:      return "POWER";
        case TOK_EQ:         return "EQ";
        case TOK_EQEQ:       return "EQEQ";
        case TOK_EQEQEQ:     return "EQEQEQ";
        case TOK_NOT:        return "NOT";
        case TOK_NOTEQ:      return "NOTEQ";
        case TOK_NOTEQUALEQ: return "NOTEQUALEQ";
        case TOK_LT:         return "LT";
        case TOK_LTEQ:       return "LTEQ";
        case TOK_GT:         return "GT";
        case TOK_GTEQ:       return "GTEQ";
        case TOK_AND:        return "AND";
        case TOK_OR:         return "OR";
        case TOK_AMPERSAND:  return "AMPERSAND";
        case TOK_PIPE:       return "PIPE";
        case TOK_CARET:      return "CARET";
        case TOK_TILDE:      return "TILDE";
        case TOK_LSHIFT:     return "LSHIFT";
        case TOK_RSHIFT:     return "RSHIFT";
        case TOK_ARROW:      return "ARROW";
        case TOK_QUESTION:   return "QUESTION";

        // Punctuation
        case TOK_LPAREN:     return "LPAREN";
        case TOK_RPAREN:     return "RPAREN";
        case TOK_LBRACE:     return "LBRACE";
        case TOK_RBRACE:     return "RBRACE";
        case TOK_LBRACKET:   return "LBRACKET";
        case TOK_RBRACKET:   return "RBRACKET";
        case TOK_COMMA:      return "COMMA";
        case TOK_DOT:        return "DOT";
        case TOK_SEMICOLON:  return "SEMICOLON";
        case TOK_COLON:      return "COLON";
        case TOK_AT:         return "AT";
        case TOK_HASH:       return "HASH";
        case TOK_DOLLAR:     return "DOLLAR";

        // Compound assignments
        case TOK_PLUS_EQ:    return "PLUS_EQ";
        case TOK_MINUS_EQ:   return "MINUS_EQ";
        case TOK_STAR_EQ:    return "STAR_EQ";
        case TOK_SLASH_EQ:   return "SLASH_EQ";

        // Special
        case TOK_NEWLINE:    return "NEWLINE";
        case TOK_EOF:        return "EOF";
        default:             return "UNKNOWN";
    }
}

void token_print(Token *tok) {
    if (!tok) return;
    printf("Token { type: %s, value: %s, line: %d, col: %d }\n",
           token_type_name(tok->type),
           tok->value ? tok->value : "(nil)",
           tok->line,
           tok->col);
}
