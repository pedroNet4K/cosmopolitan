/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:4;tab-width:8;coding:utf-8 -*-│
│ vi: set noet ft=c ts=4 sts=4 sw=4 fenc=utf-8                             :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright (c) 2008-2016 Stefan Krah. All rights reserved.                    │
│                                                                              │
│ Redistribution and use in source and binary forms, with or without           │
│ modification, are permitted provided that the following conditions           │
│ are met:                                                                     │
│                                                                              │
│ 1. Redistributions of source code must retain the above copyright            │
│    notice, this list of conditions and the following disclaimer.             │
│                                                                              │
│ 2. Redistributions in binary form must reproduce the above copyright         │
│    notice, this list of conditions and the following disclaimer in           │
│    the documentation and/or other materials provided with the                │
│    distribution.                                                             │
│                                                                              │
│ THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND         │
│ ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE        │
│ IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR           │
│ PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS         │
│ BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,          │
│ OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT         │
│ OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR           │
│ BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,        │
│ WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE         │
│ OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,            │
│ EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                           │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/assert.h"
#include "third_party/python/Modules/_decimal/libmpdec/bits.h"
#include "third_party/python/Modules/_decimal/libmpdec/difradix2.h"
#include "third_party/python/Modules/_decimal/libmpdec/fnt.h"
#include "third_party/python/Modules/_decimal/libmpdec/mpdecimal.h"
#include "third_party/python/Modules/_decimal/libmpdec/numbertheory.h"

asm(".ident\t\"\\n\\n\
libmpdec (BSD-2)\\n\
Copyright 2008-2016 Stefan Krah\"");
asm(".include \"libc/disclaimer.inc\"");

/* Bignum: Fast transform for medium-sized coefficients. */

/* forward transform, sign = -1 */
int
std_fnt(mpd_uint_t *a, mpd_size_t n, int modnum)
{
    struct fnt_params *tparams;
    assert(ispower2(n));
    assert(n >= 4);
    assert(n <= 3*MPD_MAXTRANSFORM_2N);
    if ((tparams = _mpd_init_fnt_params(n, -1, modnum)) == NULL) {
        return 0;
    }
    fnt_dif2(a, n, tparams);
    mpd_free(tparams);
    return 1;
}

/* reverse transform, sign = 1 */
int
std_inv_fnt(mpd_uint_t *a, mpd_size_t n, int modnum)
{
    struct fnt_params *tparams;
    assert(ispower2(n));
    assert(n >= 4);
    assert(n <= 3*MPD_MAXTRANSFORM_2N);
    if ((tparams = _mpd_init_fnt_params(n, 1, modnum)) == NULL) {
        return 0;
    }
    fnt_dif2(a, n, tparams);
    mpd_free(tparams);
    return 1;
}
