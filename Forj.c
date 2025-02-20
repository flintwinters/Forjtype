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
enum form {word, func, vect, atom};
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
    // if (a->f == atom) {del(a->w.a);}
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

Atom* exec(Atom* a, Atom* p);
void print(Atom* a, int depth) {
    if (!a) {puts("None\n"); return;}
    if (a->f == word) {BLACK;}
    else if (a->f == func) {GREEN;}
    else if (a->f == vect) {RED;}
    else if (a->f == atom) {YELLOW;}
    if (a->w.f == exec) {puts(" exec ");}
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
    if (a->f == func && a->w.f == exec) {
        pull(p);
        exec(p->t, p);
    }
    else if (a->t) {pushnew(pull(p), a->t);}
    else {dup(a, p);}
    del(a);
    return p;
}
Atom* exec(Atom* a, Atom* p) {
    if (a->t) {
        return array(a->t, p);
    }
    if (a->f == word) {return dup(a, p);}
    else if (a->f == func) {
        if (a->w.f == exec) {
            pull(p);
            return exec(p->t, p);
        }
        return a->w.f(a, p);
    }
    return array(a->t, p);
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
// ,	        -   new empty node
Atom* S, *R;
Atom* token(Atom* p, char* c);
Atom* searchsymbol(Atom* a, Atom* p) {

}
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
    if (--i) {
        return push(t, createmultidot(i));
    }
    t->f = func;
    t->w.f = exec;
    return t;
    // Atom* t = new();
    // if (!--i) {
    //     t->f = func;
    //     t->w.f = exec;
    // }
    // else {push(t, createmultidot(i))->t->e = true;}
    // return t;
}
Atom* token(Atom* p, char* c) {
    int i = 0;
    for (; c[i] == '.'; i++);
    if (i == 1) {return exec(p->t, p);}
    if (i > 1) {
        return push(p, createmultidot(i-1));
        // Atom* t = tset(new(), 0);
        // t->f = func;
        // t->w.f = exec; i--;
        // while (--i) {t = tset(new(), t);}
        // return push(p, t);
    }
    if (c[0] == ':') {
        if (c[1] == 0) {return pushnew(p, R);}
        return push(p, strfromcharp(c+1));
    }
    if (c[0] == ',') {return pushnew(p, 0);}
    if (c[0] == '!') {return pushfunc(p, top);}
    if (c[0] == '[') {return pushfunc(p, open);}
    if (c[0] == ']') {return pushfunc(p, close);}
    if (c[0] == '"') {

    }
    return pushfunc(p, printer);
}
void mainc() {
    R = ref(new()); R->e = true;
    S = ref(new()); S->e = true;
    pushfunc(S, printer);
    S = token(S, "..");
    Atom* p = ref(new()); p->e = true;
    p = token(p, "a");
    p = token(p, ":hello");
    p = token(p, ",");
    p = token(p, "[");
    p = token(p, ".");
    p = token(p, "a");
    p = token(p, "...");
    p = token(p, "]");
    p = token(p, ".");
    p = token(p, ".");
    // p = token(p, ".");
    // // p = token(p, ".");
    // // tset(p, nset(strfromcharp("hello"), p->t));
    p = token(p, "[");
    p = token(p, ".");
    p = token(p, "[");
    p = token(p, ".");
    p = token(p, "a");
    p = token(p, "]");
    p = token(p, ".");
    p = token(p, "]");
    p = token(p, ".");

    // p = token(p, "a");
    p = token(p, "a");
    p = token(p, "a");
    p = token(p, "a");


    p = token(p, "..");
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

    println(p);
    del(p);
    del(R);
    del(S);
}
int main() {mainc();}