/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:4;tab-width:8;coding:utf-8 -*-│
│ vi: set noet ft=c ts=4 sts=4 sw=4 fenc=utf-8                             :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Python 3                                                                     │
│ https://docs.python.org/3/license.html                                       │
╚─────────────────────────────────────────────────────────────────────────────*/

/* stringlib: count implementation */

#ifndef STRINGLIB_FASTSEARCH_H
#error must include fastsearch.inc before including this module
#endif

Py_LOCAL_INLINE(Py_ssize_t)
STRINGLIB(count)(const STRINGLIB_CHAR* str, Py_ssize_t str_len,
                const STRINGLIB_CHAR* sub, Py_ssize_t sub_len,
                Py_ssize_t maxcount)
{
    Py_ssize_t count;

    if (str_len < 0)
        return 0; /* start > len(str) */
    if (sub_len == 0)
        return (str_len < maxcount) ? str_len + 1 : maxcount;

    count = FASTSEARCH(str, str_len, sub, sub_len, maxcount, FAST_COUNT);

    if (count < 0)
        return 0; /* no match */

    return count;
}


