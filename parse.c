#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "defs.h"

char     lK0, lK1;                 // input delay line (lK1 = lookahead)
int      lLine;                    // currently lexed line number

char     tSym[128];                // extracted currently parsed token
int      tSymLen;                  // current symbol length
int      tSymValue;                // value if symbol is a literal

token_t  tToken;                   // last parsed token
bool     tPeeked;                  // a peek was performed

char     sSymbolTable[SYMTABLEN];  // symbol table
int      sSymbols;                 // current length of symbol table

int      sFuncs;                   // number of functions
symbol_t sFuncTable[NFUNC];        // function table
int      sFuncPos  [NFUNC];        // function code offsets
int      sFuncArgs [NFUNC];        // function argument counts
type_t   sFuncType [NFUNC];        // function return type

int      sGlobals;                 // number of globals
symbol_t sGlobalTable[NGLOBAL];    // global table
int      sGlobalOffs [NGLOBAL];    // global offsets
type_t   sGlobalType [NGLOBAL];    // global type
int      sGlobalSize [NGLOBAL];    // global size (0=not array)
int      sGlobalSectSize;          // total global section size

int      sArgs;                    // number of arguments
symbol_t sArgTable[NARG];          // argument table
type_t   sArgType [NARG];          // argument type

int      sLocals;                  // number of locals
symbol_t sLocalTable[NLOCAL];      // locals table
type_t   sLocalType [NLOCAL];      // local type

symbol_t sSymPutchar;              // putchar system call symbol
symbol_t sSymMain;                 // main symbol

int      cCode[NCODELEN];          // code stream
int      cCodeLen;                 // code length

FILE    *inFile;                   // input file

//----------------------------------------------------------------------------
// FORWARD DECLARATIONS
//----------------------------------------------------------------------------

bool  pExpr      (int v, bool rvalueReq);
void  pStmt      ();
void  cEmit0     (int c);
int   cEmit1     (int c, int opr);
void  cPatch     (int loc, int opr);
int   cPos       ();
void  cPushSymbol(symbol_t s);

//----------------------------------------------------------------------------
// LEXER
//----------------------------------------------------------------------------

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
  // track new lines
  lLine = (lK0 == '\n') ? (lLine + 1) : lLine;
  return lK0;
}

// return true if lookahead is a specific char
bool lFound(char c) {
  return (lK1 == c) ? (lNext(), true) : false;
}

#define CHECK(X, T) if (strMatch(tSym, X)) return T;
token_t tKeywordCheck() {
  switch (tSym[0]) {
  case 'c': CHECK("char",   TOK_CHAR);   break;
  case 'd': CHECK("do",     TOK_DO);     break;
  case 'e': CHECK("else",   TOK_ELSE);   break;
  case 'f': CHECK("for",    TOK_FOR);    break;
  case 'i': CHECK("int",    TOK_INT);
            CHECK("if",     TOK_IF);     break;
  case 'v': CHECK("void",   TOK_VOID);   break;
  case 'r': CHECK("return", TOK_RETURN); break;
  case 'w': CHECK("while",  TOK_WHILE);  break;
  }
  return TOK_SYMBOL;
}
#undef CHECK

// return the next token
token_t tNext() {

  // if peeked, just mark as consumed and return
  if (tPeeked) {
    tPeeked = false;
    return tToken;
  }

  char c = lNext();

  while (true) {
    // consume whitespace
    while (lIsWhiteSpace(c)) {
      c = lNext();
    }
    // single line comments
    if (c == '/' && lFound('/')) {
      do {
        c = lNext();
      } while (c != '\0' && c != '\n');
      continue;
    }
    // multi line comments
    if (c == '/' && lFound('*')) {
      while (true) {
        c = lNext();
        if (c == '\0' || (c == '*' && lFound('/'))) {
          break;
        }
      }
      c = lNext();
      continue;
    }
    break;
  }

  tSymLen = 0;

  // tokenize literal values
  if (lIsNumber(c)) {
    for (;;c = lNext()) {
      tSym[tSymLen++] = c;
      if (!lIsNumber(lK1))
        break;
    }
    tSym[tSymLen] = '\0';
    tSymValue = strToInt(tSym);
    return tToken = TOK_LITERAL;
  }

  // tokenize alphanumeric symbols
  if (lIsAlpha(c)) {
    for (;;c = lNext()) {
      tSym[tSymLen++] = c;
      if (!lIsAlpha(lK1) && !lIsNumber(lK1))
        break;
    }
    tSym[tSymLen] = '\0';
    return tToken = tKeywordCheck();
  }

  // tokenize character literals
  if (c == '\'') {
    char c = lNext();
    if (lNext() != '\'') {
      fatal("%u: error: malformed character literal", lLine);
    }
    tSymValue = c;
    return tToken = TOK_LITERAL;
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
  case '%':  return tToken = TOK_MOD;
  case '[':  return tToken = TOK_LBRACK;
  case ']':  return tToken = TOK_RBRACK;
  case '<':  return tToken = lFound('=') ? TOK_LTEQU  : TOK_LT;
  case '>':  return tToken = lFound('=') ? TOK_GTEQU  : TOK_GT;
  case '|':  return tToken = lFound('|') ? TOK_LOGOR  : TOK_BITOR;
  case '&':  return tToken = lFound('&') ? TOK_LOGAND : TOK_BITAND;
  case '+':  return tToken = lFound('+') ? TOK_INC    : TOK_ADD;
  case '-':  return tToken = lFound('-') ? TOK_DEC    : TOK_SUB;
  case '=':  return tToken = lFound('=') ? TOK_EQU    : TOK_ASSIGN;
  case '!':  return tToken = lFound('=') ? TOK_NEQU   : TOK_LOGNOT;
  }

  fatal("%u: error: unexpected character '%c'", lLine, c);
  return tToken = TOK_EOF;
}

// advance to next token but dont treat as consumed
// warning, a peek will wipe out the previous token
token_t tPeek() {
  tNext();
  tPeeked = true;
  return tToken;
}

// expect a specific token
void tExpect(token_t tok) {
  token_t got = tNext();
  if (got != tok) {
    char *e = tokName(tok);
    char *g = tokName(got);
    fatal("%u: error: expecting token '%s' but got '%s'", lLine, e, g);
  }
}

// return true and consume if a specific token was found
bool tFound(token_t tok) {
  return (tPeek() == tok) ? (tNext(), true) : false;
}

// return true if current token is a type
bool tIsType() {
  switch (tToken) {
  case TOK_CHAR:
  case TOK_INT:
  case TOK_VOID: return true;
  default:       return false;
  }
}

//----------------------------------------------------------------------------
// SYMBOL TABLE
//----------------------------------------------------------------------------

// intern the string held in 'tSym'
// add to symbol table if not present and return index
int sIntern(char *name) {
  // XXX: todo, check for symtab overflow
  char *c = sSymbolTable;
  int n = 0;
  for (;n < sSymbols; ++n) {
    if (strMatch(c, name)) {
      return n;
    }
    c = strSkip(c);
  }
  strCopy(c, name);
  return sSymbols++;
}

// a new global is being declared
void sGlobalAdd(type_t type, symbol_t sym, int size) {
  if (sGlobals >= NGLOBAL)
    fatal("%u: error: global count limit reached", lLine);
  if (contains(sym, sGlobalTable, sGlobals) >= 0)
    fatal("%u: error: global already defined", lLine);

  sGlobalTable[sGlobals] = sym;
  sGlobalType [sGlobals] = type;
  sGlobalOffs [sGlobals] = sGlobalSectSize;
  sGlobalSize [sGlobals] = size;
  sGlobalSectSize += (size == 0) ? 1 : size;
  sGlobals++;
}

// a new function is being declared
void sFuncAdd(type_t type, symbol_t sym, int nargs) {
  if (sFuncs >= NFUNC)
    fatal("%u: error: function count limit reached", lLine);
  if (contains(sym, sFuncTable, sFuncs) >= 0)
    fatal("%u: error: function already defined", lLine);

  sFuncTable[sFuncs] = sym;     // save symbol
  sFuncPos  [sFuncs] = cPos();  // code position
  sFuncArgs [sFuncs] = nargs;   // argument count
  sFuncType [sFuncs] = type;    // save type
  sFuncs++;
}

// a new argument is being declared
void sArgAdd(type_t type, symbol_t sym) {
  if (sArgs >= NARG)
    fatal("%u: error: argument count limit reached", lLine);
  if (contains(sym, sArgTable, sArgs) >= 0)
    fatal("%u: error: duplicate arguments", lLine);

  sArgTable[sArgs] = sym;
  sArgType [sArgs] = type;
  sArgs++;
}

// a new local has been declared
void sLocalAdd(type_t type, symbol_t sym) {
  if (sLocals >= NLOCAL)
    fatal("%u: error: local count limit reached", lLine);
  if (contains(sym, sLocalTable, sLocals) >= 0)
    fatal("%u: error: local already defined", lLine);

  sLocalTable[sLocals] = sym;
  sLocalType [sLocals] = type;
  sLocals++;
}

// check if a symbol is a system call
bool sIsSyscall(symbol_t sym) {
  if (sym == sSymPutchar) {
    return true;
  }
  return false;
}

// given a symbol return code position of function
int sFuncFind(symbol_t sym) {
  int pos = contains(sym, sFuncTable, sFuncs);
  if (pos >= 0) {
    return pos;
  }
  fatal("%u: error: unknown function", lLine);
  return 0;
}

// check if a symbol is an array type
bool sIsArray(symbol_t sym) {
  int i;
  if ((i = contains(sym, sGlobalTable, sGlobals)) >= 0) {
    return sGlobalSize[i] > 0;
  }
  return false;
}

//----------------------------------------------------------------------------
// PARSER
//----------------------------------------------------------------------------

// consume a function call
void pExprCall(symbol_t sym) {
  int nargs = 0;

  while (!tFound(TOK_RPAREN)) {
    do {
      pExpr(1, true);
      nargs++;
    } while (tFound(TOK_COMMA));
  }

  if (sIsSyscall(sym)) {
    cEmit1(INS_SCALL, sym);
  }
  else {
    int f = sFuncFind(sym);

    // verify argument count is correct
    if (nargs != sFuncArgs[f]) {
      fatal("%u: error: function takes %u arguments", lLine, sFuncArgs[f]);
    }

    cEmit1(INS_CALL, sFuncPos[f]);
  }
}

// consume a primary expression
// return true if lvalue else false
bool pExprPrimary() {
  token_t n = tNext();

  // parenthesized expression
  if (n == TOK_LPAREN) {
    bool lvalue = pExpr(1, false);
    tExpect(TOK_RPAREN);
    return lvalue;
  }
  // integer literal
  if (n == TOK_LITERAL) {
    cEmit1(INS_CONST, tSymValue);
    return false;
  }
  // idenfitier or function call
  if (n == TOK_SYMBOL) {
    // get symbol
    symbol_t sym = sIntern(tSym);
    // function call
    if (tFound(TOK_LPAREN)) {
      pExprCall(sym);
      return false;
    }
    else {
      // place symbol value on the stack
      cPushSymbol(sym);
      // arrays are treated as rvalues since we use their address directly
      // and must be dereferenced before use. you also cant reassign an array.
      return sIsArray(sym) ? false :  // rvalue
                             true;    // lvalue
    }
  }

  char *got = tokName(n);
  fatal("%u: error: expected literal or identifier but got '%s'",
        lLine, got);
  return false;
}

// the precedence table
int pPrec(token_t c) {
  switch (c) {
  case TOK_ASSIGN: return 1;
  case TOK_LOGOR:  return 2;
  case TOK_LOGAND: return 3;
  case TOK_BITOR:  return 4;
//case TOK_BITXOR: ...
  case TOK_BITAND: return 5;
  case TOK_NEQU:
  case TOK_EQU:    return 6;
  case TOK_LTEQU:
  case TOK_GTEQU:
  case TOK_LT:
  case TOK_GT:     return 7;
//case TOK_SHR: ...
//case TOK_SHL: ...
  case TOK_ADD:
  case TOK_SUB:    return 8;
  case TOK_MOD:
  case TOK_MUL:
  case TOK_DIV:    return 9;
  }
  return 0;
}

// return true if this token is an operator
bool pIsOperator(token_t c) {
  return pPrec(c) > 0;
}

// check for any unary ops
token_t pUnaryOpCheck() {
  if (tFound(TOK_MUL))    return TOK_MUL;
  if (tFound(TOK_BITAND)) return TOK_BITAND;
  if (tFound(TOK_SUB))    return TOK_SUB;
  return 0;
}

// apply a unary operation
// return true if lvalue or false if rvalue
bool pUnaryOpApply(bool lvalue, token_t op) {

  // dereference
  if (op == TOK_MUL) {
    // convert to rvalue
    if (lvalue) {
      cEmit0(INS_DEREF);
    }
    // dont apply a dereference just say its an lvalue
    return true;
  }

  // address of
  if (op == TOK_BITAND) {
    if (!lvalue) {
      fatal("%u: error: address of requires lvalue", lLine);
    }
    // just treat it as an rvalue now
    return false;
  }

  // unary minus
  if (op == TOK_SUB) {
    // convert to rvalue
    if (lvalue) {
      cEmit0(INS_DEREF);
    }
    // negate the value
    cEmit0(INS_NEG);
    // its an rvalue now
    return false;
  }

  return lvalue;
}

// handle a subscript operator
void pSubscript(bool lvalue) {
  // to apply a subscript to something, it must be an rvalue or we will be
  // modifying the wrong address.  dereference it to an rvalue first.
  if (lvalue) {
    cEmit0(INS_DEREF);
  }
  pExpr(1, true);
  tExpect(TOK_RBRACK);
  cEmit0(TOK_ADD);
}

// precedence climbing expression parser
bool pExpr(int minPrec, bool rvalueReq) {
  bool lvalue;

  // check if we have any unary ops to consume
  token_t unOp = pUnaryOpCheck();

  // lhs
  lvalue = pExprPrimary();

  // handle array subscript
  if (tFound(TOK_LBRACK)) {
    pSubscript(lvalue);
    // a subscript results in an lvalue.
    // they have to so that assignments work as expected.
    lvalue = true;
  }

  // apply any unary op, if we found one
  lvalue = pUnaryOpApply(lvalue, unOp);

  // while our operator is equal or higher precidence
  while (1) {
    // look ahead for possible operators
    token_t op = tPeek();
    if (!pIsOperator(op)) {
        break;
    }
    if (pPrec(op) < minPrec) {
        break;
    }
    // consume operator
    tNext();

    // dereference if needed
    if (op == TOK_ASSIGN) {
      if (!lvalue) {
        fatal("%u: error: assignment requires lvalue", lLine);
      }
    }
    else {
      if (lvalue) {
        cEmit0(INS_DEREF);
      }
    }

    // rhs
    pExpr(pPrec(op), true);

    // apply operator
    cEmit0(op);
    lvalue = false;
  }

  // ensure evaluated expression is rvalue if required
  if (lvalue && rvalueReq) {
    lvalue = false;
    cEmit0(INS_DEREF);
  }
  return lvalue;
}

// parse a type
type_t pType() {
  token_t type = tNext();
  if (!tIsType()) {
    char *c = tokName(type);
    fatal("%u: error: type expected but found '%s'", lLine, c);
  }

  // consume pointers
  int nderef = 0;
  while (tFound(TOK_MUL)) {
    ++nderef;
  }

  // XXX: munge into a packed int?

  return type;
}

// parse an if statement
void pStmtIf() {
                                  // if
  tExpect(TOK_LPAREN);            // (
  pExpr(1, true);                 // <expr>
  tExpect(TOK_RPAREN);            // )
  int tf = cEmit1(INS_JZ, -1);    // ---> target false  (JZ)
  pStmt();                        // <stmt>
  if (tFound(TOK_ELSE)) {         // else
    int te = cEmit1(INS_JMP, -1); // ---> target end    (JMP)
    cPatch(tf, cPos());           // <--- target false
    pStmt();                      // <stmt>
    cPatch(te, cPos());           // <--- target end
  }
  else {
    cPatch(tf, cPos());           // <--- target false
  }
}

// parse a while statement
void pStmtWhile() {
  int tt = cPos();                // <--- target top
                                  // while
  tExpect(TOK_LPAREN);            // (
  pExpr(1, true);                 // <expr>
  tExpect(TOK_RPAREN);            // )
  int tf = cEmit1(INS_JZ, -1);    // ---> target false  (JZ)
  pStmt();                        // <stmt>
  cEmit1(INS_JMP, tt);            // ---> target top    (JMP)
  cPatch(tf, cPos());             // <--- target false
}

// parse a return statement
void pStmtReturn() {
                                  // return
  pExpr(1, true);                 // <expr>
  tExpect(TOK_SEMI);              // ;
  cEmit1(INS_RETURN, sArgs);
}

// parse a do while statement
void pStmtDo() {
  int tt = cPos();                // <--- target top
                                  // do
  pStmt();                        // <stmt>
  tExpect(TOK_WHILE);             // while
  tExpect(TOK_LPAREN);            // (
  pExpr(1, true);                 // <expr>
  tExpect(TOK_RPAREN);            // )
  tExpect(TOK_SEMI);              // ;
  cEmit1(INS_JNZ, tt);            // ---> target top  (JNZ)
}

void pStmtFor() {

  tExpect(TOK_LPAREN);

  // intial
  if (!tFound(TOK_SEMI)) {
    pExpr(1, true);
    tExpect(TOK_SEMI);
  }

  int locCond = cPos();           // condition
  if (!tFound(TOK_SEMI)) {
    pExpr(1, true);
    tExpect(TOK_SEMI);
  }
  else {
    cEmit1(INS_CONST, 1);
  }
  int jmpBody = cEmit1(INS_JNZ, -1);
  int jmpEnd  = cEmit1(INS_JMP, -1);

  int locInc  = cPos();           // inc
  if (!tFound(TOK_RPAREN)) {
    pExpr(1, true);
    tExpect(TOK_RPAREN);
  }
  cEmit1(INS_JMP, locCond);       // ---> cond

  cPatch(jmpBody, cPos());
  pStmt();                        // <stmt>
  cEmit1(INS_JMP, locInc);        // ---> inc

  cPatch(jmpEnd, cPos());         // jmp to the end
}

// parse a statement
void pStmt() {
  // if statement
  if (tFound(TOK_IF)) {
    pStmtIf();
    return;
  }
  // return statement
  if (tFound(TOK_RETURN)) {
    pStmtReturn();
    return;
  }
  // while statement
  if (tFound(TOK_WHILE)) {
    pStmtWhile();
    return;
  }
  // do while statement
  if (tFound(TOK_DO)) {
    pStmtDo();
    return;
  }
  // for loop
  if (tFound(TOK_FOR)) {
    pStmtFor();
    return;
  }
  // compound statement
  if (tFound(TOK_LBRACE)) {
    while (!tFound(TOK_RBRACE)) {
      pStmt();
    }
    return;
  }
  // empty statement
  if (tFound(TOK_SEMI)) {
    return;
  }
  // expression
  pExpr(1, true);
  tExpect(TOK_SEMI);
  // rvalue not used
  cEmit0(INS_DROP);
}

// parse a global decl
void pParseGlobal(type_t type, symbol_t sym) {

  int size = 0;

  if (tFound(TOK_LBRACK)) {
    tExpect(TOK_LITERAL);
    size = tSymValue;
    tExpect(TOK_RBRACK);
  }

  sGlobalAdd(type, sym, size);
  tExpect(TOK_SEMI);
}

// parse a local decl
void pParseLocal() {
  type_t   type = pType();

  do {
    token_t  name = tNext();
    symbol_t sym  = sIntern(tSym);
    sLocalAdd(type, sym);
  } while (tFound(TOK_COMMA));

  tExpect(TOK_SEMI);
}

// parse a function decl
void pParseFunc(type_t type, symbol_t sym) {

  // new scope so clear args and locals
  sArgs   = 0;
  sLocals = 0;

  // parse arguments
  if (!tFound(TOK_RPAREN)) {
    do {
      type_t  type = pType();
      token_t name = tNext();
      symbol_t sym = sIntern(tSym);
      sArgAdd(type, sym);
    } while (tFound(TOK_COMMA));
    tExpect(TOK_RPAREN);
  }

  // record new function
  sFuncAdd(type, sym, sArgs);

  // check main takes no arguments
  if (sym == sSymMain) {
    if (sArgs != 0) {
      fatal("%u: error: main takes no arguments", lLine);
    }
  }

  // function body
  tExpect(TOK_LBRACE);

  // parse locals
  for (;;) {
    tPeek();
    if (!tIsType()) {
      break;
    }
    pParseLocal();
  }

  // allocate space for locals
  if (sLocals) {
    cEmit1(INS_ALLOC, sLocals);
  }

  // parse statements
  while (!tFound(TOK_RBRACE)) {
    pStmt();
  }

  // return from function
  cEmit1(INS_CONST, 0);
  cEmit1(INS_RETURN, sArgs);
}

void pParse() {
  while (!tFound(TOK_EOF)) {

    // parse a type
    type_t type = pType();

    // parse a name
    tExpect(TOK_SYMBOL);
    symbol_t sym = sIntern(tSym);

    // if a function decl 
    if (tFound(TOK_LPAREN)) {
      pParseFunc(type, sym);
    }
    else {
      pParseGlobal(type, sym);
    }
  }
}

//----------------------------------------------------------------------------
// CODEGEN
//----------------------------------------------------------------------------

// return current code stream position
int cPos() {
  return cCodeLen;
}

// emit to output code stream
void cEmit0(int ins) {
  if (cCodeLen >= NCODELEN)
    fatal("%u: error: code limit reached", lLine);
  cCode[cCodeLen++] = ins;
}

// emit instruction and operand to code stream
int cEmit1(int ins, int opr) {
  cEmit0(ins);
  cEmit0(opr);
  return cCodeLen - 1;
}

// patch a previous operand
void cPatch(int loc, int opr) {
  cCode[loc] = opr;
}

// lookup a symbol and push its value onto the stack
// note we do this inner to outer scope for shadowing
void cPushSymbol(symbol_t s) {
  int i;
  if ((i = contains(s, sLocalTable, sLocals)) >= 0) {
    cEmit1(INS_GETAL, i);
    return;
  }
  if ((i = contains(s, sArgTable, sArgs)) >= 0) {
    // work backwards here to match stack indexing
    cEmit1(INS_GETAA, sArgs - i);
    return;
  }
  if ((i = contains(s, sGlobalTable, sGlobals)) >= 0) {
    cEmit1(INS_GETAG, sGlobalOffs[i]);
    return;
  }
  fatal("%u: error: unknown identifier", lLine);
}

//----------------------------------------------------------------------------
// DRIVER
//----------------------------------------------------------------------------

int main(int argc, char **args) {

  sSymPutchar = sIntern("putchar");
  sSymMain    = sIntern("main");

  // line counting starts at 1
  lLine = 1;

  if (argc <= 1) {
    fatal("%u: error: argument expected", lLine);
  }

  // open input file for reading
  inFile = fopen(args[1], "r");
  if (!inFile) {
    return 1;
  }

  // discard first read (lK0 invalid)
  lNext();

  // code starts with globals and call to main
  int globOpr = cEmit1(INS_ALLOC, -1);
  int jmpMain = cEmit1(INS_CALL, -1);

  // start parsing
  pParse();

  // patch in globals count
  cPatch(globOpr, sGlobalSectSize);

  // patch call to main after parsing
  symbol_t sMain = sIntern("main");
  int idMain = sFuncFind(sMain);
  cPatch(jmpMain, sFuncPos[idMain]);

  // output code stream
  fwrite(cCode, 4, cCodeLen, stdout);

  return 0;
}
