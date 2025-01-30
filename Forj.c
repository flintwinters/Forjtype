#include <stdlib.h>
typedef struct Atom Atom;
typedef long long word;
typedef Atom* (*Go)(Atom*, Atom*);

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
    if (!a) {return 0;}
    if (--a->r) {return a;}
    del(a->n); del(a->t);
    free(a);
    return 0;
}
Atom* ref(Atom* a) {if (a) {a->r++;} return a;}
Atom* nset(Atom* a, Atom* n) {del(a->n); a->n = ref(n); return a;}
Atom* tset(Atom* a, Atom* t) {del(a->t); a->t = ref(t); return a;}
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
Atom* last(Atom* a) {
    if (a->n) {return last(a->n);}
    return a;
}
Atom* append(Atom* a, Atom* t) {return tset(nset(last(a), new())->n, t);}
Atom* prepend(Atom* n, Atom* t) {return tset(nset(new(), n), t);}

Atom* ptr;
Atom* val(Atom* a, Atom* e) {
    e = prepend(e, a->t);
    e->w = a->w;
    return ref(e);
}
Atom* get(Atom* a, Atom* e) {
    e = prepend(e, ptr);
    e->w = (word) a->t;
    return ref(e);
}
Atom* set(Atom* a, Atom* e) {
    tset(a, e->t);
    return e;
}
int main() {
    Atom* T = new();
    Atom* Tget = ref(append(T, new()));
    Atom* goer = ref(tset(Tget->t, T));
    Atom* valu = ref(tset(new(), T));
    ptr = ref(tset(new(), valu));
    append(valu, goer)->w = (word) val;
    Atom* symbol = ref(tset(new(), T));
    Atom* Tset = ref(append(symbol, Tget->t));
    Tset->w = (word) set;
    Tget->w = (word) get;

    del(Tset);
    del(symbol);
    del(ptr);
    del(valu);
    del(goer);
    del(Tget);
    del(T);
}