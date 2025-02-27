#include "Vect.c"
#include <stdio.h>

typedef long long Word;
typedef struct Atom Atom;
typedef union data data;
typedef enum form form;
typedef Atom* (*Func)(Atom*, Atom*);

union data {
    Word w;
    Func f;
    Vect* v;
    Atom* a;
};
enum form {word, func, vect, atom, exec};
struct Atom {
    Atom* n, *t; // next.  type.
    data w;
    form f;  // shape of w;
    short r; // reference counter
    short e; // 'end'.  true means n points to the parent
};
Atom* new() {
    Atom* a = malloc(sizeof(Atom));
    *a = (Atom) {0, 0, 0, 0, 0, 0};
    return a;
}
Atom* del(Atom* a) {
    if (!a) {return a;}
    if (--a->r) {return a;}
    if (a->f == vect) {freevect(a->w.v);}
    Atom* e = a->t;
    while (e) {
        if (e->e) {e->n = 0;}
        e = e->n;
    }
    del(a->n);
    del(a->t);
    reclaim(a, sizeof(struct Atom));
    return 0;
}
Atom* get(Atom* a, int i) {
    if (i == 0) {return a;}
    if (i >= 0) {return get(a->n, i-1);}
    if (!a->e) {return get(a->n, i);}
    return a;
}
Atom* ref(Atom* a) {if (a) {a->r++;} return a;}
Atom* nset(Atom* a, Atom* n) {
    if (a->e) {a->e = false;}
    if (!n) {a->e = true;}
    ref(n); del(a->n); a->n = n; return a;
}
Atom* tset(Atom* a, Atom* t) {ref(t); del(a->t); a->t = t; return a;}
Atom* pull(Atom* p) {return tset(p, p->t->n);}
Atom* push(Atom* p, Atom* a) {
    if (p->t) {
        nset(a, p->t);
    }
    else {
        a->e = true;
        a->n = (p->n) ? p->n->t : 0;
    }
    return tset(p, a);
}
Atom* pushnew(Atom* p, Atom* t) {return push(p, tset(new(), t));}

Atom* dot(Atom* a, Atom* p);
void print(Atom* a, int depth) {
    if (!a) {puts("None\n"); return;}
    if (a->f == word) {BLACK;}
    else if (a->f == func) {GREEN;}
    else if (a->f == exec) {DARKRED;}
    else if (a->f == vect) {RED;}
    else if (a->f == atom) {YELLOW;}
    printint((Word) a, 8);
    putchar(' ');
    if (a->f == exec) {puts("dot");}
    else {printint(a->w.w, 4);}
    if (a->r != 1) {RED; printint(a->r, 4);}
    RESET; puts("\n");
    if (a->t) {
        PURPLE;
        for (int i = 0; i < depth+1; i++) {puts("  ");}
        if (a->t->e) {puts(" ┗ ");}
        else {puts(" ┣ ");}
        print(a->t, depth+1);
    }
    if (!a->e) {
        DARKYELLOW;
        for (int i = 0; i < depth; i++) {puts("  ");}
        if (a->n->e) {puts(" ┗ ");}
        else {puts(" ┣ ");}
        print(a->n, depth);
    }
}
void println(Atom* a) {print(a, 0); putchar('\n');}

Atom* dup(Atom* a, Atom* p) {
    pushnew(p, a->t);
    p->t->f = a->f;
    p->t->w = a->w;
    return p;
}
Atom* array(Atom* a, Atom* p) {
    p = (!ref(a)->e) ? array(a->n, p) : p;
    if (a->f == exec) {
        if (a->t) {dup(a->t, p);}
        else {p = dot(p->t, p);}
    }
    else {
        dup(a, p);
    }
    del(a);
    return p;
}
Atom* dot(Atom* a, Atom* p) {
    a = p->t;
    if (a->f == func) {return a->w.f(a, p);}
    a = ref(a);
    pull(p);
    if (a->f == exec) {
        if (a->t) {dup(a->t, p);}
        else {dot(a, p);}
    }
    else {p = array(a->t, p);}
    del(a);
    return p;
}
Atom* open(Atom* a, Atom* p) {
    pull(p);
    a = (p->t) ? p->t->t : 0;
    return ref(tset(nset(new(), p), a));
}
Atom* close(Atom* a, Atom* p) {
    pull(p);
    tset(p->n->t, p->t);
    Atom* n = ref(p->n);
    del(p);
    return del(n);
}
Atom* top(Atom* a, Atom* p) {
    pull(p);
    return push(p, p->t);
}
Atom* pulls(Atom* a, Atom* p) {return pull(p);}

char contains(char c, char* s) {while (*s != c) {if (!*s) {return 0;} s++;} return c;}
char isseparator(char c) {return c == 0 || contains(c, ". \n\t\b\r");}
Atom* printer(Atom* a, Atom* p) {
    puts("OKOKOK\n");
    return pull(p);
}
Atom* pushfunc(Atom* p, Func f) {
    pushnew(p, 0);
    p->t->w.f = f;
    p->t->f = func;
    return p;
}
// .	        -   dot - execute
// :	        -   root directory
// :symbol      -   searches stack[1] for a matching symbol.
//                  ie: search(stack[1], "symbol")
// [	        -   points P into stack[1]
// ]	        -   pull P to S
// ;	        -   P
// !            -   point to stack[1]
// ,	        -   pull
Atom* S, *R, *P;
Atom* token(Atom* p, char* c);
int charplen(char* c) {
    int n = 0;
    while (*c++) {n++;}
    return n;
}
Atom* newvect(int maxlen) {
    Vect* v = valloclen(maxlen);
    Atom* a = pushnew(new(), 0);
    a->t->f = vect;
    a->t->w.v = v;
    v->len = maxlen;
    return a;
}
Atom* pushvect(Atom* a, Atom* p) {
    int maxlen = a->w.w;
    pull(p);
    return push(p, newvect(maxlen));
}
Atom* newstrlen(char* c, int len) {
    Atom* s = new();
    pushnew(s, 0);
    s->w.w = len;
    pushvect(s, s);
    cpymem(s->t->t->w.v->v, c, s->t->t->w.v->maxlen);
    pushnew(s, S->t);
    return token(s, "..");
}
Atom* newstr(char* c) {return newstrlen(c, charplen(c)+1);}
Atom* createmultidot(int i) {
    Atom* t = new();
    t->f = exec;
    if (--i) {return push(t, createmultidot(i));}
    t->w.f = dot;
    return t;
}
Atom* token(Atom* p, char* c) {
    int i = 0;
    for (; c[i] == '.'; i++);
    if (i == 1) {return dot(p->t, p);}
    if (i > 1) {return push(p, createmultidot(i-1));}
    if (c[0] == ':') {
        if (c[1] == 0) {return pushnew(p, R);}
        return push(p, newstr(c+1));
    }
    if (c[0] == 'a') {return pushfunc(p, printer);}
    if (c[0] == ',') {return pushfunc(p, pulls);}
    if (c[0] == '!') {return pushfunc(p, top);}
    if (c[0] == '[') {return pushfunc(p, open);}
    if (c[0] == ']') {return pushfunc(p, close);}
    if (c[0] == '"') {
        // TODO //
        return p;
    }
    if (c[0] == '0') {return pushnew(p, 0);}

    return p;
}
void mainc() {
    R = ref(new()); R->e = true;
    S = ref(new()); S->e = true;
    pushfunc(S, printer);
    S = token(S, "..");
    P = ref(new()); P->e = true;
    P = token(P, "a");
    P = token(P, ":hello");
    P = token(P, "."); // OK
    P = token(P, "0");
    P = token(P, "[");
    P = token(P, ".");
    P = token(P, "a");
    P = token(P, "...");
    P = token(P, "]");
    P = token(P, ".");
    
    P = token(P, ".");
    P = token(P, "."); // OK

    P = token(P, "0");
    P = token(P, "[");
    P = token(P, ".");
    P = token(P, "0");
    P = token(P, "[");
    P = token(P, ".");
    P = token(P, "a");
    P = token(P, "]");
    P = token(P, ".");
    P = token(P, "]");
    P = token(P, ".");
    P = token(P, ".");

    P = token(P, "a");
    P = token(P, "a");
    P = token(P, "a");
    P = token(P, "...");
    P = token(P, ".");
    P = token(P, "."); // OK
    P = token(P, "."); // OK

    P = token(P, "0");
    P = token(P, "a");
    P = token(P, "...");
    P = token(P, ".");
    P = token(P, "."); // OK

    P = token(P, "[");
    P = token(P, ".");
    P = token(P, "0");
    P = token(P, "a");
    P = token(P, "a");
    P = token(P, "a");
    P = token(P, "]");
    P = token(P, ".");

    P = token(P, "[");
    P = token(P, ".");
    // P = token(P, ".");
    P = token(P, "[");
    P = token(P, ".");
    P = token(P, "0");
    P = token(P, "]");
    P = token(P, ".");
    P = token(P, "...");
    P = token(P, "]");
    P = token(P, ".");

    P = token(P, "."); 
    P = token(P, "."); // OK
    println(P);
    del(P);
    del(R);
    del(S);
}
int main() {mainc();}