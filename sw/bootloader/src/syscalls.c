#include <errno.h>
#include <sys/types.h>


extern char _sheap;
extern char _eheap;


caddr_t _sbrk_r (struct _reent *prt, ptrdiff_t incr) {
    static char *curr_heap_end = &_sheap;
    char *prev_heap_end;

    prev_heap_end = curr_heap_end;
    curr_heap_end += incr;

    if (curr_heap_end > &_eheap) {
        errno = ENOMEM;
        return (caddr_t) -1;
    }

    return (caddr_t) prev_heap_end;
}
