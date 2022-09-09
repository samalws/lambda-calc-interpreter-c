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

struct ExprStruct {
  int rc;
  enum ExprType exprType;
};

typedef struct ExprStruct* Expr;

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
#define enumVal(expr) expr->exprType
#define argLoc(relTo,t) ((t*) (relTo+1))
#define viewArgAs(relTo,t) *argLoc(relTo,t)
#define viewExprAs(expr,exTy,t) ((enumVal(expr) == exTy) ? viewArgAs(expr,t) : exitAs(t,1))
#define allocExpr(name,t,exTy,val) \
  Expr name = malloc(sizeof(struct ExprStruct) + sizeof(t)); \
  name->rc = 0; \
  name->exprType = exTy; \
  *argLoc(name,t) = val;

void incRC(Expr expr);
void decRC(Expr expr);
void printExpr(Expr expr);
Expr subst(Expr substIn, const char* var, Expr val);
Expr call(Expr f, Expr x);
struct InterpretResult interpret(Expr expr);
Expr interpretFully(Expr expr);

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

  free(expr);
}

void printExpr(Expr expr) {
  if (enumVal(expr) == Var) {
    const char* arg = viewArgAs(expr, const char*);
    printf("%s", arg);
  } else if (enumVal(expr) == Lam) {
    struct LamArg arg = viewArgAs(expr, struct LamArg);
    printf("(\\%s. ", arg.argName);
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

Expr subst(Expr substIn, const char* var, Expr val) {
  if (enumVal(substIn) == Var) {
    const char* arg = viewArgAs(substIn, const char*);
    return (strcmp(arg, var) == 0) ? val : substIn;
  } else if (enumVal(substIn) == Lam) {
    struct LamArg arg = viewArgAs(substIn, struct LamArg);
    if (strcmp(arg.argName, var) == 0)
      return substIn;
    else {
      arg.body = subst(arg.body, var, val);
      incRC(arg.body);
      allocExpr(retVal, struct LamArg, Lam, arg);
      return retVal;
    }
  } else if (enumVal(substIn) == App) {
    struct TwoExprs arg = viewArgAs(substIn, struct TwoExprs);
    arg.a = subst(arg.a, var, val);
    arg.b = subst(arg.b, var, val);
    incRC(arg.a);
    incRC(arg.b);
    allocExpr(retVal, struct TwoExprs, App, arg);
    return retVal;
  } else if (enumVal(substIn) == Add) {
    struct TwoExprs arg = viewArgAs(substIn, struct TwoExprs);
    arg.a = subst(arg.a, var, val);
    arg.b = subst(arg.b, var, val);
    incRC(arg.a);
    incRC(arg.b);
    allocExpr(retVal, struct TwoExprs, Add, arg);
    return retVal;
  } else if (enumVal(substIn) == Mul) {
    struct TwoExprs arg = viewArgAs(substIn, struct TwoExprs);
    arg.a = subst(arg.a, var, val);
    arg.b = subst(arg.b, var, val);
    incRC(arg.a);
    incRC(arg.b);
    allocExpr(retVal, struct TwoExprs, Mul, arg);
    return retVal;
  } else if (enumVal(substIn) == Ifz) {
    struct ThreeExprs arg = viewArgAs(substIn, struct ThreeExprs);
    arg.a = subst(arg.a, var, val);
    arg.b = subst(arg.b, var, val);
    arg.c = subst(arg.c, var, val);
    incRC(arg.a);
    incRC(arg.b);
    incRC(arg.c);
    allocExpr(retVal, struct ThreeExprs, Ifz, arg);
    return retVal;
  } else if (enumVal(substIn) == LitInt) {
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
  if (enumVal(expr) == App) {
    struct TwoExprs arg = viewArgAs(expr, struct TwoExprs);
    arg.a = interpretFully(arg.a);
    arg.b = interpretFully(arg.b);
    Expr called = call(arg.a, arg.b);
    decRC(arg.a);
    decRC(arg.b);
    return (struct InterpretResult) { false, called };
  } else if (enumVal(expr) == Add) {
    struct TwoExprs arg = viewArgAs(expr, struct TwoExprs);
    arg.a = interpretFully(arg.a);
    arg.b = interpretFully(arg.b);
    allocExpr(retVal, intType, LitInt, viewExprAs(arg.a, LitInt, intType) + viewExprAs(arg.b, LitInt, intType));
    decRC(arg.a);
    decRC(arg.b);
    return (struct InterpretResult) { true, retVal };
  } else if (enumVal(expr) == Mul) {
    struct TwoExprs arg = viewArgAs(expr, struct TwoExprs);
    arg.a = interpretFully(arg.a);
    arg.b = interpretFully(arg.b);
    allocExpr(retVal, intType, LitInt, viewExprAs(arg.a, LitInt, intType) * viewExprAs(arg.b, LitInt, intType));
    decRC(arg.a);
    decRC(arg.b);
    return (struct InterpretResult) { true, retVal };
  } else if (enumVal(expr) == Ifz) {
    struct ThreeExprs arg = viewArgAs(expr, struct ThreeExprs);
    arg.a = interpretFully(arg.a);
    bool cond = viewExprAs(arg.a, LitInt, intType) == 0;
    decRC(arg.a);
    return (struct InterpretResult) { false, cond ? arg.b : arg.c };
  } else {
    return (struct InterpretResult) { true, expr };
  }
}

Expr interpretFully(Expr expr) {
  incRC(expr);
  struct InterpretResult result = { false, expr };
  do {
    Expr oldExpr = result.newExpr;
    result = interpret(result.newExpr);
    incRC(result.newExpr);
    decRC(oldExpr);
  } while (!result.isDone);
  return result.newExpr;
}

Expr var(const char* val) {
  allocExpr(retVal, const char*, Var, val);
  return retVal;
}

Expr lam(const char* argName, Expr body) {
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

int main() {
  Expr o = lam("x", app(var("f"), lam("v", app(app(var("x"), var("x")), var("v")))));
  Expr y = lam("f", app(o, o));

  Expr factorial = app(app(y, lam("self", lam("r", lam("x", ifz(var("x"), var("r"), app(app(var("self"), mul(var("r"), var("x"))), add(var("x"), litInt(-1)))))))), litInt(1));

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
