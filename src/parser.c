/**
 * FreeLang Recursive Descent Parser Implementation
 * Converts lexer tokens to Abstract Syntax Tree (AST)
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../include/parser.h"
#include "../include/ast.h"
#include "../include/token.h"

/* Type aliases for compatibility */
typedef fl_ast_node_t ASTNode;
typedef fl_token_t Token;

static bool is_at_end(fl_parser_t *p) {
    return p->pos >= p->token_count;
}

static Token current_token(fl_parser_t *p) {
    if (is_at_end(p)) {
        return p->tokens[p->token_count - 1];
    }
    return p->tokens[p->pos];
}

static Token advance(fl_parser_t *p) {
    Token prev = current_token(p);
    if (!is_at_end(p)) {
        p->pos++;
    }
    return prev;
}

static bool check(fl_parser_t *p, TokenType type) {
    if (is_at_end(p)) return false;
    return current_token(p).type == type;
}

static bool match(fl_parser_t *p, TokenType type) {
    if (!check(p, type)) return false;
    advance(p);
    return true;
}

static ASTNode* parse_statement(fl_parser_t *p);
static ASTNode* parse_expression(fl_parser_t *p);
static ASTNode* parse_assignment(fl_parser_t *p);
static ASTNode* parse_primary(fl_parser_t *p);

static ASTNode* parse_var_decl(fl_parser_t *p) {
    // Phase 4: Check for decorator (@watch, @transaction)
    char decorator[32] = "";
    if (check(p, TOK_AT)) {
        advance(p);  // consume @
        if (check(p, TOK_IDENT)) {
            Token dec_tok = advance(p);
            strncpy(decorator, dec_tok.value, 31);
        }
    }

    // Phase 4: Check for reactive keyword
    int is_reactive = 0;
    if (match(p, TOK_REACTIVE)) {
        is_reactive = 1;
    }

    Token kind_tok = advance(p);        // "let", "const", "var"
    Token name_tok = advance(p);        // variable name
    ASTNode *init = NULL;

    if (match(p, TOK_EQ)) {
        init = parse_expression(p);
    }
    match(p, TOK_SEMICOLON);

    // Choose node type based on is_reactive flag
    ASTNode *node = ast_alloc(is_reactive ? NODE_REACTIVE_DECL : NODE_VAR_DECL);
    if (node) {
        if (is_reactive) {
            // NODE_REACTIVE_DECL
            node->data.reactive_decl.is_reactive = 1;
            strncpy(node->data.reactive_decl.kind, kind_tok.value, 7);
            strncpy(node->data.reactive_decl.name, name_tok.value, 255);
            strncpy(node->data.reactive_decl.decorator, decorator, 31);
            node->data.reactive_decl.init = init;
        } else {
            // NODE_VAR_DECL
            strncpy(node->data.var_decl.kind, kind_tok.value, 7);
            strncpy(node->data.var_decl.name, name_tok.value, 255);
            node->data.var_decl.init = init;
        }
        node->line = kind_tok.line;
        node->col = kind_tok.col;
    }
    return node;
}

static ASTNode* parse_fn_decl(fl_parser_t *p) {
    Token fn_tok = advance(p);
    Token name_tok = advance(p);
    if (!match(p, TOK_LPAREN)) return NULL;

    char **param_names = NULL;
    int param_count = 0;

    if (!check(p, TOK_RPAREN)) {
        while (!check(p, TOK_RPAREN) && !is_at_end(p)) {
            if (check(p, TOK_IDENT)) {
                Token param = advance(p);
                param_count++;
                param_names = (char**)realloc(param_names, sizeof(char*) * param_count);
                if (param_names) {
                    param_names[param_count - 1] = (char*)malloc(strlen(param.value) + 1);
                    strcpy(param_names[param_count - 1], param.value);
                }
            }
            if (!match(p, TOK_COMMA)) break;
        }
    }

    if (!match(p, TOK_RPAREN)) return NULL;

    ASTNode *body = NULL;
    if (check(p, TOK_LBRACE)) {
        Token lbrace = current_token(p);
        match(p, TOK_LBRACE);

        ASTNode **statements = NULL;
        int stmt_count = 0;
        while (!check(p, TOK_RBRACE) && !is_at_end(p)) {
            ASTNode *stmt = parse_statement(p);
            if (stmt) {
                stmt_count++;
                statements = (ASTNode**)realloc(statements, sizeof(ASTNode*) * stmt_count);
                if (statements) {
                    statements[stmt_count - 1] = stmt;
                }
            }
        }
        match(p, TOK_RBRACE);

        body = ast_alloc(NODE_BLOCK);
        if (body) {
            body->data.block.statements = statements;
            body->data.block.stmt_count = stmt_count;
            body->line = lbrace.line;
            body->col = lbrace.col;
        }
    }

    ASTNode *node = ast_alloc(NODE_FN_DECL);
    if (node) {
        strncpy(node->data.fn_decl.name, name_tok.value, 255);
        node->data.fn_decl.param_names = param_names;
        node->data.fn_decl.param_count = param_count;
        node->data.fn_decl.body = body;
        node->line = fn_tok.line;
        node->col = fn_tok.col;
    }
    return node;
}

static ASTNode* parse_block(fl_parser_t *p) {
    Token lbrace = current_token(p);
    if (!match(p, TOK_LBRACE)) return NULL;

    ASTNode **statements = NULL;
    int stmt_count = 0;

    while (!check(p, TOK_RBRACE) && !is_at_end(p)) {
        ASTNode *stmt = parse_statement(p);
        if (stmt) {
            stmt_count++;
            statements = (ASTNode**)realloc(statements, sizeof(ASTNode*) * stmt_count);
            if (statements) {
                statements[stmt_count - 1] = stmt;
            }
        }
    }

    match(p, TOK_RBRACE);

    ASTNode *node = ast_alloc(NODE_BLOCK);
    if (node) {
        node->data.block.statements = statements;
        node->data.block.stmt_count = stmt_count;
        node->line = lbrace.line;
        node->col = lbrace.col;
    }
    return node;
}

static ASTNode* parse_if_stmt(fl_parser_t *p) {
    Token if_tok = advance(p);
    if (!match(p, TOK_LPAREN)) return NULL;
    ASTNode *test = parse_expression(p);
    if (!match(p, TOK_RPAREN)) return NULL;
    ASTNode *consequent = parse_statement(p);
    ASTNode *alternate = NULL;
    if (match(p, TOK_ELSE)) {
        alternate = parse_statement(p);
    }

    ASTNode *node = ast_alloc(NODE_IF);
    if (node) {
        node->data.if_stmt.test = test;
        node->data.if_stmt.consequent = consequent;
        node->data.if_stmt.alternate = alternate;
        node->line = if_tok.line;
        node->col = if_tok.col;
    }
    return node;
}

static ASTNode* parse_while_stmt(fl_parser_t *p) {
    Token while_tok = advance(p);
    if (!match(p, TOK_LPAREN)) return NULL;
    ASTNode *test = parse_expression(p);
    if (!match(p, TOK_RPAREN)) return NULL;
    ASTNode *body = parse_statement(p);

    ASTNode *node = ast_alloc(NODE_WHILE);
    if (node) {
        node->data.while_stmt.test = test;
        node->data.while_stmt.body = body;
        node->line = while_tok.line;
        node->col = while_tok.col;
    }
    return node;
}

static ASTNode* parse_for_stmt(fl_parser_t *p) {
    Token for_tok = advance(p);
    if (!match(p, TOK_LPAREN)) return NULL;

    if (check(p, TOK_IDENT)) {
        size_t save_pos = p->pos;
        Token var_name = advance(p);
        if (match(p, TOK_IN)) {
            ASTNode *iterable = parse_expression(p);
            if (!match(p, TOK_RPAREN)) return NULL;
            ASTNode *body = parse_statement(p);

            ASTNode *node = ast_alloc(NODE_FOR_IN);
            if (node) {
                strncpy(node->data.for_in_stmt.var_name, var_name.value, 255);
                node->data.for_in_stmt.iterable = iterable;
                node->data.for_in_stmt.body = body;
                node->line = for_tok.line;
                node->col = for_tok.col;
            }
            return node;
        }
        p->pos = save_pos;
    }

    ASTNode *init = NULL;
    if (!check(p, TOK_SEMICOLON)) {
        if (check(p, TOK_LET) || check(p, TOK_VAR) || check(p, TOK_CONST)) {
            init = parse_var_decl(p);
        } else {
            init = parse_expression(p);
            match(p, TOK_SEMICOLON);
        }
    } else {
        match(p, TOK_SEMICOLON);
    }

    ASTNode *test = NULL;
    if (!check(p, TOK_SEMICOLON)) {
        test = parse_expression(p);
    }
    match(p, TOK_SEMICOLON);

    ASTNode *update = NULL;
    if (!check(p, TOK_RPAREN)) {
        update = parse_expression(p);
    }
    match(p, TOK_RPAREN);
    ASTNode *body = parse_statement(p);

    ASTNode *node = ast_alloc(NODE_FOR);
    if (node) {
        node->data.for_stmt.init = init;
        node->data.for_stmt.test = test;
        node->data.for_stmt.update = update;
        node->data.for_stmt.body = body;
        node->line = for_tok.line;
        node->col = for_tok.col;
    }
    return node;
}

static ASTNode* parse_return_stmt(fl_parser_t *p) {
    Token return_tok = advance(p);
    ASTNode *value = NULL;
    if (!check(p, TOK_SEMICOLON) && !check(p, TOK_RBRACE) && !is_at_end(p)) {
        value = parse_expression(p);
    }
    match(p, TOK_SEMICOLON);

    ASTNode *node = ast_alloc(NODE_RETURN);
    if (node) {
        node->data.return_stmt.value = value;
        node->line = return_tok.line;
        node->col = return_tok.col;
    }
    return node;
}

static ASTNode* parse_break_stmt(fl_parser_t *p) {
    Token break_tok = advance(p);
    match(p, TOK_SEMICOLON);
    ASTNode *node = ast_alloc(NODE_BREAK);
    if (node) {
        node->data.break_stmt.label = 0;
        node->line = break_tok.line;
        node->col = break_tok.col;
    }
    return node;
}

static ASTNode* parse_continue_stmt(fl_parser_t *p) {
    Token continue_tok = advance(p);
    match(p, TOK_SEMICOLON);
    ASTNode *node = ast_alloc(NODE_CONTINUE);
    if (node) {
        node->data.continue_stmt.label = 0;
        node->line = continue_tok.line;
        node->col = continue_tok.col;
    }
    return node;
}

static ASTNode* parse_try_stmt(fl_parser_t *p) {
    Token try_tok = advance(p);
    ASTNode *body = parse_block(p);
    ASTNode *catch_clause = NULL;
    ASTNode *finally_clause = NULL;

    if (match(p, TOK_CATCH)) {
        char error_var[256] = "error";
        if (match(p, TOK_LPAREN)) {
            if (check(p, TOK_IDENT)) {
                Token err_name = advance(p);
                strncpy(error_var, err_name.value, 255);
            }
            match(p, TOK_RPAREN);
        }
        ASTNode *handler = parse_block(p);
        catch_clause = ast_alloc(NODE_CATCH_CLAUSE);
        if (catch_clause) {
            strncpy(catch_clause->data.catch_clause.error_var, error_var, 255);
            catch_clause->data.catch_clause.handler = handler;
        }
    }

    if (match(p, TOK_FINALLY)) {
        ASTNode *handler = parse_block(p);
        finally_clause = ast_alloc(NODE_FINALLY_CLAUSE);
        if (finally_clause) {
            finally_clause->data.finally_clause.handler = handler;
        }
    }

    ASTNode *node = ast_alloc(NODE_TRY);
    if (node) {
        node->data.try_stmt.body = body;
        node->data.try_stmt.catch_clause = catch_clause;
        node->data.try_stmt.finally_clause = finally_clause;
        node->line = try_tok.line;
        node->col = try_tok.col;
    }
    return node;
}

static ASTNode* parse_throw_stmt(fl_parser_t *p) {
    Token throw_tok = advance(p);
    ASTNode *argument = parse_expression(p);
    match(p, TOK_SEMICOLON);
    ASTNode *node = ast_alloc(NODE_THROW);
    if (node) {
        node->data.throw_stmt.argument = argument;
        node->line = throw_tok.line;
        node->col = throw_tok.col;
    }
    return node;
}

/* Phase 8: @cluster(n) / @autorestart(true) 어노테이션 파싱
 * 어노테이션은 fn 선언 앞에 붙으며, NODE_DECORATOR_DECL로 래핑됨 */
static ASTNode* parse_annotation_decl(fl_parser_t *p) {
    /* @ 소비 */
    advance(p);

    /* 어노테이션 이름 */
    char ann_name[64] = "";
    if (check(p, TOK_IDENT)) {
        Token t = advance(p);
        strncpy(ann_name, t.value, 63);
    }

    /* 파라미터 (옵션): @cluster(4), @autorestart(true) */
    char ann_param[64] = "";
    if (match(p, TOK_LPAREN)) {
        Token param_tok = current_token(p);
        if (!check(p, TOK_RPAREN)) {
            advance(p);
            strncpy(ann_param, param_tok.value ? param_tok.value : "", 63);
        }
        match(p, TOK_RPAREN);
    }

    /* @log_level(debug|info|warn|error|off) → 컴파일 레벨 설정 후 계속 파싱 */
    if (strcmp(ann_name, "log_level") == 0) {
        /* scan_log_level_annotation이 runtime 단계에서 처리하므로
         * 파서는 이 어노테이션을 무시하고 다음 statement를 반환 */
        ASTNode* next = NULL;
        if (!is_at_end(p)) {
            next = parse_statement(p);
        }
        return next;  /* 어노테이션은 투명하게 처리 */
    }

    /* Phase 8: MOSS-Autodoc @api 어노테이션
     * 문법: @api(path: "/...", method: "GET", summary: "...", tag: "...", returns: "...")
     * fn 선언 앞에만 사용 가능. fn_decl 노드에 메타데이터 직접 주입. */
    if (strcmp(ann_name, "api") == 0) {
        char api_path[512]    = "/";
        char api_method[16]   = "GET";
        char api_summary[512] = "";
        char api_tag[128]     = "";
        char api_returns[64]  = "object";

        /* @api(key: "value", ...) 파싱 */
        if (ann_param[0] == '\0') {
            /* ann_param이 비어있으면 아직 괄호를 소비하지 않은 상태 */
            if (match(p, TOK_LPAREN)) {
                while (!check(p, TOK_RPAREN) && !is_at_end(p)) {
                    char key[64] = "";
                    char val[512] = "";
                    /* key */
                    if (check(p, TOK_IDENT)) {
                        Token kt = advance(p);
                        strncpy(key, kt.value, 63);
                    }
                    match(p, TOK_COLON);
                    /* value - 문자열 리터럴 */
                    if (check(p, TOK_STRING)) {
                        Token vt = advance(p);
                        strncpy(val, vt.value, 511);
                    } else if (check(p, TOK_IDENT)) {
                        Token vt = advance(p);
                        strncpy(val, vt.value, 511);
                    }
                    /* 키별 할당 */
                    if      (strcmp(key, "path")    == 0) strncpy(api_path,    val, 511);
                    else if (strcmp(key, "method")  == 0) strncpy(api_method,  val, 15);
                    else if (strcmp(key, "summary") == 0) strncpy(api_summary, val, 511);
                    else if (strcmp(key, "tag")     == 0) strncpy(api_tag,     val, 127);
                    else if (strcmp(key, "returns") == 0) strncpy(api_returns, val, 63);
                    match(p, TOK_COMMA);
                }
                match(p, TOK_RPAREN);
            }
        }

        /* 뒤따르는 fn 선언 파싱 */
        ASTNode *fn = NULL;
        if (check(p, TOK_FN)) {
            fn = parse_fn_decl(p);
        } else {
            fn = parse_statement(p);
        }

        /* fn_decl 노드에 API 메타데이터 주입 */
        if (fn && fn->type == NODE_FN_DECL) {
            fn->data.fn_decl.has_api_annotation = 1;
            strncpy(fn->data.fn_decl.api_path,    api_path,    511);
            strncpy(fn->data.fn_decl.api_method,  api_method,  15);
            strncpy(fn->data.fn_decl.api_summary, api_summary, 511);
            strncpy(fn->data.fn_decl.api_tag,     api_tag,     127);
            strncpy(fn->data.fn_decl.api_returns, api_returns, 63);
        }
        return fn;  /* decorator 래핑 없이 fn_decl 직접 반환 */
    }

    /* Phase 6: @vectorize { body } → NODE_VECTORIZE_HINT */
    if (strcmp(ann_name, "vectorize") == 0) {
        ASTNode* body = NULL;
        if (check(p, TOK_LBRACE)) {
            body = parse_block(p);
        } else {
            body = parse_statement(p);
        }
        ASTNode* vnode = ast_alloc(NODE_VECTORIZE_HINT);
        if (vnode) {
            vnode->data.vectorize_hint.body = body;
            /* ann_param: "128", "256", "512" → simd_width; 없으면 0(자동) */
            vnode->data.vectorize_hint.simd_width = ann_param[0] ? atoi(ann_param) : 0;
        }
        return vnode;
    }

    /* 뒤따르는 실제 선언 파싱 */
    ASTNode* inner = NULL;
    if (check(p, TOK_AT)) {
        inner = parse_annotation_decl(p); /* 중첩 어노테이션 */
    } else if (check(p, TOK_FN)) {
        inner = parse_fn_decl(p);
    } else if (check(p, TOK_LET) || check(p, TOK_VAR) || check(p, TOK_CONST)) {
        inner = parse_var_decl(p);
    }

    /* NODE_DECORATOR_DECL에 저장 */
    ASTNode* node = ast_alloc(NODE_DECORATOR_DECL);
    if (node && inner) {
        /* decorator_decl.name: "cluster(4)", "autorestart(true)", etc */
        snprintf(node->data.decorator_decl.name,
                 sizeof(node->data.decorator_decl.name),
                 "%s(%s)", ann_name, ann_param);
        node->data.decorator_decl.target = inner;
    }
    return node;
}

static ASTNode* parse_statement(fl_parser_t *p) {
    if (is_at_end(p)) return NULL;

    /* Phase 8: @cluster/@autorestart/@daemon 어노테이션 */
    if (check(p, TOK_AT)) {
        /* 다음 토큰이 식별자인지 확인 */
        if (p->pos + 1 < p->token_count &&
            p->tokens[p->pos + 1].type == TOK_IDENT) {
            return parse_annotation_decl(p);
        }
    }

    /* Phase 6: aligned let/const/var x = ... → NODE_ALIGNED_DECL */
    if (check(p, TOK_ALIGNED)) {
        advance(p); /* consume 'aligned' */
        int align_bytes = 32; /* 기본값: 32바이트 (AVX2) */
        /* aligned(64) 형태의 파라미터 처리 */
        if (match(p, TOK_LPAREN)) {
            if (check(p, TOK_NUMBER)) {
                Token nt = advance(p);
                align_bytes = (int)nt.num_value;
            }
            match(p, TOK_RPAREN);
        }
        ASTNode* decl = parse_var_decl(p);
        ASTNode* anode = ast_alloc(NODE_ALIGNED_DECL);
        if (anode) {
            anode->data.aligned_decl.align_bytes = align_bytes;
            anode->data.aligned_decl.decl = decl;
        }
        return anode;
    }

    if (check(p, TOK_LET) || check(p, TOK_VAR) || check(p, TOK_CONST)) {
        return parse_var_decl(p);
    }
    if (check(p, TOK_FN)) {
        return parse_fn_decl(p);
    }
    if (check(p, TOK_LBRACE)) {
        return parse_block(p);
    }
    if (check(p, TOK_IF)) {
        return parse_if_stmt(p);
    }
    if (check(p, TOK_WHILE)) {
        return parse_while_stmt(p);
    }
    if (check(p, TOK_FOR)) {
        return parse_for_stmt(p);
    }
    if (check(p, TOK_RETURN)) {
        return parse_return_stmt(p);
    }
    if (check(p, TOK_BREAK)) {
        return parse_break_stmt(p);
    }
    if (check(p, TOK_CONTINUE)) {
        return parse_continue_stmt(p);
    }
    if (check(p, TOK_TRY)) {
        return parse_try_stmt(p);
    }
    if (check(p, TOK_THROW)) {
        return parse_throw_stmt(p);
    }

    ASTNode *expr = parse_expression(p);
    match(p, TOK_SEMICOLON);
    return expr;
}

static ASTNode* parse_primary(fl_parser_t *p) {
    if (is_at_end(p)) return NULL;

    Token tok = current_token(p);

    if (match(p, TOK_TRUE) || match(p, TOK_FALSE) || match(p, TOK_NULL)) {
        ASTNode *node = ast_alloc(NODE_LITERAL);
        if (node) {
            node->data.literal.num = (tok.type == TOK_TRUE) ? 1 : 0;
            node->line = tok.line;
            node->col = tok.col;
        }
        return node;
    }

    if (match(p, TOK_NUMBER)) {
        ASTNode *node = ast_alloc(NODE_LITERAL);
        if (node) {
            node->data.literal.num = tok.num_value;
            node->line = tok.line;
            node->col = tok.col;
        }
        return node;
    }

    if (match(p, TOK_STRING)) {
        ASTNode *node = ast_alloc(NODE_LITERAL);
        if (node) {
            node->data.literal.str = (char*)malloc(strlen(tok.value) + 1);
            if (node->data.literal.str) strcpy(node->data.literal.str, tok.value);
            node->line = tok.line;
            node->col = tok.col;
        }
        return node;
    }

    if (match(p, TOK_IDENT)) {
        ASTNode *node = ast_alloc(NODE_IDENT);
        if (node) {
            strncpy(node->data.ident.name, tok.value, 255);
            node->line = tok.line;
            node->col = tok.col;
        }
        return node;
    }

    if (match(p, TOK_LPAREN)) {
        /* Save position to check for arrow function */
        size_t save_pos = p->pos;
        Token paren_tok = tok;  /* Save opening paren token */

        /* Try to parse as arrow function parameters */
        char **param_names = NULL;
        int param_count = 0;
        int is_arrow_params = 1;

        /* Parse parameter list or expression */
        if (!check(p, TOK_RPAREN)) {
            /* Try parsing as param list first */
            if (check(p, TOK_IDENT)) {
                Token first_tok = advance(p);

                /* Check if it's param list (followed by comma or rparen) */
                if (check(p, TOK_COMMA) || check(p, TOK_RPAREN)) {
                    /* It's param list */
                    param_count++;
                    param_names = (char**)malloc(sizeof(char*));
                    if (param_names) {
                        param_names[0] = (char*)malloc(strlen(first_tok.value) + 1);
                        if (param_names[0]) strcpy(param_names[0], first_tok.value);
                    }

                    /* Parse remaining params */
                    while (match(p, TOK_COMMA) && !is_at_end(p)) {
                        if (!check(p, TOK_IDENT)) {
                            is_arrow_params = 0;
                            break;
                        }
                        Token param_tok = advance(p);
                        param_count++;
                        param_names = (char**)realloc(param_names, sizeof(char*) * param_count);
                        if (param_names) {
                            param_names[param_count - 1] = (char*)malloc(strlen(param_tok.value) + 1);
                            if (param_names[param_count - 1]) {
                                strcpy(param_names[param_count - 1], param_tok.value);
                            }
                        }
                    }
                } else {
                    /* Not a param list, restore position */
                    is_arrow_params = 0;
                    p->pos = save_pos;
                }
            } else if (check(p, TOK_RPAREN)) {
                /* Empty params () => expr */
                /* Fall through to rparen check below */
            } else {
                /* Not a param list */
                is_arrow_params = 0;
                p->pos = save_pos;
            }
        }

        if (is_arrow_params && match(p, TOK_RPAREN) && check(p, TOK_ARROW)) {
            /* It's an arrow function! */
            advance(p);  /* consume => */

            /* Parse body - can be expression or block */
            ASTNode *body = NULL;
            if (check(p, TOK_LBRACE)) {
                /* Block body */
                body = parse_block(p);
            } else {
                /* Expression body - wrap in return statement */
                ASTNode *expr = parse_assignment(p);
                if (expr) {
                    /* Create return statement for expression */
                    ASTNode *ret = ast_alloc(NODE_RETURN);
                    if (ret) {
                        ret->data.return_stmt.value = expr;
                        body = ret;
                    } else {
                        body = expr;
                    }
                }
            }

            ASTNode *node = ast_alloc(NODE_ARROW_FN);
            if (node) {
                node->data.arrow_fn.param_names = param_names;
                node->data.arrow_fn.param_count = param_count;
                node->data.arrow_fn.body = body;
                node->data.arrow_fn.is_expression = !check(p, TOK_LBRACE);
                node->line = paren_tok.line;
                node->col = paren_tok.col;
            }
            return node;
        } else {
            /* Not an arrow function, restore and parse as expression */
            p->pos = save_pos;
            ASTNode *expr = parse_expression(p);
            match(p, TOK_RPAREN);
            return expr;
        }
    }

    /* Function expression: fn(params) { body } */
    if (check(p, TOK_FN)) {
        advance(p);  /* consume 'fn' */

        if (!match(p, TOK_LPAREN)) {
            return NULL;  /* Expected '(' */
        }

        /* Parse parameters */
        char **param_names = NULL;
        int param_count = 0;

        while (!check(p, TOK_RPAREN) && !is_at_end(p)) {
            if (!check(p, TOK_IDENT)) break;
            Token param_tok = advance(p);

            param_count++;
            param_names = (char**)realloc(param_names, sizeof(char*) * param_count);
            if (param_names) {
                param_names[param_count - 1] = (char*)malloc(strlen(param_tok.value) + 1);
                if (param_names[param_count - 1]) {
                    strcpy(param_names[param_count - 1], param_tok.value);
                }
            }

            if (!match(p, TOK_COMMA)) break;
        }

        if (!match(p, TOK_RPAREN)) {
            return NULL;  /* Expected ')' */
        }

        /* Parse body block */
        ASTNode *body = NULL;
        if (check(p, TOK_LBRACE)) {
            body = parse_block(p);
        }

        ASTNode *node = ast_alloc(NODE_FN_EXPR);
        if (node) {
            node->data.fn_expr.param_names = param_names;
            node->data.fn_expr.param_count = param_count;
            node->data.fn_expr.body = body;
            node->data.fn_expr.param_types = NULL;
            node->data.fn_expr.return_type = NULL;
            node->line = tok.line;
            node->col = tok.col;
        }
        return node;
    }

    if (match(p, TOK_LBRACKET)) {
        ASTNode **elements = NULL;
        int element_count = 0;

        while (!check(p, TOK_RBRACKET) && !is_at_end(p)) {
            ASTNode *elem = parse_expression(p);
            if (elem) {
                element_count++;
                elements = (ASTNode**)realloc(elements, sizeof(ASTNode*) * element_count);
                if (elements) elements[element_count - 1] = elem;
            }
            if (!match(p, TOK_COMMA)) break;
        }

        match(p, TOK_RBRACKET);

        ASTNode *node = ast_alloc(NODE_ARRAY);
        if (node) {
            node->data.array.elements = elements;
            node->data.array.element_count = element_count;
            node->line = tok.line;
            node->col = tok.col;
        }
        return node;
    }

    if (match(p, TOK_LBRACE)) {
        ASTNode **properties = NULL;
        int property_count = 0;

        while (!check(p, TOK_RBRACE) && !is_at_end(p)) {
            if (!check(p, TOK_IDENT) && !check(p, TOK_STRING)) break;
            Token key_tok = advance(p);
            if (!match(p, TOK_COLON)) break;
            ASTNode *value = parse_expression(p);

            ASTNode *prop = ast_alloc(NODE_PROPERTY);
            if (prop) {
                strncpy(prop->data.property.key, key_tok.value, 255);
                prop->data.property.value = value;
                prop->data.property.is_computed = 0;
                property_count++;
                properties = (ASTNode**)realloc(properties, sizeof(ASTNode*) * property_count);
                if (properties) properties[property_count - 1] = prop;
            }

            if (!match(p, TOK_COMMA)) break;
        }

        match(p, TOK_RBRACE);

        ASTNode *node = ast_alloc(NODE_OBJECT);
        if (node) {
            node->data.object.properties = properties;
            node->data.object.property_count = property_count;
            node->line = tok.line;
            node->col = tok.col;
        }
        return node;
    }

    return NULL;
}

static ASTNode* parse_postfix(fl_parser_t *p) {
    ASTNode *expr = parse_primary(p);
    if (!expr) return NULL;

    while (1) {
        if (match(p, TOK_LBRACKET)) {
            ASTNode *index = parse_expression(p);
            match(p, TOK_RBRACKET);
            ASTNode *member = ast_alloc(NODE_MEMBER);
            if (member) {
                member->data.member.object = expr;
                member->data.member.property = index;
                member->data.member.is_computed = 1;
            }
            expr = member;
        } else if (match(p, TOK_DOT)) {
            if (!check(p, TOK_IDENT)) break;
            Token prop_name = advance(p);
            ASTNode *prop = ast_alloc(NODE_IDENT);
            if (prop) strncpy(prop->data.ident.name, prop_name.value, 255);
            ASTNode *member = ast_alloc(NODE_MEMBER);
            if (member) {
                member->data.member.object = expr;
                member->data.member.property = prop;
                member->data.member.is_computed = 0;
            }
            expr = member;
        } else if (check(p, TOK_LPAREN)) {
            Token paren = advance(p);
            ASTNode **arguments = NULL;
            int arg_count = 0;

            if (!check(p, TOK_RPAREN)) {
                while (!check(p, TOK_RPAREN) && !is_at_end(p)) {
                    ASTNode *arg = parse_expression(p);
                    if (arg) {
                        arg_count++;
                        arguments = (ASTNode**)realloc(arguments, sizeof(ASTNode*) * arg_count);
                        if (arguments) arguments[arg_count - 1] = arg;
                    }
                    if (!match(p, TOK_COMMA)) break;
                }
            }

            match(p, TOK_RPAREN);

            ASTNode *call = ast_alloc(NODE_CALL);
            if (call) {
                call->data.call.callee = expr;
                call->data.call.arguments = arguments;
                call->data.call.arg_count = arg_count;
                call->line = paren.line;
                call->col = paren.col;
            }
            expr = call;
        } else {
            break;
        }
    }

    return expr;
}

static ASTNode* parse_unary(fl_parser_t *p) {
    Token op_tok = current_token(p);

    if (check(p, TOK_NOT) || check(p, TOK_MINUS) || check(p, TOK_PLUS) || check(p, TOK_TILDE)) {
        advance(p);
        ASTNode *operand = parse_unary(p);
        ASTNode *node = ast_alloc(NODE_UNARY);
        if (node) {
            strncpy(node->data.unary.op, op_tok.value, 15);
            node->data.unary.operand = operand;
            node->data.unary.is_prefix = 1;
        }
        return node;
    }

    return parse_postfix(p);
}

static ASTNode* parse_power(fl_parser_t *p) {
    ASTNode *left = parse_unary(p);

    while (match(p, TOK_POWER)) {
        ASTNode *right = parse_unary(p);
        ASTNode *node = ast_alloc(NODE_BINARY);
        if (node) {
            strncpy(node->data.binary.op, "**", 15);
            node->data.binary.left = left;
            node->data.binary.right = right;
        }
        left = node;
    }

    return left;
}

static ASTNode* parse_multiplicative(fl_parser_t *p) {
    ASTNode *left = parse_power(p);

    while (check(p, TOK_STAR) || check(p, TOK_SLASH) || check(p, TOK_PERCENT)) {
        Token op_tok = advance(p);
        ASTNode *right = parse_power(p);
        ASTNode *node = ast_alloc(NODE_BINARY);
        if (node) {
            strncpy(node->data.binary.op, op_tok.value, 15);
            node->data.binary.left = left;
            node->data.binary.right = right;
        }
        left = node;
    }

    return left;
}

static ASTNode* parse_additive(fl_parser_t *p) {
    ASTNode *left = parse_multiplicative(p);

    while (check(p, TOK_PLUS) || check(p, TOK_MINUS)) {
        Token op_tok = advance(p);
        ASTNode *right = parse_multiplicative(p);
        ASTNode *node = ast_alloc(NODE_BINARY);
        if (node) {
            strncpy(node->data.binary.op, op_tok.value, 15);
            node->data.binary.left = left;
            node->data.binary.right = right;
        }
        left = node;
    }

    return left;
}

static ASTNode* parse_comparison(fl_parser_t *p) {
    ASTNode *left = parse_additive(p);

    while (check(p, TOK_LT) || check(p, TOK_LTEQ) || check(p, TOK_GT) || check(p, TOK_GTEQ)) {
        Token op_tok = advance(p);
        ASTNode *right = parse_additive(p);
        ASTNode *node = ast_alloc(NODE_BINARY);
        if (node) {
            strncpy(node->data.binary.op, op_tok.value, 15);
            node->data.binary.left = left;
            node->data.binary.right = right;
        }
        left = node;
    }

    return left;
}

static ASTNode* parse_equality(fl_parser_t *p) {
    ASTNode *left = parse_comparison(p);

    while (check(p, TOK_EQEQ) || check(p, TOK_NOTEQ) || check(p, TOK_EQEQEQ) || check(p, TOK_NOTEQUALEQ)) {
        Token op_tok = advance(p);
        ASTNode *right = parse_comparison(p);
        ASTNode *node = ast_alloc(NODE_BINARY);
        if (node) {
            strncpy(node->data.binary.op, op_tok.value, 15);
            node->data.binary.left = left;
            node->data.binary.right = right;
        }
        left = node;
    }

    return left;
}

static ASTNode* parse_and(fl_parser_t *p) {
    ASTNode *left = parse_equality(p);

    while (match(p, TOK_AND)) {
        ASTNode *right = parse_equality(p);
        ASTNode *node = ast_alloc(NODE_LOGICAL);
        if (node) {
            strncpy(node->data.logical.op, "&&", 7);
            node->data.logical.left = left;
            node->data.logical.right = right;
        }
        left = node;
    }

    return left;
}

static ASTNode* parse_or(fl_parser_t *p) {
    ASTNode *left = parse_and(p);

    while (match(p, TOK_OR)) {
        ASTNode *right = parse_and(p);
        ASTNode *node = ast_alloc(NODE_LOGICAL);
        if (node) {
            strncpy(node->data.logical.op, "||", 7);
            node->data.logical.left = left;
            node->data.logical.right = right;
        }
        left = node;
    }

    return left;
}

static ASTNode* parse_ternary(fl_parser_t *p) {
    ASTNode *test = parse_or(p);

    if (match(p, TOK_QUESTION)) {
        ASTNode *consequent = parse_expression(p);
        if (!match(p, TOK_COLON)) return test;
        ASTNode *alternate = parse_expression(p);

        ASTNode *node = ast_alloc(NODE_TERNARY);
        if (node) {
            node->data.ternary.test = test;
            node->data.ternary.consequent = consequent;
            node->data.ternary.alternate = alternate;
        }
        return node;
    }

    return test;
}

static ASTNode* parse_assignment(fl_parser_t *p) {
    ASTNode *expr = parse_ternary(p);

    if (check(p, TOK_EQ) || check(p, TOK_PLUS_EQ) || check(p, TOK_MINUS_EQ) ||
        check(p, TOK_STAR_EQ) || check(p, TOK_SLASH_EQ)) {

        Token op_tok = advance(p);
        ASTNode *value = parse_assignment(p);

        ASTNode *node = ast_alloc(NODE_ASSIGN);
        if (node) {
            node->data.assign.target = expr;
            strncpy(node->data.assign.op, op_tok.value, 7);
            node->data.assign.value = value;
        }
        return node;
    }

    return expr;
}

static ASTNode* parse_expression(fl_parser_t *p) {
    return parse_assignment(p);
}

fl_parser_t* fl_parser_create(fl_token_t* tokens, size_t token_count) {
    fl_parser_t* parser = (fl_parser_t*)malloc(sizeof(fl_parser_t));
    if (!parser) return NULL;
    parser->tokens = tokens;
    parser->token_count = token_count;
    parser->pos = 0;
    return parser;
}

void fl_parser_destroy(fl_parser_t* parser) {
    if (parser) free(parser);
}

fl_ast_node_t* fl_parser_parse(fl_parser_t* parser) {
    if (!parser) return NULL;

    ASTNode **items = NULL;
    int item_count = 0;

    while (!is_at_end(parser)) {
        ASTNode *stmt = parse_statement(parser);
        if (stmt) {
            item_count++;
            items = (ASTNode**)realloc(items, sizeof(ASTNode*) * item_count);
            if (items) items[item_count - 1] = stmt;
        } else {
            /* If statement returns NULL and we're not at end, advance to avoid infinite loop */
            if (!is_at_end(parser)) {
                advance(parser);
            }
        }
        if (is_at_end(parser)) break;
    }

    ASTNode *program = ast_alloc(NODE_PROGRAM);
    if (program) {
        program->data.program.items = items;
        program->data.program.item_count = item_count;
    }

    return program;
}

fl_ast_node_t* fl_parser_parse_statement(fl_parser_t* parser) {
    if (!parser) return NULL;
    return parse_statement(parser);
}

fl_ast_node_t* fl_parser_parse_expression(fl_parser_t* parser) {
    if (!parser) return NULL;
    return parse_expression(parser);
}

fl_ast_node_t* fl_parser_parse_function(fl_parser_t* parser) {
    if (!parser) return NULL;
    return parse_fn_decl(parser);
}
