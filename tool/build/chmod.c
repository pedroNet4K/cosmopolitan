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
#include "libc/calls/calls.h"
#include "libc/errno.h"
#include "libc/fmt/conv.h"
#include "libc/fmt/magnumstrs.internal.h"
#include "libc/runtime/runtime.h"
#include "libc/stdio/stdio.h"
#include "libc/str/str.h"
#include "third_party/getopt/getopt.internal.h"

#define USAGE \
  " OCTAL DST...\n\
\n\
SYNOPSIS\n\
\n\
  Changes File Mode Bits\n\
\n\
FLAGS\n\
\n\
  -h      help\n\
\n"

const char *prog;

static wontreturn void PrintUsage(int fd, int rc) {
  tinyprint(fd, "USAGE\n\n  ", prog, USAGE, NULL);
  exit(rc);
}

static void GetOpts(int argc, char *argv[]) {
  int opt;
  while ((opt = getopt(argc, argv, "h")) != -1) {
    switch (opt) {
      case 'h':
        PrintUsage(1, 0);
      default:
        PrintUsage(2, 1);
    }
  }
}

int main(int argc, char *argv[]) {
  int i, mode;
  char *endptr;
  prog = argv[0];
  if (!prog) prog = "chmod";
  GetOpts(argc, argv);
  if (argc - optind < 2) {
    tinyprint(2, prog, ": missing operand\n", NULL);
    exit(1);
  }
  mode = strtol(argv[optind], &endptr, 8) & 07777;
  if (*endptr) {
    tinyprint(2, prog, ": invalid mode octal\n", NULL);
    exit(1);
  }
  for (i = optind + 1; i < argc; ++i) {
    if (chmod(argv[i], mode) == -1) {
      perror(argv[i]);
      exit(1);
    }
  }
  return 0;
}
