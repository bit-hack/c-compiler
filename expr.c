#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "defs.h"

char lK0, lK1;          // input delay line (lK1 = lookahead)

char tSymbol[128];      // currently parsed symbol
int  tSymLen;           // current symbol length

token_t tToken;         // last parsed token
bool    tPeeked;        // a peek was performed

char sSymTab[1024*4];   // symbol table
int  sSymNum;           // length of symbol table

int sFuncTable[16];     // function table
int sArgTable[16];      // argument table

FILE *inFile;

// XXX: make fatal(msg, ...)
void fatal(char *msg, ...) {
  va_list args;
  va_start(args, msg);
  vfprintf(stderr, msg, args);
  fprintf(stderr, "\n");
  va_end(args);
  exit(1);
}

bool lIsWhiteSpace(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool lIsNumber(char c) {
  return c >= '0' && c <= '9';
}

bool lIsAlpha(char c) {
  return c >= 'A' && c <= 'Z' ||
         c >= 'a' && c <= 'z' ||
         c == '_';
}

// get next character from input stream
char lNext() {
  int in = fgetc(inFile);
  lK0 = lK1;
  lK1 = (in >= 0) ? in : '\0';
  return lK0;
}

// return true if lookahead is a specific char
bool lFound(char c) {
  if (lK1 == c) {
    lNext();
    return true;
  }
  return false;
}

// next token
token_t tNext() {

  // if peeked, just mark as consumed and return
  if (tPeeked) {
    tPeeked = false;
    return tToken;
  }

  char c = lNext();

  // consume whitespace
  while (lIsWhiteSpace(c)) {
    c = lNext();
  }

  tSymLen = 0;

  // tokenize literal values
  if (lIsNumber(c)) {
    for (;;c = lNext()) {
      tSymbol[tSymLen++] = c;
      if (!lIsNumber(lK1))
        break;
    }
    tSymbol[tSymLen] = '\0';
    return tToken = TOK_LITERAL;
  }

  // tokenize alphanumeric symbols
  if (lIsAlpha(c)) {
    for (;;c = lNext()) {
      tSymbol[tSymLen++] = c;
      if (!lIsAlpha(lK1) && !lIsNumber(lK1))
        break;
    }
    tSymbol[tSymLen] = '\0';
    return tToken = TOK_SYMBOL;
  }

  // try simple tokens
  switch (c) {
  case '\0': return tToken = TOK_EOF;
  case '/':  return tToken = TOK_DIV;
  case '*':  return tToken = TOK_MUL;
  case ';':  return tToken = TOK_SEMI;
  case '(':  return tToken = TOK_LPAREN;
  case ')':  return tToken = TOK_RPAREN;
  case ',':  return tToken = TOK_COMMA;
  case '{':  return tToken = TOK_LBRACE;
  case '}':  return tToken = TOK_RBRACE;
  case '|':  return tToken = lFound('|') ? TOK_LOGOR  : TOK_BITOR;
  case '&':  return tToken = lFound('&') ? TOK_LOGAND : TOK_BITAND;
  case '+':  return tToken = lFound('+') ? TOK_INC    : TOK_ADD;
  case '-':  return tToken = lFound('-') ? TOK_DEC    : TOK_SUB;
  case '=':  return tToken = lFound('=') ? TOK_EQUALS : TOK_ASSIGN;
  }

  // unknown token in input stream
  fatal("Unknown token %u in input stream", c);
  return tToken = TOK_EOF;
}

// advance to next token but dont treat as consumed
// warning, a peek will wipe out the previous token.
token_t tPeek() {
  tNext();
  tPeeked = true;
  return tToken;
}

// print a token
void tPrint(token_t t) {
  switch (t) {
  case TOK_SYMBOL:
  case TOK_LITERAL: puts(tSymbol); break;
  case TOK_EOF    : puts("\0");    break;
  case TOK_ASSIGN : puts("=");     break;
  case TOK_OR     : puts("|");     break;
  case TOK_AND    : puts("&");     break;
  case TOK_ADD    : puts("+");     break;
  case TOK_SUB    : puts("-");     break;
  case TOK_MUL    : puts("*");     break;
  case TOK_DIV    : puts("/");     break;
  case TOK_INC    : puts("++");    break;
  case TOK_DEC    : puts("--");    break;
  case TOK_EQUALS : puts("==");    break;
  case TOK_LOGOR  : puts("||");    break;
  case TOK_BITOR  : puts("|");     break;
  case TOK_LOGAND : puts("&&");    break;
  case TOK_BITAND : puts("&");     break;
  case TOK_SEMI   : puts(";");     break;
  case TOK_LPAREN : puts("(");     break;
  case TOK_RPAREN : puts(")");     break;
  case TOK_COMMA  : puts(",");     break;
  case TOK_LBRACE : puts("{");     break;
  case TOK_RBRACE : puts("}");     break;
  default:
    fatal("Unable to print unknown token");
  }
}

// expect a specific token
void tExpect(token_t tok) {
  token_t got = tNext();
  if (got != tok) {
    fatal("Expecting token %u but got %u\n", tok, got);
  }
}

// return true if a specific token was found
bool tFound(token_t tok) {
  if (tPeek() == tok) {
    tNext();  // consume
    return true;
  }
  return false;
}

bool strmatch(char *a, char *b) {
  while (true) {
    if (*a == '\0' && *b == '\0') {
      return true;
    }
    if (*a++ != *b++) {
      return false;
    }
  }
}

// skip a string
char *strskip(char *c) {
  while (*c++);
  return c;
}

char *strcopy(char *dst, char *src) {
  while (*src) {
    *dst++ = *src++;
  }
  *dst++ = '\0';
  return dst;
}

// intern the string held in 'tSymbol'
// add to symbol table if not present and return index
int sIntern(char *name) {
  char *c = sSymTab;
  int n = 0;
  for (;n < sSymNum; ++n) {
    if (strmatch(c, name)) {
      return n;
    }
    c = strskip(c);
  }
  strcopy(c, name);
  return sSymNum++;
}

void sFuncAdd(symbol_t sym) {
}

// return true if current token is a type
bool tIsType() {
  if (tToken == TOK_SYMBOL) {
    switch (sIntern(tSymbol)) {
//  case SYM_CHAR:
    case SYM_INT: return true;
    }
  }
  return false;
}

// emit to output stream
void cEmit(int c) {
//  fputc(c, stdout);
}

// consume a primary expression
void pPrimary() {
  token_t n = tNext();

  if (n == TOK_LPAREN) {
    pExpr(1);
    tExpect(TOK_RPAREN);
    return;
  }

  if (n == TOK_LITERAL) {
    tPrint(n);
    cEmit(n);
    return;
  }

  if (n == TOK_SYMBOL) {
    tPrint(n);
    cEmit(n);

    // xxx if ( function call
    // xxx if [ array access

    return;
  }

  fatal("expected literal or name");
}

// the precedence table
int pPrec(token_t c) {
  switch (c) {
  case TOK_ASSIGN: return 1;
  case TOK_BITAND: return 2;
  case TOK_BITOR:  return 2;
  case TOK_ADD:    return 3;
  case TOK_SUB:    return 3;
  case TOK_MUL:    return 4;
  case TOK_DIV:    return 4;
  }
  return 0;
}

// return true if this token is an operator
bool pIsOperator(token_t c) {
  return pPrec(c) > 0;
}

// precedence climbing expression parser
void pExpr(int min_prec) {
  // lhs
  pPrimary();
  // while our operator is equal or higher precidence
  while (1) {
    // look ahead for possible operators
    token_t op = tPeek();
    if (!pIsOperator(op)) {
        return;
    }
    if (pPrec(op) < min_prec) {
        return;
    }
    // consume operator
    tNext();

    // rhs
    pExpr(pPrec(op));

    tPrint(op);
    cEmit(op);
  }
}

void pType() {
  token_t type = tNext();
  if (!tIsType()) {
    fatal("Type expected");
  }
}

void pStmtIf() {
  tNext();              // if
  tExpect(TOK_LPAREN);  // (
  pExpr(1);             // <expr>
  tExpect(TOK_RPAREN);  // )
  pStmt();              // <stmt>
  // XXX: if else ...
}

void pStmtReturn() {
  tNext();              // return
  pExpr(1);             // <expr>
  tExpect(TOK_SEMI);    // ;
}

void pStmt() {
  token_t t = tPeek();

  // check symbol names
  if (t == TOK_SYMBOL) {
    symbol_t sym = sIntern(tSymbol);
    switch (sym) {
    case SYM_IF:     pStmtIf();     return;
    case SYM_RETURN: pStmtReturn(); return;
    }
  }

  // compound statement
  if (t == TOK_LBRACE) {
    while (!tFound(TOK_RBRACE)) {
      pStmt();
    }
    return;
  }

  // try to parse an expression
  pExpr(1);
  tExpect(TOK_SEMI);
}

void pParseFunc() {

  // parse arguments
  if (!tFound(TOK_RPAREN)) {
    do {
      pType();
      token_t name = tNext();
    } while (tFound(TOK_COMMA));
    tExpect(TOK_RPAREN);
  }

  // parse function body
  tExpect(TOK_LBRACE);
  while (!tFound(TOK_RBRACE)) {
    pStmt();
  }
}

void pParse() {
  while (!tFound(TOK_EOF)) {

    // parse a type
    pType();

    // parse a name
    tExpect(TOK_SYMBOL);
    symbol_t sym = sIntern(tSymbol);
    sFuncAdd(sym);

    // if a function decl 
    if (tFound(TOK_LPAREN)) {
      pParseFunc();
    }
    else {
      // XXX: global
    }

  }
}

int main(int argc, char **args) {

  // open input file for reading
  inFile = fopen(args[1], "r");
  if (!inFile) {
    return 1;
  }

  // add keywords, note that this should match SYM_XXX constants
  sIntern("if");
  sIntern("int");
  sIntern("return");

  // discard first read (lK0 invalid)
  lNext();

  // start parsing
  pParse();

  return 0;
}
