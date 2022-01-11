#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "exception.h"
#include "sc64.h"


extern char _sheap __attribute__((section(".data")));
extern char _eheap __attribute__((section(".data")));


int _close_r (struct _reent *prt, int fd) {
    errno = ENOSYS;
    return -1;
}

int _fstat_r (struct _reent *prt, int fd, struct stat *pstat) {
    errno = ENOSYS;
    return -1;
}

int _isatty_r (struct _reent *prt, int fd) {
    if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        return 1;
    }
    errno = EBADF;
    return 0;
}

off_t _lseek_r (struct _reent *prt, int fd, off_t pos, int whence) {
    errno = ENOSYS;
    return -1;
}

ssize_t _read_r (struct _reent *prt, int fd, void *buf, size_t cnt) {
    errno = ENOSYS;
    return -1;
}

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

ssize_t _write_r (struct _reent *prt, int fd, const void *buf, size_t cnt) {
    if (fd == STDIN_FILENO) {
        errno = EBADF;
        return -1;
    } else if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        sc64_debug_write(SC64_DEBUG_ID_TEXT, buf, cnt);
        return cnt;
    }
    errno = ENOSYS;
    return -1;
}

void __assert_func (const char *file, int line, const char *func, const char *failedexpr) {
    EXCEPTION_TRIGGER(TRIGGER_CODE_ASSERT);
    while (1);
}
