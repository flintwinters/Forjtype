#include "Vect.c"

typedef long long word;
typedef struct Atom Atom;
typedef union data data;
typedef Atom* (*Func)(Atom*, Atom*);

#define Exec ((Atom*) 1)
#define Bang (1)

union data {
    word w;
    Func f;
    Vect* v;
    Atom* a;
};
struct Atom {
    Atom* n, *t;
    data w; word r;
};
Atom* new() {
    Atom* a = malloc(sizeof(Atom));
    *a = (Atom) {0, 0, 0, 0};
    return a;
}
Atom* del(Atom* a) {
    if (!a || a == Exec) {return a;}
    if (--a->r) {return a;}
    del(a->n); del(a->t);
    reclaim(a, sizeof(struct Atom));
    return 0;
}
Atom* ref(Atom* a) {if (a && a != Exec) {a->r++;} return a;}
Atom* nset(Atom* a, Atom* n) {ref(n); del(a->n); a->n = n; return a;}
Atom* tset(Atom* a, Atom* t) {ref(t); del(a->t); a->t = t; return a;}
Atom* pull(Atom* p) {return tset(p, p->t->n);}
Atom* push(Atom* p, Atom* a) {return tset(p, nset(a, p->t));}
Atom* pushnew(Atom* p, Atom* t) {return tset(p, tset(nset(new(), p->t), t));}

void print(Atom* a) {
    if (!a) {puts("None\n"); return;}
    printint(a->w.w, 8); putchar(':'); printint(a->r, 8);
    puts(" ~~> "); printint((word) a->t, 8); putchar('\n');
    if (a->n) {
        puts("\t\t|--> ");
        print(a->n);
    }
}

Atom* bang(Atom* a, Atom* p);
Atom* dup(Atom* a, Atom* p) {
    pushnew(p, a->t);
    p->t->w = a->w;
    return p;
}
Atom* array(Atom* a, Atom* p) {
    p = (ref(a)->n) ? array(a->n, p) : p;
    if (a->t == Exec && a->w.w == Bang) {bang(a, p);}
    else if (!a->t || a->t == Exec) {dup(a, p);}
    else {pushnew(pull(p), a->t);}
    del(a);
    return p;
}
Atom* exec(Atom* a, Atom* p) {
    if (a->t == 0) {return dup(a, p);}
    else if (a->t == Exec) {return a->w.f(p->t, p);}
    return array(a->t, p);
}
Atom* bang(Atom* a, Atom* p) {
    pull(p);
    return exec(p->t, p);
}
Atom* open(Atom* a, Atom* p) {
    // print(p);
    // print(p->t);
    pull(p);
    return ref(tset(nset(new(), p), 0));
}
Atom* close(Atom* a, Atom* p) {
    pull(p);
    Atom* n = pushnew(p->n, p->t);
    del(p);
    // print(p);
    // print(p->t);
    return n;
}

char contains(char c, char* s) {while (*s != c) {if (!*s) {return 0;} s++;} return c;}
char isseparator(char c) {return c == 0 || contains(c, "! \n\t");}
Atom* printer(Atom* a, Atom* p) {
    puts("OKOKOK\n");
    return pull(p);
}
Atom* pushfunc(Atom* p, Func f) {
    pushnew(p, Exec)->t->w.f = f;
    return p;
}
// :	        -   get t
// :symbol      -   searches stack[1] for a matching symbol.
//                  ie: search(stack[1], "symbol").  Leaves a t pointer to its n on the stack.
// :[	        -   pull S to P
// [	        -   creates an empty, unnamed, non-nil stack, and pushes it to P
// ]	        -   returns.  (pull P to S)
// .	        -   next
// ;	        -   P
// ,	        -   duplicate
Atom* S, *T;
Atom* token(char* c, Atom* p) {
    int i = 0;
    for (; c[i] == '.'; i++);
    if (i == 1) {return exec(p->t, p);}
    if (i > 1) {
        Atom* t = tset(new(), Exec);
        t->w.w = Bang;
        while (--i) {t = tset(new(), t);}
        return tset(p, nset(t, p->t));
    }
    if (c[0] == ':') {return pushnew(p, T);}
    if (c[0] == '[') {return pushfunc(p, open);}
    if (c[0] == ']') {return pushfunc(p, close);}
    if (c[0] == '"') {
        
    }
    return pushfunc(p, printer);
}
int charplen(char* c) {
    int n = 0;
    while (*c++) {n++;}
    return n;
}
Atom* newvect(int maxlen) {
    Vect* v = valloclen(maxlen);
    Atom* a = new();
    a->w.v = v;
    return a;
}
Atom* pushvect(Atom* a, Atom* p) {
    int maxlen = p->t->w.w; pull(p);
    return tset(p, nset(newvect(maxlen), p->t));
}
Atom* strfromcharp(char* c) {
    Atom* s = new();
    pushnew(s, 0);
    s->t->w.w = charplen(c)+1;
    pushvect(s, s);
    cpymem(s->t->w.v->v, c, s->t->w.v->maxlen);
    pushnew(s, S);
    return token("..", s);
}
void mainc() {
    T = ref(new()); S = ref(new());
    Atom* p, *b;
    b = p = ref(new());
    p = token("a", p);
    tset(p, nset(strfromcharp("hello"), p->t));
    p = token("[", p);
    p = token(".", p);
    p = token("[", p);
    p = token(".", p);
    p = token("a", p);
    p = token("]", p);
    p = token(".", p);
    p = token("]", p);
    p = token(".", p);

    p = token("a", p);
    p = token("a", p);


    p = token("...", p);

    p = token(".", p);
    p = token(".", p);
    p = token("[", p);
    p = token(".", p);
    p = token("a", p);
    p = token("]", p);
    p = token(".", p);

    p = token("[", p);
    del(p);
    del(T);
}
int main() {mainc();}