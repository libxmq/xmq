
package org.libxmq.imp;

/** A Line stores the index of the first (start) char of the line
    and stores the index of the char efter the last char of the line (stop).
    The (cr)nl is not included within start-stop. If the line ends with
    a newline, then nl is true.
*/
record Line(int start, int stop, int indent, boolean trim, boolean nl) { }
