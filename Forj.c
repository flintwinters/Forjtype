#include "Vect.c"
#include <stdio.h>

typedef long long Word;
typedef struct Atom Atom;
typedef union data data;
typedef enum form form;

// A program thread has three components:
// 1.   "P" - data pointer stack
//      This stack tracks the working data branch
//      a new value is pushed to this stack when
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
typedef Atom Prog;
typedef void (*Func)(Atom*, Prog*);

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
        Atom* n = get(a->t, -1);
        if (n->n == a) {n->n = 0;}
    }
    reclaim(a, sizeof(struct Atom));
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
Atom* tset(Atom* a, Atom* t) {ref(t); del(a->t); a->t = t; return a;}
// Sets p->t to a and appends a to the old p->t
// e == 2 is used only in tandem for pull: it signifies the
// empty list, while still pointing to its owner.
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
        // if (p->n) {a->n = p->n->t;}
        // else {a->n = p;}
        a->n = p;
        a->e = true;
    }
    return tset(p, a);
}
// Init a new atom with type t, and push it to p.
Atom* pushnew(Atom* p, Atom* t) {return push(p, tset(new(), t));}
// Unique push function which should only be used on
// an empty p.  Creates a new signifier atom, that
// marks an empty p.  Note that t is not ref'd.
Atom* pushe(Atom* p, Atom* t) {
    Atom* e = new();
    e->n = t;
    e->e = 2;
    tset(p, e);
    return p;
}
// Pops an element from the top of p.  If it's the last element,
// replace it with an e == 2 atom.
void pull(Atom* p) {
    if (p->t->e) {pushe(p, p->t->n);}
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
void dot(Prog* g);
Atom* chscan(Atom* a, char* c);
Prog* prog();
Atom* P(Prog* g) {return chscan(g->t, "P");}
Atom* E(Prog* g) {return chscan(g->t, "E");}
Atom* R(Prog* g) {return chscan(g->t, "R");}
// Executing behavior for a list.
// [ a b c ] .
// ^ From left to right, (bottom to top of stack),
// dup the element onto p.  If it's a multidot (ie: ...),
// Execute it, reducing by one.
// Double dot (..) will run dot() on the top of stack.
void recursearr(Atom* a, Prog* g) {
    if (!ref(a)->e) {recursearr(a->n, g);}
    if (a->f == exec) {
        if (a->t) {dup(a->t, P(g)->t);}
        else {dot(g);}
    }
    else {dup(a, P(g)->t);}
    del(a);
}
void debugarr(Atom* a, Prog* g) {
    // find(a, "\"");
}
void array(Atom* a, Prog* g) {
    // Recursively descend
    // We execute bottom up so recursion is not tail.
    // if we are in debug mode, build the program tree,
    // else choose the more efficient, C-driven tracker
    if (R(g)->t) {debugarr(a, g);}
    else {recursearr(a, g);}
}
// Single dot, or activated double dot:
//      A single dot '.', runs immediately when it is
//      parsed (at parse-time).  Whereas a double dot
//      '..' merely pushes this function.
void dot(Prog* g) {
    Atom* p = P(g)->t;
    Atom* a = p->t;
    // If 'a' holds a function pointer, run it
    if (a->f == func) {return a->w.f(a, g);}
    a = pulln(p);
    // a->f == exec indicates 'a' is a dot ie: ..
    if (a->f == exec) {
        if (a->t) {dup(a->t, p);}
        else {dot(g);}
    }
    else if (a->t) {array(a->t, g);}
    else {dup(a, p);}
    del(a);
}
void dotter(Atom* a, Prog* g) {dot(g);}
// Get the ith index next node after a
Atom* get(Atom* a, int i) {
    if (!a) {return 0;}
    if (i == 0) {return a;}
    if (!a->e) {
        if (i >= 0) {return get(a->n, i-1);}
        return get(a->n, -1);
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
void open(Atom* a, Prog* g) {
    pull(P(g)->t);
    Atom* p = pushnew(P(g), 0)->t;
    p->w.a = p->n->t;
    if (p->n->t) {
        if (p->n->t->t) {tset(p, p->n->t->t);}
        else {
            // pushe(p->n->t, p->n->t);
            pushe(p, p->n->t);
        }
    }
    // else {pushe(p, p->t);}
}
// Close the current atom ]
void close(Atom* a, Prog* g) {
    pull(P(g)->t);
    Atom* p = P(g)->t;
    if (p->w.a) {
        if (p->t->e == 2) {tset(p->w.a, 0);}
        else {tset(p->w.a, p->t);}
    }
    pull(P(g));
}
// Func wrapping of len
void getlen(Atom* a, Prog* g) {
    Atom* p = P(g)->t;
    pull(p);
    pushw(p, len(p->t->t));
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
void next(Atom* a, Prog* g) {
    Atom* p = P(g)->t;
    pull(p);
    if (p->t->t) {tset(p->t, p->t->t->n);}
    else {tset(p->t, p->t->n);}
}
// ~> inbuilt
void enter(Atom* a, Prog* g) {
    Atom* p = P(g)->t;
    pull(p);
    if (p->t->t) {tset(p->t, p->t->t->t);}
    else {tset(p->t, p->t->n->t);}
}
// Wrap pull in 'Func' type compatible function
void consume(Atom* a, Prog* g) {
    Atom* p = P(g)->t;
    pull(p);
    if (p->n) {dup(p->w.a->n, p);}
}
void throw(Atom* a, Prog* g) {
    Atom* p = P(g)->t;
    pull(p);
    if (p->n) {
        dup(p->t, p->n);
        pull(p);
    }
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
void token(Prog* g, char* c);
// Returns c if c is in s, else 0
char contains(char c, char* s) {while (*s != c) {if (!*s) {return 0;} s++;} return c;}
char iswhitespace(char c) {return contains(c, " \n\t\b\r");}
// Returns true if a == b
bool equstr(char* a, char* b) {
    while (*a == *b && *a && *b) {a++; b++;}
    if (*a || *b) {return false;}
    return true;
}
void scan(Atom* a, Prog* g);
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
    token(g, ".");
}
// Searches straight down.  If a list has a parent,
// It will be searched as well.
// This is used for implicit variable search: `val :name name`
// This is related to the :symbol functionality.
// Since one scan call leaves (a reference to the) the associated
// variable on the top, :symbol2 can be called again to get
// a variable from the second, interior layer.
Atom* chscan(Atom* a, char* c) {
    Atom* v = 0;
    while (a) {
        if (!a->e && isstr(a) &&
            equstr(getstr(a)->t->w.v->v, c)
            ) {
            return a->n;
        }
        a = a->n;
    }
    return 0;
}
Atom* varscan(Atom* a, Atom* s) {
    a = chscan(a, s->t->w.v->v);
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
void scanfunc(Atom* a, Prog* g) {
    Atom* p = P(g)->t;
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
void scan(Atom* a, Prog* g) {
    Atom* p = P(g)->t;
    pull(p);
    Atom* s = pulln(p);
    Atom* v;
    if (isstr(p->t)) {
        token(g, ".");
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
// Pushes a vector, consumes length from stack
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
    getstr(s)->t->w.v->len = 0;
    getstr(s)->t->w.v = rawpushv(getstr(s)->t->w.v, c, len);
    return s;
}
// Creates a string object with a given length
Atom* newstrlen(char* c, int len) {return setstr(newstr(), c, len);}
// Creates a string object and pushes it
Atom* pushstr(Atom* p, char* c) {return push(p, newstrlen(c, chlen(c)+1));}

Prog* prog() {
    Prog* g = new();
    pushnew(g, 0); pushstr(g, "R");
    pushnew(g, 0); pushstr(g, "E");
    pushnew(g, 0); pushstr(g, "P");
    pushnew(g, 0); pushstr(g, "\"");
    g->e = true;
    return g;
}

// Consumes the numeric string and outputs a literal
void strtoint(Atom* a, Prog* g) {
    Atom* p = P(g)->t;
    pull(p);
    Vect* v = getstr(p->t)->t->w.v;
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
void splitstrat(Atom* a, Prog* g) {
    Atom* p = P(g)->t;
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
void tokenizer(Atom* a, Prog* g) {
    pull(P(g)->t);
    parseone(g);
}
bool tequ(Atom* a, Atom* b) {
    if (a == b) {return true;}
    if (a->f != b->f) {return false;}
    if (!a->t != !b->t || !a->e != !b->e) {return false;}
    if (a->t && !tequ(a->t, b->t)) {return false;}
    if (!a->e && !tequ(a->n, b->n)) {return false;}
    return true;
}
void choice(Atom* a, Prog* g) {
    Atom* p = P(g)->t;
    pull(p);
    Word b = pullw(p);
    if (b) {swap(p);}
    pull(p);
}
void typeequ(Atom* a, Prog* g) {
    Atom* p = P(g)->t;
    pull(p);
    Atom* b = pulln(p);
    a = pulln(p);
    pushw(p, tequ(a->t, b->t));
    del(a); del(b);
}
// void zip(Atom* a, Prog* g) {
//     Atom* p = P(g)->t;
//     pull(p);
//     Atom* x = pulln(p);
//     Atom* y = pulln(p);
    
// }
void multiplier(Atom* a, Prog* g) {
    Atom* p = P(g)->t;
    pull(p);
    Word n = pullw(p);
    a = pulln(p);
    while (n-- > 0) {dup(a, p);}
    del(a);
}
void mult(Atom* a, Prog* g) {
    Atom* p = P(g)->t;
    pull(p);
    Word x = pullw(p);
    Word y = pullw(p);
    pushw(p, y*x);
}
void sub(Atom* a, Prog* g) {
    Atom* p = P(g)->t;
    pull(p);
    Word x = pullw(p);
    Word y = pullw(p);
    pushw(p, y-x);
}

void printstr(Atom* a, Prog* g) {
    Atom* p = P(g)->t;
    pull(p);
    Atom* s = pulln(p);
    puts(getstr(s)->t->w.v->v);
    del(s);
}
Func builtins(char* c) {
    if (equstr(c, ","))      {return multiplier;}
    if (equstr(c, "["))      {return open;}
    if (equstr(c, "]"))      {return close;}
    if (equstr(c, "?"))      {return choice;}
    if (equstr(c, "#"))      {return typeequ;}
    if (equstr(c, "-"))      {return sub;}
    if (equstr(c, "*"))      {return mult;}
    if (equstr(c, "~>"))     {return enter;}
    if (equstr(c, "<~"))     {return next;}
    if (equstr(c, "->"))     {return consume;}
    if (equstr(c, "<-"))     {return throw;}
    if (equstr(c, "token"))  {return tokenizer;}
    if (equstr(c, "length")) {return getlen;}
    if (equstr(c, "print"))  {return printstr;}
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
    if (f) {pushfunc(p, f); return;}
    push(p, newstrlen(c, chlen(c)+1));
    runfunc(g, scanfunc);
}
void addtok(int i, Prog* g) {
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
void discardtok(int i, Prog* g) {
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
        discardtok(i, p);
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
        return;
    }
    Vect* str = getstr(p->t)->t->w.v;
    for (i = 0; str->v[i] == '.'; i++);
    if (i) {addtok(i, g); return;}
    while (!contains(str->v[i], " \n\t\b\r.")) {
        if (i+1 == str->len) {break;}
        i++;
    }
    addtok(i, g);
}
void tokens(Prog* g) {
    // Splitting on these 
    char* c = " \n\t\b\r";
    Atom* p = P(g)->t;
    Vect* str = getstr(p->t)->t->w.v;
    int i = 0;
    while (contains(str->v[i], c)) {i++;}
    if (i) {
        discardtok(i, g);
        str = getstr(p->t)->t->w.v;
    }
    char b = str->v[0] == '"';
    if (b) {
        charptostr(p, str->v+1);
        i = pullw(p);
        swap(p);
        discardtok(i, g);
    }
    else {
        for (i = 0; str->v[i] == '.'; i++);
        if (!i) {
            while (1) {
                if (i == str->len) {addtok(i-1, g); return;}
                if (str->v[i] == '.' || contains(str->v[i], c)) {break;}
                i++;
            }
        }
        addtok(i, g);
    }
    tokens(g);
}

// print helpers
void printvect(Vect* v) {
    for (int i = 0; v->v[i] && i < v->len; i++) {putchar(v->v[i]);}
}
void print(Atom* a, int depth);
void printarr(Atom* a, int depth) {
    puts("\n");
    DARKYELLOW;
    for (int i = 0; i < depth; i++) {puts("  ");}
    if (a->e) {puts("┗ ");}
    else {puts("┣ ");}
    print(a, depth);
}
void print(Atom* a, int depth) {
    if (!a) {puts("None\n"); return;}
    Atom* s = chscan(a->t, "\"");
    bool showt = false || a->t;
    if (s) {
        // printer functionality
        Atom* p = tset(ref(new()), a);
        pushw(p, depth);
        pushnew(p, s->t);
        Atom* g = ref(prog());
        tset(P(g), p);
        dot(g);
        del(g);
        showt = false;
        del(p);
    }
    else {
        // printint((Word) a, 8);
        // putchar(' ');
        if (isstr(a)) {RED; printvect(getstr(a)->t->w.v); showt = false;}
        else {
            if (a->e == 2) {DARKGREEN;}
            else if (a->f == word) {BLACK; printint(a->w.w, 4);}
            else if (a->f == func) {GREEN; puts("func");}
            else if (a->f == exec) {DARKRED; puts("dot");}
            else if (a->f == vect) {RED; printvect(a->w.v);}
            else if (a->f == atom) {YELLOW;}
            if (a->r != 1) {RED; printint(a->r, 4);}
        }
    }
    if (a->e) {
        DARKGREEN; puts(" e");
        if (!a->n) {putchar('0');}
        RESET;
    }
    if (showt) {printarr(a->t, depth+1);}
    if (!a->e) {printarr(a->n, depth);}
}
void println(Atom* a) {DARKYELLOW; puts("┏ "); print(a, 0); putchar('\n');}

int main() {
// linux only
    FILE* FP = fopen("challenge", "r");
    fseeko(FP, 0, SEEK_END);
    int i = ftell(FP);
    char program[i+1];
    char* program_ = program;
    rewind(FP);
    while (!feof(FP)) {*program_++ = fgetc(FP);}
    *--program_ = 0;
    fclose(FP);

    Atom* G = ref(prog());
    pushnew(P(G), 0);
    pushstr(P(G)->t, program);
    tokens(G);
    pull(P(G)->t);
    println(P(G)->t);
    del(G);

    // char buff[0x100];
    // for (int i = 0; i < 0x100; i++) {buff[i] = 0;}
    // P = ref(new());
    // P->e = true;
    
    // while (buff[0] != '\n') {
    //     push(P, newstrlen(buff, chlen(buff)));
    //     P = runfunc(P, tokenizer);
    //     println(P);

    //     puts("-> ");
    //     fgets(buff, 0x100, stdin);
    // }

    // del(G);
}