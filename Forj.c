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
    del(a->n); del(a->t);
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
    if (a->w.f == dot) {puts("dot");}
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
        dot(p->t, p);
    }
    else {dup(a, p);}
    del(a);
    return p;
}
Atom* dot(Atom* a, Atom* p) {
    pull(p);
    if (p->t->f == func) {
        return p->t->w.f(p->t, p);
    }
    a = ref(p->t);
    pull(p);
    p = array(a->t, p);
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
Atom* S, *R;
Atom* token(Atom* p, char* c);
int charplen(char* c) {
    int n = 0;
    while (*c++) {n++;}
    return n;
}
Atom* newvect(int maxlen) {
    Vect* v = valloclen(maxlen);
    Atom* a = new();
    a->f = vect;
    a->w.v = v;
    return a;
}
Atom* pushvect(Atom* a, Atom* p) {
    int maxlen = p->t->w.w; pull(p);
    return tset(p, nset(newvect(maxlen), p->t));
}
Atom* strfromcharplen(char* c, int len) {
    Atom* s = new();
    pushnew(s, 0);
    s->t->w.w = len;
    pushvect(s, s);
    cpymem(s->t->w.v->v, c, s->t->w.v->maxlen);
    pushnew(s, S->t);
    return token(s, "..");
}
Atom* strfromcharp(char* c) {return strfromcharplen(c, charplen(c)+1);}
Atom* createmultidot(int i) {
    Atom* t = new();
    if (--i) {return push(t, createmultidot(i));}
    t->f = exec;
    t->w.f = dot;
    return t;
}
Atom* token(Atom* p, char* c) {
    int i = 0;
    for (; c[i] == '.'; i++);
    if (i == 1) {push(p, createmultidot(1)); return dot(p->t, p);}
    if (i > 1) {return push(p, createmultidot(i));}
    if (c[0] == ':') {
        if (c[1] == 0) {return pushnew(p, R);}
        return push(p, strfromcharp(c+1));
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
    Atom* p = ref(new()); p->e = true;
    p = token(p, "a");
    p = token(p, ":hello");
    p = token(p, "0");
    p = token(p, "[");
    p = token(p, ".");
    p = token(p, "a");
    p = token(p, "...");
    p = token(p, "]");
    p = token(p, ".");
    println(p);

    p = token(p, "[");
    p = token(p, ".");
    p = token(p, "[");
    p = token(p, ".");
    p = token(p, "a");
    p = token(p, "]");
    p = token(p, ".");
    p = token(p, "]");
    p = token(p, ".");

    p = token(p, "a");
    p = token(p, "a");
    p = token(p, "a");
    p = token(p, "..");
    p = token(p, ".");
    p = token(p, ".");

    p = token(p, "a");
    p = token(p, "a");
    p = token(p, "a");
    p = token(p, "...");
    p = token(p, ".");
    p = token(p, ".");

    p = token(p, "[");
    p = token(p, ".");
    p = token(p, "a");
    p = token(p, "a");
    p = token(p, "a");
    p = token(p, "a");
    p = token(p, "]");
    p = token(p, ".");

    p = token(p, "[");
    p = token(p, ".");
    // p = token(p, ".");
    p = token(p, "..");
    p = token(p, "]");
    p = token(p, ".");

    p = token(p, ".");
    println(p);
    del(p);
    del(R);
    del(S);
}
int main() {mainc();}