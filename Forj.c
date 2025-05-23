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
enum form {words, funcs, vects, atoms, dots};

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
bool reversible = false;

// Creates a new, zero-initialized atom with no references.
Atom* new() {
    Atom* a = malloc(sizeof(Atom));
    *a = (Atom) {0, 0, 0, 0, 0, true};
    return a;
}
Atom* get(Atom* a, int i);
Atom* tail(Atom* a) {
    while (!a->e) {a = a->n;}
    return a;
}

// Deletes a reference to an atom.
// If the atom reaches zero references, free its memory.
// Also frees vects:
//  - Vects don't have refcounts, so must be referenced by
//    exactly one atom at all times.
Atom* del(Atom* a) {
    if (!a) {return 0;}
    if (--a->r) {return a;}

    if (a->f == vects) {freevect(a->w.v);}
    if (a->e == false) {del(a->n);}
    if (del(a->t)) {
        // If a->t was referenced by something else,
        // and a owns it, the parent points must be corrected.
        Atom* n = tail(a->t);
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
Atom* pushnew(Atom* p, Atom* t) {return push(p, tset(new(), t))->t;}

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
void pullx(Atom* p) {
    int i = pullw(p);
    Atom* a = p->t;
    while (a && !a->e && i) {a = a->n; i--;}
    if (i) {pushend(p, a->n);}
    else {tset(p, a);}
}

// Duplicates a onto p
Atom* dup(Atom* a, Atom* p) {
    pushnew(p, a->t);
    p->t->f = a->f;
    if (p->t->f != vects) {p->t->w = a->w;}
    else {
        p->t->w.v = dupvect(a->w.v);
    }
    return p;
}
void dot(Atom* g);
Atom* chscan(Atom* a, char* c, bool full);
Atom* P(Atom* g) {return chscan(g->t, "P", true);}
Atom* E(Atom* g) {return chscan(g->t, "E", true);}
Atom* R(Atom* g) {return chscan(g->t, "R", true);}

Atom* open(Atom* p) {
    Atom* pt = pushnew(p, 0);
    pt->w.a = pt->n->t;
    if ((pt->n->f == words || pt->n->f == atoms) && pt->n->t) {
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
void openr(Atom* r) {pushnew(r, 0);}
// push residual
Atom* throwr(Atom* r, Atom* dest, Atom* src, int consume) {
    Atom* n = get(src->t, consume-1);
    Atom* a = ref(src->t);
    pushnew(r->t, a)->w.w = consume;
    pushnew(r->t, dest);
    if (n->e) {pushend(src, n->n);}
    else {tset(src, n->n);}
    return a;
}


// Prepend a and its tail onto p
Atom* prepend(Atom* a, Atom* p) {
    if (p->t) {
        if (p->t->e < 2) {nset(tail(a), p->t);}
        else {tail(a)->n = p->t->n; tail(a)->e = true;}
    }
    tset(p, a);
    return p;
}
// Executing behavior for a list.
// [ a b c ] .
// ^ From left to right, (bottom to top of stack),
// dup the element onto p.  If it's a multidot (ie: ...),
// Execute it, reducing by one.
// Double dot (..) will run dot() on the top of stack.
void println(Atom* p);
void scan(Atom* a, Atom* p, Atom* g);
Atom* pushfunc(Atom* p, Func f);
void exechandler(Atom* a, Atom* p, Atom* g) {
    if (a->f == dots) {
        if (!a->t) {
            // printf(">>%lld\n", a->n->w.w);
            dot(g);
        }
        else {prepend(a->t, p);}
        // else {dup(a->t, p);}
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
    if (e->t->t->e == 2) {pull(e);}
    exechandler(e2->t, P(g)->t, g);
    del(e2);
    return e->t && e->t->e != 2;
}
void growexec(Atom* a, Atom* g) {
    Atom* e = pushnew(E(g), 0);
    pushnew(e, a);
    while (!a->e) {pushnew(e, a = a->n);}
}

// Safetly checks if the atom holds a string.
// In the future, methods will be written in Forj to 
// compare to templates, making this more elegant.
bool isstr(Atom* s) {
    return s->f == vects;
    // if (!s->t) {return false;}
    // s = get(s->t, 1);
    // return s && s->w.f == scan;
}
// Single dot, or activated double dot:
//      A single dot '.', runs immediately when it is
//      parsed (at parse-time).  Whereas a double dot
//      '..' merely pushes this function.
void dot(Atom* g) {
    Atom* p = P(g)->t;
    // If 'a' holds a function pointer, run it
    if (p->t->f == funcs) {return p->t->w.f(p->t, p, g);}
    if (isstr(p->t)) {
        pushfunc(p, scan);
        return dot(g);
    }
    Atom* a = pulln(p);
    // a->f == exec indicates 'a' is a dot ie: ..
    if (a->f != dots && a->t) {growexec(a->t, g);}
    else {exechandler(a, P(g)->t, g);}
    del(a);
}
void dotter(Atom* a, Atom* p, Atom* g) {
    pull(p);
    dot(g);
}
// Get the ith index next node after a
Atom* get(Atom* a, int i) {
    while (a && !a->e && i--) {a = a->n;}
    return a;
}
Atom* tget(Atom* a, int i) {
    while (a && i--) {a = a->t;}
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

// Convenience function gets the Vect from a string.
Atom* getstr(Atom* s) {return s; get(s->t, 2);}

// Convenience function to push a function pointer
Atom* pushfunc(Atom* p, Func f) {
    pushnew(p, 0);
    p->t->w.f = f;
    p->t->f = funcs;
    return p;
}

// Searches straight down.  If a containing list has a parent,
// It will be searched as well.
// This is used for implicit variable search: `val :name name`
// This is related to the :symbol functionality.
// Since one scan call leaves (a reference to) the associated
// variable on the top, :symbol2 can be called again to get
// a variable from the second, interior layer.
Atom* chscan(Atom* a, char* c, bool full) {
    while (a && (full || !a->e)) {
        if (!a->e && isstr(a) &&
            equstr(getstr(a)->w.v->v, c)) {
            return a->n;
        }
        if (full && !a->e && a->f == dots) {
            Atom* v = chscan(a->n->t, c, false);
            if (v) {return v;}
        }
        a = a->n;
    }
    return 0;
}
Atom* varscan(Atom* a, Atom* s) {
    a = chscan(a, s->w.v->v, true);
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
    return pulln(p);
    Atom* a = pulln(p);
    Atom* b = ref(a);
    del(a);
    return b;
}
void scanfunc(Atom* a, Atom* p, Atom* g) {
    pull(p);
    Atom* v = varscan(p->t, refstr(p));
    if (v) {push(p, v);}
}
// 3 possible cases:
// 1. implicit scan with variable name (varscan)
// `val :name name`
// Case: run when a valid token can't be found.
//
// 2. recursive scan
// `0 [. val1 [. val2 :name2 ]. :name1 ]. :name1 :name2.`
// Notice only a single dot at the end, :name2 dots :name1 implicitly
// Case: scan notices that the next element is also a string.
// (This means a string object cannot be normally searched for variables)
//
// 3. internal scan
// `0 [. val2 :name2 ]. :name2.`
// Enters into the next element and searches down from there.
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
Atom* newvect(int len) {
    int maxlen = len;
    Atom* a = new();
    a->f = vects;
    a->w.v = valloclen((maxlen) ? maxlen : 1);
    return a;
}
// pushes a vector, consumes length from stack
Atom* pushvect(Atom* a, Atom* p, int i) {
    Atom* v = newvect(i);
    pushnew(p, v);
    return p;
}
void pushdot(Atom* p, int i);
// Builds a new string object
Atom* newstr() {
    return newvect(0);
    // Atom* s = new();
    // pushw(s, 0);
    // pushvect(s, s);
    // pushfunc(s, scan);
    // pushdot(s, 1);
    // return s;
}
// Changes the string object to the new string
Atom* setstr(Atom* s, char* c, int len) {
    Atom* st = getstr(s);
    st->w.v->len = 0;
    st->w.v = rawpushv(st->w.v, c, len);
    return s;
}
// Creates a string object with a given length
Atom* newstrlen(char* c, int len) {return setstr(newstr(), c, len);}
// Atom* s(char* c) {return newstrlen(c, chlen(c));}
// Creates a string object and pushes it
Atom* pushstr(Atom* p, char* c) {return push(p, newstrlen(c, chlen(c)+1));}
Atom* str(char* c) {return newstrlen(c, chlen(c));}
Atom* dupstr(Atom* s) {return str(s->w.v->v);}
Atom* substr(Atom* s, int a, int b) {
    Vect* v = s->w.v;
    if (b == -1) {
        if (a) {return s;}
        b = v->len;
    }
    if (a == 0) {
        v->len = b;
        v->v[b] = 0;
    }
    else {
        Atom* s1 = newstrlen(v->v+a, b-a);
        freevect(s->w.v);
        s->w.v = s1->w.v;
        s1->w.v = 0;
    }
    return s;
}
Atom* addstr(Atom* str1, Atom* str2) {
    Vect* v = str1->w.v;
    if (str1->w.v->len) {str1->w.v->len--;} // remove the null char at the end
    str1->w.v = rawpushv(v, str2->w.v->v, str2->w.v->len+1);
    return str1;
}
Atom* padstr(Atom* s, int padto) {
    for (int i = 0; i < padto; i++) {
        s->w.v = vectpushc(s->w.v, ' ');
    }
    return s;
}

Word chtoint(char* s) {
    int n = 0;
    int b = 10;
    Word i = 0;
    bool neg = false;
    if (s[i] == '-') {neg = true; i++;}
    if (s[i] == '0') {
        if (s[i-1] == 'x') {b = 0x10; i += 2;}
        if (s[i-1] == 'b') {b = 2; i += 2;}
    }
    while (i < chlen(s)) {
        n *= b;
        if (s[i] >= 'a' && s[i] <= 'f') {n += s[i]-'a';}
        else {n += s[i]-'0';}
        i++;
    }
    if (neg) {n *= -1;}
    return n;
}
// Consumes the numeric string and outputs a literal
void strtoint(Atom* a, Atom* p, Atom* g) {
    pull(p);
    Word n = chtoint(getstr(p->t)->w.v->v);
    pull(p);
    pushw(p, n);
}
// Returns the length of the char*, accounting for
// any backslashes
// - Both this and charptostr need some work. -
Atom* charptostr(Atom* p, char* c) {
    int i, j;
    Atom* s = newstr();
    Vect* v = getstr(s)->w.v;
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
    getstr(s)->w.v = vectpushc(v, 0);
    push(p, s);
    pushw(p, i+j+2);
    return p;
}

void split(Atom* p, Word i) {
    pushstr(p, getstr(p->t)->w.v->v+i);
    swap(p);
    getstr(p->t)->w.v->len = i;
    getstr(p->t)->w.v->v[i] = 0;
}
// Breaks a string into two separate strings, at
// the consumed integer index.
void splitstrat(Atom* a, Atom* p, Atom* g) {
    pull(p);
    split(p, pullw(p));
}

// Creates an object indicating several dots at once
// ... and .... etc.
Atom* createmultidot(int i) {
    // Atom* t = nset(new(), new());
    Atom* t = new();
    // t->n->w.w = 1;
    t->f = dots;
    while (--i) {
        t = prepend(t, new());
        tail(t->t)->n = t;
        t->f = dots;
    }
    // if (--i) {return push(t, createmultidot(i));}
    // if (i) {
    //     pushw(t, 1);
    //     return push(t, createmultidot(i));
    // }
    // t->w.f = dotter;

    return t;
}
void pushdot(Atom* p, int i) {
    // push(p, createmultidot(i));
    prepend(createmultidot(i), p);
}
// Parse one token from a string on g
void parseone(Atom* p, Atom* g);
void parseonefunc(Atom* a, Atom* p, Atom* g) {
    pull(p);
    parseone(p, g);
}

// Handles thread switching
Atom* threader(Atom* prev, Atom* g);
// Creates a new program object
Atom* newprog();
// Run the text in c as a program in g
void tokench(Atom* g, Atom* p, char* c) {
    pushstr(p->t, c);
    pushfunc(p->t, parseonefunc);
    pushdot(p->t, 1);
    Atom* e = pushnew(E(g), 0);
    pushnew(e, p->t->t);
    while (run(g));
}

// Unused:
// Checks if a and b have matching topological tree shapes
bool typeequ(Atom* a, Atom* b) {
    if (a == b) {return true;}
    if (a->f != b->f) {return false;}
    if (!a->t != !b->t || !a->e != !b->e) {return false;}
    if (a->t && !typeequ(a->t, b->t)) {return false;}
    if (!a->e && !typeequ(a->n, b->n)) {return false;}
    return true;
}
// Ternery operator uses the top element to
// decide which of the next two to discard.
void choice(Atom* a, Atom* p, Atom* g) {
    pull(p);
    Word b = p->t->n->n->w.w;
    if (b) {swap(p);}
    pull(p);
}
void typeequfunq(Atom* a, Atom* p, Atom* g) {
    pull(p);
    Atom* b = pulln(p);
    a = pulln(p);
    pushw(p, typeequ(a->t, b->t));
    del(a); del(b);
}
// Zip two lists together, element-wise
// void zip(Atom* a, Atom* g) {
//     Atom* p = P(g)->t;
//     pull(p);
//     Atom* x = pulln(p);
//     Atom* y = pulln(p);
    
// }
// Removes n elements from the stack.
void destroyer(Atom* a, Atom* p, Atom* g) {
    pull(p);
    pullx(p);
}
// Duplicates the top element n times.
void multiplier(Atom* a, Atom* p, Atom* g) {
    pull(p);
    Word n = pullw(p);
    a = pulln(p);
    while (n-- > 0) {dup(a, p);}
    del(a);
}
// Arithmetical Multiplication
void mult(Atom* a, Atom* p, Atom* g) {
    // Atom* r = R(g);
    Word x = p->t->n->n->w.w * p->t->n->w.w;
    pull(p);
    pull(p);
    pull(p);
    // push(p, createmultidot(1));
    // del(throwr(r, E(g), p, 2));
    // del(throwr(r, P(g), p, 2));
    pushw(p, x);
}
// Arithmetical Subtraction
void sub(Atom* a, Atom* p, Atom* g) {
    // Atom* r = R(g);
    Word x = p->t->n->n->w.w - p->t->n->w.w;
    pull(p);
    pull(p);
    pull(p);
    // del(throwr(r, E(g)->t, p, 1));
    // del(throwr(r, p, p, 3));
    pushw(p, x);
}
void breakpoint(Atom* a, Atom* p, Atom* g) {
    pull(p);
}
// Node printing utility
void println(Atom* a);
void printlnfunc(Atom* a, Atom* p, Atom* g) {
    pull(p);
    println(p->t);
}

// Puts the top of the stack on stdout
void printstr(Atom* a, Atom* p, Atom* g) {
    pull(p);
    Atom* s = pulln(p);
    puts(getstr(s)->w.v->v);
    fflush(stdout);
    del(s);
}
// Raw print function
void print(Atom* a, int depth, bool shown, char* spinecolor);
// Utility to print the decorative lines in a given color.
void printspine(bool e, int depth, char* spinecolor);
// Printer function for a program.
void printprog(Atom* a, Atom* p, Atom* g) {
    pull(p);
    int depth = pullw(p);
    if (p->t->w.w) {printint(p->t->w.w, 8); puts(" ");}
    a = P(p->t)->t;
    puts(YELLOW);
    if (a) {puts("program:\n"); print(a, depth+1, true, DARKYELLOW);}
    else {puts("empty program");}

    Atom* r = R(p->t)->t;
    // if (r && len(r)) {
    //     puts("\n");
    //     printspine(0, depth+1, BLUE);
    //     puts("residual:\n");
    //     r = r->t;
    //     while (r) {
    //         print(r->t, depth+1, false, BLUE);
    //         puts("\n");
    //         if (r->e) {break;}
    //         r = r->n;
    //     }
    // }
    Atom* e = E(p->t)->t;
    if (e && len(e)) {
        puts("\n");
        printspine(0, depth+1, GREEN);
        puts("exec:\n");
        e = e->t;
        while (e) {
            print(e->t, depth+1, false, GREEN);
            puts("\n");
            if (e->e) {break;}
            e = e->n;
        }
    }
}
// Wrap a function pointer in a nice Forj object
void buildfunc(Atom* p, Func f, char* name) {
    Atom* s = pushnew(p, 0);
    pushnew(s, 0);
    pushstr(s->t, RESET);
    pushfunc(s->t, printstr);
    pushdot(s->t, 1);
    pushstr(s->t, GREEN);
    pushfunc(s->t, printstr);
    pushdot(s->t, 1);
    pushstr(s->t, name);
    pushfunc(s->t, printstr);
    pushdot(s->t, 1);
    pushstr(s, "\"");
    pushw(s, len(s->t));
    pushfunc(s, destroyer);
    pushdot(s, 1);
    pushfunc(s, f);
    pushdot(s, 1);
    pushstr(p, name);
}
// Creates all the built-in functions.
Func builtins(Atom* p);
Atom* newprog() {
    Atom* g = new();
    builtins(g);
    pushnew(g, 0); pushnew(g->t, 0); pushstr(g, "P");
    pushnew(g, 0); pushstr(g, "E");
    pushnew(g, 0); pushnew(g->t, 0); pushstr(g, "R");
    pushnew(g, 0); pushstr(g, "dependents");
    pushnew(g, 0);
    pushfunc(g->t, printprog);
    pushdot(g->t, 1);
    pushstr(g, "\"");
    g->e = true;
    return g;
}

// Turns a list into a thread.
// Does not start execution.
void addthread(Atom* a, Atom* p, Atom* g) {
    pull(p);
    Atom* g2 = newprog();
    growexec(p->t->t, g2);
    g2->w.w = p->t->w.w;
    pull(p);
    push(p, g2);
}
// Detach the thread, adding it to the execution
// queue, it will now start running.
// This does not pop it from the stack.
void detachthread(Atom* a, Atom* p, Atom* g) {
    pull(p);
    pushnew(G, p->t);
}

void await(Atom* a, Atom* p, Atom* g) {
    pull(p);
    if (p->t->w.w) {return;}
    p = chscan(p->t->t, "dependents", true);
    pushnew(p, g);
    g->w.w = 1;
}

// Step the thread at the top of the stack backwards.
void undoone(Atom* r, Atom* g) {
    if (r->t->w.w == 1) {
        Word consume = r->t->n->w.w;
        Atom* a = get(r->t->n->t, consume-1);
        while (consume--) {
            del(pushnew(r->t->t, pulln(r->t->n)));
        }
        // push(r->t->t, a);
        // nset(a, 0);
        // a->n = r->t->t->t->n;
    }
    else tset(r->t->t->t, r->t->n->t);
    pull(r);
    pull(r);
}
void undo(Atom* g) {
    // Atom* r = R(g);
    // while (r->t->t->e != 2) {
    //     undoone(r->t, g);
    // }
    // pull(r);
}
void undofunc(Atom* a, Atom* p, Atom* g) {
    pull(p);
    undo(p->t);
}
// Concatenate strings
void strconcat(Atom* a, Atom* p, Atom* g) {
    pull(p);
    Atom* s = refstr(p);
    Atom* t = getstr(p->t);
    t->w.v->len--;
    t->w.v = rawpushv(t->w.v, s->w.v->v, s->w.v->len+1);
    del(s);
}
// Pad the string to a certain length
void strpadto(Atom* a, Atom* p, Atom* g) {
    pull(p);
    Word n = pullw(p);
    Atom* s = getstr(p->t);
    s->w.v->len--;
    while (s->w.v->len < n) {s->w.v = vectpushc(s->w.v, ' ');}
    s->w.v = vectpushc(s->w.v, 0);
}
// Close the current thread.
void exitfunc(Atom* a, Atom* p, Atom* g) {
    pull(p);
    tset(E(g), 0);
}

Func builtins(Atom* p) {
    // Atom* p, Func f, char* name, int consume, int produce
    buildfunc(p, destroyer,      ",");
    buildfunc(p, multiplier,     ";");
    buildfunc(p, openatom,       "[");
    buildfunc(p, closeatom,      "]");
    buildfunc(p, choice,         "?");
    buildfunc(p, typeequfunq,    "#");
    buildfunc(p, sub,            "-");
    buildfunc(p, mult,           "*");
    buildfunc(p, enter,          "~>");
    buildfunc(p, next,           "<~");
    buildfunc(p, consume,        "->");
    buildfunc(p, throw,          "<-");
    buildfunc(p, exitfunc,       "exit");
    buildfunc(p, strpadto,       "pad");
    buildfunc(p, strconcat,      "concat");
    buildfunc(p, parseonefunc,   "tokens");
    buildfunc(p, getlen,         "length");
    buildfunc(p, printstr,       "print");
    buildfunc(p, stepprog,       "step");
    buildfunc(p, printlnfunc,    "printnode");
    buildfunc(p, breakpoint,     "breakpoint");
    buildfunc(p, addthread,      "thread");
    buildfunc(p, detachthread,   "detach");
    buildfunc(p, undofunc,       "undo");
    buildfunc(p, await,          "await");
    return 0;
}

// The string at the top of the program.
void token(Atom* g, char* c) {
    Atom* p = P(g)->t;
    int i = 0;
    for (; c[i] == '.'; i++);
    if (i == 1) {dot(g); return;}
    if (i > 1) {pushdot(p, i-1); return;}
    if (c[0] == ':') {
        if (c[1]) {pushstr(p, c+1);}
        else {pushfunc(p, scanfunc);}
        return;
    }
    if (contains(c[0], "0123456789")) {pushw(p, chtoint(c)); return;}
    if (equstr(c, "[]")) {pushnew(p, 0); return;}
    if (equstr(c, "\"")) {pull(charptostr(p, c+1)); return;}
    push(p, newstrlen(c, chlen(c)+1));
    Atom* v = varscan(p->t, refstr(p));
    if (v) {push(p, v);}
}
// Finish parsing and adding a token to the program, calls token().
void addtok(int i, Atom* g) {
    Atom* p = P(g)->t;
    split(p, i);
    Atom* s = pulln(p);
    Vect* v = getstr(s)->w.v;
    pull(p);
    token(g, v->v);
    push(P(g)->t, s->n);
    del(s);
}
// Discards the first i characters of the string at the top of p
void discardtok(int i, Atom* p) {
    split(p, i);
    pull(p);
}
// Breaks out the first token from the string.
// Returns true if it split on a quote: ".
bool capturetoken(Atom* p) {
    Vect* s = getstr(p->t)->w.v;
    int i = 0;
    while (iswhitespace(s->v[i])) {i++;}
    if (i) {
        discardtok(i, p);
        s = getstr(p->t)->w.v;
    }
    bool b = s->v[0] == '"';
    if (b) {charptostr(p, s->v+1);}
    return b;
}
// Parse one token.
void parseone(Atom* p, Atom* g) {
    bool b = capturetoken(p);
    if (b) {
        int i = pullw(p);
        swap(p);
        discardtok(i, p);
        pushfunc(p, parseonefunc);
        pushdot(p, 1);
        pushnew(pushnew(E(g), 0), p->t);
        return;
    }
    Vect* s = getstr(p->t)->w.v;
    int i = 0;
    for (; s->v[i] == '.'; i++);
    if (!i) {
        while (!contains(s->v[i], " \n\t\b\r.")) {
            if (i+1 == s->len) {
                addtok(i, g);
                pull(P(g)->t);
                return;
            }
            i++;
        }
    }
    Atom* e = pushnew(E(g), 0);
    addtok(i, g);
    if (E(g)->t == 0) {pull(P(g)); return;}
    pushnew(e, createmultidot(1));
    pushnew(e, new());
    e->t->t->w.f = parseonefunc;
    e->t->t->f = funcs;
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
    if (!a) {puts("None\n"); return;}
    printspine(a->e, depth, spinecolor);
    Atom* s = (debugging) ? 0 : chscan(a->t, "\"", false);
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
            puts(DARKGREEN); printvect(getstr(a)->w.v); showt = false;
        }
        else {
            if (a->e == 2) {}
            else if (a->f == words) {puts(DARKRED); printint(a->w.w, 4);}
            else if (a->f == funcs) {puts(RESET); puts(GREEN); puts("func");}
            else if (a->f == dots) {puts(DARKRED); puts(".");}
            else if (a->f == atoms) {puts(DARKYELLOW);}
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
    if (!a) {puts(DARKYELLOW); puts("None"); return;}
    if (a->e == 2) {return;}
    puts(DARKYELLOW);
    print(a, 1, true, DARKYELLOW);
    putchar('\n');
}

// Returns a program object with the ch* program loaded into it.
Atom* loadprog(Atom* a, char* c) {
    Atom* g = pushnew(a, newprog())->t;
    Atom* p = P(g);
    pushstr(p->t, c);
    pushfunc(p->t, parseonefunc);
    pushdot(p->t, 1);
    Atom* e = pushnew(E(g), 0);
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
bool threadrun(Atom* g) {
    return run(g);
}
Atom* threader(Atom* prev, Atom* g) {
    if (g->t->w.w == 0 && !threadrun(g->t)) {
        g->t->w.w = 1;
        Atom* w = chscan(g->t->t, "dependents", true);
        while (w && w->t && w->t->e != 2) {
            w->t->t->w.w = 0;
            pull(w);
        }
        print(g->t, 0, true, DARKYELLOW); puts("\n");
        if (g == G->t) {pull(G);}
        else {removeat(G, prev);}
    }
    else {prev = g;}
    return prev;
}
int main() {
    G = ref(new());
    Atom* g = ref(new());

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

    loadprog(G, program);
    tset(g, G->t);
    Atom* prev = g->t;
    while (G->t->e != 2) {
        prev = threader(prev, g->t);
        if (prev->e) {tset(g, G->t);}
        else {tset(g, prev->n);}
    }

    #else
    Atom* interactive = pushnew(G, newprog())->t;
    interactive->w.w = 1;
    tset(g, G->t);
    Atom* prev = g->t;
    char buff[0x100];
    for (int i = 0; i < 0x100; i++) {buff[i] = 0;}

    Atom* p = P(interactive);
    while (G->t->e != 2) {
        if (buff[0] != '\n') {
            pushstr(p->t, buff);
            pushfunc(p->t, parseonefunc);
            pushdot(p->t, 1);
            Atom* e = pushnew(E(interactive), 0);
            pushnew(e, p->t->t);
        }
        run(interactive);
        if (p->t->e == 2) {break;}
        println(g->t);
        puts(DARKBLUE); puts("🡪🡪 ");
        puts(BLUE);
        fgets(buff, 0x100, stdin);
        puts(RESET);

        prev = threader(prev, g->t);
        if (prev->e) {tset(g, G->t);}
        else {tset(g, prev->n);}
    }
    #endif
    del(g);
    del(G);
}