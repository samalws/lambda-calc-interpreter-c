// TODO heap allocated stack / manually do the stack
// TODO multithreading?
//      should be fairly easy to make everything atomic, since ints should be atomic already;
//      might need to do some jank with "int newVal = rc--"
//      also either need to make to-execute pool
//      and decide when to run in parallel: let the programmer specify which stuff should be parallelized?
//      look into pthread.h
// TODO use unions
// TODO freed stack for envs
// TODO inline decRC and decRCEnv? what percent of decs result in freeing it?

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

struct Env {
  struct Env * next;
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

inline Expr getVarEnv(struct Env* env, int var);
inline struct Env* pushEnv(struct Env* env, Expr expr);
inline void incRCEnv(struct Env* env);
void decRCEnv(struct Env* env);
Expr mallocExpr();
void freeExpr(Expr expr);
void freeExprStack();
inline void incRC(Expr expr);
void decRC(Expr expr);
void printExpr(Expr expr);
inline Expr force(struct BoolEnvAndExpr* thunk);
inline struct BoolEnvAndExpr interpret(struct Env* env, Expr expr);
Expr interpretFully(struct Env* env, Expr expr);

inline Expr getVarEnv(struct Env* env, int var) {
  while (var > 0) {
    env = env->next;
    --var;
  }

  return env->val;
}

inline struct Env* pushEnv(struct Env* env, Expr expr) {
  struct Env* retVal = malloc(sizeof(struct Env));
  retVal->next = env;
  retVal->val = expr;
  retVal->rc = 0;
  incRCEnv(env);
  incRC(expr);
  return retVal;
}

inline void incRCEnv(struct Env* env) {
  if (env == 0) return;
  ++env->rc;
}

void decRCEnv(struct Env* env) {
  if (env == 0) return;
  --env->rc;
  if (env->rc > 0) return;
  if (env->rc < 0) exit(6);

  // env->rc == 0

  decRCEnv(env->next);
  decRC(env->val);

  free(env);
}

#define nFreedStack 1000
Expr freedStack[nFreedStack];
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

void freeExprStack() {
  while (freedStackPtr > 0)
    free(freedStack[--freedStackPtr]);
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

  if (enumVal(expr) == Lam) {
    struct EnvAndExpr arg = viewArgAs(expr, struct EnvAndExpr);
    decRCEnv(arg.envVal);
    decRC(arg.exprVal);
  } else if (enumVal(expr) == App || enumVal(expr) == Add || enumVal(expr) == Mul) {
    struct TwoExprs arg = viewArgAs(expr, struct TwoExprs);
    decRC(arg.a);
    decRC(arg.b);
  } else if (enumVal(expr) == Ifz) {
    struct ThreeExprs arg = viewArgAs(expr, struct ThreeExprs);
    decRC(arg.a);
    decRC(arg.b);
    decRC(arg.c);
  } else if (enumVal(expr) == MakeThunk || enumVal(expr) == ForceThunk) {
    Expr arg = viewArgAs(expr, Expr);
    decRC(arg);
  } else if (enumVal(expr) == Thunk) {
    struct BoolEnvAndExpr arg = viewArgAs(expr, struct BoolEnvAndExpr);
    decRC(arg.exprVal);
    decRCEnv(arg.envVal);
  }

  freeExpr(expr);
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

void printExpr(Expr expr) {
  if (enumVal(expr) == Var) {
    int arg = viewArgAs(expr, int);
    printf("v%d", arg);
  } else if (enumVal(expr) == Lam) {
    struct EnvAndExpr arg = viewArgAs(expr, struct EnvAndExpr);
    printf("(\\ ");
    printExpr(arg.exprVal);
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
    printf("ifz ");
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
  if (enumVal(expr) == Var) {
    int arg = viewArgAs(expr, int);
    return (struct BoolEnvAndExpr) { true, env, getVarEnv(env, arg) };
  } else if (enumVal(expr) == Lam) {
    struct EnvAndExpr arg = viewArgAs(expr, struct EnvAndExpr);
    if (arg.envVal != 0) return (struct BoolEnvAndExpr) { true, env, expr };

    allocExpr(retVal, struct EnvAndExpr, Lam, ((struct EnvAndExpr) { env, arg.exprVal }));
    incRCEnv(env);
    incRC(arg.exprVal);
    return (struct BoolEnvAndExpr) { true, env, retVal };
  } else if (enumVal(expr) == App) {
    struct TwoExprs arg = viewArgAs(expr, struct TwoExprs);
    arg.a = interpretFully(env, arg.a);
    arg.b = interpretFully(env, arg.b);
    struct EnvAndExpr body = viewExprAs(arg.a, Lam, struct EnvAndExpr);
    env = pushEnv(body.envVal, arg.b);
    incRC(body.exprVal); // jank; makes it so that decRC(arg.a) doesn't also deallocate body.exprVal
    decRC(arg.a);
    --body.exprVal->rc; // jank pt 2; TODO make this not directly write to rc and use helper fns instead
    decRC(arg.b);
    return (struct BoolEnvAndExpr) { false, env, body.exprVal };
  } else if (enumVal(expr) == Add) {
    struct TwoExprs arg = viewArgAs(expr, struct TwoExprs);
    arg.a = interpretFully(env, arg.a);
    arg.b = interpretFully(env, arg.b);
    allocExpr(retVal, intType, LitInt, viewExprAs(arg.a, LitInt, intType) + viewExprAs(arg.b, LitInt, intType));
    decRC(arg.a);
    decRC(arg.b);
    return (struct BoolEnvAndExpr) { true, env, retVal };
  } else if (enumVal(expr) == Mul) {
    struct TwoExprs arg = viewArgAs(expr, struct TwoExprs);
    arg.a = interpretFully(env, arg.a);
    arg.b = interpretFully(env, arg.b);
    allocExpr(retVal, intType, LitInt, viewExprAs(arg.a, LitInt, intType) * viewExprAs(arg.b, LitInt, intType));
    decRC(arg.a);
    decRC(arg.b);
    return (struct BoolEnvAndExpr) { true, env, retVal };
  } else if (enumVal(expr) == Ifz) {
    struct ThreeExprs arg = viewArgAs(expr, struct ThreeExprs);
    arg.a = interpretFully(env, arg.a);
    bool cond = viewExprAs(arg.a, LitInt, intType) == 0;
    decRC(arg.a);
    return (struct BoolEnvAndExpr) { false, env, cond ? arg.b : arg.c };
  } else if (enumVal(expr) == MakeThunk) {
    Expr arg = viewArgAs(expr, Expr);
    allocExpr(retVal, struct BoolEnvAndExpr, Thunk, ((struct BoolEnvAndExpr) { false, env, arg }));
    incRC(arg);
    return (struct BoolEnvAndExpr) { true, env, retVal };
  } else if (enumVal(expr) == ForceThunk) {
    Expr arg = viewArgAs(expr, Expr);
    arg = interpretFully(env, arg);
    if (enumVal(arg) != Thunk) exit(5);
    struct BoolEnvAndExpr* thunk = argLoc(arg, struct BoolEnvAndExpr);
    return (struct BoolEnvAndExpr) { false, env, force(thunk) };
  } else {
    return (struct BoolEnvAndExpr) { true, env, expr };
  }
}

Expr interpretFully(struct Env* env, Expr expr) {
  incRC(expr);
  incRCEnv(env);
  struct BoolEnvAndExpr result = { false, env, expr };
  do {
    struct Env* oldEnv = result.envVal;
    Expr oldExpr = result.exprVal;
    result = interpret(result.envVal, result.exprVal);
    incRC(result.exprVal);
    decRC(oldExpr);
    incRCEnv(result.envVal);
    decRCEnv(oldEnv);
  } while (!result.boolVal);
  decRCEnv(result.envVal);
  return result.exprVal;
}

Expr var(int val) {
  allocExpr(retVal, int, Var, val);
  return retVal;
}

Expr lam(Expr body) {
  incRC(body);
  allocExpr(retVal, struct EnvAndExpr, Lam, ((struct EnvAndExpr) { 0, body }));
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

  Expr mainReduced = interpretFully(0, mainExpr);
  printf("value: ");
  printExpr(mainReduced);
  printf("\n");

  decRC(mainExpr);
  decRC(mainReduced);

  freeExprStack();

  return 0;
}
