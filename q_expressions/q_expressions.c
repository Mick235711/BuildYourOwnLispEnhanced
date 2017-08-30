#include "mpc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <editline/readline.h>

/* Declare New lval Struct */
typedef struct lval {
    int type;
    long num;
    /* Error and Symbol types hava some string data */
    char* err;
    char* sym;
    /* Count and Pointer to a list of "lval*" */
    int count;
    struct lval** cell;
} lval;

/* Construct Enumeration of Possible lval Types */
enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

/* Construct a pointer to a new Number lval */
lval* lval_num(long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

/* Construct a pointer to a new Error lval */
lval* lval_err(char* m) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
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

/* Delete an "lval" */
void lval_del(lval* v) {
    
    switch (v->type) {
        /* Do nothing special for number type */
        case LVAL_NUM: break;
        
        /* For Err or Sym free the string data */
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;
        
        /* For Sexpr and Qexpr then delete all elements inside */
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            /* Also free the memory allocated to contain the pointers */
            free(v->cell);
            break;
    }
    
    /* Free the memory allocated for the "lval" struct itself */
    free(v);
}

/* Read a number into "lval" */
lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ?
        lval_num(x) : lval_err("invalid number");
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
    
    /* If Symbol or Number return conversion to that type */
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
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

/* Print an "lval" */
void lval_print(lval* v) {
    switch (v->type) {
        /* In the case the type is a number print it */
        /* Then 'break' out of the switch. */
        case LVAL_NUM: printf("%li", v->num); break;
        
        /* In the case the type is an error */
        case LVAL_ERR: printf("Error: %s", v->err); break;
        
        /* In the case the type is an symbol */
        case LVAL_SYM: printf("%s", v->sym); break;
        
        /* In the case the type is an sexpr or qexpr */
        case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
        case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
    }
}

/* Print an "lval" followed by a newline */
void lval_println(lval* v) { lval_print(v); putchar('\n'); }

lval* lval_eval(lval*);

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

#define LASSERT(args, cond, err) \
    if (!(cond)) { lval_del(args); return lval_err(err); }

#define LASSERT_FUN(fun) \
    LASSERT(a, a->count == 1,\
            "Function '" #fun "' passed too many arguments!");\
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,\
            "Function '" #fun "' passed incorrect type!")
#define LASSERT_NEMPTY(fun) \
    LASSERT(a, a->cell[0]->count != 0,\
            "Function '" #fun "' passed {}!")


/* Builtin function head */
lval* builtin_head(lval* a) {
    LASSERT_FUN(head);
    LASSERT_NEMPTY(head);
    
    lval* v = lval_take(a, 0);
    while (v->count > 1) { lval_del(lval_pop(v, 1)); }
    return v;
}

/* Builtin function tail */
lval* builtin_tail(lval* a) {
    LASSERT_FUN(tail);
    LASSERT_NEMPTY(tail);
    
    lval* v = lval_take(a, 0);
    lval_del(lval_pop(v, 0));
    return v;
}

/* Builtin function list */
lval* builtin_list(lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

/* Builtin function eval */
lval* builtin_eval(lval* a) {
    LASSERT_FUN(eval);
    
    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(x);
}

/* Builtin function join */
lval* builtin_join(lval* a) {
    
    for (int i = 0; i < a->count; i++) {
        LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
                "Function 'join' passed incorrect type.");
    }
    
    lval* x = lval_pop(a, 0);
    
    while (a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }
    
    lval_del(a);
    return x;
}

/* Eval operators on an "lval" */
lval* builtin_op(lval* a, char* op) {
    
    /* Ensure all arguments are numbers */
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != LVAL_NUM) {
            lval_del(a);
            return lval_err("Cannot operate on non-number!");
        }
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

/* Choose which builtin to invoke */
lval* builtin(lval* a, char* func) {
#define INVOKE_FUN(name) \
    if (strcmp(#name, func) == 0) { return builtin_##name(a); }
    INVOKE_FUN(list);
    INVOKE_FUN(head);
    INVOKE_FUN(tail);
    INVOKE_FUN(join);
    INVOKE_FUN(eval);
#undef INVOKE_FUN
    if (strstr("+-*/", func)) { return builtin_op(a, func); }
    lval_del(a);
    return lval_err("Unknown Function!");
}

/* Eval an Sexpr "lval" */
lval* lval_eval_sexpr(lval* v) {
    
    /* Evaluate Children */
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }
    
    /* Error Checking */
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }
    
    /* Empty Expression */
    if (v->count == 0) { return v; }
    
    /* Single Expression */
    if (v->count == 1) { return lval_take(v, 0); }
    
    /* Ensure First Element is Symbol */
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f); lval_del(v);
        return lval_err("S-expression Does not start with symbol!");
    }
    
    /* Call builtin with operator */
    lval* result = builtin(v, f->sym);
    lval_del(f);
    return result;
}

/* Eval an "lval" */
lval* lval_eval(lval* v) {
    /* Evaluate S-expressions */
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
    /* All other lval types remain the same */
    return v;
}

int main(int argc, char** argv) {
    
    /* Construct Some Parsers */
    mpc_parser_t* Number   = mpc_new("number");
    mpc_parser_t* Symbol   = mpc_new("symbol");
    mpc_parser_t* Sexpr    = mpc_new("sexpr");
    mpc_parser_t* Qexpr    = mpc_new("qexpr");
    mpc_parser_t* Expr     = mpc_new("expr");
    mpc_parser_t* Lispy    = mpc_new("lispy");
    
    /* Define them with the following Language */
    mpca_lang(MPCA_LANG_DEFAULT, 
        "                                                      \
            number : /-?[0-9]+/ ;                              \
            symbol : \"list\" | \"head\" | \"tail\"            \
                   | \"join\" | \"eval\"                       \
                   | '+' | '-' | '*' | '/' ;                   \
            sexpr  : '(' <expr>* ')' ;                         \
            qexpr  : '{' <expr>* '}' ;                         \
            expr   : <number> | <symbol> | <sexpr> | <qexpr> ; \
            lispy  : /^/ <expr>* /$/ ;                         \
        ",
        Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
    
    /* Print Version and Exit Information */
    puts("Lispy Version 0.0.0.0.6");
    puts("Press Ctrl+c to Exit\n");
    
    /* In a never ending loop */
    while (1) {
        
        /* Output our prompt and get input */
        char* input = readline("lispy> ");
        
        /* Add input to history */
        add_history(input);
        
        /* Attempt to Parse the user Input */
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            /* On Success eval the AST */
            lval* result = lval_read(r.output);
            result = lval_eval(result);
            lval_println(result);
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
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
    
    return 0;
}
