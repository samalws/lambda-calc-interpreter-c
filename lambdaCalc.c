// TODO lazy evaluation
// TODO heap allocated stack / manually do the stack
// TODO different exit codes per thing
// TODO multithreading?
//      should be fairly easy to make everything atomic, since ints should be atomic already;
//      might need to do some jank with "int newVal = rc--"
//      also either need to make to-execute pool
//      and decide when to run in parallel: let the programmer specify which stuff should be parallelized?
//      look into pthread.h
// TODO maybe malloc a big chunk all at once

#include <stdio.h>
#include <stdlib.h>

typedef enum { false, true } bool;

typedef long intType;

void* exitWithPtr(int status) {
  exit(status);
  return 0;
}

enum ExprType {
  Var,
  Lam,
  App,
  Add,
  Mul,
  Ifz,
  LitInt
};

struct ExprStruct {
  int rc;
  enum ExprType exprType;
};

typedef struct ExprStruct* Expr;

struct LamArg {
  int argName;
  Expr body;
};

struct TwoExprs {
  Expr a;
  Expr b;
};

struct ThreeExprs {
  Expr a;
  Expr b;
  Expr c;
};

struct BoolAndExpr {
  bool boolVal;
  Expr exprVal;
};

#define exitAs(t,status) *((t*) exitWithPtr(status))
#define enumVal(expr) expr->exprType
#define argLoc(relTo,t) ((t*) (relTo+1))
#define viewArgAs(relTo,t) *argLoc(relTo,t)
#define viewExprAs(expr,exTy,t) ((enumVal(expr) == exTy) ? viewArgAs(expr,t) : exitAs(t,1))
#define allocExpr(name,t,exTy,val) \
  Expr name = mallocExpr(); \
  name->rc = 0; \
  name->exprType = exTy; \
  *argLoc(name,t) = val;

Expr mallocExpr();
void freeExpr(Expr expr);
void incRC(Expr expr);
void decRC(Expr expr);
void printExpr(Expr expr);
struct BoolAndExpr subst(Expr substIn, int var, Expr val);
Expr call(Expr f, Expr x);
struct BoolAndExpr interpret(Expr expr);
Expr interpretFully(Expr expr);

#define nFreedStack 1000
Expr freedStack[1000];
int freedStackPtr = 0;
// freedStackPtr-1 is the highest valid value on freedStack
// int nStackFull = 0;

Expr mallocExpr() {
  if (freedStackPtr == 0)
    return malloc(sizeof(struct ExprStruct) + sizeof(struct ThreeExprs));
  else
    return freedStack[--freedStackPtr];
}

void freeExpr(Expr expr) {
  // if (freedStackPtr == nFreedStack) nStackFull++;
  if (freedStackPtr == nFreedStack)
    free(expr);
  else
    freedStack[freedStackPtr++] = expr;
}

void incRC(Expr expr) {
  expr->rc++;
}

void decRC(Expr expr) {
  expr->rc--;
  if (expr->rc > 0)
    return;
  else if (expr->rc < 0)
    exit(1);

  // expr->rc == 0

  if (enumVal(expr) == Lam) {
    struct LamArg arg = viewArgAs(expr, struct LamArg);
    decRC(arg.body);
  } else if (enumVal(expr) == App || enumVal(expr) == Add || enumVal(expr) == Mul) {
    struct TwoExprs arg = viewArgAs(expr, struct TwoExprs);
    decRC(arg.a);
    decRC(arg.b);
  } else if (enumVal(expr) == Ifz) {
    struct ThreeExprs arg = viewArgAs(expr, struct ThreeExprs);
    decRC(arg.a);
    decRC(arg.b);
    decRC(arg.c);
  }

  freeExpr(expr);
}

void printExpr(Expr expr) {
  if (enumVal(expr) == Var) {
    int arg = viewArgAs(expr, int);
    printf("v%d", arg);
  } else if (enumVal(expr) == Lam) {
    struct LamArg arg = viewArgAs(expr, struct LamArg);
    printf("(\\v%d. ", arg.argName);
    printExpr(arg.body);
    printf(")");
  } else if (enumVal(expr) == App) {
    struct TwoExprs arg = viewArgAs(expr, struct TwoExprs);
    printf("(");
    printExpr(arg.a);
    printf(" ");
    printExpr(arg.b);
    printf(")");
  } else if (enumVal(expr) == Add) {
    struct TwoExprs arg = viewArgAs(expr, struct TwoExprs);
    printf("(");
    printExpr(arg.a);
    printf(" + ");
    printExpr(arg.b);
    printf(")");
  } else if (enumVal(expr) == Mul) {
    struct TwoExprs arg = viewArgAs(expr, struct TwoExprs);
    printf("(");
    printExpr(arg.a);
    printf(" * ");
    printExpr(arg.b);
    printf(")");
  } else if (enumVal(expr) == Ifz) {
    struct ThreeExprs arg = viewArgAs(expr, struct ThreeExprs);
    printf("if ");
    printExpr(arg.a);
    printf(" then ");
    printExpr(arg.b);
    printf(" else ");
    printExpr(arg.c);
  } else if (enumVal(expr) == LitInt) {
    long arg = viewArgAs(expr, intType);
    printf("%ld", arg);
  } else {
    exit(1);
  }
}

// bool is whether anything changed
struct BoolAndExpr subst(Expr substIn, int var, Expr val) {
  if (enumVal(substIn) == Var) {
    int arg = viewArgAs(substIn, int);
    return (struct BoolAndExpr) { arg == var, arg == var ? val : substIn };
  } else if (enumVal(substIn) == Lam) {
    struct LamArg arg = viewArgAs(substIn, struct LamArg);
    if (arg.argName == var)
      return (struct BoolAndExpr) { false, substIn };
    else {
      struct BoolAndExpr substBody = subst(arg.body, var, val);
      if (!substBody.boolVal)
        return (struct BoolAndExpr) { false, substIn };
      arg.body = substBody.exprVal;
      incRC(arg.body);
      allocExpr(retVal, struct LamArg, Lam, arg);
      return (struct BoolAndExpr) { true, retVal };
    }
  } else if (enumVal(substIn) == App) {
    struct TwoExprs arg = viewArgAs(substIn, struct TwoExprs);
    struct BoolAndExpr substA = subst(arg.a, var, val);
    struct BoolAndExpr substB = subst(arg.b, var, val);
    if (!(substA.boolVal || substB.boolVal))
      return (struct BoolAndExpr) { false, substIn };
    arg.a = substA.exprVal;
    arg.b = substB.exprVal;
    incRC(arg.a);
    incRC(arg.b);
    allocExpr(retVal, struct TwoExprs, App, arg);
    return (struct BoolAndExpr) { true, retVal };
  } else if (enumVal(substIn) == Add) {
    struct TwoExprs arg = viewArgAs(substIn, struct TwoExprs);
    struct BoolAndExpr substA = subst(arg.a, var, val);
    struct BoolAndExpr substB = subst(arg.b, var, val);
    if (!(substA.boolVal || substB.boolVal))
      return (struct BoolAndExpr) { false, substIn };
    arg.a = substA.exprVal;
    arg.b = substB.exprVal;
    incRC(arg.a);
    incRC(arg.b);
    allocExpr(retVal, struct TwoExprs, Add, arg);
    return (struct BoolAndExpr) { true, retVal };
  } else if (enumVal(substIn) == Mul) {
    struct TwoExprs arg = viewArgAs(substIn, struct TwoExprs);
    struct BoolAndExpr substA = subst(arg.a, var, val);
    struct BoolAndExpr substB = subst(arg.b, var, val);
    if (!(substA.boolVal || substB.boolVal))
      return (struct BoolAndExpr) { false, substIn };
    arg.a = substA.exprVal;
    arg.b = substB.exprVal;
    incRC(arg.a);
    incRC(arg.b);
    allocExpr(retVal, struct TwoExprs, Mul, arg);
    return (struct BoolAndExpr) { true, retVal };
  } else if (enumVal(substIn) == Ifz) {
    struct ThreeExprs arg = viewArgAs(substIn, struct ThreeExprs);
    struct BoolAndExpr substA = subst(arg.a, var, val);
    struct BoolAndExpr substB = subst(arg.b, var, val);
    struct BoolAndExpr substC = subst(arg.c, var, val);
    if (!(substA.boolVal || substB.boolVal || substC.boolVal))
      return (struct BoolAndExpr) { false, substIn };
    arg.a = substA.exprVal;
    arg.b = substB.exprVal;
    arg.c = substC.exprVal;
    incRC(arg.a);
    incRC(arg.b);
    incRC(arg.c);
    allocExpr(retVal, struct ThreeExprs, Ifz, arg);
    return (struct BoolAndExpr) { true, retVal };
  } else if (enumVal(substIn) == LitInt) {
    return (struct BoolAndExpr) { false, substIn };
  } else {
    exit(1);
  }
}

Expr call(Expr f, Expr x) {
  struct LamArg arg = viewArgAs(f, struct LamArg);
  return subst(arg.body, arg.argName, x).exprVal;
}

// bool is whether it's done interpreting
struct BoolAndExpr interpret(Expr expr) {
  if (enumVal(expr) == App) {
    struct TwoExprs arg = viewArgAs(expr, struct TwoExprs);
    arg.a = interpretFully(arg.a);
    arg.b = interpretFully(arg.b);
    Expr called = call(arg.a, arg.b);
    decRC(arg.a);
    decRC(arg.b);
    return (struct BoolAndExpr) { false, called };
  } else if (enumVal(expr) == Add) {
    struct TwoExprs arg = viewArgAs(expr, struct TwoExprs);
    arg.a = interpretFully(arg.a);
    arg.b = interpretFully(arg.b);
    allocExpr(retVal, intType, LitInt, viewExprAs(arg.a, LitInt, intType) + viewExprAs(arg.b, LitInt, intType));
    decRC(arg.a);
    decRC(arg.b);
    return (struct BoolAndExpr) { true, retVal };
  } else if (enumVal(expr) == Mul) {
    struct TwoExprs arg = viewArgAs(expr, struct TwoExprs);
    arg.a = interpretFully(arg.a);
    arg.b = interpretFully(arg.b);
    allocExpr(retVal, intType, LitInt, viewExprAs(arg.a, LitInt, intType) * viewExprAs(arg.b, LitInt, intType));
    decRC(arg.a);
    decRC(arg.b);
    return (struct BoolAndExpr) { true, retVal };
  } else if (enumVal(expr) == Ifz) {
    struct ThreeExprs arg = viewArgAs(expr, struct ThreeExprs);
    arg.a = interpretFully(arg.a);
    bool cond = viewExprAs(arg.a, LitInt, intType) == 0;
    decRC(arg.a);
    return (struct BoolAndExpr) { false, cond ? arg.b : arg.c };
  } else {
    return (struct BoolAndExpr) { true, expr };
  }
}

Expr interpretFully(Expr expr) {
  incRC(expr);
  struct BoolAndExpr result = { false, expr };
  do {
    Expr oldExpr = result.exprVal;
    result = interpret(result.exprVal);
    incRC(result.exprVal);
    decRC(oldExpr);
  } while (!result.boolVal);
  return result.exprVal;
}

Expr var(int val) {
  allocExpr(retVal, int, Var, val);
  return retVal;
}

Expr lam(int argName, Expr body) {
  incRC(body);
  allocExpr(retVal, struct LamArg, Lam, ((struct LamArg) { argName, body }));
  return retVal;
}

Expr app(Expr a, Expr b) {
  incRC(a);
  incRC(b);
  allocExpr(retVal, struct TwoExprs, App, ((struct TwoExprs) { a, b }));
  return retVal;
}

Expr add(Expr a, Expr b) {
  incRC(a);
  incRC(b);
  allocExpr(retVal, struct TwoExprs, Add, ((struct TwoExprs) { a, b }));
  return retVal;
}

Expr mul(Expr a, Expr b) {
  incRC(a);
  incRC(b);
  allocExpr(retVal, struct TwoExprs, Mul, ((struct TwoExprs) { a, b }));
  return retVal;
}

Expr ifz(Expr a, Expr b, Expr c) {
  incRC(a);
  incRC(b);
  incRC(c);
  allocExpr(retVal, struct ThreeExprs, Ifz, ((struct ThreeExprs) { a, b, c }));
  return retVal;
}

Expr litInt(intType val) {
  allocExpr(retVal, intType, LitInt, val);
  return retVal;
}

long factorial(int inp) {
  long result = 1;
  for (int i = 1; i <= inp; i++)
    result *= i;
  return result;
}

int main() {
  int i = 0;
  const int F = i++;
  const int R = i++;
  const int S = i++;
  const int V = i++;
  const int X = i++;

  Expr o = lam(X, app(var(F), lam(V, app(app(var(X), var(X)), var(V)))));
  Expr y = lam(F, app(o, o));

  Expr factorial = app(app(y, lam(S, lam(R, lam(X, ifz(var(X), var(R), app(app(var(S), mul(var(R), var(X))), add(var(X), litInt(-1)))))))), litInt(1));

  Expr mainExpr = app(factorial, litInt(10000000));
  incRC(mainExpr);

  Expr mainReduced = interpretFully(mainExpr);
  printf("value: ");
  printExpr(mainReduced);
  printf("\n");

  decRC(mainExpr);
  decRC(mainReduced);

  return 0;
}
