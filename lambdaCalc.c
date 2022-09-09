// TODO heap allocated stack / manually do the stack
// TODO multithreading?
//      should be fairly easy to make everything atomic, since ints should be atomic already;
//      might need to do some jank with "int newVal = rc--"
//      also either need to make to-execute pool
//      and decide when to run in parallel: let the programmer specify which stuff should be parallelized?
//      look into pthread.h
// TODO change subst to map

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
  LitInt,
  MakeThunk,
  ForceThunk,
  Thunk
};

struct ExprStruct {
  int rc;
  enum ExprType exprType;
};

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
inline void incRC(Expr expr);
void decRC(Expr expr);
void printExpr(Expr expr);
struct BoolAndExpr subst(Expr substIn, int var, Expr val);
inline Expr call(Expr f, Expr x);
Expr force(struct BoolAndExpr* thunk);
inline struct BoolAndExpr interpret(Expr expr);
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
  // if (freedStackPtr == nFreedStack) ++nStackFull;
  if (freedStackPtr == nFreedStack)
    free(expr);
  else
    freedStack[freedStackPtr++] = expr;
}

inline void incRC(Expr expr) {
  ++expr->rc;
}

void decRC(Expr expr) {
  --expr->rc;
  if (expr->rc > 0)
    return;
  else if (expr->rc < 0)
    exit(2);

  // expr->rc == 0

  if (enumVal(expr) == Lam || enumVal(expr) == MakeThunk || enumVal(expr) == ForceThunk) {
    Expr arg = viewArgAs(expr, Expr);
    decRC(arg);
  } else if (enumVal(expr) == App || enumVal(expr) == Add || enumVal(expr) == Mul) {
    struct TwoExprs arg = viewArgAs(expr, struct TwoExprs);
    decRC(arg.a);
    decRC(arg.b);
  } else if (enumVal(expr) == Ifz) {
    struct ThreeExprs arg = viewArgAs(expr, struct ThreeExprs);
    decRC(arg.a);
    decRC(arg.b);
    decRC(arg.c);
  } else if (enumVal(expr) == Thunk) {
    struct BoolAndExpr arg = viewArgAs(expr, struct BoolAndExpr);
    decRC(arg.exprVal);
  }

  freeExpr(expr);
}

void printExpr(Expr expr) {
  if (enumVal(expr) == Var) {
    int arg = viewArgAs(expr, int);
    printf("v%d", arg);
  } else if (enumVal(expr) == Lam) {
    Expr arg = viewArgAs(expr, Expr);
    printf("(\\ ");
    printExpr(arg);
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
  } else if (enumVal(expr) == MakeThunk) {
    Expr arg = viewArgAs(expr, Expr);
    printf("[");
    printExpr(arg);
    printf("]");
  } else if (enumVal(expr) == ForceThunk) {
    Expr arg = viewArgAs(expr, Expr);
    printf("$(");
    printExpr(arg);
    printf(")");
  } else if (enumVal(expr) == Thunk) {
    printf("THUNK");
  } else if (enumVal(expr) == LitInt) {
    long arg = viewArgAs(expr, intType);
    printf("%ld", arg);
  } else {
    exit(3);
  }
}

// bool is whether anything changed
struct BoolAndExpr subst(Expr substIn, int var, Expr val) {
  if (enumVal(substIn) == Var) {
    int arg = viewArgAs(substIn, int);
    if (arg == var)
      return (struct BoolAndExpr) { true, val };
    else if (arg > var) {
      allocExpr(retVal, int, Var, arg-1);
      return (struct BoolAndExpr) { true, retVal };
    } else
      return (struct BoolAndExpr) { false, substIn };
  } else if (enumVal(substIn) == Lam) {
    Expr arg = viewArgAs(substIn, Expr);
    struct BoolAndExpr substBody = subst(arg, var+1, val);
    if (!substBody.boolVal)
      return (struct BoolAndExpr) { false, substIn };
    arg = substBody.exprVal;
    incRC(arg);
    allocExpr(retVal, Expr, Lam, arg);
    return (struct BoolAndExpr) { true, retVal };
  } else if (enumVal(substIn) == App || enumVal(substIn) == Add || enumVal(substIn) == Mul) {
    struct TwoExprs arg = viewArgAs(substIn, struct TwoExprs);
    struct BoolAndExpr substA = subst(arg.a, var, val);
    struct BoolAndExpr substB = subst(arg.b, var, val);
    if (!(substA.boolVal || substB.boolVal))
      return (struct BoolAndExpr) { false, substIn };
    arg.a = substA.exprVal;
    arg.b = substB.exprVal;
    incRC(arg.a);
    incRC(arg.b);
    allocExpr(retVal, struct TwoExprs, enumVal(substIn), arg);
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
  } else if (enumVal(substIn) == MakeThunk || enumVal(substIn) == ForceThunk) {
    Expr arg = viewArgAs(substIn, Expr);
    struct BoolAndExpr substArg = subst(arg, var, val);
    if (!substArg.boolVal)
      return (struct BoolAndExpr) { false, substIn };
    arg = substArg.exprVal;
    incRC(arg);
    allocExpr(retVal, Expr, enumVal(substIn), arg);
    return (struct BoolAndExpr) { true, retVal };
  } else if (enumVal(substIn) == LitInt || enumVal(substIn) == Thunk) {
    return (struct BoolAndExpr) { false, substIn };
  } else {
    exit(4);
  }
}

inline Expr call(Expr f, Expr x) {
  return subst(viewArgAs(f, Expr), 0, x).exprVal;
}

Expr force(struct BoolAndExpr* thunk) {
  if (!thunk->boolVal) {
    thunk->exprVal = interpretFully(thunk->exprVal);
    thunk->boolVal = true;
  }
  return thunk->exprVal;
}

// bool is whether it's done interpreting
inline struct BoolAndExpr interpret(Expr expr) {
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
  } else if (enumVal(expr) == MakeThunk) {
    Expr arg = viewArgAs(expr, Expr);
    allocExpr(retVal, struct BoolAndExpr, Thunk, ((struct BoolAndExpr) { false, arg }));
    incRC(arg);
    return (struct BoolAndExpr) { true, retVal };
  } else if (enumVal(expr) == ForceThunk) {
    Expr arg = viewArgAs(expr, Expr);
    arg = interpretFully(arg);
    if (enumVal(arg) != Thunk) exit(5);
    struct BoolAndExpr* thunk = argLoc(arg, struct BoolAndExpr);
    return (struct BoolAndExpr) { false, force(thunk) };
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

Expr lam(Expr body) {
  incRC(body);
  allocExpr(retVal, Expr, Lam, body);
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

Expr makeThunk(Expr a) {
  incRC(a);
  allocExpr(retVal, Expr, MakeThunk, a);
  return retVal;
}

Expr forceThunk(Expr a) {
  incRC(a);
  allocExpr(retVal, Expr, ForceThunk, a);
  return retVal;
}

int main() {
  Expr o = lam(app(var(1), lam(app(app(var(1), var(1)), var(0)))));
  Expr y = lam(app(o, o));

  Expr factorial = app(app(y, lam(lam(lam(ifz(var(0), var(1), app(app(var(2), mul(var(1), var(0))), add(var(0), litInt(-1)))))))), litInt(1));

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
