#pragma once

#define TOK_EOF     0   // \0
#define TOK_ASSIGN  1   // =
#define TOK_OR      2   // |
#define TOK_AND     3   // &
#define TOK_ADD     4   // +
#define TOK_SUB     5   // -
#define TOK_MUL     6   // *
#define TOK_DIV     7   // /
#define TOK_INC     8   // ++
#define TOK_DEC     9   // --
#define TOK_EQUALS  10  // ==
#define TOK_LOGOR   11  // ||
#define TOK_BITOR   12  // |
#define TOK_LOGAND  13  // &&
#define TOK_BITAND  14  // &
#define TOK_SEMI    15  // ;
#define TOK_LPAREN  16  // (
#define TOK_RPAREN  17  // )
#define TOK_COMMA   18  // ,
#define TOK_LBRACE  19  // {
#define TOK_RBRACE  20  // }
#define TOK_SYMBOL  128
#define TOK_LITERAL 129

#define SYM_IF      0 // if
#define SYM_INT     1 // int
#define SYM_RETURN  2 // return

#define token_t int
#define symbol_t int

#define bool int
#define true  1
#define false 0

void pExpr(int v);
void pStmt();