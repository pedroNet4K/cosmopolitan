/*-*-mode:c++;indent-tabs-mode:nil;c-basic-offset:4;tab-width:8;coding:utf-8-*-│
│ vi: set noet ft=c++ ts=4 sts=4 sw=4 fenc=utf-8                           :vi │
╚──────────────────────────────────────────────────────────────────────────────╝
│                                                                              │
│  radpajama.com                                                               │
│  Copyright (c) 2023 Ariel Núñez                                              │
│  Copyright (c) 2023 Georgi Gerganov                                          │
│                                                                              │
│  Permission is hereby granted, free of charge, to any person obtaining       │
│  a copy of this software and associated documentation files (the             │
│  "Software"), to deal in the Software without restriction, including         │
│  without limitation the rights to use, copy, modify, merge, publish,         │
│  distribute, sublicense, and/or sell copies of the Software, and to          │
│  permit persons to whom the Software is furnished to do so, subject to       │
│  the following conditions:                                                   │
│                                                                              │
│  The above copyright notice and this permission notice shall be              │
│  included in all copies or substantial portions of the Software.             │
│                                                                              │
│  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,             │
│  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF          │
│  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.      │
│  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY        │
│  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,        │
│  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE           │
│  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                      │
│                                                                              │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/log/log.h"
#include "third_party/ggml/ggml.h"
#include "third_party/ggml/llama_util.h"
#include "third_party/libcxx/cstdio"
#include "third_party/libcxx/map"
#include "third_party/libcxx/string"
#include "third_party/radpajama/gptneox.h"

static const std::map<std::string, enum gptneox_ftype> GPTNEOX_FTYPE_MAP = {
    {"f16", GPTNEOX_FTYPE_MOSTLY_F16},
    {"q4_0", GPTNEOX_FTYPE_MOSTLY_Q4_0},
    {"q4_1", GPTNEOX_FTYPE_MOSTLY_Q4_1},
    {"q4_2", GPTNEOX_FTYPE_MOSTLY_Q4_2},
    //{"q4_3", GPTNEOX_FTYPE_MOSTLY_Q4_3},
    {"q5_0", GPTNEOX_FTYPE_MOSTLY_Q5_0},
    {"q5_1", GPTNEOX_FTYPE_MOSTLY_Q5_1},
    {"q8_0", GPTNEOX_FTYPE_MOSTLY_Q8_0},
};

// usage:
//  ./quantize models/llama/ggml-model.bin models/llama/ggml-model-quant.bin type
//
int main(int argc, char ** argv) {
    verynice();
    ShowCrashReports();

    ggjt_v2();
    ggml_time_init();

    if (argc < 4) {
        fprintf(stderr, "usage: %s model-f32.bin model-quant.bin type [nthread]\n", argv[0]);
        for (auto it = GPTNEOX_FTYPE_MAP.begin(); it != GPTNEOX_FTYPE_MAP.end(); it++) {
            fprintf(stderr, "  type = \"%s\" or %d\n", it->first.c_str(), it->second);
        }
        return 1;
    }

    // needed to initialize f16 tables
    {
        struct ggml_init_params params = { 0, NULL, false };
        struct ggml_context * ctx = ggml_init(params);
        ggml_free(ctx);
    }

    const std::string fname_inp = argv[1];
    const std::string fname_out = argv[2];

    if (fname_inp == fname_out) {
        fprintf(stderr, "%s: input and output names are same\n", fname_inp.c_str());
        exit(1);
    }

    enum gptneox_ftype ftype;
    if (!is_integer_str(argv[3])) {
        auto it = GPTNEOX_FTYPE_MAP.find(argv[3]);
        if (it == GPTNEOX_FTYPE_MAP.end()) {
            fprintf(stderr, "%s: unknown ftype '%s'\n", __func__, argv[3]);
            return 1;
        }
        ftype = it->second;
    } else {
        ftype = (enum gptneox_ftype)atoi(argv[3]);
    }

    int nthread = argc > 4 ? atoi(argv[4]) : 0;

    const int64_t t_main_start_us = ggml_time_us();

    int64_t t_quantize_us = 0;

    // load the model
    {
        const int64_t t_start_us = ggml_time_us();

        if (gptneox_model_quantize(fname_inp.c_str(), fname_out.c_str(), ftype, nthread)) {
            fprintf(stderr, "%s: failed to quantize model from '%s'\n", __func__, fname_inp.c_str());
            return 1;
        }

        t_quantize_us = ggml_time_us() - t_start_us;
    }

    // report timing
    {
        const int64_t t_main_end_us = ggml_time_us();

        printf("\n");
        printf("%s: quantize time = %8.2f ms\n", __func__, t_quantize_us/1000.0);
        printf("%s:    total time = %8.2f ms\n", __func__, (t_main_end_us - t_main_start_us)/1000.0);
    }

    return 0;
}
