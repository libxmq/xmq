// libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
//
// Runs the libFuzzer harness (fuzz_xmq.c) once on each file given on the command line,
// so that the fuzz regressions can be replayed with gcc and the sanitizers, without
// clang and libFuzzer. See tests/test_fuzz_regressions.sh.

#include<stdint.h>
#include<stdio.h>
#include<stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int main(int argc, char **argv)
{
    for (int a = 1; a < argc; a++)
    {
        FILE *f = fopen(argv[a], "rb");
        if (!f) { fprintf(stderr, "Cannot open %s\n", argv[a]); return 1; }
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        uint8_t *buf = (uint8_t*)malloc(n > 0 ? n : 1);
        size_t got = fread(buf, 1, n, f);
        fclose(f);
        LLVMFuzzerTestOneInput(buf, got);
        free(buf);
    }
    return 0;
}
