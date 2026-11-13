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

package org.libxmq.imp.xml;

import java.util.ArrayList;
import java.util.List;

/**
 * Compiles an xpath expression string into an expression tree. This is a
 * port of the libxml2 recursive descent compiler (xmlXPathCompileExpr and
 * friends in xpath.c, Copyright (C) 1998-2012 Daniel Veillard, MIT
 * license), so that the same expressions are accepted and rejected, and
 * the expression tree has the same shape (eg where node-sets are sorted).
 */
final class XPathParser
{
    private final String s_;
    private int pos_;

    XPathParser(String s)
    {
        s_ = s;
    }

    /** Compile the whole expression, like libxml2 xmlXPathEvalExpr. */
    XPathExpr parse() throws XPathException
    {
        XPathExpr e;
        try
        {
            e = compileExpr(true);
        }
        catch (StackOverflowError so)
        {
            throw new XPathException(XPathException.RECURSION_LIMIT_EXCEEDED, s_, pos_);
        }
        if (cur() != 0) throw error(XPathException.EXPR_ERROR);
        optimize(e);
        return e;
    }

    private XPathException error(int code)
    {
        return new XPathException(code, s_, pos_);
    }

    private int cur()
    {
        return pos_ < s_.length() ? s_.charAt(pos_) : 0;
    }

    private int nxt(int n)
    {
        return pos_ + n < s_.length() ? s_.charAt(pos_ + n) : 0;
    }

    private void next()
    {
        if (pos_ < s_.length()) pos_++;
    }

    private void skip(int n)
    {
        pos_ = Math.min(pos_ + n, s_.length());
    }

    private void skipBlanks()
    {
        while (XPathUtil.isBlank(cur())) pos_++;
    }

    /** The current character as a code point. */
    private int curChar()
    {
        return pos_ < s_.length() ? s_.codePointAt(pos_) : 0;
    }

    // ----------------------------------------------------------------------
    // Expressions

    private XPathExpr compileExpr(boolean sort) throws XPathException
    {
        XPathExpr e = compileAndExpr();
        skipBlanks();
        while (cur() == 'o' && nxt(1) == 'r')
        {
            skip(2);
            skipBlanks();
            XPathExpr r = compileAndExpr();
            e = new XPathExpr.Or(e, r);
            skipBlanks();
        }
        if (sort && !e.isValue()) e = new XPathExpr.Sort(e);
        return e;
    }

    private XPathExpr compileAndExpr() throws XPathException
    {
        XPathExpr e = compileEqualityExpr();
        skipBlanks();
        while (cur() == 'a' && nxt(1) == 'n' && nxt(2) == 'd')
        {
            skip(3);
            skipBlanks();
            XPathExpr r = compileEqualityExpr();
            e = new XPathExpr.And(e, r);
            skipBlanks();
        }
        return e;
    }

    private XPathExpr compileEqualityExpr() throws XPathException
    {
        XPathExpr e = compileRelationalExpr();
        skipBlanks();
        while (cur() == '=' || (cur() == '!' && nxt(1) == '='))
        {
            boolean eq = cur() == '=';
            next();
            if (!eq) next();
            skipBlanks();
            XPathExpr r = compileRelationalExpr();
            e = new XPathExpr.Equal(e, r, eq);
            skipBlanks();
        }
        return e;
    }

    private XPathExpr compileRelationalExpr() throws XPathException
    {
        XPathExpr e = compileAdditiveExpr();
        skipBlanks();
        while (cur() == '<' || cur() == '>')
        {
            boolean inf = cur() == '<';
            boolean strict = nxt(1) != '=';
            next();
            if (!strict) next();
            skipBlanks();
            XPathExpr r = compileAdditiveExpr();
            e = new XPathExpr.Compare(e, r, inf, strict);
            skipBlanks();
        }
        return e;
    }

    private XPathExpr compileAdditiveExpr() throws XPathException
    {
        XPathExpr e = compileMultiplicativeExpr();
        skipBlanks();
        while (cur() == '+' || cur() == '-')
        {
            boolean plus = cur() == '+';
            next();
            skipBlanks();
            XPathExpr r = compileMultiplicativeExpr();
            e = new XPathExpr.Arith(e, r, plus ? XPathExpr.OP_ADD : XPathExpr.OP_SUB);
            skipBlanks();
        }
        return e;
    }

    private XPathExpr compileMultiplicativeExpr() throws XPathException
    {
        XPathExpr e = compileUnaryExpr();
        skipBlanks();
        while (cur() == '*' || (cur() == 'd' && nxt(1) == 'i' && nxt(2) == 'v')
               || (cur() == 'm' && nxt(1) == 'o' && nxt(2) == 'd'))
        {
            int op;
            if (cur() == '*')
            {
                op = XPathExpr.OP_MUL;
                next();
            }
            else if (cur() == 'd')
            {
                op = XPathExpr.OP_DIV;
                skip(3);
            }
            else
            {
                op = XPathExpr.OP_MOD;
                skip(3);
            }
            skipBlanks();
            XPathExpr r = compileUnaryExpr();
            e = new XPathExpr.Arith(e, r, op);
            skipBlanks();
        }
        return e;
    }

    private XPathExpr compileUnaryExpr() throws XPathException
    {
        boolean minus = false;
        boolean found = false;
        skipBlanks();
        while (cur() == '-')
        {
            minus = !minus;
            found = true;
            next();
            skipBlanks();
        }
        XPathExpr e = compileUnionExpr();
        if (found) e = new XPathExpr.Negate(e, minus);
        return e;
    }

    private XPathExpr compileUnionExpr() throws XPathException
    {
        XPathExpr e = compilePathExpr();
        skipBlanks();
        while (cur() == '|')
        {
            next();
            skipBlanks();
            XPathExpr r = compilePathExpr();
            e = new XPathExpr.Union(e, r);
            skipBlanks();
        }
        return e;
    }

    // ----------------------------------------------------------------------
    // Primary expressions

    private XPathExpr compilePrimaryExpr() throws XPathException
    {
        XPathExpr e;
        skipBlanks();
        if (cur() == '$')
        {
            e = compileVariableReference();
        }
        else if (cur() == '(')
        {
            next();
            skipBlanks();
            e = compileExpr(true);
            if (cur() != ')') throw error(XPathException.EXPR_ERROR);
            next();
            skipBlanks();
        }
        else if (XPathUtil.isAsciiDigit(cur()) || (cur() == '.' && XPathUtil.isAsciiDigit(nxt(1))))
        {
            e = compileNumber();
        }
        else if (cur() == '\'' || cur() == '"')
        {
            e = new XPathExpr.Literal(parseLiteral());
        }
        else
        {
            e = compileFunctionCall();
        }
        skipBlanks();
        return e;
    }

    private XPathExpr compileFilterExpr() throws XPathException
    {
        XPathExpr e = compilePrimaryExpr();
        skipBlanks();
        while (cur() == '[')
        {
            XPathExpr p = compilePredicate(true);
            e = new XPathExpr.Filter(e, p);
            skipBlanks();
        }
        return e;
    }

    private XPathExpr compilePredicate(boolean filter) throws XPathException
    {
        skipBlanks();
        if (cur() != '[') throw error(XPathException.INVALID_PREDICATE_ERROR);
        next();
        skipBlanks();
        XPathExpr e = compileExpr(filter);
        if (cur() != ']') throw error(XPathException.INVALID_PREDICATE_ERROR);
        next();
        skipBlanks();
        return e;
    }

    private XPathExpr compileVariableReference() throws XPathException
    {
        skipBlanks();
        if (cur() != '$') throw error(XPathException.VARIABLE_REF_ERROR);
        next();
        String[] q = parseQName();
        if (q == null) throw error(XPathException.VARIABLE_REF_ERROR);
        skipBlanks();
        return new XPathExpr.Var(q[0], q[1]);
    }

    private XPathExpr compileFunctionCall() throws XPathException
    {
        String[] q = parseQName();
        if (q == null) throw error(XPathException.EXPR_ERROR);
        skipBlanks();
        if (cur() != '(') throw error(XPathException.EXPR_ERROR);
        next();
        skipBlanks();
        List<XPathExpr> args = new ArrayList<>();
        if (cur() != ')')
        {
            while (cur() != 0)
            {
                args.add(compileExpr(true));
                if (cur() == ')') break;
                if (cur() != ',') throw error(XPathException.EXPR_ERROR);
                next();
                skipBlanks();
            }
        }
        next();
        skipBlanks();
        return new XPathExpr.Call(q[0], q[1], args);
    }

    /** Like libxml2 xmlXPathCompNumber. */
    private XPathExpr compileNumber() throws XPathException
    {
        double ret = 0;
        boolean ok = false;
        int exponent = 0;
        boolean is_exponent_negative = false;
        if (cur() != '.' && !XPathUtil.isAsciiDigit(cur())) throw error(XPathException.NUMBER_ERROR);
        while (XPathUtil.isAsciiDigit(cur()))
        {
            ret = ret * 10;
            ret = ret + (double)(cur() - '0');
            ok = true;
            next();
        }
        if (cur() == '.')
        {
            int frac = 0;
            double fraction = 0;
            next();
            if (!XPathUtil.isAsciiDigit(cur()) && !ok) throw error(XPathException.NUMBER_ERROR);
            while (cur() == '0')
            {
                frac++;
                next();
            }
            int max = frac + 20;
            while (XPathUtil.isAsciiDigit(cur()) && frac < max)
            {
                fraction = fraction * 10 + (cur() - '0');
                frac++;
                next();
            }
            fraction /= Math.pow(10.0, frac);
            ret = ret + fraction;
            while (XPathUtil.isAsciiDigit(cur())) next();
        }
        if (cur() == 'e' || cur() == 'E')
        {
            next();
            if (cur() == '-')
            {
                is_exponent_negative = true;
                next();
            }
            else if (cur() == '+')
            {
                next();
            }
            while (XPathUtil.isAsciiDigit(cur()))
            {
                if (exponent < 1000000) exponent = exponent * 10 + (cur() - '0');
                next();
            }
            if (is_exponent_negative) exponent = -exponent;
            ret *= Math.pow(10.0, (double)exponent);
        }
        return new XPathExpr.NumberLit(ret);
    }

    /** Like libxml2 xmlXPathParseLiteral. */
    private String parseLiteral() throws XPathException
    {
        int q = cur();
        if (q != '"' && q != '\'') throw error(XPathException.START_LITERAL_ERROR);
        next();
        int start = pos_;
        while (isCharCh(cur()) && cur() != q) next();
        if (!isCharCh(cur())) throw error(XPathException.UNFINISHED_LITERAL_ERROR);
        String ret = s_.substring(start, pos_);
        next();
        return ret;
    }

    /** Like libxml2 IS_CHAR_CH, applied to utf-16 code units, never true at the end of input. */
    private static boolean isCharCh(int c)
    {
        return c >= 0x20 || c == 0x09 || c == 0x0A || c == 0x0D;
    }

    // ----------------------------------------------------------------------
    // Names

    private boolean isNameChar(int c)
    {
        return XPathUtil.isLetter(c) || XPathUtil.isDigit(c) || c == '.' || c == '-' || c == '_'
            || XPathUtil.isCombining(c) || XPathUtil.isExtender(c);
    }

    /** Like libxml2 xmlXPathParseNCName. */
    private String parseNCName()
    {
        int c = curChar();
        if (c == 0 || (!XPathUtil.isLetter(c) && c != '_')) return null;
        int start = pos_;
        while (c != 0 && isNameChar(c))
        {
            pos_ += Character.charCount(c);
            c = curChar();
        }
        if (pos_ == start) return null;
        return s_.substring(start, pos_);
    }

    /** Like libxml2 xmlXPathParseQName, returns {prefix, name} or null. */
    private String[] parseQName()
    {
        String prefix = null;
        String ret = parseNCName();
        if (ret != null && cur() == ':')
        {
            prefix = ret;
            next();
            ret = parseNCName();
        }
        if (ret == null) return null;
        return new String[] { prefix, ret };
    }

    /** Like libxml2 xmlXPathScanName, scans a name (with colons) without consuming it. */
    private String scanName()
    {
        int c = curChar();
        if (c == ' ' || c == '>' || c == '/' || (!XPathUtil.isLetter(c) && c != '_' && c != ':')) return null;
        int p = pos_;
        while (c != 0 && (isNameChar(c) || c == ':'))
        {
            p += Character.charCount(c);
            c = p < s_.length() ? s_.codePointAt(p) : 0;
        }
        return s_.substring(pos_, p);
    }

    private static boolean isNodeType(String name)
    {
        return name.equals("node") || name.equals("text") || name.equals("comment")
            || name.equals("processing-instruction");
    }

    // ----------------------------------------------------------------------
    // Location paths

    /** Like libxml2 xmlXPathCompPathExpr. */
    private XPathExpr compilePathExpr() throws XPathException
    {
        boolean lc = true;
        skipBlanks();
        int c = cur();
        if (c == '$' || c == '(' || XPathUtil.isAsciiDigit(c) || c == '\'' || c == '"'
            || (c == '.' && XPathUtil.isAsciiDigit(nxt(1))))
        {
            lc = false;
        }
        else if (c == '*' || c == '/' || c == '@' || c == '.')
        {
            lc = true;
        }
        else
        {
            skipBlanks();
            String name = scanName();
            if (name != null && name.contains("::"))
            {
                lc = true;
            }
            else if (name != null)
            {
                int len = name.length();
                while (nxt(len) != 0)
                {
                    int n = nxt(len);
                    if (n == '/')
                    {
                        lc = true;
                        break;
                    }
                    else if (XPathUtil.isBlank(n))
                    {
                        // ignore blanks
                    }
                    else if (n == ':')
                    {
                        lc = true;
                        break;
                    }
                    else if (n == '(')
                    {
                        lc = isNodeType(name);
                        break;
                    }
                    else
                    {
                        lc = true;
                        break;
                    }
                    len++;
                }
                if (nxt(len) == 0) lc = true;
            }
            else
            {
                throw error(XPathException.EXPR_ERROR);
            }
        }

        if (lc)
        {
            int start = cur() == '/' ? XPathExpr.START_ROOT : XPathExpr.START_CONTEXT;
            List<XPathExpr.Step> steps = new ArrayList<>();
            compileLocationPath(steps);
            return new XPathExpr.Path(start, null, steps);
        }
        XPathExpr f = compileFilterExpr();
        if (cur() == '/' && nxt(1) == '/')
        {
            skip(2);
            skipBlanks();
            List<XPathExpr.Step> steps = new ArrayList<>();
            steps.add(descendantOrSelfNode());
            compileRelativeLocationPath(steps);
            return new XPathExpr.Path(XPathExpr.START_FILTER, f, steps);
        }
        else if (cur() == '/')
        {
            List<XPathExpr.Step> steps = new ArrayList<>();
            compileRelativeLocationPath(steps);
            return new XPathExpr.Path(XPathExpr.START_FILTER, f, steps);
        }
        skipBlanks();
        return f;
    }

    private static XPathExpr.Step descendantOrSelfNode()
    {
        return new XPathExpr.Step(XPathExpr.AXIS_DESCENDANT_OR_SELF, XPathExpr.NODE_TEST_TYPE,
                                  XPathExpr.NODE_TYPE_NODE, null, null, new ArrayList<>());
    }

    /** Like libxml2 xmlXPathCompLocationPath. */
    private void compileLocationPath(List<XPathExpr.Step> steps) throws XPathException
    {
        skipBlanks();
        if (cur() != '/')
        {
            compileRelativeLocationPath(steps);
            return;
        }
        while (cur() == '/')
        {
            if (cur() == '/' && nxt(1) == '/')
            {
                skip(2);
                skipBlanks();
                steps.add(descendantOrSelfNode());
                compileRelativeLocationPath(steps);
            }
            else if (cur() == '/')
            {
                next();
                skipBlanks();
                int c = cur();
                if (c != 0 && (XPathUtil.isAsciiLetter(c) || c == '_' || c == '.' || c == '@' || c == '*'))
                {
                    compileRelativeLocationPath(steps);
                }
            }
        }
    }

    /** Like libxml2 xmlXPathCompRelativeLocationPath. */
    private void compileRelativeLocationPath(List<XPathExpr.Step> steps) throws XPathException
    {
        skipBlanks();
        if (cur() == '/' && nxt(1) == '/')
        {
            skip(2);
            skipBlanks();
            steps.add(descendantOrSelfNode());
        }
        else if (cur() == '/')
        {
            next();
            skipBlanks();
        }
        compileStep(steps);
        skipBlanks();
        while (cur() == '/')
        {
            if (cur() == '/' && nxt(1) == '/')
            {
                skip(2);
                skipBlanks();
                steps.add(descendantOrSelfNode());
                compileStep(steps);
            }
            else if (cur() == '/')
            {
                next();
                skipBlanks();
                compileStep(steps);
            }
            skipBlanks();
        }
    }

    private static int axisFromName(String name)
    {
        switch (name)
        {
        case "ancestor": return XPathExpr.AXIS_ANCESTOR;
        case "ancestor-or-self": return XPathExpr.AXIS_ANCESTOR_OR_SELF;
        case "attribute": return XPathExpr.AXIS_ATTRIBUTE;
        case "child": return XPathExpr.AXIS_CHILD;
        case "descendant": return XPathExpr.AXIS_DESCENDANT;
        case "descendant-or-self": return XPathExpr.AXIS_DESCENDANT_OR_SELF;
        case "following": return XPathExpr.AXIS_FOLLOWING;
        case "following-sibling": return XPathExpr.AXIS_FOLLOWING_SIBLING;
        case "namespace": return XPathExpr.AXIS_NAMESPACE;
        case "parent": return XPathExpr.AXIS_PARENT;
        case "preceding": return XPathExpr.AXIS_PRECEDING;
        case "preceding-sibling": return XPathExpr.AXIS_PRECEDING_SIBLING;
        case "self": return XPathExpr.AXIS_SELF;
        default: return 0;
        }
    }

    /** Like libxml2 xmlXPathCompStep. */
    private void compileStep(List<XPathExpr.Step> steps) throws XPathException
    {
        skipBlanks();
        if (cur() == '.' && nxt(1) == '.')
        {
            skip(2);
            skipBlanks();
            steps.add(new XPathExpr.Step(XPathExpr.AXIS_PARENT, XPathExpr.NODE_TEST_TYPE,
                                         XPathExpr.NODE_TYPE_NODE, null, null, new ArrayList<>()));
            return;
        }
        if (cur() == '.')
        {
            next();
            skipBlanks();
            return;
        }
        String name = null;
        int axis;
        if (cur() == '*')
        {
            axis = XPathExpr.AXIS_CHILD;
        }
        else
        {
            name = parseNCName();
            if (name != null)
            {
                axis = axisFromName(name);
                if (axis != 0)
                {
                    skipBlanks();
                    if (cur() == ':' && nxt(1) == ':')
                    {
                        skip(2);
                        name = null;
                    }
                    else
                    {
                        axis = XPathExpr.AXIS_CHILD;
                    }
                }
                else
                {
                    axis = XPathExpr.AXIS_CHILD;
                }
            }
            else if (cur() == '@')
            {
                next();
                axis = XPathExpr.AXIS_ATTRIBUTE;
            }
            else
            {
                axis = XPathExpr.AXIS_CHILD;
            }
        }

        // xmlXPathCompNodeTest
        int test;
        int type = XPathExpr.NODE_TYPE_NODE;
        String prefix = null;
        skipBlanks();
        if (name == null && cur() == '*')
        {
            next();
            test = XPathExpr.NODE_TEST_ALL;
        }
        else
        {
            if (name == null) name = parseNCName();
            if (name == null) throw error(XPathException.EXPR_ERROR);
            boolean blanks = XPathUtil.isBlank(cur());
            skipBlanks();
            if (cur() == '(')
            {
                next();
                if (name.equals("comment")) type = XPathExpr.NODE_TYPE_COMMENT;
                else if (name.equals("node")) type = XPathExpr.NODE_TYPE_NODE;
                else if (name.equals("processing-instruction")) type = XPathExpr.NODE_TYPE_PI;
                else if (name.equals("text")) type = XPathExpr.NODE_TYPE_TEXT;
                else throw error(XPathException.EXPR_ERROR);
                test = XPathExpr.NODE_TEST_TYPE;
                skipBlanks();
                name = null;
                if (type == XPathExpr.NODE_TYPE_PI)
                {
                    if (cur() != ')')
                    {
                        name = parseLiteral();
                        test = XPathExpr.NODE_TEST_PI;
                        skipBlanks();
                    }
                }
                if (cur() != ')') throw error(XPathException.UNCLOSED_ERROR);
                next();
            }
            else
            {
                test = XPathExpr.NODE_TEST_NAME;
                if (!blanks && cur() == ':')
                {
                    next();
                    prefix = name;
                    if (cur() == '*')
                    {
                        next();
                        test = XPathExpr.NODE_TEST_ALL;
                        name = null;
                    }
                    else
                    {
                        name = parseNCName();
                        if (name == null) throw error(XPathException.EXPR_ERROR);
                    }
                }
            }
        }

        List<XPathExpr> preds = new ArrayList<>();
        skipBlanks();
        while (cur() == '[')
        {
            preds.add(compilePredicate(false));
        }
        steps.add(new XPathExpr.Step(axis, test, type, prefix, name, preds));
    }

    // ----------------------------------------------------------------------
    // Optimization, like libxml2 xmlXPathOptimizeExpression: rewrite
    // descendant-or-self::node()/child::x into descendant::x.

    private static void optimize(XPathExpr e)
    {
        if (e instanceof XPathExpr.Path)
        {
            XPathExpr.Path p = (XPathExpr.Path)e;
            List<XPathExpr.Step> steps = p.steps;
            int i = steps.size() - 1;
            while (i >= 1)
            {
                XPathExpr.Step s = steps.get(i);
                XPathExpr.Step prev = steps.get(i - 1);
                if (s.predicates.isEmpty() && prev.isDescendantOrSelfNode())
                {
                    if (s.axis == XPathExpr.AXIS_CHILD || s.axis == XPathExpr.AXIS_DESCENDANT)
                    {
                        s.axis = XPathExpr.AXIS_DESCENDANT;
                        steps.remove(i - 1);
                        i -= 2;
                        continue;
                    }
                    if (s.axis == XPathExpr.AXIS_SELF || s.axis == XPathExpr.AXIS_DESCENDANT_OR_SELF)
                    {
                        s.axis = XPathExpr.AXIS_DESCENDANT_OR_SELF;
                        steps.remove(i - 1);
                        i -= 2;
                        continue;
                    }
                }
                i--;
            }
            for (XPathExpr.Step s : steps)
            {
                for (XPathExpr pr : s.predicates) optimize(pr);
            }
            if (p.filter != null) optimize(p.filter);
        }
        else if (e instanceof XPathExpr.Sort) optimize(((XPathExpr.Sort)e).e);
        else if (e instanceof XPathExpr.Filter)
        {
            optimize(((XPathExpr.Filter)e).e);
            optimize(((XPathExpr.Filter)e).pred);
        }
        else if (e instanceof XPathExpr.Negate) optimize(((XPathExpr.Negate)e).e);
        else if (e instanceof XPathExpr.Call)
        {
            for (XPathExpr a : ((XPathExpr.Call)e).args) optimize(a);
        }
        else if (e instanceof XPathExpr.Or)
        {
            optimize(((XPathExpr.Or)e).l);
            optimize(((XPathExpr.Or)e).r);
        }
        else if (e instanceof XPathExpr.And)
        {
            optimize(((XPathExpr.And)e).l);
            optimize(((XPathExpr.And)e).r);
        }
        else if (e instanceof XPathExpr.Equal)
        {
            optimize(((XPathExpr.Equal)e).l);
            optimize(((XPathExpr.Equal)e).r);
        }
        else if (e instanceof XPathExpr.Compare)
        {
            optimize(((XPathExpr.Compare)e).l);
            optimize(((XPathExpr.Compare)e).r);
        }
        else if (e instanceof XPathExpr.Arith)
        {
            optimize(((XPathExpr.Arith)e).l);
            optimize(((XPathExpr.Arith)e).r);
        }
        else if (e instanceof XPathExpr.Union)
        {
            optimize(((XPathExpr.Union)e).l);
            optimize(((XPathExpr.Union)e).r);
        }
    }
}
