#ifndef LB_PARSER_INTERNAL_H
#define LB_PARSER_INTERNAL_H

#include "lastbreach.h"

/*
 * Private parser interfaces shared by parser translation units.
 *
 * Ownership:
 * - lb_parser_expr.c      : parse_expr
 * - lb_parser_stmt.c      : parse_action_stmt, parse_stmt_list
 * - lb_parser_sections.c  : parse_character (public entry in lastbreach.h)
 */

Expr *parse_expr(Parser *ps);
/* Parses "task ...", "set ...", "yield_tick", etc. (without trailing semicolon handling). */
Stmt *parse_action_stmt(Parser *ps);
/* Parses a brace-delimited statement sequence until '}' or EOF. */
void parse_stmt_list(Parser *ps, VecStmtPtr *out);

#endif
