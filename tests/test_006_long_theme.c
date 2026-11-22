#include<xmq.h>
#include"test.h"

// A theme spec of 2048 characters, without a comma, overflowed the stack buffer
// it was copied into by one byte. Longer specs are rejected.

int main(int argc, char **argv)
{
    char spec[4096];
    for (int len = 2040; len < 2060; ++len)
    {
        for (int comma = 0; comma < 3; ++comma)
        {
            memset(spec, 'a', len);
            spec[len] = 0;
            if (comma == 1) spec[0] = ',';
            if (comma == 2) spec[len-1] = ',';
            XMQOutputSettings *os = xmqNewOutputSettings();
            xmqSetRenderTheme(os, spec);
            xmqSetupDefaultColors(os);
            xmqFreeOutputSettings(os);
        }
    }
    return 0;
}
