// TODO heap allocated stack / manually do the stack
// TODO multithreading?
//      should be fairly easy to make everything atomic, since ints should be atomic already;
//      might need to do some jank with "int newVal = rc--"
//      also either need to make to-execute pool
//      and decide when to run in parallel: let the programmer specify which stuff should be parallelized?
//      look into pthread.h

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// #define inline

typedef enum { false, true } bool;

typedef long intType;

enum ExprType {
  Var,
  Lam,
  App,
  Add,
  Mul,
  Ifz,
  LitInt,
  MakeThunk,
  ForceThunk,
  Thunk
};

struct ExprStruct;

typedef struct ExprStruct* Expr;

struct TwoExprs {
  Expr a;
  Expr b;
};

struct ThreeExprs {
  Expr a;
  Expr b;
  Expr c;
};

struct Env {
  struct Env* next;
  Expr val;
  int rc;
};

struct BoolEnvAndExpr {
  bool boolVal;
  struct Env* envVal;
  Expr exprVal;
};

struct EnvAndExpr {
  struct Env* envVal;
  Expr exprVal;
};

union ExprUnion {
  int var;
  intType litInt;
  Expr expr;
  struct TwoExprs twoExprs;
  struct ThreeExprs threeExprs;
  struct BoolEnvAndExpr boolEnvAndExpr;
  struct EnvAndExpr envAndExpr;
};

struct ExprStruct {
  int rc;
  enum ExprType exprType;
  union ExprUnion exprUnion;
};

inline void incRC(Expr expr);
inline void decRC(Expr expr);

#define nFreedStackEnv 1000
struct Env* freedStackEnv[nFreedStackEnv];
int freedStackEnvPtr = 0;
// freedStackEnvPtr-1 is the highest valid value on freedStackEnv

inline void incRCEnv(struct Env* env) {
  if (env == 0) return;
  ++env->rc;
}

void decRCEnvRec(struct Env* env);

inline void decRCEnv(struct Env* env) {
  if (env == 0) return;
  --env->rc;
  if (env->rc == 0) decRCEnvRec(env);
}

void decRCEnvRec(struct Env* env) {
  // env->rc == 0

  decRCEnv(env->next);
  decRC(env->val);

  if (freedStackEnvPtr == nFreedStackEnv)
    free(env);
  else
    freedStackEnv[freedStackEnvPtr++] = env;
}

inline Expr getVarEnv(struct Env* env, int var) {
  while (var > 0) {
    env = env->next;
    --var;
  }

  return env->val;
}

inline struct Env* pushEnv(struct Env* env, Expr expr) {
  struct Env* retVal = (freedStackEnvPtr == 0) ? malloc(sizeof(struct Env)) : freedStackEnv[--freedStackEnvPtr];
  retVal->next = env;
  retVal->val = expr;
  retVal->rc = 0;
  incRCEnv(env);
  incRC(expr);
  return retVal;
}

#define nFreedStack 1000
Expr freedStack[nFreedStack];
int freedStackPtr = 0;
// freedStackPtr-1 is the highest valid value on freedStack
// int nStackFull = 0;

inline Expr mallocExpr() {
  if (freedStackPtr == 0)
    return malloc(sizeof(struct ExprStruct));
  else
    return freedStack[--freedStackPtr];
}

inline Expr allocExpr(enum ExprType exprType, union ExprUnion exprUnion) {
  Expr expr = mallocExpr();
  expr->exprType = exprType;
  expr->exprUnion = exprUnion;
  expr->rc = 0;
  return expr;
}

inline void freeExpr(Expr expr) {
  // if (freedStackPtr == nFreedStack) ++nStackFull;
  if (freedStackPtr == nFreedStack)
    free(expr);
  else
    freedStack[freedStackPtr++] = expr;
}

void freeStacks() {
  while (freedStackPtr > 0)
    free(freedStack[--freedStackPtr]);
  while (freedStackEnvPtr > 0)
    free(freedStackEnv[--freedStackEnvPtr]);
}

inline void incRC(Expr expr) {
  ++expr->rc;
}

void decRCRec(Expr expr);

inline void decRC(Expr expr) {
  --expr->rc;
  if (expr->rc == 0)
    decRCRec(expr);
}

void decRCRec(Expr expr) {
  // expr->rc == 0

  if (expr->exprType == Lam) {
    struct EnvAndExpr arg = expr->exprUnion.envAndExpr;
    decRCEnv(arg.envVal);
    decRC(arg.exprVal);
  } else if (expr->exprType == App || expr->exprType == Add || expr->exprType == Mul) {
    struct TwoExprs arg = expr->exprUnion.twoExprs;
    decRC(arg.a);
    decRC(arg.b);
  } else if (expr->exprType == Ifz) {
    struct ThreeExprs arg = expr->exprUnion.threeExprs;
    decRC(arg.a);
    decRC(arg.b);
    decRC(arg.c);
  } else if (expr->exprType == MakeThunk || expr->exprType == ForceThunk) {
    Expr arg = expr->exprUnion.expr;
    decRC(arg);
  } else if (expr->exprType == Thunk) {
    struct BoolEnvAndExpr arg = expr->exprUnion.boolEnvAndExpr;
    decRC(arg.exprVal);
    decRCEnv(arg.envVal);
  }

  freeExpr(expr);
}

void printExpr(Expr expr) {
  if (expr->exprType == Var) {
    int arg = expr->exprUnion.var;
    printf("v%d", arg);
  } else if (expr->exprType == Lam) {
    struct EnvAndExpr arg = expr->exprUnion.envAndExpr;
    printf("(\\ ");
    printExpr(arg.exprVal);
    printf(")");
  } else if (expr->exprType == App) {
    struct TwoExprs arg = expr->exprUnion.twoExprs;
    printf("(");
    printExpr(arg.a);
    printf(" ");
    printExpr(arg.b);
    printf(")");
  } else if (expr->exprType == Add) {
    struct TwoExprs arg = expr->exprUnion.twoExprs;
    printf("(");
    printExpr(arg.a);
    printf(" + ");
    printExpr(arg.b);
    printf(")");
  } else if (expr->exprType == Mul) {
    struct TwoExprs arg = expr->exprUnion.twoExprs;
    printf("(");
    printExpr(arg.a);
    printf(" * ");
    printExpr(arg.b);
    printf(")");
  } else if (expr->exprType == Ifz) {
    struct ThreeExprs arg = expr->exprUnion.threeExprs;
    printf("ifz ");
    printExpr(arg.a);
    printf(" then ");
    printExpr(arg.b);
    printf(" else ");
    printExpr(arg.c);
  } else if (expr->exprType == MakeThunk) {
    Expr arg = expr->exprUnion.expr;
    printf("[");
    printExpr(arg);
    printf("]");
  } else if (expr->exprType == ForceThunk) {
    Expr arg = expr->exprUnion.expr;
    printf("$(");
    printExpr(arg);
    printf(")");
  } else if (expr->exprType == Thunk) {
    printf("THUNK");
  } else if (expr->exprType == LitInt) {
    long arg = expr->exprUnion.litInt;
    printf("%ld", arg);
  } else {
    exit(1);
  }
}

void printEnv(struct Env* env) {
  if (env == 0) {
    printf("[]");
  } else {
    printExpr(env->val);
    printf(" | ");
    printEnv(env->next);
  }
}

Expr interpretFully(struct Env* env, Expr expr);

inline Expr force(struct BoolEnvAndExpr* thunk) {
  if (!thunk->boolVal) {
    thunk->exprVal = interpretFully(thunk->envVal, thunk->exprVal);
    decRCEnv(thunk->envVal);
    // thunk->envVal = 0;
    thunk->boolVal = true;
  }
  return thunk->exprVal;
}

// bool is whether it's done interpreting
inline struct BoolEnvAndExpr interpret(struct Env* env, Expr expr) {
  if (expr->exprType == Var) {
    int arg = expr->exprUnion.var;
    return (struct BoolEnvAndExpr) { true, env, getVarEnv(env, arg) };
  } else if (expr->exprType == Lam) {
    struct EnvAndExpr arg = expr->exprUnion.envAndExpr;
    if (arg.envVal != 0) return (struct BoolEnvAndExpr) { true, env, expr };

    Expr retVal = allocExpr(Lam, (union ExprUnion) { .envAndExpr = (struct EnvAndExpr) { env, arg.exprVal } });
    incRCEnv(env);
    incRC(arg.exprVal);
    return (struct BoolEnvAndExpr) { true, env, retVal };
  } else if (expr->exprType == App) {
    struct TwoExprs arg = expr->exprUnion.twoExprs;
    arg.a = interpretFully(env, arg.a);
    arg.b = interpretFully(env, arg.b);
    if (arg.a->exprType != Lam) exit(1);
    struct EnvAndExpr body = arg.a->exprUnion.envAndExpr;
    env = pushEnv(body.envVal, arg.b);
    incRC(body.exprVal); // jank; makes it so that decRC(arg.a) doesn't also deallocate body.exprVal
    decRC(arg.a);
    --body.exprVal->rc; // jank pt 2; TODO make this not directly write to rc and use helper fns instead
    decRC(arg.b);
    return (struct BoolEnvAndExpr) { false, env, body.exprVal };
  } else if (expr->exprType == Add) {
    struct TwoExprs arg = expr->exprUnion.twoExprs;
    arg.a = interpretFully(env, arg.a);
    arg.b = interpretFully(env, arg.b);
    if (arg.a->exprType != LitInt || arg.b->exprType != LitInt) exit(1);
    Expr retVal = allocExpr(LitInt, (union ExprUnion) { .litInt = arg.a->exprUnion.litInt + arg.b->exprUnion.litInt });
    decRC(arg.a);
    decRC(arg.b);
    return (struct BoolEnvAndExpr) { true, env, retVal };
  } else if (expr->exprType == Mul) {
    struct TwoExprs arg = expr->exprUnion.twoExprs;
    arg.a = interpretFully(env, arg.a);
    arg.b = interpretFully(env, arg.b);
    if (arg.a->exprType != LitInt || arg.b->exprType != LitInt) exit(1);
    Expr retVal = allocExpr(LitInt, (union ExprUnion) { .litInt = arg.a->exprUnion.litInt * arg.b->exprUnion.litInt });
    decRC(arg.a);
    decRC(arg.b);
    return (struct BoolEnvAndExpr) { true, env, retVal };
  } else if (expr->exprType == Ifz) {
    struct ThreeExprs arg = expr->exprUnion.threeExprs;
    arg.a = interpretFully(env, arg.a);
    if (arg.a->exprType != LitInt) exit(1);
    bool cond = arg.a->exprUnion.litInt == 0;
    decRC(arg.a);
    return (struct BoolEnvAndExpr) { false, env, cond ? arg.b : arg.c };
  } else if (expr->exprType == MakeThunk) {
    Expr arg = expr->exprUnion.expr;
    Expr retVal = allocExpr(Thunk, (union ExprUnion) { .boolEnvAndExpr = (struct BoolEnvAndExpr) { false, env, arg } });
    incRC(arg);
    return (struct BoolEnvAndExpr) { true, env, retVal };
  } else if (expr->exprType == ForceThunk) {
    Expr arg = expr->exprUnion.expr;
    arg = interpretFully(env, arg);
    if (arg->exprType != Thunk) exit(1);
    struct BoolEnvAndExpr* thunk = &arg->exprUnion.boolEnvAndExpr;
    return (struct BoolEnvAndExpr) { false, env, force(thunk) };
  } else {
    return (struct BoolEnvAndExpr) { true, env, expr };
  }
}

Expr interpretFully(struct Env* env, Expr expr) {
  incRCEnv(env);
  incRC(expr);

  struct BoolEnvAndExpr result = { false, env, expr };
  do {
    struct BoolEnvAndExpr oldResult = result;

    result = interpret(result.envVal, result.exprVal);

    incRCEnv(result.envVal);
    decRCEnv(oldResult.envVal);

    incRC(result.exprVal);
    decRC(oldResult.exprVal);

  } while (!result.boolVal);

  decRCEnv(result.envVal);

  return result.exprVal;
}

Expr var(int val) {
  Expr retVal = allocExpr(Var, (union ExprUnion) { .var = val });
  return retVal;
}

Expr lam(Expr body) {
  incRC(body);
  Expr retVal = allocExpr(Lam, (union ExprUnion) { .envAndExpr = (struct EnvAndExpr) { 0, body } });
  return retVal;
}

Expr app(Expr a, Expr b) {
  incRC(a);
  incRC(b);
  Expr retVal = allocExpr(App, (union ExprUnion) { .twoExprs = (struct TwoExprs) { a, b } });
  return retVal;
}

Expr add(Expr a, Expr b) {
  incRC(a);
  incRC(b);
  Expr retVal = allocExpr(Add, (union ExprUnion) { .twoExprs = (struct TwoExprs) { a, b } });
  return retVal;
}

Expr mul(Expr a, Expr b) {
  incRC(a);
  incRC(b);
  Expr retVal = allocExpr(Mul, (union ExprUnion) { .twoExprs = (struct TwoExprs) { a, b } });
  return retVal;
}

Expr ifz(Expr a, Expr b, Expr c) {
  incRC(a);
  incRC(b);
  incRC(c);
  Expr retVal = allocExpr(Ifz, (union ExprUnion) { .threeExprs = (struct ThreeExprs) { a, b, c } });
  return retVal;
}

Expr litInt(intType val) {
  Expr retVal = allocExpr(LitInt, (union ExprUnion) { .litInt = val });
  return retVal;
}

Expr makeThunk(Expr a) {
  incRC(a);
  Expr retVal = allocExpr(MakeThunk, (union ExprUnion) { .expr = a });
  return retVal;
}

Expr forceThunk(Expr a) {
  incRC(a);
  Expr retVal = allocExpr(ForceThunk, (union ExprUnion) { .expr = a });
  return retVal;
}

void assert(bool b) {
  if (!b)
    exit(1);
}

char peekFor(char** str, char minC, char maxC) {
  return **str >= minC && **str <= maxC;
}

char peekForChr(char** str, char c) {
  return **str == c;
}

char tryConsoom(char** str, char minC, char maxC) {
  char chr = **str;
  bool found = peekFor(str, minC, maxC);
  if (found)
    ++*str;
  return found ? chr : 0;
}

char tryConsoomChr(char** str, char c) {
  return tryConsoom(str, c, c);
}

bool tryConsoomWhitespace(char** str) {
  return tryConsoomChr(str, ' ') || tryConsoomChr(str, '\t') || tryConsoomChr(str, '\n') || tryConsoomChr(str, '\r');
}

void consoomWhitespaces(char** str) {
  while (tryConsoomWhitespace(str));
}

bool tryConsoomWhitespaces1(char** str) {
  if (!tryConsoomWhitespace(str)) return false;
  consoomWhitespaces(str);
  return true;
}

int varStrToIntName(char* varStack[], int varStackPtr, char* str) {
  int i = varStackPtr;
  while (i > 0)
    if (strcmp(varStack[--i], str) == 0)
      return varStackPtr-i-1;
  exit(1);
}

char* parseVar(char** str) {
  char* startPt = *str;
  while (tryConsoom(str, 'a', 'z') || tryConsoom(str, 'A', 'Z') || tryConsoom(str, '0', '9') || tryConsoomChr(str, '-') || tryConsoomChr(str, '_'));
  if (*str == startPt) exit(0);
  int len = *str - startPt + 1;
  char* retVal = malloc(len);
  memcpy(retVal, startPt, len-1);
  retVal[len-1] = 0;
  return retVal;
}

Expr parseExpr(char* varStack[], int* varStackPtr, char** str) {
  consoomWhitespaces(str);
  if (peekFor(str, 'a', 'z') || peekFor(str, 'A', 'Z') || peekForChr(str, '_')) {
    char* varA = parseVar(str);

    if (strcmp(varA, "ifz") == 0) {
      consoomWhitespaces(str);
      assert(tryConsoomChr(str, '('));

      Expr a = parseExpr(varStack, varStackPtr, str);

      consoomWhitespaces(str);
      assert(tryConsoomChr(str, ')'));

      consoomWhitespaces(str);
      char* varB = parseVar(str);
      assert(strcmp(varB, "then") == 0);

      consoomWhitespaces(str);
      assert(tryConsoomChr(str, '('));

      Expr b = parseExpr(varStack, varStackPtr, str);

      consoomWhitespaces(str);
      assert(tryConsoomChr(str, ')'));

      consoomWhitespaces(str);
      char* varC = parseVar(str);
      assert(strcmp(varC, "else") == 0);

      consoomWhitespaces(str);
      assert(tryConsoomChr(str, '('));

      Expr c = parseExpr(varStack, varStackPtr, str);

      consoomWhitespaces(str);
      assert(tryConsoomChr(str, ')'));

      return ifz(a, b, c);
    } else
      return var(varStrToIntName(varStack, *varStackPtr, varA));
  } else if (tryConsoomChr(str, '\\')) {
    char* name = parseVar(str);

    consoomWhitespaces(str);
    assert(tryConsoomChr(str, '.'));

    varStack[(*varStackPtr)++] = name;

    Expr body = parseExpr(varStack, varStackPtr, str);

    --*varStackPtr;

    return lam(body);
  } else if (tryConsoomChr(str, '(')) {
    Expr a = parseExpr(varStack, varStackPtr, str);

    bool foundSpace = tryConsoomWhitespaces1(str);
    bool foundPlus = tryConsoomChr(str, '+');
    bool foundMul = !foundPlus && tryConsoomChr(str, '*');
    if (!foundPlus && !foundMul) assert(foundSpace);

    Expr b = parseExpr(varStack, varStackPtr, str);

    consoomWhitespaces(str);
    assert(tryConsoomChr(str, ')'));

    if (foundPlus)
      return add(a, b);
    else if (foundMul)
      return mul(a, b);
    else
      return app(a, b);
  } else if (peekFor(str, '0', '9') || peekForChr(str, '-')) {
    char* num = parseVar(str);
    return litInt(atoi(num));
  } else if (tryConsoomChr(str, '[')) {
    Expr a = parseExpr(varStack, varStackPtr, str);

    consoomWhitespaces(str);
    assert(tryConsoomChr(str, ']'));

    return makeThunk(a);
  } else if (tryConsoomChr(str, '{')) {
    Expr a = parseExpr(varStack, varStackPtr, str);

    consoomWhitespaces(str);
    assert(tryConsoomChr(str, '}'));

    return forceThunk(a);
  }
}

int main() {
  char* varStack[1000];
  int varStackPtr = 0;
  char* toParse = "(((\\f. (\\x. (f \\v. ((x x) v)) \\x. (f \\v. ((x x) v))) \\self. \\r. \\x. ifz (x) then (r) else (((self (r * x)) (x + -1)))) 1) 10000000)";
  Expr mainExpr = parseExpr(varStack, &varStackPtr, &toParse);

  incRC(mainExpr);

  Expr mainReduced = interpretFully(0, mainExpr);
  printf("value: ");
  printExpr(mainReduced);
  printf("\n");

  decRC(mainExpr);
  decRC(mainReduced);

  freeStacks();

  return 0;
}
