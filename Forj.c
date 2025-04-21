#include "Vect.c"
#include <stdio.h>

typedef long long Word;
typedef struct Atom Atom;
typedef union data data;
typedef enum form form;

// A program thread has three components:
// The structure and way these are accessed will probably change.
// 1.   "P" - data pointer stack
//      This stack tracks the working data branch
//      a new value is pushendd to this stack when
//      entering into an array, using [. for example
// 2.   "E" - execution stack
//      This stack is needed for multithreaded debugging.
//      Future non-debug threads may take advantage of
//      OS threads for efficiency.
//      Elements on this stack point to to-be-handled
//      atoms.  When an array is run, the environment
//      must get the last element and back out using
//      breadcrumbs held by this stack.
// 3.   "R" - optional residual stack
//      Reversible operations need to add their
//      residuals to this stack so they can be retrieved
//      at undo-time.
typedef void (*Func)(Atom*, Atom*, Atom*);

union data {
    Word w;
    Func f;
    Vect* v;
    Atom* a;
};
enum form {word, func, vect, atom, exec};

// Can t and w be combined into one member?
struct Atom {
    Atom* n, *t; // next.  type.
    data w;
    form f;  // shape of w;
    short r; // reference counter
    short e; // 'end'.  true means n points to the parent
};

Atom* G;
bool debugging = false;

// Creates a new, zero-initialized atom with no references.
Atom* new() {
    Atom* a = malloc(sizeof(Atom));
    *a = (Atom) {0, 0, 0, 0, 0, true};
    return a;
}
Atom* get(Atom* a, int i);

// Deletes a reference to an atom.
// If the atom reaches zero references, free its memory.
// Also frees vects:
//  - Vects don't have refcounts, so must be referenced by
//    exactly one atom at all times.
Atom* del(Atom* a) {
    if (!a) {return 0;}
    if (--a->r) {return a;}
    
    if (a->f == vect) {freevect(a->w.v);}
    if (a->e == false) {del(a->n);}
    if (del(a->t)) {
        // If a->t was referenced by something else,
        // and a owns it, the parent points must be corrected.
        Atom* n = get(a->t, -1);
        if (n->n == a) {n->n = 0;}
    }
    reclaim(a, sizeof(struct Atom));
    // return b;
    return 0;
}
// Get a reference to a.
// Does nothing if a == 0.
Atom* ref(Atom* a) {if (a) {a->r++;} return a;}
// Set a's n value.  Adjust references accordingly.
// Adjusts the 'end' flag if needed.
Atom* nset(Atom* a, Atom* n) {
    ref(n);
    if (!a->e) {del(a->n);}
    a->e = !n;
    a->n = n;
    return a;
}
// Set a's t value.  Adjust references accordingly.
Atom* tset(Atom* a, Atom* t) {
    ref(t);
    del(a->t);
    a->t = t;
    return a;
}
// Points p->t to a and appends a to the old p->t
// e == 2 is used only in tandem with pull: it signifies the
// empty list, while still holding a parent pointer.
Atom* push(Atom* p, Atom* a) {
    if (p->t) {
        if (p->t->e == 2) {
            if (!a->e) {del(a->n);}
            if (p->t) {a->n = p->t->n;}
            a->e = true;
            return tset(p, a);
        }
        nset(a, p->t);
    }
    else {
        if (!a->e) {del(a->n);}
        a->n = p;
        a->e = true;
    }
    return tset(p, a);
}
// Init a new atom pointing to t, and push it to p.
Atom* pushnew(Atom* p, Atom* t) {return push(p, tset(new(), t));}

// Unique push function which should only be used on
// an empty p.  Creates a new signifier atom, that
// marks an empty p.  Note that t is not ref'd.
Atom* pushend(Atom* p, Atom* t) {
    Atom* e = new();
    e->n = t;
    e->e = 2;
    tset(p, e);
    return p;
}
// Pops an element from the top of p.  If it's the last element,
// replace it with an e == 2 atom.
void pull(Atom* p) {
    if (p->t->e) {pushend(p, p->t->n);}
    else {tset(p, p->t->n);}
}
// Pulls without deleting the resulting atom, and returns it.
Atom* pulln(Atom* p) {
    Atom* a = ref(p->t);
    pull(p);
    return a;
}
// Push a number
Atom* pushw(Atom* p, Word l) {
    pushnew(p, 0);
    p->t->w.w = l;
    return p;
}
// Pull an atom and return the word it was storing
Word pullw(Atom* p) {
    Word w = p->t->w.w;
    pull(p);
    return w;
}

// Duplicates a onto p
Atom* dup(Atom* a, Atom* p) {
    pushnew(p, a->t);
    p->t->f = a->f;
    p->t->w = a->w;
    return p;
}
void dot(Atom* g);
Atom* chscan(Atom* a, char* c, bool full);
Atom* P(Atom* g) {return chscan(g->t, "P", true);}
Atom* E(Atom* g) {return chscan(g->t, "E", true);}
Atom* R(Atom* g) {return chscan(g->t, "R", true);}
// push residual
void throwr(Atom* r, Atom* p, int count) {
    pushnew(r, p->t);
    r->t->w.w = count;
    for (int i = 0; i < count; i++) {pull(p);}
}
Atom* open(Atom* p) {
    Atom* pt = pushnew(p, 0)->t;
    pt->w.a = pt->n->t;
    if (pt->n->t) {
        if (pt->n->t->t) {tset(pt, pt->n->t->t);}
        else {pushend(pt, pt->n->t);}
    }
    return p;
}
Atom* close(Atom* p) {
    if (p->t->w.a) {
        if (p->t->t->e == 2) {tset(p->t->w.a, 0);}
        else {tset(p->t->w.a, p->t->t);}
    }
    pull(p);
    return p;
}

// Executing behavior for a list.
// [ a b c ] .
// ^ From left to right, (bottom to top of stack),
// dup the element onto p.  If it's a multidot (ie: ...),
// Execute it, reducing by one.
// Double dot (..) will run dot() on the top of stack.
void exechandler(Atom* a, Atom* p, Atom* g) {
    if (a->f == exec) {
        if (!a->t) {dot(g);}
        else {dup(a->t, p);}
    }
    else {dup(a, p);}
}
bool run(Atom* g) {
    Atom* e = E(g);
    if (!e->t || !e->t->t) {return false;}
    if (e->t->t->e == 2) {
        pull(e);
        return false;
    }
    Atom* e2 = pulln(e->t);
    if (e->t->t->e == 2) {
        pull(e);
    }
    exechandler(e2->t, P(g)->t, g);
    del(e2);
    return E(g)->t && E(g)->t->e != 2;
}
void growexec(Atom* a, Atom* g) {
    Atom* e = pushnew(E(g), 0)->t;
    pushnew(e, a);
    while (!a->e) {pushnew(e, a = a->n);}
}
// Single dot, or activated double dot:
//      A single dot '.', runs immediately when it is
//      parsed (at parse-time).  Whereas a double dot
//      '..' merely pushes this function.
void dot(Atom* g) {
    Atom* p = P(g)->t;
    Atom* a = p->t;
    // If 'a' holds a function pointer, run it
    if (a->f == func) {return a->w.f(a, p, g);}
    a = pulln(p);
    // a->f == exec indicates 'a' is a dot ie: ..
    if (a->f != exec && a->t) {growexec(a->t, g);}
    else {exechandler(a, P(g)->t, g);}
    del(a);
}
void dotter(Atom* a, Atom* p, Atom* g) {dot(g);}
// Get the ith index next node after a
Atom* get(Atom* a, int i) {
    while (a && !a->e && i--) {
        a = a->n;
    }
    return a;
}
int len(Atom* a) {
    if (!a) {return 0;}
    if (a->e == 2) {return 0;}
    int i = 1;
    while (!a->e) {a = a->n; i++;}
    return i;
}
// Open the top atom.
//      Points p inside the top atom.
//      Until ] is called, everything will take place
//      inside that atom.
void openatom(Atom* a, Atom* p, Atom* g)  {pull(p); open(P(g));}
// Close the current atom ]
void closeatom(Atom* a, Atom* p, Atom* g) {pull(p); close(P(g));}
// Func wrapping of len
void getlen(Atom* a, Atom* p, Atom* g) {
    pull(p);
    pushw(p, len(p->t));
}
// Wrapper for `run`, to step through threads.
void stepprog(Atom* a, Atom* p, Atom* g) {
    pull(p);
    run(p->t);
}
// Swap the top two elements.
void swap(Atom* p) {
    Atom* b = p->t->n;
    p->t->n = p->t->n->n;
    b->n = p->t;
    p->t = b;
    // Swap the 'end' flags
    short e = b->e;
    b->e = b->n->e;
    b->n->e = e;
}
// <~ inbuilt function
// If the top element has no type, set its type to the next
// Element, otherwise move the type that already exists to its next element
void next(Atom* a, Atom* p, Atom* g) {
    pull(p);
    if (p->t->t) {tset(p->t, p->t->t->n);}
    else {tset(p->t, p->t->n);}
}
// ~> inbuilt
void enter(Atom* a, Atom* p, Atom* g) {
    pull(p);
    if (p->t->t) {tset(p->t, p->t->t->t);}
    else {tset(p->t, p->t->n->t);}
}
// Wrap pull in 'Func' type compatible function
void consume(Atom* a, Atom* p, Atom* g) {
    pull(p);
    if (!p->e) {dup(p->w.a->n, p);}
    else {pushw(p, pullw(g));}
}
void throw(Atom* a, Atom* p, Atom* g) {
    pull(p);
    if (!p->e) {
        dup(p->t, p->n);
        pull(p);
    }
    else {pushw(g, pullw(p));}
}

// .	        -   dot - execute
// :	        -   root directory
// :symbol      -   a function which searches
//                  stack[1] for a matching symbol.
//                  ie: search(stack[1], "symbol")
// [	        -   points P into stack[1]
// ]	        -   pull P to S
// ;	        -   P
// !            -   point to stack[1]
// ,	        -   pull
void token(Atom* g, char* c);
// Returns c if c is in s, else 0
char contains(char c, char* s) {while (*s != c) {if (!*s) {return 0;} s++;} return c;}
char iswhitespace(char c) {return contains(c, " \n\t\b\r");}
// Returns true if a == b
bool equstr(char* a, char* b) {
    while (*a == *b && *a && *b) {a++; b++;}
    if (*a || *b) {return false;}
    return true;
}
void scan(Atom* a, Atom* p, Atom* g);
// Safetly checks if the atom holds a string
// In the future, methods will be written in Forj to 
// compare to templates, making this more elegant.
bool isstr(Atom* str) {
    if (!str->t) {return false;}
    str = get(str->t, 1);
    return str && str->w.f == scan;
}
// Convenience function gets the Vect from a string.
Atom* getstr(Atom* str) {return get(str->t, 2);}

// Convenience function to push a function pointer
Atom* pushfunc(Atom* p, Func f) {
    pushnew(p, 0);
    p->t->w.f = f;
    p->t->f = func;
    return p;
}
void runfunc(Atom* g, Func f) {
    pushfunc(P(g)->t, f);
    dot(g);
}
// Searches straight down.  If a list has a parent,
// It will be searched as well.
// This is used for implicit variable search: `val :name name`
// This is related to the :symbol functionality.
// Since one scan call leaves (a reference to) the associated
// variable on the top, :symbol2 can be called again to get
// a variable from the second, interior layer.
Atom* chscan(Atom* a, char* c, bool full) {
    while (a && (full || !a->e)) {
        if (!a->e && isstr(a) &&
            equstr(getstr(a)->t->w.v->v, c)) {
            return a->n;
        }
        if (full && !a->e && a->f == exec) {
            Atom* v = chscan(a->n, c, false);
            if (v) {return v;}
        }
        a = a->n;
    }
    return 0;
}
Atom* varscan(Atom* a, Atom* s) {
    a = chscan(a, s->t->w.v->v, true);
    del(s);
    if (!a) {return 0;}
    Atom* v = new();
    tset(v, a->t);
    v->f = a->f;
    v->w.w = a->w.w;
    return v;
}
// Rip out a reference to the atom holding the raw vect,
// discard/free the rest of the enclosing string object.
Atom* refstr(Atom* p) {
    Atom* a = pulln(p);
    Atom* b = ref(getstr(a));
    del(a);
    return b;
}
void scanfunc(Atom* a, Atom* p, Atom* g) {
    pull(p);
    Atom* v = varscan(p->t, refstr(p));
    if (v) {push(p, v);}
    // Need some kind of error handling system for this
}
// 3 possible cases:
// 1. implicit scan with variable name (varscan)
// `val :name name`
// Case: run directly when a token can't be found.
//
// 2. recursive scan (recscan)
// `0 [. val1 [. val2 :name2 ]. :name1 ]. :name1 :name2.`
// Notice only a single dot, :name2 dots :name1 implicitly
// Case: scan notices that the next element is also a string.
// (This means a string object cannot be normally searched for variables)
//
// 3. internal scan (intscan)
// `0 [. val2 :name2 ]. :name2.`
// Enters into the top element and searches down from there.
// Case: else
void scan(Atom* a, Atom* p, Atom* g) {
    pull(p);
    Atom* s = pulln(p);
    Atom* v;
    if (isstr(p->t)) {
        dot(g);
        while (run(g));
        p = P(g)->t;
        v = varscan(p->t->t, s);
        pull(p);
    }
    else {v = varscan(p->t->t, s);}
    if (v) {push(p, v);}
}
// Returns the length of the char*
int chlen(char* c) {
    int n = 0;
    while (*c++) {n++;}
    return n;
}
// pushes a vector, consumes length from stack
Atom* pushvect(Atom* a, Atom* p) {
    int maxlen = pullw(p);
    Vect* v = valloclen((maxlen) ? maxlen : 1);
    a = pushnew(new(), 0);
    a->t->f = vect;
    a->t->w.v = v;
    a->w.w = v->len = maxlen;
    return push(p, a);
}
Atom* createmultidot(int i);
// Builds a new string object
Atom* newstr() {
    Atom* s = new();
    pushw(s, 0);
    pushvect(s, s);
    pushfunc(s, scan);
    push(s, createmultidot(1));
    return s;
}
// Changes the string object to the new string
Atom* setstr(Atom* s, char* c, int len) {
    Atom* st = getstr(s);
    st->t->w.v->len = 0;
    st->t->w.v = rawpushv(st->t->w.v, c, len);
    return s;
}
// Creates a string object with a given length
Atom* newstrlen(char* c, int len) {return setstr(newstr(), c, len);}
// Creates a string object and pushes it
Atom* pushstr(Atom* p, char* c) {return push(p, newstrlen(c, chlen(c)+1));}

// Consumes the numeric string and outputs a literal
void strtoint(Atom* a, Atom* p, Atom* g) {
    pull(p);
    Vect* v = getstr(p->t)->t->w.v;
    int n = 0;
    int b = 10;
    char* str = v->v;
    Word i = 0;
    bool neg = false;
    if (str[i] == '-') {neg = true; i++;}
    if (str[i] == '0') {
        if (str[i-1] == 'x') {b = 0x10; i += 2;}
        if (str[i-1] == 'b') {b = 2; i += 2;}
    }
    while (i < v->len-1) {
        n *= b;
        if (str[i] >= 'a' && str[i] <= 'f') {n += str[i]-'a';}
        else {n += str[i]-'0';}
        i++;
    }
    if (neg) {n *= -1;}
    pull(p);
    pushw(p, n);
}
// Returns the length of the char*, accounting for
// any backslashes
// - Both this and charptostr need some work. -
Atom* charptostr(Atom* p, char* c) {
    int i, j;
    Atom* s = newstr();
    Vect* v = getstr(s)->t->w.v;
    char ch;
    for (i = j = 0; c[i+j]; i++) {
        if (c[i+j] == '"') {break;}
        ch = c[i+j];
        if (c[i+j] == '\\') {
            j++;
            ch = c[i+j];
            if (c[i+j] == 'n') {ch = '\n';}
            if (c[i+j] == 't') {ch = '\t';}
        }
        v = vectpushc(v, ch);
    }
    v = vectpushc(v, 0);
    getstr(s)->t->w.v = v;
    push(p, s);
    pushw(p, i+j+2);
    return p;
}

// Searches for a variable with the name 
Atom* find(Atom* a, char* c) {
    if (!a || !a->t) {return 0;}
    Atom* p = ref(new());

    pushnew(p, a->t);
    push(p, newstrlen(c, chlen(c)+1));
    Atom* v = varscan(a->t, refstr(p));
    if (v) {push(p, v);}
    else {return 0;}
    Atom* s = pulln(p);
    del(p);
    return s;
}

// Breaks a string into two separate strings, at
// the consumed integer index.
void splitstrat(Atom* a, Atom* p, Atom* g) {
    pull(p);
    Word i = pullw(p);
    pushstr(p, getstr(p->t)->t->w.v->v+i);
    swap(p);
    getstr(p->t)->t->w.v->len = i;
    getstr(p->t)->t->w.v->v[i] = 0;
}

// Creates an object indicating several dots at once
// ... and .... etc.
Atom* createmultidot(int i) {
    Atom* t = new();
    t->f = exec;
    if (--i) {return push(t, createmultidot(i));}
    t->w.f = dotter;
    return t;
}
void parseone(Atom* g);
void debugger(Atom* g) {}
void tokenizer(Atom* a, Atom* p, Atom* g) {
    pull(p);
    if (debugging) {
        debugger(g);
    }
    parseone(g);
}

Atom* threader(Atom* prev, Atom* g);
Atom* newprog();
void tokenprogch(Atom* g, Atom* p, char* c) {
    pushstr(p->t, c);
    pushfunc(p->t, tokenizer);
    push(p->t, createmultidot(1));
    Atom* e = pushnew(E(g), 0)->t;
    pushnew(e, p->t->t);
    while (run(g));
}

bool tequ(Atom* a, Atom* b) {
    if (a == b) {return true;}
    if (a->f != b->f) {return false;}
    if (!a->t != !b->t || !a->e != !b->e) {return false;}
    if (a->t && !tequ(a->t, b->t)) {return false;}
    if (!a->e && !tequ(a->n, b->n)) {return false;}
    return true;
}
void choice(Atom* a, Atom* p, Atom* g) {
    pull(p);
    Word b = p->t->n->n->w.w;
    if (b) {swap(p);}
    pull(p);
}
void typeequ(Atom* a, Atom* p, Atom* g) {
    pull(p);
    Atom* b = pulln(p);
    a = pulln(p);
    pushw(p, tequ(a->t, b->t));
    del(a); del(b);
}
// void zip(Atom* a, Atom* g) {
//     Atom* p = P(g)->t;
//     pull(p);
//     Atom* x = pulln(p);
//     Atom* y = pulln(p);
    
// }
void destroyer(Atom* a, Atom* p, Atom* g) {
    pull(p);
    Word n = pullw(p);
    while (n-- > 0) {pull(p);}
}
void multiplier(Atom* a, Atom* p, Atom* g) {
    pull(p);
    Word n = pullw(p);
    a = pulln(p);
    while (n-- > 0) {dup(a, p);}
    del(a);
}
void mult(Atom* a, Atom* p, Atom* g) {
    pull(p);
    // Word x = p->t->w.w * p->t->n->w.w;
    // throwr(R(g), p, 2);
    // pushw(p, x);
    p->t->w.w *= pullw(p);
}
void sub(Atom* a, Atom* p, Atom* g) {
    pull(p);
    // Word x = p->t->n->w.w - p->t->w.w;
    // throwr(R(g), p, 2);
    // pushw(p, x);
    p->t->w.w -= pullw(p);
}
void toggledebug(Atom* a, Atom* p, Atom* g) {
    pull(p);
    debugging = !debugging;
}
void println(Atom* a);
void dbprint(Atom* a, Atom* p, Atom* g) {
    pull(p);
    println(p->t);
}

void printstr(Atom* a, Atom* p, Atom* g) {
    pull(p);
    Atom* s = pulln(p);
    puts(getstr(s)->t->w.v->v);
    fflush(stdout);
    del(s);
}
void print(Atom* a, int depth, bool shown, char* spinecolor);
void printspine(bool e, int depth, char* spinecolor);
void printprog(Atom* a, Atom* p, Atom* g) {
    pull(p);
    int depth = pullw(p);
    if (p->t->w.w) {printint(p->t->w.w, 8); puts(" ");}
    a = P(p->t)->t;
    puts(YELLOW);
    if (a) {puts("program:\n"); print(a, depth+1, true, DARKYELLOW);}
    else {puts("empty program");}

    Atom* r = R(p->t)->t;
    if (r && len(r)) {
        puts("\n");
        printspine(0, depth, BLUE);
        puts("residual:\n");
        print(r, depth+1, true, BLUE);
        while (1) {
            puts("\n");
            if (r->e) {break;}
            r = r->n;
        }
    }
    Atom* e = E(p->t)->t;
    if (e && len(e)) {
        puts("\n");
        printspine(0, depth, GREEN);
        puts("exec:\n");
        e = e->t;
        while (e) {
            print(e->t, depth, false, GREEN);
            puts("\n");
            if (e->e) {break;}
            e = e->n;
        }
    }
}
Atom* newprog() {
    Atom* g = new();
    pushnew(g, 0);
    pushnew(g->t, 0);
    pushstr(g, "P");
    pushnew(g, 0); pushstr(g, "E");
    pushnew(g, 0); pushstr(g, "R");
    Atom* quot = pushnew(g, 0)->t;
    pushfunc(quot, printprog);
    push(quot, createmultidot(1));
    pushstr(g, "\"");
    g->e = true;
    return g;
}
void addthread(Atom* a, Atom* p, Atom* g) {
    pull(p);
    Atom* g2 = newprog();
    growexec(p->t->t, g2);
    g2->w.w = p->t->w.w;
    pull(p);
    push(p, g2);
}
void detachthread(Atom* a, Atom* p, Atom* g) {
    pull(p);
    pushnew(G, p->t);
}
void strconcat(Atom* a, Atom* p, Atom* g) {
    pull(p);
    Atom* s = refstr(p);
    Atom* t = getstr(p->t);
    t->t->w.v->len--;
    t->t->w.v = rawpushv(t->t->w.v, s->t->w.v->v, s->t->w.v->len+1);
    del(s);
}
void strpadto(Atom* a, Atom* p, Atom* g) {
    pull(p);
    Word n = pullw(p);
    Atom* s = getstr(p->t);
    s->t->w.v->len--;
    while (s->t->w.v->len < n) {s->t->w.v = vectpushc(s->t->w.v, ' ');}
    s->t->w.v = vectpushc(s->t->w.v, 0);
}
void exitfunc(Atom* a, Atom* p, Atom* g) {
    pull(p);
    tset(E(g), 0);
}

Atom* buildfunc(char* name, Func f) {
    Atom* a = new();
    pushfunc(a, f);
    push(a, createmultidot(1));
    pushnew(a, 0);
    pushstr(a->t, DARKGREEN);
    pushfunc(a->t, printstr);
    push(a->t, createmultidot(1));
    pushstr(a->t, name);
    pushfunc(a->t, printstr);
    push(a->t, createmultidot(1));
    pushstr(a, "\"");
    pushw(a, 2);
    pushfunc(a, destroyer);
    push(a, createmultidot(1));
    return a;
}
Func builtins(char* c) {
    if (equstr(c, ","))      {return destroyer;}
    if (equstr(c, ";"))      {return multiplier;}
    if (equstr(c, "["))      {return openatom;}
    if (equstr(c, "]"))      {return closeatom;}
    if (equstr(c, "?"))      {return choice;}
    if (equstr(c, "#"))      {return typeequ;}
    if (equstr(c, "-"))      {return sub;}
    if (equstr(c, "*"))      {return mult;}
    if (equstr(c, "~>"))     {return enter;}
    if (equstr(c, "<~"))     {return next;}
    if (equstr(c, "->"))     {return consume;}
    if (equstr(c, "<-"))     {return throw;}
    if (equstr(c, "exit"))   {return exitfunc;}
    if (equstr(c, "pad"))    {return strpadto;}
    if (equstr(c, "concat")) {return strconcat;}
    if (equstr(c, "tokens")) {return tokenizer;}
    if (equstr(c, "length")) {return getlen;}
    if (equstr(c, "print"))  {return printstr;}
    if (equstr(c, "step"))   {return stepprog;}
    if (equstr(c, "dbprint")){return dbprint;}
    if (equstr(c, "debug"))  {return toggledebug;}
    if (equstr(c, "thread")) {return addthread;}
    if (equstr(c, "detach")) {return detachthread;}
    return 0;
}
// Parses one token over the program g
void token(Atom* g, char* c) {
    Atom* p = P(g)->t;
    int i = 0;
    for (; c[i] == '.'; i++);
    if (i == 1) {dot(g); return;}
    if (i > 1) {push(p, createmultidot(i-1)); return;}
    if (c[0] == ':') {
        if (c[1]) {pushstr(p, c+1);}
        else {pushfunc(p, scanfunc);}
        return;
    }
    if (contains(c[0], "0123456789")) {
        pushstr(p, c);
        runfunc(g, strtoint);
        return;
    }
    if (equstr(c, "\"")) {pull(charptostr(p, c+1)); return;}
    
    Func f = builtins(c);
    if (f) {
        push(p, buildfunc(c, f));
        return;
    }
    push(p, newstrlen(c, chlen(c)+1));
    runfunc(g, scanfunc);
}
void addtok(int i, Atom* g) {
    Atom* p = P(g)->t;
    pushw(p, i);
    runfunc(g, splitstrat);
    Atom* s = pulln(p);
    Vect* v = getstr(s)->t->w.v;
    pull(p);
    token(g, v->v);
    push(P(g)->t, s->n);
    del(s);
}
void discardtok(int i, Atom* g) {
    Atom* p = P(g)->t;
    pushw(p, i);
    runfunc(g, splitstrat);
    pull(p);
}
bool splittok(Atom* g) {
    Atom* p = P(g)->t;
    Vect* str = getstr(p->t)->t->w.v;
    int i = 0;
    while (iswhitespace(str->v[i])) {i++;}
    if (i) {
        discardtok(i, g);
        str = getstr(p->t)->t->w.v;
    }
    bool b = str->v[0] == '"';
    if (b) {charptostr(p, str->v+1);}
    return b;
}
// Parses repeated tokens.
void parseone(Atom* g) {
    Atom* p = P(g)->t;
    bool b = splittok(g);
    int i = 0;
    if (b) {
        i = pullw(p);
        swap(p);
        discardtok(i, g);
        pushfunc(p, tokenizer);
        push(p, createmultidot(1));
        pushnew(pushnew(E(g), 0)->t, p->t);
        return;
    }
    Vect* str = getstr(p->t)->t->w.v;
    for (i = 0; str->v[i] == '.'; i++);
    if (!i) {
        while (!contains(str->v[i], " \n\t\b\r.")) {
            if (i+1 == str->len) {
                addtok(i, g);
                pull(P(g)->t);
                return;
            }
            i++;
        }
    }
    Atom* e = pushnew(E(g), 0)->t;
    addtok(i, g);
    if (E(g)->t == 0) {
        pull(P(g));
        return;
    }
    pushnew(e, createmultidot(1));
    pushnew(e, new());
    e->t->t->w.f = tokenizer;
    e->t->t->f = func;
    pushnew(e, pulln(P(g)->t));
    del(e->t->t);
}

// print helpers
void printvect(Vect* v) {
    for (int i = 0; v->v[i] && i < v->len; i++) {putchar(v->v[i]);}
}
void printspine(bool e, int depth, char* spinecolor) {
    // puts(BLACK);
    for (int i = 0; i < depth; i++) {puts("  ");}
    puts(spinecolor);
    if (e) {puts("┗ ");}
    else {puts("┣ ");}
}

void print(Atom* a, int depth, bool shown, char* spinecolor) {
    printspine(a->e, depth, spinecolor);
    if (!a) {puts("None\n"); return;}
    // Atom* s = (debugging) ? 0 : chscan(a->t, "\"", false);
    Atom* s = chscan(a->t, "\"", false);
    bool showt = false || a->t;
    if (s) {
        // printer functionality
        Atom* p = tset(ref(new()), a);
        pushw(p, depth);
        pushnew(p, s->t);
        Atom* g = ref(newprog());
        tset(P(g), p);
        dot(g);
        while (run(g));
        del(g);
        del(p);
        showt = false;
    }
    else {
        // printint((Word) a, 8);
        // putchar(' ');
        if (isstr(a)) {
            puts(DARKGREEN); printvect(getstr(a)->t->w.v); showt = false;
        }
        else {
            if (a->e == 2) {}
            else if (a->f == word) {puts(DARKRED); printint(a->w.w, 4);}
            else if (a->f == func) {puts(GREEN); puts("func");}
            else if (a->f == exec) {puts(DARKRED); puts(".");}
            else if (a->f == vect) {puts(RED); printvect(a->w.v);}
            else if (a->f == atom) {puts(DARKYELLOW);}
            if (a->r != 1) {puts(RED); puts(" x"); printint(a->r, 4);}
        }
        puts(RESET);
        #ifndef INTERACTIVE
        if (a->e) {
            puts(DARKGREEN); puts(" e");
            if (!a->n) {putchar('0');}
            // else {
            //     printint((Word) a->n, 8);
            //     putchar(' ');
            // }
        }
        #endif
    }
    if (!shown) {return;}
    if (showt) {puts("\n"); print(a->t, depth+1, shown, spinecolor);}
    if (depth && !a->e) {puts("\n"); print(a->n, depth, shown, spinecolor);}
}
void println(Atom* a) {
    if (a->e == 2) {return;}
    puts(DARKYELLOW);
    print(a, 1, true, DARKYELLOW);
    putchar('\n');
}

Atom* loadprog(Atom* a, char* c) {
    Atom* g = pushnew(a, newprog())->t->t;
    Atom* p = P(g);
    pushstr(p->t, c);
    pushfunc(p->t, tokenizer);
    push(p->t, createmultidot(1));
    Atom* e = pushnew(E(g), 0)->t;
    pushnew(e, p->t->t);
    return g;
}

// Removes the atom after a in p
void removeat(Atom* p, Atom* a) {
    bool e = a->n->e;
    if (!e) {nset(a, a->n->n);}
    else {
        Atom* n = a->n;
        a->n = a->n->n;
        del(n);
    }
    a->e = e;
}
Atom* threader(Atom* prev, Atom* g) {
    if (g->t->t->w.w == 0 && !run(g->t->t)) {
        print(g->t->t, 0, true, DARKYELLOW); puts("\n");
        if (g->t == G->t) {pull(G);}
        else {removeat(G, prev);}
    }
    else {prev = g->t;}
    if (prev->e) {tset(g, G->t);}
    else {tset(g, prev->n);}
    return prev;
}
void runprog(char* c) {
    loadprog(G, c);
    Atom* g = ref(new());
    tset(g, G->t);
    Atom* prev = g->t;
    while (G->t->e != 2) {prev = threader(prev, g);}
    del(g);
}

int main() {
    #ifndef INTERACTIVE
    FILE* FP = fopen("challenge", "r");
    fseeko(FP, 0, SEEK_END);
    int i = ftell(FP);
    char program[i+1];
    char* program_ = program;
    rewind(FP);
    while (!feof(FP)) {*program_++ = fgetc(FP);}
    *--program_ = 0;
    fclose(FP);

    G = ref(new());
    runprog(program);
    del(G);

    #else
    G = ref(new());
    Atom* interactive = pushnew(G, newprog())->t->t;
    interactive->w.w = 1;
    Atom* g = ref(new());
    tset(g, G->t);
    Atom* prev = g->t;
    char buff[0x100];
    for (int i = 0; i < 0x100; i++) {buff[i] = 0;}

    Atom* p = P(interactive);
    while (G->t->e != 2 && buff[0] != '\n') {
        pushstr(p->t, buff);
        pushfunc(p->t, tokenizer);
        push(p->t, createmultidot(1));
        Atom* e = pushnew(E(interactive), 0)->t;
        pushnew(e, p->t->t);
        while (run(interactive));
        if (p->t->e == 2) {break;}
        println(p->t->t);
        puts(DARKBLUE); puts("🡪🡪 ");
        puts(BLUE);
        fgets(buff, 0x100, stdin);
        puts(RESET);

        prev = threader(prev, g);
    }
    del(g);
    del(G);
    #endif
}