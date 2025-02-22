/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set noet ft=c ts=2 sts=2 sw=2 fenc=utf-8                             :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2020 Justine Alexandra Roberts Tunney                              │
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
#include "libc/dns/consts.h"
#include "libc/dns/dns.h"
#include "libc/dns/hoststxt.h"
#include "libc/mem/alg.h"
#include "libc/sock/sock.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/af.h"
#include "libc/sysv/errfuns.h"

/**
 * Finds address associated with name in HOSTS.TXT table.
 *
 * @param ht can be GetHostsTxt()
 * @param af can be AF_INET, AF_UNSPEC
 * @param name can be a local or fully-qualified hostname
 * @param addr should point to a struct sockaddr_in; if this function
 *     succeeds, its sin_family and sin_addr fields will be modified
 * @param addrsize is the byte size of addr
 * @param canon be used to return a pointer to the canonical name
 * @return number of matches found, or -1 w/ errno
 * @error EAFNOSUPPORT
 */
int ResolveHostsTxt(const struct HostsTxt *ht, int af, const char *name,
                    struct sockaddr *addr, uint32_t addrsize,
                    const char **canon) {
  size_t i;
  struct sockaddr_in *addr4;
  if (af != AF_INET && af != AF_UNSPEC) return eafnosupport();
  for (i = 0; i < ht->entries.i; ++i) {
    if (!CompareDnsNames(name, ht->strings.p + ht->entries.p[i].name)) {
      if (addr) {
        if (addrsize < kMinSockaddr4Size) return einval();
        addr4 = (struct sockaddr_in *)addr;
        addr4->sin_family = AF_INET;
        memcpy(&addr4->sin_addr.s_addr, &ht->entries.p[i].ip[0], 4);
      }
      if (canon) {
        *canon = ht->strings.p + ht->entries.p[i].canon;
      }
      return 1;
    }
  }
  return 0;
}
