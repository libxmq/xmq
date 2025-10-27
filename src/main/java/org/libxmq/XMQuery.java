package org.libxmq;

import org.w3c.dom.Node;

public interface XMQuery
{
    /**
       xmqGetInt:
       @doc: the xmq doc object
       @xpath: the location of the content to be parsed as an 32 bit signed integer.
    */
    int xmqGetInt(String xpath);

    /**
       xmqGetIntRel:
       @doc: the xmq doc object
       @xpath: the location of the content to be parsed as an 32 bit signed integer.
       @relative: the xpath is search using this node as the starting point.
    */
    int xmqGetIntRel(String xpath, Node relative);

    /**
       xmqGetLong:
       @doc: the xmq doc object
       @xpath: the location of the content to be parsed as an 64 bit signed integer.
    */
    long xmqGetLong(String xpath);

    /**
       xmqGetLongRel:
       @doc: the xmq doc object
       @xpath: the location of the content to be parsed as an 64 bit signed integer.
       @relative: the xpath is search using this node as the starting point.
    */
    long xmqGetLongRel(String xpath, Node relative);

    /**
       xmqGetDouble:
       @doc: the xmq doc object
       @xpath: the location of the content to be parsed as double float.
    */
    double xmqGetDouble(String xpath);

    /**
       xmqGetDoubleRel:
       @doc: the xmq doc object
       @xpath: the location of the content to be parsed as double float.
       @relative: the xpath is search using this node as the starting point.
    */
    double xmqGetDoubleRel(String xpath, Node relative);

    /**
       xmqGetString:
       @doc: the xmq doc object
       @xpath: the location of the content to be parsed as string.
    */
    String xmqGetString(String xpath);

    /**
       xmqGetStringRel:
       @doc: the xmq doc object
       @xpath: the location of the content to be parsed as string.
       @relative: the xpath is search using this node as the starting point.
    */
    String xmqGetStringRel(String xpath, Node relative);

    /**
       xmqForeach: Find all locations matching the xpath.
       @xpath: the xpath pattern.
       @cb: the function to call for each found node. Can be NULL.
       @user_data: the user_data supplied to the function.

       Returns the number of hits.
    */
    int xmqForeach(String xpath, XMQNodeCallback cb, Object user_data);

    /**
       xmqForeachRel: Find all locations matching the xpath.
       @xpath: the xpath pattern.
       @cb: the function to call for each found node. Can be NULL.
       @user_data: the user_data supplied to the function.
       @relative: find nodes relative to this node.

       Returns the number of hits.
    */
    int xmqForeachRel(String xpath, XMQNodeCallback cb, Object user_data, Node relative);

}
