// #include <stdio.h>
// #include "Vect.c"
#ifdef __linux__
#include <stdlib.h>
#include "utils.c"
#else
#include "../rv64/alloc.c"
#endif

typedef long long word;
typedef struct Atom Atom;
typedef Atom* (*Func)(Atom*, Atom*);

#define Exec ((Atom*) 1)
#define Bang (1)

struct Atom {
    Atom* n, *t;
    word w, r;
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
    #ifdef __linux__
    free(a);
    #else
    reclaim(a, sizeof(struct Atom));
    #endif
    return 0;
}
Atom* ref(Atom* a) {if (a && a != Exec) {a->r++;} return a;}
Atom* nset(Atom* a, Atom* n) {ref(n); del(a->n); a->n = n; return a;}
Atom* tset(Atom* a, Atom* t) {ref(t); del(a->t); a->t = t; return a;}
Atom* pull(Atom* p) {return tset(p, p->t->n);}
Atom* push(Atom* p, Atom* t) {return tset(p, tset(nset(new(), p->t), t));}

void print(Atom* a) {
    if (!a) {puts("None\n"); return;}
    printint(a->w, 8); putchar(':'); printint(a->r, 8);
    puts(" ~~> "); printint((word) a->t, 8); putchar('\n');
    if (a->n) {
        puts("\t\t|--> ");
        print(a->n);
    }
}

Atom* T;
Atom* bang(Atom* a, Atom* p);
Atom* dup(Atom* a, Atom* p) {
    p = push(p, a->t);
    p->t->w = a->w;
    return p;
}
Atom* exec(Atom* a, Atom* p) {
    if (a->t == 0) {return dup(a, p);}
    else if (a->t == Exec) {
        return ((Func) a->w)(p->t, p);
    }
    Atom* e = tset(nset(new(), 0), a->t);
    while (e->t->n && e->t->n->n) {e = tset(nset(new(), e->n), e->t->n);}
    Atom* edel = ref(e);
    while (e) {
        if (e->t->t == Exec && e->t->w == Bang) {p = bang(e->t, p);}
        else if (!e->t->t || e->t->t == Exec) {p = dup(e->t, p);}
        else {p = push(pull(p), e->t->t);}
        e = e->n;
    }
    del(edel);
    return p;
}
Atom* bang(Atom* a, Atom* p) {
    p = pull(p);
    return exec(p->t, p);
}
Atom* open(Atom* a, Atom* p) {
    p = pull(p);
    return ref(tset(nset(new(), p), 0));
}
Atom* close(Atom* a, Atom* p) {
    p = pull(p);
    Atom* n = push(ref(p->n), p->t);
    del(p);
    return n;
}

char contains(char c, char* s) {while (*s != c) {if (!*s) {return 0;} s++;} return c;}
char isseparator(char c) {return c == 0 || contains(c, "! \n\t");}
Atom* printer(Atom* a, Atom* p) {
    puts("OKOKOK\n");
    return pull(p);
}
Atom* pushfunc(Atom* p, Func f) {
    p = push(p, Exec);
    p->t->w = (word) f;
    return p;
}
Atom* token(char* c, Atom* p) {
    int i = 0;
    for (; c[i] == '!'; i++);
    if (i == 1) {return exec(p->t, p);}
    if (i > 1) {
        Atom* t = tset(new(), Exec);
        t->w = Bang;
        while (--i) {t = tset(new(), t);}
        return tset(p, nset(t, p->t));
    }
    if (c[0] == ':') {return push(p, T);}
    if (c[0] == '[') {return pushfunc(p, open);}
    if (c[0] == ']') {return pushfunc(p, close);}
    return pushfunc(p, printer);
}
void mainc() {
    T = ref(new());
    Atom* p;
    p = new();
    p = token("a", p);
    p = token("a", p);
    p = token("!!!", p);
    p = token("!", p);
    p = token("!", p);
    p = token("[", p);
    p = token("!", p);
    p = token("a", p);
    p = token("]", p);
    p = token("!", p);
    print(p);
    print(p->t);
    print(p->t->t);
    del(p);
    del(T);
}
int main() {mainc();}