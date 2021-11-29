#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "sc64.h"


int _close_r(struct _reent *prt, int fd) {
    return -1;
}

int _fstat_r(struct _reent *prt, int fd, struct stat *pstat) {
    pstat->st_mode = S_IFCHR;
    return 0;
}

int _isatty_r(struct _reent *prt, int fd) {
    return 1;
}

off_t _lseek_r(struct _reent *prt, int fd, off_t pos, int whence) {
    return 0;
}

ssize_t _read_r(struct _reent *prt, int fd, void *buf, size_t cnt) {
    return 0;
}

caddr_t _sbrk_r(struct _reent *prt, ptrdiff_t incr) {
    extern char _sheap;
    extern char _eheap;

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

ssize_t _write_r(struct _reent *prt, int fd, const void *buf, size_t cnt) {
    sc64_debug_write(SC64_DEBUG_TYPE_TEXT, buf, cnt);
    return cnt;
}

void __assert_func(const char *file, int line, const char *func, const char *failedexpr) {
    LOG_E("\r\nassertion \"%s\" failed: file \"%s\", line %d%s%s\r\n", failedexpr, file, line, func ? ", function: " : "", func ? func : "");
    while (1);
}
