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
#include "libc/errno.h"
#include "libc/mem/mem.h"
#include "libc/str/str.h"
#include "libc/testlib/testlib.h"

TEST(PascalifyDnsName, testEmpty) {
  uint8_t *buf = malloc(1);
  char *name = strdup("");
  EXPECT_EQ(0, PascalifyDnsName(buf, 1, name));
  EXPECT_BINEQ(u" ", buf);
  free(name);
  free(buf);
}

TEST(PascalifyDnsName, testOneLabel) {
  uint8_t *buf = malloc(1 + 3 + 1);
  char *name = strdup("foo");
  EXPECT_EQ(1 + 3, PascalifyDnsName(buf, 1 + 3 + 1, name));
  EXPECT_BINEQ(u"♥foo ", buf);
  free(name);
  free(buf);
}

TEST(PascalifyDnsName, testTwoLabels) {
  uint8_t *buf = malloc(1 + 3 + 1 + 3 + 1);
  char *name = strdup("foo.bar");
  EXPECT_EQ(1 + 3 + 1 + 3, PascalifyDnsName(buf, 1 + 3 + 1 + 3 + 1, name));
  EXPECT_BINEQ(u"♥foo♥bar ", buf);
  free(name);
  free(buf);
}

TEST(PascalifyDnsName, testFqdnDot_isntIncluded) {
  uint8_t *buf = malloc(1 + 3 + 1 + 3 + 1);
  char *name = strdup("foo.bar.");
  EXPECT_EQ(1 + 3 + 1 + 3, PascalifyDnsName(buf, 1 + 3 + 1 + 3 + 1, name));
  EXPECT_BINEQ(u"♥foo♥bar ", buf);
  free(name);
  free(buf);
}

TEST(PascalifyDnsName, testTooLong) {
  uint8_t *buf = malloc(1);
  char *name = malloc(1000);
  memset(name, '.', 999);
  name[999] = '\0';
  EXPECT_EQ(-1, PascalifyDnsName(buf, 1, name));
  EXPECT_EQ(ENAMETOOLONG, errno);
  free(name);
  free(buf);
}

TEST(PascalifyDnsName, testNoSpace) {
  uint8_t *buf = malloc(1);
  char *name = strdup("foo");
  EXPECT_EQ(-1, PascalifyDnsName(buf, 1, name));
  EXPECT_EQ(ENOSPC, errno);
  free(name);
  free(buf);
}
