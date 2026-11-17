package org.libxmq.imp;

import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlDomApi;
import org.libxmq.imp.xml.XmlNode;
import org.libxmq.imp.xml.xsd10.Xsd10Build;
import org.libxmq.imp.xml.xsd10.Xsd10Error;
import org.libxmq.imp.xml.xsd10.Xsd10ParserCtxt;
import org.libxmq.imp.xml.xsd10.Xsd10Schema;
import org.libxmq.imp.xml.xsd10.Xsd10Schemas;
import org.libxmq.imp.xml.xsd10.Xsd10Valid;
import org.libxmq.imp.xml.xsd10.Xsd10ValidCtxt;

/**
 * A compiled xsd 1.0 schema, used to validate documents (XMQXsd, xmqCompileXsd,
 * xmqSetXsdMessageCallback, xmqValidate in the C version).
 */
public class Validate
{
    /** The status of xmqValidate: the document is valid. */
    public static final int OK = 0;
    /** XMQ_ERROR_XSD_COMPILE: the schema is not usable. */
    public static final int ERROR_XSD_COMPILE = 62;
    /** XMQ_ERROR_XSD_INVALID: the document is not valid. */
    public static final int ERROR_XSD_INVALID = 63;

    /** Receives the messages of compilation and validation (XMQXsdMessageCallback). */
    public interface MessageCallback
    {
        /**
         * A message.
         * @param user_data The user data.
         * @param message The message.
         * @param is_warning True for warnings.
         */
        void message(Object user_data, String message, boolean is_warning);
    }

    private Xsd10Schema schema;
    private XmlDocument dom;
    private MessageCallback callback;
    private Object user_data;
    private StringBuilder messages;

    private Validate()
    {
    }

    private void deliver(String message, boolean is_warning)
    {
        if (messages != null) messages.append(message);
        if (callback != null) callback.message(user_data, message, is_warning);
    }

    /** Like xmq_xsd_error/xmq_xsd_warning: called as cb(ctx, "%s", message). */
    private void vdeliver(boolean is_warning, String fmt, Object... args)
    {
        String m = fmt.equals("%s") && args.length == 1 ? String.valueOf(args[0]) : org.libxmq.imp.xml.CFormat.format(fmt, args);
        deliver(m, is_warning);
    }

    /** Like xmq_xsd_structured: print the formatted error on stderr. */
    private void structured(Xsd10Error err)
    {
        String m = Xsd10Schemas.formatErrorString(err);
        if (m == null) return;
        System.err.print(m);
        if (messages != null) messages.append(m);
    }

    /**
     * Compile a schema (xmqCompileXsd).
     * @param src The schema document.
     * @param callback The message callback, or null to print the messages on stderr.
     * @param user_data The user data of the callback.
     * @return The compiled schema, or null.
     */
    public static Validate compile(XmlDocument src, MessageCallback callback, Object user_data)
    {
        if (src == null) return null;
        Validate x = new Validate();
        x.callback = callback;
        x.user_data = user_data;
        // A copy of the schema document, it must live as long as the schema.
        x.dom = XmlDomApi.domNew();
        if (src.version() != null) x.dom.setVersion(src.version());
        if (src.encoding() != null) x.dom.setEncoding(src.encoding());
        if (src.url() != null) x.dom.setUrl(src.url());
        for (XmlNode c = src.firstChild(); c != null; c = c.next())
        {
            XmlNode copy = XmlDomApi.nodeCopy(x.dom, c, true);
            if (copy != null) XmlDomApi.nodeAppendChild(x.dom, copy);
        }
        x.messages = new StringBuilder();
        Xsd10ParserCtxt pctxt = Xsd10Schemas.newDocParserCtxt(x.dom);
        if (pctxt != null)
        {
            if (callback != null)
            {
                Xsd10Schemas.setParserErrors(pctxt,
                                             (ctx, fmt, args) -> x.vdeliver(false, fmt, args),
                                             (ctx, fmt, args) -> x.vdeliver(true, fmt, args), x);
            }
            else
            {
                Xsd10Schemas.setParserStructuredErrors(pctxt, (ud, err) -> x.structured(err), x);
            }
            x.schema = Xsd10Build.parse(pctxt);
            Xsd10Schemas.freeParserCtxt(pctxt);
        }
        x.messages = null;
        if (x.schema == null) return null;
        return x;
    }

    /**
     * Set the message callback used by validate (xmqSetXsdMessageCallback).
     * @param callback The callback.
     * @param user_data The user data.
     */
    public void setMessageCallback(MessageCallback callback, Object user_data)
    {
        this.callback = callback;
        this.user_data = user_data;
    }

    /**
     * Validate a document (xmqValidate).
     * @param doc The document.
     * @return OK, ERROR_XSD_COMPILE or ERROR_XSD_INVALID.
     */
    public int validate(XmlDocument doc)
    {
        if (schema == null) return ERROR_XSD_COMPILE;
        if (doc == null) return ERROR_XSD_INVALID;
        Xsd10ValidCtxt vctxt = Xsd10Valid.newValidCtxt(schema);
        if (vctxt == null) return ERROR_XSD_COMPILE;
        messages = new StringBuilder();
        if (callback != null)
        {
            Xsd10Schemas.setValidErrors(vctxt,
                                        (ctx, fmt, args) -> vdeliver(false, fmt, args),
                                        (ctx, fmt, args) -> vdeliver(true, fmt, args), this);
        }
        else
        {
            Xsd10Schemas.setValidStructuredErrors(vctxt, (ud, err) -> structured(err), this);
        }
        if (Xsd10Schemas.isValid(vctxt) == 0)
        {
            Xsd10Valid.freeValidCtxt(vctxt);
            messages = null;
            return ERROR_XSD_COMPILE;
        }
        int rc = Xsd10Valid.validateDoc(vctxt, doc);
        Xsd10Valid.freeValidCtxt(vctxt);
        messages = null;
        return rc != 0 ? ERROR_XSD_INVALID : OK;
    }
}
