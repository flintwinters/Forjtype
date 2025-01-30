#include <stdlib.h>
#include "utils.c"
typedef struct Atom Atom;
typedef long long word;

struct Atom {
    Atom* n, *t;
    word w, r;
};
Atom* new() {return malloc(sizeof(Atom));}
Atom* init(Atom* n, Atom* t) {
    Atom* a = new();
    a->n = n; a->t = t;
    a->w = a->r = 0;
    return a;
}
Atom* del(Atom* a) {
    if (!a) {return 0;}
    if (--a->r) {return a;}
    del(a->n); del(a->t);
    free(a);
    return 0;
}
Atom* ref(Atom* a) {if (a) {a->r++;} return a;}
Atom* pushn(Atom* n) {return init(ref(n), 0);}
Atom* pusht(Atom* t) {return init(0, ref(t));}
Atom* is(Atom* a, Atom* t) {
    if (!a) {return 0;}
    if (a->t == t) {return a;}
    return is(a->t, t);
}
Atom* as(Atom* a, Atom* t) {
    if (!a) {return 0;}
    Atom* n = a->t->n;
    while (!is(n, t)) {a = n->n;}
    if (n) {return n;}
    return as(a->t, t);
}

int main() {}