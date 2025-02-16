#ifdef __linux__
#include <stdlib.h>
#include <stdio.h>
#include "utils.c"
extern void reclaim(void* b_, word a) {free(b_);}
#else
#include "../rv64/alloc.c"
#endif

typedef char byte;
typedef long long word;
typedef struct Vect Vect;

// Dynamic array
struct Vect {short len, maxlen; byte v[];};

// Pre-allocate a dynamic array of `maxlen` bytes
struct Vect* valloclen(int maxlen) {
    int n = sizeof(struct Vect)+maxlen;
    struct Vect* newv = malloc(n);
    newv->maxlen = maxlen;
    newv->len = 0;
    return newv;
}
// Copy `len` bytes from `src` to `dest`
void cpymem(byte* dest, byte* src, word len) {
    for (int i = 0; i < len; i++) {
        dest[i] = src[i];
    }
}
// Copy `len` bytes from `src` to `dest`, mirroring the data
void cpymemrev(byte* dest, byte* src, word len) {
    for (int i = 0; i < len; i++) {
        dest[i] = src[len-i-1];
    }
}
// Resize a dynamic array's allocation
struct Vect* resize(struct Vect* v, int newmaxlen) {
    if (!newmaxlen) {
        newmaxlen = (v->maxlen) ? 2*v->maxlen: 1;
    }
    struct Vect* newv = valloclen(newmaxlen);
    newv->len = v->len;
    cpymem(newv->v, v->v, v->len);
    free(v);
    return newv;
}

Vect* growtofit(Vect* v) {
    word newlen = 1;
    if (!v->maxlen) {newlen = v->maxlen;}
    while (newlen < v->len) {
        newlen = newlen << 1;
    }
    return resize(v, newlen);
}
// Resize if the new len is bigger.
Vect* condresize(Vect* v, int addlen) {
    v->len += addlen;
    if (v->len > v->maxlen) {v = growtofit(v);}
    return v;
}
