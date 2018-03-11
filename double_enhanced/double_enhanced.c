#include "mpc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include <editline/readline.h>

struct lval;
typedef struct lval lval;
struct lenv;
typedef struct lenv lenv;

typedef lval* (*lbuiltin)(lenv*, lval*);

/* Declare New lval Struct */
struct lval {
    int type;
    long num;
    double dnum;
    /* Error and Symbol and String types have some string data */
    char* err;
    char* sym;
    char* str;
    /* Function have pointer */
    lbuiltin builtin;
    lenv* env;
    lval* formals;
    lval* body;
    /* Count and Pointer to a list of "lval*" */
    int count;
    struct lval** cell;
};

/* Declare New lenv Struct */
struct lenv {
    lenv* par;
    int count;
    char** syms;
    lval** vals;
};

/* Construct Enumeration of Possible lval Types */
enum { LVAL_ERR, LVAL_NUM, LVAL_DNUM, LVAL_SYM, LVAL_STR, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR};

/* Parsers */
mpc_parser_t* Number;
mpc_parser_t* Dnumber;
mpc_parser_t* Symbol;
mpc_parser_t* String;
mpc_parser_t* Comment;
mpc_parser_t* Sexpr;
mpc_parser_t* Qexpr;
mpc_parser_t* Expr;
mpc_parser_t* Lispy;

/* Construct a pointer to a new Number lval */
lval* lval_num(long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

/* Construct a pointer to a new Dnumber lval */
lval* lval_dnum(double x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_DNUM;
    v->dnum = x;
    return v;
}

/* Construct a pointer to a new Error lval */
lval* lval_err(char* m, ...) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    
    /* Create a va list and initialize it */
    va_list va;
    va_start(va, m);
    
    /* Allocate 512 bytes of space */
    v->err = malloc(sizeof(char) * 512);
    
    /* printf the error string with a maximum of 511 characters */
    vsnprintf(v->err, 511, m, va);
    
    /* Reallocate to number of bytes actually used */
    v->err = realloc(v->err, sizeof(char) * (strlen(v->err) + 1));
    
    /* Cleanup our va list */
    va_end(va);
    
    return v;
}

/* Construct a pointer to a new Symbol lval */
lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

/* Construct a pointer to a new String lval */
lval* lval_str(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_STR;
    v->str = malloc(strlen(s) + 1);
    strcpy(v->str, s);
    return v;
}

/* Construct a pointer to a new Function lval */
lval* lval_fun(lbuiltin x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->builtin = x;
    v->env = NULL;
    v->formals = NULL;
    v->body = NULL;
    return v;
}

lenv* lenv_new(void);

/* Construct a pointer to a new Lambda lval */
lval* lval_lambda(lval* formals, lval* body) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    
    /* Set Builtin to Null */
    v->builtin = NULL;
    
    /* Build new enviroment */
    v->env = lenv_new();
    
    /* Set Formals and Body */
    v->formals = formals;
    v->body = body;
    return v;
}

/* Construct a pointer to a new empty Sexpr lval */
lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* Construct a pointer to a new empty Qexpr lval */
lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* Construct a pointer to a new empty lenv */
lenv* lenv_new(void) {
    lenv* e = malloc(sizeof(lenv));
    e->par = NULL;
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

void lenv_del(lenv*);

/* Delete an "lval" */
void lval_del(lval* v) {
    
    switch (v->type) {
        /* Do nothing special for number type */
        case LVAL_NUM: break;
        case LVAL_DNUM: break;
        
        /* For Err or Sym or Str free the string data */
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;
        case LVAL_STR: free(v->str); break;
        
        /* For Sexpr and Qexpr then delete all elements inside */
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            /* Also free the memory allocated to contain the pointers */
            free(v->cell);
            break;
        
        /* For Function and Lambda delete as well */
        case LVAL_FUN:
            if (!v->builtin) {
                lenv_del(v->env);
                lval_del(v->formals);
                lval_del(v->body);
            }
            break;
    }
    
    /* Free the memory allocated for the "lval" struct itself */
    free(v);
}

/* Delete an "lenv" */
void lenv_del(lenv* e) {
    for (int i = 0; i < e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

/* Read a number into "lval" */
lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ?
        lval_num(x) : lval_err("invalid number");
}

/* Read a double into "lval" */
lval* lval_read_dnum(mpc_ast_t* t) {
    errno = 0;
    double x = strtod(t->contents, NULL);
    return errno != ERANGE ?
        lval_dnum(x) : lval_err("invalid number");
}

/* Read a string into "lval" */
lval* lval_read_str(mpc_ast_t* t) {
    /* Cur off the final quote character */
    t->contents[strlen(t->contents) - 1] = 0;
    /* Copy the string missing out the first quote character */
    char* unescaped = malloc(strlen(t->contents + 1) + 1);
    strcpy(unescaped, t->contents + 1);
    /* Pass through the unescape function */
    unescaped = mpcf_unescape(unescaped);
    /* COnstruct a new lval using the string */
    lval* str = lval_str(unescaped);
    /* Free the string and return */
    free(unescaped);
    return str;
}

/* Add into "lval" */
lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

/* Read into "lval" */
lval* lval_read(mpc_ast_t* t) {
    
    /* If Symbol or Number or String return conversion to that type */
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "double")) { return lval_read_dnum(t); }
    if (strstr(t->tag, "string")) { return lval_read_str(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }
    
    /* If root (>) or sexpr then create empty list */
    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
    if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
    if (strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }
    
    /* Fill this list with ony valid expression contained within */
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
        if (strstr(t->children[i]->tag,     "comment")) { continue; }
        x = lval_add(x, lval_read(t->children[i]));
    }
    
    return x;
}

void lval_print(lval*);

/* Print the Expr part of an "lval" */
void lval_expr_print(lval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        
        /* Print Value contained within */
        lval_print(v->cell[i]);
         
        /* Don't print trailing space if last element */
        if (i != (v->count - 1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

/* Print an String "lval" */
void lval_print_str(lval* v) {
    /* Make a Copy of the string */
    char* escaped = malloc(strlen(v->str) + 1);
    strcpy(escaped, v->str);
    /* Pass it through the escape function */
    escaped = mpcf_escape(escaped);
    /* Print it between " characters */
    printf("\"%s\"", escaped);
    /* free the copied string */
    free(escaped);
}

/* Print an "lval" */
void lval_print(lval* v) {
    switch (v->type) {
        /* In the case the type is a number print it */
        /* Then 'break' out of the switch. */
        case LVAL_NUM: printf("%li", v->num); break;
        case LVAL_DNUM: printf("%lf", v->dnum); break;
        
        /* In the case the type is an error */
        case LVAL_ERR: printf("Error: %s", v->err); break;
        
        /* In the case the type is an symbol */
        case LVAL_SYM: printf("%s", v->sym); break;
        
        /* In the case the type is an sexpr or qexpr */
        case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
        case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
        
        /* In the case the type is an string */
        case LVAL_STR: lval_print_str(v); break;
        
        /* In the case the type is an function or lambda */
        case LVAL_FUN:
            if (v->builtin) {
                printf("<function>");
            } else {
                printf ("(\\ "); lval_print(v->formals);
                putchar(' '); lval_print(v->body); putchar(')');
            }
            break;
    }
}

/* Print an "lval" followed by a newline */
void lval_println(lval* v) { lval_print(v); putchar('\n'); }

lenv* lenv_copy(lenv*);

/* Copy a "lval" */
lval* lval_copy(lval* v) {
    
    lval* x = malloc(sizeof(lval));
    x->type = v->type;
    
    switch(v->type) {
        
        /* Copy Functions and Numbers Directly */
        case LVAL_FUN:
            if (v->builtin) {
                x->builtin = v->builtin;
            } else {
                x->builtin = NULL;
                x->env = lenv_copy(v->env);
                x->formals = lval_copy(v->formals);
                x->body = lval_copy(v->body);
            }
            break;
        case LVAL_NUM: x->num = v->num; break;
        case LVAL_DNUM: x->dnum = v->dnum; break;
        
        /* Copy Strings using malloc and strcpy */
        case LVAL_ERR:
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err); break;
        
        case LVAL_SYM:
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym); break;
        
        case LVAL_STR:
            x->str = malloc(strlen(v->str) + 1);
            strcpy(x->str, v->str); break;
        
        /* Copy Lists by copying each sub-expression */
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval*) * x->count);
            for (int i = 0; i < x->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
            break;
    }
    
    return x;
}

/* Copy an "lenv" */
lenv* lenv_copy(lenv* e) {
    lenv* n = malloc(sizeof(lenv));
    n->par = e->par;
    n->count = e->count;
    n->syms = malloc(sizeof(char*) * n->count);
    n->vals = malloc(sizeof(lval*) * n->count);
    for (int i = 0; i < e->count; i++) {
        n->syms[i] = malloc(strlen(e->syms[i]) + 1);
        strcpy(n->syms[i], e->syms[i]);
        n->vals[i] = lval_copy(e->vals[i]);
    }
    return n;
}

/* Get name for a type */
char* ltype_name(int t) {
    switch(t) {
        case LVAL_FUN: return "Function";
        case LVAL_NUM: return "Number";
        case LVAL_DNUM: return "Double";
        case LVAL_ERR: return "Error";
        case LVAL_SYM: return "Symbol";
        case LVAL_STR: return "String";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default: return "Unknown";
    }
}

lval* lval_eval(lenv*, lval*);

/* "Pop" an element from the list in an "lval" */
lval* lval_pop(lval* v, int i) {
    /* Find the item it "i" */
    lval* x = v->cell[i];
    
    /* Shift memory after the item at "i" over the top */
    memmove(&v->cell[i], &v->cell[i + 1],
            sizeof(lval*) * (v->count - i - 1));
    
    /* Decrease the count of items in the list */
    v->count--;
    
    /* Reallocate the memory used */
    v->cell = realloc(v->cell, sizeof(lval*) * (v->count));
    return x;
}

/* "Take" an element from the list in an "lval" */
lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

/* "Get" an variable from an "lenv" */
lval* lenv_get(lenv* e, lval* k) {
    
    /* Iterate over all items in enviroment */
    for (int i = 0; i < e->count; i++) {
        /* Check if the stored string matches the symbol string */
        /* If it does, return a copy of the value */
        if (strcmp(e->syms[i], k->sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }
    /* If no symbol found check in parents, otherwise return error */
    if (e->par) {
        return lenv_get(e->par, k);
    } else {
        return lval_err("Unbound symbol '%s'", k->sym);
    }
}

/* "Put" an variable into an "lenv" */
void lenv_put(lenv* e, lval* k, lval* v) {
    
    /* Iterate over all items in enviroment */
    /* This is to see if variable alreeady exists */
    for (int i = 0; i < e->count; i++) {
        
        /* If variable is found delete item at that position */
        /* And replace with variable supplied by user */
        if (strcmp(e->syms[i], k->sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }
    
    /* If no existing entry found allocate space for new entry */
    e->count++;
    e->vals = realloc(e->vals, sizeof(lval*) * e->count);
    e->syms = realloc(e->syms, sizeof(char*) * e->count);
    
    /* Copy contents of lval and symbol string into new location */
    e->vals[e->count - 1] = lval_copy(v);
    e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
    strcpy(e->syms[e->count - 1], k->sym);
}

/* "Define" an variable in the most-parent "lenv" */
void lenv_def(lenv* e, lval* k, lval* v) {
    /* Iterate till w has no parent */
    while (e->par) { e = e->par; }
    /* Put value in e */
    lenv_put(e, k, v);
}

/* "Join" two lists in two "lval"s */
lval* lval_join(lval* x, lval* y) {
    /* For each cell in 'y' add it to 'x' */
    while (y->count) {
        x = lval_add(x, lval_pop(y, 0));
    }
    
    /* Delete the empty 'y' and return 'x' */
    lval_del(y);
    return x;
}

lval* builtin_eval(lenv*, lval*);
lval* builtin_list(lenv*, lval*);

/* "Call" an "lval" */
lval* lval_call(lenv* e, lval* f, lval* a) {
    
    /* If Builtin then simply apply that */
    if (f->builtin) { return f->builtin(e, a); }
    
    /* Record Argument Counts */
    int given = a->count;
    int total = f->formals->count;
    
    /* While Arguments still remain to be processed */
    while (a->count) {
        
        /* If we've ran out of formal arguments to bind */
        if (f->formals->count == 0) {
            lval_del(a);
            return lval_err(
                    "Function passed too many arguments. "
                    "Got %i, Expected %i.", given, total);
        }
        
        /* Pop the first symbol from the formals */
        lval* sym = lval_pop(f->formals, 0);
        
        /* Special Case to deal with '&' */
        if (strcmp(sym->sym, "&") == 0) {
            
            /* Ensure '&' is followed by another symbol */
            if (f->formals->count != 1) {
                lval_del(a);
                return lval_err("Function format invalid. "
                        "Symbol '&' not followed by single symbol.");
            }
            
            /* Next formal should be bound to remaining arguments */
            lval* nsym = lval_pop(f->formals, 0);
            lenv_put(f->env, nsym, builtin_list(e, a));
            lval_del(sym); lval_del(nsym);
            break;
        }
         
        /* Pop the next argument from the list */
        lval* val = lval_pop(a, 0);
        
        /* Bind a copy into the function's enviroment */
        lenv_put(f->env, sym, val);
        
        /* Delete symbol and value */
        lval_del(sym); lval_del(val);
    }
    
    /* Argument list is now bound so can be cleaned up */
    lval_del(a);
    
    /* If '&' remains in formal list bind to empty list */
    if (f->formals->count > 0 &&
            strcmp(f->formals->cell[0]->sym, "&") == 0) {
        
        /* Check to ensure that & is not passed invalidly */
        if (f->formals->count != 2) {
            return lval_err("Function format invalid. "
                    "Symbol '&' not followed by single symbol.");
        }
        
        /* Pop and delete '&' symbol */
        lval_del(lval_pop(f->formals, 0));
        
        /* Pop next symbol and create empty list */
        lval* sym = lval_pop(f->formals, 0);
        lval* val = lval_qexpr();
        
        /* Bind to enviroment and delete */
        lenv_put(f->env, sym, val);
        lval_del(sym); lval_del(val);
    }
    
    /* If all formals have been bound, evaluate. */
    if (f->formals->count == 0) {
        
        /* Set enviroment parent to evaluation enviroment */
        f->env->par = e;
        
        /* Evaluate and return */
        return builtin_eval(
                f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
    } else {
        /* Otherwise return partially evaluated function */
        return lval_copy(f);
    }
    
}

int comp_eq(double a, double b)
{ return fabs(a - b) < 1e-9; }

/* Equality operatoe */
int lval_eq(lval* x, lval* y) {
    
    /* Different Types are always unequal */
    if (x->type != y->type) { return 0; }
    
    /* Compare Based upon type */
    switch (x->type) {
        /* Compare Number Value */
        case LVAL_NUM: return (x->num == y->num);
        case LVAL_DNUM: return comp_eq(x->dnum, y->dnum);
        
        /* Compare String Values */
        case LVAL_ERR: return (strcmp(x->err, y->err) == 0);
        case LVAL_SYM: return (strcmp(x->sym, y->sym) == 0);
        case LVAL_STR: return (strcmp(x->str, y->str) == 0);
        
        /* If builtin compare, otherwise compare formals and body */
        case LVAL_FUN:
            if (x->builtin || y->builtin) {
                return x->builtin == y->builtin;
            } else {
                return lval_eq(x->formals, y->formals) &&
                    lval_eq(x->body, y->body);
            }
        
        /* If list compare every individual element */
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            if (x->count != y->count) { return 0; }
            for (int i = 0; i < x->count; i++) {
                /* If any element not equal then whole list not equal */
                if (!lval_eq(x->cell[i], y->cell[i])) { return 0; }
            }
            /* Otherwise lists must be equal */
            return 1;
    }
    return 0;
}

#define LASSERT(args, cond, err, ...) \
    if (!(cond)) { lval* error = lval_err(err, ##__VA_ARGS__); lval_del(args); return error; }

#define LASSERT_FUN(fun) \
    LASSERT(a, a->count == 1,\
            "Function '" #fun "' passed incorrect number of arguments. "\
            "Got %i, Expected %i.",\
            a->count, 1);\
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,\
            "Function '" #fun "' passed incorrect type for argument 0. "\
            "Got %s, Expected %s.",\
            ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR))
#define LASSERT_NEMPTY(fun) \
    LASSERT(a, a->cell[0]->count != 0,\
            "Function '" #fun "' passed {}!")


/* Builtin function head */
lval* builtin_head(lenv* e, lval* a) {
    LASSERT_FUN(head);
    LASSERT_NEMPTY(head);
    
    lval* v = lval_take(a, 0);
    while (v->count > 1) { lval_del(lval_pop(v, 1)); }
    return v;
}

/* Builtin function tail */
lval* builtin_tail(lenv* e, lval* a) {
    LASSERT_FUN(tail);
    LASSERT_NEMPTY(tail);
    
    lval* v = lval_take(a, 0);
    lval_del(lval_pop(v, 0));
    return v;
}

/* Builtin function list */
lval* builtin_list(lenv* e, lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

/* Builtin function eval */
lval* builtin_eval(lenv* e, lval* a) {
    LASSERT_FUN(eval);
    
    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

/* Builtin function join */
lval* builtin_join(lenv* e, lval* a) {
    
    for (int i = 0; i < a->count; i++) {
        LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
                "Function 'join' passed incorrect type for argument %i. "
                "Got %s, Expected %s",
                i, ltype_name(a->cell[i]->type), LVAL_QEXPR);
    }
    
    lval* x = lval_pop(a, 0);
    
    while (a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }
    
    lval_del(a);
    return x;
}

/* Eval operators on an Double "lval" */
lval* builtin_op_double(lenv* e, lval* a, char* op) {
    /* Pop the first element */
    lval* x = lval_pop(a, 0);
    
    /* If no arguments and sub then perform unary negation */
    if (strcmp(op, "-") == 0 && a->count == 0) {
        x->dnum = -x->dnum;
    }
    
    /* While there are stillelements remaining */
    while (a->count > 0) {
        
        /* Pop the next element */
        lval* y = lval_pop(a, 0);
        
        if (strcmp(op, "+") == 0) { x->dnum += y->dnum; }
        if (strcmp(op, "-") == 0) { x->dnum -= y->dnum; }
        if (strcmp(op, "*") == 0) { x->dnum *= y->dnum; }
        if (strcmp(op, "/") == 0) { x->dnum /= y->dnum; }
        
        lval_del(y);
    }
    
    lval_del(a); return x;
}


/* Eval operators on an "lval" */
lval* builtin_op(lenv* e, lval* a, char* op) {
    
    int double_t = LVAL_NUM;
    if (a->cell[0]->type == LVAL_DNUM) double_t = LVAL_DNUM;
    
    /* Ensure all arguments are numbers */
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != double_t) {
            lval* err = lval_err(
                    "Function '%s' passed incorrect type for argument %i. "
                    "Got %s, Expected %s.",
                    op, i, ltype_name(a->cell[i]->type), ltype_name(double_t));
            lval_del(a);
            return err;
        }
    }
    
    /* If all Double, call builtin_op_double */
    if (double_t == LVAL_DNUM) {
        return builtin_op_double(e, a, op);
    }
    
    /* Pop the first element */
    lval* x = lval_pop(a, 0);
    
    /* If no arguments and sub then perform unary negation */
    if (strcmp(op, "-") == 0 && a->count == 0) {
        x->num = -x->num;
    }
    
    /* While there are stillelements remaining */
    while (a->count > 0) {
        
        /* Pop the next element */
        lval* y = lval_pop(a, 0);
        
        if (strcmp(op, "+") == 0) { x->num += y->num; }
        if (strcmp(op, "-") == 0) { x->num -= y->num; }
        if (strcmp(op, "*") == 0) { x->num *= y->num; }
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                lval_del(x); lval_del(y);
                x = lval_err("Division By Zero!"); break;
            }
            x->num /= y->num;
        }
        
        lval_del(y);
    }
    
    lval_del(a); return x;
}

/* Builtin operator functions */
lval* builtin_add(lenv* e, lval* a) {
    return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a) {
    return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a) {
    return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
    return builtin_op(e, a, "/");
}

/* Define a variable */
lval* builtin_var(lenv* e, lval* a, char* func) {
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function '%s' passed incorrect type for argument 0. "
            "Got %s, Expected %s.", func,
            ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));
    
    /* First argument is symbol list */
    lval* syms = a->cell[0];
    
    /* Ensure all elements of first list are symbols */
    for (int i = 0; i < syms->count; i++) {
        LASSERT(a, syms->cell[i]->type == LVAL_SYM,
                "Function '%s' passed incorrect type for the %ith element in argument 1. "
                "Got %s, Expected %s", func,
                i, ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
    }
    
    /* Check correct number of symbols and values */
    LASSERT(a, syms->count == a->count - 1,
            "Function '%s' cannot varine incorrect number of values to symbols. "
            "Got %i and %i, Expected them to be equal.", func,
            syms->count, a->count - 1);
    
    /* Assign copies of values to symbols */
    for (int i = 0; i < syms->count; i++) {
        /* If 'def' define in globally. */
        if (strcmp(func, "def") == 0) {
            lenv_def(e, syms->cell[i], a->cell[i + 1]);
        }
        
        /* If 'put' define in locally */
        if (strcmp(func, "=") == 0) {
            lenv_put(e, syms->cell[i], a->cell[i + 1]);
        }
    }
    
    lval_del(a);
    return lval_sexpr();
}

lval* builtin_def(lenv* e, lval* a) {
    return builtin_var(e, a, "def");
}

lval* builtin_put(lenv* e, lval* a) {
    return builtin_var(e, a, "=");
}

/* Define a lambda */
lval* builtin_lambda(lenv* e, lval* a) {
    /* Check Two arguments, each of which are Q-Expressions */
    LASSERT(a, a->count == 2,
            "Function \\ passed incorrect number of arguments. "
            "Got %i, Expected 2.",
            a->count);
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function \\ passed incorrect type for argument 0. "
            "Got %s, Expected %s.",
            ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));
    LASSERT(a, a->cell[1]->type == LVAL_QEXPR,
            "Function \\ passed incorrect type for argument 1. "
            "Got %s, Expected %s.",
            ltype_name(a->cell[1]->type), ltype_name(LVAL_QEXPR));
    
    /* Check first Q-Expression contains only Symbols */
    for (int i = 0; i < a->cell[0]->count; i++) {
        LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
                "Function \\ passed incorrect type for the %ith element of argument 0. "
                "Got %s, Expected %s.",
                ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
    }
    
    /* Pop first two arguments and pass them to lval_lambda */
    lval* formals = lval_pop(a, 0);
    lval* body = lval_pop(a, 0);
    lval_del(a);
    
    return lval_lambda(formals, body);
}

/* Compare two Double "lval"s */
lval* builtin_ord_double(lenv* e, lval* a, char* op) {
    double r;
    if (strcmp(op, ">") == 0) {
        r = (a->cell[0]->dnum > a->cell[1]->dnum);
    }
    if (strcmp(op, "<") == 0) {
        r = (a->cell[0]->dnum < a->cell[1]->dnum);
    }
    if (strcmp(op, ">=") == 0) {
        r = (a->cell[0]->dnum > a->cell[1]->dnum ||
                comp_eq(a->cell[0]->dnum, a->cell[1]->dnum));
    }
    if (strcmp(op, "<=") == 0) {
        r = (a->cell[0]->dnum <= a->cell[1]->dnum ||
                comp_eq(a->cell[0]->dnum, a->cell[1]->dnum));
    }
    lval_del(a);
    return lval_dnum(r);
}

/* Compare two "lval"s */
lval* builtin_ord(lenv* e, lval* a, char* op) {
    /* Check Two arguments, each of which are Numbers */
    LASSERT(a, a->count == 2,
            "Function %s passed incorrect number of arguments. "
            "Got %i, Expected 2.", op, 
            a->count);
    LASSERT(a, a->cell[0]->type == LVAL_NUM || a->cell[0]->type == LVAL_DNUM,
            "Function %s passed incorrect type for argument 0. "
            "Got %s, Expected %s or %s.", op, 
            ltype_name(a->cell[0]->type), ltype_name(LVAL_NUM), ltype_name(LVAL_DNUM));
    LASSERT(a, a->cell[1]->type == LVAL_NUM || a->cell[1]->type == LVAL_DNUM,
            "Function %s passed incorrect type for argument 1. "
            "Got %s, Expected %s or %s.", op, 
            ltype_name(a->cell[1]->type), ltype_name(LVAL_NUM), ltype_name(LVAL_DNUM));
    LASSERT(a, a->cell[0]->type == a->cell[1]->type,
            "Function %s passed unequal type for argument 0 and 1. "
            "Got %s and %s, Expect them to be equal.", op,
            ltype_name(a->cell[0]->type), ltype_name(a->cell[1]->type));
    
    if (a->cell[0]->type == LVAL_DNUM) {
        return builtin_ord_double(e, a, op);
    }
    
    int r;
    if (strcmp(op, ">") == 0) {
        r = (a->cell[0]->num > a->cell[1]->num);
    }
    if (strcmp(op, "<") == 0) {
        r = (a->cell[0]->num < a->cell[1]->num);
    }
    if (strcmp(op, ">=") == 0) {
        r = (a->cell[0]->num >= a->cell[1]->num);
    }
    if (strcmp(op, "<=") == 0) {
        r = (a->cell[0]->num <= a->cell[1]->num);
    }
    lval_del(a);
    return lval_num(r);
}

/* Builtin Ordering Operators */
lval* builtin_gt(lenv* e, lval* a) {
    return builtin_ord(e, a, ">");
}
lval* builtin_lt(lenv* e, lval* a) {
    return builtin_ord(e, a, "<");
}
lval* builtin_ge(lenv* e, lval* a) {
    return builtin_ord(e, a, ">=");
}
lval* builtin_le(lenv* e, lval* a) {
    return builtin_ord(e, a, "<=");
}

/* Equality comparison */
lval* builtin_cmp(lenv* e, lval* a, char* op) {
    LASSERT(a, a->count == 2,
            "Function %s passed incorrect number of arguments. "
            "Got %i, Expected 2.", op, 
            a->count);
    int r;
    if (strcmp(op, "==") == 0) {
        r =  lval_eq(a->cell[0], a->cell[1]);
    }
    if (strcmp(op, "!=") == 0) {
        r = !lval_eq(a->cell[0], a->cell[1]);
    }
    lval_del(a);
    return lval_num(r);
}

/* Equality builtin operators */
lval* builtin_eq(lenv* e, lval* a) {
    return builtin_cmp(e, a, "==");
}

lval* builtin_ne(lenv* e, lval* a) {
    return builtin_cmp(e, a, "!=");
}

/* If expression */
lval* builtin_if(lenv* e, lval* a) {
    /* Check Three arguments, each of which are Numbers, and two Q-expression */
    LASSERT(a, a->count == 3,
            "Function if passed incorrect number of arguments. "
            "Got %i, Expected 3.",
            a->count);
    LASSERT(a, a->cell[0]->type == LVAL_NUM,
            "Function if passed incorrect type for argument 0. "
            "Got %s, Expected %s.",
            ltype_name(a->cell[0]->type), ltype_name(LVAL_NUM));
    LASSERT(a, a->cell[1]->type == LVAL_QEXPR,
            "Function if passed incorrect type for argument 1. "
            "Got %s, Expected %s.",
            ltype_name(a->cell[1]->type), ltype_name(LVAL_QEXPR));
    LASSERT(a, a->cell[2]->type == LVAL_QEXPR,
            "Function if passed incorrect type for argument 2. "
            "Got %s, Expected %s.",
            ltype_name(a->cell[2]->type), ltype_name(LVAL_QEXPR));
    
    /* Mark Both Expressions as evaluable */
    lval* x;
    a->cell[1]->type = LVAL_SEXPR;
    a->cell[2]->type = LVAL_SEXPR;
    
    if (a->cell[0]->num) {
        /* If condition is true evaluate first expression */
        x = lval_eval(e, lval_pop(a, 1));
    } else {
        /* Otherwise evaluate second expression */
        x = lval_eval(e, lval_pop(a, 2));
    }
    
    /* Delete argument list and return */
    lval_del(a);
    return x;
}

lval* lval_eval(lenv*, lval*);

/* Load a file */
lval* builtin_load(lenv* e, lval* a) {
    /* Check One arguments, which is String */
    LASSERT(a, a->count == 1,
            "Function load passed incorrect number of arguments. "
            "Got %i, Expected 1.",
            a->count);
    LASSERT(a, a->cell[0]->type == LVAL_STR,
            "Function load passed incorrect type for argument 0. "
            "Got %s, Expected %s.",
            ltype_name(a->cell[0]->type), ltype_name(LVAL_STR));
    
    /* Parse File given by string name */
    mpc_result_t r;
    if (mpc_parse_contents(a->cell[0]->str, Lispy, &r)) {
        
        /* Read contents */
        lval* expr = lval_read(r.output);
        mpc_ast_delete(r.output);
        
        /* Evaluate each Expression */
        while (expr->count) {
            lval* x = lval_eval(e, lval_pop(expr, 0));
            /* If Evaluation leads to error print it */
            if (x->type == LVAL_ERR) { lval_println(x); }
            lval_del(x);
        }
        
        /* Delete expressions and arguments */
        lval_del(expr);
        lval_del(a);
        
        /* Return empty list */
        return lval_sexpr();
        
    } else {
        /* Get Parse Error as String */
        char* err_msg = mpc_err_string(r.error);
        mpc_err_delete(r.error);
        
        /* Create new error message using it */
        lval* err = lval_err("Could not load Library: %s", err_msg);
        free(err_msg);
        lval_del(a);
        
        /* Cleanup and return error */
        return err;
    }
}

/* Print a String */
lval* builtin_print(lenv* e, lval* a) {
    
    /* Print each argument followed by a space */
    for (int i = 0; i < a->count; i++) {
        lval_print(a->cell[i]); putchar(' ');
    }
    
    /* Print a newline and delete arguments */
    putchar('\n');
    lval_del(a);
    
    return lval_sexpr();
}

/* Suppress an error */
lval* builtin_error(lenv* e, lval* a) {
    /* Check One arguments, which is String */
    LASSERT(a, a->count == 1,
            "Function load passed incorrect number of arguments. "
            "Got %i, Expected 1.",
            a->count);
    LASSERT(a, a->cell[0]->type == LVAL_STR,
            "Function load passed incorrect type for argument 0. "
            "Got %s, Expected %s.",
            ltype_name(a->cell[0]->type), ltype_name(LVAL_STR));
    
    /* Construct Error from first argument */
    lval* err = lval_err(a->cell[0]->str);
    
    /* Delete arguments and return */
    lval_del(a);
    return err;
}

/* Double builtins */
/* cast */
lval* builtin_inttofloat(lenv* e, lval* a) {
    /* Check One arguments, which is Number */
    LASSERT(a, a->count == 1,
            "Function inttofloat passed incorrect number of arguments. "
            "Got %i, Expected 1.",
            a->count);
    if (a->cell[0]->type == LVAL_DNUM) {
        lval* dnum = lval_dnum(a->cell[0]->dnum);
        lval_del(a);
        return dnum;
    }
    LASSERT(a, a->cell[0]->type == LVAL_NUM,
            "Function inttofloat passed incorrect type for argument 0. "
            "Got %s, Expected %s.",
            ltype_name(a->cell[0]->type), ltype_name(LVAL_NUM)); 
    lval* dnum = lval_dnum(a->cell[0]->num);
    lval_del(a);
    return dnum;
}
lval* builtin_floattoint(lenv* e, lval* a) {
    /* Check One arguments, which is Number */
    LASSERT(a, a->count == 1,
            "Function floattoint passed incorrect number of arguments. "
            "Got %i, Expected 1.",
            a->count);
    if (a->cell[0]->type == LVAL_NUM) {
        lval* num = lval_num(a->cell[0]->num);
        lval_del(a);
        return num;
    }
    LASSERT(a, a->cell[0]->type == LVAL_DNUM,
            "Function floattoint passed incorrect type for argument 0. "
            "Got %s, Expected %s.",
            ltype_name(a->cell[0]->type), ltype_name(LVAL_DNUM)); 
    lval* num = lval_num(a->cell[0]->dnum);
    lval_del(a);
    return num;
}

/* ceil and floor and round */
lval* builtin_ceil(lenv* e, lval* a) {
    /* Check One arguments, which is Number */
    LASSERT(a, a->count == 1,
            "Function ceil passed incorrect number of arguments. "
            "Got %i, Expected 1.",
            a->count);
    LASSERT(a, a->cell[0]->type == LVAL_DNUM,
            "Function ceil passed incorrect type for argument 0. "
            "Got %s, Expected %s.",
            ltype_name(a->cell[0]->type), ltype_name(LVAL_DNUM)); 
    lval* num = lval_num(ceil(a->cell[0]->dnum));
    lval_del(a);
    return num;
}
lval* builtin_floor(lenv* e, lval* a) {
    /* Check One arguments, which is Number */
    LASSERT(a, a->count == 1,
            "Function floor passed incorrect number of arguments. "
            "Got %i, Expected 1.",
            a->count);
    LASSERT(a, a->cell[0]->type == LVAL_DNUM,
            "Function floor passed incorrect type for argument 0. "
            "Got %s, Expected %s.",
            ltype_name(a->cell[0]->type), ltype_name(LVAL_DNUM)); 
    lval* num = lval_num(floor(a->cell[0]->dnum));
    lval_del(a);
    return num;
}
lval* builtin_round(lenv* e, lval* a) {
    /* Check One arguments, which is Number */
    LASSERT(a, a->count == 1,
            "Function round passed incorrect number of arguments. "
            "Got %i, Expected 1.",
            a->count);
    LASSERT(a, a->cell[0]->type == LVAL_DNUM,
            "Function round passed incorrect type for argument 0. "
            "Got %s, Expected %s.",
            ltype_name(a->cell[0]->type), ltype_name(LVAL_DNUM)); 
    lval* num = lval_num(round(a->cell[0]->dnum));
    lval_del(a);
    return num;
}

/* Type Builtins */
lval* builtin_typeof(lenv* e, lval* a) {
    LASSERT(a, a->count == 1,
            "Function typeof passed incorrect number of arguments. "
            "Got %i, Expected 1.",
            a->count);
    
    return lval_str(ltype_name(a->cell[0]->type));
}

/* Quit */
lval* builtin_quit(lenv* e, lval* a) {
    LASSERT(a, a->count == 1,
            "Function quit passed incorrect number of arguments. "
            "Got %i, Expected 1.",
            a->count);
    LASSERT(a, a->cell[0]->type == LVAL_NUM,
            "Function quit passed incorrect type for argument 0. "
            "Got %s, Expected %s.",
            ltype_name(a->cell[0]->type), ltype_name(LVAL_NUM)); 
    
    exit(a->cell[0]->num);
}

/* add a builtin */
void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
    lval* k = lval_sym(name);
    lval* v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k); lval_del(v);
}

/* add builtins */
void lenv_add_builtins(lenv* e) {
    /* List Functions */
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);
    
    /* Mathematical Functions */
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);
    
    /* Variable Functions */
    lenv_add_builtin(e, "def", builtin_def);
    lenv_add_builtin(e, "=", builtin_put);
    lenv_add_builtin(e, "\\", builtin_lambda);
    
    /* Comparison Functions */
    lenv_add_builtin(e, "if", builtin_if);
    lenv_add_builtin(e, "==", builtin_eq);
    lenv_add_builtin(e, "!=", builtin_ne);
    lenv_add_builtin(e, "<" , builtin_lt);
    lenv_add_builtin(e, ">" , builtin_gt);
    lenv_add_builtin(e, "<=", builtin_le);
    lenv_add_builtin(e, ">=", builtin_ge);
    
    /* String Functions */
    lenv_add_builtin(e, "load", builtin_load);
    lenv_add_builtin(e, "error", builtin_error);
    lenv_add_builtin(e, "print", builtin_print);
    
    /* Double Functions */
    lenv_add_builtin(e, "inttofloat", builtin_inttofloat);
    lenv_add_builtin(e, "floattoint", builtin_floattoint);
    lenv_add_builtin(e, "ceil", builtin_ceil);
    lenv_add_builtin(e, "floor", builtin_floor);
    lenv_add_builtin(e, "round", builtin_round);
    
    /* Type Functions */
    lenv_add_builtin(e, "typeof", builtin_typeof);
    
    /* Quit */
    lenv_add_builtin(e, "quit", builtin_quit);
}

/* Eval an Sexpr "lval" */
lval* lval_eval_sexpr(lenv* e, lval* v) {
    
    /* Evaluate Children */
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }
    
    /* Error Checking */
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }
    
    /* Empty Expression */
    if (v->count == 0) { return v; }
    
    /* Single Expression */
    if (v->count == 1) { return lval_take(v, 0); }
    
    /* Ensure First Element is a Function after evaluation */
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_FUN) {
        lval_del(f); lval_del(v);
        return lval_err("Function 'eval' got incorrect type after evaluation for argument 1. "
                "Got %s, Expected %s.",
                ltype_name(f->type), ltype_name(LVAL_FUN));
    }
    
    /* Call builtin with operator */
    lval* result = lval_call(e, f, v);
    lval_del(f);
    return result;
}

/* Eval an "lval" */
lval* lval_eval(lenv* e, lval* v) {
    if (v->type == LVAL_SYM) {
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }
    /* Evaluate S-expressions */
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
    /* All other lval types remain the same */
    return v;
}

int main(int argc, char** argv) {
        
    /* Construct Some Parsers */
    Number   = mpc_new("number");
    Dnumber  = mpc_new("double");
    Symbol   = mpc_new("symbol");
    String   = mpc_new("string");
    Comment  = mpc_new("comment");
    Sexpr    = mpc_new("sexpr");
    Qexpr    = mpc_new("qexpr");
    Expr     = mpc_new("expr");
    Lispy    = mpc_new("lispy");
    
    /* Define them with the following Language */
    mpca_lang(MPCA_LANG_DEFAULT, 
        "                                                       \
            number  : /-?[0-9]+/ ;                              \
            double  : /-?([0-9]+\\.([0-9]+)?|\\.[0-9]+)/ ;      \
            symbol  : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;        \
            string  : /\"(\\\\.|[^\"])*\"/ ;                    \
            comment : /;[^\\r\\n]*/ ;                           \
            sexpr   : '(' <expr>* ')' ;                         \
            qexpr   : '{' <expr>* '}' ;                         \
            expr    : <double> | <number> | <symbol>            \
                    | <string> | <comment> | <sexpr>            \
                    | <qexpr> ;                                 \
            lispy   : /^/ <expr>* /$/ ;                         \
        ",
        Number, Dnumber, Symbol, String, Comment, Sexpr, Qexpr, Expr, Lispy);
    
    lenv* e = lenv_new();
    lenv_add_builtins(e);
    
    /* Supplied with list of files */
    if (argc >= 2) {
        
        /* loop over each supplied filename (starting from 1) */
        for (int i = 1; i < argc; i++) {
            
            /* Argument list with a single argument, the filename */
            lval* args = lval_add(lval_sexpr(), lval_str(argv[i]));
            
            /* Pass to builtin load and get the result */
            lval* x = builtin_load(e, args);
            
            /* If the result is an error be sure to print it */
            if (x->type == LVAL_ERR) { lval_println(x); }
            lval_del(x);
        }
        return 0;
    }
    
    /* Print Version and Exit Information */
    puts("Lispy Version 0.0.0.1.3");
    puts("Type quit 0 to Exit\n");
    
    /* In a never ending loop */
    while (1) {
        
        /* Output our prompt and get input */
        char* input = readline("lispy> ");
        if (input == NULL) break;
        
        /* Add input to history */
        add_history(input);
        
        /* Attempt to Parse the user Input */
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            /* On Success eval the AST */
            lval* result = lval_read(r.output);
            result = lval_eval(e, result);
            lval_println(result);
            lval_del(result);
            
            mpc_ast_delete(r.output);
        } else {
            /* Otherwise Print the Error */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        
        /* Free retrieved input */
        free(input);
        
    }
    
    /* Undefine and Delete our Parsers */
    lenv_del(e);
    mpc_cleanup(9, Number, Dnumber, Symbol, String, Comment, Sexpr, Qexpr, Expr, Lispy);
    
    return 0;
}
