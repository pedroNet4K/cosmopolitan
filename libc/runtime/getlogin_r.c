/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set noet ft=c ts=2 sts=2 sw=2 fenc=utf-8                             :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2022 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/calls/syscall_support-nt.internal.h"
#include "libc/dce.h"
#include "libc/intrin/strace.internal.h"
#include "libc/macros.internal.h"
#include "libc/nt/accounting.h"
#include "libc/runtime/runtime.h"
#include "libc/str/str.h"
#include "libc/sysv/errfuns.h"

int sys_getlogin(char *, uint32_t);

/**
 * Returns login name.
 */
int getlogin_r(char *buf, size_t size) {
  int rc;
  uint32_t n32;
  const char *p;
  char16_t buf16[257];
  if (IsBsd()) {
    rc = sys_getlogin(buf, size);
  } else if (IsWindows()) {
    n32 = ARRAYLEN(buf16);
    if (GetUserName(&buf16, &n32)) {
      tprecode16to8(buf, sizeof(size), buf16);
      rc = 0;
    } else {
      rc = __winerr();
    }
  } else if ((p = getenv("LOGNAME"))) {
    if (strlen(p) < size) {
      strcpy(buf, p);
      rc = 0;
    } else {
      rc = enomem();
    }
  } else {
    rc = enoent();
  }
  STRACE("getlogin_r([%#s], %'zu) → %d% m", rc != -1 ? buf : "n/a", size, rc);
  return rc;
}
