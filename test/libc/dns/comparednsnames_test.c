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
#include "libc/calls/calls.h"
#include "libc/dns/dns.h"
#include "libc/mem/mem.h"
#include "libc/str/str.h"
#include "libc/testlib/testlib.h"

void SetUpOnce(void) {
  ASSERT_SYS(0, 0, pledge("stdio rpath", 0));
}

TEST(CompareDnsNames, testEmpty) {
  char *A = strcpy(malloc(1), "");
  char *B = strcpy(malloc(1), "");
  EXPECT_EQ(CompareDnsNames(A, B), 0);
  EXPECT_EQ(CompareDnsNames(A, A), 0);
  free(B);
  free(A);
}

TEST(CompareDnsNames, testDotless_caseInsensitiveBehavior) {
  char *A = malloc(2);
  char *B = malloc(2);
  EXPECT_EQ(CompareDnsNames(strcpy(A, "a"), strcpy(B, "a")), 0);
  EXPECT_EQ(CompareDnsNames(A, A), 0);
  EXPECT_EQ(CompareDnsNames(strcpy(A, "a"), strcpy(B, "A")), 0);
  EXPECT_EQ(CompareDnsNames(strcpy(A, "A"), strcpy(B, "a")), 0);
  EXPECT_LT(CompareDnsNames(strcpy(A, "a"), strcpy(B, "b")), 0);
  EXPECT_LT(CompareDnsNames(strcpy(A, "a"), strcpy(B, "B")), 0);
  EXPECT_GT(CompareDnsNames(strcpy(A, "d"), strcpy(B, "a")), 0);
  free(B);
  free(A);
}

TEST(CompareDnsNames, testMultiLabel_lexiReverse) {
  char *A = malloc(16);
  char *B = malloc(16);
  EXPECT_EQ(CompareDnsNames(strcpy(A, "a.example"), strcpy(B, "a.example")), 0);
  EXPECT_GT(CompareDnsNames(strcpy(A, "b.example"), strcpy(B, "a.example")), 0);
  EXPECT_LT(CompareDnsNames(strcpy(A, "b.example"), strcpy(B, "a.examplz")), 0);
  EXPECT_GT(CompareDnsNames(strcpy(A, "a.zxample"), strcpy(B, "a.examplz")), 0);
  EXPECT_EQ(CompareDnsNames(strcpy(A, "c.a.example"), strcpy(B, "c.a.example")),
            0);
  EXPECT_GT(CompareDnsNames(strcpy(A, "d.a.example"), strcpy(B, "c.a.example")),
            0);
  EXPECT_LT(CompareDnsNames(strcpy(A, "cat.example"), strcpy(B, "lol.example")),
            0);
  free(B);
  free(A);
}

TEST(CompareDnsNames, testTldDotQualifier_canBeEqualToDottedNames) {
  char *A = malloc(16);
  char *B = malloc(16);
  EXPECT_EQ(
      CompareDnsNames(strcpy(B, "aaa.example."), strcpy(A, "aaa.example")), 0);
  free(B);
  free(A);
}

TEST(CompareDnsNames, testFullyQualified_alwaysComesFirst) {
  char *A = malloc(16);
  char *B = malloc(16);
  EXPECT_LT(CompareDnsNames(strcpy(B, "aaa.example."), strcpy(A, "zzz")), 0);
  EXPECT_LT(CompareDnsNames(strcpy(B, "zzz.example."), strcpy(A, "aaa")), 0);
  EXPECT_GT(CompareDnsNames(strcpy(A, "zzz"), strcpy(B, "aaa.example.")), 0);
  EXPECT_GT(CompareDnsNames(strcpy(A, "aaa"), strcpy(B, "zzz.example.")), 0);
  free(B);
  free(A);
}

TEST(CompareDnsNames, testLikelySld_alwaysComesBeforeLocalName) {
  char *A = malloc(16);
  char *B = malloc(16);
  EXPECT_LT(CompareDnsNames(strcpy(B, "z.e"), strcpy(A, "a")), 0);
  EXPECT_LT(CompareDnsNames(strcpy(B, "aaa.example"), strcpy(A, "zzz")), 0);
  EXPECT_LT(CompareDnsNames(strcpy(B, "zzz.example"), strcpy(A, "aaa")), 0);
  EXPECT_GT(CompareDnsNames(strcpy(A, "zzz"), strcpy(B, "aaa.example")), 0);
  EXPECT_GT(CompareDnsNames(strcpy(A, "aaa"), strcpy(B, "zzz.example")), 0);
  free(B);
  free(A);
}

TEST(CompareDnsNames, testLikelySubdomain_alwaysComesAfterSld) {
  char *A = malloc(16);
  char *B = malloc(16);
  EXPECT_LT(CompareDnsNames(strcpy(B, "a.e"), strcpy(A, "z.a.e")), 0);
  EXPECT_GT(CompareDnsNames(strcpy(A, "z.a.e"), strcpy(B, "a.e")), 0);
  EXPECT_LT(CompareDnsNames(strcpy(B, "b.e"), strcpy(A, "a.b.e")), 0);
  EXPECT_GT(CompareDnsNames(strcpy(A, "a.b.e"), strcpy(B, "b.e")), 0);
  free(B);
  free(A);
}
