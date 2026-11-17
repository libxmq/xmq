/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

/* Portions of this file are ported from libxslt (version 1.1.45 as found in the
   libxmq fork in 3rdparty/libxslt). libxslt is covered by the following notice
   (from libxslt/Copyright):

 Copyright (C) 2001-2002 Daniel Veillard.  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is fur-
nished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
DANIEL VEILLARD BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CON-
NECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Daniel Veillard shall not
be used in advertising or otherwise to promote the sale, use or other deal-
ings in this Software without prior written authorization from him.
*/

package org.libxmq.imp.xml.libxslt1;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.libxmq.imp.xml.XPath;
import org.libxmq.imp.xml.XPathEvalContext;
import org.libxmq.imp.xml.XPathResult;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlNode;
import org.libxmq.imp.xml.XmlNs;

/**
 * The settings of the xslt engine (XmlLibxslt1Settings in the C version): where messages go
 * and how documents are loaded.
 */
public final class Settings
{
    /** Receives the error messages, formatted like libxslt prints them. */
    public interface MessageCallback
    {
        /**
         * A message.
         * @param message The message (with its newline).
         */
        void message(String message);
    }

    /** Loads and parses the document at uri, null if it cannot be loaded. The document gets uri as its url. */
    public interface Loader
    {
        /**
         * Load a document.
         * @param uri The uri.
         * @param type What it is loaded for, LOAD_START, LOAD_STYLESHEET or LOAD_DOCUMENT.
         * @return The document or null.
         */
        XmlDocument load(String uri, int type);
    }

    /** The main stylesheet. */
    public static final int LOAD_START = 0;
    /** xsl:import and xsl:include. */
    public static final int LOAD_STYLESHEET = 1;
    /** document(). */
    public static final int LOAD_DOCUMENT = 2;

    /** null prints the messages on stderr, like libxslt. */
    public MessageCallback message_callback;
    /** null reads files and parses them with entities expanded, like libxslt XSLT_PARSE_OPTIONS. */
    public Loader loader;

    /** The default settings (xml_libxslt1_settings_default). */
    public Settings()
    {
    }
}
