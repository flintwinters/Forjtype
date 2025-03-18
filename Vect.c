#ifdef __riscv
#include "alloc.c"
#else
#include <stdlib.h>
#include <stdio.h>
#include "utils.c"
extern void reclaim(void* b_, Word a) {free(b_);}
#endif

typedef char byte;
typedef long long Word;
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
void cpymem(byte* dest, byte* src, Word len) {
    for (int i = 0; i < len; i++) {
        dest[i] = src[i];
    }
}
// Copy `len` bytes from `src` to `dest`, mirroring the data
void cpymemrev(byte* dest, byte* src, Word len) {
    for (int i = 0; i < len; i++) {
        dest[i] = src[len-i-1];
    }
}
void freevect(struct Vect* v) {
    reclaim(v, sizeof(struct Vect)+v->maxlen);
}
// Resize a dynamic array's allocation
struct Vect* resize(struct Vect* v, int newmaxlen) {
    if (!newmaxlen) {
        newmaxlen = (v->maxlen) ? 2*v->maxlen: 1;
    }
    struct Vect* newv = valloclen(newmaxlen);
    newv->len = v->len;
    cpymem(newv->v, v->v, v->maxlen);
    freevect(v);
    return newv;
}

Vect* growtofit(Vect* v) {
    Word newlen = 1;
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
// Push raw `dat` to v
// `size` is in bytes
Vect* rawpushv(Vect* v, void* dat, Word size) {
    v = condresize(v, size);
    cpymem((char*) v->v+v->len-size, dat, size);
    return v;
}
Vect* vectpushc(Vect* v, char c) {
    v = condresize(v, 1);
    v->v[v->len-1] = c;
    return v;
}
