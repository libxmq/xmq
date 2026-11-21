/* libxmq - Copyright (C) 2023-2026 Fredrik Öhrström (spdx: MIT)

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

/**
 * @mainpage libxmq
 *
 * libxmq is a library for working with xml/json and xmq.
 *
 *
 * @section getting_started Getting started
 *
 * ...
 *
 * @section api API overview
 *
 * See @ref documents and @ref nodes.
 */

#ifndef XMQ_H
#define XMQ_H

// Parts of the implementation can be excluded when building libxmq (eg -DXMQ_NO_JSON), to
// harden a build by leaving out code that is not needed (and to make it smaller). The api
// functions of an excluded part remain, they return XMQ_ERROR_EXCLUDED_FROM_BUILD (or fail
// like an error of that part), the rest of its implementation is not compiled in, so no
// input can reach it. xmqFeatures() (xmq --features) lists the parts that are compiled in.
// #define XMQ_NO_XMQ_PRINTING  No printing as xmq/htmq, nor rendering (render-terminal/html/tex).
// #define XMQ_NO_JSON          No json parsing and printing.
// #define XMQ_NO_YAML          No yaml parsing and printing.
// #define XMQ_NO_LIBXSLT       No xslt (xmqCompileXslt, xmqTransform...).
// #define XMQ_NO_XSD           No xsd validation (xmqCompileXsd, xmqValidate...).
// #define XMQ_NO_IXML          No ixml (the yaep engine, ixml grammars, xmqParseBufferWithIXML...).
// #define XMQ_NO_HTML          No html parsing and printing (also not the xslt html output method),
//                              htmq and render-html are xmq and are not affected.

#define _hideLBfromEditor {
#define _hideRBfromEditor }

#ifdef __cplusplus
// Needed for the XMQ_ATTRS macro.
#include <initializer_list>

extern "C" _hideLBfromEditor
#endif

#include<stdarg.h>
#include<stdbool.h>
#include<stdlib.h>
#include<stdint.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////// TYPES and STRUCTURES ///////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
   Opaque structure storing a document.
*/
typedef struct XMQDoc XMQDoc;

/**
   Opaque structure storing a node in the document.
   A node can be an element, text or entity.
   An element containing only text and entities is usually displayed as a key=value.
*/
typedef struct XMQNode XMQNode;

/**
   Opaque structure storing an attribute in the document.
*/
typedef struct XMQAttr XMQAttr;

/**
    An opaque structure to maintain the parse state and the list of callbacks
    to be invoked when parsing xmq.
*/
typedef struct XMQParseState XMQParseState;

/**
    Store your own parse callbacks into this structure and register it
    with the XMQParseState. Then you will get your own callbacks when parsing XMQ
    and using these callbacks you can implement your own document builder or token handler.
*/
typedef struct XMQParseCallbacks XMQParseCallbacks;

/**
   Opaque structure storing a output settings when printing xmq, xml or json.
*/
typedef struct XMQOutputSettings XMQOutputSettings;

/**
   @brief Specifies the color mode when rendering output that can be
   colorized (terminal, html and tex).
*/
typedef enum
{
    /** Render without any color. Output is plain monochrome. */
    XMQ_MONO = 0,
    /** Render with color, assuming a dark background. The dark background mode is forced. */
    XMQ_BG_DARK = 1,
    /** Render with color, assuming a light background. The light background mode is forced. */
    XMQ_BG_LIGHT = 2,
    /** Render with color and decide the background automatically,
       defaulting to a dark background. For html this means the rendered
       page follows the browser dark/light mode preference. */
    XMQ_BG_AUTO = 3,
} XMQColorMode;

/**
   @brief Specify the file/buffer content type both for input and for output.
*/
typedef enum
{
    /** Unknown buffer content. */
    XMQ_CONTENT_UNKNOWN = 0,
    /** Try to detect buffer content, is it xmq, xml or json? */
    XMQ_CONTENT_DETECT = 1,
    /** Content is xmq. */
    XMQ_CONTENT_XMQ = 2,
    /** Content is htmq. */
    XMQ_CONTENT_HTMQ = 3,
    /** Content is xml. */
    XMQ_CONTENT_XML = 4,
    /** Content is html. */
    XMQ_CONTENT_HTML = 5,
    /** Content is json. */
    XMQ_CONTENT_JSON = 6,
    /** Content is ixml. */
    XMQ_CONTENT_IXML = 7,
    /** Content is text. */
    XMQ_CONTENT_TEXT = 8,
    /** Cline content looks like: xpath="c-escaped string" */
    XMQ_CONTENT_CLINES = 9,
    /** Content is YAML. */
    XMQ_CONTENT_YAML = 10
} XMQContentType;

/**
   @brief XMQRenderFormat decides how to format the xmq output:
   PLAIN, or for human consumption in TERMINAL, HTML, HTMQ, TEX.
*/
typedef enum
{
    /** Normal output for data storage. */
    XMQ_RENDER_PLAIN = 0,
    /** Colorize output using ansi codes. */
    XMQ_RENDER_TERMINAL = 1,
    /** Colorize output using html tags. */
    XMQ_RENDER_HTML = 2,
    /** Colorize output using htmq tags. */
    XMQ_RENDER_HTMQ = 3,
    /** Colorize using latex. */
    XMQ_RENDER_TEX = 4
} XMQRenderFormat;

/**
    The flag bits specify by the parser builds the document.

    If a 0 is provided as the flag bits to the parse functions,
    then it will parse using the these default settings:

    When loading xml/html:
        trim the whitespace from the input to generate the most likely desired xmq output.
        merge character entities

    When loading xmq/htmq:
        no trimming but
        merge character entities such as &#10; and consecutive text quotes

    If you load xml with XMQ_TRIM_NONE (--trim=none) there will be a lot of unnecessary whitespace stored in
    the xmq, like &#32;&#9;&#10; etc.
    You can then view the xmq with XMQ_TRIM_HEURISTIC (--trim=heuristic) to drop the whitespace.

    If you load xmq with --nomerge then character entities and separate text blocks will be kept as is.
    The --nomerge currently does not work for XML/HTML since libxml2 does not have a setting for merge,
    it always merges.
*/
typedef enum
{
    /** Do not trim any whitespace. Only relevant when parsing xml. */
    XMQ_FLAG_TRIM_NONE = 1,
    /** Remove leading/ending whitespace, but try to keep significant, remove incidental indentation. */
    XMQ_FLAG_TRIM_HEURISTIC = 2,
    /** Not implemented. */
    XMQ_FLAG_TRIM_EXACT = 4,
    /** Do not merge adjacent text nodes and character entity nodes. */
    XMQ_FLAG_NOMERGE = 8,
    /** When ixml parse is ambiguous generate all parses. */
    XMQ_FLAG_IXML_ALL_PARSES = 16,
    /** When ixml parse fails, try to recover. */
    XMQ_FLAG_IXML_TRY_TO_RECOVER = 32,
    /** If the ixml parse fails generate an empty document and no errors. */
    XMQ_FLAG_IXML_FAIL_SILENT = 64,
    /** Record a trace of the ixml parse (the grammar, the Earley items and the tree), see xmqTakeIXMLTrace. */
    XMQ_FLAG_IXML_TRACE = 128,
    /** Find what could follow the input, for completion, see xmqTakeIXMLCompletions. */
    XMQ_FLAG_IXML_COMPLETE = 256,
} XMQFlagBits;

/**
   XMQColorName is used to color the output when pretty printing xmq.
*/
typedef enum XMQColorName {
    /** Comment. */
    XMQ_COLOR_C,
    /** Quote. */
    XMQ_COLOR_Q,
    /** Entity. */
    XMQ_COLOR_E,
    /** Name Space (both for element and attribute). */
    XMQ_COLOR_NS,
    /** Element name. */
    XMQ_COLOR_EN,
    /** Element key. */
    XMQ_COLOR_EK,
    /** Element key value. */
    XMQ_COLOR_EKV,
    /** Attribute key. */
    XMQ_COLOR_AK,
    /** Attribute key value. */
    XMQ_COLOR_AKV,
    /** Compound Parentheses. */
    XMQ_COLOR_CP,
    /** Name Space Declaration xmlns. */
    XMQ_COLOR_NSD,
    /** Unicode whitespace. */
    XMQ_COLOR_UW,
    /** Override xls prefixed element names with this color. */
    XMQ_COLOR_XLS,
} XMQColorName;

/**
   @brief XMQMoveTo decides where a node is moved to, in a new parent element and
   potentially in relation to another sibling node.
*/
typedef enum
{
    /** Place the element first in the parents list of childrens. */
    XMQ_MOVE_FIRST = 0,
    /** Place the element last in the parents list of children. */
    XMQ_MOVE_LAST = 1,
    /** Place the element before the element referenced. */
    XMQ_MOVE_BEFORE = 2,
    /** Place the element after the element referenced. */
    XMQ_MOVE_AFTER = 3,
} XMQMoveTo;

/**
    A reader that the xmq parser invokes to fetch more data. The read function stores
    data in the buffer (start <= i < stop) and returns the number of bytes stored,
    at most stop-start.

    You can create your own reader with a function that takes a pointer to the reader state.
*/
struct XMQReader
{
    /** The reader_state is passed to the read function. */
    void *reader_state;
    /** The function to be invoked from the parser to fetch more data to parse. */
    size_t (*read)(void *reader_state, char *start, char *stop);
};
typedef struct XMQReader XMQReader;

/**
    You can pass your own xmq_writer to the printer routines to do your own final output.
    Any function implementing XMQWrite must handle stop == NULL.

    @param writer_state Your own writer_state supplied to the printing function.
    @param start Start of buffer to write.
    @param stop Points to byte after buffer to write. If NULL then assume start is null terminated.
*/
typedef bool (*XMQWrite)(void *writer_state, const char *start, const char *stop);

/**
    The xmq printer uses the writer to write data supplied from a buffer (start <= i < stop).
    You can create your own writer with a function that takes a pointer to the writer state.
    The writer function must return true if the writing was successful.
*/
struct XMQWriter
{
    /** The writer_state is passed to the write function. */
    void *writer_state;
    /** The function to be invoked from the printer to write output data. */
    XMQWrite write;
};
typedef struct XMQWriter XMQWriter;

/**
    The XMQProceed is used to proceed or stop when iterating over xmq nodes.
*/
typedef enum
{
    /** Return XMQ_CONTINUE to continue iterating over xmq nodes. */
    XMQ_CONTINUE,
    /** Return XMQ_STOP to stop iterating. */
    XMQ_STOP,
} XMQProceed;

/**
    The function type which is called by foreach functions.

    @param doc The document being processed.
    @param node The node triggering the callback.
    @param user_data The user data supplied to for_each.
*/
typedef XMQProceed (*XMQNodeCallback)(XMQDoc *doc, XMQNode *node, void *user_data);

/**
    The xmq functions return OK or error values using the XMQStatus.
*/
typedef enum
{
    /** No error. */
    XMQ_OK = 0,
    /** File not found or cannot be opened for reading. */
    XMQ_ERROR_CANNOT_READ_FILE = 1,
    /** Out of memory. */
    XMQ_ERROR_OOM = 2,
    /** Expected xmq but auto detect sees early that it is not xmq. */
    XMQ_ERROR_NOT_XMQ = 3,
    /** An xmq quote is not closed, ie single quotes are missing. */
    XMQ_ERROR_QUOTE_NOT_CLOSED = 4,
    /** An entity is missing the semicolon. */
    XMQ_ERROR_ENTITY_NOT_CLOSED = 5,
    /** A comment has not been closed. */
    XMQ_ERROR_COMMENT_NOT_CLOSED = 6,
    /** A comment close is not balanced. */
    XMQ_ERROR_COMMENT_CLOSED_WITH_TOO_MANY_SLASHES = 7,
    /** A body is missing a closing brace. */
    XMQ_ERROR_BODY_NOT_CLOSED = 8,
    /** The attribute list is missing the closing parentheses. */
    XMQ_ERROR_ATTRIBUTES_NOT_CLOSED = 9,
    /** Compound content is missing the closing double parentheses. */
    XMQ_ERROR_COMPOUND_NOT_CLOSED = 10,
    /** Compound content may only contains quotes and entities. */
    XMQ_ERROR_COMPOUND_MAY_NOT_CONTAIN = 11,
    /** Too many closing single quotes. */
    XMQ_ERROR_QUOTE_CLOSED_WITH_TOO_MANY_QUOTES = 12,
    /** An unexpected closing brace. */
    XMQ_ERROR_UNEXPECTED_CLOSING_BRACE = 13,
    /** Expected a value after equals. */
    XMQ_ERROR_EXPECTED_CONTENT_AFTER_EQUALS = 14,
    /** Tabs are not permitted as token separators. */
    XMQ_ERROR_UNEXPECTED_TAB = 15,
    /** An invalid character found. */
    XMQ_ERROR_INVALID_CHAR = 16,
    /** The doctype could not be parsed. */
    XMQ_ERROR_BAD_DOCTYPE = 17,
    /** An invalid json escape sequence. */
    XMQ_ERROR_JSON_INVALID_ESCAPE = 18,
    /** An invalid json character. */
    XMQ_ERROR_JSON_INVALID_CHAR = 19,
    /** The XMl parser has been left out to shrink code size. */
    XMQ_ERROR_CANNOT_HANDLE_XML = 20,
    /** The HTML parser has been left out to shrink code size. */
    XMQ_ERROR_CANNOT_HANDLE_HTML = 21,
    /** The JSON parser has been left out to shrink code size. */
    XMQ_ERROR_CANNOT_HANDLE_JSON = 22,
    /** Expecte xmq but was given something else. */
    XMQ_ERROR_EXPECTED_XMQ = 23,
    /** Expecte htmq but was given something else. */
    XMQ_ERROR_EXPECTED_HTMQ = 24,
    /** Expecte xml but was given something else. */
    XMQ_ERROR_EXPECTED_XML = 25,
    /** Expecte html but was given something else. */
    XMQ_ERROR_EXPECTED_HTML = 26,
    /** Expecte json but was given something else. */
    XMQ_ERROR_EXPECTED_JSON = 27,
    /** Error while parsing xml. */
    XMQ_ERROR_PARSING_XML = 28,
    /** Error while parsing html. */
    XMQ_ERROR_PARSING_HTML = 29,
    /** A value after a key cannot start with comments or equals. */
    XMQ_ERROR_VALUE_CANNOT_START_WITH = 30,
    /** Not a proper uri for a namespace. */
    XMQ_ERROR_INVALID_NAMESPACE_URI = 31,
    /** Not a proper prefix for a namespace. */
    XMQ_ERROR_INVALID_NAMESPACE_PREFIX = 32,
    /** When adding a new namespace with a prefix, the prefix has already been used. */
    XMQ_ERROR_NAMESPACE_PREFIX_ALREADY_TAKEN = 33,
    /** Pointer errors to buffers are wrong. */
    XMQ_ERROR_BAD_RANGE = 35,
    /** Invalid enums provided or other bad input to functions. */
    XMQ_ERROR_BAD_VALUE = 36,
    /** An embedded xml fragment inside an xmq document is not closed. */
    XMQ_ERROR_XML_FRAGMENT_NOT_CLOSED = 37,
    /** An entity could not be expanded: a loop, too deep nesting, too large expansion or malformed replacement text. */
    XMQ_ERROR_ENTITY_EXPANSION = 38,
    /** The input is not valid UTF-8. */
    XMQ_ERROR_INVALID_UTF8 = 39,
    /** The elements, objects or arrays are nested deeper than XMQ_MAX_NESTING_DEPTH. */
    XMQ_ERROR_TOO_DEEP = 40,
    /** The ixml grammar cannot be parsed. */
    XMQ_ERROR_IXML_SYNTAX_ERROR = 50,
    /** The xslt stylesheet cannot be compiled. */
    XMQ_ERROR_XSLT_COMPILE = 60,
    /** The xslt transformation failed. */
    XMQ_ERROR_XSLT_TRANSFORM = 61,
    /** The xsd schema cannot be compiled. */
    XMQ_ERROR_XSD_COMPILE = 62,
    /** The document is not valid according to the xsd schema. */
    XMQ_ERROR_XSD_INVALID = 63,
    /** The feature was excluded when libxmq was built (eg XMQ_NO_JSON). */
    XMQ_ERROR_EXCLUDED_FROM_BUILD = 64,
    /** Warning, when parse succeeds, but it seems there is a mistake. */
    XMQ_WARNING_QUOTES_NEEDED = 1000
} XMQStatus;

/**
    The status and the document returned by a function that creates a document.
    The document is only valid if the status is XMQ_OK.
*/
struct XMQReturnDoc
{
    XMQStatus status;
    XMQDoc    *doc;
};
typedef struct XMQReturnDoc XMQReturnDoc;

/**
    The status and the node returned by a function that creates a node.
    The node is only valid if the status is XMQ_OK.
*/
struct XMQReturnNode
{
    XMQStatus status;
    XMQNode   *node;
};
typedef struct XMQReturnNode XMQReturnNode;

/**
    The status and the attribute returned by a function that creates an attribute.
    The attribute is only valid if the status is XMQ_OK.
*/
struct XMQReturnAttr
{
    XMQStatus status;
    XMQAttr   *attr;
};
typedef struct XMQReturnAttr XMQReturnAttr;

/**
    The status and a string, that the caller frees, returned by a function.
    The string is only valid if the status is XMQ_OK.
*/
struct XMQReturnString
{
    XMQStatus status;
    char *string;
};
typedef struct XMQReturnString XMQReturnString;

/**
    The status and a string, owned by the library, returned by a function.
    The string is only valid if the status is XMQ_OK.
*/
struct XMQReturnConstString
{
    XMQStatus status;
    const char *string;
};
typedef struct XMQReturnConstString XMQReturnConstString;

/**
    When loading xmq/xml/json as a config file, the content is parsed and decoded
    according the the requested type. These are the available core types.
*/
typedef enum
{
    XMQ_CORE_BOOL,
    /** Signed 8 bit integer. Can be decimal, hex (0x) or octal (0). */
    XMQ_CORE_I8,
    /** Signed 16 bit integer. Can be decimal, hex (0x) or octal (0). */
    XMQ_CORE_I16,
    /** Signed 32 bit integer. Can be decimal, hex (0x) or octal (0). */
    XMQ_CORE_I32,
    /** Signed 64 bit integer. Can be decimal, hex (0x) or octal (0). */
    XMQ_CORE_I64,
    /** Signed 128 bit integer. Can be decimal, hex (0x) or octal (0). */
    XMQ_CORE_I128,
    /** Unsigned 8 bit integer. Can be decimal, hex (0x) or octal (0). */
    XMQ_CORE_U8,
    /** Unsigned 16 bit integer. Can be decimal, hex (0x) or octal (0). */
    XMQ_CORE_U16,
    /** Unsigned 32  bit integer. Can be decimal, hex (0x) or octal (0). */
    XMQ_CORE_U32,
    /** Unsigned 64 bit integer. Can be decimal, hex (0x) or octal (0). */
    XMQ_CORE_U64,
    /** Unsigned 128 bit integer. Can be decimal, hex (0x) or octal (0). */
    XMQ_CORE_U128,

    /** Floating point 32bit. */
    XMQ_CORE_F32,
    /** Floating point 64bit. */
    XMQ_CORE_F64,

    /** Zero to infinite sized unicode string. No zero bytes. */
    XMQ_CORE_STRING,
    /** String formatted as a valid email address. */
    XMQ_CORE_EMAIL,
    /** String formatted as a valid uri/iri. */
    XMQ_CORE_URI,
    /** String formatted as a valid url. */
    XMQ_CORE_URL,

    /** Either a v4 or a v6. */
    XMQ_CORE_IP_ADDRESS,
    /** 128.0.0.1 */
    XMQ_CORE_IPV4_ADDRESS,
    /** ::0 */
    XMQ_CORE_IPV6_ADDRESS,

    /** Base64 encoded binary data. */
    XMQ_BINARY_BASE64

} XMQCoreType;

/**
    Opaque structure with the settings used to generate single line xmq for logging,
    see xmqNewLineConfig and xmqLinePrintf.
*/
typedef struct XMQLineConfig XMQLineConfig;

typedef struct XMQAddAttr XMQAddAttr;
/**
    An attribute to add, a name and a value, see XMQ_ATTRS.
*/
struct XMQAddAttr {
    const char *name;
    const char *value;
};

/**
    A list of attributes to add, created with XMQ_ATTRS.
*/
typedef struct XMQAttrList XMQAttrList;
struct XMQAttrList {
    const XMQAddAttr *array;
    size_t count;
};

#ifndef __cplusplus

/**
    Create an XMQAttrList from name value pairs, for xmqAddElementWithAttrs and
    xmqAddKeyValueWithAttrs, eg XMQ_ATTRS( { "id", "123" }, { "class", "info" } ).
*/
#define XMQ_ATTRS(...) \
    ((XMQAttrList) { \
        (XMQAddAttr[]) { __VA_ARGS__ }, \
        sizeof((XMQAddAttr[]) { __VA_ARGS__ }) / sizeof(XMQAddAttr) \
    })

#else

static inline XMQAttrList
xmqAttrs(std::initializer_list<XMQAddAttr> attrs)
{
    return { attrs.begin(), attrs.size() };
}

#define XMQ_ATTRS(...) \
    xmqAttrs({ __VA_ARGS__ })

#endif

/* This is the old definition for XMQ_ATTRS which fails to compile using g++-13.
 #define XMQ_ATTRS(...) \
    (XMQAddAttr[]){ __VA_ARGS__ }, \
    sizeof((XMQAddAttr[]){ __VA_ARGS__ }) / sizeof(XMQAddAttr)
*/

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////// FUNCTIONS  /////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
   Detect the content type xmq/xml/html/json by examining a few leading
   non-whitespace words/characters.

   @param start Points to the first byte of the buffer to scan.
   @param stop Points to the byte after the buffer.
   @return The detected content type, XMQ_CONTENT_UNKNOWN if it could not be detected.
*/
XMQContentType xmqDetectContentType(const char *start, const char *stop);

/**
   Translate an error code to a human readable string.

   @param e The error code.
   @return A static string describing the error.
*/
const char *xmqParseErrorToString(XMQStatus e);

/**
   Allocate an empty parse callbacks structure. All callbacks are NULL and none will be called.

   @return The new callbacks, free them with xmqFreeParseCallbacks.
*/
XMQParseCallbacks *xmqNewParseCallbacks();

/**
   Free the parse callbacks structure.

   @param cb The callbacks to free.
*/
void xmqFreeParseCallbacks(XMQParseCallbacks *cb);

/**
   Set up the callbacks to colorize xmq input, without building a parse tree.

   @param state The callbacks to set up.
   @param render_format The format to colorize for, eg terminal or html.
*/
void xmqSetupParseCallbacksColorizeTokens(XMQParseCallbacks *state, XMQRenderFormat render_format);

/**
   Set up the callbacks to print the location and type of each token, for debugging.

   @param state The callbacks to set up.
*/
void xmqSetupParseCallbacksDebugTokens(XMQParseCallbacks *state);

/**
   Set up the callbacks to print the decoded content of each token, for debugging.

   @param state The callbacks to set up.
*/
void xmqSetupParseCallbacksDebugContent(XMQParseCallbacks *state);

/**
   Tokenize a buffer with xmq content, invoking the callbacks of the parse state for each token.

   @param state The parse state with the callbacks.
   @param start Points to the first byte of the buffer.
   @param stop Points to the byte after the buffer.
   @return True if the buffer was tokenized without errors.
*/
bool xmqTokenizeBuffer(XMQParseState *state, const char *start, const char *stop);

/**
   Tokenize a file with xmq content, invoking the callbacks of the parse state for each token.

   @param state The parse state with the callbacks.
   @param file The file to tokenize.
   @return True if the file was tokenized without errors.
*/
bool xmqTokenizeFile(XMQParseState *state, const char *file);

/**
   Tokenize xmq content read from a file descriptor. Not implemented yet, the function is declared but has no implementation.

   @param state The parse state with the callbacks.
   @param fd The file descriptor to read from.
   @return True if the content was tokenized without errors.
*/
bool xmqTokenizeFileDescriptor(XMQParseState *state, int fd);

/**
   Create a parse state, used to tokenize xmq with your own callbacks.
   The output settings can be referenced from the callbacks, for example when colorizing.

   @param callbacks The callbacks invoked for each token.
   @param settings The settings available to the callbacks.
   @return The new parse state, free it with xmqFreeParseState.
*/
XMQParseState *xmqNewParseState(XMQParseCallbacks *callbacks, XMQOutputSettings *settings);

/**
   Free the parse state.

   @param state The parse state to free.
*/
void xmqFreeParseState(XMQParseState *state);

/**
   Set the source name used in the error messages when parsing fails.
   The source name is often the file name, but can be "-" for stdin or anything you like.

   @param state The parse state.
   @param source_name The source name.
*/
void xmqSetStateSourceName(XMQParseState *state, const char *source_name);

/**
   Get the error code of a failed parse.

   @param state The parse state.
   @return The error as the integer value of an XMQStatus.
*/
int xmqStateErrno(XMQParseState *state);

/**
   Get a message explaining why a parse failed.

   @param state The parse state.
   @return The error message, owned by the parse state.
*/
const char *xmqStateErrorMsg(XMQParseState *state);

/**
   Set if all trees should be generated when an ixml parse is ambiguous.

   @param state The parse state.
   @param all_parses If true generate all possible trees.
*/
void xmqSetPrintAllParsesIXML(XMQParseState *state, bool all_parses);

/**
   Set if a failed ixml parse should try to recover. The default is to fail.

   @param state The parse state.
   @param try_recover If true try to recover a failed parse.
*/
void xmqSetTryToRecoverIXML(XMQParseState *state, bool try_recover);

/**
   Create an empty document.

   @return The status and the document. If the status is XMQ_OK then the document is valid,
           if it is XMQ_ERROR_OOM the document is NULL. Free the document with xmqFreeDoc.
*/
XMQReturnDoc xmqNewDoc();

/**
   Set the source name used in the error messages when parsing fails.
   The source name is often the file name, but can be "-" for stdin or anything you like.

   @param doq The document.
   @param source_name The source name.
*/
void xmqSetDocSourceName(XMQDoc *doq, const char *source_name);

/**
   Get the source name of the document, used in the error messages when parsing fails.

   @param doq The document.
   @return The source name, or NULL if none was set.
*/
const char *xmqGetDocSourceName(XMQDoc *doq);

/**
   Get the content type (xmq, htmq, xml, html, json, text...) that the document was parsed from.

   @param doq The document.
   @return The original content type, if available.
*/
XMQContentType xmqGetOriginalContentType(XMQDoc *doq);

/**
   Get the size of the content that the document was parsed from, ie the loaded file size.

   @param doq The document.
   @return The original size in bytes, if available.
*/
size_t xmqGetOriginalSize(XMQDoc *doq);

/**
   Override the original size of the document.

   @param doq The document.
   @param size The size in bytes.
*/
void xmqSetOriginalSize(XMQDoc *doq, size_t size);

/**
   Get the root node, suitable as the relative node for xmqForeachRel.

   @param doq The document.
   @return The root node.
*/
XMQNode *xmqGetRootNode(XMQDoc *doq);

typedef enum
{
    XMQ_NS_NONE,
    XMQ_NS_PARENT,
    XMQ_NS_HERE,
    XMQ_NS_ANCESTOR
}
NamespaceAction;

/**
    The namespace setting of a new element or attribute, created with
    NS_NONE, NS_PARENT, NS_HERE(uri) or NS_ANCESTOR(uri).
*/
typedef struct XMQNS XMQNS;
struct XMQNS {
    NamespaceAction action;
    // A prefix (xyz) can be encoded into the uri: "{xyz}urn:myapp"
    const char *uri;
};

/** Assign no namespace to this node. */
#define NS_NONE   ((XMQNS){XMQ_NS_NONE,NULL})
/** Reuse the parent namespace for this node. */
#define NS_PARENT ((XMQNS){XMQ_NS_PARENT,NULL})
/** Create a new namespace for this node. */
#define NS_HERE(uri) ((XMQNS){XMQ_NS_HERE,uri})
/** Search for the namespace in parent and parents parent etc.
    If not found, create the namespace in the root element. */
#define NS_ANCESTOR(uri) ((XMQNS){XMQ_NS_ANCESTOR,uri})

#define END_OF_ATTRS NULL

/**
   Set the doctype of the document.

   @param doq The document.
   @param name The doctype name.
   @return XMQ_OK if the doctype was set.

   @code
   xmqSetDocType(doc, "html");
   @endcode
*/
XMQStatus xmqSetDocType(XMQDoc *doq, const char *name);

/**
    Create a new root element.

    @param doq The xmq document.
    @param name The name of the new element.
    @param ns The namespace setting.
    @return The status and the new element.

    @code
    xmqAddElement(doc, p, "el", NS_NONE); // No namespace.
    xmqAddElement(doc, p, "el", NS_HERE("urn:myapp:driver"));
    xmqAddElement(doc, p, "el", NS_HERE("{drv}urn:myapp:driver"));
    @endcode

    NS_ANCESTOR is not permitted for the root node.
*/
XMQReturnNode xmqAddRootElement(XMQDoc *doq, const char *name, XMQNS ns);

/**
    Create a new element node below an existing element.

    @param doq The xmq document.
    @param parent The existing element.
    @param name The name of the new element.
    @param ns The namespace setting.
    @return The status and the new element.

    @code
    xmqAddElement(doc, p, "el", NS_NONE); // No namespace.
    xmqAddElement(doc, p, "el", NS_PARENT); // Inherit parent namespace.
    xmqAddElement(doc, p, "el", NS_HERE("urn:myapp:driver"));
    xmqAddElement(doc, p, "el", NS_HERE("{drv}urn:myapp:driver"));
    xmqAddElement(doc, p, "el", NS_ANCESTOR("urn:myapp:driver"));
    xmqAddElement(doc, p, "el", NS_ANCESTOR("{drv}urn:myapp:driver"));
    @endcode
*/
XMQReturnNode xmqAddElement(XMQDoc *doq, XMQNode *parent, const char *name, XMQNS ns);

/**
    Create a new element node below an existing element with attributes.

    @param doq The xmq document.
    @param parent The existing element.
    @param name The name of the new element.
    @param ns The namespace setting.
    @param attrs The attributes, see XMQ_ATTRS.
    @return The status and the new element.

    @code
    xmqAddElementWithAttrs(doc, p, "div", NS_PARENT,
                           XMQ_ATTRS( { "id", "123" },
                                      { "class", "info" } ));
    xmqAddElementWithAttrs(doc, p, "a", NS_PARENT, XMQ_ATTRS( { "href", "https://libxmq.org" } ));
    @endcode
*/
XMQReturnNode xmqAddElementWithAttrs(XMQDoc *doq,
                                     XMQNode *parent,
                                     const char *name,
                                     XMQNS ns,
                                     XMQAttrList attrs);


/**
    Move an element node somewhere else. Either within the same
    document or between two documents. The element will carry its
    namespace with it.

    @param from_doq The xmq document to move from.
    @param from_parent The parent holding the element to be moved.
    @param element The element to be moved.
    @param to_doq The xmq document to move to, can be the same as from_doq.
    @param to_parent The new parent for the element.
    @param where Where to place the element below the new parent.
    @param sibling If where is in relation to a sibling, this is the sibling. Can be NULL if not needed.
    @return True if the element was moved.

    @code
    xmqMoveElement(doc, chapter1, page, chapter2, XMQ_POSITION_LAST, NULL);
    xmqMoveElement(doc, chapter1, page, chapter2, XMQ_POSITION_BEFORE, page32);
    @endcode
*/
bool xmqMoveElement(XMQDoc *from_doq,
                             XMQNode *from_parent,
                             XMQNode *element,
                             XMQDoc *to_doq,
                             XMQNode *to_parent,
                             XMQMoveTo where,
                             XMQNode *sibling);

/**
    Create a key value under an existing node.

    @param doq The xmq document.
    @param parent The parent in which the key value is created.
    @param key The key.
    @param value The value.
    @param ns The namespace setting.
    @return The status and the new node.

    @code
    xmqAddKeyValue(doc, p, "price", "123", NS_PARENT);
    @endcode
*/
XMQReturnNode xmqAddKeyValue(XMQDoc *doq, XMQNode *parent, const char *key, const char *value, XMQNS ns);

/**
    Add text last in an existing node. If the last child of the node is
    already a text node, then the text is appended to it.

    @param doq The xmq document.
    @param parent The node to add the text to.
    @param text The text, utf8, does not need to be zero terminated.
    @param len The number of bytes of the text.
    @return XMQ_OK if the text was added.

    @code
    xmqAddText(doc, p, "hello", 5);
    @endcode
*/
XMQStatus xmqAddText(XMQDoc *doq, XMQNode *parent, const char *text, size_t len);

/**
    Check if an element has an attribute (without a namespace).

    @param node The element.
    @param name The attribute name.
    @return True if the attribute exists.
*/
bool xmqHasAttribute(XMQNode *node, const char *name);

/**
    A position at the end of the current content (children and attributes)
    of a node, used by xmqWrapContent. Treat the members as opaque.
*/
struct XMQContentMark
{
    void *child; // The last child when the mark was taken.
    size_t text_len; // Its length if it is a text node, later text may be merged into it.
    void *attr; // The last attribute when the mark was taken.
};
typedef struct XMQContentMark XMQContentMark;

/**
    Mark the end of the current content of a node.

    @param node The node.
    @return The mark, to be given to xmqWrapContent.
*/
XMQContentMark xmqContentMark(XMQNode *node);

/**
    Move the content (children and attributes) added to the node after the
    mark into a new element, which is added last to the node. The mark stays
    valid, so wrapping again nests the elements:

    @code
    XMQContentMark m = xmqContentMark(e);
    xmqAddText(doc, e, "1", 1);
    xmqWrapContent(doc, e, m, "sum"); // e { sum = 1 }
    xmqAddText(doc, e, "+2", 2);
    xmqWrapContent(doc, e, m, "sum"); // e { sum { sum = 1 '+2' } }
    @endcode

    This is used by generated parsers to build the left nested ixml tree of
    left recursive rules.

    @param doq The xmq document.
    @param node The node.
    @param mark The mark from xmqContentMark on the same node.
    @param name The name of the new element.
    @return The new element.
*/
XMQReturnNode xmqWrapContent(XMQDoc *doq, XMQNode *node, XMQContentMark mark, const char *name);

/**
   Create a key value with attributes under an existing node.

   @param doq The document.
   @param parent The parent in which the key value is created.
   @param key The key.
   @param value The value.
   @param ns The namespace setting.
   @param attrs The attributes, see XMQ_ATTRS.
   @return The new node.

   @code
   xmqAddKeyValueWithAttrs(doc, p, "p", "hello", NS_NONE, XMQ_ATTRS( { "id", "123" } ));
   // <p id="123">hello</p>
   @endcode
*/
XMQReturnNode xmqAddKeyValueWithAttrs(XMQDoc *doq,
                                      XMQNode *parent,
                                      const char *key,
                                      const char *value,
                                      XMQNS ns,
                                      XMQAttrList attrs);

/**
   Create or update an attribute of an existing node.
   NS_HERE is not permitted for an attribute.

   @param doq The document.
   @param node The node to set the attribute on.
   @param name The attribute name.
   @param value The attribute value.
   @param ns The namespace setting.
   @return The attribute.

   @code
   xmqSetAttribute(doc, node, "id", "123", NS_NONE); // No namespace.
   xmqSetAttribute(doc, node, "id", "123", NS_PARENT); // The namespace of the node.
   xmqSetAttribute(doc, node, "id", "123", NS_ANCESTOR("{drv}urn:myapp:driver"));
   @endcode
*/
XMQReturnAttr xmqSetAttribute(XMQDoc *doq, XMQNode *node, const char *name, const char *value, XMQNS ns);

/**
   Add a prefixed namespace declaration to a node. It must have a prefix, since the default
   namespace can only be assigned when the node is created. Only NS_HERE is allowed.

   @param doq The document.
   @param node The node in which the namespace declaration is put.
   @param ns The namespace, NS_HERE with the prefix encoded in the uri, eg NS_HERE("{drv}urn:myapp:driver").
   @return XMQ_OK, or an error if the prefix is already taken.
*/
XMQStatus xmqAddNamespace(XMQDoc *doq, XMQNode *node, XMQNS ns);

/**
   Change the prefix that was chosen automatically for a namespace.
   Pass XMQ_NO_PREFIX to change the default prefix. Not implemented yet, the function is declared but has no implementation.

   @param doq The document.
   @param node The node with the namespace declaration.
   @param ns_uri The namespace uri.
   @param old_prefix The current prefix.
   @param new_prefix The new prefix.
   @return The status, XMQ_ERROR_PREFIX_EXISTS if the new prefix is already used.
*/
XMQReturnString xmqChangePrefix(XMQDoc *doq,
                                XMQNode *node,
                                const char *ns_uri,
                                const char *old_prefix,
                                const char *new_prefix);


/**
   Remove all content from the document, making it empty.

   @param doc The document.
*/
void xmqClearDoc(XMQDoc *doc);

/**
   Free the document and all memory associated with it.

   @param doc The document to free.
*/
void xmqFreeDoc(XMQDoc *doc);

/**
   Parse a file and create the document, or read from stdin if file is NULL.
   The xmq format permits multiple root nodes if an implicit root is supplied.

   @param doc The document to populate.
   @param file The file to parse, or NULL for stdin.
   @param implicit_root The implicit root, or NULL.
   @param flags The parse flags (XMQFlagBits), 0 for the defaults.
   @return True if the parse succeeded, otherwise see xmqDocError.
*/
bool xmqParseFile(XMQDoc *doc, const char *file, const char *implicit_root, int flags);

/**
   Parse a buffer and create the document.
   The xmq format permits multiple root nodes if an implicit root is supplied.

   @param doc The document to populate.
   @param start Points to the first byte of the buffer.
   @param stop Points to the byte after the buffer.
   @param implicit_root The implicit root, or NULL.
   @param flags The parse flags (XMQFlagBits), 0 for the defaults.
   @return True if the parse succeeded, otherwise see xmqDocError.
*/
bool xmqParseBuffer(XMQDoc *doc, const char *start, const char *stop, const char *implicit_root, int flags);

/**
   Parse data fetched with a reader and create the document.
   The xmq format permits multiple root nodes if an implicit root is supplied. Not implemented yet, the function is declared but has no implementation.

   @param doc The document to populate.
   @param reader The reader that fetches the input data.
   @param reader_state The state passed to the reader.
   @param implicit_root The implicit root, or NULL.
   @param flags The parse flags (XMQFlagBits), 0 for the defaults.
   @return True if the parse succeeded.
*/
bool xmqParseReader(XMQDoc *doc, XMQReader *reader, void *reader_state, const char *implicit_root, int flags);

/**
   Rename anonymous elements (elements with name _ ) by inferring a name from the
   parent element: an anonymous child of an element named for example 'items'
   or 'branches' gets the name 'item' or 'branch' (the trailing 's' is dropped).

   @param doq The document to modify in place.
   @param root_name If not NULL and non-empty, and the root element is anonymous (_),
                    the root element is renamed to root_name.
   @return True if the document was processed.
*/
bool xmqInferMissingElementNames(XMQDoc *doq, const char *root_name);


/**
   Allocate output settings with everything set to zero.

   @return The new settings, free them with xmqFreeOutputSettings.
*/
XMQOutputSettings *xmqNewOutputSettings();

/**
   Free the output settings.

   @param os The settings to free.
*/
void xmqFreeOutputSettings(XMQOutputSettings *os);

/**
   Set the number of spaces to indent each level when pretty printing.
   The xmq tool uses 4 (2 for json and yaml) and 0 together with compact.

   @param os The output settings to modify.
   @param add_indent The number of spaces per indentation level.
*/
void xmqSetAddIndent(XMQOutputSettings *os, int add_indent);

/**
   Set compact printing, the whole document on a single line with the whitespace
   between tokens minimized, eg a=('line one'&#10;'line two')b=2

   @param os The output settings to modify.
   @param compact If true print compact, if false pretty print.
*/
void xmqSetCompact(XMQOutputSettings *os, bool compact);

/**
   Set if and how the output is colorized when rendering to the terminal, html or tex.

   @param os The output settings to modify.
   @param color_mode No color, colors for a dark or a light background, or automatic.
*/
void xmqSetColorMode(XMQOutputSettings *os, XMQColorMode color_mode);

/**
   Set if the terminal supports truecolor (24 bit ansi colors), otherwise the colors
   are approximated with the 256 color palette.

   @param os The output settings to modify.
   @param truecolor If true use 24 bit ansi color codes.
*/
void xmqSetTrueColor(XMQOutputSettings *os, bool truecolor);

/**
   Set if quotes should use double quotes "..." instead of single quotes '...'
   when the content permits it.

   @param os The output settings to modify.
   @param prefer_double_quotes If true prefer double quotes.
*/
void xmqSetPreferDoubleQuotes(XMQOutputSettings *os, bool prefer_double_quotes);

/**
   Set if the output ends with a newline. The default is true.

   @param os The output settings to modify.
   @param final_nl If true print a final newline.
*/
void xmqSetFinalNewline(XMQOutputSettings *os, bool final_nl);

/**
   Set if newlines inside quotes should be printed as the entity &#10; instead,
   eg ( 'line one' &#10; 'line two' ) instead of a multiline quote.

   @param os The output settings to modify.
   @param escape_newlines If true escape newlines.
*/
void xmqSetEscapeNewlines(XMQOutputSettings *os, bool escape_newlines);

/**
   Set if characters outside of 7 bit ascii should be printed as entities,
   eg Göteborg becomes ( 'G' &#246; 'teborg' ).

   @param os The output settings to modify.
   @param escape_non_7bit If true escape non 7 bit characters.
*/
void xmqSetEscapeNon7bit(XMQOutputSettings *os, bool escape_non_7bit);

/**
   Set if tabs inside quotes should be printed as the entity &#9; instead.

   @param os The output settings to modify.
   @param escape_tabs If true escape tabs.
*/
void xmqSetEscapeTabs(XMQOutputSettings *os, bool escape_tabs);

/**
   Set the format that xmqPrint prints the document in.

   @param os The output settings to modify.
   @param output_format XMQ_CONTENT_XMQ, HTMQ, XML, HTML, JSON, YAML, TEXT or CLINES.
*/
void xmqSetOutputFormat(XMQOutputSettings *os, XMQContentType output_format);

/**
   Set if the xml declaration <?xml version="1.0" encoding="utf-8"?> should be
   left out when printing xml.

   @param os The output settings to modify.
   @param omit_decl If true do not print the xml declaration.
*/
void xmqSetOmitDecl(XMQOutputSettings *os, bool omit_decl);

/**
   Set how xmq output is rendered: plain, or colorized for the terminal, html, htmq or tex.

   @param os The output settings to modify.
   @param render_to The render format.
*/
void xmqSetRenderFormat(XMQOutputSettings *os, XMQRenderFormat render_to);

/**
   Set the theme used for rendering, a theme name with optional color overrides,
   eg lightbg:C=ff0000_U:AKV=00ffff. The string is not copied, it must stay valid
   while the settings are used.

   @param os The output settings to modify.
   @param theme_spec The theme specification, or NULL for the default theme.
*/
void xmqSetRenderTheme(XMQOutputSettings *os, const char *theme_spec);

/**
   Set if rendering should produce only the colorized content, without the surrounding
   page and style, eg only the pre element of render-html (--nostyle).

   @param os The output settings to modify.
   @param render_raw If true render only the content.
*/
void xmqSetRenderRaw(XMQOutputSettings *os, bool render_raw);

/**
   Set if rendering should produce only the style, eg the css of render-html (--onlystyle).

   @param os The output settings to modify.
   @param only_style If true render only the style.
*/
void xmqSetRenderOnlyStyle(XMQOutputSettings *os, bool only_style);

/**
   Set the writer that receives the printed content.

   @param os The output settings to modify.
   @param content The writer function and its state.
*/
void xmqSetWriterContent(XMQOutputSettings *os, XMQWriter content);

/**
   Set the writer that receives the error messages.

   @param os The output settings to modify.
   @param error The writer function and its state.
*/
void xmqSetWriterError(XMQOutputSettings *os, XMQWriter error);

/**
   Set up the printer to print the content to stdout and the errors to stderr.

   @param ps The output settings to modify.
*/
void xmqSetupPrintStdOutStdErr(XMQOutputSettings *ps);

/**
   Set up the printer to print to a file. Not implemented yet, the function is declared but has no implementation.

   @param ps The output settings to modify.
   @param file The file to print to.
*/
void xmqSetupPrintFile(XMQOutputSettings *ps, const char *file);

/**
   Set up the printer to print to a file descriptor. Not implemented yet, the function is declared but has no implementation.

   @param ps The output settings to modify.
   @param fd The file descriptor to print to.
*/
void xmqSetupPrintFileDescriptor(XMQOutputSettings *ps, int fd);

/**
   Set up the printer to print to a dynamically allocated memory buffer.
   When xmqPrint is done, *start points to the NUL terminated output, which the caller frees,
   and *stop to its terminating NUL.

   @param ps The output settings to modify.
   @param start Receives the start of the output.
   @param stop Receives the end of the output.
*/
void xmqSetupPrintMemory(XMQOutputSettings *ps, char **start, char **stop);

/**
   Set where to store the number of leading bytes of the memory output to skip, eg the
   xml declaration when it should be omitted. Used together with xmqSetupPrintMemory.

   @param ps The output settings to modify.
   @param skip Receives the number of bytes to skip.
*/
void xmqSetupPrintSkip(XMQOutputSettings *ps, size_t *skip);

/**
   Print the document according to the output settings.

   @param doc The document to print.
   @param settings The output settings.
*/
void xmqPrint(XMQDoc *doc, XMQOutputSettings *settings);

/**
   Add an attribute to each element with the offset of its text content.
   For example <root>ABC<a>DEF</a>GHIJ<b>xyz</b></root> becomes
   <root o="0">ABC<a o="3">DEF</a>GHIJ<b o="10">xyz</b></root> with the attribute name o.

   @param doc The document to modify.
   @param attribute_name The name of the attribute to add.
   @param ns The namespace of the attribute, or NULL.
*/
void xmqAnnotateOffsets(XMQDoc *doc, const char *attribute_name, const char *ns);

/**
   Trim the whitespace of the text nodes and comments in the document.

   @param doc The document to modify.
   @param flags The trim flags, eg XMQ_FLAG_TRIM_HEURISTIC.
*/
void xmqTrimWhitespace(XMQDoc *doc, int flags);

/**
   Create a compact single line quote that safely stores the content.
   The output can for example be: 123, John, 'John Doe', "There's a light!" or ( &#10; 'a line' &#10; )

   @param content The string to quote.
   @return A new NUL terminated string that the caller frees.
*/
char *xmqCompactQuote(const char *content);

/**
   Get the error message of a failed parse or operation on the document.

   @param doc The document.
   @return The error message, freed together with the document.
*/
const char *xmqDocError(XMQDoc *doc);

/**
   Get the error status of a failed parse or operation on the document.

   @param doc The document.
   @return The error status code.
*/
XMQStatus xmqDocErrno(XMQDoc *doc);

/**
   Get the name of a node, eg the element name.

   @param node The node.
   @return The name.
*/
const char *xmqGetName(XMQNode *node);

/**
   Get the content of a node.
   For an element, the content of its first child (the value of a key=value).
   For a text, cdata, comment or processing instruction node, its own text.

   @param node The node.
   @return The content, or NULL.
*/
const char *xmqGetContent(XMQNode *node);

/** The kind of a node, see xmqGetNodeType. */
typedef enum
{
    XMQ_NODE_UNKNOWN = 0,
    XMQ_NODE_ELEMENT = 1,
    XMQ_NODE_TEXT = 3,
    XMQ_NODE_CDATA = 4,
    /** An entity reference that has not been expanded. The name is the entity name,
        a character reference has a name starting with #, e.g. #10 */
    XMQ_NODE_ENTITY_REF = 5,
    /** A processing instruction, the name is the target and the content the data. */
    XMQ_NODE_PI = 7,
    XMQ_NODE_COMMENT = 8,
    /** The document node, the parent of the root element and of the other top level nodes. */
    XMQ_NODE_DOCUMENT = 9,
    /** The doctype, the name is the doctype name. */
    XMQ_NODE_DOCTYPE = 14
} XMQNodeType;

/**
   Get the kind of a node, eg element, text or comment.

   @param node The node.
   @return The node type.
*/
XMQNodeType xmqGetNodeType(XMQNode *node);

/**
   Get the document node, whose children are the top level nodes
   (the root element, comments, processing instructions and the doctype).

   @param doq The document.
   @return The document node.
*/
XMQNode *xmqGetDocNode(XMQDoc *doq);

/**
   Get the parent node.

   @param node The node.
   @return The parent node, the document node for a top level node, or NULL for the document node.
*/
XMQNode *xmqGetParent(XMQNode *node);
/**
   Get the first child node. Attributes are not children, see xmqFirstAttr.

   @param node The node.
   @return The first child node, or NULL if there are no children.
*/
XMQNode *xmqFirstChild(XMQNode *node);
/**
   Get the last child node.

   @param node The node.
   @return The last child node, or NULL if there are no children.
*/
XMQNode *xmqLastChild(XMQNode *node);
/**
   Get the next sibling node.

   @param node The node.
   @return The next sibling node, or NULL if it is the last.
*/
XMQNode *xmqNextSibling(XMQNode *node);
/**
   Get the previous sibling node.

   @param node The node.
   @return The previous sibling node, or NULL if it is the first.
*/
XMQNode *xmqPrevSibling(XMQNode *node);

/**
   Get the namespace uri of an element.

   @param node The element.
   @return The namespace uri, or NULL if it has no namespace.
*/
const char *xmqGetNamespaceUri(XMQNode *node);

/**
   Get the namespace prefix of an element.

   @param node The element.
   @return The prefix, or NULL if it has no prefix.
*/
const char *xmqGetNamespacePrefix(XMQNode *node);

/**
   Get the line number in the source where the node was found.

   @param node The node.
   @return The line number, or 0 if unknown.
*/
int xmqGetLine(XMQNode *node);

/**
   Get the first attribute of an element.
   Namespace declarations (xmlns, xmlns:prefix) are not attributes.

   @param node The element.
   @return The first attribute, or NULL if there are no attributes.

   @code
   for (XMQAttr *a = xmqFirstAttr(node); a; a = xmqNextAttr(a))
   {
       printf("%s=%s\n", xmqGetAttrName(a), xmqGetAttrValue(a));
   }
   @endcode
*/
XMQAttr *xmqFirstAttr(XMQNode *node);

/**
   Get the next attribute of the same element.

   @param attr The attribute.
   @return The next attribute, or NULL if it is the last.
*/
XMQAttr *xmqNextAttr(XMQAttr *attr);

/**
   Find an attribute by its name (without prefix), regardless of namespace, like xmqHasAttribute.

   @param node The element.
   @param name The attribute name.
   @return The first matching attribute, or NULL.
*/
XMQAttr *xmqGetAttr(XMQNode *node, const char *name);

/**
   Get the name of an attribute, without prefix.

   @param attr The attribute.
   @return The name.
*/
const char *xmqGetAttrName(XMQAttr *attr);

/**
   Get the value of an attribute.
   The value is NULL if it contains entity references that have not been expanded
   (only possible when xml with a DTD was parsed), use xmqExpandEntities first.

   @param attr The attribute.
   @return The value, or NULL.
*/
const char *xmqGetAttrValue(XMQAttr *attr);

/**
   Get the namespace uri of an attribute.

   @param attr The attribute.
   @return The namespace uri, or NULL if it has no namespace.
*/
const char *xmqGetAttrNamespaceUri(XMQAttr *attr);

/**
   Get the namespace prefix of an attribute.

   @param attr The attribute.
   @return The prefix, or NULL if it has no prefix.
*/
const char *xmqGetAttrNamespacePrefix(XMQAttr *attr);

/**
   Get the element that an attribute belongs to.

   @param attr The attribute.
   @return The element.
*/
XMQNode *xmqGetAttrOwner(XMQAttr *attr);

/**
   Set the content of a node, replacing its children with the text.

   @param node The node.
   @param raw_content The new content.
*/
void xmqSetContent(XMQNode *node, const char *raw_content);

/**
   Get the first node that matches the xpath.

   @param doc The document.
   @param xpath The xpath to search for.
   @return The node, or NULL if nothing matches.
*/
XMQNode *xmqGetNode(XMQDoc *doc, const char *xpath);

/**
   Get the first node that matches the xpath, evaluated relative to a node.

   @param doc The document.
   @param xpath The xpath to search for.
   @param relative The node the xpath is evaluated from.
   @return The node, or NULL if nothing matches.
*/
XMQNode *xmqGetNodeRel(XMQDoc *doc, const char *xpath, XMQNode *relative);

/**
   Get the content of the first node that matches the xpath as a 32 bit signed integer.
   A leading 0x is parsed as hex and a leading 0 as octal.

   @param doc The document.
   @param xpath The xpath to search for.
   @return The integer, or 0 if nothing matches.
*/
int32_t xmqGetInt(XMQDoc *doc, const char *xpath);

/**
   Get the content of the first node that matches the xpath, evaluated relative to a node,
   as a 32 bit signed integer. A leading 0x is parsed as hex and a leading 0 as octal.

   @param doc The document.
   @param xpath The xpath to search for.
   @param relative The node the xpath is evaluated from.
   @return The integer, or 0 if nothing matches.
*/
int32_t xmqGetIntRel(XMQDoc *doc, const char *xpath, XMQNode *relative);

/**
   Get the content of the first node that matches the xpath as a 64 bit signed integer.
   A leading 0x is parsed as hex and a leading 0 as octal.

   @param doc The document.
   @param xpath The xpath to search for.
   @return The integer, or 0 if nothing matches.
*/
int64_t xmqGetLong(XMQDoc *doc, const char *xpath);

/**
   Get the content of the first node that matches the xpath, evaluated relative to a node,
   as a 64 bit signed integer. A leading 0x is parsed as hex and a leading 0 as octal.

   @param doc The document.
   @param xpath The xpath to search for.
   @param relative The node the xpath is evaluated from.
   @return The integer, or 0 if nothing matches.
*/
int64_t xmqGetLongRel(XMQDoc *doc, const char *xpath, XMQNode *relative);

/**
   Get the content of the first node that matches the xpath as a double.

   @param doc The document.
   @param xpath The xpath to search for.
   @return The double, or 0 if nothing matches.
*/
double xmqGetDouble(XMQDoc *doc, const char *xpath);

/**
   Get the content of the first node that matches the xpath, evaluated relative to a node,
   as a double.

   @param doc The document.
   @param xpath The xpath to search for.
   @param relative The node the xpath is evaluated from.
   @return The double, or 0 if nothing matches.
*/
double xmqGetDoubleRel(XMQDoc *doc, const char *xpath, XMQNode *relative);

/**
   Get the content of the first node that matches the xpath as a string,
   eg the value of a key=value.

   @param doc The document.
   @param xpath The xpath to search for.
   @return The content, owned by the document, or NULL if nothing matches.
*/
const char *xmqGetString(XMQDoc *doc, const char *xpath);

/**
   Get the content of the first node that matches the xpath, evaluated relative to a node,
   as a string, eg the value of a key=value.

   @param doc The document.
   @param xpath The xpath to search for.
   @param relative The node the xpath is evaluated from.
   @return The content, owned by the document, or NULL if nothing matches.
*/
const char *xmqGetStringRel(XMQDoc *doc, const char *xpath, XMQNode *relative);

/**
   Call a function for each node that matches the xpath.

   @param doq The document.
   @param xpath The xpath.
   @param cb The function to call for each node, or NULL to only count.
   @param user_data The user data passed to the function.
   @return The number of matching nodes.
*/
int xmqForeach(XMQDoc *doq, const char *xpath, XMQNodeCallback cb, void *user_data);

/**
   Call a function for each node that matches the xpath, evaluated relative to a node.

   @param doq The document.
   @param xpath The xpath.
   @param cb The function to call for each node, or NULL to only count.
   @param user_data The user data passed to the function.
   @param relative The node the xpath is evaluated from.
   @return The number of matching nodes.
*/
int xmqForeachRel(XMQDoc *doq, const char *xpath, XMQNodeCallback cb, void *user_data, XMQNode *relative);

/**
   Replace each reference to an entity with a text. Not implemented yet, the function is declared but has no implementation.

   @param doq The document.
   @param entity The entity name.
   @param content The text to insert.
   @return The number of replacements.
*/
int xmqReplaceEntity(XMQDoc *doq, const char *entity, const char *content);

/**
   Replace each reference to an entity with a copy of a node from another document. Not implemented yet, the function is declared but has no implementation.

   @param doq The document.
   @param entity The entity name.
   @param idoq The document with the node to insert.
   @param inode The node to insert.
   @return The number of replacements.
*/
int xmqReplaceEntityWithNode(XMQDoc *doq, const char *entity, XMQDoc *idoq, XMQNode *inode);

/**
   Expand the entities declared in the doctype.

   Entities are never expanded when parsing, a reference &name; to a declared entity
   stays as an entity node (and contributes nothing to xpath string values) until this
   is called. Every reference to an internal entity, also inside attribute values, is
   replaced with its replacement text parsed as content (text, elements, comments...).
   References inside the replacement text are expanded too. Character references,
   undeclared entities and external entities (which are never loaded) are kept.

   Entity loops, more than 19 nested entities and an expansion larger than 1MB and
   five times the size of the parsed source are errors (the limits of libxml2).
   After an error the references expanded so far stay expanded.

   @param doq The document.
   @return XMQ_OK, or XMQ_ERROR_ENTITY_EXPANSION with the message in xmqDocError.
*/
XMQStatus xmqExpandEntities(XMQDoc *doq);

/**
   Get the version of this library.

   @return The version string.
*/
const char *xmqVersion();

/**
   Get the optional parts compiled into this library, space separated, from
   "xmq-printing json yaml xslt xsd ixml html". A part is missing when it was excluded with
   XMQ_NO_XMQ_PRINTING, XMQ_NO_JSON, XMQ_NO_YAML, XMQ_NO_LIBXSLT, XMQ_NO_XSD, XMQ_NO_IXML or
   XMQ_NO_HTML.

   @return The compiled in parts.
*/
const char *xmqFeatures();

/**
   Enable or disable verbose logging.

   @param e True to enable.
*/
void xmqSetVerbose(bool e);

/**
   Enable or disable printing the progress on stderr, eg while parsing a large input with ixml.

   @param e True to enable.
*/
void xmqSetProgress(bool e);

/**
   Enable or disable debug logging.

   @param e True to enable.
*/
void xmqSetDebug(bool e);

/**
   Enable or disable trace logging.

   @param e True to enable.
*/
void xmqSetTrace(bool e);

/**
   Only log the debug and trace messages that start with a prefix, eg "xmq=".

   @param log_filter The prefix, or NULL to log all messages.
*/
void xmqLogFilter(const char *log_filter);

/**
   Check if debug logging is enabled.

   @return True if enabled.
*/
bool xmqDebugging();

/**
   Check if trace logging is enabled.

   @return True if enabled.
*/
bool xmqTracing();

/**
   Set if the verbose, debug and trace logging is human readable. The default is xmq.

   @param e True for human readable.
*/
void xmqSetLogHumanReadable(bool e);

/**
   Check if logging is in xmq. Not implemented yet, the function is declared but has no implementation.

   @return True if logging is in xmq.
*/
bool xmqLoggingXMQ();

/**
   Parse a buffer with a given content type and create the document.

   @param doc The document to populate.
   @param start Points to the first byte of the buffer.
   @param stop Points to the byte after the buffer.
   @param implicit_root The implicit root, or NULL.
   @param ct The content type, or XMQ_CONTENT_DETECT to detect it.
   @param flags The parse flags (XMQFlagBits), 0 for the defaults.
   @return True if the parse succeeded, otherwise see xmqDocError.
*/
bool xmqParseBufferWithType(XMQDoc *doc,
                            const char *start,
                            const char *stop,
                            const char *implicit_root,
                            XMQContentType ct,
                            int flags);

/**
   Parse a file with a given content type and create the document, or read from stdin
   if file is NULL.

   @param doc The document to populate.
   @param file The file to parse, or NULL for stdin.
   @param implicit_root The implicit root, or NULL.
   @param ct The content type, or XMQ_CONTENT_DETECT to detect it.
   @param flags The parse flags (XMQFlagBits), 0 for the defaults.
   @return True if the parse succeeded, otherwise see xmqDocError.
*/
bool xmqParseFileWithType(XMQDoc *doc,
                          const char *file,
                          const char *implicit_root,
                          XMQContentType ct,
                          int flags);

/**
   Parse a buffer with an ixml grammar and create the document.

   @param doc The document to populate.
   @param start Points to the first byte of the buffer.
   @param stop Points to the byte after the buffer.
   @param ixml_grammar The grammar, a document parsed with XMQ_CONTENT_IXML.
   @param flags The parse flags (XMQFlagBits), 0 for the defaults.
   @return True if the parse succeeded, otherwise see xmqDocError.
*/
bool xmqParseBufferWithIXML(XMQDoc *doc,
                            const char *start,
                            const char *stop,
                            XMQDoc *ixml_grammar,
                            int flags);

/**
   Take the trace of the last parse of doc with xmqParseBufferWithIXML and the flag
   XMQ_FLAG_IXML_TRACE. The trace is:
   trace(engine=yaep) {
       grammar { rule(name rhs) ... }
       input { token(pos) = 1 ... token(pos) = ω }
       steps { read(pos) item(id from to state rule matched expected) ... }
       tree { node(id from to rule rhs) ... } }
   where state is predicted, partial, complete or blocked.

   @param doc The parsed document.
   @return The trace document, owned by the caller who frees it with xmqFreeDoc, or NULL.
*/
XMQDoc *xmqTakeIXMLTrace(XMQDoc *doc);

/**
   Get the ixml text of a grammar in xml form, ie the parse tree of the grammar with
   the ixml grammar, eg ixml{rule(name=S){alt{literal(string=a)}}}, read from xml, xmq
   or json. Parse the text with xmqParseBufferWithType(..., XMQ_CONTENT_IXML, ...).

   @param grammar The grammar in xml form.
   @param error Receives an error message, that the caller frees, if NULL is returned.
   @return The ixml text, that the caller frees, or NULL if the document is not such a grammar.
*/
char *xmqIXMLFromDoc(XMQDoc *grammar, char **error);

/**
   Take what could follow the input of the last parse of doc with xmqParseBufferWithIXML
   and the flag XMQ_FLAG_IXML_COMPLETE. One line for each, sorted by rule:
     string<TAB>rule<TAB>text     the characters text can follow, eg verb gives
     char<TAB>rule<TAB>#a         a control character, written as in an ixml grammar
     charset<TAB>rule<TAB>['a'-'z']  a character from the set can follow
     end<TAB><TAB>                the input can end here
   where rule is the named grammar rule (a generated rule gives the rule it was made for).
   ixml has no tokens, so text is the characters up to the next character set or rule,
   eg the rest of a word: with the input "a man gi" the line is: string verb ves

   @param doc The parsed document.
   @return The completions, that the caller frees, or NULL if the input could not be parsed up to its end.
*/
char *xmqTakeIXMLCompletions(XMQDoc *doc);

/**
   Parse a file with an ixml grammar and create the document, or read from stdin if
   file is NULL.

   @param doc The document to populate.
   @param file The file to parse, or NULL for stdin.
   @param ixml_grammar The grammar, a document parsed with XMQ_CONTENT_IXML.
   @param flags The parse flags (XMQFlagBits), 0 for the defaults.
   @return True if the parse succeeded, otherwise see xmqDocError.
*/
bool xmqParseFileWithIXML(XMQDoc *doc,
                          const char *file,
                          XMQDoc *ixml_grammar,
                          int flags);

/**
   Set up the colors of the output settings from the theme and the color mode.

   @param settings The output settings to modify.
*/
void xmqSetupDefaultColors(XMQOutputSettings *settings);

/**
   Change the strings printed for indentation and explicit whitespace.

   @param settings The output settings to modify.
   @param indentation_space The indentation, if NULL use " ".
   @param explicit_space An explicit space, if NULL use " ".
   @param explicit_tab An explicit tab, if NULL use "\t".
   @param explicit_cr An explicit carriage return, if NULL use "\r".
   @param explicit_nl An explicit newline, if NULL use "\n".
*/
void xmqOverrideSettings(XMQOutputSettings *settings,
                         const char *indentation_space,
                         const char *explicit_space,
                         const char *explicit_tab,
                         const char *explicit_cr,
                         const char *explicit_nl);

/**
   Set the id and class of the pre element of the rendered html.

   @param settings The output settings to modify.
   @param use_id The id of the pre element, or NULL.
   @param use_class The class of the pre element, or NULL.
*/
void xmqRenderHtmlSettings(XMQOutputSettings *settings,
                           const char *use_id,
                           const char *use_class);

/**
   Change the color strings of a syntax element. Currently this does nothing.

   @param settings The output settings to modify.
   @param render_style The render style, "" for the default.
   @param cn The syntax element to change the color for.
   @param pre The string printed before the element.
   @param post The string printed after the element.
   @param ns The color of the namespace prefix.
*/
void xmqOverrideColor(XMQOutputSettings *settings,
                      const char *render_style,
                      XMQColorName cn,
                      const char *pre,
                      const char *post,
                      const char *ns);


/**
   Allocate a line configuration with the defaults, used to generate single line xmq for logging.

   @return The new line configuration, free it with xmqFreeLineConfig.
*/
XMQLineConfig *xmqNewLineConfig();

/**
   Free the line configuration.

   @param lc The line configuration to free.
*/
void xmqFreeLineConfig(XMQLineConfig *lc);

/**
   Set if the generated log lines should be more human readable.

   @param lc The line configuration.
   @param enable True for more human readable lines.
*/
void xmqSetLineHumanReadable(XMQLineConfig *lc, bool enable);

/**
   Generate a compact single line xmq string, without any newlines, from a document, for logging.

   @param lc The line configuration.
   @param doc The document.
   @return A new string that the caller frees.
*/
char *xmqLineDoc(XMQLineConfig *lc, XMQDoc *doc);

/**
   Generate a compact single line xmq string, without any newlines, for logging.
   The content is constructed from printf formatted strings:

   @code
   xmqLinePrintf(&lc,
                 "car{",
                 "model=", "%s", modelstring,
                 "id=", "%d", idnumber,
                 "description=", "desc: %s", multilinedescription,
                 "}");
   @endcode

   generates: car{model=Volvo id=123 description=('desc: lines of'&#10;'next line')}

   @param lc The line configuration.
   @param element_name The element name, eg "car{", followed by key, format and value arguments.
   @return A new string that the caller frees.
*/
char *xmqLinePrintf(XMQLineConfig *lc, const char *element_name, ...);

/**
   The same as xmqLinePrintf but with the arguments as a va_list.

   @param lc The line configuration.
   @param element_name The element name, eg "car{", followed by the arguments in ap.
   @param ap The key, format and value arguments, see xmqLinePrintf.
   @return A new string that the caller must free.
*/
char *xmqLineVPrintf(XMQLineConfig *lc, const char *element_name, va_list ap);

/////////////////////////////////////////////////////////////////////////////////////
// XSLT 1.0

/** A compiled XSLT 1.0 stylesheet. */
typedef struct XMQXslt XMQXslt;

/**
    The status and the compiled stylesheet returned by xmqCompileXslt.
    The stylesheet is only valid if the status is XMQ_OK.
*/
struct XMQReturnXslt
{
    XMQStatus status;
    XMQXslt *xslt;
};
typedef struct XMQReturnXslt XMQReturnXslt;

/** Load the document at uri for xsl:import, xsl:include and document(), return NULL if it
    cannot be loaded. The returned document is owned by the caller of the loader. */
typedef XMQDoc *(*XMQXsltLoader)(void *user_data, const char *uri);

/** Receives the messages of compilation and transformation (errors, warnings, xsl:message). */
typedef void (*XMQXsltMessageCallback)(void *user_data, const char *message);

/**
    The settings for compiling a stylesheet, how to load imported files and where
    the messages go.
*/
struct XMQXsltSettings
{
    /** NULL loads the files with xmqParseFileWithType (content detection, no trimming). */
    XMQXsltLoader loader;
    void *loader_user_data;
    /** NULL prints the messages on stderr (like libxslt). */
    XMQXsltMessageCallback message_callback;
    void *message_user_data;
};
typedef struct XMQXsltSettings XMQXsltSettings;

/**
   Compile an xslt stylesheet. A copy of the stylesheet is compiled, the document is not changed.

   @param stylesheet The stylesheet document.
   @param settings The loader and message settings, or NULL for the defaults.
   @return The status and the compiled stylesheet. On failure the status is XMQ_ERROR_XSLT_COMPILE
           and the messages are also available with xmqDocError(stylesheet).
*/
XMQReturnXslt xmqCompileXslt(XMQDoc *stylesheet, XMQXsltSettings *settings);

/**
   Set a top level parameter of the stylesheet.

   @param xslt The compiled stylesheet.
   @param name The parameter name.
   @param value The value.
   @param is_xpath If true the value is an xpath expression (like --param), otherwise a string (like --stringparam).
*/
void xmqSetXsltParam(XMQXslt *xslt, const char *name, const char *value, bool is_xpath);

/**
   Apply the stylesheet to a document. Whitespace may be stripped from the document by xsl:strip-space.
   The result gets the xsl:output method as its original content type (XMQ_CONTENT_XML,
   XMQ_CONTENT_HTML or XMQ_CONTENT_TEXT).

   @param xslt The compiled stylesheet.
   @param doc The document to transform.
   @return The status and the result document. On failure the status is XMQ_ERROR_XSLT_TRANSFORM
           and the messages are also available with xmqDocError(doc).
*/
XMQReturnDoc xmqTransform(XMQXslt *xslt, XMQDoc *doc);

/**
   Serialize a result of xmqTransform as the xsl:output of the stylesheet asks for (method,
   encoding, indent, doctype, xml declaration), like libxslt xsltSaveResultToString.

   @param xslt The compiled stylesheet.
   @param result The result of xmqTransform.
   @param len Receives the length, the string may contain zeros for UTF-16.
   @return A new string that the caller frees.
*/
char *xmqSaveXsltResult(XMQXslt *xslt, XMQDoc *result, size_t *len);

/**
   Free a compiled stylesheet.

   @param xslt The compiled stylesheet to free.
*/
void xmqFreeXslt(XMQXslt *xslt);

/////////////////////////////////////////////////////////////////////////////////////
// XSD 1.0

/** A compiled XSD 1.0 schema. */
typedef struct XMQXsd XMQXsd;

/**
    The status and the compiled schema returned by xmqCompileXsd.
    The schema is only valid if the status is XMQ_OK.
*/
struct XMQReturnXsd
{
    XMQStatus status;
    XMQXsd *xsd;
};
typedef struct XMQReturnXsd XMQReturnXsd;

/** Load the schema document at uri for xs:include, xs:import and xs:redefine, return NULL if it
    cannot be loaded. The returned document is owned by the caller of the loader. */
typedef XMQDoc *(*XMQXsdLoader)(void *user_data, const char *uri);

/** Receives the messages of compilation and validation, exactly as libxml2 formats them for
    its plain error callbacks. is_warning is true for warnings. */
typedef void (*XMQXsdMessageCallback)(void *user_data, const char *message, bool is_warning);

/**
    The settings for compiling a schema, how to load included files and where
    the messages go.
*/
struct XMQXsdSettings
{
    /** NULL reads the files and parses them as xml with entities substituted (like libxml2). */
    XMQXsdLoader loader;
    void *loader_user_data;
    /** NULL prints the messages on stderr in the libxml2 format (file:line: element x: ...). */
    XMQXsdMessageCallback message_callback;
    void *message_user_data;
};
typedef struct XMQXsdSettings XMQXsdSettings;

/**
   Compile an xsd schema. A copy of the schema is compiled, the document is not changed.
   The message callback of the settings is also used by xmqValidate.

   @param schema The schema document.
   @param settings The loader and message settings, or NULL for the defaults.
   @return The status and the compiled schema. On failure the status is XMQ_ERROR_XSD_COMPILE
           and the messages are also available with xmqDocError(schema).
*/
XMQReturnXsd xmqCompileXsd(XMQDoc *schema, XMQXsdSettings *settings);

/**
   Change where the messages of xmqValidate go.

   @param xsd The compiled schema.
   @param callback The function that receives the messages, or NULL to print them on stderr.
   @param user_data The user data passed to the callback.
*/
void xmqSetXsdMessageCallback(XMQXsd *xsd, XMQXsdMessageCallback callback, void *user_data);

/**
   Validate a document against a schema.

   @param xsd The compiled schema.
   @param doc The document to validate.
   @return XMQ_OK if it is valid, XMQ_ERROR_XSD_INVALID if it is not (the messages are also available
           with xmqDocError(doc)) or XMQ_ERROR_XSD_COMPILE if the schema cannot be used.
*/
XMQStatus xmqValidate(XMQXsd *xsd, XMQDoc *doc);

/**
   Free a compiled schema.

   @param xsd The compiled schema to free.
*/
void xmqFreeXsd(XMQXsd *xsd);

#ifdef __cplusplus
_hideRBfromEditor
#endif


#endif
