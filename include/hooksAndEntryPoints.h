#ifndef hooksAndEntryPoints_h
#define hooksAndEntryPoints_h

#include <stddef.h>
#include "macros.h"
#include "hook.h"

// This extern stops the malloc etc. hooks from doing anything different to the normal
// calls until MemoryCounterManager is fully constructed. It is also used to switch
// off special behaviour when MemoryCounterManager has been destructed.
extern bool memcounter_globallyDisabled;
void enableThisThread();
void disableThisThread();

DUAL_HOOK(1, void *, domalloc, _main, _libc, (size_t n), (n), "malloc", 0, "libc.so.6")
DUAL_HOOK(2, void *, docalloc, _main, _libc, (size_t n, size_t m), (n, m), "calloc", 0, "libc.so.6")
DUAL_HOOK(2, void *, dorealloc, _main, _libc, (void *ptr, size_t n), (ptr, n), "realloc", 0, "libc.so.6")
DUAL_HOOK(3, int, dopmemalign, _main, _libc, (void **ptr, size_t alignment, size_t size), (ptr, alignment, size), "posix_memalign", 0, "libc.so.6")
DUAL_HOOK(2, void *, domemalign, _main, _libc, (size_t alignment, size_t size), (alignment, size), "memalign", 0, "libc.so.6")
DUAL_HOOK(1, void *, dovalloc, _main, _libc, (size_t size), (size), "valloc", 0, "libc.so.6")
DUAL_HOOK(1, void, dofree, _main, _libc, (void *ptr), (ptr), "free", 0, "libc.so.6")

DUAL_HOOK(1, void, doexit, _main, _libc, (int code), (code), "exit", 0, "libc.so.6")
DUAL_HOOK(1, void, doexit, _main2, _libc2, (int code), (code), "_exit", 0, "libc.so.6")
DUAL_HOOK(2, int, dokill, _main, _libc, (pid_t pid, int sig), (pid, sig), "kill", 0, "libc.so.6")

LIBHOOK(4, int, dopthread_create, _main, (pthread_t *thread, const pthread_attr_t *attr, void * (*start_routine)(void *), void *arg), (thread, attr, start_routine, arg), "pthread_create", 0, 0)
LIBHOOK(4, int, dopthread_create, _pthread20, (pthread_t *thread, const pthread_attr_t *attr, void * (*start_routine)(void *), void *arg), (thread, attr, start_routine, arg), "pthread_create", "GLIBC_2.0", 0)
LIBHOOK(4, int, dopthread_create, _pthread21, (pthread_t *thread, const pthread_attr_t *attr, void * (*start_routine)(void *), void *arg), (thread, attr, start_routine, arg), "pthread_create", "GLIBC_2.1", 0)


#endif
