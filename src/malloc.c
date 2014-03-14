#include "malloc.h"
#include "syscall.h"

static Header base; // first empty block
static Header *freep = NULL; // start of free list

void *malloc(unsigned nbytes) 
{
    Header *p, *prevp;
    unsigned nunits;
    void *cp;

    nunits = (nbytes + sizeof(Header) - 1) / sizeof(Header) + 1; // Round up

    if((prevp = freep) == NULL) { // first call malloc
        base.s.ptr = freep = prevp = &base;
        base.s.size = 0;
    }

    for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr) { // Search for avaiable block
        if(p->s.size >= nunits) { // big enough
            if(p->s.size == nunits) { // size same
                prevp->s.ptr = p->s.ptr; // remove this block from free list
            }
            else { // allocate tail end
                p->s.size -= nunits;
                p += p->s.size;
                p->s.size = nunits;
            }
            freep = prevp;
            return (void *)(p + 1);
        }
        if(p == freep) { // wrapped around free list
            cp = sbrk(nunits * sizeof(Header));
            if(cp == (void *)-1) {
                return NULL; // fail
            }
            else {
                p = (Header *)cp;
                p->s.size = nunits;
                free((void *)(p + 1));
                p = freep;
            }
        }
    }
}

void free(void *ap)
{
    Header *bp, *p;
    bp = (Header *)ap - 1; // Point to block header
    for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr) { // Search for proper location
        if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))  // freed block at start or end of arena
            break;
    }

    if(bp + bp->s.size == p->s.ptr) { // Join to upper nbr
        bp->s.size += p->s.ptr->s.size;
        bp->s.ptr = p->s.ptr->s.ptr;
    }
    else { 
        bp->s.ptr = p->s.ptr;
    }

    if(p + p->s.size == bp) { // Join to lower nbr
        p->s.size += bp->s.size;
        p->s.ptr = bp->s.ptr;
    }
    else {
        p->s.ptr = bp;
    }

    freep = p;
}
