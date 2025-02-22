/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:4;tab-width:8;coding:utf-8 -*-│
│ vi: set noet ft=c ts=4 sts=4 sw=4 fenc=utf-8                             :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Python 3                                                                     │
│ https://docs.python.org/3/license.html                                       │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/calls/calls.h"
#include "libc/errno.h"
#include "libc/limits.h"
#include "libc/log/log.h"
#include "libc/stdio/stdio.h"
#include "third_party/python/Include/ceval.h"
#include "third_party/python/Include/intrcheck.h"
#include "third_party/python/Include/pyerrors.h"
#include "third_party/python/Include/pymem.h"
#include "third_party/python/Include/pystate.h"

/* Readline interface for tokenizer.c and [raw_]input() in bltinmodule.c.
   By default, or when stdin is not a tty device, we have a super
   simple my_readline function using fgets.
   Optionally, we can use the GNU readline library.
   my_readline() has a different return value from GNU readline():
   - NULL if an interrupt occurred or if an error occurred
   - a malloc'ed empty string if EOF was read
   - a malloc'ed string ending in \n normally
*/

PyThreadState* _PyOS_ReadlineTState;

#ifdef WITH_THREAD
#include "third_party/python/Include/pythread.h"
static PyThread_type_lock _PyOS_ReadlineLock = NULL;
#endif

int (*PyOS_InputHook)(void) = NULL;

/* This function restarts a fgets() after an EINTR error occurred
   except if PyOS_InterruptOccurred() returns true. */

static int
my_fgets(char *buf, int len, FILE *fp)
{
    char *p;
    int err;
    while (1) {
        if (PyOS_InputHook != NULL)
            (void)(PyOS_InputHook)();
        errno = 0;
        clearerr(fp);
        p = fgets(buf, len, fp);
        if (p != NULL)
            return 0; /* No error */
        err = errno;
        if (feof(fp)) {
            clearerr(fp);
            return -1; /* EOF */
        }
#ifdef EINTR
        if (err == EINTR) {
            int s;
#ifdef WITH_THREAD
            PyEval_RestoreThread(_PyOS_ReadlineTState);
#endif
            s = PyErr_CheckSignals();
#ifdef WITH_THREAD
            PyEval_SaveThread();
#endif
            if (s < 0)
                    return 1;
        /* try again */
            continue;
        }
#endif
        if (PyOS_InterruptOccurred()) {
            return 1; /* Interrupt */
        }
        return -2; /* Error */
    }
    /* NOTREACHED */
}

#ifdef MS_WINDOWS
/* Readline implementation using ReadConsoleW */

extern char _get_console_type(HANDLE handle);

char *
_PyOS_WindowsConsoleReadline(HANDLE hStdIn)
{
    static wchar_t wbuf_local[1024 * 16];
    const DWORD chunk_size = 1024;

    DWORD n_read, total_read, wbuflen, u8len;
    wchar_t *wbuf;
    char *buf = NULL;
    int err = 0;

    n_read = (DWORD)-1;
    total_read = 0;
    wbuf = wbuf_local;
    wbuflen = sizeof(wbuf_local) / sizeof(wbuf_local[0]) - 1;
    while (1) {
        if (PyOS_InputHook != NULL) {
            (void)(PyOS_InputHook)();
        }
        if (!ReadConsoleW(hStdIn, &wbuf[total_read], wbuflen - total_read, &n_read, NULL)) {
            err = GetLastError();
            goto exit;
        }
        if (n_read == (DWORD)-1 && (err = GetLastError()) == ERROR_OPERATION_ABORTED) {
            break;
        }
        if (n_read == 0) {
            int s;
            err = GetLastError();
            if (err != ERROR_OPERATION_ABORTED)
                goto exit;
            err = 0;
            HANDLE hInterruptEvent = _PyOS_SigintEvent();
            if (WaitForSingleObjectEx(hInterruptEvent, 100, FALSE)
                    == WAIT_OBJECT_0) {
                ResetEvent(hInterruptEvent);
#ifdef WITH_THREAD
                PyEval_RestoreThread(_PyOS_ReadlineTState);
#endif
                s = PyErr_CheckSignals();
#ifdef WITH_THREAD
                PyEval_SaveThread();
#endif
                if (s < 0)
                    goto exit;
            }
            break;
        }

        total_read += n_read;
        if (total_read == 0 || wbuf[total_read - 1] == L'\n') {
            break;
        }
        wbuflen += chunk_size;
        if (wbuf == wbuf_local) {
            wbuf[total_read] = '\0';
            wbuf = (wchar_t*)PyMem_RawMalloc(wbuflen * sizeof(wchar_t));
            if (wbuf)
                wcscpy_s(wbuf, wbuflen, wbuf_local);
            else {
                PyErr_NoMemory();
                goto exit;
            }
        }
        else {
            wchar_t *tmp = PyMem_RawRealloc(wbuf, wbuflen * sizeof(wchar_t));
            if (tmp == NULL) {
                PyErr_NoMemory();
                goto exit;
            }
            wbuf = tmp;
        }
    }

    if (wbuf[0] == '\x1a') {
        buf = PyMem_RawMalloc(1);
        if (buf)
            buf[0] = '\0';
        else {
            PyErr_NoMemory();
        }
        goto exit;
    }

    u8len = WideCharToMultiByte(CP_UTF8, 0, wbuf, total_read, NULL, 0, NULL, NULL);
    buf = PyMem_RawMalloc(u8len + 1);
    if (buf == NULL) {
        PyErr_NoMemory();
        goto exit;
    }
    u8len = WideCharToMultiByte(CP_UTF8, 0, wbuf, total_read, buf, u8len, NULL, NULL);
    buf[u8len] = '\0';

exit:
    if (wbuf != wbuf_local)
        PyMem_RawFree(wbuf);

    if (err) {
#ifdef WITH_THREAD
        PyEval_RestoreThread(_PyOS_ReadlineTState);
#endif
        PyErr_SetFromWindowsErr(err);
#ifdef WITH_THREAD
        PyEval_SaveThread();
#endif
    }

    return buf;
}

#endif


/* Readline implementation using fgets() */

char *
PyOS_StdioReadline(FILE *sys_stdin, FILE *sys_stdout, const char *prompt)
{
    size_t n;
    char *p, *pr;

#ifdef MS_WINDOWS
    if (!Py_LegacyWindowsStdioFlag && sys_stdin == stdin) {
        HANDLE hStdIn, hStdErr;

        _Py_BEGIN_SUPPRESS_IPH
        hStdIn = (HANDLE)_get_osfhandle(fileno(sys_stdin));
        hStdErr = (HANDLE)_get_osfhandle(fileno(stderr));
        _Py_END_SUPPRESS_IPH

        if (_get_console_type(hStdIn) == 'r') {
            fflush(sys_stdout);
            if (prompt) {
                if (_get_console_type(hStdErr) == 'w') {
                    wchar_t *wbuf;
                    int wlen;
                    wlen = MultiByteToWideChar(CP_UTF8, 0, prompt, -1,
                            NULL, 0);
                    if (wlen) {
                        wbuf = PyMem_RawMalloc(wlen * sizeof(wchar_t));
                        if (wbuf == NULL) {
                            PyErr_NoMemory();
                            return NULL;
                        }
                        wlen = MultiByteToWideChar(CP_UTF8, 0, prompt, -1,
                                wbuf, wlen);
                        if (wlen) {
                            DWORD n;
                            fflush(stderr);
                            /* wlen includes null terminator, so subtract 1 */
                            WriteConsoleW(hStdErr, wbuf, wlen - 1, &n, NULL);
                        }
                        PyMem_RawFree(wbuf);
                    }
                } else {
                    fprintf(stderr, "%s", prompt);
                    fflush(stderr);
                }
            }
            clearerr(sys_stdin);
            return _PyOS_WindowsConsoleReadline(hStdIn);
        }
    }
#endif

    n = 100;
    p = (char *)PyMem_RawMalloc(n);
    if (p == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    fflush(sys_stdout);
    if (prompt)
        fprintf(stderr, "%s", prompt);
    fflush(stderr);

    switch (my_fgets(p, (int)n, sys_stdin)) {
    case 0: /* Normal case */
        break;
    case 1: /* Interrupt */
        PyMem_RawFree(p);
        return NULL;
    case -1: /* EOF */
    case -2: /* Error */
    default: /* Shouldn't happen */
        *p = '\0';
        break;
    }
    n = strlen(p);
    while (n > 0 && p[n-1] != '\n') {
        size_t incr = n+2;
        if (incr > INT_MAX) {
            PyMem_RawFree(p);
            PyErr_SetString(PyExc_OverflowError, "input line too long");
            return NULL;
        }
        pr = (char *)PyMem_RawRealloc(p, n + incr);
        if (pr == NULL) {
            PyMem_RawFree(p);
            PyErr_NoMemory();
            return NULL;
        }
        p = pr;
        if (my_fgets(p+n, (int)incr, sys_stdin) != 0)
            break;
        n += strlen(p+n);
    }
    pr = (char *)PyMem_RawRealloc(p, n+1);
    if (pr == NULL) {
        PyMem_RawFree(p);
        PyErr_NoMemory();
        return NULL;
    }
    return pr;
}


/* By initializing this function pointer, systems embedding Python can
   override the readline function.

   Note: Python expects in return a buffer allocated with PyMem_Malloc. */

char *(*PyOS_ReadlineFunctionPointer)(FILE *, FILE *, const char *);


/* Interface used by tokenizer.c and bltinmodule.c */

char *
PyOS_Readline(FILE *sys_stdin, FILE *sys_stdout, const char *prompt)
{
    char *rv, *res;
    size_t len;

    if (_PyOS_ReadlineTState == PyThreadState_GET()) {
        PyErr_SetString(PyExc_RuntimeError,
                        "can't re-enter readline");
        return NULL;
    }


    if (PyOS_ReadlineFunctionPointer == NULL) {
        PyOS_ReadlineFunctionPointer = PyOS_StdioReadline;
    }

#ifdef WITH_THREAD
    if (_PyOS_ReadlineLock == NULL) {
        _PyOS_ReadlineLock = PyThread_allocate_lock();
        if (_PyOS_ReadlineLock == NULL) {
            PyErr_SetString(PyExc_MemoryError, "can't allocate lock");
            return NULL;
        }
    }
#endif

    _PyOS_ReadlineTState = PyThreadState_GET();
    Py_BEGIN_ALLOW_THREADS
#ifdef WITH_THREAD
    PyThread_acquire_lock(_PyOS_ReadlineLock, 1);
#endif

    /* This is needed to handle the unlikely case that the
     * interpreter is in interactive mode *and* stdin/out are not
     * a tty.  This can happen, for example if python is run like
     * this: python -i < test1.py
     */
    if (!isatty (fileno (sys_stdin)) || !isatty (fileno (sys_stdout)))
        rv = PyOS_StdioReadline (sys_stdin, sys_stdout, prompt);
    else
        rv = (*PyOS_ReadlineFunctionPointer)(sys_stdin, sys_stdout,
                                             prompt);
    Py_END_ALLOW_THREADS

#ifdef WITH_THREAD
    PyThread_release_lock(_PyOS_ReadlineLock);
#endif

    _PyOS_ReadlineTState = NULL;

    if (rv == NULL)
        return NULL;

    len = strlen(rv) + 1;
    res = PyMem_Malloc(len);
    if (res != NULL) {
        memcpy(res, rv, len);
    }
    else {
        PyErr_NoMemory();
    }
    PyMem_RawFree(rv);

    return res;
}
