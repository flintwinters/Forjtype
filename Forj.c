#include <stdio.h>
#include <stdlib.h>
#include "Vect.c"

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
    free(a);
    return 0;
}
Atom* ref(Atom* a) {if (a && a != Exec) {a->r++;} return a;}
Atom* nset(Atom* a, Atom* n) {ref(n); del(a->n); a->n = n; return a;}
Atom* tset(Atom* a, Atom* t) {ref(t); del(a->t); a->t = t; return a;}
Atom* pull(Atom* p) {return tset(p, p->t->n);}
Atom* push(Atom* p, Atom* t) {return tset(p, tset(nset(new(), p->t), t));}

void print(Atom* a) {
    if (!a) {printf("None\n"); return;}
    printf("%llx:%llx ~~> %p\n", a->w, a->r, a->t);
    if (a->n) {
        printf("\t\t|--> ");
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

Atom* printer(Atom* a, Atom* p) {
    printf("OKOKOK\n");
    return pull(p);
}
Atom* token(char* c, Atom* a, Atom* p) {
    int i = 0;
    for (; c[i] == '!'; i++);
    if (i == 1) {return exec(a, p);}
    if (i > 1) {
        Atom* t = tset(new(), Exec);
        t->w = Bang;
        while (--i) {t = tset(new(), t);}
        return tset(p, nset(t, p->t));
    }
    if (c[0] == ':') {return push(p, T);}

    p = push(p, Exec);
    p->t->w = (word) printer;
    return p;
}
int main() {
    T = ref(new());
    Atom* p;
    p = ref(new());
    p = token("a", 0, p);
    p = token("a", 0, p);
    p = token("!!!", 0, p);
    p = token("!", p->t, p);
    p = token("!", p->t, p);
    print(p);
    print(p->t);
    del(p);
    del(T);
}