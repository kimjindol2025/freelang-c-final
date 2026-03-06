/**
 * FreeLang Parser Interface
 * Converts tokens to AST
 */

#ifndef FL_PARSER_H
#define FL_PARSER_H

#include "freelang.h"

/* Forward declarations */
typedef struct fl_ast_node fl_ast_node_t;

typedef struct fl_parser {
    fl_token_t* tokens;
    size_t token_count;
    size_t pos;
} fl_parser_t;

/* Create parser */
fl_parser_t* fl_parser_create(fl_token_t* tokens, size_t token_count);

/* Destroy parser */
void fl_parser_destroy(fl_parser_t* parser);

/* Parse tokens to AST */
fl_ast_node_t* fl_parser_parse(fl_parser_t* parser);

/* Parse specific constructs */
fl_ast_node_t* fl_parser_parse_statement(fl_parser_t* parser);
fl_ast_node_t* fl_parser_parse_expression(fl_parser_t* parser);
fl_ast_node_t* fl_parser_parse_function(fl_parser_t* parser);

#endif /* FL_PARSER_H */
