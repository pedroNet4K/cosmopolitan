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
#include "libc/dns/dns.h"
#include "libc/dns/dnsquestion.h"
#include "libc/sysv/errfuns.h"

/**
 * Serializes DNS question record to wire.
 *
 * @return number of bytes written
 * @see pascalifydnsname()
 */
int SerializeDnsQuestion(uint8_t *buf, size_t size,
                         const struct DnsQuestion *dq) {
  int wrote;
  if ((wrote = PascalifyDnsName(buf, size, dq->qname)) == -1) return -1;
  if (wrote + 1 + 4 > size) return enospc();
  buf[wrote + 1] = dq->qtype >> 8;
  buf[wrote + 2] = dq->qtype;
  buf[wrote + 3] = dq->qclass >> 8;
  buf[wrote + 4] = dq->qclass;
  return wrote + 5;
}
