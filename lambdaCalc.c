#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef const enum ExprType* Expr;

struct LamArg {
  const char* argName;
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

struct InterpretResult {
  bool isDone;
  Expr newExpr;
};

#define exitAs(t,status) *((t*) exitWithPtr(status))
#define argLoc(relTo,t) ((t*) (relTo+1))
#define viewArgAs(relTo,t) *argLoc(relTo,t)
#define viewExprAs(expr,exprType,t) ((*expr == exprType) ? viewArgAs(expr,t) : exitAs(t,1))
#define allocExpr(name,t,exprType,val) \
  enum ExprType* _##name = malloc(sizeof(const enum ExprType) + sizeof(t)); \
  *_##name = exprType; \
  *argLoc(_##name,t) = val; \
  Expr name = _##name;
  // TODO eventually free

void printExpr(Expr expr);
Expr subst(Expr substIn, const char* var, Expr val);
Expr call(Expr f, Expr x);
struct InterpretResult interpret(Expr expr);
Expr interpretFully(Expr expr);

void printExpr(Expr expr) {
  if (*expr == Var) {
    const char* arg = viewArgAs(expr, const char*);
    printf("%s", arg);
  } else if (*expr == Lam) {
    struct LamArg arg = viewArgAs(expr, struct LamArg);
    printf("(\\%s. ", arg.argName);
    printExpr(arg.body);
    printf(")");
  } else if (*expr == App) {
    struct TwoExprs arg = viewArgAs(expr, struct TwoExprs);
    printf("(");
    printExpr(arg.a);
    printf(" ");
    printExpr(arg.b);
    printf(")");
  } else if (*expr == Add) {
    struct TwoExprs arg = viewArgAs(expr, struct TwoExprs);
    printf("(");
    printExpr(arg.a);
    printf(" + ");
    printExpr(arg.b);
    printf(")");
  } else if (*expr == Mul) {
    struct TwoExprs arg = viewArgAs(expr, struct TwoExprs);
    printf("(");
    printExpr(arg.a);
    printf(" * ");
    printExpr(arg.b);
    printf(")");
  } else if (*expr == Ifz) {
    struct ThreeExprs arg = viewArgAs(expr, struct ThreeExprs);
    printf("if ");
    printExpr(arg.a);
    printf(" then ");
    printExpr(arg.b);
    printf("else");
    printExpr(arg.c);
  } else if (*expr == LitInt) {
    intType arg = viewArgAs(expr, intType);
    printf("%ld", arg);
  } else {
    exit(1);
  }
}

Expr subst(Expr substIn, const char* var, Expr val) {
  if (*substIn == Var) {
    const char* arg = viewArgAs(substIn, const char*);
    return (strcmp(arg, var) == 0) ? val : substIn;
  } else if (*substIn == Lam) {
    struct LamArg arg = viewArgAs(substIn, struct LamArg);
    if (strcmp(arg.argName, var) == 0)
      return substIn;
    else {
      arg.body = subst(arg.body, var, val);
      allocExpr(retVal, struct LamArg, Lam, arg);
      return retVal;
    }
  } else if (*substIn == App) {
    struct TwoExprs arg = viewArgAs(substIn, struct TwoExprs);
    arg.a = subst(arg.a, var, val);
    arg.b = subst(arg.b, var, val);
    allocExpr(retVal, struct TwoExprs, App, arg);
    return retVal;
  } else if (*substIn == Add) {
    struct TwoExprs arg = viewArgAs(substIn, struct TwoExprs);
    arg.a = subst(arg.a, var, val);
    arg.b = subst(arg.b, var, val);
    allocExpr(retVal, struct TwoExprs, Add, arg);
    return retVal;
  } else if (*substIn == Mul) {
    struct TwoExprs arg = viewArgAs(substIn, struct TwoExprs);
    arg.a = subst(arg.a, var, val);
    arg.b = subst(arg.b, var, val);
    allocExpr(retVal, struct TwoExprs, Mul, arg);
    return retVal;
  } else if (*substIn == Ifz) {
    struct ThreeExprs arg = viewArgAs(substIn, struct ThreeExprs);
    arg.a = subst(arg.a, var, val);
    arg.b = subst(arg.b, var, val);
    arg.c = subst(arg.c, var, val);
    allocExpr(retVal, struct ThreeExprs, Ifz, arg);
    return retVal;
  } else if (*substIn == LitInt) {
    return substIn;
  } else {
    exit(1);
  }
}

Expr call(Expr f, Expr x) {
  struct LamArg arg = viewArgAs(f, struct LamArg);
  return subst(arg.body, arg.argName, x);
}

struct InterpretResult interpret(Expr expr) {
  if (*expr == App) {
    struct TwoExprs arg = viewArgAs(expr, struct TwoExprs);
    arg.a = interpretFully(arg.a);
    arg.b = interpretFully(arg.b);
    return (struct InterpretResult) { false, call(arg.a, arg.b) };
  } else if (*expr == Add) {
    struct TwoExprs arg = viewArgAs(expr, struct TwoExprs);
    arg.a = interpretFully(arg.a);
    arg.b = interpretFully(arg.b);
    allocExpr(retVal, intType, LitInt, viewExprAs(arg.a, LitInt, intType) + viewExprAs(arg.b, LitInt, intType));
    return (struct InterpretResult) { true, retVal };
  } else if (*expr == Mul) {
    struct TwoExprs arg = viewArgAs(expr, struct TwoExprs);
    arg.a = interpretFully(arg.a);
    arg.b = interpretFully(arg.b);
    allocExpr(retVal, intType, LitInt, viewExprAs(arg.a, LitInt, intType) * viewExprAs(arg.b, LitInt, intType));
    return (struct InterpretResult) { true, retVal };
  } else if (*expr == Ifz) {
    struct ThreeExprs arg = viewArgAs(expr, struct ThreeExprs);
    arg.a = interpretFully(arg.a);
    return (struct InterpretResult) { false, viewExprAs(arg.a, LitInt, intType) == 0 ? arg.b : arg.c };
  } else {
    return (struct InterpretResult) { true, expr };
  }
}

Expr interpretFully(Expr expr) {
  struct InterpretResult result = { false, expr };
  do
    result = interpret(result.newExpr);
  while (!result.isDone);
  return result.newExpr;
}

Expr var(const char* val) {
  allocExpr(retVal, const char*, Var, val);
  return retVal;
}

Expr lam(const char* argName, Expr body) {
  allocExpr(retVal, struct LamArg, Lam, ((struct LamArg) { argName, body }));
  return retVal;
}

Expr app(Expr a, Expr b) {
  allocExpr(retVal, struct TwoExprs, App, ((struct TwoExprs) { a, b }));
  return retVal;
}

Expr add(Expr a, Expr b) {
  allocExpr(retVal, struct TwoExprs, Add, ((struct TwoExprs) { a, b }));
  return retVal;
}

Expr mul(Expr a, Expr b) {
  allocExpr(retVal, struct TwoExprs, Mul, ((struct TwoExprs) { a, b }));
  return retVal;
}

Expr ifz(Expr a, Expr b, Expr c) {
  allocExpr(retVal, struct ThreeExprs, Ifz, ((struct ThreeExprs) { a, b, c }));
  return retVal;
}

Expr litInt(intType val) {
  allocExpr(retVal, intType, LitInt, val);
  return retVal;
}

int main() {
  Expr o = lam("x", app(var("f"), lam("v", app(app(var("x"), var("x")), var("v")))));
  Expr y = lam("f", app(o, o));

  Expr factorial = app(y, lam("self", lam("x", ifz(var("x"), litInt(1), mul(var("x"), app(var("self"), add(var("x"), litInt(-1))))))));

  Expr mainExpr = app(factorial, litInt(20));

  Expr mainReduced = interpretFully(mainExpr);

  printf("value: ");
  printExpr(mainReduced);
  printf("\n");

  return 0;
}
