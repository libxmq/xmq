/* Print log lines in xmq with xmqLinePrintf. Each key is followed by a printf
   format and its arguments, the values are quoted as needed. The same line can
   instead be printed human readable. */

#include "xmq.h"

#include <stdio.h>
#include <stdlib.h>

int main()
{
    int num_wheels = 36;

    XMQLineConfig *lc = xmqNewLineConfig();

    char *line = xmqLinePrintf(lc,
                               "car{",
                               "nw=", "%d", num_wheels,
                               "model=", "%s %d", "car go ", 3,
                               "description=", "%s", "howdy\ndowdy",
                               "more=", "'''%s'''", "===",
                               "key=", "",
                               "}");
    printf("%s\n", line);
    free(line);

    line = xmqLinePrintf(lc, "work=", "pi is %f", 3.141590);
    printf("%s\n", line);
    free(line);

    xmqSetLineHumanReadable(lc, true);
    line = xmqLinePrintf(lc, "work=", "pi is %f", 3.141590);
    printf("%s\n", line);
    free(line);

    xmqFreeLineConfig(lc);
    return 0;
}
