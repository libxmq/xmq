package org.libxmq;

public class XMQDoc
{
    /**
    xmqDetectContentType:
    @start: points to first byte of buffer to scan for content type
    @stop: points to byte after buffer

    Detect the content type xmq/xml/html/json by examining a few leading
    non-whitespace words/characters.
    */
    XMQContentType xmqDetectContentType(String s)
    {
        return XMQContentType.XMQ_CONTENT_XMQ;
    }

    boolean xmqParseBufferWithType(String s,
                                   String implicit_root,
                                   XMQContentType ct,
                                   int flags)
    {
        return false;
    }

    public int forEach(String xpath, XMQNodeCallback cb)
    {
        return 0;
    }

}
