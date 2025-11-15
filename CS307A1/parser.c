// parser.c

#include "parser.h"
#include <ctype.h> // For isspace

// ===================================================================================
// INTERNAL HELPER STRUCTURES AND FUNCTIONS (NOT IN parser.h)
// ===================================================================================
void free_cmdvec(CmdVec *R);

// Internal state enum for AST traversal
typedef enum {
    BEFORE, IN, USCORE, CNT, AFTER, INPUT, OUTPUT, DONE
} Status;


// Not for student interaction.
typedef struct {
    char **items;
    size_t len;
    size_t cap;
} StrVec;

static bool vec_push(StrVec *v, const char *s) {
    if (v->len == v->cap) {
        size_t ncap = v->cap ? v->cap * 2 : 8;
        char **nitems = v->cap ? (char **)realloc(v->items, ncap * sizeof(char *)) : (char**)malloc(ncap * sizeof(char *));
        if (!nitems) return false;
        v->items = nitems; v->cap = ncap;
    }
    v->items[v->len] = strdup(s ? s : "");
    if (!v->items[v->len]) return false;
    v->len++;
    return true;
}

static void vec_free(StrVec *v) {
    if (!v) return;
    for (size_t i = 0; i < v->len; ++i) free(v->items[i]);
    free(v->items);
    v->items = NULL;
    v->len = 0;
    v->cap = 0;
}

// Internal function: Splits a single command string into an argv-style array.
static char **split_cmdvec(const char *s) {
    size_t cap = 8, argc = 0;
    char **argv = malloc(cap * sizeof *argv);
    if (!argv) return NULL;

    const char *p = s;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++; // Skip leading spaces
        if (!*p) break;

        char quote = 0;
        if (*p == '\'' || *p == '"') { quote = *p; p++; } // Handle quotes

        const char *start = p;
        while (*p && (quote ? (*p != quote) : !isspace((unsigned char)*p))) p++;

        size_t len = (size_t)(p - start);
        char *tok = strndup(start, len);
        if (!tok) goto fail;

        if (quote && *p == quote) p++;

        if (argc == cap) {
            cap *= 2;
            char **n = realloc(argv, cap * sizeof *n);
            if (!n) { free(tok); goto fail; }
            argv = n;
        }
        argv[argc++] = tok;
    }

    if (argc == cap) { 
        cap += 1;
        char **n = realloc(argv, cap * sizeof *n);
        if (!n) goto fail;
        argv = n;
    }
    argv[argc] = NULL; // NULL-terminate the argv array
    return argv;

fail: // Cleanup on error
    for (size_t i = 0; i < argc; i++) free(argv[i]);
    free(argv);
    return NULL;
}

// Internal function: Converts a temporary StrVec of command strings into a final CmdVec.
static CmdVec strvec_to_cmdvec(const StrVec *v) {
    CmdVec R = {0}; // Initialize with zeros
    if (!v || v->len == 0) return R;

    R.n = v->len;
    R.argvs = calloc(R.n, sizeof *R.argvs); // Allocate array of char**
    if (!R.argvs) { R.n = 0; return R; }

    for (size_t i = 0; i < v->len; i++) {
        R.argvs[i] = split_cmdvec(v->items[i]);
        if (!R.argvs[i]) {
            free_cmdvec(&R); 
            break;
        }
    }
    return R;
}



int initParser(sparser_t *p) {
    p->Fname = mpc_new("fname"); p->Redirin = mpc_new("redirin");
    p->Redirout = mpc_new("redirout"); p->Redir = mpc_new("redir");
    p->Cmd  = mpc_new("cmd"); p->Pipe = mpc_new("pipe");
    p->Loop = mpc_new("loop"); p->Composite = mpc_new("cmpCmd");
    p->RedirCmd = mpc_new("redirCmd"); p->Quit = mpc_new("quit");
    p->Line = mpc_new("line");

    mpc_err_t *e = mpca_lang(MPCA_LANG_DEFAULT,
        " fname     : /[a-zA-Z_][a-zA-Z0-9_.]*/ ;                                       \n"
        " redirin   : '>' <fname> ;                                                     \n"
        " redirout  : '<' <fname> ;                                                     \n"
        " redir     : <redirin> (<redirout>)? | <redirout> (<redirin>)? ;               \n"
        " cmd       : /[0-9a-zA-Z \\/\\-., \'\"_]+/ ;                                   \n"
        " pipe      : <cmd> ( '|' <cmd> )* ;                                            \n"
        " loop      : (<pipe> '|')? '(' <pipe> ')' '_' /[0-9]/ ('|' <pipe>)? ;          \n"
        " cmpCmd    : <loop> | <pipe> ;                                                 \n"
        " redirCmd  : <cmpCmd>  (<redir>)? ;                                            \n"
        " quit      : \":q\" ;                                                          \n"
        " line      : /^/ <quit> | <redirCmd>  /$/ ;                                    \n",
        p->Fname, p->Redirin, p->Redirout, p->Redir, p->Cmd, p->Pipe, p->Loop, p->Composite, p->RedirCmd, p->Quit, p->Line, NULL);
        
     if(e) {
        mpc_err_print(e);
        mpc_err_delete(e);
        return 0;
     }
     return 1;
}

void freeParser(sparser_t *p) {
    mpc_cleanup(11, p->Fname, p->Redirin, p->Redirout, p->Redir, p->Cmd,
                p->Pipe, p->Loop, p->Composite, p->RedirCmd, p->Quit, p->Line);
}

// --- Command Compilation and Management ---

void initCompiledCmd (compiledCmd *t) {
    t->before.argvs = NULL; t->before.n = 0;
    t->inLoop.argvs = NULL; t->inLoop.n = 0;
    t->after.argvs = NULL;  t->after.n = 0;
    t->loopLen = 0;
    t->inFile = NULL;
    t->outFile = NULL;
    t->isQuit = 0;
}


void freeCompiledCmd (compiledCmd *t) {
    if(t){
        free_cmdvec(&t->before);
        free_cmdvec(&t->inLoop);
        free_cmdvec(&t->after);
        if (t->inFile) { free(t->inFile); t->inFile = NULL; }
        if (t->outFile) { free(t->outFile); t->outFile = NULL; }
    }
}


void free_cmdvec(CmdVec *R) {
    if (!R || !R->argvs) return;
    for (size_t i = 0; i < R->n; i++) {
        if (!R->argvs[i]) continue;
        for (size_t j = 0; R->argvs[i][j]; j++) free(R->argvs[i][j]);
        free(R->argvs[i]);
    }
    free(R->argvs);
    R->argvs = NULL;
    R->n = 0;
}


int compileCommand (sparser_t *sp, char *input, compiledCmd* Shelltrv) {
    mpc_result_t result;

    initCompiledCmd(Shelltrv); // Always initialize before parsing

    if (mpc_parse("<stdin>", input, sp->Line, &result)) {
        mpc_ast_t *ast = result.output;
        mpc_ast_trav_t *trav = mpc_ast_traverse_start(ast, mpc_ast_trav_order_pre);
        mpc_ast_t *curr = NULL;
        
        StrVec temp_before = {0}, temp_inLoop = {0}, temp_after = {0};
        Status s = BEFORE;

        curr = mpc_ast_traverse_next(&trav);
        while (curr) {
            if (strstr(curr->tag, "quit")) {
                Shelltrv->isQuit = 1;
                break;
            }

            if (s == BEFORE) {
                if (strstr(curr->tag, "cmd|regex")) { vec_push(&temp_before, curr->contents); }
                else if (strstr(curr->tag, "char") && strstr(curr->contents, "(")) { s = IN; }
            } else if (s == IN) {
                if (strstr(curr->tag, "cmd|regex")) { vec_push(&temp_inLoop, curr->contents); }
                else if (strstr(curr->tag, "char") && strstr(curr->contents, ")")) { s = USCORE; }
            } else if (s == USCORE) {
                if (!(strstr(curr->tag, "char") && strstr(curr->contents, "_"))) { /* Error or unexpected token */ }
                s = CNT;
            } else if (s == CNT) {
                if (strstr(curr->tag, "regex")) {
                    Shelltrv->loopLen = (size_t)(curr->contents[0] - '0');
                    s = AFTER;
                }
            } else if (s == AFTER) {
                if (strstr(curr->tag, "cmd|regex")) { vec_push(&temp_after, curr->contents); }
            } else if (s == INPUT) {
                if (strstr(curr->tag, "fname|regex")) { Shelltrv->inFile = strdup(curr->contents); s = DONE; }
            } else if (s == OUTPUT) {
                if (strstr(curr->tag, "fname|regex")) { Shelltrv->outFile = strdup(curr->contents); s = DONE; }
            }

            if (strstr(curr->tag, "char")) {
                if (strstr(curr->contents, "<")) { s = INPUT; }
                else if (strstr(curr->contents, ">")) { s = OUTPUT; }
            }
            curr = mpc_ast_traverse_next(&trav);
        }

        Shelltrv->before = strvec_to_cmdvec(&temp_before);
        Shelltrv->inLoop = strvec_to_cmdvec(&temp_inLoop);
        Shelltrv->after  = strvec_to_cmdvec(&temp_after);

        vec_free(&temp_before);
        vec_free(&temp_inLoop);
        vec_free(&temp_after);

        mpc_ast_delete(result.output);
        return 1;
    } else {
        mpc_err_print(result.error);
        mpc_err_delete(result.error);
        return 0;
    }
}

// --- Debugging and Inspection ---

// Helper to print a single CmdVec for debugging
static void print_cmdvec_internal(const char* name, CmdVec *cv) {
    printf("Printing %s vector (%zu commands):\n", name, cv->n);
    if (cv->n > 0) {
        for (size_t i = 0; i < cv->n; i++) {
            printf("  - Command %zu: ", i);
            if (cv->argvs[i]) {
                for (size_t j = 0; cv->argvs[i][j]; j++) {
                    printf("'%s' ", cv->argvs[i][j]);
                }
            } else {
                printf("(NULL argv)");
            }
            printf("\n");
        }
    } else {
        printf("  (empty)\n");
    }
}

void printCompiledCmd(compiledCmd *c) {
    if (!c) {
        printf("Command is NULL.\n");
        return;
    }
    printf("--- Parsed Command ---\n");
    if (c->isQuit) {
        printf("Quit command detected.\n");
        return;
    }
    
    print_cmdvec_internal("Before", &c->before);
    print_cmdvec_internal("InLoop", &c->inLoop);
    printf("Loop iteration count: %zu\n", c->loopLen);
    print_cmdvec_internal("After", &c->after);

    if (c->inFile) printf("Input redirection: %s\n", c->inFile);
    else printf("No input redirection.\n");

    if (c->outFile) printf("Output redirection: %s\n", c->outFile);
    else printf("No output redirection.\n");
    printf("----------------------\n");
}
