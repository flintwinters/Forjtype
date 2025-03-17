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
Atom* P;

// Creates a new, zero-initialized atom with no references.
Atom* new() {
    Atom* a = malloc(sizeof(Atom));
    *a = (Atom) {0, 0, 0, 0, 0, 0};
    return a;
}
Atom* get(Atom* a, int i);
// Deletes a reference to an atom.
// If the atom reaches zero references, free its memory.
// Also frees vects:
//  - Vects don't have refcounts, so must be referenced by
//    exactly one atom at all times.
Atom* del(Atom* a) {
    if (!a) {return a;}
    if (--a->r) {return a;}
    if (a->f == vect) {freevect(a->w.v);}
    if (a->e == false) {del(a->n);}
    del(a->t);
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
    a->e = false;
    if (!n) {a->e = true;}
    a->n = n; return a;
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
            if (p->n) {a->n = p->n->t;}
            a->e = true;
            return tset(p, a);
        }
        nset(a, p->t);
    }
    else {
        if (!a->e) {del(a->n);}
        if (p->n) {a->n = p->n->t;}
        else {a->n = p;}
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
Atom* pull(Atom* p) {
    if (p->t->e) {return pushe(p, p->t->n);}
    return tset(p, p->t->n);
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
// Executing behavior for a list.
// [ a b c ] .
// ^ From left to right, (bottom to top of stack),
// dup the element onto p.  If it's a multidot (ie: ...),
// Execute it, reducing by one.
// Double dot (..) will run dot() on the top of stack.
Atom* dot(Atom* a, Atom* p);
Atom* array(Atom* a, Atom* p) {
    p = (!ref(a)->e) ? array(a->n, p) : p;
    if (a->f == exec) {
        if (a->t) {dup(a->t, p);}
        else {p = dot(p->t, p);}
    }
    else {dup(a, p);}
    del(a);
    return p;
}
// Single dot, or activated double dot:
//      A single dot '.', runs immediately when it is
//      parsed (at parse-time).  Whereas a double dot
//      '..' merely pushes this function.
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
Atom* get(Atom* a, int i) {
    if (!a) {return 0;}
    if (i == 0) {return a;}
    if (!a->e) {
        if (i >= 0) {return get(a->n, i-1);}
        return get(a->n, -1);
    }
    return a;
}
// Open the top atom.
//      Points p inside the top atom.
//      Until ] is called, everything will take place
//      inside that atom.
Atom* open(Atom* a, Atom* p) {
    pull(p);
    p = ref(nset(new(), p));
    p->w.a = p->n->t;
    if (p->n->t) {
        if (p->n->t->t) {return tset(p, p->n->t->t);}
        else {pushe(p->n->t, p->n->t); return p;}
    }
    return pushe(p, p->t);
}
// Close the current atom ]
Atom* close(Atom* a, Atom* p) {
    pull(p);
    if (p->w.a) {
        tset(p->w.a, p->t);
    }
    Atom* n = p->n;
    del(p);
    return n;
}
// Swap the top two elements.
Atom* swap(Atom* a, Atom* p) {
    pull(p);
    Atom* b = p->t->n;
    p->t->n = p->t->n->n;
    b->n = p->t;
    p->t = b;
    // Swap the 'end' flags
    short e = b->e;
    b->e = b->n->e;
    b->n->e = e;
    return p;
}
// ! inbuilt function
// creates a reference to the top of the stack.
Atom* top(Atom* a, Atom* p) {
    pull(p);
    return pushnew(p, p->t);
}
// Wrap pull in 'Func' type compatible function
Atom* pulls(Atom* a, Atom* p) {pull(p); return pull(p);}
Atom* throw(Atom* a, Atom* p) {
    pull(p);
    if (p->n) {
        dup(p->t, p->n);
        pull(p);
    }
    return p;
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
Atom* token(Atom* p, char* c);
// Returns c if c is in s, else 0
char contains(char c, char* s) {while (*s != c) {if (!*s) {return 0;} s++;} return c;}
// Returns true if a == b
bool equstr(char* a, char* b) {
    while (*a == *b && *a) {a++; b++;}
    if (*a) {return false;}
    return true;
}
Atom* scan(Atom* a, Atom* p);
// Safetly checks if the atom holds a string
// In the future, methods will be written in Forj to 
// compare to templates, making this more elegant.
bool isstr(Atom* str) {
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
Atom* runfunc(Atom* p, Func f) {
    pushfunc(p, f);
    return token(p, ".");
}
// Searches straight down.  If a list has a parent,
// It will be searched as well.
// This is used for implicit variable search: `val :name name`
// This is related to the :symbol functionality.
// Since one scan call leaves (a reference to the) the associated
// variable on the top, :symbol2 can be called again to get
// a variable from the second, interior layer.
Atom* varscan(Atom* a, Atom* p, Atom* s) {
    Atom* v = 0;
    while (a) {
        if (isstr(a) &&
            equstr(getstr(a)->t->w.v->v, s->t->w.v->v)
            ) {
            v = new();
            tset(v, a->n->t);
            v->f = a->n->f;
            v->w.w = a->n->w.w;
            break;
        }
        a = a->n;
    }
    del(s);
    return v;
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
Atom* scan(Atom* a, Atom* p) {
    pull(p);
    Atom* s = pulln(p);
    Atom* v;
    if (isstr(p->t)) {
        p = token(p, ".");
        v = varscan(p->t->t, p, s);
        pull(p);
    }
    else {v = varscan(p->t->t, p, s);}
    if (v) {push(p, v);}
    return p;
}
// Returns the length of the char*
int chlen(char* c) {
    int n = 0;
    while (*c++) {n++;}
    return n;
}
// Pushes a vector, consumes length from stack
Atom* pushvect(Atom* a, Atom* p) {
    int maxlen = p->t->w.w;
    pull(p);
    Vect* v = valloclen((maxlen) ? maxlen : 1);
    a = pushnew(new(), 0);
    a->t->f = vect;
    a->t->w.v = v;
    a->w.w = v->len = maxlen;
    return push(p, a);
}
Atom* allocstrdata(Atom* s, int len) {
    pushw(s, len);
    return pushvect(s, s);
}
Atom* newstr() {
    Atom* s = new();
    allocstrdata(s, 0);
    pushfunc(s, scan);
    token(s, "..");
    return s;
}
void setstr(Atom* s, char* c, int len) {
    getstr(s)->t->w.v->len = 0;
    rawpushv(getstr(s)->t->w.v, c, len);
}
// Creates a string object
Atom* newstrlen(char* c, int len) {
    Atom* s = new();
    allocstrdata(s, len);
    setstr(s, c, len);
    pushfunc(s, scan);
    token(s, "..");
    return s;
}
Atom* pushstr(Atom* p, char* c) {return push(p, newstrlen(c, chlen(c)+1));}

// Consumes the numeric string and outputs a literal
Atom* strtoint(Atom* a, Atom* p) {
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
    return p;
}
// Returns the length of the char*, accounting for
// any backslashes
// - Both this and charptostr need some work. -
Atom* charptostr(Atom* p, char* c) {
    int i, j;
    Atom* s = newstr();
    Atom* v = getstr(s)->t;
    for (i = j = 0; c[i+j]; i++) {
        if (c[i+j] == '"') {break;}
        if (c[i+j] == '\\') {j++;}
        v->w.v = vectpushc(v->w.v, c[i+j]);
    }
    return push(p, s);
}

Atom* find(Atom* a, char* c) {
    if (!a || !a->t) {return 0;}
    Atom* p = ref(new());

    // newrawstr(p, c, chlen(c)+1);
    push(p, newstrlen(c, chlen(c)+1));
    Atom* s = pulln(p);
    Atom* v = varscan(a->t, p, s);
    if (v) {push(p, v);}
    s = pulln(p);
    del(p);

    return s;
}

// Breaks a string into two separate strings, at
// the consumed integer index.
Atom* splitstrat(Atom* a, Atom* p) {
    Word i = pullw(p);
    pushstr(p, getstr(p->t)->t->w.v->v+i);
    runfunc(p, swap);
    getstr(p->t)->t->w.v->len = i;
    getstr(p->t)->t->w.v->v[i] = 0;
    return p;
}

// Creates an object indicating several dots at once
// ... and .... etc.
Atom* createmultidot(int i) {
    Atom* t = new();
    t->f = exec;
    if (--i) {return push(t, createmultidot(i));}
    t->w.f = dot;
    return t;
}
Atom* sayhi(Atom* a, Atom* p) {
    pull(p);
    puts("HI");
    return p;
}
// Parses one token over the program p
Atom* token(Atom* p, char* c) {
    int i = 0;
    for (; c[i] == '.'; i++);
    if (i == 1) {return dot(p->t, p);}
    if (i > 1) {return push(p, createmultidot(i-1));}
    if (c[0] == ':') {
        if (c[1]) {return pushstr(p, c+1);}
        return pushfunc(p, scan);
    }
    if (c[0] == 'a') {return pushfunc(p, sayhi);}
    if (c[0] == ',') {return pushfunc(p, pulls);}
    if (c[0] == ';') {return pushfunc(p, throw);}
    if (c[0] == '!') {return pushfunc(p, top);}
    if (c[0] == '[') {return pushfunc(p, open);}
    if (c[0] == ']') {return pushfunc(p, close);}
    if (c[0] == '"') {return charptostr(p, c+1);}
    if (contains(c[0], "0123456789")) {
        return runfunc(pushstr(p, c), strtoint);
    }
    push(p, newstrlen(c, chlen(c)+1));
    Atom* s = pulln(p);
    Atom* v = varscan(p->t, p, getstr(s));
    if (v) {push(p, v);}
    del(s);
    return p;
}
void debug(Atom* p) {
    return;
}
// Parses repeated tokens.
Atom* tokens(Atom* p) {
    // Splitting on these 
    char* c = " \n\t\b\r";
    Vect* str = getstr(p->t)->t->w.v;
    int i = 0;
    while (contains(str->v[i], c)) {i++;}
    if (i) {
        pushw(p, i);
        splitstrat(0, p);
        pull(p);
        str = getstr(p->t)->t->w.v;
    }
    debug(p);
    char b = str->v[0] == '"';
    if (b) {
        i = getstr(charptostr(p, str->v+1)->t)->t->w.v->len;
        p = runfunc(p, swap);
        // return tokens(p);
    }
    else {
        for (i = 0; str->v[i] == '.'; i++);
        if (!i) {
            while (1) {
                if (i == str->len) {return p;}
                if (str->v[i] == '.' || contains(str->v[i], c)) {break;}
                i++;
            }
        }
    }
    pushw(p, i);
    splitstrat(0, p);
    Atom* s = pulln(p);
    Vect* v = getstr(s)->t->w.v;
    if (!b) {
        pull(p);
        p = token(p, v->v);
        push(p, s->n);
    }
    del(s);
    return tokens(p);
}

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
    Atom* s = find(a, "\"");
    bool showt = false || a->t;
    if (s && s->f == func) {
        Atom* p = pushnew(ref(new()), a);
        pushw(p, depth);
        runfunc(p, s->w.f);
        showt = false;
        pull(p);
        pull(p);
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
            if (a->e) {
                DARKGREEN; puts(" e");
                if (!a->n) {putchar('0');}
            }
            RESET; 
        }
    }
    del(s);
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
    char* prog = program;
    rewind(FP);
    while (!feof(FP)) {*prog++ = fgetc(FP);}
    *--prog = 0;
    fclose(FP);
    P = ref(new());
    P->e = true;
    pushstr(P, program);
    P = tokens(P);
    P = pull(P);
    println(P);

    // char buff[0x100];
    // for (int i = 0; i < 0x100; i++) {buff[i] = 0;}
    // P = ref(new());
    // P->e = true;
    
    // while (buff[0] != '\n') {
    //     push(P, newstr(buff));
    //     P = tokens(P);
    //     P = pull(P);
    //     println(P);

    //     puts("-> ");
    //     fgets(buff, 0x100, stdin);
    // }

    del(P);
}