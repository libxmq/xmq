package org.libxmq;

public class SourcePos
{
    int i;
    int line;
    int col;

    /*
    void increment('/', 1, &i, &line, &col);
void increment(char c, size_t num_bytes, const char **i, size_t *line, size_t *col)
{
    if ((c & 0xc0) != 0x80) // Just ignore UTF8 parts since they do not change the line or col.
    {
        (*col)++;
	if (c == '\n')
        {
            (*line)++;
            (*col) = 1;
        }
    }
    assert(num_bytes > 0);
    (*i)+=num_bytes;
}
    */
}
