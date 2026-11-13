// Test driver for xmq_cli_join_argv: prints the joined command line.
#include <stdio.h>
#include "../src/main/c/xmq-cli-join.h"
int main(int argc, const char **argv)
{
    char *line = xmq_cli_join_argv(argc, argv);
    printf("%s\n", line);
    free(line);
    return 0;
}
