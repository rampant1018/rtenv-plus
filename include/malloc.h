#ifndef MALLOC_H
#define MALLOC_H

#include <stdint.h>
#include <stddef.h>

// for alignment to long boundary
#if UINTPTR_MAX == 0xffffffff
typedef uint32_t Align;
#else
#error "unexcepted value for UINTPTR_MAX marco"
#endif

union header { // block header
    struct {
        union header *ptr; // next free block on the free list
        unsigned size;     // size of this block
    } s;
    Align x; // force alignment
};

typedef union header Header;

void *malloc(unsigned nbytes);
void free(void *ap);

#endif
