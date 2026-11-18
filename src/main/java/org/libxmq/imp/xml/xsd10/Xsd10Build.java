/* This file is ported from libxml2 (xmlschemas.c, and the helpers
   it uses from error.c, xmlstring.c, tree.c, dict.c and hash.c, version 2.16.0
   as found in the libxmq fork in 3rdparty/libxml2).
   libxml2 is covered by the following notice (from libxml2/Copyright):

 Copyright (C) 1998-2012 Daniel Veillard.  All Rights Reserved.
 Copyright (C) The Libxml2 Contributors.

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
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

package org.libxmq.imp.xml.xsd10;

import java.util.Objects;

import org.libxmq.imp.xml.XmlAutomataState;
import org.libxmq.imp.xml.XmlRegexp;
import org.libxmq.imp.xml.XmlNode;

import static org.libxmq.imp.xml.XmlDomApi.*;
import static org.libxmq.imp.xml.xsd10.Xsd10Const.*;
import static org.libxmq.imp.xml.xsd10.Xsd10Schemas.*;
import static org.libxmq.imp.xml.xsd10.Xsd10Parse.*;
import static org.libxmq.imp.xml.xsd10.Xsd10Valid.*;
import static org.libxmq.imp.xml.xsd10.Xsd10Types.*;

/** Port of xml_xsd10_build.c: content models, fixup, the component constraints and xmlSchemaParse. */
public final class Xsd10Build
{
    private Xsd10Build()
    {
    }

    /* ***********************************************************************
     *                                                                      *
     *                 Building the content models                          *
     *                                                                      *
     ************************************************************************/

    // xml_xsd10_build_content_model_for_subst_group
    /** Port of xmlSchemaBuildContentModelForSubstGroup. @return 1 if nillable, 0 otherwise */
    static int buildContentModelForSubstGroup(Xsd10ParserCtxt pctxt, Xsd10Particle particle, int counter, XmlAutomataState end)
    {
        XmlAutomataState start, tmp;
        Xsd10Element elem_decl, member;
        Xsd10SubstGroup subst_group;
        int i;
        int ret = 0;

        elem_decl = (Xsd10Element) particle.children;
        /*
        * Wrap the substitution group with a CHOICE.
        */
        start = pctxt.state;
        if (end == null)
        {
            end = XmlRegexp.automataNewState(pctxt.am);
        }
        subst_group = substGroupGet(pctxt, elem_decl);
        if (subst_group == null)
        {
            pErr(pctxt, wxsItemNode(particle),
                XML_XSD10_SCHEMAP_INTERNAL,
                "Internal error: xmlSchemaBuildContentModelForSubstGroup, "
                + "declaration is marked having a subst. group but none "
                + "available.\n", elem_decl.name, null);
            return 0;
        }
        if (counter >= 0)
        {
            /*
            * NOTE that we put the declaration in, even if it's abstract.
            * However, an error will be raised during *validation* if an element
            * information item shall be validated against an abstract element
            * declaration.
            */
            tmp = XmlRegexp.automataNewCountedTrans(pctxt.am, start, null, counter);
            XmlRegexp.automataNewTransition2(pctxt.am, tmp, end,
                elem_decl.name, elem_decl.target_namespace, elem_decl);
            /*
            * Add subst. group members.
            */
            for (i = 0; i < subst_group.members.nb_items; i++)
            {
                member = (Xsd10Element) subst_group.members.items[i];
                XmlRegexp.automataNewTransition2(pctxt.am, tmp, end,
                    member.name, member.target_namespace, member);
            }
        }
        else if (particle.max_occurs == 1)
        {
            /*
            * NOTE that we put the declaration in, even if it's abstract,
            */
            XmlRegexp.automataNewEpsilon(pctxt.am,
                XmlRegexp.automataNewTransition2(pctxt.am,
                    start, null,
                    elem_decl.name, elem_decl.target_namespace, elem_decl), end);
            /*
            * Add subst. group members.
            */
            for (i = 0; i < subst_group.members.nb_items; i++)
            {
                member = (Xsd10Element) subst_group.members.items[i];
                /*
                * NOTE: This fixes bug #341150. xmlAutomataNewOnceTrans2()
                *  was incorrectly used instead of xmlAutomataNewTransition2()
                */
                tmp = XmlRegexp.automataNewTransition2(pctxt.am, start, null,
                    member.name, member.target_namespace, member);
                XmlRegexp.automataNewEpsilon(pctxt.am, tmp, end);
            }
        }
        else
        {
            XmlAutomataState hop;
            int max_occurs = particle.max_occurs == XML_XSD10_UNBOUNDED ?
                XML_XSD10_UNBOUNDED : particle.max_occurs - 1;
            int min_occurs = particle.min_occurs < 1 ? 0 : particle.min_occurs - 1;

            counter = XmlRegexp.automataNewCounter(pctxt.am, min_occurs, max_occurs);
            hop = XmlRegexp.automataNewState(pctxt.am);

            XmlRegexp.automataNewEpsilon(pctxt.am,
                XmlRegexp.automataNewTransition2(pctxt.am,
                    start, null,
                    elem_decl.name, elem_decl.target_namespace, elem_decl),
                hop);
            /*
             * Add subst. group members.
             */
            for (i = 0; i < subst_group.members.nb_items; i++)
            {
                member = (Xsd10Element) subst_group.members.items[i];
                XmlRegexp.automataNewEpsilon(pctxt.am,
                    XmlRegexp.automataNewTransition2(pctxt.am,
                        start, null,
                        member.name, member.target_namespace, member),
                    hop);
            }
            XmlRegexp.automataNewCountedTrans(pctxt.am, hop, start, counter);
            XmlRegexp.automataNewCounterTrans(pctxt.am, hop, end, counter);
        }
        if (particle.min_occurs == 0)
        {
            XmlRegexp.automataNewEpsilon(pctxt.am, start, end);
            ret = 1;
        }
        pctxt.state = end;
        return ret;
    }

    // xml_xsd10_build_content_model_for_element
    /** Port of xmlSchemaBuildContentModelForElement. @return 1 if nillable, 0 otherwise */
    static int buildContentModelForElement(Xsd10ParserCtxt ctxt, Xsd10Particle particle)
    {
        int ret = 0;

        if ((((Xsd10Element) particle.children).flags &
            XML_XSD10_SCHEMAS_ELEM_SUBST_GROUP_HEAD) != 0)
        {
            /*
            * Substitution groups.
            */
            ret = buildContentModelForSubstGroup(ctxt, particle, -1, null);
        }
        else
        {
            Xsd10Element elem_decl;
            XmlAutomataState start;

            elem_decl = (Xsd10Element) particle.children;

            if ((elem_decl.flags & XML_XSD10_SCHEMAS_ELEM_ABSTRACT) != 0)
            {
                return 0;
            }
            if (particle.max_occurs == 1)
            {
                start = ctxt.state;
                ctxt.state = XmlRegexp.automataNewTransition2(ctxt.am, start, null,
                    elem_decl.name, elem_decl.target_namespace, elem_decl);
            }
            else if ((particle.max_occurs >= XML_XSD10_UNBOUNDED) &&
                     (particle.min_occurs < 2))
            {
                /* Special case. */
                start = ctxt.state;
                ctxt.state = XmlRegexp.automataNewTransition2(ctxt.am, start, null,
                    elem_decl.name, elem_decl.target_namespace, elem_decl);
                ctxt.state = XmlRegexp.automataNewTransition2(ctxt.am, ctxt.state, ctxt.state,
                    elem_decl.name, elem_decl.target_namespace, elem_decl);
            }
            else
            {
                int counter;
                int max_occurs = particle.max_occurs == XML_XSD10_UNBOUNDED ?
                    XML_XSD10_UNBOUNDED : particle.max_occurs - 1;
                int min_occurs = particle.min_occurs < 1 ?
                    0 : particle.min_occurs - 1;

                start = XmlRegexp.automataNewEpsilon(ctxt.am, ctxt.state, null);
                counter = XmlRegexp.automataNewCounter(ctxt.am, min_occurs, max_occurs);
                ctxt.state = XmlRegexp.automataNewTransition2(ctxt.am, start, null,
                    elem_decl.name, elem_decl.target_namespace, elem_decl);
                XmlRegexp.automataNewCountedTrans(ctxt.am, ctxt.state, start, counter);
                ctxt.state = XmlRegexp.automataNewCounterTrans(ctxt.am, ctxt.state,
                    null, counter);
            }
            if (particle.min_occurs == 0)
            {
                XmlRegexp.automataNewEpsilon(ctxt.am, start, ctxt.state);
                ret = 1;
            }
        }
        return ret;
    }

    // xml_xsd10_build_a_content_model
    /**
     * Port of xmlSchemaBuildAContentModel.
     * Create the automaton for the {content type} of a complex type.
     * @param pctxt the schema parser context
     * @param particle the particle component
     * @return 1 if the content is nillable, 0 otherwise
     */
    static int buildAContentModel(Xsd10ParserCtxt pctxt, Xsd10Particle particle)
    {
        int ret = 0, tmp2;

        if (particle == null)
        {
            perrorInt(pctxt, "xmlSchemaBuildAContentModel", "particle is NULL");
            return 1;
        }
        if (particle.children == null)
        {
            /*
            * Just return in this case. A missing "term" of the particle
            * might arise due to an invalid "term" component.
            */
            return 1;
        }

        switch (particle.children.type)
        {
        case XML_XSD10_SCHEMA_TYPE_ANY:
        {
            XmlAutomataState start, end;
            Xsd10Wildcard wild;
            Xsd10WildcardNs ns;

            wild = (Xsd10Wildcard) particle.children;

            start = pctxt.state;
            end = XmlRegexp.automataNewState(pctxt.am);

            if (particle.max_occurs == 1)
            {
                if (wild.any == 1)
                {
                    /*
                    * We need to add both transitions:
                    *
                    * 1. the {"*", "*"} for elements in a namespace.
                    */
                    pctxt.state =
                        XmlRegexp.automataNewTransition2(pctxt.am,
                            start, null, "*", "*", wild);
                    XmlRegexp.automataNewEpsilon(pctxt.am, pctxt.state, end);
                    /*
                    * 2. the {"*"} for elements in no namespace.
                    */
                    pctxt.state =
                        XmlRegexp.automataNewTransition2(pctxt.am,
                            start, null, "*", null, wild);
                    XmlRegexp.automataNewEpsilon(pctxt.am, pctxt.state, end);

                }
                else if (wild.ns_set != null)
                {
                    ns = wild.ns_set;
                    do
                    {
                        pctxt.state = start;
                        pctxt.state = XmlRegexp.automataNewTransition2(pctxt.am,
                            pctxt.state, null, "*", ns.value, wild);
                        XmlRegexp.automataNewEpsilon(pctxt.am, pctxt.state, end);
                        ns = ns.next;
                    } while (ns != null);

                }
                else if (wild.neg_ns_set != null)
                {
                    pctxt.state = XmlRegexp.automataNewNegTrans(pctxt.am,
                        start, end, "*", wild.neg_ns_set.value,
                        wild);
                }
            }
            else
            {
                int counter;
                XmlAutomataState hop;
                int max_occurs =
                    particle.max_occurs == XML_XSD10_UNBOUNDED ? XML_XSD10_UNBOUNDED :
                    particle.max_occurs - 1;
                int min_occurs =
                    particle.min_occurs < 1 ? 0 : particle.min_occurs - 1;

                counter = XmlRegexp.automataNewCounter(pctxt.am, min_occurs, max_occurs);
                hop = XmlRegexp.automataNewState(pctxt.am);
                if (wild.any == 1)
                {
                    pctxt.state =
                        XmlRegexp.automataNewTransition2(pctxt.am,
                            start, null, "*", "*", wild);
                    XmlRegexp.automataNewEpsilon(pctxt.am, pctxt.state, hop);
                    pctxt.state =
                        XmlRegexp.automataNewTransition2(pctxt.am,
                            start, null, "*", null, wild);
                    XmlRegexp.automataNewEpsilon(pctxt.am, pctxt.state, hop);
                }
                else if (wild.ns_set != null)
                {
                    ns = wild.ns_set;
                    do
                    {
                        pctxt.state =
                            XmlRegexp.automataNewTransition2(pctxt.am,
                                start, null, "*", ns.value, wild);
                        XmlRegexp.automataNewEpsilon(pctxt.am, pctxt.state, hop);
                        ns = ns.next;
                    } while (ns != null);

                }
                else if (wild.neg_ns_set != null)
                {
                    pctxt.state = XmlRegexp.automataNewNegTrans(pctxt.am,
                        start, hop, "*", wild.neg_ns_set.value,
                        wild);
                }
                XmlRegexp.automataNewCountedTrans(pctxt.am, hop, start, counter);
                XmlRegexp.automataNewCounterTrans(pctxt.am, hop, end, counter);
            }
            if (particle.min_occurs == 0)
            {
                XmlRegexp.automataNewEpsilon(pctxt.am, start, end);
                ret = 1;
            }
            pctxt.state = end;
            break;
        }
        case XML_XSD10_SCHEMA_TYPE_ELEMENT:
            ret = buildContentModelForElement(pctxt, particle);
            break;
        case XML_XSD10_SCHEMA_TYPE_SEQUENCE:
        {
            Xsd10TreeItem sub;

            ret = 1;
            /*
             * If max and min occurrences are default (1) then
             * simply iterate over the particles of the <sequence>.
             */
            if ((particle.min_occurs == 1) && (particle.max_occurs == 1))
            {
                sub = (Xsd10TreeItem) ((Xsd10TreeItem) particle.children).children;

                while (sub != null)
                {
                    tmp2 = buildAContentModel(pctxt, (Xsd10Particle) sub);
                    if (tmp2 != 1) ret = 0;
                    sub = sub.next;
                }
            }
            else
            {
                XmlAutomataState oldstate = pctxt.state;

                if (particle.max_occurs >= XML_XSD10_UNBOUNDED)
                {
                    if (particle.min_occurs > 1)
                    {
                        XmlAutomataState tmp;
                        int counter;

                        pctxt.state = XmlRegexp.automataNewEpsilon(pctxt.am,
                            oldstate, null);
                        oldstate = pctxt.state;

                        counter = XmlRegexp.automataNewCounter(pctxt.am,
                            particle.min_occurs - 1, XML_XSD10_UNBOUNDED);

                        sub = (Xsd10TreeItem) ((Xsd10TreeItem) particle.children).children;
                        while (sub != null)
                        {
                            tmp2 = buildAContentModel(pctxt, (Xsd10Particle) sub);
                            if (tmp2 != 1) ret = 0;
                            sub = sub.next;
                        }
                        tmp = pctxt.state;
                        XmlRegexp.automataNewCountedTrans(pctxt.am, tmp,
                            oldstate, counter);
                        pctxt.state =
                            XmlRegexp.automataNewCounterTrans(pctxt.am, tmp,
                                null, counter);
                        if (ret == 1)
                        {
                            XmlRegexp.automataNewEpsilon(pctxt.am,
                                oldstate, pctxt.state);
                        }

                    }
                    else
                    {
                        pctxt.state = XmlRegexp.automataNewEpsilon(pctxt.am,
                            oldstate, null);
                        oldstate = pctxt.state;

                        sub = (Xsd10TreeItem) ((Xsd10TreeItem) particle.children).children;
                        while (sub != null)
                        {
                            tmp2 = buildAContentModel(pctxt, (Xsd10Particle) sub);
                            if (tmp2 != 1) ret = 0;
                            sub = sub.next;
                        }
                        XmlRegexp.automataNewEpsilon(pctxt.am, pctxt.state,
                            oldstate);
                        /*
                         * epsilon needed to block previous trans from
                         * being allowed to enter back from another
                         * construct
                         */
                        pctxt.state = XmlRegexp.automataNewEpsilon(pctxt.am,
                            pctxt.state, null);
                        if (particle.min_occurs == 0)
                        {
                            XmlRegexp.automataNewEpsilon(pctxt.am,
                                oldstate, pctxt.state);
                            ret = 1;
                        }
                    }
                }
                else if ((particle.max_occurs > 1)
                         || (particle.min_occurs > 1))
                {
                    XmlAutomataState tmp;
                    int counter;

                    pctxt.state = XmlRegexp.automataNewEpsilon(pctxt.am,
                        oldstate, null);
                    oldstate = pctxt.state;

                    counter = XmlRegexp.automataNewCounter(pctxt.am,
                        particle.min_occurs - 1,
                        particle.max_occurs - 1);

                    sub = (Xsd10TreeItem) ((Xsd10TreeItem) particle.children).children;
                    while (sub != null)
                    {
                        tmp2 = buildAContentModel(pctxt, (Xsd10Particle) sub);
                        if (tmp2 != 1) ret = 0;
                        sub = sub.next;
                    }
                    tmp = pctxt.state;
                    XmlRegexp.automataNewCountedTrans(pctxt.am,
                        tmp, oldstate, counter);
                    pctxt.state =
                        XmlRegexp.automataNewCounterTrans(pctxt.am, tmp, null,
                            counter);
                    if ((particle.min_occurs == 0) || (ret == 1))
                    {
                        XmlRegexp.automataNewEpsilon(pctxt.am,
                            oldstate, pctxt.state);
                        ret = 1;
                    }
                }
                else
                {
                    sub = (Xsd10TreeItem) ((Xsd10TreeItem) particle.children).children;
                    while (sub != null)
                    {
                        tmp2 = buildAContentModel(pctxt, (Xsd10Particle) sub);
                        if (tmp2 != 1) ret = 0;
                        sub = sub.next;
                    }

                    /*
                     * epsilon needed to block previous trans from
                     * being allowed to enter back from another
                     * construct
                     */
                    pctxt.state = XmlRegexp.automataNewEpsilon(pctxt.am,
                        pctxt.state, null);

                    if (particle.min_occurs == 0)
                    {
                        XmlRegexp.automataNewEpsilon(pctxt.am, oldstate,
                            pctxt.state);
                        ret = 1;
                    }
                }
            }
            break;
        }
        case XML_XSD10_SCHEMA_TYPE_CHOICE:
        {
            Xsd10TreeItem sub;
            XmlAutomataState start, end;

            ret = 0;
            start = pctxt.state;
            end = XmlRegexp.automataNewState(pctxt.am);

            /*
             * iterate over the subtypes and remerge the end with an
             * epsilon transition
             */
            if (particle.max_occurs == 1)
            {
                sub = (Xsd10TreeItem) ((Xsd10TreeItem) particle.children).children;
                while (sub != null)
                {
                    pctxt.state = start;
                    tmp2 = buildAContentModel(pctxt, (Xsd10Particle) sub);
                    if (tmp2 == 1) ret = 1;
                    XmlRegexp.automataNewEpsilon(pctxt.am, pctxt.state, end);
                    sub = sub.next;
                }
            }
            else
            {
                int counter;
                XmlAutomataState hop, base;
                int max_occurs = particle.max_occurs == XML_XSD10_UNBOUNDED ?
                    XML_XSD10_UNBOUNDED : particle.max_occurs - 1;
                int min_occurs =
                    particle.min_occurs < 1 ? 0 : particle.min_occurs - 1;

                /*
                 * use a counter to keep track of the number of transitions
                 * which went through the choice.
                 */
                counter =
                    XmlRegexp.automataNewCounter(pctxt.am, min_occurs, max_occurs);
                hop = XmlRegexp.automataNewState(pctxt.am);
                base = XmlRegexp.automataNewState(pctxt.am);

                sub = (Xsd10TreeItem) ((Xsd10TreeItem) particle.children).children;
                while (sub != null)
                {
                    pctxt.state = base;
                    tmp2 = buildAContentModel(pctxt, (Xsd10Particle) sub);
                    if (tmp2 == 1) ret = 1;
                    XmlRegexp.automataNewEpsilon(pctxt.am, pctxt.state, hop);
                    sub = sub.next;
                }
                XmlRegexp.automataNewEpsilon(pctxt.am, start, base);
                XmlRegexp.automataNewCountedTrans(pctxt.am, hop, base, counter);
                XmlRegexp.automataNewCounterTrans(pctxt.am, hop, end, counter);
                if (ret == 1)
                {
                    XmlRegexp.automataNewEpsilon(pctxt.am, base, end);
                }
            }
            if (particle.min_occurs == 0)
            {
                XmlRegexp.automataNewEpsilon(pctxt.am, start, end);
                ret = 1;
            }
            pctxt.state = end;
            break;
        }
        case XML_XSD10_SCHEMA_TYPE_ALL:
        {
            XmlAutomataState start, tmp;
            Xsd10Particle sub;
            Xsd10Element elem_decl;

            ret = 1;

            sub = (Xsd10Particle) ((Xsd10TreeItem) particle.children).children;
            if (sub == null)
            {
                break;
            }

            ret = 0;

            start = pctxt.state;
            tmp = XmlRegexp.automataNewState(pctxt.am);
            XmlRegexp.automataNewEpsilon(pctxt.am, pctxt.state, tmp);
            pctxt.state = tmp;
            while (sub != null)
            {
                pctxt.state = tmp;

                elem_decl = (sub.children instanceof Xsd10Element) ? (Xsd10Element) sub.children : null;
                if (elem_decl == null)
                {
                    perrorInt(pctxt, "xmlSchemaBuildAContentModel",
                        "<element> particle has no term");
                    return ret;
                }
                /*
                * NOTE: The {max occurs} of all the particles in the
                * {particles} of the group must be 0 or 1; this is
                * already ensured during the parse of the content of
                * <all>.
                */
                if ((elem_decl.flags & XML_XSD10_SCHEMAS_ELEM_SUBST_GROUP_HEAD) != 0)
                {
                    int counter;

                    /*
                     * This is an abstract group, we need to share
                     * the same counter for all the element transitions
                     * derived from the group
                     */
                    counter = XmlRegexp.automataNewCounter(pctxt.am,
                        sub.min_occurs, sub.max_occurs);
                    buildContentModelForSubstGroup(pctxt,
                        sub, counter, pctxt.state);
                }
                else
                {
                    if ((sub.min_occurs == 1) &&
                        (sub.max_occurs == 1))
                    {
                        XmlRegexp.automataNewOnceTrans2(pctxt.am, pctxt.state,
                            pctxt.state,
                            elem_decl.name,
                            elem_decl.target_namespace,
                            1, 1, elem_decl);
                    }
                    else if ((sub.min_occurs == 0) &&
                             (sub.max_occurs == 1))
                    {

                        XmlRegexp.automataNewCountTrans2(pctxt.am, pctxt.state,
                            pctxt.state,
                            elem_decl.name,
                            elem_decl.target_namespace,
                            0,
                            1,
                            elem_decl);
                    }
                }
                sub = (Xsd10Particle) sub.next;
            }
            pctxt.state =
                XmlRegexp.automataNewAllTrans(pctxt.am, pctxt.state, null, 0);
            if (particle.min_occurs == 0)
            {
                XmlRegexp.automataNewEpsilon(pctxt.am, start, pctxt.state);
                ret = 1;
            }
            break;
        }
        case XML_XSD10_SCHEMA_TYPE_GROUP:
            /*
            * If we hit a model group definition, then this means that
            * it was empty, thus was not substituted for the containing
            * model group. Just do nothing in this case.
            */
            ret = 1;
            break;
        default:
            internalErr2(pctxt,
                "xmlSchemaBuildAContentModel",
                "found unexpected term of type '%s' in content model",
                wxsItemTypeName(particle.children), null);
            return ret;
        }
        return ret;
    }

    // xml_xsd10_build_content_model
    /**
     * Port of xmlSchemaBuildContentModel. Builds the content model of the complex type.
     * @param type the complex type definition
     * @param ctxt the schema parser context
     */
    static void buildContentModel(Xsd10Type type, Xsd10ParserCtxt ctxt)
    {
        if ((type.type != XML_XSD10_SCHEMA_TYPE_COMPLEX) ||
            (type.cont_model != null) ||
            ((type.content_type != XML_XSD10_SCHEMA_CONTENT_ELEMENTS) &&
             (type.content_type != XML_XSD10_SCHEMA_CONTENT_MIXED)))
        {
            return;
        }

        ctxt.am = null;
        ctxt.am = XmlRegexp.automataNew();
        if (ctxt.am == null)
        {
            pErrMemory(ctxt);
            return;
        }
        ctxt.state = XmlRegexp.automataGetInitState(ctxt.am);
        /*
        * Build the automaton.
        */
        buildAContentModel(ctxt, (type.subtypes instanceof Xsd10Particle) ? (Xsd10Particle) type.subtypes : null);
        XmlRegexp.automataSetFinalState(ctxt.am, ctxt.state);
        type.cont_model = XmlRegexp.automataCompile(ctxt.am);
        if (type.cont_model == null)
        {
            pCustomErr(ctxt,
                XML_XSD10_SCHEMAP_INTERNAL,
                type, type.node,
                "Failed to compile the content model", null);
        }
        else if (XmlRegexp.regexpIsDeterminist((XmlRegexp) type.cont_model) != 1)
        {
            pCustomErr(ctxt,
                XML_XSD10_SCHEMAP_NOT_DETERMINISTIC,
                /* XML_SCHEMAS_ERR_NOTDETERMINIST, */
                type, type.node,
                "The content model is not determinist", null);
        }
        ctxt.state = null;
        XmlRegexp.automataFree(ctxt.am);
        ctxt.am = null;
    }

    // xml_xsd10_resolve_element_references
    /**
     * Port of xmlSchemaResolveElementReferences. Resolves the references of an element
     * declaration or particle, which has an element declaration as it's term.
     * @param elem_decl the schema element context
     * @param ctxt the schema parser context
     */
    static void resolveElementReferences(Xsd10Element elem_decl, Xsd10ParserCtxt ctxt)
    {
        if ((ctxt == null) || (elem_decl == null) ||
            ((elem_decl.flags & XML_XSD10_SCHEMAS_ELEM_INTERNAL_RESOLVED) != 0))
        {
            return;
        }
        elem_decl.flags |= XML_XSD10_SCHEMAS_ELEM_INTERNAL_RESOLVED;

        if ((elem_decl.subtypes == null) && (elem_decl.named_type != null))
        {
            Xsd10Type type;

            /* (type definition) ... otherwise the type definition `resolved`
            * to by the `actual value` of the type [attribute] ...
            */
            type = getType(ctxt.schema, elem_decl.named_type,
                elem_decl.named_type_ns);
            if (type == null)
            {
                pResCompAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_SRC_RESOLVE,
                    elem_decl, elem_decl.node,
                    "type", elem_decl.named_type, elem_decl.named_type_ns,
                    XML_XSD10_SCHEMA_TYPE_BASIC, "type definition");
            }
            else
            {
                elem_decl.subtypes = type;
            }
        }
        if (elem_decl.subst_group != null)
        {
            Xsd10Element subst_head;

            /*
            * FIXME TODO: Do we need a new field in _xmlSchemaElement for
            * substitutionGroup?
            */
            subst_head = getElem(ctxt.schema, elem_decl.subst_group,
                elem_decl.subst_group_ns);
            if (subst_head == null)
            {
                pResCompAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_SRC_RESOLVE,
                    elem_decl, null,
                    "substitutionGroup", elem_decl.subst_group,
                    elem_decl.subst_group_ns, XML_XSD10_SCHEMA_TYPE_ELEMENT, null);
            }
            else
            {
                resolveElementReferences(subst_head, ctxt);
                /*
                * Set the "substitution group affiliation".
                * NOTE that now we use the "ref_decl" field for this.
                */
                elem_decl.ref_decl = subst_head;
                /*
                * The type definitions is set to:
                * SPEC "...the {type definition} of the element
                * declaration `resolved` to by the `actual value`
                * of the substitutionGroup [attribute], if present"
                */
                if (elem_decl.subtypes == null)
                {
                    if (subst_head.subtypes == null)
                    {
                        /*
                         * This can happen with self-referencing substitution
                         * groups. The cycle will be detected later, but we have
                         * to set subtypes to avoid null-pointer dereferences.
                         */
                        elem_decl.subtypes = getBuiltInType(
                            XML_XSD10_SCHEMAS_ANYTYPE);
                    }
                    else
                    {
                        elem_decl.subtypes = subst_head.subtypes;
                    }
                }
            }
        }
        /*
        * SPEC "The definition of anyType serves as the default type definition
        * for element declarations whose XML representation does not specify one."
        */
        if ((elem_decl.subtypes == null) &&
            (elem_decl.named_type == null) &&
            (elem_decl.subst_group == null))
        {
            elem_decl.subtypes = getBuiltInType(XML_XSD10_SCHEMAS_ANYTYPE);
        }
    }

    // xml_xsd10_resolve_union_member_types
    /**
     * Port of xmlSchemaResolveUnionMemberTypes. Checks and builds the "member type
     * definitions" property of the union simple type.
     * @param ctxt the schema parser context
     * @param type the schema simple type definition
     * @return -1 in case of an internal error, 0 otherwise.
     */
    static int resolveUnionMemberTypes(Xsd10ParserCtxt ctxt, Xsd10Type type)
    {

        Xsd10TypeLink link, last_link;
        Xsd10Type member_type;

        /*
        * Resolve references.
        */
        link = type.member_types;
        last_link = null;
        while (link != null)
        {
            String name, ns_name;

            if (!(link.type instanceof Xsd10QNameRef))
            {
                /* Not reachable in C (only QName refs are here). */
                last_link = link;
                link = link.next;
                continue;
            }
            name = ((Xsd10QNameRef) link.type).name;
            ns_name = ((Xsd10QNameRef) link.type).target_namespace;

            member_type = getType(ctxt.schema, name, ns_name);
            if ((member_type == null) || (wxsIsSimple(member_type) == 0))
            {
                pResCompAttrErr(ctxt, XML_XSD10_SCHEMAP_SRC_RESOLVE,
                    type, type.node, "memberTypes",
                    name, ns_name, XML_XSD10_SCHEMA_TYPE_SIMPLE, null);
                /*
                * Remove the member type link.
                */
                if (last_link == null)
                {
                    type.member_types = link.next;
                }
                else
                {
                    last_link.next = link.next;
                }
                link = link.next;
            }
            else
            {
                link.type = member_type;
                last_link = link;
                link = link.next;
            }
        }
        /*
        * Add local simple types,
        */
        member_type = asType(type.subtypes);
        while (member_type != null)
        {
            link = new Xsd10TypeLink();
            link.type = member_type;
            link.next = null;
            if (last_link == null)
            {
                type.member_types = link;
            }
            else
            {
                last_link.next = link;
            }
            last_link = link;
            member_type = member_type.next;
        }
        return 0;
    }

    // xml_xsd10_is_derived_from_built_in_type
    /**
     * Port of xmlSchemaIsDerivedFromBuiltInType.
     * @param type the type definition
     * @param val_type the value type
     * @return 1 if the type has the given value type, or is derived from such a type.
     */
    static int isDerivedFromBuiltInType(Xsd10Type type, int val_type)
    {
        if (type == null)
        {
            return 0;
        }
        if (wxsIsComplex(type) != 0)
        {
            return 0;
        }
        if (type.type == XML_XSD10_SCHEMA_TYPE_BASIC)
        {
            if (type.built_in_type == val_type)
            {
                return 1;
            }
            if ((type.built_in_type == XML_XSD10_SCHEMAS_ANYSIMPLETYPE) ||
                (type.built_in_type == XML_XSD10_SCHEMAS_ANYTYPE))
            {
                return 0;
            }
            return isDerivedFromBuiltInType(asType(type.subtypes), val_type);
        }
        return isDerivedFromBuiltInType(asType(type.subtypes), val_type);
    }

    // xml_xsd10_get_primitive_type
    /**
     * Port of xmlSchemaGetPrimitiveType.
     * @param type the simpleType definition
     * @return the primitive type of the given type or null in case of error.
     */
    static Xsd10Type getPrimitiveType(Xsd10Type type)
    {

        while (type != null)
        {
            /*
            * Note that anySimpleType is actually not a primitive type
            * but we need that here.
            */
            if ((type.built_in_type == XML_XSD10_SCHEMAS_ANYSIMPLETYPE) ||
                ((type.flags & XML_XSD10_SCHEMAS_TYPE_BUILTIN_PRIMITIVE) != 0))
            {
                return type;
            }
            type = type.base_type;
        }

        return null;
    }

    // xml_xsd10_clone_wildcard_ns_constraints
    /**
     * Port of xmlSchemaCloneWildcardNsConstraints. Clones the namespace constraints of
     * source and assigns them to dest.
     * @param ctxt the schema parser context
     * @param dest the destination wildcard
     * @param source the source wildcard
     * @return -1 on internal error, 0 otherwise.
     */
    static int cloneWildcardNsConstraints(Xsd10ParserCtxt ctxt, Xsd10Wildcard dest, Xsd10Wildcard source)
    {
        Xsd10WildcardNs cur, tmp, last;

        if ((source == null) || (dest == null))
        {
            return -1;
        }
        dest.any = source.any;
        cur = source.ns_set;
        last = null;
        while (cur != null)
        {
            tmp = newWildcardNsConstraint(ctxt);
            if (tmp == null)
            {
                return -1;
            }
            tmp.value = cur.value;
            if (last == null)
            {
                dest.ns_set = tmp;
            }
            else
            {
                last.next = tmp;
            }
            last = tmp;
            cur = cur.next;
        }
        if (dest.neg_ns_set != null)
        {
            freeWildcardNsSet(dest.neg_ns_set);
        }
        if (source.neg_ns_set != null)
        {
            dest.neg_ns_set = newWildcardNsConstraint(ctxt);
            if (dest.neg_ns_set == null)
            {
                return -1;
            }
            dest.neg_ns_set.value = source.neg_ns_set.value;
        }
        else
        {
            dest.neg_ns_set = null;
        }
        return 0;
    }

    // xml_xsd10_union_wildcards
    /**
     * Port of xmlSchemaUnionWildcards. Unions the namespace constraints of the given
     * wildcards. complete_wild will hold the resulting union.
     * @param ctxt the schema parser context
     * @param complete_wild the first wildcard
     * @param cur_wild the second wildcard
     * @return a positive error code on failure, -1 in case of an internal error, 0 otherwise.
     */
    static int unionWildcards(Xsd10ParserCtxt ctxt, Xsd10Wildcard complete_wild, Xsd10Wildcard cur_wild)
    {
        Xsd10WildcardNs cur, cur_b, tmp;

        /*
        * 1 If O1 and O2 are the same value, then that value must be the
        * value.
        */
        if ((complete_wild.any == cur_wild.any) &&
            ((complete_wild.ns_set == null) == (cur_wild.ns_set == null)) &&
            ((complete_wild.neg_ns_set == null) == (cur_wild.neg_ns_set == null)))
        {

            if ((complete_wild.neg_ns_set == null) ||
                Objects.equals(complete_wild.neg_ns_set.value, cur_wild.neg_ns_set.value))
            {

                if (complete_wild.ns_set != null)
                {
                    int found = 0;

                    /*
                    * Check equality of sets.
                    */
                    cur = complete_wild.ns_set;
                    while (cur != null)
                    {
                        found = 0;
                        cur_b = cur_wild.ns_set;
                        while (cur_b != null)
                        {
                            if (Objects.equals(cur.value, cur_b.value))
                            {
                                found = 1;
                                break;
                            }
                            cur_b = cur_b.next;
                        }
                        if (found == 0)
                        {
                            break;
                        }
                        cur = cur.next;
                    }
                    if (found != 0)
                    {
                        return 0;
                    }
                }
                else
                {
                    return 0;
                }
            }
        }
        /*
        * 2 If either O1 or O2 is any, then any must be the value
        */
        if (complete_wild.any != cur_wild.any)
        {
            if (complete_wild.any == 0)
            {
                complete_wild.any = 1;
                if (complete_wild.ns_set != null)
                {
                    freeWildcardNsSet(complete_wild.ns_set);
                    complete_wild.ns_set = null;
                }
                if (complete_wild.neg_ns_set != null)
                {
                    complete_wild.neg_ns_set = null;
                }
            }
            return 0;
        }
        /*
        * 3 If both O1 and O2 are sets of (namespace names or `absent`),
        * then the union of those sets must be the value.
        */
        if ((complete_wild.ns_set != null) && (cur_wild.ns_set != null))
        {
            int found;
            Xsd10WildcardNs start;

            cur = cur_wild.ns_set;
            start = complete_wild.ns_set;
            while (cur != null)
            {
                found = 0;
                cur_b = start;
                while (cur_b != null)
                {
                    if (Objects.equals(cur.value, cur_b.value))
                    {
                        found = 1;
                        break;
                    }
                    cur_b = cur_b.next;
                }
                if (found == 0)
                {
                    tmp = newWildcardNsConstraint(ctxt);
                    if (tmp == null)
                    {
                        return -1;
                    }
                    tmp.value = cur.value;
                    tmp.next = complete_wild.ns_set;
                    complete_wild.ns_set = tmp;
                }
                cur = cur.next;
            }

            return 0;
        }
        /*
        * 4 If the two are negations of different values (namespace names
        * or `absent`), then a pair of not and `absent` must be the value.
        */
        if ((complete_wild.neg_ns_set != null) &&
            (cur_wild.neg_ns_set != null) &&
            !Objects.equals(complete_wild.neg_ns_set.value, cur_wild.neg_ns_set.value))
        {
            complete_wild.neg_ns_set.value = null;

            return 0;
        }
        /*
         * 5.
         */
        if (((complete_wild.neg_ns_set != null) &&
             (complete_wild.neg_ns_set.value != null) &&
             (cur_wild.ns_set != null)) ||
            ((cur_wild.neg_ns_set != null) &&
             (cur_wild.neg_ns_set.value != null) &&
             (complete_wild.ns_set != null)))
        {

            int ns_found, absent_found = 0;

            if (complete_wild.ns_set != null)
            {
                cur = complete_wild.ns_set;
                cur_b = cur_wild.neg_ns_set;
            }
            else
            {
                cur = cur_wild.ns_set;
                cur_b = complete_wild.neg_ns_set;
            }
            ns_found = 0;
            while (cur != null)
            {
                if (cur.value == null)
                {
                    absent_found = 1;
                }
                else if (Objects.equals(cur.value, cur_b.value))
                {
                    ns_found = 1;
                }
                if ((ns_found != 0) && (absent_found != 0))
                {
                    break;
                }
                cur = cur.next;
            }

            if ((ns_found != 0) && (absent_found != 0))
            {
                /*
                * 5.1 If the set S includes both the negated namespace
                * name and `absent`, then any must be the value.
                */
                complete_wild.any = 1;
                if (complete_wild.ns_set != null)
                {
                    freeWildcardNsSet(complete_wild.ns_set);
                    complete_wild.ns_set = null;
                }
                if (complete_wild.neg_ns_set != null)
                {
                    complete_wild.neg_ns_set = null;
                }
            }
            else if ((ns_found != 0) && (absent_found == 0))
            {
                /*
                * 5.2 If the set S includes the negated namespace name
                * but not `absent`, then a pair of not and `absent` must
                * be the value.
                */
                if (complete_wild.ns_set != null)
                {
                    freeWildcardNsSet(complete_wild.ns_set);
                    complete_wild.ns_set = null;
                }
                if (complete_wild.neg_ns_set == null)
                {
                    complete_wild.neg_ns_set = newWildcardNsConstraint(ctxt);
                    if (complete_wild.neg_ns_set == null)
                    {
                        return -1;
                    }
                }
                complete_wild.neg_ns_set.value = null;
            }
            else if ((ns_found == 0) && (absent_found != 0))
            {
                /*
                * 5.3 If the set S includes `absent` but not the negated
                * namespace name, then the union is not expressible.
                */
                pErr(ctxt, complete_wild.node,
                    XML_XSD10_SCHEMAP_UNION_NOT_EXPRESSIBLE,
                    "The union of the wildcard is not expressible.\n",
                    null, null);
                return XML_XSD10_SCHEMAP_UNION_NOT_EXPRESSIBLE;
            }
            else if ((ns_found == 0) && (absent_found == 0))
            {
                /*
                * 5.4 If the set S does not include either the negated namespace
                * name or `absent`, then whichever of O1 or O2 is a pair of not
                * and a namespace name must be the value.
                */
                if (complete_wild.neg_ns_set == null)
                {
                    if (complete_wild.ns_set != null)
                    {
                        freeWildcardNsSet(complete_wild.ns_set);
                        complete_wild.ns_set = null;
                    }
                    complete_wild.neg_ns_set = newWildcardNsConstraint(ctxt);
                    if (complete_wild.neg_ns_set == null)
                    {
                        return -1;
                    }
                    complete_wild.neg_ns_set.value = cur_wild.neg_ns_set.value;
                }
            }
            return 0;
        }
        /*
         * 6.
         */
        if (((complete_wild.neg_ns_set != null) &&
             (complete_wild.neg_ns_set.value == null) &&
             (cur_wild.ns_set != null)) ||
            ((cur_wild.neg_ns_set != null) &&
             (cur_wild.neg_ns_set.value == null) &&
             (complete_wild.ns_set != null)))
        {

            if (complete_wild.ns_set != null)
            {
                cur = complete_wild.ns_set;
            }
            else
            {
                cur = cur_wild.ns_set;
            }
            while (cur != null)
            {
                if (cur.value == null)
                {
                    /*
                    * 6.1 If the set S includes `absent`, then any must be the
                    * value.
                    */
                    complete_wild.any = 1;
                    if (complete_wild.ns_set != null)
                    {
                        freeWildcardNsSet(complete_wild.ns_set);
                        complete_wild.ns_set = null;
                    }
                    if (complete_wild.neg_ns_set != null)
                    {
                        complete_wild.neg_ns_set = null;
                    }
                    return 0;
                }
                cur = cur.next;
            }
            if (complete_wild.neg_ns_set == null)
            {
                /*
                * 6.2 If the set S does not include `absent`, then a pair of not
                * and `absent` must be the value.
                */
                if (complete_wild.ns_set != null)
                {
                    freeWildcardNsSet(complete_wild.ns_set);
                    complete_wild.ns_set = null;
                }
                complete_wild.neg_ns_set = newWildcardNsConstraint(ctxt);
                if (complete_wild.neg_ns_set == null)
                {
                    return -1;
                }
                complete_wild.neg_ns_set.value = null;
            }
            return 0;
        }
        return 0;

    }

    // xml_xsd10_intersect_wildcards
    /**
     * Port of xmlSchemaIntersectWildcards. Intersects the namespace constraints of the
     * given wildcards. complete_wild will hold the resulting intersection.
     * @param ctxt the schema parser context
     * @param complete_wild the first wildcard
     * @param cur_wild the second wildcard
     * @return a positive error code on failure, -1 in case of an internal error, 0 otherwise.
     */
    static int intersectWildcards(Xsd10ParserCtxt ctxt, Xsd10Wildcard complete_wild, Xsd10Wildcard cur_wild)
    {
        Xsd10WildcardNs cur, cur_b, prev, tmp;

        /*
        * 1 If O1 and O2 are the same value, then that value must be the
        * value.
        */
        if ((complete_wild.any == cur_wild.any) &&
            ((complete_wild.ns_set == null) == (cur_wild.ns_set == null)) &&
            ((complete_wild.neg_ns_set == null) == (cur_wild.neg_ns_set == null)))
        {

            if ((complete_wild.neg_ns_set == null) ||
                Objects.equals(complete_wild.neg_ns_set.value, cur_wild.neg_ns_set.value))
            {

                if (complete_wild.ns_set != null)
                {
                    int found = 0;

                    /*
                    * Check equality of sets.
                    */
                    cur = complete_wild.ns_set;
                    while (cur != null)
                    {
                        found = 0;
                        cur_b = cur_wild.ns_set;
                        while (cur_b != null)
                        {
                            if (Objects.equals(cur.value, cur_b.value))
                            {
                                found = 1;
                                break;
                            }
                            cur_b = cur_b.next;
                        }
                        if (found == 0)
                        {
                            break;
                        }
                        cur = cur.next;
                    }
                    if (found != 0)
                    {
                        return 0;
                    }
                }
                else
                {
                    return 0;
                }
            }
        }
        /*
        * 2 If either O1 or O2 is any, then the other must be the value.
        */
        if ((complete_wild.any != cur_wild.any) && (complete_wild.any != 0))
        {
            if (cloneWildcardNsConstraints(ctxt, complete_wild, cur_wild) == -1)
            {
                return -1;
            }
            return 0;
        }
        /*
        * 3 If either O1 or O2 is a pair of not and a value (a namespace
        * name or `absent`) and the other is a set of (namespace names or
        * `absent`), then that set, minus the negated value if it was in
        * the set, minus `absent` if it was in the set, must be the value.
        */
        if (((complete_wild.neg_ns_set != null) && (cur_wild.ns_set != null)) ||
            ((cur_wild.neg_ns_set != null) && (complete_wild.ns_set != null)))
        {
            String neg;

            if (complete_wild.ns_set == null)
            {
                neg = complete_wild.neg_ns_set.value;
                if (cloneWildcardNsConstraints(ctxt, complete_wild, cur_wild) == -1)
                {
                    return -1;
                }
            }
            else
            {
                neg = cur_wild.neg_ns_set.value;
            }
            /*
            * Remove absent and negated.
            */
            prev = null;
            cur = complete_wild.ns_set;
            while (cur != null)
            {
                if (cur.value == null)
                {
                    if (prev == null)
                    {
                        complete_wild.ns_set = cur.next;
                    }
                    else
                    {
                        prev.next = cur.next;
                    }
                    break;
                }
                prev = cur;
                cur = cur.next;
            }
            if (neg != null)
            {
                prev = null;
                cur = complete_wild.ns_set;
                while (cur != null)
                {
                    if (Objects.equals(cur.value, neg))
                    {
                        if (prev == null)
                        {
                            complete_wild.ns_set = cur.next;
                        }
                        else
                        {
                            prev.next = cur.next;
                        }
                        break;
                    }
                    prev = cur;
                    cur = cur.next;
                }
            }

            return 0;
        }
        /*
        * 4 If both O1 and O2 are sets of (namespace names or `absent`),
        * then the intersection of those sets must be the value.
        */
        if ((complete_wild.ns_set != null) && (cur_wild.ns_set != null))
        {
            int found;

            cur = complete_wild.ns_set;
            prev = null;
            while (cur != null)
            {
                found = 0;
                cur_b = cur_wild.ns_set;
                while (cur_b != null)
                {
                    if (Objects.equals(cur.value, cur_b.value))
                    {
                        found = 1;
                        break;
                    }
                    cur_b = cur_b.next;
                }
                if (found == 0)
                {
                    if (prev == null)
                    {
                        complete_wild.ns_set = cur.next;
                    }
                    else
                    {
                        prev.next = cur.next;
                    }
                    tmp = cur.next;
                    cur = tmp;
                    continue;
                }
                prev = cur;
                cur = cur.next;
            }

            return 0;
        }
        /* 5 If the two are negations of different namespace names,
        * then the intersection is not expressible
        */
        if ((complete_wild.neg_ns_set != null) &&
            (cur_wild.neg_ns_set != null) &&
            !Objects.equals(complete_wild.neg_ns_set.value, cur_wild.neg_ns_set.value) &&
            (complete_wild.neg_ns_set.value != null) &&
            (cur_wild.neg_ns_set.value != null))
        {

            pErr(ctxt, complete_wild.node, XML_XSD10_SCHEMAP_INTERSECTION_NOT_EXPRESSIBLE,
                "The intersection of the wildcard is not expressible.\n",
                null, null);
            return XML_XSD10_SCHEMAP_INTERSECTION_NOT_EXPRESSIBLE;
        }
        /*
        * 6 If the one is a negation of a namespace name and the other
        * is a negation of `absent`, then the one which is the negation
        * of a namespace name must be the value.
        */
        if ((complete_wild.neg_ns_set != null) && (cur_wild.neg_ns_set != null) &&
            !Objects.equals(complete_wild.neg_ns_set.value, cur_wild.neg_ns_set.value) &&
            (complete_wild.neg_ns_set.value == null))
        {
            complete_wild.neg_ns_set.value = cur_wild.neg_ns_set.value;
        }
        return 0;
    }

    // xml_xsd10_check_cosns_subset
    /**
     * Port of xmlSchemaCheckCOSNSSubset. Schema Component Constraint: Wildcard Subset
     * (cos-ns-subset)
     * @param sub the first wildcard
     * @param super_ the second wildcard
     * @return 0 if the namespace constraint of sub is an intensional subset of super, 1 otherwise.
     */
    static int checkCosnsSubset(Xsd10Wildcard sub, Xsd10Wildcard super_)
    {
        /*
        * 1 super must be any.
        */
        if (super_.any != 0)
        {
            return 0;
        }
        /*
        * 2.1 sub must be a pair of not and a namespace name or `absent`.
        * 2.2 super must be a pair of not and the same value.
        */
        if ((sub.neg_ns_set != null) &&
            (super_.neg_ns_set != null) &&
            Objects.equals(sub.neg_ns_set.value, super_.neg_ns_set.value))
        {
            return 0;
        }
        /*
        * 3.1 sub must be a set whose members are either namespace names or `absent`.
        */
        if (sub.ns_set != null)
        {
            /*
            * 3.2.1 super must be the same set or a superset thereof.
            */
            if (super_.ns_set != null)
            {
                Xsd10WildcardNs cur, cur_b;
                int found = 0;

                cur = sub.ns_set;
                while (cur != null)
                {
                    found = 0;
                    cur_b = super_.ns_set;
                    while (cur_b != null)
                    {
                        if (Objects.equals(cur.value, cur_b.value))
                        {
                            found = 1;
                            break;
                        }
                        cur_b = cur_b.next;
                    }
                    if (found == 0)
                    {
                        return 1;
                    }
                    cur = cur.next;
                }
                if (found != 0)
                {
                    return 0;
                }
            }
            else if (super_.neg_ns_set != null)
            {
                Xsd10WildcardNs cur;
                /*
                * 3.2.2 super must be a pair of not and a namespace name or
                * `absent` and that value must not be in sub's set.
                */
                cur = sub.ns_set;
                while (cur != null)
                {
                    if (Objects.equals(cur.value, super_.neg_ns_set.value))
                    {
                        return 1;
                    }
                    cur = cur.next;
                }
                return 0;
            }
        }
        return 1;
    }

    // xml_xsd10_get_effective_value_constraint
    /** Port of xmlSchemaGetEffectiveValueConstraint. */
    static int getEffectiveValueConstraint(Xsd10AttributeUse attruse, int[] fixed, String[] value, Xsd10Val[] val)
    {
        fixed[0] = 0;
        value[0] = null;
        if (val != null)
        {
            val[0] = null;
        }

        if (attruse.def_value != null)
        {
            value[0] = attruse.def_value;
            if (val != null)
            {
                val[0] = attruse.def_val;
            }
            if ((attruse.flags & XML_XSD10_SCHEMA_ATTR_USE_FIXED) != 0)
            {
                fixed[0] = 1;
            }
            return 1;
        }
        else if ((attruse.attr_decl instanceof Xsd10Attribute) &&
                 (((Xsd10Attribute) attruse.attr_decl).def_value != null))
        {
            Xsd10Attribute decl = (Xsd10Attribute) attruse.attr_decl;
            value[0] = decl.def_value;
            if (val != null)
            {
                val[0] = decl.def_val;
            }
            if ((decl.flags & XML_XSD10_SCHEMAS_ATTR_FIXED) != 0)
            {
                fixed[0] = 1;
            }
            return 1;
        }
        return 0;
    }

    // xml_xsd10_check_cvc_wildcard_namespace
    /**
     * Port of xmlSchemaCheckCVCWildcardNamespace. Validation Rule: Wildcard allows
     * Namespace Name (cvc-wildcard-namespace)
     * @param wild the wildcard
     * @param ns the namespace
     * @return 0 if the given namespace matches the wildcard, 1 otherwise and -1 on API errors.
     */
    static int checkCvcWildcardNamespace(Xsd10Wildcard wild, String ns)
    {
        if (wild == null)
        {
            return -1;
        }

        if (wild.any != 0)
        {
            return 0;
        }
        else if (wild.ns_set != null)
        {
            Xsd10WildcardNs cur;

            cur = wild.ns_set;
            while (cur != null)
            {
                if (strEqual(cur.value, ns) != 0)
                {
                    return 0;
                }
                cur = cur.next;
            }
        }
        else if ((wild.neg_ns_set != null) && (ns != null) &&
                 (strEqual(wild.neg_ns_set.value, ns) == 0))
        {
            return 0;
        }

        return 1;
    }

    private static final int XML_XSD10_SCHEMA_ACTION_DERIVE = 0;
    private static final int XML_XSD10_SCHEMA_ACTION_REDEFINE = 1;

    // XSD10_WXS_ACTION_STR
    private static String wxsActionStr(int a)
    {
        return (a == XML_XSD10_SCHEMA_ACTION_DERIVE) ? "base" : "redefined";
    }

    /** Cast helper: C casts of void-ish component pointers to XmlXsd10Type. */
    private static Xsd10Type asType(Object o)
    {
        return (o instanceof Xsd10Type) ? (Xsd10Type) o : null;
    }

    // xml_xsd10_check_derivation_ok_restriction2to4
    /**
     * Port of xmlSchemaCheckDerivationOKRestriction2to4. Schema Component Constraint:
     * Derivation Valid (Restriction, Complex) derivation-ok-restriction (2) - (4)
     */
    static int checkDerivationOkRestriction2to4(Xsd10ParserCtxt pctxt, int action, Xsd10BasicItem item, Xsd10BasicItem base_item, Xsd10ItemList uses, Xsd10ItemList base_uses, Xsd10Wildcard wild, Xsd10Wildcard base_wild)
    {
        Xsd10AttributeUse cur = null, bcur;
        int i, j, found; /* err = 0; */
        String b_eff_value;
        int eff_fixed;

        if (uses != null)
        {
            for (i = 0; i < uses.nb_items; i++)
            {
                cur = (Xsd10AttributeUse) uses.items[i];
                found = 0;
                if (base_uses != null)
                {
                    for (j = 0; j < base_uses.nb_items; j++)
                    {
                        bcur = (Xsd10AttributeUse) base_uses.items[j];
                        if (Objects.equals(wxsAttruseDeclName(cur),
                                wxsAttruseDeclName(bcur)) &&
                            Objects.equals(wxsAttruseDeclTns(cur),
                                wxsAttruseDeclTns(bcur)))
                        {
                            /*
                            * (2.1) "If there is an attribute use in the {attribute
                            * uses} of the {base type definition} (call this B) whose
                            * {attribute declaration} has the same {name} and {target
                            * namespace}, then  all of the following must be true:"
                            */
                            found = 1;

                            if ((cur.occurs == XML_XSD10_SCHEMAS_ATTR_USE_OPTIONAL) &&
                                (bcur.occurs == XML_XSD10_SCHEMAS_ATTR_USE_REQUIRED))
                            {
                                String[] str = new String[1];
                                /*
                                * (2.1.1) "one of the following must be true:"
                                * (2.1.1.1) "B's {required} is false."
                                * (2.1.1.2) "R's {required} is true."
                                */
                                pAttrUseErr4(pctxt,
                                    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_2_1_1,
                                    wxsItemNode(item), item, cur,
                                    "The 'optional' attribute use is inconsistent "
                                    + "with the corresponding 'required' attribute use of "
                                    + "the %s %s",
                                    wxsActionStr(action),
                                    getComponentDesignation(str, base_item),
                                    null, null);
                                /* err = pctxt->err; */
                            }
                            else if (checkCosstDerivedOk(pctxt,
                                         wxsAttruseTypedef(cur),
                                         wxsAttruseTypedef(bcur), 0) != 0)
                            {
                                String[] str_a = new String[1], str_b = new String[1], str_c = new String[1];

                                /*
                                * SPEC (2.1.2) "R's {attribute declaration}'s
                                * {type definition} must be validly derived from
                                * B's {type definition} given the empty set as
                                * defined in Type Derivation OK (Simple) ($3.14.6)."
                                */
                                pAttrUseErr4(pctxt,
                                    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_2_1_2,
                                    wxsItemNode(item), item, cur,
                                    "The attribute declaration's %s "
                                    + "is not validly derived from "
                                    + "the corresponding %s of the "
                                    + "attribute declaration in the %s %s",
                                    getComponentDesignation(str_a,
                                        wxsAttruseTypedef(cur)),
                                    getComponentDesignation(str_b,
                                        wxsAttruseTypedef(bcur)),
                                    wxsActionStr(action),
                                    getComponentDesignation(str_c, base_item));
                                /* err = pctxt->err; */
                            }
                            else
                            {
                                int[] fx = new int[1];
                                String[] sv = new String[1];
                                /*
                                * 2.1.3 [Definition:]  Let the effective value
                                * constraint of an attribute use be its {value
                                * constraint}, if present, otherwise its {attribute
                                * declaration}'s {value constraint} .
                                */
                                getEffectiveValueConstraint(bcur,
                                    fx, sv, null);
                                eff_fixed = fx[0];
                                b_eff_value = sv[0];
                                /*
                                * 2.1.3 ... one of the following must be true
                                *
                                * 2.1.3.1 B's `effective value constraint` is
                                * `absent` or default.
                                */
                                if ((b_eff_value != null) &&
                                    (eff_fixed == 1))
                                {
                                    String r_eff_value = null;

                                    getEffectiveValueConstraint(bcur,
                                        fx, sv, null);
                                    eff_fixed = fx[0];
                                    r_eff_value = sv[0];
                                    /*
                                    * 2.1.3.2 R's `effective value constraint` is
                                    * fixed with the same string as B's.
                                    */
                                    if ((eff_fixed == 0) ||
                                        (wxsAreDefaultStrEqual(r_eff_value, b_eff_value) == 0))
                                    {
                                        String[] str = new String[1];

                                        pAttrUseErr4(pctxt,
                                            XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_2_1_3,
                                            wxsItemNode(item), item, cur,
                                            "The effective value constraint of the "
                                            + "attribute use is inconsistent with "
                                            + "its correspondent in the %s %s",
                                            wxsActionStr(action),
                                            getComponentDesignation(str,
                                                base_item),
                                            null, null);
                                        /* err = pctxt->err; */
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
                /* not_found: */
                if (found == 0)
                {
                    /*
                    * (2.2) "otherwise the {base type definition} must have an
                    * {attribute wildcard} and the {target namespace} of the
                    * R's {attribute declaration} must be `valid` with respect
                    * to that wildcard, as defined in Wildcard allows Namespace
                    * Name ($3.10.4)."
                    */
                    if ((base_wild == null) ||
                        (checkCvcWildcardNamespace(base_wild,
                            (wxsAttruseDecl(cur)).target_namespace) != 0))
                    {
                        String[] str = new String[1];

                        pAttrUseErr4(pctxt,
                            XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_2_2,
                            wxsItemNode(item), item, cur,
                            "Neither a matching attribute use, "
                            + "nor a matching wildcard exists in the %s %s",
                            wxsActionStr(action),
                            getComponentDesignation(str, base_item),
                            null, null);
                        /* err = pctxt->err; */
                    }
                }
            }
        }
        /*
        * SPEC derivation-ok-restriction (3):
        * (3) "For each attribute use in the {attribute uses} of the {base type
        * definition} whose {required} is true, there must be an attribute
        * use with an {attribute declaration} with the same {name} and
        * {target namespace} as its {attribute declaration} in the {attribute
        * uses} of the complex type definition itself whose {required} is true.
        */
        if (base_uses != null)
        {
            for (j = 0; j < base_uses.nb_items; j++)
            {
                bcur = (Xsd10AttributeUse) base_uses.items[j];
                if (bcur.occurs != XML_XSD10_SCHEMAS_ATTR_USE_REQUIRED)
                {
                    continue;
                }
                found = 0;
                if (uses != null)
                {
                    for (i = 0; i < uses.nb_items; i++)
                    {
                        cur = (Xsd10AttributeUse) uses.items[i];
                        if (Objects.equals(wxsAttruseDeclName(cur),
                                wxsAttruseDeclName(bcur)) &&
                            Objects.equals(wxsAttruseDeclTns(cur),
                                wxsAttruseDeclTns(bcur)))
                        {
                            found = 1;
                            break;
                        }
                    }
                }
                if (found == 0)
                {
                    String[] str_a = new String[1], str_b = new String[1];

                    customErr4(pctxt,
                        XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_3,
                        null, item,
                        "A matching attribute use for the "
                        + "'required' %s of the %s %s is missing",
                        getComponentDesignation(str_a, bcur),
                        wxsActionStr(action),
                        getComponentDesignation(str_b, base_item),
                        null);
                }
            }
        }
        /*
        * derivation-ok-restriction (4)
        */
        if (wild != null)
        {
            /*
            * (4) "If there is an {attribute wildcard}, all of the
            * following must be true:"
            */
            if (base_wild == null)
            {
                String[] str = new String[1];

                /*
                * (4.1) "The {base type definition} must also have one."
                */
                customErr4(pctxt,
                    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_4_1,
                    null, item,
                    "The %s has an attribute wildcard, "
                    + "but the %s %s '%s' does not have one",
                    wxsItemTypeName(item),
                    wxsActionStr(action),
                    wxsItemTypeName(base_item),
                    getComponentQName(str, base_item));
                return pctxt.err;
            }
            else if ((base_wild.any == 0) &&
                     (checkCosnsSubset(wild, base_wild) != 0))
            {
                String[] str = new String[1];
                /*
                * (4.2) "The complex type definition's {attribute wildcard}'s
                * {namespace constraint} must be a subset of the {base type
                * definition}'s {attribute wildcard}'s {namespace constraint},
                * as defined by Wildcard Subset ($3.10.6)."
                */
                customErr4(pctxt,
                    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_4_2,
                    null, item,
                    "The attribute wildcard is not a valid "
                    + "subset of the wildcard in the %s %s '%s'",
                    wxsActionStr(action),
                    wxsItemTypeName(base_item),
                    getComponentQName(str, base_item),
                    null);
                return pctxt.err;
            }
            /* 4.3 Unless the {base type definition} is the `ur-type
            * definition`, the complex type definition's {attribute
            * wildcard}'s {process contents} must be identical to or
            * stronger than the {base type definition}'s {attribute
            * wildcard}'s {process contents}, where strict is stronger
            * than lax is stronger than skip.
            */
            if ((wxsIsAnytype(base_item) == 0) &&
                (wild.process_contents < base_wild.process_contents))
            {
                String[] str = new String[1];
                customErr4(pctxt,
                    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_4_3,
                    null, base_item,
                    "The {process contents} of the attribute wildcard is "
                    + "weaker than the one in the %s %s '%s'",
                    wxsActionStr(action),
                    wxsItemTypeName(base_item),
                    getComponentQName(str, base_item),
                    null);
                return pctxt.err;
            }
        }
        return 0;
    }

    // xml_xsd10_fixup_type_attribute_uses
    /**
     * Port of xmlSchemaFixupTypeAttributeUses. Builds the wildcard and the attribute uses
     * on the given complex type.
     * @param pctxt the schema parser context
     * @param type the complex type definition
     * @return -1 if an internal error occurs, 0 otherwise.
     */
    static int fixupTypeAttributeUses(Xsd10ParserCtxt pctxt, Xsd10Type type)
    {
        Xsd10Type base_type = null;
        Xsd10AttributeUse use;
        Xsd10ItemList uses, base_uses, prohibs = null;

        if (type.base_type == null)
        {
            perrorInt(pctxt, "xmlSchemaFixupTypeAttributeUses",
                "no base type");
            return -1;
        }
        base_type = type.base_type;
        if (wxsIsTypeNotFixed(base_type) != 0)
        {
            if (typeFixup(base_type, pctxt) == -1)
            {
                return -1;
            }
        }

        uses = (Xsd10ItemList) type.attr_uses;
        base_uses = (Xsd10ItemList) base_type.attr_uses;
        /*
        * Expand attribute group references. And build the 'complete'
        * wildcard, i.e. intersect multiple wildcards.
        * Move attribute prohibitions into a separate list.
        */
        if (uses != null)
        {
            Xsd10Wildcard[] wc = new Xsd10Wildcard[] { type.attribute_wildcard };
            if (wxsIsRestriction(type) != 0)
            {
                /*
                * This one will transfer all attr. prohibitions
                * into pctxt->attr_prohibs.
                */
                int r = expandAttributeGroupRefs(pctxt,
                    type, wc, uses,
                    pctxt.attr_prohibs);
                type.attribute_wildcard = wc[0];
                if (r == -1)
                {
                    perrorInt(pctxt, "xmlSchemaFixupTypeAttributeUses",
                        "failed to expand attributes");
                    return -1;
                }
                if (pctxt.attr_prohibs.nb_items != 0)
                {
                    prohibs = pctxt.attr_prohibs;
                }
            }
            else
            {
                int r = expandAttributeGroupRefs(pctxt,
                    type, wc, uses,
                    null);
                type.attribute_wildcard = wc[0];
                if (r == -1)
                {
                    perrorInt(pctxt, "xmlSchemaFixupTypeAttributeUses",
                        "failed to expand attributes");
                    return -1;
                }
            }
        }
        /*
        * Inherit the attribute uses of the base type.
        */
        if (base_uses != null)
        {
            int i, j;
            Xsd10AttributeUseProhib pro;

            if (wxsIsRestriction(type) != 0)
            {
                int uses_count;
                Xsd10AttributeUse tmp;

                if (uses != null)
                {
                    uses_count = uses.nb_items;
                }
                else
                {
                    uses_count = 0;
                }

                /* Restriction. */
                inherit:
                for (i = 0; i < base_uses.nb_items; i++)
                {
                    use = (Xsd10AttributeUse) base_uses.items[i];
                    if (prohibs != null)
                    {
                        /*
                        * Filter out prohibited uses.
                        */
                        for (j = 0; j < prohibs.nb_items; j++)
                        {
                            pro = (Xsd10AttributeUseProhib) prohibs.items[j];
                            if (Objects.equals(wxsAttruseDeclName(use), pro.name) &&
                                Objects.equals(wxsAttruseDeclTns(use),
                                    pro.target_namespace))
                            {
                                continue inherit;
                            }
                        }
                    }
                    if (uses_count != 0)
                    {
                        /*
                        * Filter out existing uses.
                        */
                        for (j = 0; j < uses_count; j++)
                        {
                            tmp = (Xsd10AttributeUse) uses.items[j];
                            if (Objects.equals(wxsAttruseDeclName(use),
                                    wxsAttruseDeclName(tmp)) &&
                                Objects.equals(wxsAttruseDeclTns(use),
                                    wxsAttruseDeclTns(tmp)))
                            {
                                continue inherit;
                            }
                        }
                    }
                    if (uses == null)
                    {
                        type.attr_uses = itemListCreate();
                        if (type.attr_uses == null)
                        {
                            return -1;
                        }
                        uses = (Xsd10ItemList) type.attr_uses;
                    }
                    itemListAddSize(uses, 2, use);
                }
            }
            else
            {
                /* Extension. */
                for (i = 0; i < base_uses.nb_items; i++)
                {
                    use = (Xsd10AttributeUse) base_uses.items[i];
                    if (uses == null)
                    {
                        type.attr_uses = itemListCreate();
                        if (type.attr_uses == null)
                        {
                            return -1;
                        }
                        uses = (Xsd10ItemList) type.attr_uses;
                    }
                    itemListAddSize(uses, base_uses.nb_items, use);
                }
            }
        }
        /*
        * Shrink attr. uses.
        */
        if (uses != null)
        {
            if (uses.nb_items == 0)
            {
                itemListFree(uses);
                type.attr_uses = null;
            }
        }
        /*
        * Compute the complete wildcard.
        */
        if (wxsIsExtension(type) != 0)
        {
            if (base_type.attribute_wildcard != null)
            {
                /*
                * (3.2.2.1) "If the `base wildcard` is non-`absent`, then
                * the appropriate case among the following:"
                */
                if (type.attribute_wildcard != null)
                {
                    /*
                    * Union the complete wildcard with the base wildcard.
                    */
                    if (unionWildcards(pctxt, type.attribute_wildcard,
                            base_type.attribute_wildcard) == -1)
                    {
                        return -1;
                    }
                }
                else
                {
                    /*
                    * (3.2.2.1.1) "If the `complete wildcard` is `absent`,
                    * then the `base wildcard`."
                    */
                    type.attribute_wildcard = base_type.attribute_wildcard;
                }
            }
        }

        return 0;
    }

    // xml_xsd10_type_final_contains
    /**
     * Port of xmlSchemaTypeFinalContains. Evaluates if a type definition contains the given "final".
     * @param type the type definition
     * @param final_ the final
     * @return 1 if the type does contain the given "final", 0 otherwise.
     */
    static int typeFinalContains(Xsd10Type type, int final_)
    {
        if (type == null)
        {
            return 0;
        }
        if ((type.flags & final_) != 0)
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }

    // xml_xsd10_get_union_simple_type_member_types
    /**
     * Port of xmlSchemaGetUnionSimpleTypeMemberTypes.
     * @param type the Union Simple Type
     * @return a list of member types of type if existing, null otherwise.
     */
    static Xsd10TypeLink getUnionSimpleTypeMemberTypes(Xsd10Type type)
    {
        while ((type != null) && (type.type == XML_XSD10_SCHEMA_TYPE_SIMPLE))
        {
            if (type.member_types != null)
            {
                return type.member_types;
            }
            else
            {
                type = type.base_type;
            }
        }
        return null;
    }

    // xml_xsd10_get_particle_emptiable
    /**
     * Port of xmlSchemaGetParticleEmptiable.
     * @param particle the particle
     * @return 1 if emptiable, 0 otherwise.
     */
    static int getParticleEmptiable(Xsd10Particle particle)
    {
        Xsd10Particle part;
        int emptiable;

        if ((particle.children == null) || (particle.min_occurs == 0))
        {
            return 1;
        }

        part = (Xsd10Particle) ((Xsd10TreeItem) particle.children).children;
        if (part == null)
        {
            return 1;
        }

        while (part != null)
        {
            if ((part.children.type == XML_XSD10_SCHEMA_TYPE_ELEMENT) ||
                (part.children.type == XML_XSD10_SCHEMA_TYPE_ANY))
            {
                emptiable = (part.min_occurs == 0) ? 1 : 0;
            }
            else
            {
                emptiable = getParticleEmptiable(part);
            }
            if (particle.children.type == XML_XSD10_SCHEMA_TYPE_CHOICE)
            {
                if (emptiable != 0)
                {
                    return 1;
                }
            }
            else
            {
                /* <all> and <sequence> */
                if (emptiable == 0)
                {
                    return 0;
                }
            }
            part = (Xsd10Particle) part.next;
        }

        if (particle.children.type == XML_XSD10_SCHEMA_TYPE_CHOICE)
        {
            return 0;
        }
        else
        {
            return 1;
        }
    }

    // xml_xsd10_is_particle_emptiable
    /**
     * Port of xmlSchemaIsParticleEmptiable. Schema Component Constraint: Particle Emptiable.
     * @param particle the particle
     * @return 1 if emptiable, 0 otherwise.
     */
    static int isParticleEmptiable(Xsd10Particle particle)
    {
        /*
        * SPEC (1) "Its {min occurs} is 0."
        */
        if ((particle == null) || (particle.min_occurs == 0) ||
            (particle.children == null))
        {
            return 1;
        }
        /*
        * SPEC (2) "Its {term} is a group and the minimum part of the
        * effective total range of that group, [...] is 0."
        */
        if (wxsIsModelGroup(particle.children) != 0)
        {
            return getParticleEmptiable(particle);
        }
        return 0;
    }

    // xml_xsd10_check_cosst_derived_ok
    /**
     * Port of xmlSchemaCheckCOSSTDerivedOK. Schema Component Constraint: Type Derivation
     * OK (Simple) (cos-st-derived-OK)
     * @param actxt a context
     * @param type the derived simple type definition
     * @param base_type the base type definition
     * @param subset the subset of ('restriction', etc.)
     * @return 0 on success, an positive error code otherwise.
     */
    static int checkCosstDerivedOk(Xsd10AbstractCtxt actxt, Xsd10Type type, Xsd10Type base_type, int subset)
    {
        /*
        * 1 They are the same type definition.
        */
        if (type == base_type)
        {
            return 0;
        }
        /*
        * 2.1 restriction is not in the subset, or in the {final}
        * of its own {base type definition};
        */
        if (wxsIsTypeNotFixed(type) != 0)
        {
            if (typeFixup(type, actxt) == -1)
            {
                return -1;
            }
        }
        if (wxsIsTypeNotFixed(base_type) != 0)
        {
            if (typeFixup(base_type, actxt) == -1)
            {
                return -1;
            }
        }
        if (((subset & XSD10_SUBSET_RESTRICTION) != 0) ||
            (typeFinalContains(type.base_type,
                XML_XSD10_SCHEMAS_TYPE_FINAL_RESTRICTION) != 0))
        {
            return XML_XSD10_SCHEMAP_COS_ST_DERIVED_OK_2_1;
        }
        /* 2.2 */
        if (type.base_type == base_type)
        {
            /*
            * 2.2.1 D's `base type definition` is B.
            */
            return 0;
        }
        /*
        * 2.2.2 D's `base type definition` is not the `ur-type definition`
        * and is validly derived from B given the subset, as defined by this
        * constraint.
        */
        if ((wxsIsAnytype(type.base_type) == 0) &&
            (checkCosstDerivedOk(actxt, type.base_type,
                base_type, subset) == 0))
        {
            return 0;
        }
        /*
        * 2.2.3 D's {variety} is list or union and B is the `simple ur-type
        * definition`.
        */
        if ((wxsIsAnySimpleType(base_type) != 0) &&
            ((wxsIsList(type) != 0) || (wxsIsUnion(type) != 0)))
        {
            return 0;
        }
        /*
        * 2.2.4 B's {variety} is union and D is validly derived from a type
        * definition in B's {member type definitions} given the subset, as
        * defined by this constraint.
        */
        if (wxsIsUnion(base_type) != 0)
        {
            Xsd10TypeLink cur;

            cur = base_type.member_types;
            while (cur != null)
            {
                Xsd10Type ct = asType(cur.type);
                if (wxsIsTypeNotFixed(ct) != 0)
                {
                    if (typeFixup(ct, actxt) == -1)
                    {
                        return -1;
                    }
                }
                if (checkCosstDerivedOk(actxt,
                        type, ct, subset) == 0)
                {
                    /*
                    * It just has to be validly derived from at least one
                    * member-type.
                    */
                    return 0;
                }
                cur = cur.next;
            }
        }
        return XML_XSD10_SCHEMAP_COS_ST_DERIVED_OK_2_2;
    }

    // xml_xsd10_check_type_def_circular_internal
    /**
     * Port of xmlSchemaCheckTypeDefCircularInternal. Checks st-props-correct (2) +
     * ct-props-correct (3).
     * @param pctxt the schema parser context
     * @param ctxt_type the type definition
     * @param ancestor an ancestor of ctxt_type
     * @return XML_SCHEMAP_ST_PROPS_CORRECT_2 if the given type is circular, 0 otherwise.
     */
    static int checkTypeDefCircularInternal(Xsd10ParserCtxt pctxt, Xsd10Type ctxt_type, Xsd10Type ancestor)
    {
        int ret;

        if ((ancestor == null) || (ancestor.type == XML_XSD10_SCHEMA_TYPE_BASIC))
        {
            return 0;
        }

        if (ctxt_type == ancestor)
        {
            pCustomErr(pctxt,
                XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_2,
                ctxt_type, wxsItemNode(ctxt_type),
                "The definition is circular", null);
            return XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_2;
        }
        if ((ancestor.flags & XML_XSD10_SCHEMAS_TYPE_MARKED) != 0)
        {
            /*
            * Avoid infinite recursion on circular types not yet checked.
            */
            return 0;
        }
        ancestor.flags |= XML_XSD10_SCHEMAS_TYPE_MARKED;
        ret = checkTypeDefCircularInternal(pctxt, ctxt_type,
            ancestor.base_type);
        ancestor.flags ^= XML_XSD10_SCHEMAS_TYPE_MARKED;
        return ret;
    }

    // xml_xsd10_check_type_def_circular
    /**
     * Port of xmlSchemaCheckTypeDefCircular. Checks for circular type definitions.
     * @param item the complex/simple type definition
     * @param ctxt the parser context
     */
    static void checkTypeDefCircular(Xsd10Type item, Xsd10ParserCtxt ctxt)
    {
        if ((item == null) ||
            (item.type == XML_XSD10_SCHEMA_TYPE_BASIC) ||
            (item.base_type == null))
        {
            return;
        }
        checkTypeDefCircularInternal(ctxt, item,
            item.base_type);
    }

    // xml_xsd10_check_union_type_def_circular_recur
    /**
     * Port of xmlSchemaCheckUnionTypeDefCircularRecur. Simple Type Definition
     * Representation OK (src-simple-type) 4.
     */
    static int checkUnionTypeDefCircularRecur(Xsd10ParserCtxt pctxt, Xsd10Type ctx_type, Xsd10TypeLink members)
    {
        Xsd10TypeLink member;
        Xsd10Type member_type;

        member = members;
        while (member != null)
        {
            member_type = asType(member.type);
            while ((member_type != null) &&
                   (member_type.type != XML_XSD10_SCHEMA_TYPE_BASIC))
            {
                if (member_type == ctx_type)
                {
                    pCustomErr(pctxt,
                        XML_XSD10_SCHEMAP_SRC_SIMPLE_TYPE_4,
                        ctx_type, null,
                        "The union type definition is circular", null);
                    return XML_XSD10_SCHEMAP_SRC_SIMPLE_TYPE_4;
                }
                if ((wxsIsUnion(member_type) != 0) &&
                    ((member_type.flags & XML_XSD10_SCHEMAS_TYPE_MARKED) == 0))
                {
                    int res;
                    member_type.flags |= XML_XSD10_SCHEMAS_TYPE_MARKED;
                    res = checkUnionTypeDefCircularRecur(pctxt,
                        ctx_type,
                        getUnionSimpleTypeMemberTypes(member_type));
                    member_type.flags ^= XML_XSD10_SCHEMAS_TYPE_MARKED;
                    if (res != 0)
                    {
                        return res;
                    }
                }
                member_type = member_type.base_type;
            }
            member = member.next;
        }
        return 0;
    }

    // xml_xsd10_check_union_type_def_circular
    /** Port of xmlSchemaCheckUnionTypeDefCircular. */
    static int checkUnionTypeDefCircular(Xsd10ParserCtxt pctxt, Xsd10Type type)
    {
        if (wxsIsUnion(type) == 0)
        {
            return 0;
        }
        return checkUnionTypeDefCircularRecur(pctxt, type,
            type.member_types);
    }

    // xml_xsd10_resolve_type_references
    /**
     * Port of xmlSchemaResolveTypeReferences. Resolves type definition references.
     * @param type_def the complex/simple type definition
     * @param ctxt the parser context
     */
    static void resolveTypeReferences(Xsd10Type type_def, Xsd10ParserCtxt ctxt)
    {
        if (type_def == null)
        {
            return;
        }

        /*
        * Resolve the base type.
        */
        if (type_def.base_type == null)
        {
            type_def.base_type = getType(ctxt.schema,
                type_def.base, type_def.base_ns);
            if (type_def.base_type == null)
            {
                pResCompAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_SRC_RESOLVE,
                    type_def, type_def.node,
                    "base", type_def.base, type_def.base_ns,
                    XML_XSD10_SCHEMA_TYPE_SIMPLE, null);
                return;
            }
        }
        if (wxsIsSimple(type_def) != 0)
        {
            if (wxsIsUnion(type_def) != 0)
            {
                /*
                * Resolve the member_types.
                */
                resolveUnionMemberTypes(ctxt, type_def);
                return;
            }
            else if (wxsIsList(type_def) != 0)
            {
                /*
                * Resolve the item_type.
                */
                if ((type_def.subtypes == null) && (type_def.base != null))
                {

                    type_def.subtypes = getType(ctxt.schema,
                        type_def.base, type_def.base_ns);

                    if ((type_def.subtypes == null) ||
                        (wxsIsSimple((Xsd10Type) type_def.subtypes) == 0))
                    {
                        type_def.subtypes = null;
                        pResCompAttrErr(ctxt,
                            XML_XSD10_SCHEMAP_SRC_RESOLVE,
                            type_def, type_def.node,
                            "itemType", type_def.base, type_def.base_ns,
                            XML_XSD10_SCHEMA_TYPE_SIMPLE, null);
                    }
                }
                return;
            }
        }
        /*
        * The ball of letters below means, that if we have a particle
        * which has a QName-helper component as its {term}, we want
        * to resolve it...
        */
        else if ((wxsTypeContenttype(type_def) != null) &&
                 ((wxsTypeContenttype(type_def)).type ==
                     XML_XSD10_SCHEMA_TYPE_PARTICLE) &&
                 (wxsTypeParticleTerm(type_def) != null) &&
                 ((wxsTypeParticleTerm(type_def)).type ==
                     XML_XSD10_SCHEMA_EXTRA_QNAMEREF))
        {
            Xsd10QNameRef ref =
                (Xsd10QNameRef) wxsTypeParticleTerm(type_def);
            Xsd10ModelGroupDef group_def;
            Xsd10BasicItem nc;

            /*
            * URGENT TODO: Test this.
            */
            wxsTypeParticle(type_def).children = null;
            /*
            * Resolve the MG definition reference.
            */
            nc = getNamedComponent(ctxt.schema,
                ref.item_type, ref.name, ref.target_namespace);
            group_def = (nc instanceof Xsd10ModelGroupDef) ? (Xsd10ModelGroupDef) nc : null;
            if (group_def == null)
            {
                pResCompAttrErr(ctxt, XML_XSD10_SCHEMAP_SRC_RESOLVE,
                    null, wxsItemNode(wxsTypeParticle(type_def)),
                    "ref", ref.name, ref.target_namespace, ref.item_type,
                    null);
                /* Remove the particle. */
                type_def.subtypes = null;
            }
            else if (wxsModelgroupdefModel(group_def) == null)
            {
                /* Remove the particle. */
                type_def.subtypes = null;
            }
            else
            {
                /*
                * Assign the MG definition's {model group} to the
                * particle's {term}.
                */
                wxsTypeParticle(type_def).children = wxsModelgroupdefModel(group_def);

                if (wxsModelgroupdefModel(group_def).type == XML_XSD10_SCHEMA_TYPE_ALL)
                {
                    /*
                    * SPEC cos-all-limited (1.2)
                    * "1.2 the {term} property of a particle with
                    * {max occurs}=1 which is part of a pair which constitutes
                    * the {content type} of a complex type definition."
                    */
                    if ((wxsTypeParticle(type_def)).max_occurs != 1)
                    {
                        customErr(ctxt,
                            /* TODO: error code */
                            XML_XSD10_SCHEMAP_COS_ALL_LIMITED,
                            wxsItemNode(wxsTypeParticle(type_def)), null,
                            "The particle's {max occurs} must be 1, since the "
                            + "reference resolves to an 'all' model group",
                            null, null);
                    }
                }
            }
        }
    }

    // xml_xsd10_check_st_props_correct
    /**
     * Port of xmlSchemaCheckSTPropsCorrect. Checks st-props-correct.
     * @param ctxt the schema parser context
     * @param type the simple type definition
     * @return 0 if the properties are correct, if not, a positive error code and -1 on internal errors.
     */
    static int checkStPropsCorrect(Xsd10ParserCtxt ctxt, Xsd10Type type)
    {
        Xsd10Type base_type = type.base_type;
        String[] str = new String[1];

        /* STATE: error funcs converted. */
        /*
        * Schema Component Constraint: Simple Type Definition Properties Correct
        *
        * NOTE: This is somehow redundant, since we actually built a simple type
        * to have all the needed information; this acts as an self test.
        */
        /* Base type: If the datatype has been `derived` by `restriction`
        * then the Simple Type Definition component from which it is `derived`,
        * otherwise the Simple Type Definition for anySimpleType ($4.1.6).
        */
        if (base_type == null)
        {
            /*
            * TODO: Think about: "modulo the impact of Missing
            * Sub-components ($5.3)."
            */
            pCustomErr(ctxt,
                XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_1,
                type, null,
                "No base type existent", null);
            return XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_1;

        }
        if (wxsIsSimple(base_type) == 0)
        {
            pCustomErr(ctxt,
                XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_1,
                type, null,
                "The base type '%s' is not a simple type",
                getComponentQName(str, base_type));
            return XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_1;
        }
        if (((wxsIsList(type) != 0) || (wxsIsUnion(type) != 0)) &&
            (wxsIsRestriction(type) == 0) &&
            ((wxsIsAnySimpleType(base_type) == 0) &&
             (base_type.type != XML_XSD10_SCHEMA_TYPE_SIMPLE)))
        {
            pCustomErr(ctxt,
                XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_1,
                type, null,
                "A type, derived by list or union, must have "
                + "the simple ur-type definition as base type, not '%s'",
                getComponentQName(str, base_type));
            return XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_1;
        }
        /*
        * Variety: One of {atomic, list, union}.
        */
        if ((wxsIsAtomic(type) == 0) && (wxsIsUnion(type) == 0) &&
            (wxsIsList(type) == 0))
        {
            pCustomErr(ctxt,
                XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_1,
                type, null,
                "The variety is absent", null);
            return XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_1;
        }
        /* TODO: Finish this. Hmm, is this finished? */

        /*
        * 3 The {final} of the {base type definition} must not contain restriction.
        */
        if (typeFinalContains(base_type,
                XML_XSD10_SCHEMAS_TYPE_FINAL_RESTRICTION) != 0)
        {
            pCustomErr(ctxt,
                XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_3,
                type, null,
                "The 'final' of its base type '%s' must not contain "
                + "'restriction'",
                getComponentQName(str, base_type));
            return XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_3;
        }

        /*
        * 2 All simple type definitions must be derived ultimately from the `simple
        * ur-type definition` (so circular definitions are disallowed).
        *
        * NOTE: this is done in xmlSchemaCheckTypeDefCircular().
        */
        return 0;
    }

    // xml_xsd10_check_cosst_restricts
    /**
     * Port of xmlSchemaCheckCOSSTRestricts. Schema Component Constraint: Derivation Valid
     * (Restriction, Simple) (cos-st-restricts)
     * @param pctxt the schema parser context
     * @param type the simple type definition
     * @return -1 on internal errors, 0 if the type is validly derived, a positive error code otherwise.
     */
    static int checkCosstRestricts(Xsd10ParserCtxt pctxt, Xsd10Type type)
    {
        String[] str = new String[1];

        if (type.type != XML_XSD10_SCHEMA_TYPE_SIMPLE)
        {
            perrorInt(pctxt, "xmlSchemaCheckCOSSTRestricts",
                "given type is not a user-derived simpleType");
            return -1;
        }

        if (wxsIsAtomic(type) != 0)
        {
            Xsd10Type primitive;
            /*
            * 1.1 The {base type definition} must be an atomic simple
            * type definition or a built-in primitive datatype.
            */
            if (wxsIsAtomic(type.base_type) == 0)
            {
                pCustomErr(pctxt,
                    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_1,
                    type, null,
                    "The base type '%s' is not an atomic simple type",
                    getComponentQName(str, type.base_type));
                return XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_1;
            }
            /* 1.2 The {final} of the {base type definition} must not contain
            * restriction.
            */
            /* OPTIMIZE TODO : This is already done in xmlSchemaCheckStPropsCorrect */
            if (typeFinalContains(type.base_type,
                    XML_XSD10_SCHEMAS_TYPE_FINAL_RESTRICTION) != 0)
            {
                pCustomErr(pctxt,
                    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_2,
                    type, null,
                    "The final of its base type '%s' must not contain 'restriction'",
                    getComponentQName(str, type.base_type));
                return XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_2;
            }

            /*
            * 1.3.1 DF must be an allowed constraining facet for the {primitive
            * type definition}, as specified in the appropriate subsection of 3.2
            * Primitive datatypes.
            */
            if (type.facets != null)
            {
                Xsd10Facet facet;
                int ok = 1;

                primitive = getPrimitiveType(type);
                if (primitive == null)
                {
                    perrorInt(pctxt, "xmlSchemaCheckCOSSTRestricts",
                        "failed to get primitive type");
                    return -1;
                }
                facet = type.facets;
                do
                {
                    if (isBuiltInTypeFacet(primitive, facet.type) == 0)
                    {
                        ok = 0;
                        pIllegalFacetAtomicErr(pctxt,
                            XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_3_1,
                            type, primitive, facet);
                    }
                    facet = facet.next;
                } while (facet != null);
                if (ok == 0)
                {
                    return XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_3_1;
                }
            }
            /*
            * NOTE (1.3.2) Facet derivation constraints are currently handled in
            * xmlSchemaDeriveAndValidateFacets()
            */
        }
        else if (wxsIsList(type) != 0)
        {
            Xsd10Type item_type = null;

            item_type = asType(type.subtypes);
            if ((item_type == null) || (wxsIsSimple(item_type) == 0))
            {
                perrorInt(pctxt, "xmlSchemaCheckCOSSTRestricts",
                    "failed to evaluate the item type");
                return -1;
            }
            if (wxsIsTypeNotFixed(item_type) != 0)
            {
                typeFixup(item_type, pctxt);
            }
            /*
            * 2.1 The {item type definition} must have a {variety} of atomic or
            * union (in which case all the {member type definitions}
            * must be atomic).
            */
            if ((wxsIsAtomic(item_type) == 0) &&
                (wxsIsUnion(item_type) == 0))
            {
                pCustomErr(pctxt,
                    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_1,
                    type, null,
                    "The item type '%s' does not have a variety of atomic or union",
                    getComponentQName(str, item_type));
                return XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_1;
            }
            else if (wxsIsUnion(item_type) != 0)
            {
                Xsd10TypeLink member;

                member = item_type.member_types;
                while (member != null)
                {
                    if (wxsIsAtomic(asType(member.type)) == 0)
                    {
                        pCustomErr(pctxt,
                            XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_1,
                            type, null,
                            "The item type is a union type, but the "
                            + "member type '%s' of this item type is not atomic",
                            getComponentQName(str, member.type));
                        return XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_1;
                    }
                    member = member.next;
                }
            }

            if (wxsIsAnySimpleType(type.base_type) != 0)
            {
                Xsd10Facet facet;
                /*
                * This is the case if we have: <simpleType><list ..
                */
                /*
                * 2.3.1
                * 2.3.1.1 The {final} of the {item type definition} must not
                * contain list.
                */
                if (typeFinalContains(item_type,
                        XML_XSD10_SCHEMAS_TYPE_FINAL_LIST) != 0)
                {
                    pCustomErr(pctxt,
                        XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_1_1,
                        type, null,
                        "The final of its item type '%s' must not contain 'list'",
                        getComponentQName(str, item_type));
                    return XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_1_1;
                }
                /*
                * 2.3.1.2 The {facets} must only contain the whiteSpace
                * facet component.
                */
                if (type.facets != null)
                {
                    facet = type.facets;
                    do
                    {
                        if (facet.type != XML_XSD10_SCHEMA_FACET_WHITESPACE)
                        {
                            pIllegalFacetListUnionErr(pctxt,
                                XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_1_2,
                                type, facet);
                            return XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_1_2;
                        }
                        facet = facet.next;
                    } while (facet != null);
                }
            }
            else
            {
                /*
                * This is the case if we have: <simpleType><restriction ...
                * I.e. the variety of "list" is inherited.
                */
                /*
                * 2.3.2
                * 2.3.2.1 The {base type definition} must have a {variety} of list.
                */
                if (wxsIsList(type.base_type) == 0)
                {
                    pCustomErr(pctxt,
                        XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_1,
                        type, null,
                        "The base type '%s' must be a list type",
                        getComponentQName(str, type.base_type));
                    return XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_1;
                }
                /*
                * 2.3.2.2 The {final} of the {base type definition} must not
                * contain restriction.
                */
                if (typeFinalContains(type.base_type,
                        XML_XSD10_SCHEMAS_TYPE_FINAL_RESTRICTION) != 0)
                {
                    pCustomErr(pctxt,
                        XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_2,
                        type, null,
                        "The 'final' of the base type '%s' must not contain 'restriction'",
                        getComponentQName(str, type.base_type));
                    return XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_2;
                }
                /*
                * 2.3.2.3 The {item type definition} must be validly derived
                * from the {base type definition}'s {item type definition} given
                * the empty set, as defined in Type Derivation OK (Simple) ($3.14.6).
                */
                {
                    Xsd10Type base_item_type;

                    base_item_type = asType(type.base_type.subtypes);
                    if ((base_item_type == null) || (wxsIsSimple(base_item_type) == 0))
                    {
                        perrorInt(pctxt, "xmlSchemaCheckCOSSTRestricts",
                            "failed to eval the item type of a base type");
                        return -1;
                    }
                    if ((item_type != base_item_type) &&
                        (checkCosstDerivedOk(pctxt, item_type,
                            base_item_type, 0) != 0))
                    {
                        String[] str_bit = new String[1], str_bt = new String[1];
                        pCustomErrExt(pctxt,
                            XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_3,
                            type, null,
                            "The item type '%s' is not validly derived from "
                            + "the item type '%s' of the base type '%s'",
                            getComponentQName(str, item_type),
                            getComponentQName(str_bit, base_item_type),
                            getComponentQName(str_bt, type.base_type));

                        return XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_3;
                    }
                }

                if (type.facets != null)
                {
                    Xsd10Facet facet;
                    int ok = 1;
                    /*
                    * 2.3.2.4 Only length, minLength, maxLength, whiteSpace, pattern
                    * and enumeration facet components are allowed among the {facets}.
                    */
                    facet = type.facets;
                    do
                    {
                        switch (facet.type)
                        {
                        case XML_XSD10_SCHEMA_FACET_LENGTH:
                        case XML_XSD10_SCHEMA_FACET_MINLENGTH:
                        case XML_XSD10_SCHEMA_FACET_MAXLENGTH:
                        case XML_XSD10_SCHEMA_FACET_WHITESPACE:
                            /*
                            * TODO: 2.5.1.2 List datatypes
                            * The value of `whiteSpace` is fixed to the value collapse.
                            */
                        case XML_XSD10_SCHEMA_FACET_PATTERN:
                        case XML_XSD10_SCHEMA_FACET_ENUMERATION:
                            break;
                        default:
                        {
                            pIllegalFacetListUnionErr(pctxt,
                                XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_4,
                                type, facet);
                            /*
                            * We could return, but it's nicer to report all
                            * invalid facets.
                            */
                            ok = 0;
                        }
                        }
                        facet = facet.next;
                    } while (facet != null);
                    if (ok == 0)
                    {
                        return XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_4;
                    }
                    /*
                    * NOTE (2.3.2.5) This is currently done in
                    * xmlSchemaDeriveAndValidateFacets()
                    */
                }
            }
        }
        else if (wxsIsUnion(type) != 0)
        {
            /*
            * 3.1 The {member type definitions} must all have {variety} of
            * atomic or list.
            */
            Xsd10TypeLink member;

            member = type.member_types;
            while (member != null)
            {
                Xsd10Type mt = asType(member.type);
                if (wxsIsTypeNotFixed(mt) != 0)
                {
                    typeFixup(mt, pctxt);
                }

                if ((wxsIsAtomic(mt) == 0) &&
                    (wxsIsList(mt) == 0))
                {
                    pCustomErr(pctxt,
                        XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_1,
                        type, null,
                        "The member type '%s' is neither an atomic, nor a list type",
                        getComponentQName(str, mt));
                    return XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_1;
                }
                member = member.next;
            }
            /*
            * 3.3.1 If the {base type definition} is the `simple ur-type
            * definition`
            */
            if (type.base_type.built_in_type == XML_XSD10_SCHEMAS_ANYSIMPLETYPE)
            {
                /*
                * 3.3.1.1 All of the {member type definitions} must have a
                * {final} which does not contain union.
                */
                member = type.member_types;
                while (member != null)
                {
                    if (typeFinalContains(asType(member.type),
                            XML_XSD10_SCHEMAS_TYPE_FINAL_UNION) != 0)
                    {
                        pCustomErr(pctxt,
                            XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_1,
                            type, null,
                            "The 'final' of member type '%s' contains 'union'",
                            getComponentQName(str, member.type));
                        return XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_1;
                    }
                    member = member.next;
                }
                /*
                * 3.3.1.2 The {facets} must be empty.
                */
                if (type.facet_set != null)
                {
                    pCustomErr(pctxt,
                        XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_1_2,
                        type, null,
                        "No facets allowed", null);
                    return XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_1_2;
                }
            }
            else
            {
                /*
                * 3.3.2.1 The {base type definition} must have a {variety} of union.
                * I.e. the variety of "list" is inherited.
                */
                if (wxsIsUnion(type.base_type) == 0)
                {
                    pCustomErr(pctxt,
                        XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_1,
                        type, null,
                        "The base type '%s' is not a union type",
                        getComponentQName(str, type.base_type));
                    return XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_1;
                }
                /*
                * 3.3.2.2 The {final} of the {base type definition} must not contain restriction.
                */
                if (typeFinalContains(type.base_type,
                        XML_XSD10_SCHEMAS_TYPE_FINAL_RESTRICTION) != 0)
                {
                    pCustomErr(pctxt,
                        XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_2,
                        type, null,
                        "The 'final' of its base type '%s' must not contain 'restriction'",
                        getComponentQName(str, type.base_type));
                    return XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_2;
                }
                /*
                * 3.3.2.3 The {member type definitions}, in order, must be validly
                * derived from the corresponding type definitions in the {base
                * type definition}'s {member type definitions} given the empty set,
                * as defined in Type Derivation OK (Simple) ($3.14.6).
                */
                {
                    Xsd10TypeLink base_member;

                    if (type.member_types != null)
                    {
                        member = type.member_types;
                        base_member = getUnionSimpleTypeMemberTypes(type.base_type);
                        if ((member == null) && (base_member != null))
                        {
                            perrorInt(pctxt, "xmlSchemaCheckCOSSTRestricts",
                                "different number of member types in base");
                        }
                        while (member != null)
                        {
                            if (base_member == null)
                            {
                                perrorInt(pctxt, "xmlSchemaCheckCOSSTRestricts",
                                    "different number of member types in base");
                            }
                            else if ((member.type != base_member.type) &&
                                     (checkCosstDerivedOk(pctxt,
                                         asType(member.type), asType(base_member.type), 0) != 0))
                            {
                                String[] str_bmt = new String[1], str_bt = new String[1];

                                pCustomErrExt(pctxt,
                                    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_3,
                                    type, null,
                                    "The member type %s is not validly "
                                    + "derived from its corresponding member "
                                    + "type %s of the base type %s",
                                    getComponentQName(str, member.type),
                                    getComponentQName(str_bmt, base_member.type),
                                    getComponentQName(str_bt, type.base_type));
                                return XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_3;
                            }
                            member = member.next;
                            if (base_member != null)
                            {
                                base_member = base_member.next;
                            }
                        }
                    }
                }
                /*
                * 3.3.2.4 Only pattern and enumeration facet components are
                * allowed among the {facets}.
                */
                if (type.facets != null)
                {
                    Xsd10Facet facet;
                    int ok = 1;

                    facet = type.facets;
                    do
                    {
                        if ((facet.type != XML_XSD10_SCHEMA_FACET_PATTERN) &&
                            (facet.type != XML_XSD10_SCHEMA_FACET_ENUMERATION))
                        {
                            pIllegalFacetListUnionErr(pctxt,
                                XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_4,
                                type, facet);
                            ok = 0;
                        }
                        facet = facet.next;
                    } while (facet != null);
                    if (ok == 0)
                    {
                        return XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_4;
                    }

                }
                /*
                * NOTE (3.3.2.5) This is currently done in
                * xmlSchemaDeriveAndValidateFacets()
                */
            }
        }

        return 0;
    }

    // xml_xsd10_create_v_ctxt_on_p_ctxt
    /** Port of xmlSchemaCreateVCtxtOnPCtxt. */
    static int createVCtxtOnPCtxt(Xsd10ParserCtxt ctxt)
    {
        if (ctxt.vctxt == null)
        {
            ctxt.vctxt = newValidCtxt(null);
            if (ctxt.vctxt == null)
            {
                pErr(ctxt, null,
                    XML_XSD10_SCHEMAP_INTERNAL,
                    "Internal error: xmlSchemaCreateVCtxtOnPCtxt, "
                    + "failed to create a temp. validation context.\n",
                    null, null);
                return -1;
            }
            /* TODO: Pass user data. */
            setValidErrors(ctxt.vctxt,
                ctxt.error, ctxt.warning, ctxt.err_ctxt);
            setValidStructuredErrors(ctxt.vctxt,
                ctxt.serror, ctxt.err_ctxt);
        }
        return 0;
    }

    // xml_xsd10_parse_check_cos_valid_default
    /**
     * Port of xmlSchemaParseCheckCOSValidDefault. Schema Component Constraint: Element
     * Default Valid (Immediate) (cos-valid-default)
     * @param pctxt the schema parser context
     * @param node an optional node (the holder of the value)
     * @param type the simple type definition
     * @param value the default value
     * @param val resulting value
     * @return 0 if the constraints are satisfied, if not, a positive error code and -1 on internal errors.
     */
    static int parseCheckCosValidDefault(Xsd10ParserCtxt pctxt, XmlNode node, Xsd10Type type, String value, Xsd10Val[] val)
    {
        int ret = 0;

        /*
        * cos-valid-default:
        * Schema Component Constraint: Element Default Valid (Immediate)
        * For a string to be a valid default with respect to a type
        * definition the appropriate case among the following must be true:
        */
        if (wxsIsComplex(type) != 0)
        {
            /*
            * Complex type.
            *
            * SPEC (2.1) "its {content type} must be a simple type definition
            * or mixed."
            * SPEC (2.2.2) "If the {content type} is mixed, then the {content
            * type}'s particle must be `emptiable` as defined by
            * Particle Emptiable ($3.9.6)."
            */
            if ((wxsHasSimpleContent(type) == 0) &&
                ((wxsHasMixedContent(type) == 0) || (isParticleEmptiable(asParticle(type.subtypes)) == 0)))
            {
                /* NOTE that this covers (2.2.2) as well. */
                pCustomErr(pctxt,
                    XML_XSD10_SCHEMAP_COS_VALID_DEFAULT_2_1,
                    type, type.node,
                    "For a string to be a valid default, the type definition "
                    + "must be a simple type or a complex type with mixed content "
                    + "and a particle emptiable", null);
                return XML_XSD10_SCHEMAP_COS_VALID_DEFAULT_2_1;
            }
        }
        /*
        * 1 If the type definition is a simple type definition, then the string
        * must be `valid` with respect to that definition as defined by String
        * Valid ($3.14.4).
        *
        * AND
        *
        * 2.2.1 If the {content type} is a simple type definition, then the
        * string must be `valid` with respect to that simple type definition
        * as defined by String Valid ($3.14.4).
        */
        if (wxsIsSimple(type) != 0)
        {
            ret = vCheckCvcSimpleType(pctxt, node,
                type, value, val, 1, 1, 0);
        }
        else if (wxsHasSimpleContent(type) != 0)
        {
            ret = vCheckCvcSimpleType(pctxt, node,
                type.content_type_def, value, val, 1, 1, 0);
        }
        else
        {
            return ret;
        }

        if (ret < 0)
        {
            perrorInt(pctxt, "xmlSchemaParseCheckCOSValidDefault",
                "calling xmlSchemaVCheckCVCSimpleType()");
        }

        return ret;
    }

    /** Cast helper for {content type} particles. */
    private static Xsd10Particle asParticle(Object o)
    {
        return (o instanceof Xsd10Particle) ? (Xsd10Particle) o : null;
    }

    // xml_xsd10_check_ct_props_correct
    /**
     * Port of xmlSchemaCheckCTPropsCorrect. Schema Component Constraint: Complex Type
     * Definition Properties Correct (ct-props-correct)
     * @param pctxt the schema parser context
     * @param type the complex type definition
     * @return 0 if the constraints are satisfied, a positive error code if not and -1 if an internal error occurred.
     */
    static int checkCtPropsCorrect(Xsd10ParserCtxt pctxt, Xsd10Type type)
    {
        /*
        * SPEC (1) "The values of the properties of a complex type definition must
        * be as described in the property tableau in The Complex Type Definition
        * Schema Component ($3.4.1), modulo the impact of Missing
        * Sub-components ($5.3)."
        */
        if ((type.base_type != null) &&
            (wxsIsSimple(type.base_type) != 0) &&
            (wxsIsExtension(type) == 0))
        {
            /*
            * SPEC (2) "If the {base type definition} is a simple type definition,
            * the {derivation method} must be extension."
            */
            customErr(pctxt,
                XML_XSD10_SCHEMAP_SRC_CT_1,
                null, type,
                "If the base type is a simple type, the derivation method must be "
                + "'extension'", null, null);
            return XML_XSD10_SCHEMAP_SRC_CT_1;
        }
        /*
        * SPEC (3) "Circular definitions are disallowed, except for the `ur-type
        * definition`."
        *
        * NOTE (3) is done in xmlSchemaCheckTypeDefCircular().
        */
        /*
        * NOTE that (4) and (5) need the following:
        *   - attribute uses need to be already inherited (apply attr. prohibitions)
        *   - attribute group references need to be expanded already
        *   - simple types need to be typefixed already
        */
        if ((type.attr_uses != null) &&
            (((Xsd10ItemList) type.attr_uses).nb_items > 1))
        {
            Xsd10ItemList uses = (Xsd10ItemList) type.attr_uses;
            Xsd10AttributeUse use, tmp;
            int i, j, has_id = 0;

            next_use:
            for (i = uses.nb_items - 1; i >= 0; i--)
            {
                use = (Xsd10AttributeUse) uses.items[i];

                /*
                * SPEC ct-props-correct
                * (4) "Two distinct attribute declarations in the
                * {attribute uses} must not have identical {name}s and
                * {target namespace}s."
                */
                if (i > 0)
                {
                    for (j = i - 1; j >= 0; j--)
                    {
                        tmp = (Xsd10AttributeUse) uses.items[j];
                        if (Objects.equals(wxsAttruseDeclName(use),
                                wxsAttruseDeclName(tmp)) &&
                            Objects.equals(wxsAttruseDeclTns(use),
                                wxsAttruseDeclTns(tmp)))
                        {
                            String[] str = new String[1];

                            customErr(pctxt,
                                XML_XSD10_SCHEMAP_AG_PROPS_CORRECT,
                                null, type,
                                "Duplicate %s",
                                getComponentDesignation(str, use),
                                null);
                            /*
                            * Remove the duplicate.
                            */
                            if (itemListRemove(uses, i) == -1)
                            {
                                return -1;
                            }
                            continue next_use;
                        }
                    }
                }
                /*
                * SPEC ct-props-correct
                * (5) "Two distinct attribute declarations in the
                * {attribute uses} must not have {type definition}s which
                * are or are derived from ID."
                */
                if (wxsAttruseTypedef(use) != null)
                {
                    if (isDerivedFromBuiltInType(
                            wxsAttruseTypedef(use), XML_XSD10_SCHEMAS_ID) != 0)
                    {
                        if (has_id != 0)
                        {
                            String[] str = new String[1];

                            customErr(pctxt,
                                XML_XSD10_SCHEMAP_AG_PROPS_CORRECT,
                                null, type,
                                "There must not exist more than one attribute "
                                + "declaration of type 'xs:ID' "
                                + "(or derived from 'xs:ID'). The %s violates this "
                                + "constraint",
                                getComponentDesignation(str, use),
                                null);
                            if (itemListRemove(uses, i) == -1)
                            {
                                return -1;
                            }
                        }

                        has_id = 1;
                    }
                }
            }
        }
        return 0;
    }

    // xml_xsd10_are_equal_types
    /** Port of xmlSchemaAreEqualTypes. */
    static int areEqualTypes(Xsd10Type type_a, Xsd10Type type_b)
    {
        /*
        * TODO: This should implement component-identity
        * in the future.
        */
        if ((type_a == null) || (type_b == null))
        {
            return 0;
        }
        return (type_a == type_b) ? 1 : 0;
    }

    // xml_xsd10_check_cosct_derived_ok
    /**
     * Port of xmlSchemaCheckCOSCTDerivedOK. Schema Component Constraint: Type Derivation
     * OK (Complex) (cos-ct-derived-ok)
     * @param actxt the schema parser context
     * @param type the to-be derived complex type definition
     * @param base_type the base complex type definition
     * @param set the given set
     * @return 0 if the constraints are satisfied, or 1 if not.
     */
    static int checkCosctDerivedOk(Xsd10AbstractCtxt actxt, Xsd10Type type, Xsd10Type base_type, int set)
    {
        int equal = areEqualTypes(type, base_type);
        /* TODO: Error codes. */
        if (equal == 0)
        {
            /*
            * SPEC (1) "If B and D are not the same type definition, then the
            * {derivation method} of D must not be in the subset."
            */
            if ((((set & XSD10_SUBSET_EXTENSION) != 0) && (wxsIsExtension(type) != 0)) ||
                (((set & XSD10_SUBSET_RESTRICTION) != 0) && (wxsIsRestriction(type) != 0)))
            {
                return 1;
            }
        }
        else
        {
            /*
            * SPEC (2.1) "B and D must be the same type definition."
            */
            return 0;
        }
        /*
        * SPEC (2.2) "B must be D's {base type definition}."
        */
        if (type.base_type == base_type)
        {
            return 0;
        }
        /*
        * SPEC (2.3.1) "D's {base type definition} must not be the `ur-type
        * definition`."
        */
        if (wxsIsAnytype(type.base_type) != 0)
        {
            return 1;
        }

        if (wxsIsComplex(type.base_type) != 0)
        {
            /*
            * SPEC (2.3.2.1) "If D's {base type definition} is complex, then it
            * must be validly derived from B given the subset as defined by this
            * constraint."
            */
            return checkCosctDerivedOk(actxt, type.base_type,
                base_type, set);
        }
        else
        {
            /*
            * SPEC (2.3.2.2) "If D's {base type definition} is simple, then it
            * must be validly derived from B given the subset as defined in Type
            * Derivation OK (Simple) ($3.14.6).
            */
            return checkCosstDerivedOk(actxt, type.base_type,
                base_type, set);
        }
    }

    // xml_xsd10_check_cos_derived_ok
    /**
     * Port of xmlSchemaCheckCOSDerivedOK. Calls Type Derivation OK (Simple) AND Type
     * Derivation OK (Complex).
     * @param actxt the schema parser context
     * @param type the derived simple type definition
     * @param base_type the base type definition
     * @param set the given set
     * @return 0 on success, an positive error code otherwise.
     */
    static int checkCosDerivedOk(Xsd10AbstractCtxt actxt, Xsd10Type type, Xsd10Type base_type, int set)
    {
        if (wxsIsSimple(type) != 0)
        {
            return checkCosstDerivedOk(actxt, type, base_type, set);
        }
        else
        {
            return checkCosctDerivedOk(actxt, type, base_type, set);
        }
    }

    // xml_xsd10_check_cosct_extends
    /**
     * Port of xmlSchemaCheckCOSCTExtends. Schema Component Constraint: Derivation Valid
     * (Extension) (cos-ct-extends)
     * @param ctxt the schema parser context
     * @param type the complex type definition
     * @return 0 if the constraints are satisfied, a positive error code if not and -1 if an internal error occurred.
     */
    static int checkCosctExtends(Xsd10ParserCtxt ctxt, Xsd10Type type)
    {
        Xsd10Type base = type.base_type;
        /*
        * SPEC (1) "If the {base type definition} is a complex type definition,
        * then all of the following must be true:"
        */
        if (wxsIsComplex(base) != 0)
        {
            /*
            * SPEC (1.1) "The {final} of the {base type definition} must not
            * contain extension."
            */
            if ((base.flags & XML_XSD10_SCHEMAS_TYPE_FINAL_EXTENSION) != 0)
            {
                pCustomErr(ctxt,
                    XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_1,
                    type, null,
                    "The 'final' of the base type definition "
                    + "contains 'extension'", null);
                return XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_1;
            }

            /*
            * ATTENTION: The constrains (1.2) and (1.3) are not applied,
            * since they are automatically satisfied through the
            * inheriting mechanism.
            */
            /*
            * SPEC (1.4) "One of the following must be true:"
            */
            if ((type.content_type_def != null) &&
                (type.content_type_def == base.content_type_def))
            {
                /*
                * SPEC (1.4.1) PASS
                */
            }
            else if ((type.content_type == XML_XSD10_SCHEMA_CONTENT_EMPTY) &&
                     (base.content_type == XML_XSD10_SCHEMA_CONTENT_EMPTY))
            {
                /*
                * SPEC (1.4.2) PASS
                */
            }
            else
            {
                /*
                * SPEC (1.4.3) "All of the following must be true:"
                */
                if (type.subtypes == null)
                {
                    /*
                    * SPEC 1.4.3.1 The {content type} of the complex type
                    * definition itself must specify a particle.
                    */
                    pCustomErr(ctxt,
                        XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_1,
                        type, null,
                        "The content type must specify a particle", null);
                    return XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_1;
                }
                /*
                * SPEC (1.4.3.2) "One of the following must be true:"
                */
                if (base.content_type == XML_XSD10_SCHEMA_CONTENT_EMPTY)
                {
                    /*
                    * SPEC (1.4.3.2.1) PASS
                    */
                }
                else
                {
                    /*
                    * SPEC (1.4.3.2.2) "All of the following must be true:"
                    */
                    if ((type.content_type != base.content_type) ||
                        ((type.content_type != XML_XSD10_SCHEMA_CONTENT_MIXED) &&
                         (type.content_type != XML_XSD10_SCHEMA_CONTENT_ELEMENTS)))
                    {
                        /*
                        * SPEC (1.4.3.2.2.1) "Both {content type}s must be mixed
                        * or both must be element-only."
                        */
                        pCustomErr(ctxt,
                            XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_1,
                            type, null,
                            "The content type of both, the type and its base "
                            + "type, must either 'mixed' or 'element-only'", null);
                        return XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_1;
                    }
                    /*
                    * URGENT TODO SPEC (1.4.3.2.2.2) not checked.
                    */
                }
                /*
                * URGENT TODO (1.5)
                */
            }
        }
        else
        {
            /*
            * SPEC (2) "If the {base type definition} is a simple type definition,
            * then all of the following must be true:"
            */
            if (type.content_type_def != base)
            {
                /*
                * SPEC (2.1) "The {content type} must be the same simple type
                * definition."
                */
                pCustomErr(ctxt,
                    XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_1,
                    type, null,
                    "The content type must be the simple base type", null);
                return XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_1;
            }
            if ((base.flags & XML_XSD10_SCHEMAS_TYPE_FINAL_EXTENSION) != 0)
            {
                /*
                * SPEC (2.2) "The {final} of the {base type definition} must not
                * contain extension"
                * NOTE that this is the same as (1.1).
                */
                pCustomErr(ctxt,
                    XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_1,
                    type, null,
                    "The 'final' of the base type definition "
                    + "contains 'extension'", null);
                return XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_1;
            }
        }
        return 0;
    }

    // xml_xsd10_check_derivation_ok_restriction
    /**
     * Port of xmlSchemaCheckDerivationOKRestriction. Schema Component Constraint:
     * Derivation Valid (Restriction, Complex) (derivation-ok-restriction)
     * @param ctxt the schema parser context
     * @param type the complex type definition
     * @return 0 if the constraints are satisfied, a positive error code if not and -1 if an internal error occurred.
     */
    static int checkDerivationOkRestriction(Xsd10ParserCtxt ctxt, Xsd10Type type)
    {
        Xsd10Type base;

        base = type.base_type;
        if (wxsIsComplex(base) == 0)
        {
            customErr(ctxt,
                XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_1,
                type.node, type,
                "The base type must be a complex type", null, null);
            return ctxt.err;
        }
        if ((base.flags & XML_XSD10_SCHEMAS_TYPE_FINAL_RESTRICTION) != 0)
        {
            /*
            * SPEC (1) "The {base type definition} must be a complex type
            * definition whose {final} does not contain restriction."
            */
            customErr(ctxt,
                XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_1,
                type.node, type,
                "The 'final' of the base type definition "
                + "contains 'restriction'", null, null);
            return ctxt.err;
        }
        /*
        * SPEC (2), (3) and (4)
        * Those are handled in a separate function, since the
        * same constraints are needed for redefinition of
        * attribute groups as well.
        */
        if (checkDerivationOkRestriction2to4(ctxt,
                XML_XSD10_SCHEMA_ACTION_DERIVE,
                type, base,
                (Xsd10ItemList) type.attr_uses, (Xsd10ItemList) base.attr_uses,
                type.attribute_wildcard,
                base.attribute_wildcard) == -1)
        {
            return -1;
        }
        /*
        * SPEC (5) "One of the following must be true:"
        */
        if (base.built_in_type == XML_XSD10_SCHEMAS_ANYTYPE)
        {
            /*
            * SPEC (5.1) PASS
            */
        }
        else if ((type.content_type == XML_XSD10_SCHEMA_CONTENT_SIMPLE) ||
                 (type.content_type == XML_XSD10_SCHEMA_CONTENT_BASIC))
        {
            /*
            * SPEC (5.2.1) "The {content type} of the complex type definition
            * must be a simple type definition"
            *
            * SPEC (5.2.2) "One of the following must be true:"
            */
            if ((base.content_type == XML_XSD10_SCHEMA_CONTENT_SIMPLE) ||
                (base.content_type == XML_XSD10_SCHEMA_CONTENT_BASIC))
            {
                int err;
                /*
                * SPEC (5.2.2.1)
                */
                err = checkCosstDerivedOk(ctxt,
                    type.content_type_def, base.content_type_def, 0);
                if (err != 0)
                {
                    String[] str_a = new String[1], str_b = new String[1];

                    if (err == -1)
                    {
                        return -1;
                    }
                    customErr(ctxt,
                        XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_1,
                        null, type,
                        "The {content type} %s is not validly derived from the "
                        + "base type's {content type} %s",
                        getComponentDesignation(str_a,
                            type.content_type_def),
                        getComponentDesignation(str_b,
                            base.content_type_def));
                    return ctxt.err;
                }
            }
            else if ((base.content_type == XML_XSD10_SCHEMA_CONTENT_MIXED) &&
                     (isParticleEmptiable(
                         asParticle(base.subtypes)) != 0))
            {
                /*
                * SPEC (5.2.2.2) PASS
                */
            }
            else
            {
                pCustomErr(ctxt,
                    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_1,
                    type, null,
                    "The content type of the base type must be either "
                    + "a simple type or 'mixed' and an emptiable particle", null);
                return ctxt.err;
            }
        }
        else if (type.content_type == XML_XSD10_SCHEMA_CONTENT_EMPTY)
        {
            /*
            * SPEC (5.3.1) "The {content type} of the complex type itself must
            * be empty"
            */
            if (base.content_type == XML_XSD10_SCHEMA_CONTENT_EMPTY)
            {
                /*
                * SPEC (5.3.2.1) PASS
                */
            }
            else if (((base.content_type == XML_XSD10_SCHEMA_CONTENT_ELEMENTS) ||
                      (base.content_type == XML_XSD10_SCHEMA_CONTENT_MIXED)) &&
                     (isParticleEmptiable(
                         asParticle(base.subtypes)) != 0))
            {
                /*
                * SPEC (5.3.2.2) PASS
                */
            }
            else
            {
                pCustomErr(ctxt,
                    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_1,
                    type, null,
                    "The content type of the base type must be either "
                    + "empty or 'mixed' (or 'elements-only') and an emptiable "
                    + "particle", null);
                return ctxt.err;
            }
        }
        else if ((type.content_type == XML_XSD10_SCHEMA_CONTENT_ELEMENTS) ||
                 (wxsHasMixedContent(type) != 0))
        {
            /*
            * SPEC (5.4.1.1) "The {content type} of the complex type definition
            * itself must be element-only"
            */
            if ((wxsHasMixedContent(type) != 0) && (wxsHasMixedContent(base) == 0))
            {
                /*
                * SPEC (5.4.1.2) "The {content type} of the complex type
                * definition itself and of the {base type definition} must be
                * mixed"
                */
                pCustomErr(ctxt,
                    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_1,
                    type, null,
                    "If the content type is 'mixed', then the content type of the "
                    + "base type must also be 'mixed'", null);
                return ctxt.err;
            }
            /*
            * URGENT TODO: (5.4.2)
            */
        }
        else
        {
            pCustomErr(ctxt,
                XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_1,
                type, null,
                "The type is not a valid restriction of its base type", null);
            return ctxt.err;
        }
        return 0;
    }

    // xml_xsd10_check_ct_component
    /**
     * Port of xmlSchemaCheckCTComponent.
     * @param ctxt the schema parser context
     * @param type the complex type definition
     * @return 0 if the constraints are satisfied, a positive error code if not and -1 if an internal error occurred.
     */
    static int checkCtComponent(Xsd10ParserCtxt ctxt, Xsd10Type type)
    {
        int ret;
        /*
        * Complex Type Definition Properties Correct
        */
        ret = checkCtPropsCorrect(ctxt, type);
        if (ret != 0)
        {
            return ret;
        }
        if (wxsIsExtension(type) != 0)
        {
            ret = checkCosctExtends(ctxt, type);
        }
        else
        {
            ret = checkDerivationOkRestriction(ctxt, type);
        }
        return ret;
    }

    // xml_xsd10_check_srcct
    /**
     * Port of xmlSchemaCheckSRCCT. Schema Representation Constraint: Complex Type
     * Definition Representation OK (src-ct)
     * @param ctxt the schema parser context
     * @param type the complex type definition
     * @return 0 if the constraints are satisfied, a positive error code if not and -1 if an internal error occurred.
     */
    static int checkSrcct(Xsd10ParserCtxt ctxt, Xsd10Type type)
    {
        Xsd10Type base;
        int ret = 0;

        base = type.base_type;
        if (wxsHasSimpleContent(type) == 0)
        {
            /*
            * 1 If the <complexContent> alternative is chosen, the type definition
            * `resolved` to by the `actual value` of the base [attribute]
            * must be a complex type definition;
            */
            if (wxsIsComplex(base) == 0)
            {
                String[] str = new String[1];
                pCustomErr(ctxt,
                    XML_XSD10_SCHEMAP_SRC_CT_1,
                    type, type.node,
                    "If using <complexContent>, the base type is expected to be "
                    + "a complex type. The base type '%s' is a simple type",
                    formatQName(str, base.target_namespace,
                        base.name));
                return XML_XSD10_SCHEMAP_SRC_CT_1;
            }
        }
        else
        {
            /*
            * SPEC
            * 2 If the <simpleContent> alternative is chosen, all of the
            * following must be true:
            * 2.1 The type definition `resolved` to by the `actual value` of the
            * base [attribute] must be one of the following:
            */
            if (wxsIsSimple(base) != 0)
            {
                if (wxsIsExtension(type) == 0)
                {
                    String[] str = new String[1];
                    /*
                    * 2.1.3 only if the <extension> alternative is also
                    * chosen, a simple type definition.
                    */
                    /* TODO: Change error code to ..._SRC_CT_2_1_3. */
                    pCustomErr(ctxt,
                        XML_XSD10_SCHEMAP_SRC_CT_1,
                        type, null,
                        "If using <simpleContent> and <restriction>, the base "
                        + "type must be a complex type. The base type '%s' is "
                        + "a simple type",
                        formatQName(str, base.target_namespace,
                            base.name));
                    return XML_XSD10_SCHEMAP_SRC_CT_1;
                }
            }
            else
            {
                /* Base type is a complex type. */
                if ((base.content_type == XML_XSD10_SCHEMA_CONTENT_SIMPLE) ||
                    (base.content_type == XML_XSD10_SCHEMA_CONTENT_BASIC))
                {
                    /*
                    * 2.1.1 a complex type definition whose {content type} is a
                    * simple type definition;
                    * PASS
                    */
                    if (base.content_type_def == null)
                    {
                        pCustomErr(ctxt, XML_XSD10_SCHEMAP_INTERNAL,
                            type, null,
                            "Internal error: xmlSchemaCheckSRCCT, "
                            + "'%s', base type has no content type",
                            type.name);
                        return -1;
                    }
                }
                else if ((base.content_type == XML_XSD10_SCHEMA_CONTENT_MIXED) &&
                         (wxsIsRestriction(type) != 0))
                {

                    /*
                    * 2.1.2 only if the <restriction> alternative is also
                    * chosen, a complex type definition whose {content type}
                    * is mixed and a particle emptiable.
                    */
                    if (isParticleEmptiable(
                            asParticle(base.subtypes)) == 0)
                    {
                        ret = XML_XSD10_SCHEMAP_SRC_CT_1;
                    }
                    else
                    /*
                    * Attention: at this point the <simpleType> child is in
                    * ->content_type_def (put there during parsing).
                    */
                    if (type.content_type_def == null)
                    {
                        String[] str = new String[1];
                        /*
                        * 2.2 If clause 2.1.2 above is satisfied, then there
                        * must be a <simpleType> among the [children] of
                        * <restriction>.
                        */
                        /* TODO: Change error code to ..._SRC_CT_2_2. */
                        pCustomErr(ctxt,
                            XML_XSD10_SCHEMAP_SRC_CT_1,
                            type, null,
                            "A <simpleType> is expected among the children "
                            + "of <restriction>, if <simpleContent> is used and "
                            + "the base type '%s' is a complex type",
                            formatQName(str, base.target_namespace,
                                base.name));
                        return XML_XSD10_SCHEMAP_SRC_CT_1;
                    }
                }
                else
                {
                    ret = XML_XSD10_SCHEMAP_SRC_CT_1;
                }
            }
            if (ret > 0)
            {
                String[] str = new String[1];
                if (wxsIsRestriction(type) != 0)
                {
                    pCustomErr(ctxt,
                        XML_XSD10_SCHEMAP_SRC_CT_1,
                        type, null,
                        "If <simpleContent> and <restriction> is used, the "
                        + "base type must be a simple type or a complex type with "
                        + "mixed content and particle emptiable. The base type "
                        + "'%s' is none of those",
                        formatQName(str, base.target_namespace,
                            base.name));
                }
                else
                {
                    pCustomErr(ctxt,
                        XML_XSD10_SCHEMAP_SRC_CT_1,
                        type, null,
                        "If <simpleContent> and <extension> is used, the "
                        + "base type must be a simple type. The base type '%s' "
                        + "is a complex type",
                        formatQName(str, base.target_namespace,
                            base.name));
                }
            }
        }
        /*
        * SPEC (3) NOTE (3) will be done in xmlSchemaTypeFixup().
        * SPEC (4) NOTE (4) is done in xmlSchemaFixupTypeAttributeUses().
        */
        return ret;
    }

    // XSD10_FACET_RESTR_MUTUAL_ERR
    private static void facetRestrMutualErr(Xsd10ParserCtxt pctxt, Xsd10Facet fac1, Xsd10Facet fac2)
    {
        pCustomErrExt(pctxt,
            XML_XSD10_SCHEMAP_INVALID_FACET_VALUE,
            fac1, fac1.node,
            "It is an error for both '%s' and '%s' to be specified on the "
            + "same type definition",
            facetTypeToString(fac1.type),
            facetTypeToString(fac2.type), null);
    }

    // XSD10_FACET_RESTR_ERR
    private static void facetRestrErr(Xsd10ParserCtxt pctxt, Xsd10Facet fac1, String msg)
    {
        pCustomErr(pctxt,
            XML_XSD10_SCHEMAP_INVALID_FACET_VALUE,
            fac1, fac1.node,
            msg, null);
    }

    // XSD10_FACET_RESTR_FIXED_ERR
    private static void facetRestrFixedErr(Xsd10ParserCtxt pctxt, Xsd10Facet fac)
    {
        pCustomErr(pctxt,
            XML_XSD10_SCHEMAP_INVALID_FACET_VALUE,
            fac, fac.node,
            "The base type's facet is 'fixed', thus the value must not "
            + "differ", null);
    }

    // xml_xsd10_derive_facet_err
    /** Port of xmlSchemaDeriveFacetErr. */
    static void deriveFacetErr(Xsd10ParserCtxt pctxt, Xsd10Facet facet1, Xsd10Facet facet2, int less_greater, int or_equal, int of_base)
    {
        String msg = null;

        msg = strdup("'");
        msg = strcat(msg, facetTypeToString(facet1.type));
        msg = strcat(msg, "' has to be");
        if (less_greater == 0)
        {
            msg = strcat(msg, " equal to");
        }
        if (less_greater == 1)
        {
            msg = strcat(msg, " greater than");
        }
        else
        {
            msg = strcat(msg, " less than");
        }

        if (or_equal != 0)
        {
            msg = strcat(msg, " or equal to");
        }
        msg = strcat(msg, " '");
        msg = strcat(msg, facetTypeToString(facet2.type));
        if (of_base != 0)
        {
            msg = strcat(msg, "' of the base type");
        }
        else
        {
            msg = strcat(msg, "'");
        }

        pCustomErr(pctxt,
            XML_XSD10_SCHEMAP_INVALID_FACET_VALUE,
            facet1, null,
            msg, null);
    }

    // xml_xsd10_derive_and_validate_facets
    /**
     * Port of xmlSchemaDeriveAndValidateFacets. Schema Component Constraint: Simple Type
     * Restriction (Facets) (st-restrict-facets)
     */
    static int deriveAndValidateFacets(Xsd10ParserCtxt pctxt, Xsd10Type type)
    {
        Xsd10Type base = type.base_type;
        Xsd10FacetLink link, cur, last = null;
        Xsd10Facet facet, bfacet, flength = null, ftotdig = null, ffracdig = null, fmaxlen = null, fminlen = null, /* facets of the current type */
            fmininc = null, fmaxinc = null, fminexc = null, fmaxexc = null, bflength = null, bftotdig = null, bffracdig = null, bfmaxlen = null, bfminlen = null, /* facets of the base type */
            bfmininc = null, bfmaxinc = null, bfminexc = null, bfmaxexc = null;
        int res; /* err = 0, fixedErr; */

        /*
        * SPEC st-restrict-facets 1/2: left out, satisfied by the derivation process.
        */
        /*
        * SPEC st-restrict-facets 3:
        * "The {facets} of R are the union of S and the {facets}
        * of B, eliminating duplicates. ..."
        */

        if ((type.facet_set == null) && (base.facet_set == null))
        {
            return 0;
        }

        last = type.facet_set;
        if (last != null)
        {
            while (last.next != null)
            {
                last = last.next;
            }
        }

        for (cur = type.facet_set; cur != null; cur = cur.next)
        {
            facet = cur.facet;
            switch (facet.type)
            {
            case XML_XSD10_SCHEMA_FACET_LENGTH:
                flength = facet; break;
            case XML_XSD10_SCHEMA_FACET_MINLENGTH:
                fminlen = facet; break;
            case XML_XSD10_SCHEMA_FACET_MININCLUSIVE:
                fmininc = facet; break;
            case XML_XSD10_SCHEMA_FACET_MINEXCLUSIVE:
                fminexc = facet; break;
            case XML_XSD10_SCHEMA_FACET_MAXLENGTH:
                fmaxlen = facet; break;
            case XML_XSD10_SCHEMA_FACET_MAXINCLUSIVE:
                fmaxinc = facet; break;
            case XML_XSD10_SCHEMA_FACET_MAXEXCLUSIVE:
                fmaxexc = facet; break;
            case XML_XSD10_SCHEMA_FACET_TOTALDIGITS:
                ftotdig = facet; break;
            case XML_XSD10_SCHEMA_FACET_FRACTIONDIGITS:
                ffracdig = facet; break;
            default:
                break;
            }
        }
        for (cur = base.facet_set; cur != null; cur = cur.next)
        {
            facet = cur.facet;
            switch (facet.type)
            {
            case XML_XSD10_SCHEMA_FACET_LENGTH:
                bflength = facet; break;
            case XML_XSD10_SCHEMA_FACET_MINLENGTH:
                bfminlen = facet; break;
            case XML_XSD10_SCHEMA_FACET_MININCLUSIVE:
                bfmininc = facet; break;
            case XML_XSD10_SCHEMA_FACET_MINEXCLUSIVE:
                bfminexc = facet; break;
            case XML_XSD10_SCHEMA_FACET_MAXLENGTH:
                bfmaxlen = facet; break;
            case XML_XSD10_SCHEMA_FACET_MAXINCLUSIVE:
                bfmaxinc = facet; break;
            case XML_XSD10_SCHEMA_FACET_MAXEXCLUSIVE:
                bfmaxexc = facet; break;
            case XML_XSD10_SCHEMA_FACET_TOTALDIGITS:
                bftotdig = facet; break;
            case XML_XSD10_SCHEMA_FACET_FRACTIONDIGITS:
                bffracdig = facet; break;
            default:
                break;
            }
        }
        /*
        * length and minLength or maxLength (2.2) + (3.2)
        */
        if ((flength != null) && ((fminlen != null) || (fmaxlen != null)))
        {
            facetRestrErr(pctxt, flength, "It is an error for both 'length' and "
                + "either of 'minLength' or 'maxLength' to be specified on "
                + "the same type definition");
        }
        /*
        * Mutual exclusions in the same derivation step.
        */
        if ((fmaxinc != null) && (fmaxexc != null))
        {
            /*
            * SCC "maxInclusive and maxExclusive"
            */
            facetRestrMutualErr(pctxt, fmaxinc, fmaxexc);
        }
        if ((fmininc != null) && (fminexc != null))
        {
            /*
            * SCC "minInclusive and minExclusive"
            */
            facetRestrMutualErr(pctxt, fmininc, fminexc);
        }

        if ((flength != null) && (bflength != null))
        {
            /*
            * SCC "length valid restriction"
            * The values have to be equal.
            */
            res = compareValues(flength.val, bflength.val);
            if (res == -2)
            {
                return deriveFacetsInternalError(pctxt);
            }
            if (res != 0)
            {
                deriveFacetErr(pctxt, flength, bflength, 0, 0, 1);
            }
            if ((res != 0) && (bflength.fixed != 0))
            {
                facetRestrFixedErr(pctxt, flength);
            }

        }
        if ((fminlen != null) && (bfminlen != null))
        {
            /*
            * SCC "minLength valid restriction"
            * minLength >= BASE minLength
            */
            res = compareValues(fminlen.val, bfminlen.val);
            if (res == -2)
            {
                return deriveFacetsInternalError(pctxt);
            }
            if (res == -1)
            {
                deriveFacetErr(pctxt, fminlen, bfminlen, 1, 1, 1);
            }
            if ((res != 0) && (bfminlen.fixed != 0))
            {
                facetRestrFixedErr(pctxt, fminlen);
            }
        }
        if ((fmaxlen != null) && (bfmaxlen != null))
        {
            /*
            * SCC "maxLength valid restriction"
            * maxLength <= BASE minLength
            */
            res = compareValues(fmaxlen.val, bfmaxlen.val);
            if (res == -2)
            {
                return deriveFacetsInternalError(pctxt);
            }
            if (res == 1)
            {
                deriveFacetErr(pctxt, fmaxlen, bfmaxlen, -1, 1, 1);
            }
            if ((res != 0) && (bfmaxlen.fixed != 0))
            {
                facetRestrFixedErr(pctxt, fmaxlen);
            }
        }
        /*
        * SCC "length and minLength or maxLength"
        */
        if (flength == null)
        {
            flength = bflength;
        }
        if (flength != null)
        {
            if (fminlen == null)
            {
                fminlen = bfminlen;
            }
            if (fminlen != null)
            {
                /* (1.1) length >= minLength */
                res = compareValues(flength.val, fminlen.val);
                if (res == -2)
                {
                    return deriveFacetsInternalError(pctxt);
                }
                if (res == -1)
                {
                    deriveFacetErr(pctxt, flength, fminlen, 1, 1, 0);
                }
            }
            if (fmaxlen == null)
            {
                fmaxlen = bfmaxlen;
            }
            if (fmaxlen != null)
            {
                /* (2.1) length <= maxLength */
                res = compareValues(flength.val, fmaxlen.val);
                if (res == -2)
                {
                    return deriveFacetsInternalError(pctxt);
                }
                if (res == 1)
                {
                    deriveFacetErr(pctxt, flength, fmaxlen, -1, 1, 0);
                }
            }
        }
        if (fmaxinc != null)
        {
            /*
            * "maxInclusive"
            */
            if (fmininc != null)
            {
                /* SCC "maxInclusive >= minInclusive" */
                res = compareValues(fmaxinc.val, fmininc.val);
                if (res == -2)
                {
                    return deriveFacetsInternalError(pctxt);
                }
                if (res == -1)
                {
                    deriveFacetErr(pctxt, fmaxinc, fmininc, 1, 1, 0);
                }
            }
            /*
            * SCC "maxInclusive valid restriction"
            */
            if (bfmaxinc != null)
            {
                /* maxInclusive <= BASE maxInclusive */
                res = compareValues(fmaxinc.val, bfmaxinc.val);
                if (res == -2)
                {
                    return deriveFacetsInternalError(pctxt);
                }
                if (res == 1)
                {
                    deriveFacetErr(pctxt, fmaxinc, bfmaxinc, -1, 1, 1);
                }
                if ((res != 0) && (bfmaxinc.fixed != 0))
                {
                    facetRestrFixedErr(pctxt, fmaxinc);
                }
            }
            if (bfmaxexc != null)
            {
                /* maxInclusive < BASE maxExclusive */
                res = compareValues(fmaxinc.val, bfmaxexc.val);
                if (res == -2)
                {
                    return deriveFacetsInternalError(pctxt);
                }
                if (res != -1)
                {
                    deriveFacetErr(pctxt, fmaxinc, bfmaxexc, -1, 0, 1);
                }
            }
            if (bfmininc != null)
            {
                /* maxInclusive >= BASE minInclusive */
                res = compareValues(fmaxinc.val, bfmininc.val);
                if (res == -2)
                {
                    return deriveFacetsInternalError(pctxt);
                }
                if (res == -1)
                {
                    deriveFacetErr(pctxt, fmaxinc, bfmininc, 1, 1, 1);
                }
            }
            if (bfminexc != null)
            {
                /* maxInclusive > BASE minExclusive */
                res = compareValues(fmaxinc.val, bfminexc.val);
                if (res == -2)
                {
                    return deriveFacetsInternalError(pctxt);
                }
                if (res != 1)
                {
                    deriveFacetErr(pctxt, fmaxinc, bfminexc, 1, 0, 1);
                }
            }
        }
        if (fmaxexc != null)
        {
            /*
            * "maxExclusive >= minExclusive"
            */
            if (fminexc != null)
            {
                res = compareValues(fmaxexc.val, fminexc.val);
                if (res == -2)
                {
                    return deriveFacetsInternalError(pctxt);
                }
                if (res == -1)
                {
                    deriveFacetErr(pctxt, fmaxexc, fminexc, 1, 1, 0);
                }
            }
            /*
            * "maxExclusive valid restriction"
            */
            if (bfmaxexc != null)
            {
                /* maxExclusive <= BASE maxExclusive */
                res = compareValues(fmaxexc.val, bfmaxexc.val);
                if (res == -2)
                {
                    return deriveFacetsInternalError(pctxt);
                }
                if (res == 1)
                {
                    deriveFacetErr(pctxt, fmaxexc, bfmaxexc, -1, 1, 1);
                }
                if ((res != 0) && (bfmaxexc.fixed != 0))
                {
                    facetRestrFixedErr(pctxt, fmaxexc);
                }
            }
            if (bfmaxinc != null)
            {
                /* maxExclusive <= BASE maxInclusive */
                res = compareValues(fmaxexc.val, bfmaxinc.val);
                if (res == -2)
                {
                    return deriveFacetsInternalError(pctxt);
                }
                if (res == 1)
                {
                    deriveFacetErr(pctxt, fmaxexc, bfmaxinc, -1, 1, 1);
                }
            }
            if (bfmininc != null)
            {
                /* maxExclusive > BASE minInclusive */
                res = compareValues(fmaxexc.val, bfmininc.val);
                if (res == -2)
                {
                    return deriveFacetsInternalError(pctxt);
                }
                if (res != 1)
                {
                    deriveFacetErr(pctxt, fmaxexc, bfmininc, 1, 0, 1);
                }
            }
            if (bfminexc != null)
            {
                /* maxExclusive > BASE minExclusive */
                res = compareValues(fmaxexc.val, bfminexc.val);
                if (res == -2)
                {
                    return deriveFacetsInternalError(pctxt);
                }
                if (res != 1)
                {
                    deriveFacetErr(pctxt, fmaxexc, bfminexc, 1, 0, 1);
                }
            }
        }
        if (fminexc != null)
        {
            /*
            * "minExclusive < maxInclusive"
            */
            if (fmaxinc != null)
            {
                res = compareValues(fminexc.val, fmaxinc.val);
                if (res == -2)
                {
                    return deriveFacetsInternalError(pctxt);
                }
                if (res != -1)
                {
                    deriveFacetErr(pctxt, fminexc, fmaxinc, -1, 0, 0);
                }
            }
            /*
            * "minExclusive valid restriction"
            */
            if (bfminexc != null)
            {
                /* minExclusive >= BASE minExclusive */
                res = compareValues(fminexc.val, bfminexc.val);
                if (res == -2)
                {
                    return deriveFacetsInternalError(pctxt);
                }
                if (res == -1)
                {
                    deriveFacetErr(pctxt, fminexc, bfminexc, 1, 1, 1);
                }
                if ((res != 0) && (bfminexc.fixed != 0))
                {
                    facetRestrFixedErr(pctxt, fminexc);
                }
            }
            if (bfmaxinc != null)
            {
                /* minExclusive <= BASE maxInclusive */
                res = compareValues(fminexc.val, bfmaxinc.val);
                if (res == -2)
                {
                    return deriveFacetsInternalError(pctxt);
                }
                if (res == 1)
                {
                    deriveFacetErr(pctxt, fminexc, bfmaxinc, -1, 1, 1);
                }
            }
            if (bfmininc != null)
            {
                /* minExclusive >= BASE minInclusive */
                res = compareValues(fminexc.val, bfmininc.val);
                if (res == -2)
                {
                    return deriveFacetsInternalError(pctxt);
                }
                if (res == -1)
                {
                    deriveFacetErr(pctxt, fminexc, bfmininc, 1, 1, 1);
                }
            }
            if (bfmaxexc != null)
            {
                /* minExclusive < BASE maxExclusive */
                res = compareValues(fminexc.val, bfmaxexc.val);
                if (res == -2)
                {
                    return deriveFacetsInternalError(pctxt);
                }
                if (res != -1)
                {
                    deriveFacetErr(pctxt, fminexc, bfmaxexc, -1, 0, 1);
                }
            }
        }
        if (fmininc != null)
        {
            /*
            * "minInclusive < maxExclusive"
            */
            if (fmaxexc != null)
            {
                res = compareValues(fmininc.val, fmaxexc.val);
                if (res == -2)
                {
                    return deriveFacetsInternalError(pctxt);
                }
                if (res != -1)
                {
                    deriveFacetErr(pctxt, fmininc, fmaxexc, -1, 0, 0);
                }
            }
            /*
            * "minExclusive valid restriction"
            */
            if (bfmininc != null)
            {
                /* minInclusive >= BASE minInclusive */
                res = compareValues(fmininc.val, bfmininc.val);
                if (res == -2)
                {
                    return deriveFacetsInternalError(pctxt);
                }
                if (res == -1)
                {
                    deriveFacetErr(pctxt, fmininc, bfmininc, 1, 1, 1);
                }
                if ((res != 0) && (bfmininc.fixed != 0))
                {
                    facetRestrFixedErr(pctxt, fmininc);
                }
            }
            if (bfmaxinc != null)
            {
                /* minInclusive <= BASE maxInclusive */
                res = compareValues(fmininc.val, bfmaxinc.val);
                if (res == -2)
                {
                    return deriveFacetsInternalError(pctxt);
                }
                if (res == 1)
                {
                    deriveFacetErr(pctxt, fmininc, bfmaxinc, -1, 1, 1);
                }
            }
            if (bfminexc != null)
            {
                /* minInclusive > BASE minExclusive */
                res = compareValues(fmininc.val, bfminexc.val);
                if (res == -2)
                {
                    return deriveFacetsInternalError(pctxt);
                }
                if (res != 1)
                {
                    deriveFacetErr(pctxt, fmininc, bfminexc, 1, 0, 1);
                }
            }
            if (bfmaxexc != null)
            {
                /* minInclusive < BASE maxExclusive */
                res = compareValues(fmininc.val, bfmaxexc.val);
                if (res == -2)
                {
                    return deriveFacetsInternalError(pctxt);
                }
                if (res != -1)
                {
                    deriveFacetErr(pctxt, fmininc, bfmaxexc, -1, 0, 1);
                }
            }
        }
        if ((ftotdig != null) && (bftotdig != null))
        {
            /*
            * SCC " totalDigits valid restriction"
            * totalDigits <= BASE totalDigits
            */
            res = compareValues(ftotdig.val, bftotdig.val);
            if (res == -2)
            {
                return deriveFacetsInternalError(pctxt);
            }
            if (res == 1)
            {
                deriveFacetErr(pctxt, ftotdig, bftotdig,
                    -1, 1, 1);
            }
            if ((res != 0) && (bftotdig.fixed != 0))
            {
                facetRestrFixedErr(pctxt, ftotdig);
            }
        }
        if ((ffracdig != null) && (bffracdig != null))
        {
            /*
            * SCC  "fractionDigits valid restriction"
            * fractionDigits <= BASE fractionDigits
            */
            res = compareValues(ffracdig.val, bffracdig.val);
            if (res == -2)
            {
                return deriveFacetsInternalError(pctxt);
            }
            if (res == 1)
            {
                deriveFacetErr(pctxt, ffracdig, bffracdig,
                    -1, 1, 1);
            }
            if ((res != 0) && (bffracdig.fixed != 0))
            {
                facetRestrFixedErr(pctxt, ffracdig);
            }
        }
        /*
        * SCC "fractionDigits less than or equal to totalDigits"
        */
        if (ftotdig == null)
        {
            ftotdig = bftotdig;
        }
        if (ffracdig == null)
        {
            ffracdig = bffracdig;
        }
        if ((ftotdig != null) && (ffracdig != null))
        {
            res = compareValues(ffracdig.val, ftotdig.val);
            if (res == -2)
            {
                return deriveFacetsInternalError(pctxt);
            }
            if (res == 1)
            {
                deriveFacetErr(pctxt, ffracdig, ftotdig,
                    -1, 1, 0);
            }
        }
        /*
        * *Enumerations* and *Patterns* won't be added here.
        */
        for (cur = base.facet_set; cur != null; cur = cur.next)
        {
            bfacet = cur.facet;
            /*
            * Special handling of enumerations and patterns.
            */
            if ((bfacet.type == XML_XSD10_SCHEMA_FACET_PATTERN) ||
                (bfacet.type == XML_XSD10_SCHEMA_FACET_ENUMERATION))
            {
                continue;
            }
            /*
            * Search for a duplicate facet in the current type.
            */
            link = type.facet_set;
            while (link != null)
            {
                facet = link.facet;
                if (facet.type == bfacet.type)
                {
                    switch (facet.type)
                    {
                    case XML_XSD10_SCHEMA_FACET_WHITESPACE:
                        /*
                        * The whitespace must be stronger.
                        */
                        if (facet.whitespace < bfacet.whitespace)
                        {
                            facetRestrErr(pctxt, facet,
                                "The 'whitespace' value has to be equal to "
                                + "or stronger than the 'whitespace' value of "
                                + "the base type");
                        }
                        if ((bfacet.fixed != 0) &&
                            (facet.whitespace != bfacet.whitespace))
                        {
                            facetRestrFixedErr(pctxt, facet);
                        }
                        break;
                    default:
                        break;
                    }
                    /* Duplicate found. */
                    break;
                }
                link = link.next;
            }
            /*
            * If no duplicate was found: add the base types's facet
            * to the set.
            */
            if (link == null)
            {
                link = new Xsd10FacetLink();
                link.facet = cur.facet;
                link.next = null;
                if (last == null)
                {
                    type.facet_set = link;
                }
                else
                {
                    last.next = link;
                }
                last = link;
            }

        }

        return 0;
    }

    /** The internal_error exit of xml_xsd10_derive_and_validate_facets. */
    private static int deriveFacetsInternalError(Xsd10ParserCtxt pctxt)
    {
        perrorInt(pctxt, "xmlSchemaDeriveAndValidateFacets",
            "an error occurred");
        return -1;
    }

    // xml_xsd10_finish_member_type_definitions_property
    /** Port of xmlSchemaFinishMemberTypeDefinitionsProperty. */
    static int finishMemberTypeDefinitionsProperty(Xsd10ParserCtxt pctxt, Xsd10Type type)
    {
        Xsd10TypeLink link, last_link, prev_link, sub_link, new_link;
        /*
        * The actual value is then formed by replacing any union type
        * definition in the `explicit members` with the members of their
        * {member type definitions}, in order.
        */
        link = type.member_types;
        while (link != null)
        {
            Xsd10Type lt = asType(link.type);

            if (wxsIsTypeNotFixed(lt) != 0)
            {
                typeFixup(lt, pctxt);
            }

            if (wxsIsUnion(lt) != 0)
            {
                sub_link = getUnionSimpleTypeMemberTypes(lt);
                if (sub_link != null)
                {
                    link.type = sub_link.type;
                    if (sub_link.next != null)
                    {
                        last_link = link.next;
                        sub_link = sub_link.next;
                        prev_link = link;
                        while (sub_link != null)
                        {
                            new_link = new Xsd10TypeLink();
                            new_link.type = sub_link.type;
                            prev_link.next = new_link;
                            prev_link = new_link;
                            new_link.next = last_link;

                            sub_link = sub_link.next;
                        }
                    }
                }
            }
            link = link.next;
        }
        return 0;
    }

    // xml_xsd10_type_fixup_optim_facets
    /** Port of xmlSchemaTypeFixupOptimFacets. */
    static void typeFixupOptimFacets(Xsd10Type type)
    {
        int has = 0, need_val = 0, norm_val = 0;

        has = ((type.base_type.flags & XML_XSD10_SCHEMAS_TYPE_HAS_FACETS) != 0) ? 1 : 0;
        if (has != 0)
        {
            need_val = ((type.base_type.flags &
                XML_XSD10_SCHEMAS_TYPE_FACETSNEEDVALUE) != 0) ? 1 : 0;
            norm_val = ((type.base_type.flags &
                XML_XSD10_SCHEMAS_TYPE_NORMVALUENEEDED) != 0) ? 1 : 0;
        }
        if (type.facets != null)
        {
            Xsd10Facet fac;

            for (fac = type.facets; fac != null; fac = fac.next)
            {
                switch (fac.type)
                {
                case XML_XSD10_SCHEMA_FACET_WHITESPACE:
                    break;
                case XML_XSD10_SCHEMA_FACET_PATTERN:
                    norm_val = 1;
                    has = 1;
                    break;
                case XML_XSD10_SCHEMA_FACET_ENUMERATION:
                    need_val = 1;
                    norm_val = 1;
                    has = 1;
                    break;
                default:
                    has = 1;
                    break;
                }
            }
        }
        if (norm_val != 0)
        {
            type.flags |= XML_XSD10_SCHEMAS_TYPE_NORMVALUENEEDED;
        }
        if (need_val != 0)
        {
            type.flags |= XML_XSD10_SCHEMAS_TYPE_FACETSNEEDVALUE;
        }
        if (has != 0)
        {
            type.flags |= XML_XSD10_SCHEMAS_TYPE_HAS_FACETS;
        }

        if ((has != 0) && (need_val == 0) && (wxsIsAtomic(type) != 0))
        {
            Xsd10Type prim = getPrimitiveType(type);
            /*
            * OPTIMIZE VAL TODO: Some facets need a computed value.
            */
            if ((prim != null) &&
                (prim.built_in_type != XML_XSD10_SCHEMAS_ANYSIMPLETYPE) &&
                (prim.built_in_type != XML_XSD10_SCHEMAS_STRING))
            {
                type.flags |= XML_XSD10_SCHEMAS_TYPE_FACETSNEEDVALUE;
            }
        }
    }

    // xml_xsd10_type_fixup_whitespace
    /** Port of xmlSchemaTypeFixupWhitespace. */
    static int typeFixupWhitespace(Xsd10Type type)
    {
        /*
        * Evaluate the whitespace-facet value.
        */
        if (wxsIsList(type) != 0)
        {
            type.flags |= XML_XSD10_SCHEMAS_TYPE_WHITESPACE_COLLAPSE;
            return 0;
        }
        else if (wxsIsUnion(type) != 0)
        {
            return 0;
        }

        if (type.facet_set != null)
        {
            Xsd10FacetLink lin;

            for (lin = type.facet_set; lin != null; lin = lin.next)
            {
                if (lin.facet.type == XML_XSD10_SCHEMA_FACET_WHITESPACE)
                {
                    switch (lin.facet.whitespace)
                    {
                    case XML_XSD10_SCHEMAS_FACET_PRESERVE:
                        type.flags |= XML_XSD10_SCHEMAS_TYPE_WHITESPACE_PRESERVE;
                        break;
                    case XML_XSD10_SCHEMAS_FACET_REPLACE:
                        type.flags |= XML_XSD10_SCHEMAS_TYPE_WHITESPACE_REPLACE;
                        break;
                    case XML_XSD10_SCHEMAS_FACET_COLLAPSE:
                        type.flags |= XML_XSD10_SCHEMAS_TYPE_WHITESPACE_COLLAPSE;
                        break;
                    default:
                        return -1;
                    }
                    return 0;
                }
            }
        }
        /*
        * For all `atomic` datatypes other than string (and types `derived`
        * by `restriction` from it) the value of whiteSpace is fixed to
        * collapse
        */
        {
            Xsd10Type anc;

            for (anc = type.base_type; (anc != null) &&
                     (anc.built_in_type != XML_XSD10_SCHEMAS_ANYTYPE);
                 anc = anc.base_type)
            {

                if (anc.type == XML_XSD10_SCHEMA_TYPE_BASIC)
                {
                    if (anc.built_in_type == XML_XSD10_SCHEMAS_NORMSTRING)
                    {
                        type.flags |= XML_XSD10_SCHEMAS_TYPE_WHITESPACE_REPLACE;

                    }
                    else if ((anc.built_in_type == XML_XSD10_SCHEMAS_STRING) ||
                             (anc.built_in_type == XML_XSD10_SCHEMAS_ANYSIMPLETYPE))
                    {
                        type.flags |= XML_XSD10_SCHEMAS_TYPE_WHITESPACE_PRESERVE;

                    }
                    else
                    {
                        type.flags |= XML_XSD10_SCHEMAS_TYPE_WHITESPACE_COLLAPSE;
                    }
                    break;
                }
            }
        }
        return 0;
    }

    // xml_xsd10_fixup_simple_type_stage_one
    /** Port of xmlSchemaFixupSimpleTypeStageOne. */
    static int fixupSimpleTypeStageOne(Xsd10ParserCtxt pctxt, Xsd10Type type)
    {
        if (type.type != XML_XSD10_SCHEMA_TYPE_SIMPLE)
        {
            return 0;
        }
        if (wxsIsTypeNotFixed1(type) == 0)
        {
            return 0;
        }
        type.flags |= XML_XSD10_SCHEMAS_TYPE_FIXUP_1;

        if (wxsIsList(type) != 0)
        {
            /*
            * Corresponds to <simpleType><list>...
            */
            if (type.subtypes == null)
            {
                /*
                * This one is really needed, so get out.
                */
                perrorInt(pctxt, "xmlSchemaFixupSimpleTypeStageOne",
                    "list type has no item-type assigned");
                return -1;
            }
        }
        else if (wxsIsUnion(type) != 0)
        {
            /*
            * Corresponds to <simpleType><union>...
            */
            if (type.member_types == null)
            {
                /*
                * This one is really needed, so get out.
                */
                perrorInt(pctxt, "xmlSchemaFixupSimpleTypeStageOne",
                    "union type has no member-types assigned");
                return -1;
            }
        }
        else
        {
            /*
            * Corresponds to <simpleType><restriction>...
            */
            if (type.base_type == null)
            {
                perrorInt(pctxt, "xmlSchemaFixupSimpleTypeStageOne",
                    "type has no base-type assigned");
                return -1;
            }
            if (wxsIsTypeNotFixed1(type.base_type) != 0)
            {
                if (fixupSimpleTypeStageOne(pctxt, type.base_type) == -1)
                {
                    return -1;
                }
            }
            /*
            * Variety
            * If the <restriction> alternative is chosen, then the
            * {variety} of the {base type definition}.
            */
            if (wxsIsAtomic(type.base_type) != 0)
            {
                type.flags |= XML_XSD10_SCHEMAS_TYPE_VARIETY_ATOMIC;
            }
            else if (wxsIsList(type.base_type) != 0)
            {
                type.flags |= XML_XSD10_SCHEMAS_TYPE_VARIETY_LIST;
                /*
                * Inherit the item_type.
                */
                type.subtypes = type.base_type.subtypes;
            }
            else if (wxsIsUnion(type.base_type) != 0)
            {
                type.flags |= XML_XSD10_SCHEMAS_TYPE_VARIETY_UNION;
                /*
                * NOTE that we won't assign the member_types of the base,
                * since this will make trouble when freeing them; we will
                * use a lookup function to access them instead.
                */
            }
        }
        return 0;
    }

    // xml_xsd10_fixup_simple_type_stage_two
    /** Port of xmlSchemaFixupSimpleTypeStageTwo. 3.14.6 Constraints on Simple Type Definition Schema Components. */
    static int fixupSimpleTypeStageTwo(Xsd10ParserCtxt pctxt, Xsd10Type type)
    {
        int res, olderrs = pctxt.nberrors;

        if (type.type != XML_XSD10_SCHEMA_TYPE_SIMPLE)
        {
            return -1;
        }

        if (wxsIsTypeNotFixed(type) == 0)
        {
            return 0;
        }

        type.flags |= XML_XSD10_SCHEMAS_TYPE_INTERNAL_RESOLVED;
        type.content_type = XML_XSD10_SCHEMA_CONTENT_SIMPLE;

        if (type.base_type == null)
        {
            perrorInt(pctxt, "xmlSchemaFixupSimpleTypeStageTwo",
                "missing baseType");
            return -1;
        }
        if (wxsIsTypeNotFixed(type.base_type) != 0)
        {
            typeFixup(type.base_type, pctxt);
        }
        /*
        * If a member type of a union is a union itself, we need to substitute
        * that member type for its member types.
        */
        if ((type.member_types != null) &&
            (finishMemberTypeDefinitionsProperty(pctxt, type) == -1))
        {
            return -1;
        }
        exit_error:
        {
            /*
            * Schema Component Constraint: Simple Type Definition Properties Correct
            * (st-props-correct)
            */
            res = checkStPropsCorrect(pctxt, type);
            if (res == -1) return -1;
            if (res != 0) break exit_error;
            /*
            * Schema Component Constraint: Derivation Valid (Restriction, Simple)
            * (cos-st-restricts)
            */
            res = checkCosstRestricts(pctxt, type);
            if (res == -1) return -1;
            if (res != 0) break exit_error;
            /*
            * Schema Component Constraint: Simple Type Restriction (Facets)
            * (st-restrict-facets)
            */
            res = checkFacetValues(type, pctxt);
            if (res == -1) return -1;
            if (res != 0) break exit_error;
            if ((type.facet_set != null) ||
                (type.base_type.facet_set != null))
            {
                res = deriveAndValidateFacets(pctxt, type);
                if (res == -1) return -1;
                if (res != 0) break exit_error;
            }
            /*
            * Whitespace value.
            */
            res = typeFixupWhitespace(type);
            if (res == -1) return -1;
            if (res != 0) break exit_error;
            typeFixupOptimFacets(type);
        }

        /* exit_error: */
        if (olderrs != pctxt.nberrors)
        {
            return pctxt.err;
        }
        return 0;
    }

    // xml_xsd10_fixup_complex_type
    /** Port of xmlSchemaFixupComplexType. */
    static int fixupComplexType(Xsd10ParserCtxt pctxt, Xsd10Type type)
    {
        int res = 0, olderrs = pctxt.nberrors;
        Xsd10Type base_type = type.base_type;

        if (wxsIsTypeNotFixed(type) == 0)
        {
            return 0;
        }
        type.flags |= XML_XSD10_SCHEMAS_TYPE_INTERNAL_RESOLVED;
        if (base_type == null)
        {
            perrorInt(pctxt, "xmlSchemaFixupComplexType",
                "missing baseType");
            type.flags |= XML_XSD10_SCHEMAS_TYPE_INTERNAL_INVALID;
            return -1;
        }
        /*
        * Fixup the base type.
        */
        if (wxsIsTypeNotFixed(base_type) != 0)
        {
            typeFixup(base_type, pctxt);
        }
        if ((base_type.flags & XML_XSD10_SCHEMAS_TYPE_INTERNAL_INVALID) != 0)
        {
            /*
            * Skip fixup if the base type is invalid.
            * TODO: Generate a warning!
            */
            return 0;
        }
        res = fixupComplexTypeBody(pctxt, type, base_type);
        if (res == -1)
        {
            /* exit_failure: */
            type.flags |= XML_XSD10_SCHEMAS_TYPE_INTERNAL_INVALID;
            return -1;
        }
        if (res != 0)
        {
            /* exit_error: */
            type.flags |= XML_XSD10_SCHEMAS_TYPE_INTERNAL_INVALID;
            return pctxt.err;
        }

        if (olderrs != pctxt.nberrors)
        {
            return pctxt.err;
        }
        else
        {
            return 0;
        }
    }

    /**
     * Body of xml_xsd10_fixup_complex_type after the base type checks.
     * @return -1 for goto exit_failure, a non zero value for goto exit_error, 0 otherwise.
     */
    private static int fixupComplexTypeBody(Xsd10ParserCtxt pctxt, Xsd10Type type, Xsd10Type base_type)
    {
        int res;
        /*
        * This basically checks if the base type can be derived.
        */
        res = checkSrcct(pctxt, type);
        if (res != 0) return res;
        /*
        * Fixup the content type.
        */
        if (type.content_type == XML_XSD10_SCHEMA_CONTENT_SIMPLE)
        {
            /*
            * Corresponds to <complexType><simpleContent>...
            */
            if ((wxsIsComplex(base_type) != 0) &&
                (base_type.content_type_def != null) &&
                (wxsIsRestriction(type) != 0))
            {
                Xsd10Type content_base, content;
                /*
                * SPEC (1) If <restriction> + base type is <complexType>,
                * "whose own {content type} is a simple type..."
                */
                if (type.content_type_def != null)
                {
                    /*
                    * SPEC (1.1) "the simple type definition corresponding to the
                    * <simpleType> among the [children] of <restriction> if there
                    * is one;"
                    */
                    content_base = type.content_type_def;
                    type.content_type_def = null;
                }
                else
                {
                    /*
                    * (1.2) "...otherwise (<restriction> has no <simpleType>
                    * among its [children]), the simple type definition which
                    * is the {content type} of the ... base type."
                    */
                    content_base = base_type.content_type_def;
                }
                /*
                * Create the anonymous simple type, which will be the content
                * type of the complex type.
                */
                content = addType(pctxt, pctxt.schema,
                    XML_XSD10_SCHEMA_TYPE_SIMPLE, null, type.target_namespace,
                    type.node, 0);
                if (content == null)
                {
                    return -1;
                }
                /*
                * We will use the same node as for the <complexType>
                * to have it somehow anchored in the schema doc.
                */
                content.type = XML_XSD10_SCHEMA_TYPE_SIMPLE;
                content.base_type = content_base;
                /*
                * Move the facets, previously anchored on the
                * complexType during parsing.
                */
                content.facets = type.facets;
                type.facets = null;
                content.facet_set = type.facet_set;
                type.facet_set = null;

                type.content_type_def = content;
                if (wxsIsTypeNotFixed(content_base) != 0)
                {
                    typeFixup(content_base, pctxt);
                }
                /*
                * Fixup the newly created type. We don't need to check
                * for circularity here.
                */
                res = fixupSimpleTypeStageOne(pctxt, content);
                if (res != 0) return res;
                res = fixupSimpleTypeStageTwo(pctxt, content);
                if (res != 0) return res;

            }
            else if ((wxsIsComplex(base_type) != 0) &&
                     (base_type.content_type == XML_XSD10_SCHEMA_CONTENT_MIXED) &&
                     (wxsIsRestriction(type) != 0))
            {
                /*
                * SPEC (2) If <restriction> + base is a mixed <complexType> with
                * an emptiable particle, then a simple type definition which
                * restricts the <restriction>'s <simpleType> child.
                */
                if ((type.content_type_def == null) ||
                    (type.content_type_def.base_type == null))
                {
                    /*
                    * TODO: Check if this ever happens.
                    */
                    pCustomErr(pctxt,
                        XML_XSD10_SCHEMAP_INTERNAL,
                        type, null,
                        "Internal error: xmlSchemaTypeFixup, "
                        + "complex type '%s': the <simpleContent><restriction> "
                        + "is missing a <simpleType> child, but was not caught "
                        + "by xmlSchemaCheckSRCCT()", type.name);
                    return -1;
                }
            }
            else if ((wxsIsComplex(base_type) != 0) && (wxsIsExtension(type) != 0))
            {
                /*
                * SPEC (3) If <extension> + base is <complexType> with
                * <simpleType> content, "...then the {content type} of that
                * complex type definition"
                */
                if (base_type.content_type_def == null)
                {
                    /*
                    * TODO: Check if this ever happens. xmlSchemaCheckSRCCT
                    * should have caught this already.
                    */
                    pCustomErr(pctxt,
                        XML_XSD10_SCHEMAP_INTERNAL,
                        type, null,
                        "Internal error: xmlSchemaTypeFixup, "
                        + "complex type '%s': the <extension>ed base type is "
                        + "a complex type with no simple content type",
                        type.name);
                    return -1;
                }
                type.content_type_def = base_type.content_type_def;
            }
            else if ((wxsIsSimple(base_type) != 0) && (wxsIsExtension(type) != 0))
            {
                /*
                * SPEC (4) <extension> + base is <simpleType>
                * "... then that simple type definition"
                */
                type.content_type_def = base_type;
            }
            else
            {
                /*
                * TODO: Check if this ever happens.
                */
                pCustomErr(pctxt,
                    XML_XSD10_SCHEMAP_INTERNAL,
                    type, null,
                    "Internal error: xmlSchemaTypeFixup, "
                    + "complex type '%s' with <simpleContent>: unhandled "
                    + "derivation case", type.name);
                return -1;
            }
        }
        else
        {
            int dummy_sequence = 0;
            Xsd10Particle particle = asParticle(type.subtypes);
            /*
            * Corresponds to <complexType><complexContent>...
            *
            * Compute the "effective content":
            * (2.1.1) + (2.1.2) + (2.1.3)
            */
            if ((particle == null) ||
                ((particle.type == XML_XSD10_SCHEMA_TYPE_PARTICLE) &&
                 ((particle.children.type == XML_XSD10_SCHEMA_TYPE_ALL) ||
                  (particle.children.type == XML_XSD10_SCHEMA_TYPE_SEQUENCE) ||
                  ((particle.children.type == XML_XSD10_SCHEMA_TYPE_CHOICE) &&
                   (particle.min_occurs == 0))) &&
                 (((Xsd10TreeItem) particle.children).children == null)))
            {
                if ((type.flags & XML_XSD10_SCHEMAS_TYPE_MIXED) != 0)
                {
                    /*
                    * SPEC (2.1.4) "If the `effective mixed` is true, then
                    * a particle whose properties are as follows:..."
                    */
                    if ((particle == null) ||
                        (particle.children.type != XML_XSD10_SCHEMA_TYPE_SEQUENCE))
                    {
                        /*
                        * Create the particle.
                        */
                        particle = addParticle(pctxt,
                            type.node, 1, 1);
                        if (particle == null)
                        {
                            return -1;
                        }
                        /*
                        * Create the model group.
                        */ /* URGENT TODO: avoid adding to pending items. */
                        particle.children =
                            addModelGroup(pctxt, pctxt.schema,
                                XML_XSD10_SCHEMA_TYPE_SEQUENCE, type.node);
                        if (particle.children == null)
                        {
                            return -1;
                        }

                        type.subtypes = particle;
                    }
                    dummy_sequence = 1;
                    type.content_type = XML_XSD10_SCHEMA_CONTENT_ELEMENTS;
                }
                else
                {
                    /*
                    * SPEC (2.1.5) "otherwise empty"
                    */
                    type.content_type = XML_XSD10_SCHEMA_CONTENT_EMPTY;
                }
            }
            else
            {
                /*
                * SPEC (2.2) "otherwise the particle corresponding to the
                * <all>, <choice>, <group> or <sequence> among the
                * [children]."
                */
                type.content_type = XML_XSD10_SCHEMA_CONTENT_ELEMENTS;
            }
            /*
            * Compute the "content type".
            */
            if (wxsIsRestriction(type) != 0)
            {
                /*
                * SPEC (3.1) "If <restriction>..."
                * (3.1.1) + (3.1.2) */
                if (type.content_type != XML_XSD10_SCHEMA_CONTENT_EMPTY)
                {
                    if ((type.flags & XML_XSD10_SCHEMAS_TYPE_MIXED) != 0)
                    {
                        type.content_type = XML_XSD10_SCHEMA_CONTENT_MIXED;
                    }
                }
            }
            else
            {
                /*
                * SPEC (3.2) "If <extension>..."
                */
                if (type.content_type == XML_XSD10_SCHEMA_CONTENT_EMPTY)
                {
                    /*
                    * SPEC (3.2.1)
                    * "If the `effective content` is empty, then the
                    *  {content type} of the [...] base ..."
                    */
                    type.content_type = base_type.content_type;
                    type.subtypes = base_type.subtypes;
                    /*
                    * Fixes bug #347316:
                    * This is the case when the base type has a simple
                    * type definition as content.
                    */
                    type.content_type_def = base_type.content_type_def;
                    /*
                    * NOTE that the effective mixed is ignored here.
                    */
                }
                else if (base_type.content_type == XML_XSD10_SCHEMA_CONTENT_EMPTY)
                {
                    /*
                    * SPEC (3.2.2)
                    */
                    if ((type.flags & XML_XSD10_SCHEMAS_TYPE_MIXED) != 0)
                    {
                        type.content_type = XML_XSD10_SCHEMA_CONTENT_MIXED;
                    }
                }
                else
                {
                    /*
                    * SPEC (3.2.3)
                    */
                    if ((type.flags & XML_XSD10_SCHEMAS_TYPE_MIXED) != 0)
                    {
                        type.content_type = XML_XSD10_SCHEMA_CONTENT_MIXED;
                    }
                    /*
                    * "A model group whose {compositor} is sequence and whose
                    * {particles} are..."
                    */
                    if ((asParticle(type.subtypes) != null) &&
                        (wxsTypeParticleTerm(type) != null) &&
                        ((wxsTypeParticleTerm(type)).type ==
                            XML_XSD10_SCHEMA_TYPE_ALL))
                    {
                        /*
                        * SPEC cos-all-limited (1)
                        */
                        customErr(pctxt,
                            /* TODO: error code */
                            XML_XSD10_SCHEMAP_COS_ALL_LIMITED,
                            wxsItemNode(type), null,
                            "The type has an 'all' model group in its "
                            + "{content type} and thus cannot be derived from "
                            + "a non-empty type, since this would produce a "
                            + "'sequence' model group containing the 'all' "
                            + "model group; 'all' model groups are not "
                            + "allowed to appear inside other model groups",
                            null, null);

                    }
                    else if ((asParticle(base_type.subtypes) != null) &&
                             (wxsTypeParticleTerm(base_type) != null) &&
                             ((wxsTypeParticleTerm(base_type)).type ==
                                 XML_XSD10_SCHEMA_TYPE_ALL))
                    {
                        /*
                        * SPEC cos-all-limited (1)
                        */
                        customErr(pctxt,
                            /* TODO: error code */
                            XML_XSD10_SCHEMAP_COS_ALL_LIMITED,
                            wxsItemNode(type), null,
                            "A type cannot be derived by extension from a type "
                            + "which has an 'all' model group in its "
                            + "{content type}, since this would produce a "
                            + "'sequence' model group containing the 'all' "
                            + "model group; 'all' model groups are not "
                            + "allowed to appear inside other model groups",
                            null, null);

                    }
                    else if ((dummy_sequence == 0) && (base_type.subtypes != null))
                    {
                        Xsd10TreeItem effective_content =
                            (Xsd10TreeItem) type.subtypes;
                        Xsd10Particle base_particle = (Xsd10Particle) base_type.subtypes;
                        /*
                        * Create the particle.
                        */
                        particle = addParticle(pctxt,
                            type.node, 1, 1);
                        if (particle == null)
                        {
                            return -1;
                        }
                        /*
                        * Create the "sequence" model group.
                        */
                        particle.children =
                            addModelGroup(pctxt, pctxt.schema,
                                XML_XSD10_SCHEMA_TYPE_SEQUENCE, type.node);
                        if (particle.children == null)
                        {
                            return -1;
                        }
                        type.subtypes = particle;
                        /*
                        * SPEC "the particle of the {content type} of
                        * the ... base ..."
                        * Create a duplicate of the base type's particle
                        * and assign its "term" to it.
                        */
                        ((Xsd10TreeItem) particle.children).children =
                            addParticle(pctxt,
                                type.node,
                                base_particle.min_occurs,
                                base_particle.max_occurs);
                        if (((Xsd10TreeItem) particle.children).children == null)
                        {
                            return -1;
                        }
                        particle = (Xsd10Particle)
                            ((Xsd10TreeItem) particle.children).children;
                        particle.children =
                            base_particle.children;
                        /*
                        * SPEC "followed by the `effective content`."
                        */
                        particle.next = effective_content;
                        /*
                        * This all will result in:
                        * new-particle
                        *   --> new-sequence(
                        *         new-particle
                        *           --> base-model,
                        *         this-particle
                        *           --> this-model
                        *       )
                        */
                    }
                    else
                    {
                        /*
                        * This is the case when there is already an empty
                        * <sequence> with min_occurs==max_occurs==1.
                        * Just add the base types's content type.
                        */
                        ((Xsd10TreeItem) particle.children).children =
                            base_type.subtypes;
                    }
                }
            }
        }
        /*
        * Now fixup attribute uses:
        *   - expand attr. group references
        *     - intersect attribute wildcards
        *   - inherit attribute uses of the base type
        *   - inherit or union attr. wildcards if extending
        *   - apply attr. use prohibitions if restricting
        */
        res = fixupTypeAttributeUses(pctxt, type);
        if (res != 0) return res;
        /*
        * Apply the complex type component constraints; this will not
        * check attributes, since this is done in
        * xmlSchemaFixupTypeAttributeUses().
        */
        res = checkCtComponent(pctxt, type);
        if (res != 0) return res;
        return 0;
    }

    // xml_xsd10_type_fixup
    /**
     * Port of xmlSchemaTypeFixup. Fixes the content model of the type.
     * @param type the schema type definition
     * @param actxt the schema parser context
     * @return the fixup result
     */
    static int typeFixup(Xsd10Type type, Xsd10AbstractCtxt actxt)
    {
        if (type == null)
        {
            return 0;
        }
        if (actxt.type != XML_XSD10_SCHEMA_CTXT_PARSER)
        {
            aerrorInt(actxt, "xmlSchemaTypeFixup",
                "this function needs a parser context");
            return -1;
        }
        if (wxsIsTypeNotFixed(type) == 0)
        {
            return 0;
        }
        if (type.type == XML_XSD10_SCHEMA_TYPE_COMPLEX)
        {
            return fixupComplexType((Xsd10ParserCtxt) actxt, type);
        }
        else if (type.type == XML_XSD10_SCHEMA_TYPE_SIMPLE)
        {
            return fixupSimpleTypeStageTwo((Xsd10ParserCtxt) actxt, type);
        }
        return 0;
    }

    // xml_xsd10_check_facet_ctxt
    /**
     * Port of xmlSchemaCheckFacet. Checks and computes the values of facets.
     * @param facet the facet
     * @param type_decl the schema type definition
     * @param pctxt the schema parser context or null
     * @param name the optional name of the type
     * @return 0 if valid, a positive error code if not valid and -1 in case of an internal or API error.
     */
    static int checkFacetCtxt(Xsd10Facet facet, Xsd10Type type_decl, Xsd10ParserCtxt pctxt, String name)
    {
        int ret = 0, ctxt_given;

        if ((facet == null) || (type_decl == null))
        {
            return -1;
        }
        /*
        * TODO: will the parser context be given if used from
        * the relaxNG module?
        */
        if (pctxt == null)
        {
            ctxt_given = 0;
        }
        else
        {
            ctxt_given = 1;
        }

        switch (facet.type)
        {
        case XML_XSD10_SCHEMA_FACET_MININCLUSIVE:
        case XML_XSD10_SCHEMA_FACET_MINEXCLUSIVE:
        case XML_XSD10_SCHEMA_FACET_MAXINCLUSIVE:
        case XML_XSD10_SCHEMA_FACET_MAXEXCLUSIVE:
        case XML_XSD10_SCHEMA_FACET_ENUMERATION:
        {
            /*
             * Okay we need to validate the value
             * at that point.
             */
            Xsd10Type base;

            /* 4.3.5.5 Constraints on enumeration Schema Components
            * Schema Component Constraint: enumeration valid restriction
            * It is an `error` if any member of {value} is not in the
            * `value space` of {base type definition}.
            *
            * minInclusive, maxInclusive, minExclusive, maxExclusive:
            * The value `must` be in the
            * `value space` of the `base type`.
            */
            if (type_decl.type != XML_XSD10_SCHEMA_TYPE_BASIC)
            {
                base = type_decl.base_type;
                if (base == null)
                {
                    perrorInt(pctxt, "xmlSchemaCheckFacet",
                        "a type user derived type has no base type");
                    return -1;
                }
            }
            else
            {
                base = type_decl;
            }

            if (ctxt_given == 0)
            {
                /*
                * A context is needed if called from RelaxNG.
                */
                pctxt = newParserCtxt("*");
                if (pctxt == null)
                {
                    return -1;
                }
            }
            /*
            * NOTE: This call does not check the content nodes,
            * since they are not available:
            * facet->node is just the node holding the facet
            * definition, *not* the attribute holding the *value*
            * of the facet.
            */
            {
                Xsd10Val[] fv = new Xsd10Val[] { facet.val };
                ret = vCheckCvcSimpleType(
                    pctxt, facet.node, base,
                    facet.value, fv, 1, 1, 0);
                facet.val = fv[0];
            }
            if (ret != 0)
            {
                if (ret < 0)
                {
                    /* No error message for RelaxNG. */
                    if (ctxt_given != 0)
                    {
                        customErr(pctxt,
                            XML_XSD10_SCHEMAP_INTERNAL, facet.node, null,
                            "Internal error: xmlSchemaCheckFacet, "
                            + "failed to validate the value '%s' of the "
                            + "facet '%s' against the base type",
                            facet.value, facetTypeToString(facet.type));
                    }
                    /* internal_error: */
                    if ((ctxt_given == 0) && (pctxt != null))
                    {
                        freeParserCtxt(pctxt);
                    }
                    return -1;
                }
                ret = XML_XSD10_SCHEMAP_INVALID_FACET_VALUE;
                /* No error message for RelaxNG. */
                if (ctxt_given != 0)
                {
                    String[] str = new String[1];

                    customErr(pctxt,
                        ret, facet.node, facet,
                        "The value '%s' of the facet does not validate "
                        + "against the base type '%s'",
                        facet.value,
                        formatQName(str,
                            base.target_namespace, base.name));
                }
                /* goto exit; */
                break;
            }
            else if (facet.val == null)
            {
                if (ctxt_given != 0)
                {
                    perrorInt(pctxt, "xmlSchemaCheckFacet",
                        "value was not computed");
                }
                /* TODO */
            }
            break;
        }
        case XML_XSD10_SCHEMA_FACET_PATTERN:
            facet.regexp = XmlRegexp.regexpCompile(facet.value);
            if (facet.regexp == null)
            {
                ret = XML_XSD10_SCHEMAP_REGEXP_INVALID;
                /* No error message for RelaxNG. */
                if (ctxt_given != 0)
                {
                    customErr(pctxt,
                        ret, facet.node, type_decl,
                        "The value '%s' of the facet 'pattern' is not a "
                        + "valid regular expression",
                        facet.value, null);
                }
            }
            break;
        case XML_XSD10_SCHEMA_FACET_TOTALDIGITS:
        case XML_XSD10_SCHEMA_FACET_FRACTIONDIGITS:
        case XML_XSD10_SCHEMA_FACET_LENGTH:
        case XML_XSD10_SCHEMA_FACET_MAXLENGTH:
        case XML_XSD10_SCHEMA_FACET_MINLENGTH:
        {
            Xsd10Val[] fv = new Xsd10Val[] { facet.val };
            if (facet.type == XML_XSD10_SCHEMA_FACET_TOTALDIGITS)
            {
                ret = validatePredefinedType(
                    getBuiltInType(XML_XSD10_SCHEMAS_PINTEGER),
                    facet.value, fv);
            }
            else
            {
                ret = validatePredefinedType(
                    getBuiltInType(XML_XSD10_SCHEMAS_NNINTEGER),
                    facet.value, fv);
            }
            facet.val = fv[0];
            if (ret != 0)
            {
                if (ret < 0)
                {
                    /* No error message for RelaxNG. */
                    if (ctxt_given != 0)
                    {
                        perrorInt(pctxt, "xmlSchemaCheckFacet",
                            "validating facet value");
                    }
                    /* internal_error: */
                    if ((ctxt_given == 0) && (pctxt != null))
                    {
                        freeParserCtxt(pctxt);
                    }
                    return -1;
                }
                ret = XML_XSD10_SCHEMAP_INVALID_FACET_VALUE;
                /* No error message for RelaxNG. */
                if (ctxt_given != 0)
                {
                    /* error code */
                    customErr4(pctxt,
                        ret, facet.node, type_decl,
                        "The value '%s' of the facet '%s' is not a valid '%s'",
                        facet.value,
                        facetTypeToString(facet.type),
                        (facet.type != XML_XSD10_SCHEMA_FACET_TOTALDIGITS) ?
                            "nonNegativeInteger" :
                            "positiveInteger",
                        null);
                }
            }
            break;
        }

        case XML_XSD10_SCHEMA_FACET_WHITESPACE:
        {
            if (strEqual(facet.value, "preserve") != 0)
            {
                facet.whitespace = XML_XSD10_SCHEMAS_FACET_PRESERVE;
            }
            else if (strEqual(facet.value, "replace") != 0)
            {
                facet.whitespace = XML_XSD10_SCHEMAS_FACET_REPLACE;
            }
            else if (strEqual(facet.value, "collapse") != 0)
            {
                facet.whitespace = XML_XSD10_SCHEMAS_FACET_COLLAPSE;
            }
            else
            {
                ret = XML_XSD10_SCHEMAP_INVALID_FACET_VALUE;
                /* No error message for RelaxNG. */
                if (ctxt_given != 0)
                {
                    /* error was previously: XML_SCHEMAP_INVALID_WHITE_SPACE */
                    customErr(pctxt,
                        ret, facet.node, type_decl,
                        "The value '%s' of the facet 'whitespace' is not "
                        + "valid", facet.value, null);
                }
            }
            break;
        }
        default:
            break;
        }
        /* exit: */
        if ((ctxt_given == 0) && (pctxt != null))
        {
            freeParserCtxt(pctxt);
        }
        return ret;
    }

    // xml_xsd10_check_facet_values
    /**
     * Port of xmlSchemaCheckFacetValues. Checks the default values types, especially for facets.
     * @param type_decl the schema type definition
     * @param pctxt the schema parser context
     * @return 0, the last error code or -1
     */
    static int checkFacetValues(Xsd10Type type_decl, Xsd10ParserCtxt pctxt)
    {
        int res, olderrs = pctxt.nberrors;
        String name = type_decl.name;
        /*
        * NOTE: It is intended to use the facets list, instead
        * of facet_set.
        */
        if (type_decl.facets != null)
        {
            Xsd10Facet facet = type_decl.facets;

            /*
            * Temporarily assign the "schema" to the validation context
            * of the parser context. This is needed for NOTATION validation.
            */
            if (pctxt.vctxt == null)
            {
                if (createVCtxtOnPCtxt(pctxt) == -1)
                {
                    return -1;
                }
            }
            pctxt.vctxt.schema = pctxt.schema;
            while (facet != null)
            {
                res = checkFacetCtxt(facet, type_decl, pctxt, name);
                if (res == -1)
                {
                    return -1;
                }
                facet = facet.next;
            }
            pctxt.vctxt.schema = null;
        }
        if (olderrs != pctxt.nberrors)
        {
            return pctxt.err;
        }
        return 0;
    }

    // xml_xsd10_get_circ_model_gr_def_ref
    /**
     * Port of xmlSchemaGetCircModelGrDefRef. Intended to be used by
     * xmlSchemaCheckGroupDefCircular only.
     * @param group_def the searched model group
     * @param particle the first particle
     * @return the particle with the circular model group definition reference, otherwise null.
     */
    static Xsd10TreeItem getCircModelGrDefRef(Xsd10ModelGroupDef group_def, Xsd10TreeItem particle)
    {
        Xsd10TreeItem circ = null;
        Xsd10BasicItem term;
        Xsd10ModelGroupDef gdef;

        for (; particle != null; particle = particle.next)
        {
            term = particle.children;
            if (term == null)
            {
                continue;
            }
            switch (term.type)
            {
            case XML_XSD10_SCHEMA_TYPE_GROUP:
                gdef = (Xsd10ModelGroupDef) term;
                if (gdef == group_def)
                {
                    return particle;
                }
                /*
                * Mark this model group definition to avoid infinite
                * recursion on circular references not yet examined.
                */
                if ((gdef.flags & XML_XSD10_SCHEMA_MODEL_GROUP_DEF_MARKED) != 0)
                {
                    continue;
                }
                if (gdef.children != null)
                {
                    gdef.flags |= XML_XSD10_SCHEMA_MODEL_GROUP_DEF_MARKED;
                    circ = getCircModelGrDefRef(group_def,
                        (Xsd10TreeItem) ((Xsd10TreeItem) gdef.children).children);
                    gdef.flags ^= XML_XSD10_SCHEMA_MODEL_GROUP_DEF_MARKED;
                    if (circ != null)
                    {
                        return circ;
                    }
                }
                break;
            case XML_XSD10_SCHEMA_TYPE_SEQUENCE:
            case XML_XSD10_SCHEMA_TYPE_CHOICE:
            case XML_XSD10_SCHEMA_TYPE_ALL:
                circ = getCircModelGrDefRef(group_def, (Xsd10TreeItem) ((Xsd10TreeItem) term).children);
                if (circ != null)
                {
                    return circ;
                }
                break;
            default:
                break;
            }
        }
        return null;
    }

    // xml_xsd10_check_group_def_circular
    /**
     * Port of xmlSchemaCheckGroupDefCircular. Checks for circular references to model
     * group definitions.
     * @param item the model group definition
     * @param ctxt the parser context
     */
    static void checkGroupDefCircular(Xsd10ModelGroupDef item, Xsd10ParserCtxt ctxt)
    {
        /*
        * Schema Component Constraint: Model Group Correct
        * 2 Circular groups are disallowed. That is, within the {particles}
        * of a group there must not be at any depth a particle whose {term}
        * is the group itself.
        */
        if ((item == null) ||
            (item.type != XML_XSD10_SCHEMA_TYPE_GROUP) ||
            (item.children == null))
        {
            return;
        }
        {
            Xsd10TreeItem circ;

            circ = getCircModelGrDefRef(item, (Xsd10TreeItem) ((Xsd10TreeItem) item.children).children);
            if (circ != null)
            {
                String[] str = new String[1];
                /*
                * TODO: The error report is not adequate: this constraint
                * is defined for model groups but not definitions, but since
                * there cannot be any circular model groups without a model group
                * definition (if not using a construction API), we check those
                * definitions only.
                */
                pCustomErr(ctxt,
                    XML_XSD10_SCHEMAP_MG_PROPS_CORRECT_2,
                    null, wxsItemNode(circ),
                    "Circular reference to the model group definition '%s' "
                    + "defined", formatQName(str,
                        item.target_namespace, item.name));
                /*
                * NOTE: We will cut the reference to avoid further
                * confusion of the processor. This is a fatal error.
                */
                circ.children = null;
            }
        }
    }

    // xml_xsd10_model_group_to_model_group_def_fixup
    /**
     * Port of xmlSchemaModelGroupToModelGroupDefFixup. Assigns the model group of model
     * group definitions to the "term" of the referencing particle.
     * @param ctxt the parser context
     * @param mg the model group
     */
    static void modelGroupToModelGroupDefFixup(Xsd10ParserCtxt ctxt, Xsd10ModelGroup mg)
    {
        Xsd10Particle particle = wxsModelgroupParticle(mg);

        while (particle != null)
        {
            if ((wxsParticleTerm(particle) == null) ||
                ((wxsParticleTerm(particle)).type !=
                    XML_XSD10_SCHEMA_TYPE_GROUP))
            {
                particle = (Xsd10Particle) particle.next;
                continue;
            }
            if (wxsModelgroupdefModel(wxsParticleTerm(particle)) == null)
            {
                /*
                * TODO: Remove the particle.
                */
                particle.children = null;
                particle = (Xsd10Particle) particle.next;
                continue;
            }
            /*
            * Assign the model group to the {term} of the particle.
            */
            particle.children =
                wxsModelgroupdefModel(wxsParticleTerm(particle));

            particle = (Xsd10Particle) particle.next;
        }
    }

    // xml_xsd10_check_attr_group_circular_recur
    /**
     * Port of xmlSchemaCheckAttrGroupCircularRecur.
     * @param ctxt_gr the searched attribute group
     * @param list the current attribute list to be processed
     * @return the circular attribute group reference, otherwise null.
     */
    static Xsd10QNameRef checkAttrGroupCircularRecur(Xsd10AttributeGroup ctxt_gr, Xsd10ItemList list)
    {
        Xsd10AttributeGroup gr;
        Xsd10QNameRef ref, circ;
        int i;
        /*
        * We will search for an attribute group reference which
        * references the context attribute group.
        */
        for (i = 0; i < list.nb_items; i++)
        {
            if (!(list.items[i] instanceof Xsd10QNameRef))
            {
                continue;
            }
            ref = (Xsd10QNameRef) list.items[i];
            if ((ref.type == XML_XSD10_SCHEMA_EXTRA_QNAMEREF) &&
                (ref.item_type == XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP) &&
                (ref.item != null))
            {
                gr = (Xsd10AttributeGroup) ref.item;
                if (gr == ctxt_gr)
                {
                    return ref;
                }
                if ((gr.flags & XML_XSD10_SCHEMAS_ATTRGROUP_MARKED) != 0)
                {
                    continue;
                }
                /*
                * Mark as visited to avoid infinite recursion on
                * circular references not yet examined.
                */
                if ((gr.attr_uses != null) &&
                    ((gr.flags & XML_XSD10_SCHEMAS_ATTRGROUP_HAS_REFS) != 0))
                {
                    gr.flags |= XML_XSD10_SCHEMAS_ATTRGROUP_MARKED;
                    circ = checkAttrGroupCircularRecur(ctxt_gr,
                        (Xsd10ItemList) gr.attr_uses);
                    gr.flags ^= XML_XSD10_SCHEMAS_ATTRGROUP_MARKED;
                    if (circ != null)
                    {
                        return circ;
                    }
                }

            }
        }
        return null;
    }

    // xml_xsd10_check_attr_group_circular
    /**
     * Port of xmlSchemaCheckAttrGroupCircular. Checks for circular references of attribute groups.
     * @param attr_gr the attribute group definition
     * @param ctxt the parser context
     * @return 0 or the error code
     */
    static int checkAttrGroupCircular(Xsd10AttributeGroup attr_gr, Xsd10ParserCtxt ctxt)
    {
        /*
        * Schema Representation Constraint:
        * Attribute Group Definition Representation OK
        * 3 Circular group reference is disallowed outside <redefine>.
        */
        if (attr_gr.attr_uses == null)
        {
            return 0;
        }
        else if ((attr_gr.flags & XML_XSD10_SCHEMAS_ATTRGROUP_HAS_REFS) == 0)
        {
            return 0;
        }
        else
        {
            Xsd10QNameRef circ;

            circ = checkAttrGroupCircularRecur(attr_gr,
                (Xsd10ItemList) attr_gr.attr_uses);
            if (circ != null)
            {
                String[] str = new String[1];
                /*
                * TODO: Report the referenced attr group as QName.
                */
                pCustomErr(ctxt,
                    XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_GROUP_3,
                    null, wxsItemNode(circ),
                    "Circular reference to the attribute group '%s' "
                    + "defined", getComponentQName(str, attr_gr));
                /*
                * NOTE: We will cut the reference to avoid further
                * confusion of the processor.
                */
                circ.item = null;
                return ctxt.err;
            }
        }
        return 0;
    }

    // xml_xsd10_expand_attribute_group_refs
    /**
     * Port of xmlSchemaExpandAttributeGroupRefs. Substitutes contained attribute group
     * references for their attribute uses. Wildcards are intersected.
     * @param pctxt the parser context
     * @param item the schema item
     * @param complete_wild the intersected wildcard to be returned
     * @param list the attribute uses
     * @param prohibs the attribute prohibitions
     * @return 0 or -1
     */
    static int expandAttributeGroupRefs(Xsd10ParserCtxt pctxt, Xsd10BasicItem item, Xsd10Wildcard[] complete_wild, Xsd10ItemList list, Xsd10ItemList prohibs)
    {
        Xsd10AttributeGroup gr;
        Xsd10BasicItem use;
        Xsd10ItemList sublist;
        int i, j;
        int created = (complete_wild[0] == null) ? 0 : 1;

        if (prohibs != null)
        {
            prohibs.nb_items = 0;
        }

        for (i = 0; i < list.nb_items; i++)
        {
            use = (Xsd10BasicItem) list.items[i];

            if (use.type == XML_XSD10_SCHEMA_EXTRA_ATTR_USE_PROHIB)
            {
                if (prohibs == null)
                {
                    perrorInt(pctxt, "xmlSchemaExpandAttributeGroupRefs",
                        "unexpected attr prohibition found");
                    return -1;
                }
                /*
                * Remove from attribute uses.
                */
                if (itemListRemove(list, i) == -1)
                {
                    return -1;
                }
                i--;
                /*
                * Note that duplicate prohibitions were already
                * handled at parsing time.
                */
                /*
                * Add to list of prohibitions.
                */
                itemListAddSize(prohibs, 2, use);
                continue;
            }
            if ((use.type == XML_XSD10_SCHEMA_EXTRA_QNAMEREF) &&
                (((Xsd10QNameRef) use).item_type == XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP))
            {
                if (((Xsd10QNameRef) use).item == null)
                {
                    return -1;
                }
                gr = (Xsd10AttributeGroup) ((Xsd10QNameRef) use).item;
                /*
                * Expand the referenced attr. group.
                * TODO: remove this, this is done in a previous step, so
                * already done here.
                */
                if ((gr.flags & XML_XSD10_SCHEMAS_ATTRGROUP_WILDCARD_BUILDED) == 0)
                {
                    if (attributeGroupExpandRefs(pctxt, gr) == -1)
                    {
                        return -1;
                    }
                }
                /*
                * Build the 'complete' wildcard; i.e. intersect multiple
                * wildcards.
                */
                if (gr.attribute_wildcard != null)
                {
                    if (complete_wild[0] == null)
                    {
                        complete_wild[0] = gr.attribute_wildcard;
                    }
                    else
                    {
                        if (created == 0)
                        {
                            Xsd10Wildcard tmp_wild;

                            /*
                            * Copy the first encountered wildcard as context,
                            * except for the annotation.
                            *
                            * Although the complete wildcard might not correspond
                            * to any node in the schema, we will anchor it on
                            * the node of the owner component.
                            */
                            tmp_wild = addWildcard(pctxt, pctxt.schema,
                                XML_XSD10_SCHEMA_TYPE_ANY_ATTRIBUTE,
                                wxsItemNode(item));
                            if (tmp_wild == null)
                            {
                                return -1;
                            }
                            if (cloneWildcardNsConstraints(pctxt,
                                    tmp_wild, complete_wild[0]) == -1)
                            {
                                return -1;
                            }
                            tmp_wild.process_contents = (complete_wild[0]).process_contents;
                            complete_wild[0] = tmp_wild;
                            created = 1;
                        }

                        if (intersectWildcards(pctxt, complete_wild[0],
                                gr.attribute_wildcard) == -1)
                        {
                            return -1;
                        }
                    }
                }
                /*
                * Just remove the reference if the referenced group does not
                * contain any attribute uses.
                */
                sublist = ((Xsd10ItemList) gr.attr_uses);
                if ((sublist == null) || (sublist.nb_items == 0))
                {
                    if (itemListRemove(list, i) == -1)
                    {
                        return -1;
                    }
                    i--;
                    continue;
                }
                /*
                * Add the attribute uses.
                */
                list.items[i] = sublist.items[0];
                if (sublist.nb_items != 1)
                {
                    for (j = 1; j < sublist.nb_items; j++)
                    {
                        i++;
                        if (itemListInsert(list,
                                sublist.items[j], i) == -1)
                        {
                            return -1;
                        }
                    }
                }
            }

        }
        /*
        * Handle pointless prohibitions of declared attributes.
        */
        if ((prohibs != null) && (prohibs.nb_items != 0) && (list.nb_items != 0))
        {
            Xsd10AttributeUseProhib prohib;

            for (i = prohibs.nb_items - 1; i >= 0; i--)
            {
                prohib = (Xsd10AttributeUseProhib) prohibs.items[i];
                for (j = 0; j < list.nb_items; j++)
                {
                    use = (Xsd10BasicItem) list.items[j];

                    if (Objects.equals(prohib.name, wxsAttruseDeclName(use)) &&
                        Objects.equals(prohib.target_namespace, wxsAttruseDeclTns(use)))
                    {
                        String[] str = new String[1];

                        customWarning(pctxt,
                            XML_XSD10_SCHEMAP_WARN_ATTR_POINTLESS_PROH,
                            prohib.node, null,
                            "Skipping pointless attribute use prohibition "
                            + "'%s', since a corresponding attribute use "
                            + "exists already in the type definition",
                            formatQName(str,
                                prohib.target_namespace, prohib.name),
                            null, null);
                        /*
                        * Remove the prohibition.
                        */
                        if (itemListRemove(prohibs, i) == -1)
                        {
                            return -1;
                        }
                        break;
                    }
                }
            }
        }
        return 0;
    }

    // xml_xsd10_attribute_group_expand_refs
    /**
     * Port of xmlSchemaAttributeGroupExpandRefs. Computation of {attribute uses} and
     * {attribute wildcard} properties.
     * @param pctxt the parser context
     * @param attr_gr the attribute group definition
     * @return 0 or -1
     */
    static int attributeGroupExpandRefs(Xsd10ParserCtxt pctxt, Xsd10AttributeGroup attr_gr)
    {
        if ((attr_gr.attr_uses == null) ||
            ((attr_gr.flags & XML_XSD10_SCHEMAS_ATTRGROUP_WILDCARD_BUILDED) != 0))
        {
            return 0;
        }

        attr_gr.flags |= XML_XSD10_SCHEMAS_ATTRGROUP_WILDCARD_BUILDED;
        Xsd10Wildcard[] wc = new Xsd10Wildcard[] { attr_gr.attribute_wildcard };
        int r = expandAttributeGroupRefs(pctxt, attr_gr,
            wc, (Xsd10ItemList) attr_gr.attr_uses, null);
        attr_gr.attribute_wildcard = wc[0];
        if (r == -1)
        {
            return -1;
        }
        return 0;
    }

    // xml_xsd10_check_ag_props_correct
    /**
     * Port of xmlSchemaCheckAGPropsCorrect. Schema Component Constraint: Attribute Group
     * Definition Properties Correct (ag-props-correct)
     * @param pctxt the parser context
     * @param attr_gr the attribute group definition
     * @return 0 or -1
     */
    static int checkAgPropsCorrect(Xsd10ParserCtxt pctxt, Xsd10AttributeGroup attr_gr)
    {
        /*
        * SPEC ag-props-correct
        * (1) "The values of the properties of an attribute group definition
        * must be as described in the property tableau ..."
        */

        if ((attr_gr.attr_uses != null) &&
            (((Xsd10ItemList) attr_gr.attr_uses).nb_items > 1))
        {
            Xsd10ItemList uses = (Xsd10ItemList) attr_gr.attr_uses;
            Xsd10AttributeUse use, tmp;
            int i, j, has_id = 0;

            next_use:
            for (i = uses.nb_items - 1; i >= 0; i--)
            {
                use = (Xsd10AttributeUse) uses.items[i];
                /*
                * SPEC ag-props-correct
                * (2) "Two distinct members of the {attribute uses} must not have
                * {attribute declaration}s both of whose {name}s match and whose
                * {target namespace}s are identical."
                */
                if (i > 0)
                {
                    for (j = i - 1; j >= 0; j--)
                    {
                        tmp = (Xsd10AttributeUse) uses.items[j];
                        if (Objects.equals(wxsAttruseDeclName(use),
                                wxsAttruseDeclName(tmp)) &&
                            Objects.equals(wxsAttruseDeclTns(use),
                                wxsAttruseDeclTns(tmp)))
                        {
                            String[] str = new String[1];

                            customErr(pctxt,
                                XML_XSD10_SCHEMAP_AG_PROPS_CORRECT,
                                attr_gr.node, attr_gr,
                                "Duplicate %s",
                                getComponentDesignation(str, use),
                                null);
                            /*
                            * Remove the duplicate.
                            */
                            if (itemListRemove(uses, i) == -1)
                            {
                                return -1;
                            }
                            continue next_use;
                        }
                    }
                }
                /*
                * SPEC ag-props-correct
                * (3) "Two distinct members of the {attribute uses} must not have
                * {attribute declaration}s both of whose {type definition}s are or
                * are derived from ID."
                */
                if (wxsAttruseTypedef(use) != null)
                {
                    if (isDerivedFromBuiltInType(
                            wxsAttruseTypedef(use), XML_XSD10_SCHEMAS_ID) != 0)
                    {
                        if (has_id != 0)
                        {
                            String[] str = new String[1];

                            customErr(pctxt,
                                XML_XSD10_SCHEMAP_AG_PROPS_CORRECT,
                                attr_gr.node, attr_gr,
                                "There must not exist more than one attribute "
                                + "declaration of type 'xs:ID' "
                                + "(or derived from 'xs:ID'). The %s violates this "
                                + "constraint",
                                getComponentDesignation(str, use),
                                null);
                            if (itemListRemove(uses, i) == -1)
                            {
                                return -1;
                            }
                        }
                        has_id = 1;
                    }
                }
            }
        }
        return 0;
    }

    // xml_xsd10_resolve_attr_group_references
    /**
     * Port of xmlSchemaResolveAttrGroupReferences. Resolves references to attribute group definitions.
     * @param ref the schema attribute definition
     * @param ctxt the schema parser context
     * @return 0 or the error code
     */
    static int resolveAttrGroupReferences(Xsd10QNameRef ref, Xsd10ParserCtxt ctxt)
    {
        Xsd10AttributeGroup group;

        if (ref.item != null)
        {
            return 0;
        }
        group = getAttributeGroup(ctxt.schema,
            ref.name,
            ref.target_namespace);
        if (group == null)
        {
            pResCompAttrErr(ctxt,
                XML_XSD10_SCHEMAP_SRC_RESOLVE,
                null, ref.node,
                "ref", ref.name, ref.target_namespace,
                ref.item_type, null);
            return ctxt.err;
        }
        ref.item = group;
        return 0;
    }

    // xml_xsd10_check_attr_props_correct
    /**
     * Port of xmlSchemaCheckAttrPropsCorrect. Schema Component Constraint: Attribute
     * Declaration Properties Correct (a-props-correct)
     * @param pctxt a schema parser context
     * @param attr an schema attribute declaration/use
     * @return 0, the error code or -1
     */
    static int checkAttrPropsCorrect(Xsd10ParserCtxt pctxt, Xsd10Attribute attr)
    {

        /*
        * SPEC a-props-correct (1)
        */

        if (wxsAttrTypedef(attr) == null)
        {
            return 0;
        }

        if (attr.def_value != null)
        {
            int ret;

            /*
            * SPEC a-props-correct (3)
            * "If the {type definition} is or is derived from ID then there
            * must not be a {value constraint}."
            */
            if (isDerivedFromBuiltInType(
                    wxsAttrTypedef(attr), XML_XSD10_SCHEMAS_ID) != 0)
            {
                customErr(pctxt,
                    XML_XSD10_SCHEMAP_A_PROPS_CORRECT_3,
                    null, attr,
                    "Value constraints are not allowed if the type definition "
                    + "is or is derived from xs:ID",
                    null, null);
                return pctxt.err;
            }
            /*
            * SPEC a-props-correct (2)
            * "if there is a {value constraint}, the canonical lexical
            * representation of its value must be `valid` with respect
            * to the {type definition} as defined in String Valid ($3.14.4)."
            */
            Xsd10Val[] dv = new Xsd10Val[] { attr.def_val };
            ret = vCheckCvcSimpleType(pctxt,
                attr.node, wxsAttrTypedef(attr),
                attr.def_value, dv,
                1, 1, 0);
            attr.def_val = dv[0];
            if (ret != 0)
            {
                if (ret < 0)
                {
                    perrorInt(pctxt, "xmlSchemaCheckAttrPropsCorrect",
                        "calling xmlSchemaVCheckCVCSimpleType()");
                    return -1;
                }
                customErr(pctxt,
                    XML_XSD10_SCHEMAP_A_PROPS_CORRECT_2,
                    null, attr,
                    "The value of the value constraint is not valid",
                    null, null);
                return pctxt.err;
            }
        }

        return 0;
    }

    // xml_xsd10_check_subst_group_circular
    /** Port of xmlSchemaCheckSubstGroupCircular. */
    static Xsd10Element checkSubstGroupCircular(Xsd10Element elem_decl, Xsd10Element ancestor)
    {
        Xsd10Element ret;

        if (wxsSubstHead(ancestor) == null)
        {
            return null;
        }
        if (wxsSubstHead(ancestor) == elem_decl)
        {
            return ancestor;
        }

        if ((wxsSubstHead(ancestor).flags & XML_XSD10_SCHEMAS_ELEM_CIRCULAR) != 0)
        {
            return null;
        }
        wxsSubstHead(ancestor).flags |= XML_XSD10_SCHEMAS_ELEM_CIRCULAR;
        ret = checkSubstGroupCircular(elem_decl,
            wxsSubstHead(ancestor));
        wxsSubstHead(ancestor).flags ^= XML_XSD10_SCHEMAS_ELEM_CIRCULAR;

        return ret;
    }

    // xml_xsd10_check_elem_props_correct
    /**
     * Port of xmlSchemaCheckElemPropsCorrect. Schema Component Constraint: Element
     * Declaration Properties Correct (e-props-correct)
     * @param pctxt a schema parser context
     * @param elem_decl the element declaration
     * @return 0, an error code or -1
     */
    static int checkElemPropsCorrect(Xsd10ParserCtxt pctxt, Xsd10Element elem_decl)
    {
        int ret = 0;
        Xsd10Type type_def = wxsElemTypedef(elem_decl);
        /*
        * SPEC (1) "The values of the properties of an element declaration
        * must be as described in the property tableau in The Element
        * Declaration Schema Component ($3.3.1), modulo the impact of Missing
        * Sub-components ($5.3)."
        */
        if (wxsSubstHead(elem_decl) != null)
        {
            Xsd10Element head = wxsSubstHead(elem_decl), circ;

            checkElementDeclComponent(head, pctxt);
            /*
            * SPEC (3) "If there is a non-`absent` {substitution group
            * affiliation}, then {scope} must be global."
            */
            if ((elem_decl.flags & XML_XSD10_SCHEMAS_ELEM_GLOBAL) == 0)
            {
                pCustomErr(pctxt,
                    XML_XSD10_SCHEMAP_E_PROPS_CORRECT_3,
                    elem_decl, null,
                    "Only global element declarations can have a "
                    + "substitution group affiliation", null);
                ret = XML_XSD10_SCHEMAP_E_PROPS_CORRECT_3;
            }
            /*
            * TODO: SPEC (6) "Circular substitution groups are disallowed."
            */
            if (head == elem_decl)
            {
                circ = head;
            }
            else if (wxsSubstHead(head) != null)
            {
                circ = checkSubstGroupCircular(head, head);
            }
            else
            {
                circ = null;
            }
            if (circ != null)
            {
                String[] str_a = new String[1], str_b = new String[1];

                pCustomErrExt(pctxt,
                    XML_XSD10_SCHEMAP_E_PROPS_CORRECT_6,
                    circ, null,
                    "The element declaration '%s' defines a circular "
                    + "substitution group to element declaration '%s'",
                    getComponentQName(str_a, circ),
                    getComponentQName(str_b, head),
                    null);
                ret = XML_XSD10_SCHEMAP_E_PROPS_CORRECT_6;
            }
            /*
            * SPEC (4) "If there is a {substitution group affiliation},
            * the {type definition} of the element declaration must be validly
            * derived from the {type definition} of the {substitution group
            * affiliation}, given the value of the {substitution group
            * exclusions} of the {substitution group affiliation}..."
            *
            * NOTE: {substitution group exclusions} means the values of the
            * attribute "final".
            */

            if (type_def != wxsElemTypedef(wxsSubstHead(elem_decl)))
            {
                int set = 0;

                if ((head.flags & XML_XSD10_SCHEMAS_ELEM_FINAL_EXTENSION) != 0)
                {
                    set |= XSD10_SUBSET_EXTENSION;
                }
                if ((head.flags & XML_XSD10_SCHEMAS_ELEM_FINAL_RESTRICTION) != 0)
                {
                    set |= XSD10_SUBSET_RESTRICTION;
                }

                if (checkCosDerivedOk(pctxt, type_def,
                        wxsElemTypedef(head), set) != 0)
                {
                    String[] str_a = new String[1], str_b = new String[1], str_c = new String[1];

                    ret = XML_XSD10_SCHEMAP_E_PROPS_CORRECT_4;
                    pCustomErrExt(pctxt,
                        XML_XSD10_SCHEMAP_E_PROPS_CORRECT_4,
                        elem_decl, null,
                        "The type definition '%s' was "
                        + "either rejected by the substitution group "
                        + "affiliation '%s', or not validly derived from its type "
                        + "definition '%s'",
                        getComponentQName(str_a, type_def),
                        getComponentQName(str_b, head),
                        getComponentQName(str_c, wxsElemTypedef(head)));
                }
            }
        }
        /*
        * SPEC (5) "If the {type definition} or {type definition}'s
        * {content type} is or is derived from ID then there must not be a
        * {value constraint}."
        */
        if ((elem_decl.value != null) &&
            (((wxsIsSimple(type_def) != 0) &&
              (isDerivedFromBuiltInType(type_def, XML_XSD10_SCHEMAS_ID) != 0)) ||
             ((wxsIsComplex(type_def) != 0) &&
              (wxsHasSimpleContent(type_def) != 0) &&
              (isDerivedFromBuiltInType(type_def.content_type_def,
                  XML_XSD10_SCHEMAS_ID) != 0))))
        {

            ret = XML_XSD10_SCHEMAP_E_PROPS_CORRECT_5;
            pCustomErr(pctxt,
                XML_XSD10_SCHEMAP_E_PROPS_CORRECT_5,
                elem_decl, null,
                "The type definition (or type definition's content type) is or "
                + "is derived from ID; value constraints are not allowed in "
                + "conjunction with such a type definition", null);
        }
        else if (elem_decl.value != null)
        {
            int vcret;
            XmlNode node = null;

            /*
            * SPEC (2) "If there is a {value constraint}, the canonical lexical
            * representation of its value must be `valid` with respect to the
            * {type definition} as defined in Element Default Valid (Immediate)
            * ($3.3.6)."
            */
            if (type_def == null)
            {
                pErr(pctxt, elem_decl.node,
                    XML_XSD10_SCHEMAP_INTERNAL,
                    "Internal error: xmlSchemaCheckElemPropsCorrect, "
                    + "type is missing... skipping validation of "
                    + "the value constraint", null, null);
                return -1;
            }
            if (elem_decl.node != null)
            {
                if ((elem_decl.flags & XML_XSD10_SCHEMAS_ELEM_FIXED) != 0)
                {
                    node = treeHasProp(elem_decl.node,
                        "fixed");
                }
                else
                {
                    node = treeHasProp(elem_decl.node,
                        "default");
                }
            }
            Xsd10Val[] dv = new Xsd10Val[] { elem_decl.def_val };
            vcret = parseCheckCosValidDefault(pctxt, node,
                type_def, elem_decl.value, dv);
            elem_decl.def_val = dv[0];
            if (vcret != 0)
            {
                if (vcret < 0)
                {
                    perrorInt(pctxt, "xmlSchemaElemCheckValConstr",
                        "failed to validate the value constraint of an "
                        + "element declaration");
                    return -1;
                }
                return vcret;
            }
        }

        return ret;
    }

    // xml_xsd10_check_elem_subst_group
    /**
     * Port of xmlSchemaCheckElemSubstGroup. Schema Component Constraint: Substitution
     * Group (cos-equiv-class)
     * @param ctxt a schema parser context
     * @param elem_decl the element declaration
     */
    static void checkElemSubstGroup(Xsd10ParserCtxt ctxt, Xsd10Element elem_decl)
    {
        if ((wxsSubstHead(elem_decl) == null) ||
            /* SPEC (1) "Its {abstract} is false." */
            ((elem_decl.flags & XML_XSD10_SCHEMAS_ELEM_ABSTRACT) != 0))
        {
            return;
        }
        {
            Xsd10Element head;
            Xsd10Type head_type, type;
            int set, meth_set;
            /*
            * SPEC (2) "It is validly substitutable for HEAD subject to HEAD's
            * {disallowed substitutions} as the blocking constraint, as defined in
            * Substitution Group OK (Transitive) ($3.3.6)."
            */
            for (head = wxsSubstHead(elem_decl); head != null;
                 head = wxsSubstHead(head))
            {
                set = 0;
                meth_set = 0;
                /*
                * The blocking constraints.
                */
                if ((head.flags & XML_XSD10_SCHEMAS_ELEM_BLOCK_SUBSTITUTION) != 0)
                {
                    continue;
                }
                head_type = head.subtypes;
                type = elem_decl.subtypes;
                if (head_type != type)
                {
                    if ((head.flags & XML_XSD10_SCHEMAS_ELEM_BLOCK_RESTRICTION) != 0)
                    {
                        set |= XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION;
                    }
                    if ((head.flags & XML_XSD10_SCHEMAS_ELEM_BLOCK_EXTENSION) != 0)
                    {
                        set |= XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION;
                    }
                    /*
                    * SPEC: Substitution Group OK (Transitive) (2.3)
                    */
                    /*
                    * The set of all {derivation method}s involved in the derivation
                    */
                    while ((type != null) && (type != head_type) &&
                           (type != type.base_type))
                    {
                        if ((wxsIsExtension(type) != 0) &&
                            ((meth_set & XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION) == 0))
                        {
                            meth_set |= XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION;
                        }

                        if ((wxsIsRestriction(type) != 0) &&
                            ((meth_set & XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION) == 0))
                        {
                            meth_set |= XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION;
                        }

                        type = type.base_type;
                    }
                    /*
                    * The {prohibited substitutions} of all intermediate types +
                    * the head's type.
                    */
                    type = elem_decl.subtypes.base_type;
                    while (type != null)
                    {
                        if (wxsIsComplex(type) != 0)
                        {
                            if (((type.flags &
                                  XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION) != 0) &&
                                ((set & XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION) == 0))
                            {
                                set |= XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION;
                            }
                            if (((type.flags &
                                  XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION) != 0) &&
                                ((set & XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION) == 0))
                            {
                                set |= XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION;
                            }
                        }
                        else
                        {
                            break;
                        }
                        if (type == head_type)
                        {
                            break;
                        }
                        type = type.base_type;
                    }
                    if ((set != 0) &&
                        ((((set & XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION) != 0) &&
                          ((meth_set & XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION) != 0)) ||
                         (((set & XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION) != 0) &&
                          ((meth_set & XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION) != 0))))
                    {
                        continue;
                    }
                }
                /* add_member: */
                addElementSubstitutionMember(ctxt, head, elem_decl);
                if ((head.flags & XML_XSD10_SCHEMAS_ELEM_SUBST_GROUP_HEAD) == 0)
                {
                    head.flags |= XML_XSD10_SCHEMAS_ELEM_SUBST_GROUP_HEAD;
                }
            }
        }
    }

    // xml_xsd10_check_element_decl_component
    /**
     * Port of xmlSchemaCheckElementDeclComponent. Validates the value constraints of an
     * element declaration. Adds substitution group members.
     * @param elem_decl an schema element declaration/particle
     * @param ctxt a schema parser context
     */
    static void checkElementDeclComponent(Xsd10Element elem_decl, Xsd10ParserCtxt ctxt)
    {
        if (elem_decl == null)
        {
            return;
        }
        if ((elem_decl.flags & XML_XSD10_SCHEMAS_ELEM_INTERNAL_CHECKED) != 0)
        {
            return;
        }
        elem_decl.flags |= XML_XSD10_SCHEMAS_ELEM_INTERNAL_CHECKED;
        if (checkElemPropsCorrect(ctxt, elem_decl) == 0)
        {
            /*
            * Adds substitution group members.
            */
            checkElemSubstGroup(ctxt, elem_decl);
        }
    }

    // xml_xsd10_resolve_model_group_particle_references
    /**
     * Port of xmlSchemaResolveModelGroupParticleReferences. Resolves references of a model
     * group's {particles} to model group definitions and to element declarations.
     * @param ctxt a parser context
     * @param mg a model group
     */
    static void resolveModelGroupParticleReferences(Xsd10ParserCtxt ctxt, Xsd10ModelGroup mg)
    {
        Xsd10Particle particle = wxsModelgroupParticle(mg);
        Xsd10QNameRef ref;
        Xsd10BasicItem ref_item;

        /*
        * URGENT TODO: Test this.
        */
        while (particle != null)
        {
            if ((wxsParticleTerm(particle) == null) ||
                ((wxsParticleTerm(particle)).type !=
                    XML_XSD10_SCHEMA_EXTRA_QNAMEREF))
            {
                particle = (Xsd10Particle) particle.next;
                continue;
            }
            ref = (Xsd10QNameRef) wxsParticleTerm(particle);
            /*
            * Resolve the reference.
            * NULL the {term} by default.
            */
            particle.children = null;

            ref_item = getNamedComponent(ctxt.schema,
                ref.item_type, ref.name, ref.target_namespace);
            if (ref_item == null)
            {
                pResCompAttrErr(ctxt, XML_XSD10_SCHEMAP_SRC_RESOLVE,
                    null, wxsItemNode(particle), "ref", ref.name,
                    ref.target_namespace, ref.item_type, null);
                /* TODO: remove the particle. */
                particle = (Xsd10Particle) particle.next;
                continue;
            }
            if (ref_item.type == XML_XSD10_SCHEMA_TYPE_GROUP)
            {
                if (wxsModelgroupdefModel(ref_item) == null)
                {
                    /* TODO: remove the particle. */
                    particle = (Xsd10Particle) particle.next;
                    continue;
                }
                /*
                * NOTE that we will assign the model group definition
                * itself to the "term" of the particle.
                */
                if ((wxsModelgroupdefModel(ref_item)).type ==
                    XML_XSD10_SCHEMA_TYPE_ALL)
                {
                    /*
                    * SPEC cos-all-limited (1)
                    * SPEC cos-all-limited (1.2)
                    */
                    customErr(ctxt,
                        /* TODO: error code */
                        XML_XSD10_SCHEMAP_COS_ALL_LIMITED,
                        wxsItemNode(particle), null,
                        "A model group definition is referenced, but "
                        + "it contains an 'all' model group, which "
                        + "cannot be contained by model groups",
                        null, null);
                    /* TODO: remove the particle. */
                    particle = (Xsd10Particle) particle.next;
                    continue;
                }
                particle.children = ref_item;
            }
            else
            {
                /*
                * TODO: Are referenced element declarations the only
                * other components we expect here?
                */
                particle.children = ref_item;
            }
            /* next_particle: */
            particle = (Xsd10Particle) particle.next;
        }
    }

    // xml_xsd10_are_values_equal
    /** Port of xmlSchemaAreValuesEqual. */
    static int areValuesEqual(Xsd10Val x, Xsd10Val y)
    {
        Xsd10Type tx, ty, ptx, pty;
        int ret;

        while (x != null)
        {
            /* Same types. */
            tx = getBuiltInType(getValType(x));
            ty = getBuiltInType(getValType(y));
            ptx = getPrimitiveType(tx);
            pty = getPrimitiveType(ty);
            /*
            * (1) if a datatype T' is `derived` by `restriction` from an
            * atomic datatype T then the `value space` of T' is a subset of
            * the `value space` of T. */
            /*
            * (2) if datatypes T' and T'' are `derived` by `restriction`
            * from a common atomic ancestor T then the `value space`s of T'
            * and T'' may overlap.
            */
            if (ptx != pty)
            {
                return 0;
            }
            if (ptx == null)
            {
                /* C would dereference NULL here. */
                return 0;
            }
            /*
            * We assume computed values to be normalized, so do a fast
            * string comparison for string based types.
            */
            if ((ptx.built_in_type == XML_XSD10_SCHEMAS_STRING) ||
                (wxsIsAnySimpleType(ptx) != 0))
            {
                if (strEqual(
                        valueGetAsString(x),
                        valueGetAsString(y)) == 0)
                {
                    return 0;
                }
            }
            else
            {
                ret = compareValuesWhtsp(
                    x, XML_XSD10_SCHEMA_WHITESPACE_PRESERVE,
                    y, XML_XSD10_SCHEMA_WHITESPACE_PRESERVE);
                if (ret == -2)
                {
                    return -1;
                }
                if (ret != 0)
                {
                    return 0;
                }
            }
            /*
            * Lists.
            */
            x = valueGetNext(x);
            if (x != null)
            {
                y = valueGetNext(y);
                if (y == null)
                {
                    return 0;
                }
            }
            else if (valueGetNext(y) != null)
            {
                return 0;
            }
            else
            {
                return 1;
            }
        }
        return 0;
    }

    // xml_xsd10_resolve_attr_use_references
    /**
     * Port of xmlSchemaResolveAttrUseReferences. Resolves the referenced attribute declaration.
     * @param ause an attribute use
     * @param ctxt a parser context
     * @return 0, an error code or -1
     */
    static int resolveAttrUseReferences(Xsd10AttributeUse ause, Xsd10ParserCtxt ctxt)
    {
        if ((ctxt == null) || (ause == null))
        {
            return -1;
        }
        if ((ause.attr_decl == null) ||
            (ause.attr_decl.type != XML_XSD10_SCHEMA_EXTRA_QNAMEREF))
        {
            return 0;
        }

        {
            Xsd10QNameRef ref = (Xsd10QNameRef) ause.attr_decl;

            /*
            * TODO: Evaluate, what errors could occur if the declaration is not
            * found.
            */
            ause.attr_decl = getAttributeDecl(ctxt.schema,
                ref.name, ref.target_namespace);
            if (ause.attr_decl == null)
            {
                pResCompAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_SRC_RESOLVE,
                    ause, ause.node,
                    "ref", ref.name, ref.target_namespace,
                    XML_XSD10_SCHEMA_TYPE_ATTRIBUTE, null);
                return ctxt.err;
            }
        }
        return 0;
    }

    // xml_xsd10_check_attr_use_props_correct
    /**
     * Port of xmlSchemaCheckAttrUsePropsCorrect. Schema Component Constraint: Attribute
     * Use Correct (au-props-correct)
     * @param ctxt a parser context
     * @param use an attribute use
     * @return 0, an error code or -1
     */
    static int checkAttrUsePropsCorrect(Xsd10ParserCtxt ctxt, Xsd10AttributeUse use)
    {
        if ((ctxt == null) || (use == null))
        {
            return -1;
        }
        if ((use.def_value == null) || (wxsAttruseDecl(use) == null) ||
            ((wxsAttruseDecl(use)).type != XML_XSD10_SCHEMA_TYPE_ATTRIBUTE))
        {
            return 0;
        }

        /*
        * SPEC au-props-correct (1)
        */

        if (((wxsAttruseDecl(use)).def_value != null) &&
            (((wxsAttruseDecl(use)).flags & XML_XSD10_SCHEMAS_ATTR_FIXED) != 0) &&
            ((use.flags & XML_XSD10_SCHEMA_ATTR_USE_FIXED) == 0))
        {
            pCustomErr(ctxt,
                XML_XSD10_SCHEMAP_AU_PROPS_CORRECT_2,
                use, null,
                "The attribute declaration has a 'fixed' value constraint "
                + ", thus the attribute use must also have a 'fixed' value "
                + "constraint",
                null);
            return ctxt.err;
        }
        /*
        * Compute and check the value constraint's value.
        */
        if ((use.def_val != null) && (wxsAttruseTypedef(use) != null))
        {
            int ret;
            /*
            * TODO: The spec seems to be missing a check of the
            * value constraint of the attribute use. We will do it here.
            */
            /*
            * SPEC a-props-correct (3)
            */
            if (isDerivedFromBuiltInType(
                    wxsAttruseTypedef(use), XML_XSD10_SCHEMAS_ID) != 0)
            {
                customErr(ctxt,
                    XML_XSD10_SCHEMAP_AU_PROPS_CORRECT,
                    null, use,
                    "Value constraints are not allowed if the type definition "
                    + "is or is derived from xs:ID",
                    null, null);
                return ctxt.err;
            }

            Xsd10Val[] dv = new Xsd10Val[] { use.def_val };
            ret = vCheckCvcSimpleType(ctxt,
                use.node, wxsAttruseTypedef(use),
                use.def_value, dv,
                1, 1, 0);
            use.def_val = dv[0];
            if (ret != 0)
            {
                if (ret < 0)
                {
                    perrorInt2(ctxt, "xmlSchemaCheckAttrUsePropsCorrect",
                        "calling xmlSchemaVCheckCVCSimpleType()");
                    return -1;
                }
                customErr(ctxt,
                    XML_XSD10_SCHEMAP_AU_PROPS_CORRECT,
                    null, use,
                    "The value of the value constraint is not valid",
                    null, null);
                return ctxt.err;
            }
        }
        /*
        * SPEC au-props-correct (2)
        * "If the {attribute declaration} has a fixed
        * {value constraint}, then if the attribute use itself has a
        * {value constraint}, it must also be fixed and its value must match
        * that of the {attribute declaration}'s {value constraint}."
        */
        if (((wxsAttruseDecl(use)).def_val != null) &&
            (((wxsAttruseDecl(use)).flags & XML_XSD10_SCHEMA_ATTR_USE_FIXED) == 0))
        {
            if (areValuesEqual(use.def_val,
                    (wxsAttruseDecl(use)).def_val) == 0)
            {
                pCustomErr(ctxt,
                    XML_XSD10_SCHEMAP_AU_PROPS_CORRECT_2,
                    use, null,
                    "The 'fixed' value constraint of the attribute use "
                    + "must match the attribute declaration's value "
                    + "constraint '%s'",
                    (wxsAttruseDecl(use)).def_value);
            }
            return ctxt.err;
        }
        return 0;
    }

    // xml_xsd10_resolve_attr_type_references
    /**
     * Port of xmlSchemaResolveAttrTypeReferences. Resolves the referenced type definition component.
     * @param item an attribute declaration
     * @param ctxt a parser context
     * @return 0 or an error code
     */
    static int resolveAttrTypeReferences(Xsd10Attribute item, Xsd10ParserCtxt ctxt)
    {
        /*
        * The simple type definition corresponding to the <simpleType> element
        * information item in the [children], if present, otherwise the simple
        * type definition `resolved` to by the `actual value` of the type
        * [attribute], if present, otherwise the `simple ur-type definition`.
        */
        if ((item.flags & XML_XSD10_SCHEMAS_ATTR_INTERNAL_RESOLVED) != 0)
        {
            return 0;
        }
        item.flags |= XML_XSD10_SCHEMAS_ATTR_INTERNAL_RESOLVED;
        if (item.subtypes != null)
        {
            return 0;
        }
        if (item.type_name != null)
        {
            Xsd10Type type;

            type = getType(ctxt.schema, item.type_name,
                item.type_ns);
            if ((type == null) || (wxsIsSimple(type) == 0))
            {
                pResCompAttrErr(ctxt,
                    XML_XSD10_SCHEMAP_SRC_RESOLVE,
                    item, item.node,
                    "type", item.type_name, item.type_ns,
                    XML_XSD10_SCHEMA_TYPE_SIMPLE, null);
                return ctxt.err;
            }
            else
            {
                item.subtypes = type;
            }

        }
        else
        {
            /*
            * The type defaults to the xs:anySimpleType.
            */
            item.subtypes = getBuiltInType(XML_XSD10_SCHEMAS_ANYSIMPLETYPE);
        }
        return 0;
    }

    // xml_xsd10_resolve_idc_key_references
    /**
     * Port of xmlSchemaResolveIDCKeyReferences. Resolve keyRef references to key/unique IDCs.
     * @param idc the identity-constraint definition
     * @param pctxt the schema parser context
     * @return 0 or an error code
     */
    static int resolveIdcKeyReferences(Xsd10IDC idc, Xsd10ParserCtxt pctxt)
    {
        if (idc.type != XML_XSD10_SCHEMA_TYPE_IDC_KEYREF)
        {
            return 0;
        }
        if (idc.ref.name != null)
        {
            idc.ref.item =
                getIdc(pctxt.schema, idc.ref.name,
                    idc.ref.target_namespace);
            if (idc.ref.item == null)
            {
                /*
                * TODO: It is actually not an error to fail to resolve
                * at this stage. BUT we need to be that strict!
                */
                pResCompAttrErr(pctxt,
                    XML_XSD10_SCHEMAP_SRC_RESOLVE,
                    idc, idc.node,
                    "refer", idc.ref.name,
                    idc.ref.target_namespace,
                    XML_XSD10_SCHEMA_TYPE_IDC_KEY, null);
                return pctxt.err;
            }
            else if (idc.ref.item.type == XML_XSD10_SCHEMA_TYPE_IDC_KEYREF)
            {
                /*
                * SPEC c-props-correct (1)
                */
                customErr(pctxt,
                    XML_XSD10_SCHEMAP_C_PROPS_CORRECT,
                    null, idc,
                    "The keyref references a keyref",
                    null, null);
                idc.ref.item = null;
                return pctxt.err;
            }
            else
            {
                if (idc.nb_fields !=
                    ((Xsd10IDC) idc.ref.item).nb_fields)
                {
                    String[] str = new String[1];
                    Xsd10IDC refer;

                    refer = (Xsd10IDC) idc.ref.item;
                    /*
                    * SPEC c-props-correct(2)
                    * "If the {identity-constraint category} is keyref,
                    * the cardinality of the {fields} must equal that of
                    * the {fields} of the {referenced key}.
                    */
                    customErr(pctxt,
                        XML_XSD10_SCHEMAP_C_PROPS_CORRECT,
                        null, idc,
                        "The cardinality of the keyref differs from the "
                        + "cardinality of the referenced key/unique '%s'",
                        formatQName(str, refer.target_namespace,
                            refer.name),
                        null);
                    return pctxt.err;
                }
            }
        }
        return 0;
    }

    // xml_xsd10_resolve_attr_use_prohib_references
    /** Port of xmlSchemaResolveAttrUseProhibReferences. */
    static int resolveAttrUseProhibReferences(Xsd10AttributeUseProhib prohib, Xsd10ParserCtxt pctxt)
    {
        if (getAttributeDecl(pctxt.schema, prohib.name,
                prohib.target_namespace) == null)
        {

            pResCompAttrErr(pctxt,
                XML_XSD10_SCHEMAP_SRC_RESOLVE,
                null, prohib.node,
                "ref", prohib.name, prohib.target_namespace,
                XML_XSD10_SCHEMA_TYPE_ATTRIBUTE, null);
            return XML_XSD10_SCHEMAP_SRC_RESOLVE;
        }
        return 0;
    }

    // XSD10_WXS_REDEFINED_TYPE
    private static int wxsRedefinedType(Xsd10BasicItem item)
    {
        return ((Xsd10Type) item).flags & XML_XSD10_SCHEMAS_TYPE_REDEFINED;
    }

    // XSD10_WXS_REDEFINED_MODEL_GROUP_DEF
    private static int wxsRedefinedModelGroupDef(Xsd10BasicItem item)
    {
        return ((Xsd10ModelGroupDef) item).flags & XML_XSD10_SCHEMA_MODEL_GROUP_DEF_REDEFINED;
    }

    // XSD10_WXS_REDEFINED_ATTR_GROUP
    private static int wxsRedefinedAttrGroup(Xsd10BasicItem item)
    {
        return ((Xsd10AttributeGroup) item).flags & XML_XSD10_SCHEMAS_ATTRGROUP_REDEFINED;
    }

    // xml_xsd10_check_src_redefine_first
    /** Port of xmlSchemaCheckSRCRedefineFirst. */
    static int checkSrcRedefineFirst(Xsd10ParserCtxt pctxt)
    {
        int err = 0;
        Xsd10Redef redef = wxsConstructor(pctxt).redefs;
        Xsd10BasicItem prev, item;
        int was_redefined;

        if (redef == null)
        {
            return 0;
        }

        do
        {
            item = redef.item;
            /*
            * First try to locate the redefined component in the
            * schema graph starting with the redefined schema.
            * NOTE: According to this schema bug entry:
            *   http://lists.w3.org/Archives/Public/www-xml-schema-comments/2005OctDec/0019.html
            *   it's not clear if the referenced component needs to originate
            *   from the <redefine>d schema _document_ or the schema; the latter
            *   would include all imported and included sub-schemas of the
            *   <redefine>d schema. Currently the latter approach is used.
            */
            prev = findRedefCompInGraph(
                redef.target_bucket, item.type,
                redef.ref_name, redef.ref_target_ns);
            if (prev == null)
            {
                String[] str = new String[1];
                XmlNode node;

                /*
                * SPEC src-redefine:
                * (6.2.1) "The `actual value` of its own name attribute plus
                * target namespace must successfully `resolve` to a model
                * group definition in I."
                * (7.2.1) "The `actual value` of its own name attribute plus
                * target namespace must successfully `resolve` to an attribute
                * group definition in I."
                */
                if (redef.reference != null)
                {
                    node = wxsItemNode(redef.reference);
                }
                else
                {
                    node = wxsItemNode(item);
                }
                customErr(pctxt,
                    /*
                    * TODO: error code.
                    */
                    XML_XSD10_SCHEMAP_SRC_REDEFINE, node, null,
                    "The %s '%s' to be redefined could not be found in "
                    + "the redefined schema",
                    wxsItemTypeName(item),
                    formatQName(str, redef.ref_target_ns,
                        redef.ref_name));
                err = pctxt.err;
                redef = redef.next;
                continue;
            }
            /*
            * TODO: Obtaining and setting the redefinition state is really
            * clumsy.
            */
            was_redefined = 0;
            switch (item.type)
            {
            case XML_XSD10_SCHEMA_TYPE_COMPLEX:
            case XML_XSD10_SCHEMA_TYPE_SIMPLE:
                if ((((Xsd10Type) prev).flags &
                     XML_XSD10_SCHEMAS_TYPE_REDEFINED) != 0)
                {
                    was_redefined = 1;
                    break;
                }
                /* Mark it as redefined. */
                ((Xsd10Type) prev).flags |= XML_XSD10_SCHEMAS_TYPE_REDEFINED;
                /*
                * Assign the redefined type to the
                * base type of the redefining type.
                * TODO: How
                */
                ((Xsd10Type) item).base_type =
                    (Xsd10Type) prev;
                break;
            case XML_XSD10_SCHEMA_TYPE_GROUP:
                if ((((Xsd10ModelGroupDef) prev).flags &
                     XML_XSD10_SCHEMA_MODEL_GROUP_DEF_REDEFINED) != 0)
                {
                    was_redefined = 1;
                    break;
                }
                /* Mark it as redefined. */
                ((Xsd10ModelGroupDef) prev).flags |=
                    XML_XSD10_SCHEMA_MODEL_GROUP_DEF_REDEFINED;
                if (redef.reference != null)
                {
                    /*
                    * Overwrite the QName-reference with the
                    * referenced model group def.
                    */
                    ((Xsd10TreeItem) redef.reference).children =
                        prev;
                }
                redef.target = prev;
                break;
            case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
                if ((((Xsd10AttributeGroup) prev).flags &
                     XML_XSD10_SCHEMAS_ATTRGROUP_REDEFINED) != 0)
                {
                    was_redefined = 1;
                    break;
                }
                ((Xsd10AttributeGroup) prev).flags |=
                    XML_XSD10_SCHEMAS_ATTRGROUP_REDEFINED;
                if (redef.reference != null)
                {
                    /*
                    * Assign the redefined attribute group to the
                    * QName-reference component.
                    * This is the easy case, since we will just
                    * expand the redefined group.
                    */
                    ((Xsd10QNameRef) redef.reference).item = prev;
                    redef.target = null;
                }
                else
                {
                    /*
                    * This is the complicated case: we need
                    * to apply src-redefine (7.2.2) at a later
                    * stage, i.e. when attribute group references
                    * have been expanded and simple types have
                    * been fixed.
                    */
                    redef.target = prev;
                }
                break;
            default:
                perrorInt(pctxt, "xmlSchemaResolveRedefReferences",
                    "Unexpected redefined component type");
                return -1;
            }
            if (was_redefined != 0)
            {
                String[] str = new String[1];
                XmlNode node;

                if (redef.reference != null)
                {
                    node = wxsItemNode(redef.reference);
                }
                else
                {
                    node = wxsItemNode(redef.item);
                }

                customErr(pctxt,
                    /* TODO: error code. */
                    XML_XSD10_SCHEMAP_SRC_REDEFINE,
                    node, null,
                    "The referenced %s was already redefined. Multiple "
                    + "redefinition of the same component is not supported",
                    getComponentDesignation(str, prev),
                    null);
                err = pctxt.err;
                redef = redef.next;
                continue;
            }
            redef = redef.next;
        } while (redef != null);

        return err;
    }

    // xml_xsd10_check_src_redefine_second
    /** Port of xmlSchemaCheckSRCRedefineSecond. */
    static int checkSrcRedefineSecond(Xsd10ParserCtxt pctxt)
    {
        int err = 0;
        Xsd10Redef redef = wxsConstructor(pctxt).redefs;
        Xsd10BasicItem item;

        if (redef == null)
        {
            return 0;
        }

        do
        {
            if (redef.target == null)
            {
                redef = redef.next;
                continue;
            }
            item = redef.item;

            switch (item.type)
            {
            case XML_XSD10_SCHEMA_TYPE_SIMPLE:
            case XML_XSD10_SCHEMA_TYPE_COMPLEX:
                /*
                * Since the spec wants the {name} of the redefined
                * type to be 'absent', we'll NULL it.
                */
                ((Xsd10Type) redef.target).name = null;

                /*
                * TODO: Seems like there's nothing more to do. The normal
                * inheritance mechanism is used. But not 100% sure.
                */
                break;
            case XML_XSD10_SCHEMA_TYPE_GROUP:
                /*
                * URGENT TODO:
                * SPEC src-redefine: (6.2.2) not checked.
                */
                break;
            case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
                /*
                * SPEC src-redefine:
                * (7.2.2) "The {attribute uses} and {attribute wildcard} of
                * the attribute group definition which corresponds to it
                * per XML Representation of Attribute Group Definition Schema
                * Components ($3.6.2) must be `valid restrictions` of the
                * {attribute uses} and {attribute wildcard} of that attribute
                * group definition in I, as defined in clause 2, clause 3 and
                * clause 4 of Derivation Valid (Restriction, Complex)
                * ($3.4.6) (where references to the base type definition are
                * understood as references to the attribute group definition
                * in I)."
                */
                err = checkDerivationOkRestriction2to4(pctxt,
                    XML_XSD10_SCHEMA_ACTION_REDEFINE,
                    item, redef.target,
                    (Xsd10ItemList) ((Xsd10AttributeGroup) item).attr_uses,
                    (Xsd10ItemList) ((Xsd10AttributeGroup) redef.target).attr_uses,
                    ((Xsd10AttributeGroup) item).attribute_wildcard,
                    ((Xsd10AttributeGroup) redef.target).attribute_wildcard);
                if (err == -1)
                {
                    return -1;
                }
                break;
            default:
                break;
            }
            redef = redef.next;
        } while (redef != null);
        return 0;
    }

    private static final int SLOT_TYPE = 0;
    private static final int SLOT_ELEM = 1;
    private static final int SLOT_ATTR = 2;
    private static final int SLOT_GROUP = 3;
    private static final int SLOT_ATTRGRP = 4;
    private static final int SLOT_IDC = 5;
    private static final int SLOT_NOTA = 6;

    // XSD10_WXS_GET_GLOBAL_HASH (the schema owning the slot)
    private static Xsd10Schema globalHashSchema(Xsd10Bucket c)
    {
        if (wxsIsBucketImpmain(c.type) != 0)
        {
            return wxsImpbucket(c).schema;
        }
        else
        {
            return wxsIncbucket(c).owner_import.schema;
        }
    }

    private static Xsd10Hash getSlot(Xsd10Schema s, int slot)
    {
        switch (slot)
        {
        case SLOT_TYPE: return s.type_decl;
        case SLOT_ELEM: return s.elem_decl;
        case SLOT_ATTR: return s.attr_decl;
        case SLOT_GROUP: return s.group_decl;
        case SLOT_ATTRGRP: return s.attrgrp_decl;
        case SLOT_IDC: return s.idc_def;
        default: return s.nota_decl;
        }
    }

    private static void setSlot(Xsd10Schema s, int slot, Xsd10Hash h)
    {
        switch (slot)
        {
        case SLOT_TYPE: s.type_decl = h; break;
        case SLOT_ELEM: s.elem_decl = h; break;
        case SLOT_ATTR: s.attr_decl = h; break;
        case SLOT_GROUP: s.group_decl = h; break;
        case SLOT_ATTRGRP: s.attrgrp_decl = h; break;
        case SLOT_IDC: s.idc_def = h; break;
        default: s.nota_decl = h; break;
        }
    }

    // xml_xsd10_add_components
    /** Port of xmlSchemaAddComponents. */
    static int addComponents(Xsd10ParserCtxt pctxt, Xsd10Bucket bucket)
    {
        Xsd10BasicItem item;
        int err;
        String name;
        int slot;
        int i;

        /*
        * Add global components to the schema's hash tables.
        * This is the place where duplicate components will be
        * detected.
        */
        if (bucket == null)
        {
            return -1;
        }
        if ((bucket.flags & XML_XSD10_SCHEMA_BUCKET_COMPS_ADDED) != 0)
        {
            return 0;
        }
        bucket.flags |= XML_XSD10_SCHEMA_BUCKET_COMPS_ADDED;

        for (i = 0; i < bucket.globals.nb_items; i++)
        {
            item = (Xsd10BasicItem) bucket.globals.items[i];
            switch (item.type)
            {
            case XML_XSD10_SCHEMA_TYPE_COMPLEX:
            case XML_XSD10_SCHEMA_TYPE_SIMPLE:
                if (wxsRedefinedType(item) != 0)
                {
                    continue;
                }
                name = ((Xsd10Type) item).name;
                slot = SLOT_TYPE;
                break;
            case XML_XSD10_SCHEMA_TYPE_ELEMENT:
                name = ((Xsd10Element) item).name;
                slot = SLOT_ELEM;
                break;
            case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE:
                name = ((Xsd10Attribute) item).name;
                slot = SLOT_ATTR;
                break;
            case XML_XSD10_SCHEMA_TYPE_GROUP:
                if (wxsRedefinedModelGroupDef(item) != 0)
                {
                    continue;
                }
                name = ((Xsd10ModelGroupDef) item).name;
                slot = SLOT_GROUP;
                break;
            case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
                if (wxsRedefinedAttrGroup(item) != 0)
                {
                    continue;
                }
                name = ((Xsd10AttributeGroup) item).name;
                slot = SLOT_ATTRGRP;
                break;
            case XML_XSD10_SCHEMA_TYPE_IDC_KEY:
            case XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE:
            case XML_XSD10_SCHEMA_TYPE_IDC_KEYREF:
                name = ((Xsd10IDC) item).name;
                slot = SLOT_IDC;
                break;
            case XML_XSD10_SCHEMA_TYPE_NOTATION:
                name = ((Xsd10Notation) item).name;
                slot = SLOT_NOTA;
                break;
            default:
                perrorInt(pctxt, "xmlSchemaAddComponents",
                    "Unexpected global component type");
                continue;
            }
            Xsd10Schema owner = globalHashSchema(bucket);
            Xsd10Hash table = getSlot(owner, slot);
            if (table == null)
            {
                table = hashCreateDict(10, pctxt.dict);
                setSlot(owner, slot, table);
                if (table == null)
                {
                    perrorInt(pctxt, "xmlSchemaAddComponents",
                        "failed to create a component hash table");
                    return -1;
                }
            }
            err = hashAddEntry(table, name, item);
            if (err != 0)
            {
                String[] str = new String[1];

                customErr(pctxt,
                    XML_XSD10_SCHEMAP_REDEFINED_TYPE,
                    wxsItemNode(item),
                    item,
                    "A global %s '%s' does already exist",
                    wxsItemTypeName(item),
                    getComponentQName(str, item));
            }
        }
        /*
        * Process imported/included schemas.
        */
        if (bucket.relations != null)
        {
            Xsd10SchemaRelation rel = bucket.relations;
            do
            {
                if ((rel.bucket != null) &&
                    ((rel.bucket.flags & XML_XSD10_SCHEMA_BUCKET_COMPS_ADDED) == 0))
                {
                    if (addComponents(pctxt, rel.bucket) == -1)
                    {
                        return -1;
                    }
                }
                rel = rel.next;
            } while (rel != null);
        }
        return 0;
    }

    private static final int FIX_EXIT = 0;
    private static final int FIX_EXIT_ERROR = 1;
    private static final int FIX_EXIT_FAILURE = 2;

    // XSD10_FIXHFAILURE
    private static boolean fixHFailure(Xsd10ParserCtxt pctxt)
    {
        return pctxt.err == XML_XSD10_SCHEMAP_INTERNAL;
    }

    // xml_xsd10_fixup_components
    /** Port of xmlSchemaFixupComponents. */
    static int fixupComponents(Xsd10ParserCtxt pctxt, Xsd10Bucket root_bucket)
    {
        Xsd10ConstructionCtxt con = pctxt.constructor;
        int ret = 0;
        Xsd10Bucket oldbucket = con.bucket;

        if ((con.pending == null) ||
            (con.pending.nb_items == 0))
        {
            return 0;
        }

        /*
        * Since xmlSchemaFixupComplexType() will create new particles
        * (local components), and those particle components need a bucket
        * on the constructor, we'll assure here that the constructor has
        * a bucket.
        * TODO: Think about storing locals _only_ on the main bucket.
        */
        if (con.bucket == null)
        {
            con.bucket = root_bucket;
        }

        int how = fixupComponentsBody(pctxt, con, root_bucket);
        if (how == FIX_EXIT_ERROR)
        {
            ret = pctxt.err;
        }
        else if (how == FIX_EXIT_FAILURE)
        {
            ret = -1;
        }

        /* exit: */
        /*
        * Reset the constructor. This is needed for XSI acquisition, since
        * those items will be processed over and over again for every XSI
        * if not cleared here.
        */
        con.bucket = oldbucket;
        con.pending.nb_items = 0;
        if (con.subst_groups != null)
        {
            hashFree(con.subst_groups, Xsd10Schemas::substGroupFreeEntry);
            con.subst_groups = null;
        }
        if (con.redefs != null)
        {
            redefListFree(con.redefs);
            con.redefs = null;
        }
        return ret;
    }

    /** The body of xml_xsd10_fixup_components up to the exit labels. */
    private static int fixupComponentsBody(Xsd10ParserCtxt pctxt, Xsd10ConstructionCtxt con, Xsd10Bucket root_bucket)
    {
        Xsd10BasicItem item;
        Object[] items;
        int nb_items, i;
        Xsd10Element elem_decl;

        /* TODO:
        * SPEC (src-redefine):
        * (6.2) "If it has no such self-reference, then all of the
        * following must be true:"
        * (6.2.2) The {model group} of the model group definition which
        * corresponds to it per XML Representation of Model Group
        * Definition Schema Components ($3.7.2) must be a `valid
        * restriction` of the {model group} of that model group definition
        * in I, as defined in Particle Valid (Restriction) ($3.9.6)."
        */
        checkSrcRedefineFirst(pctxt);

        /*
        * Add global components to the schemata's hash tables.
        */
        addComponents(pctxt, root_bucket);

        pctxt.ctxt_type = null;
        items = con.pending.items;
        nb_items = con.pending.nb_items;
        /*
        * Now that we have parsed *all* the schema document(s) and converted
        * them to schema components, we can resolve references, apply component
        * constraints, create the FSA from the content model, etc.
        */
        /*
        * Resolve references of..
        *
        * 1. element declarations:
        *   - the type definition
        *   - the substitution group affiliation
        * 2. simple/complex types:
        *   - the base type definition
        *   - the member_types of union types
        *   - the item_type of list types
        * 3. attributes declarations and attribute uses:
        *   - the type definition
        *   - if an attribute use, then the attribute declaration
        * 4. attribute group references:
        *   - the attribute group definition
        * 5. particles:
        *   - the term of the particle (e.g. a model group)
        * 6. IDC key-references:
        *   - the referenced IDC 'key' or 'unique' definition
        * 7. Attribute prohibitions which had a "ref" attribute.
        */
        for (i = 0; i < nb_items; i++)
        {
            item = (Xsd10BasicItem) items[i];
            switch (item.type)
            {
            case XML_XSD10_SCHEMA_TYPE_ELEMENT:
                resolveElementReferences(
                    (Xsd10Element) item, pctxt);
                if (fixHFailure(pctxt)) return FIX_EXIT_FAILURE;
                break;
            case XML_XSD10_SCHEMA_TYPE_COMPLEX:
            case XML_XSD10_SCHEMA_TYPE_SIMPLE:
                resolveTypeReferences(
                    (Xsd10Type) item, pctxt);
                if (fixHFailure(pctxt)) return FIX_EXIT_FAILURE;
                break;
            case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE:
                resolveAttrTypeReferences(
                    (Xsd10Attribute) item, pctxt);
                if (fixHFailure(pctxt)) return FIX_EXIT_FAILURE;
                break;
            case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE_USE:
                resolveAttrUseReferences(
                    (Xsd10AttributeUse) item, pctxt);
                if (fixHFailure(pctxt)) return FIX_EXIT_FAILURE;
                break;
            case XML_XSD10_SCHEMA_EXTRA_QNAMEREF:
                if (((Xsd10QNameRef) item).item_type ==
                    XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP)
                {
                    resolveAttrGroupReferences(
                        (Xsd10QNameRef) item, pctxt);
                }
                if (fixHFailure(pctxt)) return FIX_EXIT_FAILURE;
                break;
            case XML_XSD10_SCHEMA_TYPE_SEQUENCE:
            case XML_XSD10_SCHEMA_TYPE_CHOICE:
            case XML_XSD10_SCHEMA_TYPE_ALL:
                resolveModelGroupParticleReferences(pctxt,
                    (Xsd10ModelGroup) item);
                if (fixHFailure(pctxt)) return FIX_EXIT_FAILURE;
                break;
            case XML_XSD10_SCHEMA_TYPE_IDC_KEY:
            case XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE:
            case XML_XSD10_SCHEMA_TYPE_IDC_KEYREF:
                resolveIdcKeyReferences(
                    (Xsd10IDC) item, pctxt);
                if (fixHFailure(pctxt)) return FIX_EXIT_FAILURE;
                break;
            case XML_XSD10_SCHEMA_EXTRA_ATTR_USE_PROHIB:
                /*
                * Handle attribute prohibition which had a
                * "ref" attribute.
                */
                resolveAttrUseProhibReferences(
                    (Xsd10AttributeUseProhib) item, pctxt);
                if (fixHFailure(pctxt)) return FIX_EXIT_FAILURE;
                break;
            default:
                break;
            }
        }
        if (pctxt.nberrors != 0)
        {
            return FIX_EXIT_ERROR;
        }

        /*
        * Now that all references are resolved we
        * can check for circularity of...
        * 1. the base axis of type definitions
        * 2. nested model group definitions
        * 3. nested attribute group definitions
        * TODO: check for circular substitution groups.
        */
        for (i = 0; i < nb_items; i++)
        {
            item = (Xsd10BasicItem) items[i];
            /*
            * Let's better stop on the first error here.
            */
            switch (item.type)
            {
            case XML_XSD10_SCHEMA_TYPE_COMPLEX:
            case XML_XSD10_SCHEMA_TYPE_SIMPLE:
                checkTypeDefCircular(
                    (Xsd10Type) item, pctxt);
                if (fixHFailure(pctxt)) return FIX_EXIT_FAILURE;
                if (pctxt.nberrors != 0)
                {
                    return FIX_EXIT_ERROR;
                }
                break;
            case XML_XSD10_SCHEMA_TYPE_GROUP:
                checkGroupDefCircular(
                    (Xsd10ModelGroupDef) item, pctxt);
                if (fixHFailure(pctxt)) return FIX_EXIT_FAILURE;
                if (pctxt.nberrors != 0)
                {
                    return FIX_EXIT_ERROR;
                }
                break;
            case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
                checkAttrGroupCircular(
                    (Xsd10AttributeGroup) item, pctxt);
                if (fixHFailure(pctxt)) return FIX_EXIT_FAILURE;
                if (pctxt.nberrors != 0)
                {
                    return FIX_EXIT_ERROR;
                }
                break;
            default:
                break;
            }
        }
        if (pctxt.nberrors != 0)
        {
            return FIX_EXIT_ERROR;
        }
        /*
        * Model group definition references:
        * Such a reference is reflected by a particle at the component
        * level. Until now the 'term' of such particles pointed
        * to the model group definition; this was done, in order to
        * ease circularity checks. Now we need to set the 'term' of
        * such particles to the model group of the model group definition.
        */
        for (i = 0; i < nb_items; i++)
        {
            item = (Xsd10BasicItem) items[i];
            switch (item.type)
            {
            case XML_XSD10_SCHEMA_TYPE_SEQUENCE:
            case XML_XSD10_SCHEMA_TYPE_CHOICE:
                modelGroupToModelGroupDefFixup(pctxt,
                    (Xsd10ModelGroup) item);
                break;
            default:
                break;
            }
        }
        if (pctxt.nberrors != 0)
        {
            return FIX_EXIT_ERROR;
        }
        /*
        * Expand attribute group references of attribute group definitions.
        */
        for (i = 0; i < nb_items; i++)
        {
            item = (Xsd10BasicItem) items[i];
            switch (item.type)
            {
            case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
                if ((wxsAttrGroupExpanded(item) == 0) &&
                    (wxsAttrGroupHasRefs(item) != 0))
                {
                    attributeGroupExpandRefs(pctxt,
                        (Xsd10AttributeGroup) item);
                    if (fixHFailure(pctxt)) return FIX_EXIT_FAILURE;
                }
                break;
            default:
                break;
            }
        }
        if (pctxt.nberrors != 0)
        {
            return FIX_EXIT_ERROR;
        }
        /*
        * First compute the variety of simple types. This is needed as
        * a separate step, since otherwise we won't be able to detect
        * circular union types in all cases.
        */
        for (i = 0; i < nb_items; i++)
        {
            item = (Xsd10BasicItem) items[i];
            switch (item.type)
            {
            case XML_XSD10_SCHEMA_TYPE_SIMPLE:
                if (wxsIsTypeNotFixed1((Xsd10Type) item) != 0)
                {
                    fixupSimpleTypeStageOne(pctxt,
                        (Xsd10Type) item);
                    if (fixHFailure(pctxt)) return FIX_EXIT_FAILURE;
                }
                break;
            default:
                break;
            }
        }
        if (pctxt.nberrors != 0)
        {
            return FIX_EXIT_ERROR;
        }
        /*
        * Detect circular union types. Note that this needs the variety to
        * be already computed.
        */
        for (i = 0; i < nb_items; i++)
        {
            item = (Xsd10BasicItem) items[i];
            switch (item.type)
            {
            case XML_XSD10_SCHEMA_TYPE_SIMPLE:
                if (((Xsd10Type) item).member_types != null)
                {
                    checkUnionTypeDefCircular(pctxt,
                        (Xsd10Type) item);
                    if (fixHFailure(pctxt)) return FIX_EXIT_FAILURE;
                }
                break;
            default:
                break;
            }
        }
        if (pctxt.nberrors != 0)
        {
            return FIX_EXIT_ERROR;
        }

        /*
        * Do the complete type fixup for simple types.
        */
        for (i = 0; i < nb_items; i++)
        {
            item = (Xsd10BasicItem) items[i];
            switch (item.type)
            {
            case XML_XSD10_SCHEMA_TYPE_SIMPLE:
                if (wxsIsTypeNotFixed((Xsd10Type) item) != 0)
                {
                    fixupSimpleTypeStageTwo(pctxt, (Xsd10Type) item);
                    if (fixHFailure(pctxt)) return FIX_EXIT_FAILURE;
                }
                break;
            default:
                break;
            }
        }
        if (pctxt.nberrors != 0)
        {
            return FIX_EXIT_ERROR;
        }
        /*
        * At this point we need build and check all simple types.
        */
        /*
        * Apply constraints for attribute declarations.
        */
        for (i = 0; i < nb_items; i++)
        {
            item = (Xsd10BasicItem) items[i];
            switch (item.type)
            {
            case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE:
                checkAttrPropsCorrect(pctxt, (Xsd10Attribute) item);
                if (fixHFailure(pctxt)) return FIX_EXIT_FAILURE;
                break;
            default:
                break;
            }
        }
        if (pctxt.nberrors != 0)
        {
            return FIX_EXIT_ERROR;
        }
        /*
        * Apply constraints for attribute uses.
        */
        for (i = 0; i < nb_items; i++)
        {
            item = (Xsd10BasicItem) items[i];
            switch (item.type)
            {
            case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE_USE:
                if (((Xsd10AttributeUse) item).def_value != null)
                {
                    checkAttrUsePropsCorrect(pctxt,
                        (Xsd10AttributeUse) item);
                    if (fixHFailure(pctxt)) return FIX_EXIT_FAILURE;
                }
                break;
            default:
                break;
            }
        }
        if (pctxt.nberrors != 0)
        {
            return FIX_EXIT_ERROR;
        }

        /*
        * Apply constraints for attribute group definitions.
        */
        for (i = 0; i < nb_items; i++)
        {
            item = (Xsd10BasicItem) items[i];
            switch (item.type)
            {
            case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
                if ((((Xsd10AttributeGroup) item).attr_uses != null) &&
                    (((Xsd10ItemList) ((Xsd10AttributeGroup) item).attr_uses).nb_items > 1))
                {
                    checkAgPropsCorrect(pctxt, (Xsd10AttributeGroup) item);
                    if (fixHFailure(pctxt)) return FIX_EXIT_FAILURE;
                }
                break;
            default:
                break;
            }
        }
        if (pctxt.nberrors != 0)
        {
            return FIX_EXIT_ERROR;
        }

        /*
        * Apply constraints for redefinitions.
        */
        if (wxsConstructor(pctxt).redefs != null)
        {
            checkSrcRedefineSecond(pctxt);
        }
        if (pctxt.nberrors != 0)
        {
            return FIX_EXIT_ERROR;
        }

        /*
        * Complex types are built and checked.
        */
        for (i = 0; i < nb_items; i++)
        {
            item = (Xsd10BasicItem) con.pending.items[i];
            switch (item.type)
            {
            case XML_XSD10_SCHEMA_TYPE_COMPLEX:
                if (wxsIsTypeNotFixed((Xsd10Type) item) != 0)
                {
                    fixupComplexType(pctxt, (Xsd10Type) item);
                    if (fixHFailure(pctxt)) return FIX_EXIT_FAILURE;
                }
                break;
            default:
                break;
            }
        }
        if (pctxt.nberrors != 0)
        {
            return FIX_EXIT_ERROR;
        }

        /*
        * The list could have changed, since xmlSchemaFixupComplexType()
        * will create particles and model groups in some cases.
        */
        items = con.pending.items;
        nb_items = con.pending.nb_items;

        /*
        * Apply some constraints for element declarations.
        */
        for (i = 0; i < nb_items; i++)
        {
            item = (Xsd10BasicItem) items[i];
            switch (item.type)
            {
            case XML_XSD10_SCHEMA_TYPE_ELEMENT:
                elem_decl = (Xsd10Element) item;

                if ((elem_decl.flags & XML_XSD10_SCHEMAS_ELEM_INTERNAL_CHECKED) == 0)
                {
                    checkElementDeclComponent(
                        elem_decl, pctxt);
                    if (fixHFailure(pctxt)) return FIX_EXIT_FAILURE;
                }

                break;
            default:
                break;
            }
        }
        if (pctxt.nberrors != 0)
        {
            return FIX_EXIT_ERROR;
        }

        /*
        * Finally we can build the automaton from the content model of
        * complex types.
        */

        for (i = 0; i < nb_items; i++)
        {
            item = (Xsd10BasicItem) items[i];
            switch (item.type)
            {
            case XML_XSD10_SCHEMA_TYPE_COMPLEX:
                buildContentModel((Xsd10Type) item, pctxt);
                /* FIXHFAILURE; */
                break;
            default:
                break;
            }
        }
        if (pctxt.nberrors != 0)
        {
            return FIX_EXIT_ERROR;
        }
        /*
        * URGENT TODO: cos-element-consistent
        */
        return FIX_EXIT;
    }

    // xml_xsd10_parse
    /**
     * Port of xmlSchemaParse. Parse a schema definition resource and build an internal
     * XML Schema structure which can be used to validate instances.
     * @param ctxt a schema validation context
     * @return the internal XML Schema structure built from the resource or null in case of error
     */
    public static Xsd10Schema parse(Xsd10ParserCtxt ctxt)
    {
        Xsd10Schema main_schema = null;
        Xsd10Bucket[] bucket = new Xsd10Bucket[1];
        int res;

        /*
        * This one is used if the schema to be parsed was specified via
        * the API; i.e. not automatically by the validated instance document.
        */

        if (initTypes() < 0)
        {
            return null;
        }

        if (ctxt == null)
        {
            return null;
        }

        /* TODO: Init the context. Is this all we need?*/
        ctxt.nberrors = 0;
        ctxt.err = 0;
        ctxt.counter = 0;

        failure:
        {
            /* Create the *main* schema. */
            main_schema = newSchema(ctxt);
            if (main_schema == null)
            {
                break failure;
            }
            /*
            * Create the schema constructor.
            */
            if (ctxt.constructor == null)
            {
                ctxt.constructor = constructionCtxtCreate(ctxt.dict);
                if (ctxt.constructor == null)
                {
                    break failure;
                }
                /* Take ownership of the constructor to be able to free it. */
                ctxt.owns_constructor = 1;
            }
            ctxt.constructor.main_schema = main_schema;
            exit:
            {
                /*
                * Locate and add the schema document.
                */
                res = addSchemaDoc(ctxt, XML_XSD10_SCHEMA_SCHEMA_MAIN,
                    ctxt.URL, ctxt.doc, ctxt.buffer, ctxt.size, null,
                    null, null, bucket);
                if (res == -1)
                {
                    break failure;
                }
                if (res != 0)
                {
                    break exit;
                }

                if (bucket[0] == null)
                {
                    /* TODO: Error code, actually we failed to *locate* the schema. */
                    if (ctxt.URL != null)
                    {
                        customErr(ctxt, XML_XSD10_SCHEMAP_FAILED_LOAD,
                            null, null,
                            "Failed to locate the main schema resource at '%s'",
                            ctxt.URL, null);
                    }
                    else
                    {
                        customErr(ctxt, XML_XSD10_SCHEMAP_FAILED_LOAD,
                            null, null,
                            "Failed to locate the main schema resource",
                            null, null);
                    }
                    break exit;
                }
                /* Then do the parsing for good. */
                if (parseNewDocWithContext(ctxt, main_schema, bucket[0]) == -1)
                {
                    break failure;
                }
                if (ctxt.nberrors != 0)
                {
                    break exit;
                }

                main_schema.doc = bucket[0].doc;
                main_schema.preserve = ctxt.preserve;

                ctxt.schema = main_schema;

                if (fixupComponents(ctxt, wxsConstructor(ctxt).main_bucket) == -1)
                {
                    break failure;
                }
            }
            /*
            * TODO: This is not nice, since we cannot distinguish from the
            * result if there was an internal error or not.
            */
            /* exit: */
            if (ctxt.nberrors != 0)
            {
                if (main_schema != null)
                {
                    free(main_schema);
                    main_schema = null;
                }
                if (ctxt.constructor != null)
                {
                    constructionCtxtFree(ctxt.constructor);
                    ctxt.constructor = null;
                    ctxt.owns_constructor = 0;
                }
            }
            ctxt.schema = null;
            return main_schema;
        }
        /* exit_failure: */
        /*
        * Quite verbose, but should catch internal errors, which were
        * not communicated.
        */
        if (main_schema != null)
        {
            free(main_schema);
            main_schema = null;
        }
        if (ctxt.constructor != null)
        {
            constructionCtxtFree(ctxt.constructor);
            ctxt.constructor = null;
            ctxt.owns_constructor = 0;
        }
        perrorInt2(ctxt, "xmlSchemaParse",
            "An internal error occurred");
        ctxt.schema = null;
        return null;
    }
}
