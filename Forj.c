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
Atom* S, *R, *P;

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
Atom* push(Atom* p, Atom* a) {
    if (p->t) {
        if (p->t->e == 2) {
            nset(a, p->t->n);
            a->e = true;
            return tset(p, a);
        }
        nset(a, p->t);
    }
    else {
        a->e = true;
        a->n = (p->n) ? p->n->t : 0;
    }
    return tset(p, a);
}
Atom* pushnew(Atom* p, Atom* t) {return push(p, tset(new(), t));}
Atom* pull(Atom* p) {
    if (p->t->e) {
        pushnew(p, 0);
        nset(p->t, p->t->n->n);
        p->t->e = 2;
        return p;
    }
    return tset(p, p->t->n);
}

Atom* dot(Atom* a, Atom* p);
bool isstr(Atom* str) {return str->t && str->t->n && str->t->n->t == S->t;}
Vect* getstrvect(Atom* str) {return str->t->n->n->t->w.v;}
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
    else if (a->f == vect) {
        puts(a->w.v->v);
    }
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
Atom* pushw(Atom* p, int l) {
    pushnew(p, 0);
    p->t->w.w = l;
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
    return pushnew(p, p->t);
}
Atom* pulls(Atom* a, Atom* p) {return pull(p);}
Word pullw(Atom* p) {
    Word w = p->t->w.w;
    pull(p);
    return w;
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
Atom* token(Atom* p, char* c);
char contains(char c, char* s) {while (*s != c) {if (!*s) {return 0;} s++;} return c;}
char isseparator(char c) {return c == 0 || contains(c, ". \n\t\b\r");}
bool equstr(Atom* a, Atom* b) {
    char* as = a->t->w.v->v;
    char* bs = b->t->w.v->v;
    for (int i = 0; i < a->t->w.v->len; i++) {
        if (as[i] != bs[i]) {return false;}
    }
    return true;
}
Atom* printer(Atom* a, Atom* p) {
    puts("OKOKOK\n");
    return pull(p);
}
Atom* scan(Atom* a, Atom* p) {
    pull(p);
    Atom* s = p->t;
    pull(p);
    a = p->t;
    while (a) {
        if (isstr(a) && equstr(a->t->n->n, s)) {
            pushnew(p, a->n->t);
            p->t->f = a->n->f;
            p->t->w.w = a->n->w.w;
            return p;
        }
        a = a->n;
    }
    return p;
}
Atom* search(Atom* a, Atom* p) {
    // scan all
    return pull(p);
}
Atom* pushfunc(Atom* p, Func f) {
    pushnew(p, 0);
    p->t->w.f = f;
    p->t->f = func;
    return p;
}
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
    a->w.w = v->len = maxlen;
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
    s->t->w.w = len;
    pushvect(s->t, s);
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

Atom* strtoint(Atom* a, Atom* p) {
    pull(p);
    Vect* v = getstrvect(p->t);
    int n = 0;
    int b = 10;
    char* str = v->v;
    Word i = 0;
    bool neg = false;
    if (str[i] == '-') {neg = true; i--;}
    if (str[i] == '0') {
        if (str[i-1] == 'x') {b = 0x10; i += 2;}
        if (str[i-1] == 'b') {b = 2; i += 2;}
    }
    while (i < v->len-1) {
        n *= b;
        if (str[i] >= 'a' && str[i] <= 'f') {
            n += str[i]-'a';
        }
        else {n += str[i]-'0';}
        i++;
    }
    if (neg) {n *= -1;}
    pull(p);
    pushw(p, n);
    return p;
}
Atom* charptostr(Atom* p, char* c) {
    int i = 0;
    while (1) {
        if (c[i] == '"') {break;}
        if (c[i] == '\\') {i++;}
        i++;
    }
    Atom* s = new();
    pushw(s, i+1);
    Vect* v = pushvect(s->t, s)->t->t->w.v;
    pushnew(s, S->t);
    token(s, "..");
    i = 0;
    while (1) {
        if (c[i] == '"') {break;}
        if (c[i] == '\\') {i++;}
        // TODO: ADD \n, \t, ETC //
        v->v[i] = c[i];
        i++;
    }
    v->v[i] = 0;
    return push(p, s);
}
Atom* splitstrat(Atom* a, Atom* p) {
    Word i = pullw(p);
    Vect* str = getstrvect(p->t);
    push(p, newstrlen(str->v+i, str->len-i));
    str->v[i] = 0;
    return p;
}
int indexofchar(Atom* s, char c) {
    Vect* str = getstrvect(s);
    int i = str->len;
    while (i--) {if (str->v[i] == c) {return i;}}
    return -1;
}
Atom* splitfind(Atom* a, Atom* p) {
    Atom* s = ref(p->t);
    char* c = getstrvect(s)->v;
    pull(p);
    Vect* str = getstrvect(p->t);
    int i = str->len;
    while (i--) {
        if (contains(str->v[i], c)) {
            pushw(p, i); break;
        }
    }
    if (i == -1) {}
    splitstrat(0, p);
    del(s);
    return p;
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
        return charptostr(p, c+1);
    }
    if (contains(c[0], "0123456789")) {
        push(p, newstr(c));
        pushfunc(p, strtoint);
        return token(p, ".");
    }
    return p;
}
Atom* tokens(Atom* p, char* c) {
    // Repeatedly call token() //
    Atom* s = ref(newstr(" \n\t\b\r."));
    push(p, s);
    splitfind(0, p);
}
void mainc() {
    R = ref(new()); R->e = true;
    S = ref(new()); S->e = true;
    pushfunc(S, scan);
    S = token(S, "..");
    P = ref(new()); P->e = true;
    P = token(P, "a");
    P = token(P, "5");
    P = token(P, "5");
    P = token(P, "8");
    P = token(P, ":hello");
    P = token(P, "\"hi\"");

    P = token(P, "0");
    P = token(P, "[");
    P = token(P, ".");
    P = token(P, "0");
    P = token(P, "[");
    P = token(P, ".");
    P = token(P, ":hi");
    P = token(P, "]");
    P = token(P, ".");
    P = token(P, ".");
    P = token(P, ".");
    P = token(P, "]");
    P = token(P, ".");

    P = token(P, "0");
    P = token(P, "[");
    P = token(P, ".");
    P = token(P, ":hi");
    P = token(P, ".");
    P = token(P, "]");
    P = token(P, ".");
    
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