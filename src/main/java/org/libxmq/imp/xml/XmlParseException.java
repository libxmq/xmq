/* The error message format reproduces the libxml2 error output.
   libxml2: Copyright (C) 1998-2012 Daniel Veillard, Copyright (C) The Libxml2 Contributors, MIT license. */


package org.libxmq.imp.xml;

/**
 * Thrown when xml cannot be parsed. The message is formatted like the
 * libxml2 error output, ie:
 * <pre>
 * file.xml:1: parser error : Entity 'foo' not defined
 * &lt;a&gt;&amp;foo;&lt;/a&gt;
 *         ^
 * </pre>
 */
public class XmlParseException extends Exception
{
    private static final long serialVersionUID = 1L;

    /** The line where the error was found, starting at 1. */
    public final int line;

    /** The error message without the source context. */
    public final String error;

    /**
     * Create an exception.
     * @param full The full formatted message, with the source context.
     * @param error The error message itself.
     * @param line The line number.
     */
    public XmlParseException(String full, String error, int line)
    {
        super(full);
        this.error = error;
        this.line = line;
        this.bytes = full.getBytes(java.nio.charset.StandardCharsets.UTF_8);
    }

    /**
     * Create an exception from a message formatted as bytes. Like the C version the
     * source context is cut at a byte position, which can split a UTF-8 character.
     * @param full The full formatted message, with the source context.
     * @param error The error message itself.
     * @param line The line number.
     */
    public XmlParseException(byte[] full, String error, int line)
    {
        super(new String(full, java.nio.charset.StandardCharsets.UTF_8));
        this.error = error;
        this.line = line;
        this.bytes = full;
    }

    /** The full formatted message as bytes, print these to get exactly the C output. */
    public final byte[] bytes;

    /** Print the message bytes unchanged to stderr. */
    public void print()
    {
        System.err.write(bytes, 0, bytes.length);
        System.err.flush();
    }
}
