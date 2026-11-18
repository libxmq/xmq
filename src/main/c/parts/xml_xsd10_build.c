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

/* Schema construction (port of libxml2 xmlschemas.c: building the content models,
   fixup of the components and the schema component constraints). */

#ifndef BUILDING_DIST_XMQ

#define XML_XSD10_INTERNALS
#include"xml_xsd10_schemas.h"
#include"xml_xsd10_build.h"
#include"xml_dom.h"
#include"xml_parser.h"
#include"xml_regexp.h"
#include"xml_pattern.h"
#include"xml_uri.h"
#include"xml_xsd10_types.h"

#include<errno.h>
#include<limits.h>
#include<stdarg.h>
#include<stdint.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#endif

#ifdef XML_XSD10_BUILD_MODULE

/************************************************************************
 *									*
 *			Building the content models			*
 *									*
 ************************************************************************/

/**
 * Port of xmlSchemaBuildContentModelForSubstGroup.
 *
 * @returns 1 if nillable, 0 otherwise
 */
int xml_xsd10_build_content_model_for_subst_group(XmlXsd10ParserCtxt *pctxt, XmlXsd10Particle *particle, int counter, XmlAutomataState *end)
{
    XmlAutomataState *start, *tmp;
    XmlXsd10Element *elemDecl, *member;
    XmlXsd10SubstGroup *substGroup;
    int i;
    int ret = 0;

    elemDecl = (XmlXsd10Element *) particle->children;
    /*
    * Wrap the substitution group with a CHOICE.
    */
    start = pctxt->state;
    if (end == NULL)
	end = xml_automata_new_state(pctxt->am);
    substGroup = xml_xsd10_subst_group_get(pctxt, elemDecl);
    if (substGroup == NULL) {
	xml_xsd10_p_err(pctxt, XSD10_WXS_ITEM_NODE(particle),
	    XML_XSD10_SCHEMAP_INTERNAL,
	    "Internal error: xmlSchemaBuildContentModelForSubstGroup, "
	    "declaration is marked having a subst. group but none "
	    "available.\n", elemDecl->name, NULL);
	return(0);
    }
    if (counter >= 0) {
	/*
	* NOTE that we put the declaration in, even if it's abstract.
	* However, an error will be raised during *validation* if an element
	* information item shall be validated against an abstract element
	* declaration.
	*/
	tmp = xml_automata_new_counted_trans(pctxt->am, start, NULL, counter);
        xml_automata_new_transition2(pctxt->am, tmp, end,
	            elemDecl->name, elemDecl->targetNamespace, elemDecl);
	/*
	* Add subst. group members.
	*/
	for (i = 0; i < substGroup->members->nbItems; i++) {
	    member = (XmlXsd10Element *) substGroup->members->items[i];
            xml_automata_new_transition2(pctxt->am, tmp, end,
		               member->name, member->targetNamespace, member);
	}
    } else if (particle->maxOccurs == 1) {
	/*
	* NOTE that we put the declaration in, even if it's abstract,
	*/
	xml_automata_new_epsilon(pctxt->am,
	    xml_automata_new_transition2(pctxt->am,
	    start, NULL,
	    elemDecl->name, elemDecl->targetNamespace, elemDecl), end);
	/*
	* Add subst. group members.
	*/
	for (i = 0; i < substGroup->members->nbItems; i++) {
	    member = (XmlXsd10Element *) substGroup->members->items[i];
	    /*
	    * NOTE: This fixes bug #341150. xmlAutomataNewOnceTrans2()
	    *  was incorrectly used instead of xmlAutomataNewTransition2()
	    *  (seems like a copy&paste bug from the XML_SCHEMA_TYPE_ALL
	    *  section in xmlSchemaBuildAContentModel() ).
	    * TODO: Check if xmlAutomataNewOnceTrans2() was instead
	    *  intended for the above "counter" section originally. I.e.,
	    *  check xs:all with subst-groups.
	    *
	    * tmp = xmlAutomataNewOnceTrans2(pctxt->am, start, NULL,
	    *	               member->name, member->targetNamespace,
	    *		       1, 1, member);
	    */
	    tmp = xml_automata_new_transition2(pctxt->am, start, NULL,
		member->name, member->targetNamespace, member);
	    xml_automata_new_epsilon(pctxt->am, tmp, end);
	}
    } else {
	XmlAutomataState *hop;
	int maxOccurs = particle->maxOccurs == XML_XSD10_UNBOUNDED ?
	    XML_XSD10_UNBOUNDED : particle->maxOccurs - 1;
	int minOccurs = particle->minOccurs < 1 ? 0 : particle->minOccurs - 1;

	counter =
	    xml_automata_new_counter(pctxt->am, minOccurs,
	    maxOccurs);
	hop = xml_automata_new_state(pctxt->am);

	xml_automata_new_epsilon(pctxt->am,
	    xml_automata_new_transition2(pctxt->am,
	    start, NULL,
	    elemDecl->name, elemDecl->targetNamespace, elemDecl),
	    hop);
	/*
	 * Add subst. group members.
	 */
	for (i = 0; i < substGroup->members->nbItems; i++) {
	    member = (XmlXsd10Element *) substGroup->members->items[i];
	    xml_automata_new_epsilon(pctxt->am,
		xml_automata_new_transition2(pctxt->am,
		start, NULL,
		member->name, member->targetNamespace, member),
		hop);
	}
	xml_automata_new_counted_trans(pctxt->am, hop, start, counter);
	xml_automata_new_counter_trans(pctxt->am, hop, end, counter);
    }
    if (particle->minOccurs == 0) {
	xml_automata_new_epsilon(pctxt->am, start, end);
        ret = 1;
    }
    pctxt->state = end;
    return(ret);
}

/**
 * Port of xmlSchemaBuildContentModelForElement.
 *
 * @returns 1 if nillable, 0 otherwise
 */
int xml_xsd10_build_content_model_for_element(XmlXsd10ParserCtxt *ctxt, XmlXsd10Particle *particle)
{
    int ret = 0;

    if (((XmlXsd10Element *) particle->children)->flags &
	XML_XSD10_SCHEMAS_ELEM_SUBST_GROUP_HEAD) {
	/*
	* Substitution groups.
	*/
	ret = xml_xsd10_build_content_model_for_subst_group(ctxt, particle, -1, NULL);
    } else {
	XmlXsd10Element *elemDecl;
	XmlAutomataState *start;

	elemDecl = (XmlXsd10Element *) particle->children;

	if (elemDecl->flags & XML_XSD10_SCHEMAS_ELEM_ABSTRACT)
	    return(0);
	if (particle->maxOccurs == 1) {
	    start = ctxt->state;
	    ctxt->state = xml_automata_new_transition2(ctxt->am, start, NULL,
		    elemDecl->name, elemDecl->targetNamespace, elemDecl);
	} else if ((particle->maxOccurs >= XML_XSD10_UNBOUNDED) &&
	           (particle->minOccurs < 2)) {
	    /* Special case. */
	    start = ctxt->state;
	    ctxt->state = xml_automata_new_transition2(ctxt->am, start, NULL,
		elemDecl->name, elemDecl->targetNamespace, elemDecl);
	    ctxt->state = xml_automata_new_transition2(ctxt->am, ctxt->state, ctxt->state,
		elemDecl->name, elemDecl->targetNamespace, elemDecl);
	} else {
	    int counter;
	    int maxOccurs = particle->maxOccurs == XML_XSD10_UNBOUNDED ?
			    XML_XSD10_UNBOUNDED : particle->maxOccurs - 1;
	    int minOccurs = particle->minOccurs < 1 ?
			    0 : particle->minOccurs - 1;

	    start = xml_automata_new_epsilon(ctxt->am, ctxt->state, NULL);
	    counter = xml_automata_new_counter(ctxt->am, minOccurs, maxOccurs);
	    ctxt->state = xml_automata_new_transition2(ctxt->am, start, NULL,
		elemDecl->name, elemDecl->targetNamespace, elemDecl);
	    xml_automata_new_counted_trans(ctxt->am, ctxt->state, start, counter);
	    ctxt->state = xml_automata_new_counter_trans(ctxt->am, ctxt->state,
		NULL, counter);
	}
	if (particle->minOccurs == 0) {
	    xml_automata_new_epsilon(ctxt->am, start, ctxt->state);
            ret = 1;
        }
    }
    return(ret);
}

/**
 * Port of xmlSchemaBuildAContentModel.
 *
 * Create the automaton for the {content type} of a complex type.
 *
 * @param pctxt  the schema parser context
 * @param particle  the particle component
 * @returns 1 if the content is nillable, 0 otherwise
 */
int xml_xsd10_build_a_content_model(XmlXsd10ParserCtxt *pctxt, XmlXsd10Particle *particle)
{
    int ret = 0, tmp2;

    if (particle == NULL) {
	XSD10_PERROR_INT("xmlSchemaBuildAContentModel", "particle is NULL");
	return(1);
    }
    if (particle->children == NULL) {
	/*
	* Just return in this case. A missing "term" of the particle
	* might arise due to an invalid "term" component.
	*/
	return(1);
    }

    switch (particle->children->type) {
	case XML_XSD10_SCHEMA_TYPE_ANY: {
	    XmlAutomataState *start, *end;
	    XmlXsd10Wildcard *wild;
	    XmlXsd10WildcardNs *ns;

	    wild = (XmlXsd10Wildcard *) particle->children;

	    start = pctxt->state;
	    end = xml_automata_new_state(pctxt->am);

	    if (particle->maxOccurs == 1) {
		if (wild->any == 1) {
		    /*
		    * We need to add both transitions:
		    *
		    * 1. the {"*", "*"} for elements in a namespace.
		    */
		    pctxt->state =
			xml_automata_new_transition2(pctxt->am,
			start, NULL, "*", "*", wild);
		    xml_automata_new_epsilon(pctxt->am, pctxt->state, end);
		    /*
		    * 2. the {"*"} for elements in no namespace.
		    */
		    pctxt->state =
			xml_automata_new_transition2(pctxt->am,
			start, NULL, "*", NULL, wild);
		    xml_automata_new_epsilon(pctxt->am, pctxt->state, end);

		} else if (wild->nsSet != NULL) {
		    ns = wild->nsSet;
		    do {
			pctxt->state = start;
			pctxt->state = xml_automata_new_transition2(pctxt->am,
			    pctxt->state, NULL, "*", ns->value, wild);
			xml_automata_new_epsilon(pctxt->am, pctxt->state, end);
			ns = ns->next;
		    } while (ns != NULL);

		} else if (wild->negNsSet != NULL) {
		    pctxt->state = xml_automata_new_neg_trans(pctxt->am,
			start, end, "*", wild->negNsSet->value,
			wild);
		}
	    } else {
		int counter;
		XmlAutomataState *hop;
		int maxOccurs =
		    particle->maxOccurs == XML_XSD10_UNBOUNDED ? XML_XSD10_UNBOUNDED :
                                           particle->maxOccurs - 1;
		int minOccurs =
		    particle->minOccurs < 1 ? 0 : particle->minOccurs - 1;

		counter = xml_automata_new_counter(pctxt->am, minOccurs, maxOccurs);
		hop = xml_automata_new_state(pctxt->am);
		if (wild->any == 1) {
		    pctxt->state =
			xml_automata_new_transition2(pctxt->am,
			start, NULL, "*", "*", wild);
		    xml_automata_new_epsilon(pctxt->am, pctxt->state, hop);
		    pctxt->state =
			xml_automata_new_transition2(pctxt->am,
			start, NULL, "*", NULL, wild);
		    xml_automata_new_epsilon(pctxt->am, pctxt->state, hop);
		} else if (wild->nsSet != NULL) {
		    ns = wild->nsSet;
		    do {
			pctxt->state =
			    xml_automata_new_transition2(pctxt->am,
				start, NULL, "*", ns->value, wild);
			xml_automata_new_epsilon(pctxt->am, pctxt->state, hop);
			ns = ns->next;
		    } while (ns != NULL);

		} else if (wild->negNsSet != NULL) {
		    pctxt->state = xml_automata_new_neg_trans(pctxt->am,
			start, hop, "*", wild->negNsSet->value,
			wild);
		}
		xml_automata_new_counted_trans(pctxt->am, hop, start, counter);
		xml_automata_new_counter_trans(pctxt->am, hop, end, counter);
	    }
	    if (particle->minOccurs == 0) {
		xml_automata_new_epsilon(pctxt->am, start, end);
                ret = 1;
	    }
	    pctxt->state = end;
            break;
	}
        case XML_XSD10_SCHEMA_TYPE_ELEMENT:
	    ret = xml_xsd10_build_content_model_for_element(pctxt, particle);
	    break;
        case XML_XSD10_SCHEMA_TYPE_SEQUENCE:{
            XmlXsd10TreeItem *sub;

            ret = 1;
            /*
             * If max and min occurrences are default (1) then
             * simply iterate over the particles of the <sequence>.
             */
            if ((particle->minOccurs == 1) && (particle->maxOccurs == 1)) {
                sub = particle->children->children;

                while (sub != NULL) {
                    tmp2 = xml_xsd10_build_a_content_model(pctxt,
                                        (XmlXsd10Particle *) sub);
                    if (tmp2 != 1) ret = 0;
                    sub = sub->next;
                }
            } else {
                XmlAutomataState *oldstate = pctxt->state;

                if (particle->maxOccurs >= XML_XSD10_UNBOUNDED) {
                    if (particle->minOccurs > 1) {
                        XmlAutomataState *tmp;
                        int counter;

                        pctxt->state = xml_automata_new_epsilon(pctxt->am,
                            oldstate, NULL);
                        oldstate = pctxt->state;

                        counter = xml_automata_new_counter(pctxt->am,
                            particle->minOccurs - 1, XML_XSD10_UNBOUNDED);

                        sub = particle->children->children;
                        while (sub != NULL) {
                            tmp2 = xml_xsd10_build_a_content_model(pctxt,
                                            (XmlXsd10Particle *) sub);
                            if (tmp2 != 1) ret = 0;
                            sub = sub->next;
                        }
                        tmp = pctxt->state;
                        xml_automata_new_counted_trans(pctxt->am, tmp,
                                                   oldstate, counter);
                        pctxt->state =
                            xml_automata_new_counter_trans(pctxt->am, tmp,
                                                       NULL, counter);
                        if (ret == 1)
                            xml_automata_new_epsilon(pctxt->am,
                                                oldstate, pctxt->state);

                    } else {
                        pctxt->state = xml_automata_new_epsilon(pctxt->am,
                            oldstate, NULL);
                        oldstate = pctxt->state;

                        sub = particle->children->children;
                        while (sub != NULL) {
                            tmp2 = xml_xsd10_build_a_content_model(pctxt,
                                        (XmlXsd10Particle *) sub);
                            if (tmp2 != 1) ret = 0;
                            sub = sub->next;
                        }
                        xml_automata_new_epsilon(pctxt->am, pctxt->state,
                                              oldstate);
                        /*
                         * epsilon needed to block previous trans from
                         * being allowed to enter back from another
                         * construct
                         */
                        pctxt->state = xml_automata_new_epsilon(pctxt->am,
                                            pctxt->state, NULL);
                        if (particle->minOccurs == 0) {
                            xml_automata_new_epsilon(pctxt->am,
                                oldstate, pctxt->state);
                            ret = 1;
                        }
                    }
                } else if ((particle->maxOccurs > 1)
                           || (particle->minOccurs > 1)) {
                    XmlAutomataState *tmp;
                    int counter;

                    pctxt->state = xml_automata_new_epsilon(pctxt->am,
                        oldstate, NULL);
                    oldstate = pctxt->state;

                    counter = xml_automata_new_counter(pctxt->am,
                        particle->minOccurs - 1,
                        particle->maxOccurs - 1);

                    sub = particle->children->children;
                    while (sub != NULL) {
                        tmp2 = xml_xsd10_build_a_content_model(pctxt,
                                        (XmlXsd10Particle *) sub);
                        if (tmp2 != 1) ret = 0;
                        sub = sub->next;
                    }
                    tmp = pctxt->state;
                    xml_automata_new_counted_trans(pctxt->am,
                        tmp, oldstate, counter);
                    pctxt->state =
                        xml_automata_new_counter_trans(pctxt->am, tmp, NULL,
                                                   counter);
                    if ((particle->minOccurs == 0) || (ret == 1)) {
                        xml_automata_new_epsilon(pctxt->am,
                                            oldstate, pctxt->state);
                        ret = 1;
                    }
                } else {
                    sub = particle->children->children;
                    while (sub != NULL) {
                        tmp2 = xml_xsd10_build_a_content_model(pctxt,
                                        (XmlXsd10Particle *) sub);
                        if (tmp2 != 1) ret = 0;
                        sub = sub->next;
                    }

		    /*
		     * epsilon needed to block previous trans from
		     * being allowed to enter back from another
		     * construct
		     */
		    pctxt->state = xml_automata_new_epsilon(pctxt->am,
					pctxt->state, NULL);

                    if (particle->minOccurs == 0) {
                        xml_automata_new_epsilon(pctxt->am, oldstate,
                                              pctxt->state);
                        ret = 1;
                    }
                }
            }
            break;
        }
        case XML_XSD10_SCHEMA_TYPE_CHOICE:{
            XmlXsd10TreeItem *sub;
            XmlAutomataState *start, *end;

            ret = 0;
            start = pctxt->state;
            end = xml_automata_new_state(pctxt->am);

            /*
             * iterate over the subtypes and remerge the end with an
             * epsilon transition
             */
            if (particle->maxOccurs == 1) {
                sub = particle->children->children;
                while (sub != NULL) {
                    pctxt->state = start;
                    tmp2 = xml_xsd10_build_a_content_model(pctxt,
                                        (XmlXsd10Particle *) sub);
                    if (tmp2 == 1) ret = 1;
                    xml_automata_new_epsilon(pctxt->am, pctxt->state, end);
                    sub = sub->next;
                }
            } else {
                int counter;
                XmlAutomataState *hop, *base;
                int maxOccurs = particle->maxOccurs == XML_XSD10_UNBOUNDED ?
                    XML_XSD10_UNBOUNDED : particle->maxOccurs - 1;
                int minOccurs =
                    particle->minOccurs < 1 ? 0 : particle->minOccurs - 1;

                /*
                 * use a counter to keep track of the number of transitions
                 * which went through the choice.
                 */
                counter =
                    xml_automata_new_counter(pctxt->am, minOccurs, maxOccurs);
                hop = xml_automata_new_state(pctxt->am);
                base = xml_automata_new_state(pctxt->am);

                sub = particle->children->children;
                while (sub != NULL) {
                    pctxt->state = base;
                    tmp2 = xml_xsd10_build_a_content_model(pctxt,
                                        (XmlXsd10Particle *) sub);
                    if (tmp2 == 1) ret = 1;
                    xml_automata_new_epsilon(pctxt->am, pctxt->state, hop);
                    sub = sub->next;
                }
                xml_automata_new_epsilon(pctxt->am, start, base);
                xml_automata_new_counted_trans(pctxt->am, hop, base, counter);
                xml_automata_new_counter_trans(pctxt->am, hop, end, counter);
                if (ret == 1)
                    xml_automata_new_epsilon(pctxt->am, base, end);
            }
            if (particle->minOccurs == 0) {
                xml_automata_new_epsilon(pctxt->am, start, end);
                ret = 1;
            }
            pctxt->state = end;
            break;
        }
        case XML_XSD10_SCHEMA_TYPE_ALL:{
            XmlAutomataState *start, *tmp;
            XmlXsd10Particle *sub;
            XmlXsd10Element *elemDecl;

            ret = 1;

            sub = (XmlXsd10Particle *) particle->children->children;
            if (sub == NULL)
                break;

            ret = 0;

            start = pctxt->state;
            tmp = xml_automata_new_state(pctxt->am);
            xml_automata_new_epsilon(pctxt->am, pctxt->state, tmp);
            pctxt->state = tmp;
            while (sub != NULL) {
                pctxt->state = tmp;

                elemDecl = (XmlXsd10Element *) sub->children;
                if (elemDecl == NULL) {
                    XSD10_PERROR_INT("xmlSchemaBuildAContentModel",
                        "<element> particle has no term");
                    return(ret);
                };
                /*
                * NOTE: The {max occurs} of all the particles in the
                * {particles} of the group must be 0 or 1; this is
                * already ensured during the parse of the content of
                * <all>.
                */
                if (elemDecl->flags & XML_XSD10_SCHEMAS_ELEM_SUBST_GROUP_HEAD) {
                    int counter;

                    /*
                     * This is an abstract group, we need to share
                     * the same counter for all the element transitions
                     * derived from the group
                     */
                    counter = xml_automata_new_counter(pctxt->am,
                                       sub->minOccurs, sub->maxOccurs);
                    xml_xsd10_build_content_model_for_subst_group(pctxt,
                                       sub, counter, pctxt->state);
                } else {
                    if ((sub->minOccurs == 1) &&
                        (sub->maxOccurs == 1)) {
                        xml_automata_new_once_trans2(pctxt->am, pctxt->state,
                                                pctxt->state,
                                                elemDecl->name,
                                                elemDecl->targetNamespace,
                                                1, 1, elemDecl);
                    } else if ((sub->minOccurs == 0) &&
                        (sub->maxOccurs == 1)) {

                        xml_automata_new_count_trans2(pctxt->am, pctxt->state,
                                                 pctxt->state,
                                                 elemDecl->name,
                                                 elemDecl->targetNamespace,
                                                 0,
                                                 1,
                                                 elemDecl);
                    }
                }
                sub = (XmlXsd10Particle *) sub->next;
            }
            pctxt->state =
                xml_automata_new_all_trans(pctxt->am, pctxt->state, NULL, 0);
            if (particle->minOccurs == 0) {
                xml_automata_new_epsilon(pctxt->am, start, pctxt->state);
                ret = 1;
            }
            break;
        }
	case XML_XSD10_SCHEMA_TYPE_GROUP:
	    /*
	    * If we hit a model group definition, then this means that
	    * it was empty, thus was not substituted for the containing
	    * model group. Just do nothing in this case.
	    * TODO: But the group should be substituted and not occur at
	    * all in the content model at this point. Fix this.
	    */
            ret = 1;
	    break;
        default:
	    xml_xsd10_internal_err2(XSD10_ACTXT_CAST pctxt,
		"xmlSchemaBuildAContentModel",
		"found unexpected term of type '%s' in content model",
		XSD10_WXS_ITEM_TYPE_NAME(particle->children), NULL);
            return(ret);
    }
    return(ret);
}

/**
 * Port of xmlSchemaBuildContentModel.
 *
 * Builds the content model of the complex type.
 *
 * @param type  the complex type definition
 * @param ctxt  the schema parser context
 */
void xml_xsd10_build_content_model(XmlXsd10Type *type, XmlXsd10ParserCtxt *ctxt)
{
    if ((type->type != XML_XSD10_SCHEMA_TYPE_COMPLEX) ||
	(type->contModel != NULL) ||
	((type->contentType != XML_XSD10_SCHEMA_CONTENT_ELEMENTS) &&
	(type->contentType != XML_XSD10_SCHEMA_CONTENT_MIXED)))
	return;

    ctxt->am = NULL;
    ctxt->am = xml_automata_new();
    if (ctxt->am == NULL) {
	xml_xsd10_p_err_memory(ctxt);
        return;
    }
    ctxt->state = xml_automata_get_init_state(ctxt->am);
    /*
    * Build the automaton.
    */
    xml_xsd10_build_a_content_model(ctxt, XSD10_WXS_TYPE_PARTICLE(type));
    xml_automata_set_final_state(ctxt->am, ctxt->state);
    type->contModel = xml_automata_compile(ctxt->am);
    if (type->contModel == NULL) {
        xml_xsd10_p_custom_err(ctxt,
	    XML_XSD10_SCHEMAP_INTERNAL,
	    XSD10_WXS_BASIC_CAST type, type->node,
	    "Failed to compile the content model", NULL);
    } else if (xml_regexp_is_determinist((XmlRegexp *) type->contModel) != 1) {
        xml_xsd10_p_custom_err(ctxt,
	    XML_XSD10_SCHEMAP_NOT_DETERMINISTIC,
	    /* XML_SCHEMAS_ERR_NOTDETERMINIST, */
	    XSD10_WXS_BASIC_CAST type, type->node,
	    "The content model is not determinist", NULL);
    } else {
    }
    ctxt->state = NULL;
    xml_automata_free(ctxt->am);
    ctxt->am = NULL;
}

/**
 * Port of xmlSchemaResolveElementReferences.
 *
 * Resolves the references of an element declaration
 * or particle, which has an element declaration as it's
 * term.
 *
 * @param elemDecl  the schema element context
 * @param ctxt  the schema parser context
 */
void xml_xsd10_resolve_element_references(XmlXsd10Element *elemDecl, XmlXsd10ParserCtxt *ctxt)
{
    if ((ctxt == NULL) || (elemDecl == NULL) ||
	((elemDecl != NULL) &&
	(elemDecl->flags & XML_XSD10_SCHEMAS_ELEM_INTERNAL_RESOLVED)))
        return;
    elemDecl->flags |= XML_XSD10_SCHEMAS_ELEM_INTERNAL_RESOLVED;

    if ((elemDecl->subtypes == NULL) && (elemDecl->namedType != NULL)) {
	XmlXsd10Type *type;

	/* (type definition) ... otherwise the type definition `resolved`
	* to by the `actual value` of the type [attribute] ...
	*/
	type = xml_xsd10_get_type(ctxt->schema, elemDecl->namedType,
	    elemDecl->namedTypeNs);
	if (type == NULL) {
	    xml_xsd10_p_res_comp_attr_err(ctxt,
		XML_XSD10_SCHEMAP_SRC_RESOLVE,
		XSD10_WXS_BASIC_CAST elemDecl, elemDecl->node,
		"type", elemDecl->namedType, elemDecl->namedTypeNs,
		XML_XSD10_SCHEMA_TYPE_BASIC, "type definition");
	} else
	    elemDecl->subtypes = type;
    }
    if (elemDecl->substGroup != NULL) {
	XmlXsd10Element *substHead;

	/*
	* FIXME TODO: Do we need a new field in _xmlSchemaElement for
	* substitutionGroup?
	*/
	substHead = xml_xsd10_get_elem(ctxt->schema, elemDecl->substGroup,
	    elemDecl->substGroupNs);
	if (substHead == NULL) {
	    xml_xsd10_p_res_comp_attr_err(ctxt,
		XML_XSD10_SCHEMAP_SRC_RESOLVE,
		XSD10_WXS_BASIC_CAST elemDecl, NULL,
		"substitutionGroup", elemDecl->substGroup,
		elemDecl->substGroupNs, XML_XSD10_SCHEMA_TYPE_ELEMENT, NULL);
	} else {
	    xml_xsd10_resolve_element_references(substHead, ctxt);
	    /*
	    * Set the "substitution group affiliation".
	    * NOTE that now we use the "refDecl" field for this.
	    */
	    XSD10_WXS_SUBST_HEAD(elemDecl) = substHead;
	    /*
	    * The type definitions is set to:
	    * SPEC "...the {type definition} of the element
	    * declaration `resolved` to by the `actual value`
	    * of the substitutionGroup [attribute], if present"
	    */
	    if (elemDecl->subtypes == NULL) {
                if (substHead->subtypes == NULL) {
                    /*
                     * This can happen with self-referencing substitution
                     * groups. The cycle will be detected later, but we have
                     * to set subtypes to avoid null-pointer dereferences.
                     */
	            elemDecl->subtypes = xml_xsd10_get_built_in_type(
                            XML_XSD10_SCHEMAS_ANYTYPE);
                } else {
		    elemDecl->subtypes = substHead->subtypes;
                }
            }
	}
    }
    /*
    * SPEC "The definition of anyType serves as the default type definition
    * for element declarations whose XML representation does not specify one."
    */
    if ((elemDecl->subtypes == NULL) &&
	(elemDecl->namedType == NULL) &&
	(elemDecl->substGroup == NULL))
	elemDecl->subtypes = xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_ANYTYPE);
}

/**
 * Port of xmlSchemaResolveUnionMemberTypes.
 *
 * Checks and builds the "member type definitions" property of the union
 * simple type. This handles part (1), part (2) is done in
 * xmlSchemaFinishMemberTypeDefinitionsProperty
 *
 * @param ctxt  the schema parser context
 * @param type  the schema simple type definition
 * @returns -1 in case of an internal error, 0 otherwise.
 */
int xml_xsd10_resolve_union_member_types(XmlXsd10ParserCtxt *ctxt, XmlXsd10Type *type)
{

    XmlXsd10TypeLink *link, *lastLink, *newLink;
    XmlXsd10Type *memberType;

    /*
    * SPEC (1) "If the <union> alternative is chosen, then [Definition:]
    * define the explicit members as the type definitions `resolved`
    * to by the items in the `actual value` of the memberTypes [attribute],
    * if any, followed by the type definitions corresponding to the
    * <simpleType>s among the [children] of <union>, if any."
    */
    /*
    * Resolve references.
    */
    link = type->memberTypes;
    lastLink = NULL;
    while (link != NULL) {
	const char *name, *nsName;

	name = ((XmlXsd10QNameRef *) link->type)->name;
	nsName = ((XmlXsd10QNameRef *) link->type)->targetNamespace;

	memberType = xml_xsd10_get_type(ctxt->schema, name, nsName);
	if ((memberType == NULL) || (! XSD10_WXS_IS_SIMPLE(memberType))) {
	    xml_xsd10_p_res_comp_attr_err(ctxt, XML_XSD10_SCHEMAP_SRC_RESOLVE,
		XSD10_WXS_BASIC_CAST type, type->node, "memberTypes",
		name, nsName, XML_XSD10_SCHEMA_TYPE_SIMPLE, NULL);
	    /*
	    * Remove the member type link.
	    */
	    if (lastLink == NULL)
		type->memberTypes = link->next;
	    else
		lastLink->next = link->next;
	    newLink = link;
	    link = link->next;
	    free(newLink);
	} else {
	    link->type = memberType;
	    lastLink = link;
	    link = link->next;
	}
    }
    /*
    * Add local simple types,
    */
    memberType = type->subtypes;
    while (memberType != NULL) {
	link = (XmlXsd10TypeLink *) malloc(sizeof(XmlXsd10TypeLink));
	if (link == NULL) {
	    xml_xsd10_p_err_memory(ctxt);
	    return (-1);
	}
	link->type = memberType;
	link->next = NULL;
	if (lastLink == NULL)
	    type->memberTypes = link;
	else
	    lastLink->next = link;
	lastLink = link;
	memberType = memberType->next;
    }
    return (0);
}

/**
 * Port of xmlSchemaIsDerivedFromBuiltInType.
 *
 *
 * @param type  the type definition
 * @param valType  the value type
 * @returns 1 if the type has the given value type, or
 * is derived from such a type.
 */
int xml_xsd10_is_derived_from_built_in_type(XmlXsd10Type *type, int valType)
{
    if (type == NULL)
	return (0);
    if (XSD10_WXS_IS_COMPLEX(type))
	return (0);
    if (type->type == XML_XSD10_SCHEMA_TYPE_BASIC) {
	if (type->builtInType == valType)
	    return(1);
	if ((type->builtInType == XML_XSD10_SCHEMAS_ANYSIMPLETYPE) ||
	    (type->builtInType == XML_XSD10_SCHEMAS_ANYTYPE))
	    return (0);
	return(xml_xsd10_is_derived_from_built_in_type(type->subtypes, valType));
    }
    return(xml_xsd10_is_derived_from_built_in_type(type->subtypes, valType));
}


/**
 * Port of xmlSchemaGetPrimitiveType.
 *
 * @param type  the simpleType definition
 * @returns the primitive type of the given type or
 * NULL in case of error.
 */
XmlXsd10Type *xml_xsd10_get_primitive_type(XmlXsd10Type *type)
{

    while (type != NULL) {
	/*
	* Note that anySimpleType is actually not a primitive type
	* but we need that here.
	*/
	if ((type->builtInType == XML_XSD10_SCHEMAS_ANYSIMPLETYPE) ||
	   (type->flags & XML_XSD10_SCHEMAS_TYPE_BUILTIN_PRIMITIVE))
	    return (type);
	type = type->baseType;
    }

    return (NULL);
}


/**
 * Port of xmlSchemaCloneWildcardNsConstraints.
 *
 * Clones the namespace constraints of source
 * and assigns them to dest.
 *
 * @param ctxt  the schema parser context
 * @param dest  the destination wildcard
 * @param source  the source wildcard
 * @returns -1 on internal error, 0 otherwise.
 */
int xml_xsd10_clone_wildcard_ns_constraints(XmlXsd10ParserCtxt *ctxt, XmlXsd10Wildcard *dest, XmlXsd10Wildcard *source)
{
    XmlXsd10WildcardNs *cur, *tmp, *last;

    if ((source == NULL) || (dest == NULL))
	return(-1);
    dest->any = source->any;
    cur = source->nsSet;
    last = NULL;
    while (cur != NULL) {
	tmp = xml_xsd10_new_wildcard_ns_constraint(ctxt);
	if (tmp == NULL)
	    return(-1);
	tmp->value = cur->value;
	if (last == NULL)
	    dest->nsSet = tmp;
	else
	    last->next = tmp;
	last = tmp;
	cur = cur->next;
    }
    if (dest->negNsSet != NULL)
	xml_xsd10_free_wildcard_ns_set(dest->negNsSet);
    if (source->negNsSet != NULL) {
	dest->negNsSet = xml_xsd10_new_wildcard_ns_constraint(ctxt);
	if (dest->negNsSet == NULL)
	    return(-1);
	dest->negNsSet->value = source->negNsSet->value;
    } else
	dest->negNsSet = NULL;
    return(0);
}

/**
 * Port of xmlSchemaUnionWildcards.
 *
 * Unions the namespace constraints of the given wildcards.
 * `completeWild` will hold the resulting union.
 *
 * @param ctxt  the schema parser context
 * @param completeWild  the first wildcard
 * @param curWild  the second wildcard
 * @returns a positive error code on failure, -1 in case of an
 * internal error, 0 otherwise.
 */
int xml_xsd10_union_wildcards(XmlXsd10ParserCtxt *ctxt, XmlXsd10Wildcard *completeWild, XmlXsd10Wildcard *curWild)
{
    XmlXsd10WildcardNs *cur, *curB, *tmp;

    /*
    * 1 If O1 and O2 are the same value, then that value must be the
    * value.
    */
    if ((completeWild->any == curWild->any) &&
	((completeWild->nsSet == NULL) == (curWild->nsSet == NULL)) &&
	((completeWild->negNsSet == NULL) == (curWild->negNsSet == NULL))) {

	if ((completeWild->negNsSet == NULL) ||
	    (completeWild->negNsSet->value == curWild->negNsSet->value)) {

	    if (completeWild->nsSet != NULL) {
		int found = 0;

		/*
		* Check equality of sets.
		*/
		cur = completeWild->nsSet;
		while (cur != NULL) {
		    found = 0;
		    curB = curWild->nsSet;
		    while (curB != NULL) {
			if (cur->value == curB->value) {
			    found = 1;
			    break;
			}
			curB = curB->next;
		    }
		    if (!found)
			break;
		    cur = cur->next;
		}
		if (found)
		    return(0);
	    } else
		return(0);
	}
    }
    /*
    * 2 If either O1 or O2 is any, then any must be the value
    */
    if (completeWild->any != curWild->any) {
	if (completeWild->any == 0) {
	    completeWild->any = 1;
	    if (completeWild->nsSet != NULL) {
		xml_xsd10_free_wildcard_ns_set(completeWild->nsSet);
		completeWild->nsSet = NULL;
	    }
	    if (completeWild->negNsSet != NULL) {
		free(completeWild->negNsSet);
		completeWild->negNsSet = NULL;
	    }
	}
	return (0);
    }
    /*
    * 3 If both O1 and O2 are sets of (namespace names or `absent`),
    * then the union of those sets must be the value.
    */
    if ((completeWild->nsSet != NULL) && (curWild->nsSet != NULL)) {
	int found;
	XmlXsd10WildcardNs *start;

	cur = curWild->nsSet;
	start = completeWild->nsSet;
	while (cur != NULL) {
	    found = 0;
	    curB = start;
	    while (curB != NULL) {
		if (cur->value == curB->value) {
		    found = 1;
		    break;
		}
		curB = curB->next;
	    }
	    if (!found) {
		tmp = xml_xsd10_new_wildcard_ns_constraint(ctxt);
		if (tmp == NULL)
		    return (-1);
		tmp->value = cur->value;
		tmp->next = completeWild->nsSet;
		completeWild->nsSet = tmp;
	    }
	    cur = cur->next;
	}

	return(0);
    }
    /*
    * 4 If the two are negations of different values (namespace names
    * or `absent`), then a pair of not and `absent` must be the value.
    */
    if ((completeWild->negNsSet != NULL) &&
	(curWild->negNsSet != NULL) &&
	(completeWild->negNsSet->value != curWild->negNsSet->value)) {
	completeWild->negNsSet->value = NULL;

	return(0);
    }
    /*
     * 5.
     */
    if (((completeWild->negNsSet != NULL) &&
	(completeWild->negNsSet->value != NULL) &&
	(curWild->nsSet != NULL)) ||
	((curWild->negNsSet != NULL) &&
	(curWild->negNsSet->value != NULL) &&
	(completeWild->nsSet != NULL))) {

	int nsFound, absentFound = 0;

	if (completeWild->nsSet != NULL) {
	    cur = completeWild->nsSet;
	    curB = curWild->negNsSet;
	} else {
	    cur = curWild->nsSet;
	    curB = completeWild->negNsSet;
	}
	nsFound = 0;
	while (cur != NULL) {
	    if (cur->value == NULL)
		absentFound = 1;
	    else if (cur->value == curB->value)
		nsFound = 1;
	    if (nsFound && absentFound)
		break;
	    cur = cur->next;
	}

	if (nsFound && absentFound) {
	    /*
	    * 5.1 If the set S includes both the negated namespace
	    * name and `absent`, then any must be the value.
	    */
	    completeWild->any = 1;
	    if (completeWild->nsSet != NULL) {
		xml_xsd10_free_wildcard_ns_set(completeWild->nsSet);
		completeWild->nsSet = NULL;
	    }
	    if (completeWild->negNsSet != NULL) {
		free(completeWild->negNsSet);
		completeWild->negNsSet = NULL;
	    }
	} else if (nsFound && (!absentFound)) {
	    /*
	    * 5.2 If the set S includes the negated namespace name
	    * but not `absent`, then a pair of not and `absent` must
	    * be the value.
	    */
	    if (completeWild->nsSet != NULL) {
		xml_xsd10_free_wildcard_ns_set(completeWild->nsSet);
		completeWild->nsSet = NULL;
	    }
	    if (completeWild->negNsSet == NULL) {
		completeWild->negNsSet = xml_xsd10_new_wildcard_ns_constraint(ctxt);
		if (completeWild->negNsSet == NULL)
		    return (-1);
	    }
	    completeWild->negNsSet->value = NULL;
	} else if ((!nsFound) && absentFound) {
	    /*
	    * 5.3 If the set S includes `absent` but not the negated
	    * namespace name, then the union is not expressible.
	    */
	    xml_xsd10_p_err(ctxt, completeWild->node,
		XML_XSD10_SCHEMAP_UNION_NOT_EXPRESSIBLE,
		"The union of the wildcard is not expressible.\n",
		NULL, NULL);
	    return(XML_XSD10_SCHEMAP_UNION_NOT_EXPRESSIBLE);
	} else if ((!nsFound) && (!absentFound)) {
	    /*
	    * 5.4 If the set S does not include either the negated namespace
	    * name or `absent`, then whichever of O1 or O2 is a pair of not
	    * and a namespace name must be the value.
	    */
	    if (completeWild->negNsSet == NULL) {
		if (completeWild->nsSet != NULL) {
		    xml_xsd10_free_wildcard_ns_set(completeWild->nsSet);
		    completeWild->nsSet = NULL;
		}
		completeWild->negNsSet = xml_xsd10_new_wildcard_ns_constraint(ctxt);
		if (completeWild->negNsSet == NULL)
		    return (-1);
		completeWild->negNsSet->value = curWild->negNsSet->value;
	    }
	}
	return (0);
    }
    /*
     * 6.
     */
    if (((completeWild->negNsSet != NULL) &&
	(completeWild->negNsSet->value == NULL) &&
	(curWild->nsSet != NULL)) ||
	((curWild->negNsSet != NULL) &&
	(curWild->negNsSet->value == NULL) &&
	(completeWild->nsSet != NULL))) {

	if (completeWild->nsSet != NULL) {
	    cur = completeWild->nsSet;
	} else {
	    cur = curWild->nsSet;
	}
	while (cur != NULL) {
	    if (cur->value == NULL) {
		/*
		* 6.1 If the set S includes `absent`, then any must be the
		* value.
		*/
		completeWild->any = 1;
		if (completeWild->nsSet != NULL) {
		    xml_xsd10_free_wildcard_ns_set(completeWild->nsSet);
		    completeWild->nsSet = NULL;
		}
		if (completeWild->negNsSet != NULL) {
		    free(completeWild->negNsSet);
		    completeWild->negNsSet = NULL;
		}
		return (0);
	    }
	    cur = cur->next;
	}
	if (completeWild->negNsSet == NULL) {
	    /*
	    * 6.2 If the set S does not include `absent`, then a pair of not
	    * and `absent` must be the value.
	    */
	    if (completeWild->nsSet != NULL) {
		xml_xsd10_free_wildcard_ns_set(completeWild->nsSet);
		completeWild->nsSet = NULL;
	    }
	    completeWild->negNsSet = xml_xsd10_new_wildcard_ns_constraint(ctxt);
	    if (completeWild->negNsSet == NULL)
		return (-1);
	    completeWild->negNsSet->value = NULL;
	}
	return (0);
    }
    return (0);

}

/**
 * Port of xmlSchemaIntersectWildcards.
 *
 * Intersects the namespace constraints of the given wildcards.
 * `completeWild` will hold the resulting intersection.
 *
 * @param ctxt  the schema parser context
 * @param completeWild  the first wildcard
 * @param curWild  the second wildcard
 * @returns a positive error code on failure, -1 in case of an
 * internal error, 0 otherwise.
 */
int xml_xsd10_intersect_wildcards(XmlXsd10ParserCtxt *ctxt, XmlXsd10Wildcard *completeWild, XmlXsd10Wildcard *curWild)
{
    XmlXsd10WildcardNs *cur, *curB, *prev, *tmp;

    /*
    * 1 If O1 and O2 are the same value, then that value must be the
    * value.
    */
    if ((completeWild->any == curWild->any) &&
	((completeWild->nsSet == NULL) == (curWild->nsSet == NULL)) &&
	((completeWild->negNsSet == NULL) == (curWild->negNsSet == NULL))) {

	if ((completeWild->negNsSet == NULL) ||
	    (completeWild->negNsSet->value == curWild->negNsSet->value)) {

	    if (completeWild->nsSet != NULL) {
		int found = 0;

		/*
		* Check equality of sets.
		*/
		cur = completeWild->nsSet;
		while (cur != NULL) {
		    found = 0;
		    curB = curWild->nsSet;
		    while (curB != NULL) {
			if (cur->value == curB->value) {
			    found = 1;
			    break;
			}
			curB = curB->next;
		    }
		    if (!found)
			break;
		    cur = cur->next;
		}
		if (found)
		    return(0);
	    } else
		return(0);
	}
    }
    /*
    * 2 If either O1 or O2 is any, then the other must be the value.
    */
    if ((completeWild->any != curWild->any) && (completeWild->any)) {
	if (xml_xsd10_clone_wildcard_ns_constraints(ctxt, completeWild, curWild) == -1)
	    return(-1);
	return(0);
    }
    /*
    * 3 If either O1 or O2 is a pair of not and a value (a namespace
    * name or `absent`) and the other is a set of (namespace names or
    * `absent`), then that set, minus the negated value if it was in
    * the set, minus `absent` if it was in the set, must be the value.
    */
    if (((completeWild->negNsSet != NULL) && (curWild->nsSet != NULL)) ||
	((curWild->negNsSet != NULL) && (completeWild->nsSet != NULL))) {
	const char *neg;

	if (completeWild->nsSet == NULL) {
	    neg = completeWild->negNsSet->value;
	    if (xml_xsd10_clone_wildcard_ns_constraints(ctxt, completeWild, curWild) == -1)
		return(-1);
	} else
	    neg = curWild->negNsSet->value;
	/*
	* Remove absent and negated.
	*/
	prev = NULL;
	cur = completeWild->nsSet;
	while (cur != NULL) {
	    if (cur->value == NULL) {
		if (prev == NULL)
		    completeWild->nsSet = cur->next;
		else
		    prev->next = cur->next;
		free(cur);
		break;
	    }
	    prev = cur;
	    cur = cur->next;
	}
	if (neg != NULL) {
	    prev = NULL;
	    cur = completeWild->nsSet;
	    while (cur != NULL) {
		if (cur->value == neg) {
		    if (prev == NULL)
			completeWild->nsSet = cur->next;
		    else
			prev->next = cur->next;
		    free(cur);
		    break;
		}
		prev = cur;
		cur = cur->next;
	    }
	}

	return(0);
    }
    /*
    * 4 If both O1 and O2 are sets of (namespace names or `absent`),
    * then the intersection of those sets must be the value.
    */
    if ((completeWild->nsSet != NULL) && (curWild->nsSet != NULL)) {
	int found;

	cur = completeWild->nsSet;
	prev = NULL;
	while (cur != NULL) {
	    found = 0;
	    curB = curWild->nsSet;
	    while (curB != NULL) {
		if (cur->value == curB->value) {
		    found = 1;
		    break;
		}
		curB = curB->next;
	    }
	    if (!found) {
		if (prev == NULL)
		    completeWild->nsSet = cur->next;
		else
		    prev->next = cur->next;
		tmp = cur->next;
		free(cur);
		cur = tmp;
		continue;
	    }
	    prev = cur;
	    cur = cur->next;
	}

	return(0);
    }
    /* 5 If the two are negations of different namespace names,
    * then the intersection is not expressible
    */
    if ((completeWild->negNsSet != NULL) &&
	(curWild->negNsSet != NULL) &&
	(completeWild->negNsSet->value != curWild->negNsSet->value) &&
	(completeWild->negNsSet->value != NULL) &&
	(curWild->negNsSet->value != NULL)) {

	xml_xsd10_p_err(ctxt, completeWild->node, XML_XSD10_SCHEMAP_INTERSECTION_NOT_EXPRESSIBLE,
	    "The intersection of the wildcard is not expressible.\n",
	    NULL, NULL);
	return(XML_XSD10_SCHEMAP_INTERSECTION_NOT_EXPRESSIBLE);
    }
    /*
    * 6 If the one is a negation of a namespace name and the other
    * is a negation of `absent`, then the one which is the negation
    * of a namespace name must be the value.
    */
    if ((completeWild->negNsSet != NULL) && (curWild->negNsSet != NULL) &&
	(completeWild->negNsSet->value != curWild->negNsSet->value) &&
	(completeWild->negNsSet->value == NULL)) {
	completeWild->negNsSet->value =  curWild->negNsSet->value;
    }
    return(0);
}

/**
 * Port of xmlSchemaCheckCOSNSSubset.
 *
 * Schema Component Constraint: Wildcard Subset (cos-ns-subset)
 *
 * @param sub  the first wildcard
 * @param super  the second wildcard
 * @returns 0 if the namespace constraint of `sub` is an intensional
 * subset of `super`, 1 otherwise.
 */
int xml_xsd10_check_cosns_subset(XmlXsd10Wildcard *sub, XmlXsd10Wildcard *super)
{
    /*
    * 1 super must be any.
    */
    if (super->any)
	return (0);
    /*
    * 2.1 sub must be a pair of not and a namespace name or `absent`.
    * 2.2 super must be a pair of not and the same value.
    */
    if ((sub->negNsSet != NULL) &&
	(super->negNsSet != NULL) &&
	(sub->negNsSet->value == super->negNsSet->value))
	return (0);
    /*
    * 3.1 sub must be a set whose members are either namespace names or `absent`.
    */
    if (sub->nsSet != NULL) {
	/*
	* 3.2.1 super must be the same set or a superset thereof.
	*/
	if (super->nsSet != NULL) {
	    XmlXsd10WildcardNs *cur, *curB;
	    int found = 0;

	    cur = sub->nsSet;
	    while (cur != NULL) {
		found = 0;
		curB = super->nsSet;
		while (curB != NULL) {
		    if (cur->value == curB->value) {
			found = 1;
			break;
		    }
		    curB = curB->next;
		}
		if (!found)
		    return (1);
		cur = cur->next;
	    }
	    if (found)
		return (0);
	} else if (super->negNsSet != NULL) {
	    XmlXsd10WildcardNs *cur;
	    /*
	    * 3.2.2 super must be a pair of not and a namespace name or
	    * `absent` and that value must not be in sub's set.
	    */
	    cur = sub->nsSet;
	    while (cur != NULL) {
		if (cur->value == super->negNsSet->value)
		    return (1);
		cur = cur->next;
	    }
	    return (0);
	}
    }
    return (1);
}

/** Port of xmlSchemaGetEffectiveValueConstraint. */
int xml_xsd10_get_effective_value_constraint(XmlXsd10AttributeUse *attruse, int *fixed, const char **value, XmlXsd10Val **val)
{
    *fixed = 0;
    *value = NULL;
    if (val != 0)
	*val = NULL;

    if (attruse->defValue != NULL) {
	*value = attruse->defValue;
	if (val != NULL)
	    *val = attruse->defVal;
	if (attruse->flags & XML_XSD10_SCHEMA_ATTR_USE_FIXED)
	    *fixed = 1;
	return(1);
    } else if ((attruse->attrDecl != NULL) &&
	(attruse->attrDecl->defValue != NULL)) {
	*value = attruse->attrDecl->defValue;
	if (val != NULL)
	    *val = attruse->attrDecl->defVal;
	if (attruse->attrDecl->flags & XML_XSD10_SCHEMAS_ATTR_FIXED)
	    *fixed = 1;
	return(1);
    }
    return(0);
}
/**
 * Port of xmlSchemaCheckCVCWildcardNamespace.
 *
 * Validation Rule: Wildcard allows Namespace Name
 * (cvc-wildcard-namespace)
 *
 * @param wild  the wildcard
 * @param ns  the namespace
 * @returns 0 if the given namespace matches the wildcard,
 * 1 otherwise and -1 on API errors.
 */
int xml_xsd10_check_cvc_wildcard_namespace(XmlXsd10Wildcard *wild, const char *ns)
{
    if (wild == NULL)
	return(-1);

    if (wild->any)
	return(0);
    else if (wild->nsSet != NULL) {
	XmlXsd10WildcardNs *cur;

	cur = wild->nsSet;
	while (cur != NULL) {
	    if (xml_xsd10_str_equal(cur->value, ns))
		return(0);
	    cur = cur->next;
	}
    } else if ((wild->negNsSet != NULL) && (ns != NULL) &&
	(!xml_xsd10_str_equal(wild->negNsSet->value, ns)))
	return(0);

    return(1);
}

#define XML_XSD10_SCHEMA_ACTION_DERIVE 0
#define XML_XSD10_SCHEMA_ACTION_REDEFINE 1

#define XSD10_WXS_ACTION_STR(a) \
((a) == XML_XSD10_SCHEMA_ACTION_DERIVE) ? (const char *) "base" : (const char *) "redefined"

/*
 * Port of xmlSchemaCheckDerivationOKRestriction2to4.
 *
* Schema Component Constraint:
*   Derivation Valid (Restriction, Complex)
*   derivation-ok-restriction (2) - (4)
*
* ATTENTION:
* In XML Schema 1.1 this will be:
* Validation Rule:
*     Checking complex type subsumption (practicalSubsumption) (1, 2 and 3)
*
*/
int xml_xsd10_check_derivation_ok_restriction2to4(XmlXsd10ParserCtxt *pctxt, int action, XmlXsd10BasicItem *item, XmlXsd10BasicItem *baseItem, XmlXsd10ItemList *uses, XmlXsd10ItemList *baseUses, XmlXsd10Wildcard *wild, XmlXsd10Wildcard *baseWild)
{
    XmlXsd10AttributeUse *cur = NULL, *bcur;
    int i, j, found; /* err = 0; */
    const char *bEffValue;
    int effFixed;

    if (uses != NULL) {
	for (i = 0; i < uses->nbItems; i++) {
	    cur = (XmlXsd10AttributeUse *) uses->items[i];
	    found = 0;
	    if (baseUses == NULL)
		goto not_found;
	    for (j = 0; j < baseUses->nbItems; j++) {
		bcur = (XmlXsd10AttributeUse *) baseUses->items[j];
		if ((XSD10_WXS_ATTRUSE_DECL_NAME(cur) ==
			XSD10_WXS_ATTRUSE_DECL_NAME(bcur)) &&
		    (XSD10_WXS_ATTRUSE_DECL_TNS(cur) ==
			XSD10_WXS_ATTRUSE_DECL_TNS(bcur)))
		{
		    /*
		    * (2.1) "If there is an attribute use in the {attribute
		    * uses} of the {base type definition} (call this B) whose
		    * {attribute declaration} has the same {name} and {target
		    * namespace}, then  all of the following must be true:"
		    */
		    found = 1;

		    if ((cur->occurs == XML_XSD10_SCHEMAS_ATTR_USE_OPTIONAL) &&
			(bcur->occurs == XML_XSD10_SCHEMAS_ATTR_USE_REQUIRED))
		    {
			char *str = NULL;
			/*
			* (2.1.1) "one of the following must be true:"
			* (2.1.1.1) "B's {required} is false."
			* (2.1.1.2) "R's {required} is true."
			*/
			xml_xsd10_p_attr_use_err4(pctxt,
			    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_2_1_1,
			    XSD10_WXS_ITEM_NODE(item), item, cur,
			    "The 'optional' attribute use is inconsistent "
			    "with the corresponding 'required' attribute use of "
			    "the %s %s",
			    XSD10_WXS_ACTION_STR(action),
			    xml_xsd10_get_component_designation(&str, baseItem),
			    NULL, NULL);
			XSD10_FREE_AND_NULL(str);
			/* err = pctxt->err; */
		    } else if (xml_xsd10_check_cosst_derived_ok(XSD10_ACTXT_CAST pctxt,
			XSD10_WXS_ATTRUSE_TYPEDEF(cur),
			XSD10_WXS_ATTRUSE_TYPEDEF(bcur), 0) != 0)
		    {
			char *strA = NULL, *strB = NULL, *strC = NULL;

			/*
			* SPEC (2.1.2) "R's {attribute declaration}'s
			* {type definition} must be validly derived from
			* B's {type definition} given the empty set as
			* defined in Type Derivation OK (Simple) ($3.14.6)."
			*/
			xml_xsd10_p_attr_use_err4(pctxt,
			    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_2_1_2,
			    XSD10_WXS_ITEM_NODE(item), item, cur,
			    "The attribute declaration's %s "
			    "is not validly derived from "
			    "the corresponding %s of the "
			    "attribute declaration in the %s %s",
			    xml_xsd10_get_component_designation(&strA,
				XSD10_WXS_ATTRUSE_TYPEDEF(cur)),
			    xml_xsd10_get_component_designation(&strB,
				XSD10_WXS_ATTRUSE_TYPEDEF(bcur)),
			    XSD10_WXS_ACTION_STR(action),
			    xml_xsd10_get_component_designation(&strC, baseItem));
			    /* xmlSchemaGetComponentDesignation(&str, baseItem), */
			XSD10_FREE_AND_NULL(strA);
			XSD10_FREE_AND_NULL(strB);
			XSD10_FREE_AND_NULL(strC);
			/* err = pctxt->err; */
		    } else {
			/*
			* 2.1.3 [Definition:]  Let the effective value
			* constraint of an attribute use be its {value
			* constraint}, if present, otherwise its {attribute
			* declaration}'s {value constraint} .
			*/
			xml_xsd10_get_effective_value_constraint(bcur,
			    &effFixed, &bEffValue, NULL);
			/*
			* 2.1.3 ... one of the following must be true
			*
			* 2.1.3.1 B's `effective value constraint` is
			* `absent` or default.
			*/
			if ((bEffValue != NULL) &&
			    (effFixed == 1)) {
			    const char *rEffValue = NULL;

			    xml_xsd10_get_effective_value_constraint(bcur,
				&effFixed, &rEffValue, NULL);
			    /*
			    * 2.1.3.2 R's `effective value constraint` is
			    * fixed with the same string as B's.
			    * MAYBE TODO: Compare the computed values.
			    *       Hmm, it says "same string" so
			    *       string-equality might really be sufficient.
			    */
			    if ((effFixed == 0) ||
				(! XSD10_WXS_ARE_DEFAULT_STR_EQUAL(rEffValue, bEffValue)))
			    {
				char *str = NULL;

				xml_xsd10_p_attr_use_err4(pctxt,
				    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_2_1_3,
				    XSD10_WXS_ITEM_NODE(item), item, cur,
				    "The effective value constraint of the "
				    "attribute use is inconsistent with "
				    "its correspondent in the %s %s",
				    XSD10_WXS_ACTION_STR(action),
				    xml_xsd10_get_component_designation(&str,
					baseItem),
				    NULL, NULL);
				XSD10_FREE_AND_NULL(str);
				/* err = pctxt->err; */
			    }
			}
		    }
		    break;
		}
	    }
not_found:
	    if (!found) {
		/*
		* (2.2) "otherwise the {base type definition} must have an
		* {attribute wildcard} and the {target namespace} of the
		* R's {attribute declaration} must be `valid` with respect
		* to that wildcard, as defined in Wildcard allows Namespace
		* Name ($3.10.4)."
		*/
		if ((baseWild == NULL) ||
		    (xml_xsd10_check_cvc_wildcard_namespace(baseWild,
		    (XSD10_WXS_ATTRUSE_DECL(cur))->targetNamespace) != 0))
		{
		    char *str = NULL;

		    xml_xsd10_p_attr_use_err4(pctxt,
			XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_2_2,
			XSD10_WXS_ITEM_NODE(item), item, cur,
			"Neither a matching attribute use, "
			"nor a matching wildcard exists in the %s %s",
			XSD10_WXS_ACTION_STR(action),
			xml_xsd10_get_component_designation(&str, baseItem),
			NULL, NULL);
		    XSD10_FREE_AND_NULL(str);
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
    if (baseUses != NULL) {
	for (j = 0; j < baseUses->nbItems; j++) {
	    bcur = (XmlXsd10AttributeUse *) baseUses->items[j];
	    if (bcur->occurs != XML_XSD10_SCHEMAS_ATTR_USE_REQUIRED)
		continue;
	    found = 0;
	    if (uses != NULL) {
		for (i = 0; i < uses->nbItems; i++) {
		    cur = (XmlXsd10AttributeUse *) uses->items[i];
		    if ((XSD10_WXS_ATTRUSE_DECL_NAME(cur) ==
			XSD10_WXS_ATTRUSE_DECL_NAME(bcur)) &&
			(XSD10_WXS_ATTRUSE_DECL_TNS(cur) ==
			XSD10_WXS_ATTRUSE_DECL_TNS(bcur))) {
			found = 1;
			break;
		    }
		}
	    }
	    if (!found) {
		char *strA = NULL, *strB = NULL;

		xml_xsd10_custom_err4(XSD10_ACTXT_CAST pctxt,
		    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_3,
		    NULL, item,
		    "A matching attribute use for the "
		    "'required' %s of the %s %s is missing",
		    xml_xsd10_get_component_designation(&strA, bcur),
		    XSD10_WXS_ACTION_STR(action),
		    xml_xsd10_get_component_designation(&strB, baseItem),
		    NULL);
		XSD10_FREE_AND_NULL(strA);
		XSD10_FREE_AND_NULL(strB);
	    }
	}
    }
    /*
    * derivation-ok-restriction (4)
    */
    if (wild != NULL) {
	/*
	* (4) "If there is an {attribute wildcard}, all of the
	* following must be true:"
	*/
	if (baseWild == NULL) {
	    char *str = NULL;

	    /*
	    * (4.1) "The {base type definition} must also have one."
	    */
	    xml_xsd10_custom_err4(XSD10_ACTXT_CAST pctxt,
		XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_4_1,
		NULL, item,
		"The %s has an attribute wildcard, "
		"but the %s %s '%s' does not have one",
		XSD10_WXS_ITEM_TYPE_NAME(item),
		XSD10_WXS_ACTION_STR(action),
		XSD10_WXS_ITEM_TYPE_NAME(baseItem),
		xml_xsd10_get_component_q_name(&str, baseItem));
	    XSD10_FREE_AND_NULL(str);
	    return(pctxt->err);
	} else if ((baseWild->any == 0) &&
		xml_xsd10_check_cosns_subset(wild, baseWild))
	{
	    char *str = NULL;
	    /*
	    * (4.2) "The complex type definition's {attribute wildcard}'s
	    * {namespace constraint} must be a subset of the {base type
	    * definition}'s {attribute wildcard}'s {namespace constraint},
	    * as defined by Wildcard Subset ($3.10.6)."
	    */
	    xml_xsd10_custom_err4(XSD10_ACTXT_CAST pctxt,
		XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_4_2,
		NULL, item,
		"The attribute wildcard is not a valid "
		"subset of the wildcard in the %s %s '%s'",
		XSD10_WXS_ACTION_STR(action),
		XSD10_WXS_ITEM_TYPE_NAME(baseItem),
		xml_xsd10_get_component_q_name(&str, baseItem),
		NULL);
	    XSD10_FREE_AND_NULL(str);
	    return(pctxt->err);
	}
	/* 4.3 Unless the {base type definition} is the `ur-type
	* definition`, the complex type definition's {attribute
	* wildcard}'s {process contents} must be identical to or
	* stronger than the {base type definition}'s {attribute
	* wildcard}'s {process contents}, where strict is stronger
	* than lax is stronger than skip.
	*/
	if ((! XSD10_WXS_IS_ANYTYPE(baseItem)) &&
	    (wild->processContents < baseWild->processContents)) {
	    char *str = NULL;
	    xml_xsd10_custom_err4(XSD10_ACTXT_CAST pctxt,
		XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_4_3,
		NULL, baseItem,
		"The {process contents} of the attribute wildcard is "
		"weaker than the one in the %s %s '%s'",
		XSD10_WXS_ACTION_STR(action),
		XSD10_WXS_ITEM_TYPE_NAME(baseItem),
		xml_xsd10_get_component_q_name(&str, baseItem),
		NULL);
	    XSD10_FREE_AND_NULL(str)
		return(pctxt->err);
	}
    }
    return(0);
}


/**
 * Port of xmlSchemaFixupTypeAttributeUses.
 *
 * Builds the wildcard and the attribute uses on the given complex type.
 *
 *
 * ATTENTION TODO: Experimentally this uses pointer comparisons for
 * strings, so recheck this if we start to hardcode some schemata, since
 * they might not be in the same dict.
 * NOTE: It is allowed to "extend" the xs:anyType type.
 *
 * @param pctxt  the schema parser context
 * @param type  the complex type definition
 * @returns -1 if an internal error occurs, 0 otherwise.
 */
int xml_xsd10_fixup_type_attribute_uses(XmlXsd10ParserCtxt *pctxt, XmlXsd10Type *type)
{
    XmlXsd10Type *baseType = NULL;
    XmlXsd10AttributeUse *use;
    XmlXsd10ItemList *uses, *baseUses, *prohibs = NULL;

    if (type->baseType == NULL) {
	XSD10_PERROR_INT("xmlSchemaFixupTypeAttributeUses",
	    "no base type");
        return (-1);
    }
    baseType = type->baseType;
    if (XSD10_WXS_IS_TYPE_NOT_FIXED(baseType))
	if (xml_xsd10_type_fixup(baseType, XSD10_ACTXT_CAST pctxt) == -1)
	    return(-1);

    uses = (XmlXsd10ItemList *) type->attrUses;
    baseUses = (XmlXsd10ItemList *) baseType->attrUses;
    /*
    * Expand attribute group references. And build the 'complete'
    * wildcard, i.e. intersect multiple wildcards.
    * Move attribute prohibitions into a separate list.
    */
    if (uses != NULL) {
	if (XSD10_WXS_IS_RESTRICTION(type)) {
	    /*
	    * This one will transfer all attr. prohibitions
	    * into pctxt->attrProhibs.
	    */
	    if (xml_xsd10_expand_attribute_group_refs(pctxt,
		XSD10_WXS_BASIC_CAST type, &(type->attributeWildcard), uses,
		pctxt->attrProhibs) == -1)
	    {
		XSD10_PERROR_INT("xmlSchemaFixupTypeAttributeUses",
		"failed to expand attributes");
                return(-1);
	    }
	    if (pctxt->attrProhibs->nbItems != 0)
		prohibs = pctxt->attrProhibs;
	} else {
	    if (xml_xsd10_expand_attribute_group_refs(pctxt,
		XSD10_WXS_BASIC_CAST type, &(type->attributeWildcard), uses,
		NULL) == -1)
	    {
		XSD10_PERROR_INT("xmlSchemaFixupTypeAttributeUses",
		"failed to expand attributes");
                return(-1);
	    }
	}
    }
    /*
    * Inherit the attribute uses of the base type.
    */
    if (baseUses != NULL) {
	int i, j;
	XmlXsd10AttributeUseProhib *pro;

	if (XSD10_WXS_IS_RESTRICTION(type)) {
	    int usesCount;
	    XmlXsd10AttributeUse *tmp;

	    if (uses != NULL)
		usesCount = uses->nbItems;
	    else
		usesCount = 0;

	    /* Restriction. */
	    for (i = 0; i < baseUses->nbItems; i++) {
		use = (XmlXsd10AttributeUse *) baseUses->items[i];
		if (prohibs) {
		    /*
		    * Filter out prohibited uses.
		    */
		    for (j = 0; j < prohibs->nbItems; j++) {
			pro = (XmlXsd10AttributeUseProhib *) prohibs->items[j];
			if ((XSD10_WXS_ATTRUSE_DECL_NAME(use) == pro->name) &&
			    (XSD10_WXS_ATTRUSE_DECL_TNS(use) ==
				pro->targetNamespace))
			{
			    goto inherit_next;
			}
		    }
		}
		if (usesCount) {
		    /*
		    * Filter out existing uses.
		    */
		    for (j = 0; j < usesCount; j++) {
			tmp = (XmlXsd10AttributeUse *) uses->items[j];
			if ((XSD10_WXS_ATTRUSE_DECL_NAME(use) ==
				XSD10_WXS_ATTRUSE_DECL_NAME(tmp)) &&
			    (XSD10_WXS_ATTRUSE_DECL_TNS(use) ==
				XSD10_WXS_ATTRUSE_DECL_TNS(tmp)))
			{
			    goto inherit_next;
			}
		    }
		}
		if (uses == NULL) {
		    type->attrUses = xml_xsd10_item_list_create();
		    if (type->attrUses == NULL)
			goto exit_failure;
		    uses = (XmlXsd10ItemList *) type->attrUses;
		}
		xml_xsd10_item_list_add_size(uses, 2, use);
inherit_next: {}
	    }
	} else {
	    /* Extension. */
	    for (i = 0; i < baseUses->nbItems; i++) {
		use = (XmlXsd10AttributeUse *) baseUses->items[i];
		if (uses == NULL) {
		    type->attrUses = xml_xsd10_item_list_create();
		    if (type->attrUses == NULL)
			goto exit_failure;
		    uses = (XmlXsd10ItemList *) type->attrUses;
		}
		xml_xsd10_item_list_add_size(uses, baseUses->nbItems, use);
	    }
	}
    }
    /*
    * Shrink attr. uses.
    */
    if (uses) {
	if (uses->nbItems == 0) {
	    xml_xsd10_item_list_free(uses);
	    type->attrUses = NULL;
	}
	/*
	* TODO: We could shrink the size of the array
	* to fit the actual number of items.
	*/
    }
    /*
    * Compute the complete wildcard.
    */
    if (XSD10_WXS_IS_EXTENSION(type)) {
	if (baseType->attributeWildcard != NULL) {
	    /*
	    * (3.2.2.1) "If the `base wildcard` is non-`absent`, then
	    * the appropriate case among the following:"
	    */
	    if (type->attributeWildcard != NULL) {
		/*
		* Union the complete wildcard with the base wildcard.
		* SPEC {attribute wildcard}
		* (3.2.2.1.2) "otherwise a wildcard whose {process contents}
		* and {annotation} are those of the `complete wildcard`,
		* and whose {namespace constraint} is the intensional union
		* of the {namespace constraint} of the `complete wildcard`
		* and of the `base wildcard`, as defined in Attribute
		* Wildcard Union ($3.10.6)."
		*/
		if (xml_xsd10_union_wildcards(pctxt, type->attributeWildcard,
		    baseType->attributeWildcard) == -1)
		    goto exit_failure;
	    } else {
		/*
		* (3.2.2.1.1) "If the `complete wildcard` is `absent`,
		* then the `base wildcard`."
		*/
		type->attributeWildcard = baseType->attributeWildcard;
	    }
	} else {
	    /*
	    * (3.2.2.2) "otherwise (the `base wildcard` is `absent`) the
	    * `complete wildcard`"
	    * NOOP
	    */
	}
    } else {
	/*
	* SPEC {attribute wildcard}
	* (3.1) "If the <restriction> alternative is chosen, then the
	* `complete wildcard`;"
	* NOOP
	*/
    }

    return (0);

exit_failure:
    return(-1);
}

/**
 * Port of xmlSchemaTypeFinalContains.
 *
 * Evaluates if a type definition contains the given "final".
 * This does take "finalDefault" into account as well.
 *
 * @param type  the type definition
 * @param final  the final
 * @returns 1 if the type does contain the given "final",
 * 0 otherwise.
 */
int xml_xsd10_type_final_contains(XmlXsd10Type *type, int final)
{
    if (type == NULL)
	return (0);
    if (type->flags & final)
	return (1);
    else
	return (0);
}

/**
 * Port of xmlSchemaGetUnionSimpleTypeMemberTypes.
 *
 * @param type  the Union Simple Type
 * @returns  a list of member types of `type` if existing,
 *           NULL otherwise.
 */
XmlXsd10TypeLink *xml_xsd10_get_union_simple_type_member_types(XmlXsd10Type *type)
{
    while ((type != NULL) && (type->type == XML_XSD10_SCHEMA_TYPE_SIMPLE)) {
	if (type->memberTypes != NULL)
	    return (type->memberTypes);
	else
	    type = type->baseType;
    }
    return (NULL);
}


/**
 * Port of xmlSchemaGetParticleEmptiable.
 *
 * @param particle  the particle
 * @returns 1 if emptiable, 0 otherwise.
 */
int xml_xsd10_get_particle_emptiable(XmlXsd10Particle *particle)
{
    XmlXsd10Particle *part;
    int emptiable;

    if ((particle->children == NULL) || (particle->minOccurs == 0))
	return (1);

    part = (XmlXsd10Particle *) particle->children->children;
    if (part == NULL)
        return (1);

    while (part != NULL) {
        if ((part->children->type == XML_XSD10_SCHEMA_TYPE_ELEMENT) ||
            (part->children->type == XML_XSD10_SCHEMA_TYPE_ANY))
            emptiable = (part->minOccurs == 0);
        else
            emptiable = xml_xsd10_get_particle_emptiable(part);
        if (particle->children->type == XML_XSD10_SCHEMA_TYPE_CHOICE) {
            if (emptiable)
                return (1);
        } else {
	    /* <all> and <sequence> */
            if (!emptiable)
                return (0);
        }
        part = (XmlXsd10Particle *) part->next;
    }

    if (particle->children->type == XML_XSD10_SCHEMA_TYPE_CHOICE)
        return (0);
    else
        return (1);
}

/**
 * Port of xmlSchemaIsParticleEmptiable.
 *
 * Schema Component Constraint: Particle Emptiable
 * Checks whether the given particle is emptiable.
 *
 * @param particle  the particle
 * @returns 1 if emptiable, 0 otherwise.
 */
int xml_xsd10_is_particle_emptiable(XmlXsd10Particle *particle)
{
    /*
    * SPEC (1) "Its {min occurs} is 0."
    */
    if ((particle == NULL) || (particle->minOccurs == 0) ||
	(particle->children == NULL))
	return (1);
    /*
    * SPEC (2) "Its {term} is a group and the minimum part of the
    * effective total range of that group, [...] is 0."
    */
    if (XSD10_WXS_IS_MODEL_GROUP(particle->children))
	return (xml_xsd10_get_particle_emptiable(particle));
    return (0);
}

/**
 * Port of xmlSchemaCheckCOSSTDerivedOK.
 *
 * Schema Component Constraint:
 * Type Derivation OK (Simple) (cos-st-derived-OK)
 *
 * Checks whether `type` can be validly
 * derived from `baseType`.
 *
 * @param actxt  a context
 * @param type  the derived simple type definition
 * @param baseType  the base type definition
 * @param subset  the subset of ('restriction', etc.)
 * @returns 0 on success, an positive error code otherwise.
 */
int xml_xsd10_check_cosst_derived_ok(XmlXsd10AbstractCtxt *actxt, XmlXsd10Type *type, XmlXsd10Type *baseType, int subset)
{
    /*
    * 1 They are the same type definition.
    * TODO: The identity check might have to be more complex than this.
    */
    if (type == baseType)
	return (0);
    /*
    * 2.1 restriction is not in the subset, or in the {final}
    * of its own {base type definition};
    *
    * NOTE that this will be used also via "xsi:type".
    *
    * TODO: Revise this, it looks strange. How can the "type"
    * not be fixed or *in* fixing?
    */
    if (XSD10_WXS_IS_TYPE_NOT_FIXED(type))
	if (xml_xsd10_type_fixup(type, actxt) == -1)
	    return(-1);
    if (XSD10_WXS_IS_TYPE_NOT_FIXED(baseType))
	if (xml_xsd10_type_fixup(baseType, actxt) == -1)
	    return(-1);
    if ((subset & XSD10_SUBSET_RESTRICTION) ||
	(xml_xsd10_type_final_contains(type->baseType,
	    XML_XSD10_SCHEMAS_TYPE_FINAL_RESTRICTION))) {
	return (XML_XSD10_SCHEMAP_COS_ST_DERIVED_OK_2_1);
    }
    /* 2.2 */
    if (type->baseType == baseType) {
	/*
	* 2.2.1 D's `base type definition` is B.
	*/
	return (0);
    }
    /*
    * 2.2.2 D's `base type definition` is not the `ur-type definition`
    * and is validly derived from B given the subset, as defined by this
    * constraint.
    */
    if ((! XSD10_WXS_IS_ANYTYPE(type->baseType)) &&
	(xml_xsd10_check_cosst_derived_ok(actxt, type->baseType,
	    baseType, subset) == 0)) {
	return (0);
    }
    /*
    * 2.2.3 D's {variety} is list or union and B is the `simple ur-type
    * definition`.
    */
    if (XSD10_WXS_IS_ANY_SIMPLE_TYPE(baseType) &&
	(XSD10_WXS_IS_LIST(type) || XSD10_WXS_IS_UNION(type))) {
	return (0);
    }
    /*
    * 2.2.4 B's {variety} is union and D is validly derived from a type
    * definition in B's {member type definitions} given the subset, as
    * defined by this constraint.
    *
    * NOTE: This seems not to involve built-in types, since there is no
    * built-in Union Simple Type.
    */
    if (XSD10_WXS_IS_UNION(baseType)) {
	XmlXsd10TypeLink *cur;

	cur = baseType->memberTypes;
	while (cur != NULL) {
	    if (XSD10_WXS_IS_TYPE_NOT_FIXED(cur->type))
		if (xml_xsd10_type_fixup(cur->type, actxt) == -1)
		    return(-1);
	    if (xml_xsd10_check_cosst_derived_ok(actxt,
		    type, cur->type, subset) == 0)
	    {
		/*
		* It just has to be validly derived from at least one
		* member-type.
		*/
		return (0);
	    }
	    cur = cur->next;
	}
    }
    return (XML_XSD10_SCHEMAP_COS_ST_DERIVED_OK_2_2);
}

/**
 * Port of xmlSchemaCheckTypeDefCircularInternal.
 *
 * Checks st-props-correct (2) + ct-props-correct (3).
 * Circular type definitions are not allowed.
 *
 * @param pctxt  the schema parser context
 * @param ctxtType  the type definition
 * @param ancestor  an ancestor of `ctxtType`
 * @returns XML_SCHEMAP_ST_PROPS_CORRECT_2 if the given type is
 * circular, 0 otherwise.
 */
int xml_xsd10_check_type_def_circular_internal(XmlXsd10ParserCtxt *pctxt, XmlXsd10Type *ctxtType, XmlXsd10Type *ancestor)
{
    int ret;

    if ((ancestor == NULL) || (ancestor->type == XML_XSD10_SCHEMA_TYPE_BASIC))
	return (0);

    if (ctxtType == ancestor) {
	xml_xsd10_p_custom_err(pctxt,
	    XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_2,
	    XSD10_WXS_BASIC_CAST ctxtType, XSD10_WXS_ITEM_NODE(ctxtType),
	    "The definition is circular", NULL);
	return (XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_2);
    }
    if (ancestor->flags & XML_XSD10_SCHEMAS_TYPE_MARKED) {
	/*
	* Avoid infinite recursion on circular types not yet checked.
	*/
	return (0);
    }
    ancestor->flags |= XML_XSD10_SCHEMAS_TYPE_MARKED;
    ret = xml_xsd10_check_type_def_circular_internal(pctxt, ctxtType,
	ancestor->baseType);
    ancestor->flags ^= XML_XSD10_SCHEMAS_TYPE_MARKED;
    return (ret);
}

/**
 * Port of xmlSchemaCheckTypeDefCircular.
 *
 * Checks for circular type definitions.
 *
 * @param item  the complex/simple type definition
 * @param ctxt  the parser context
 */
void xml_xsd10_check_type_def_circular(XmlXsd10Type *item, XmlXsd10ParserCtxt *ctxt)
{
    if ((item == NULL) ||
	(item->type == XML_XSD10_SCHEMA_TYPE_BASIC) ||
	(item->baseType == NULL))
	return;
    xml_xsd10_check_type_def_circular_internal(ctxt, item,
	item->baseType);
}

/*
 * Port of xmlSchemaCheckUnionTypeDefCircularRecur.
 *
* Simple Type Definition Representation OK (src-simple-type) 4
*
* "4 Circular union type definition is disallowed. That is, if the
* <union> alternative is chosen, there must not be any entries in the
* memberTypes [attribute] at any depth which resolve to the component
* corresponding to the <simpleType>."
*
* Note that this should work on the *representation* of a component,
* thus assumes any union types in the member types not being yet
* substituted. At this stage we need the variety of the types
* to be already computed.
*/
int xml_xsd10_check_union_type_def_circular_recur(XmlXsd10ParserCtxt *pctxt, XmlXsd10Type *ctxType, XmlXsd10TypeLink *members)
{
    XmlXsd10TypeLink *member;
    XmlXsd10Type *memberType;

    member = members;
    while (member != NULL) {
	memberType = member->type;
	while ((memberType != NULL) &&
	    (memberType->type != XML_XSD10_SCHEMA_TYPE_BASIC)) {
	    if (memberType == ctxType) {
		xml_xsd10_p_custom_err(pctxt,
		    XML_XSD10_SCHEMAP_SRC_SIMPLE_TYPE_4,
		    XSD10_WXS_BASIC_CAST ctxType, NULL,
		    "The union type definition is circular", NULL);
		return (XML_XSD10_SCHEMAP_SRC_SIMPLE_TYPE_4);
	    }
	    if ((XSD10_WXS_IS_UNION(memberType)) &&
		((memberType->flags & XML_XSD10_SCHEMAS_TYPE_MARKED) == 0))
	    {
		int res;
		memberType->flags |= XML_XSD10_SCHEMAS_TYPE_MARKED;
		res = xml_xsd10_check_union_type_def_circular_recur(pctxt,
		    ctxType,
		    xml_xsd10_get_union_simple_type_member_types(memberType));
		memberType->flags ^= XML_XSD10_SCHEMAS_TYPE_MARKED;
		if (res != 0)
		    return(res);
	    }
	    memberType = memberType->baseType;
	}
	member = member->next;
    }
    return(0);
}

/** Port of xmlSchemaCheckUnionTypeDefCircular. */
int xml_xsd10_check_union_type_def_circular(XmlXsd10ParserCtxt *pctxt, XmlXsd10Type *type)
{
    if (! XSD10_WXS_IS_UNION(type))
	return(0);
    return(xml_xsd10_check_union_type_def_circular_recur(pctxt, type,
	type->memberTypes));
}

/**
 * Port of xmlSchemaResolveTypeReferences.
 *
 * Resolves type definition references
 *
 * @param typeDef  the complex/simple type definition
 * @param ctxt  the parser context
 */
void xml_xsd10_resolve_type_references(XmlXsd10Type *typeDef, XmlXsd10ParserCtxt *ctxt)
{
    if (typeDef == NULL)
	return;

    /*
    * Resolve the base type.
    */
    if (typeDef->baseType == NULL) {
	typeDef->baseType = xml_xsd10_get_type(ctxt->schema,
	    typeDef->base, typeDef->baseNs);
	if (typeDef->baseType == NULL) {
	    xml_xsd10_p_res_comp_attr_err(ctxt,
		XML_XSD10_SCHEMAP_SRC_RESOLVE,
		XSD10_WXS_BASIC_CAST typeDef, typeDef->node,
		"base", typeDef->base, typeDef->baseNs,
		XML_XSD10_SCHEMA_TYPE_SIMPLE, NULL);
	    return;
	}
    }
    if (XSD10_WXS_IS_SIMPLE(typeDef)) {
	if (XSD10_WXS_IS_UNION(typeDef)) {
	    /*
	    * Resolve the memberTypes.
	    */
	    xml_xsd10_resolve_union_member_types(ctxt, typeDef);
	    return;
	} else if (XSD10_WXS_IS_LIST(typeDef)) {
	    /*
	    * Resolve the itemType.
	    */
	    if ((typeDef->subtypes == NULL) && (typeDef->base != NULL)) {

		typeDef->subtypes = xml_xsd10_get_type(ctxt->schema,
		    typeDef->base, typeDef->baseNs);

		if ((typeDef->subtypes == NULL) ||
		    (! XSD10_WXS_IS_SIMPLE(typeDef->subtypes)))
		{
		    typeDef->subtypes = NULL;
		    xml_xsd10_p_res_comp_attr_err(ctxt,
			XML_XSD10_SCHEMAP_SRC_RESOLVE,
			XSD10_WXS_BASIC_CAST typeDef, typeDef->node,
			"itemType", typeDef->base, typeDef->baseNs,
			XML_XSD10_SCHEMA_TYPE_SIMPLE, NULL);
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
    else if ((XSD10_WXS_TYPE_CONTENTTYPE(typeDef) != NULL) &&
	((XSD10_WXS_TYPE_CONTENTTYPE(typeDef))->type ==
	    XML_XSD10_SCHEMA_TYPE_PARTICLE) &&
	(XSD10_WXS_TYPE_PARTICLE_TERM(typeDef) != NULL) &&
	((XSD10_WXS_TYPE_PARTICLE_TERM(typeDef))->type ==
	    XML_XSD10_SCHEMA_EXTRA_QNAMEREF))
    {
	XmlXsd10QNameRef *ref =
	    XSD10_WXS_QNAME_CAST XSD10_WXS_TYPE_PARTICLE_TERM(typeDef);
	XmlXsd10ModelGroupDef *groupDef;

	/*
	* URGENT TODO: Test this.
	*/
	XSD10_WXS_TYPE_PARTICLE_TERM(typeDef) = NULL;
	/*
	* Resolve the MG definition reference.
	*/
	groupDef =
	    XSD10_WXS_MODEL_GROUPDEF_CAST xml_xsd10_get_named_component(ctxt->schema,
		ref->itemType, ref->name, ref->targetNamespace);
	if (groupDef == NULL) {
	    xml_xsd10_p_res_comp_attr_err(ctxt, XML_XSD10_SCHEMAP_SRC_RESOLVE,
		NULL, XSD10_WXS_ITEM_NODE(XSD10_WXS_TYPE_PARTICLE(typeDef)),
		"ref", ref->name, ref->targetNamespace, ref->itemType,
		NULL);
	    /* Remove the particle. */
	    XSD10_WXS_TYPE_CONTENTTYPE(typeDef) = NULL;
	} else if (XSD10_WXS_MODELGROUPDEF_MODEL(groupDef) == NULL)
	    /* Remove the particle. */
	    XSD10_WXS_TYPE_CONTENTTYPE(typeDef) = NULL;
	else {
	    /*
	    * Assign the MG definition's {model group} to the
	    * particle's {term}.
	    */
	    XSD10_WXS_TYPE_PARTICLE_TERM(typeDef) = XSD10_WXS_MODELGROUPDEF_MODEL(groupDef);

	    if (XSD10_WXS_MODELGROUPDEF_MODEL(groupDef)->type == XML_XSD10_SCHEMA_TYPE_ALL) {
		/*
		* SPEC cos-all-limited (1.2)
		* "1.2 the {term} property of a particle with
		* {max occurs}=1 which is part of a pair which constitutes
		* the {content type} of a complex type definition."
		*/
		if ((XSD10_WXS_TYPE_PARTICLE(typeDef))->maxOccurs != 1) {
		    xml_xsd10_custom_err(XSD10_ACTXT_CAST ctxt,
			/* TODO: error code */
			XML_XSD10_SCHEMAP_COS_ALL_LIMITED,
			XSD10_WXS_ITEM_NODE(XSD10_WXS_TYPE_PARTICLE(typeDef)), NULL,
			"The particle's {max occurs} must be 1, since the "
			"reference resolves to an 'all' model group",
			NULL, NULL);
		}
	    }
	}
    }
}



/**
 * Port of xmlSchemaCheckSTPropsCorrect.
 *
 * Checks st-props-correct.
 *
 * @param ctxt  the schema parser context
 * @param type  the simple type definition
 * @returns 0 if the properties are correct,
 * if not, a positive error code and -1 on internal
 * errors.
 */
int xml_xsd10_check_st_props_correct(XmlXsd10ParserCtxt *ctxt, XmlXsd10Type *type)
{
    XmlXsd10Type *baseType = type->baseType;
    char *str = NULL;

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
    if (baseType == NULL) {
	/*
	* TODO: Think about: "modulo the impact of Missing
	* Sub-components ($5.3)."
	*/
	xml_xsd10_p_custom_err(ctxt,
	    XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_1,
	    XSD10_WXS_BASIC_CAST type, NULL,
	    "No base type existent", NULL);
	return (XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_1);

    }
    if (! XSD10_WXS_IS_SIMPLE(baseType)) {
	xml_xsd10_p_custom_err(ctxt,
	    XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_1,
	    XSD10_WXS_BASIC_CAST type, NULL,
	    "The base type '%s' is not a simple type",
	    xml_xsd10_get_component_q_name(&str, baseType));
	XSD10_FREE_AND_NULL(str)
	return (XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_1);
    }
    if ((XSD10_WXS_IS_LIST(type) || XSD10_WXS_IS_UNION(type)) &&
	(XSD10_WXS_IS_RESTRICTION(type) == 0) &&
	((! XSD10_WXS_IS_ANY_SIMPLE_TYPE(baseType)) &&
         (baseType->type != XML_XSD10_SCHEMA_TYPE_SIMPLE))) {
	xml_xsd10_p_custom_err(ctxt,
	    XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_1,
	    XSD10_WXS_BASIC_CAST type, NULL,
	    "A type, derived by list or union, must have "
	    "the simple ur-type definition as base type, not '%s'",
	    xml_xsd10_get_component_q_name(&str, baseType));
	XSD10_FREE_AND_NULL(str)
	return (XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_1);
    }
    /*
    * Variety: One of {atomic, list, union}.
    */
    if ((! XSD10_WXS_IS_ATOMIC(type)) && (! XSD10_WXS_IS_UNION(type)) &&
	(! XSD10_WXS_IS_LIST(type))) {
	xml_xsd10_p_custom_err(ctxt,
	    XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_1,
	    XSD10_WXS_BASIC_CAST type, NULL,
	    "The variety is absent", NULL);
	return (XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_1);
    }
    /* TODO: Finish this. Hmm, is this finished? */

    /*
    * 3 The {final} of the {base type definition} must not contain restriction.
    */
    if (xml_xsd10_type_final_contains(baseType,
	XML_XSD10_SCHEMAS_TYPE_FINAL_RESTRICTION)) {
	xml_xsd10_p_custom_err(ctxt,
	    XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_3,
	    XSD10_WXS_BASIC_CAST type, NULL,
	    "The 'final' of its base type '%s' must not contain "
	    "'restriction'",
	    xml_xsd10_get_component_q_name(&str, baseType));
	XSD10_FREE_AND_NULL(str)
	return (XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_3);
    }

    /*
    * 2 All simple type definitions must be derived ultimately from the `simple
    * ur-type definition` (so circular definitions are disallowed). That is, it
    * must be possible to reach a built-in primitive datatype or the `simple
    * ur-type definition` by repeatedly following the {base type definition}.
    *
    * NOTE: this is done in xmlSchemaCheckTypeDefCircular().
    */
    return (0);
}

/**
 * Port of xmlSchemaCheckCOSSTRestricts.
 *
 * Schema Component Constraint:
 * Derivation Valid (Restriction, Simple) (cos-st-restricts)

 * Checks if the given `type` (simpleType) is derived validly by restriction.
 * STATUS:
 *
 * @param pctxt  the schema parser context
 * @param type  the simple type definition
 * @returns -1 on internal errors, 0 if the type is validly derived,
 * a positive error code otherwise.
 */
int xml_xsd10_check_cosst_restricts(XmlXsd10ParserCtxt *pctxt, XmlXsd10Type *type)
{
    char *str = NULL;

    if (type->type != XML_XSD10_SCHEMA_TYPE_SIMPLE) {
	XSD10_PERROR_INT("xmlSchemaCheckCOSSTRestricts",
	    "given type is not a user-derived simpleType");
	return (-1);
    }

    if (XSD10_WXS_IS_ATOMIC(type)) {
	XmlXsd10Type *primitive;
	/*
	* 1.1 The {base type definition} must be an atomic simple
	* type definition or a built-in primitive datatype.
	*/
	if (! XSD10_WXS_IS_ATOMIC(type->baseType)) {
	    xml_xsd10_p_custom_err(pctxt,
		XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_1,
		XSD10_WXS_BASIC_CAST type, NULL,
		"The base type '%s' is not an atomic simple type",
		xml_xsd10_get_component_q_name(&str, type->baseType));
	    XSD10_FREE_AND_NULL(str)
	    return (XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_1);
	}
	/* 1.2 The {final} of the {base type definition} must not contain
	* restriction.
	*/
	/* OPTIMIZE TODO : This is already done in xmlSchemaCheckStPropsCorrect */
	if (xml_xsd10_type_final_contains(type->baseType,
	    XML_XSD10_SCHEMAS_TYPE_FINAL_RESTRICTION)) {
	    xml_xsd10_p_custom_err(pctxt,
		XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_2,
		XSD10_WXS_BASIC_CAST type, NULL,
		"The final of its base type '%s' must not contain 'restriction'",
		xml_xsd10_get_component_q_name(&str, type->baseType));
	    XSD10_FREE_AND_NULL(str)
	    return (XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_2);
	}

	/*
	* 1.3.1 DF must be an allowed constraining facet for the {primitive
	* type definition}, as specified in the appropriate subsection of 3.2
	* Primitive datatypes.
	*/
	if (type->facets != NULL) {
	    XmlXsd10Facet *facet;
	    int ok = 1;

	    primitive = xml_xsd10_get_primitive_type(type);
	    if (primitive == NULL) {
		XSD10_PERROR_INT("xmlSchemaCheckCOSSTRestricts",
		    "failed to get primitive type");
		return (-1);
	    }
	    facet = type->facets;
	    do {
		if (xml_xsd10_is_built_in_type_facet(primitive, facet->type) == 0) {
		    ok = 0;
		    xml_xsd10_p_illegal_facet_atomic_err(pctxt,
			XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_3_1,
			type, primitive, facet);
		}
		facet = facet->next;
	    } while (facet != NULL);
	    if (ok == 0)
		return (XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_3_1);
	}
	/*
	* SPEC (1.3.2) "If there is a facet of the same kind in the {facets}
	* of the {base type definition} (call this BF),then the DF's {value}
	* must be a valid restriction of BF's {value} as defined in
	* [XML Schemas: Datatypes]."
	*
	* NOTE (1.3.2) Facet derivation constraints are currently handled in
	* xmlSchemaDeriveAndValidateFacets()
	*/
    } else if (XSD10_WXS_IS_LIST(type)) {
	XmlXsd10Type *itemType = NULL;

	itemType = type->subtypes;
	if ((itemType == NULL) || (! XSD10_WXS_IS_SIMPLE(itemType))) {
	    XSD10_PERROR_INT("xmlSchemaCheckCOSSTRestricts",
		"failed to evaluate the item type");
	    return (-1);
	}
	if (XSD10_WXS_IS_TYPE_NOT_FIXED(itemType))
	    xml_xsd10_type_fixup(itemType, XSD10_ACTXT_CAST pctxt);
	/*
	* 2.1 The {item type definition} must have a {variety} of atomic or
	* union (in which case all the {member type definitions}
	* must be atomic).
	*/
	if ((! XSD10_WXS_IS_ATOMIC(itemType)) &&
	    (! XSD10_WXS_IS_UNION(itemType))) {
	    xml_xsd10_p_custom_err(pctxt,
		XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_1,
		XSD10_WXS_BASIC_CAST type, NULL,
		"The item type '%s' does not have a variety of atomic or union",
		xml_xsd10_get_component_q_name(&str, itemType));
	    XSD10_FREE_AND_NULL(str)
	    return (XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_1);
	} else if (XSD10_WXS_IS_UNION(itemType)) {
	    XmlXsd10TypeLink *member;

	    member = itemType->memberTypes;
	    while (member != NULL) {
		if (! XSD10_WXS_IS_ATOMIC(member->type)) {
		    xml_xsd10_p_custom_err(pctxt,
			XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_1,
			XSD10_WXS_BASIC_CAST type, NULL,
			"The item type is a union type, but the "
			"member type '%s' of this item type is not atomic",
			xml_xsd10_get_component_q_name(&str, member->type));
		    XSD10_FREE_AND_NULL(str)
		    return (XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_1);
		}
		member = member->next;
	    }
	}

	if (XSD10_WXS_IS_ANY_SIMPLE_TYPE(type->baseType)) {
	    XmlXsd10Facet *facet;
	    /*
	    * This is the case if we have: <simpleType><list ..
	    */
	    /*
	    * 2.3.1
	    * 2.3.1.1 The {final} of the {item type definition} must not
	    * contain list.
	    */
	    if (xml_xsd10_type_final_contains(itemType,
		XML_XSD10_SCHEMAS_TYPE_FINAL_LIST)) {
		xml_xsd10_p_custom_err(pctxt,
		    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_1_1,
		    XSD10_WXS_BASIC_CAST type, NULL,
		    "The final of its item type '%s' must not contain 'list'",
		    xml_xsd10_get_component_q_name(&str, itemType));
		XSD10_FREE_AND_NULL(str)
		return (XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_1_1);
	    }
	    /*
	    * 2.3.1.2 The {facets} must only contain the whiteSpace
	    * facet component.
	    * OPTIMIZE TODO: the S4S already disallows any facet
	    * to be specified.
	    */
	    if (type->facets != NULL) {
		facet = type->facets;
		do {
		    if (facet->type != XML_XSD10_SCHEMA_FACET_WHITESPACE) {
			xml_xsd10_p_illegal_facet_list_union_err(pctxt,
			    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_1_2,
			    type, facet);
			return (XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_1_2);
		    }
		    facet = facet->next;
		} while (facet != NULL);
	    }
	    /*
	    * MAYBE TODO: (Hmm, not really) Datatypes states:
	    * A `list` datatype can be `derived` from an `atomic` datatype
	    * whose `lexical space` allows space (such as string or anyURI)or
	    * a `union` datatype any of whose {member type definitions}'s
	    * `lexical space` allows space.
	    */
	} else {
	    /*
	    * This is the case if we have: <simpleType><restriction ...
	    * I.e. the variety of "list" is inherited.
	    */
	    /*
	    * 2.3.2
	    * 2.3.2.1 The {base type definition} must have a {variety} of list.
	    */
	    if (! XSD10_WXS_IS_LIST(type->baseType)) {
		xml_xsd10_p_custom_err(pctxt,
		    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_1,
		    XSD10_WXS_BASIC_CAST type, NULL,
		    "The base type '%s' must be a list type",
		    xml_xsd10_get_component_q_name(&str, type->baseType));
		XSD10_FREE_AND_NULL(str)
		return (XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_1);
	    }
	    /*
	    * 2.3.2.2 The {final} of the {base type definition} must not
	    * contain restriction.
	    */
	    if (xml_xsd10_type_final_contains(type->baseType,
		XML_XSD10_SCHEMAS_TYPE_FINAL_RESTRICTION)) {
		xml_xsd10_p_custom_err(pctxt,
		    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_2,
		    XSD10_WXS_BASIC_CAST type, NULL,
		    "The 'final' of the base type '%s' must not contain 'restriction'",
		    xml_xsd10_get_component_q_name(&str, type->baseType));
		XSD10_FREE_AND_NULL(str)
		return (XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_2);
	    }
	    /*
	    * 2.3.2.3 The {item type definition} must be validly derived
	    * from the {base type definition}'s {item type definition} given
	    * the empty set, as defined in Type Derivation OK (Simple) ($3.14.6).
	    */
	    {
		XmlXsd10Type *baseItemType;

		baseItemType = type->baseType->subtypes;
		if ((baseItemType == NULL) || (! XSD10_WXS_IS_SIMPLE(baseItemType))) {
		    XSD10_PERROR_INT("xmlSchemaCheckCOSSTRestricts",
			"failed to eval the item type of a base type");
		    return (-1);
		}
		if ((itemType != baseItemType) &&
		    (xml_xsd10_check_cosst_derived_ok(XSD10_ACTXT_CAST pctxt, itemType,
			baseItemType, 0) != 0)) {
		    char *strBIT = NULL, *strBT = NULL;
		    xml_xsd10_p_custom_err_ext(pctxt,
			XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_3,
			XSD10_WXS_BASIC_CAST type, NULL,
			"The item type '%s' is not validly derived from "
			"the item type '%s' of the base type '%s'",
			xml_xsd10_get_component_q_name(&str, itemType),
			xml_xsd10_get_component_q_name(&strBIT, baseItemType),
			xml_xsd10_get_component_q_name(&strBT, type->baseType));

		    XSD10_FREE_AND_NULL(str)
		    XSD10_FREE_AND_NULL(strBIT)
		    XSD10_FREE_AND_NULL(strBT)
		    return (XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_3);
		}
	    }

	    if (type->facets != NULL) {
		XmlXsd10Facet *facet;
		int ok = 1;
		/*
		* 2.3.2.4 Only length, minLength, maxLength, whiteSpace, pattern
		* and enumeration facet components are allowed among the {facets}.
		*/
		facet = type->facets;
		do {
		    switch (facet->type) {
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
			default: {
			    xml_xsd10_p_illegal_facet_list_union_err(pctxt,
				XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_4,
				type, facet);
			    /*
			    * We could return, but it's nicer to report all
			    * invalid facets.
			    */
			    ok = 0;
			}
		    }
		    facet = facet->next;
		} while (facet != NULL);
		if (ok == 0)
		    return (XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_4);
		/*
		* SPEC (2.3.2.5) (same as 1.3.2)
		*
		* NOTE (2.3.2.5) This is currently done in
		* xmlSchemaDeriveAndValidateFacets()
		*/
	    }
	}
    } else if (XSD10_WXS_IS_UNION(type)) {
	/*
	* 3.1 The {member type definitions} must all have {variety} of
	* atomic or list.
	*/
	XmlXsd10TypeLink *member;

	member = type->memberTypes;
	while (member != NULL) {
	    if (XSD10_WXS_IS_TYPE_NOT_FIXED(member->type))
		xml_xsd10_type_fixup(member->type, XSD10_ACTXT_CAST pctxt);

	    if ((! XSD10_WXS_IS_ATOMIC(member->type)) &&
		(! XSD10_WXS_IS_LIST(member->type))) {
		xml_xsd10_p_custom_err(pctxt,
		    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_1,
		    XSD10_WXS_BASIC_CAST type, NULL,
		    "The member type '%s' is neither an atomic, nor a list type",
		    xml_xsd10_get_component_q_name(&str, member->type));
		XSD10_FREE_AND_NULL(str)
		return (XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_1);
	    }
	    member = member->next;
	}
	/*
	* 3.3.1 If the {base type definition} is the `simple ur-type
	* definition`
	*/
	if (type->baseType->builtInType == XML_XSD10_SCHEMAS_ANYSIMPLETYPE) {
	    /*
	    * 3.3.1.1 All of the {member type definitions} must have a
	    * {final} which does not contain union.
	    */
	    member = type->memberTypes;
	    while (member != NULL) {
		if (xml_xsd10_type_final_contains(member->type,
		    XML_XSD10_SCHEMAS_TYPE_FINAL_UNION)) {
		    xml_xsd10_p_custom_err(pctxt,
			XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_1,
			XSD10_WXS_BASIC_CAST type, NULL,
			"The 'final' of member type '%s' contains 'union'",
			xml_xsd10_get_component_q_name(&str, member->type));
		    XSD10_FREE_AND_NULL(str)
		    return (XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_1);
		}
		member = member->next;
	    }
	    /*
	    * 3.3.1.2 The {facets} must be empty.
	    */
	    if (type->facetSet != NULL) {
		xml_xsd10_p_custom_err(pctxt,
		    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_1_2,
		    XSD10_WXS_BASIC_CAST type, NULL,
		    "No facets allowed", NULL);
		return (XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_1_2);
	    }
	} else {
	    /*
	    * 3.3.2.1 The {base type definition} must have a {variety} of union.
	    * I.e. the variety of "list" is inherited.
	    */
	    if (! XSD10_WXS_IS_UNION(type->baseType)) {
		xml_xsd10_p_custom_err(pctxt,
		    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_1,
		    XSD10_WXS_BASIC_CAST type, NULL,
		    "The base type '%s' is not a union type",
		    xml_xsd10_get_component_q_name(&str, type->baseType));
		XSD10_FREE_AND_NULL(str)
		return (XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_1);
	    }
	    /*
	    * 3.3.2.2 The {final} of the {base type definition} must not contain restriction.
	    */
	    if (xml_xsd10_type_final_contains(type->baseType,
		XML_XSD10_SCHEMAS_TYPE_FINAL_RESTRICTION)) {
		xml_xsd10_p_custom_err(pctxt,
		    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_2,
		    XSD10_WXS_BASIC_CAST type, NULL,
		    "The 'final' of its base type '%s' must not contain 'restriction'",
		    xml_xsd10_get_component_q_name(&str, type->baseType));
		XSD10_FREE_AND_NULL(str)
		return (XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_2);
	    }
	    /*
	    * 3.3.2.3 The {member type definitions}, in order, must be validly
	    * derived from the corresponding type definitions in the {base
	    * type definition}'s {member type definitions} given the empty set,
	    * as defined in Type Derivation OK (Simple) ($3.14.6).
	    */
	    {
		XmlXsd10TypeLink *baseMember;

		/*
		* OPTIMIZE: if the type is restricting, it has no local defined
		* member types and inherits the member types of the base type;
		* thus a check for equality can be skipped.
		*/
		/*
		* Even worse: I cannot see a scenario where a restricting
		* union simple type can have other member types as the member
		* types of it's base type. This check seems not necessary with
		* respect to the derivation process in libxml2.
		* But necessary if constructing types with an API.
		*/
		if (type->memberTypes != NULL) {
		    member = type->memberTypes;
		    baseMember = xml_xsd10_get_union_simple_type_member_types(type->baseType);
		    if ((member == NULL) && (baseMember != NULL)) {
			XSD10_PERROR_INT("xmlSchemaCheckCOSSTRestricts",
			    "different number of member types in base");
		    }
		    while (member != NULL) {
			if (baseMember == NULL) {
			    XSD10_PERROR_INT("xmlSchemaCheckCOSSTRestricts",
			    "different number of member types in base");
			} else if ((member->type != baseMember->type) &&
			    (xml_xsd10_check_cosst_derived_ok(XSD10_ACTXT_CAST pctxt,
				member->type, baseMember->type, 0) != 0)) {
			    char *strBMT = NULL, *strBT = NULL;

			    xml_xsd10_p_custom_err_ext(pctxt,
				XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_3,
				XSD10_WXS_BASIC_CAST type, NULL,
				"The member type %s is not validly "
				"derived from its corresponding member "
				"type %s of the base type %s",
				xml_xsd10_get_component_q_name(&str, member->type),
				xml_xsd10_get_component_q_name(&strBMT, baseMember->type),
				xml_xsd10_get_component_q_name(&strBT, type->baseType));
			    XSD10_FREE_AND_NULL(str)
			    XSD10_FREE_AND_NULL(strBMT)
			    XSD10_FREE_AND_NULL(strBT)
			    return (XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_3);
			}
			member = member->next;
                        if (baseMember != NULL)
                            baseMember = baseMember->next;
		    }
		}
	    }
	    /*
	    * 3.3.2.4 Only pattern and enumeration facet components are
	    * allowed among the {facets}.
	    */
	    if (type->facets != NULL) {
		XmlXsd10Facet *facet;
		int ok = 1;

		facet = type->facets;
		do {
		    if ((facet->type != XML_XSD10_SCHEMA_FACET_PATTERN) &&
			(facet->type != XML_XSD10_SCHEMA_FACET_ENUMERATION)) {
			xml_xsd10_p_illegal_facet_list_union_err(pctxt,
				XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_4,
				type, facet);
			ok = 0;
		    }
		    facet = facet->next;
		} while (facet != NULL);
		if (ok == 0)
		    return (XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_4);

	    }
	    /*
	    * SPEC (3.3.2.5) (same as 1.3.2)
	    *
	    * NOTE (3.3.2.5) This is currently done in
	    * xmlSchemaDeriveAndValidateFacets()
	    */
	}
    }

    return (0);
}


/** Port of xmlSchemaCreateVCtxtOnPCtxt. */
int xml_xsd10_create_v_ctxt_on_p_ctxt(XmlXsd10ParserCtxt *ctxt)
{
   if (ctxt->vctxt == NULL) {
	ctxt->vctxt = xml_xsd10_new_valid_ctxt(NULL);
	if (ctxt->vctxt == NULL) {
	    xml_xsd10_p_err(ctxt, NULL,
		XML_XSD10_SCHEMAP_INTERNAL,
		"Internal error: xmlSchemaCreateVCtxtOnPCtxt, "
		"failed to create a temp. validation context.\n",
		NULL, NULL);
	    return (-1);
	}
	/* TODO: Pass user data. */
	xml_xsd10_set_valid_errors(ctxt->vctxt,
	    ctxt->error, ctxt->warning, ctxt->errCtxt);
	xml_xsd10_set_valid_structured_errors(ctxt->vctxt,
	    ctxt->serror, ctxt->errCtxt);
    }
    return (0);
}


/**
 * Port of xmlSchemaParseCheckCOSValidDefault.
 *
 * Schema Component Constraint: Element Default Valid (Immediate)
 * (cos-valid-default)
 * This will be used by the parser only. For the validator there's
 * an other version.
 *
 * @param pctxt  the schema parser context
 * @param node  an optional node (the holder of the value)
 * @param type  the simple type definition
 * @param value  the default value
 * @param val  resulting value
 * @returns 0 if the constraints are satisfied,
 * if not, a positive error code and -1 on internal
 * errors.
 */
int xml_xsd10_parse_check_cos_valid_default(XmlXsd10ParserCtxt *pctxt, XMQNode *node, XmlXsd10Type *type, const char *value, XmlXsd10Val **val)
{
    int ret = 0;

    /*
    * cos-valid-default:
    * Schema Component Constraint: Element Default Valid (Immediate)
    * For a string to be a valid default with respect to a type
    * definition the appropriate case among the following must be true:
    */
    if XSD10_WXS_IS_COMPLEX(type) {
	/*
	* Complex type.
	*
	* SPEC (2.1) "its {content type} must be a simple type definition
	* or mixed."
	* SPEC (2.2.2) "If the {content type} is mixed, then the {content
	* type}'s particle must be `emptiable` as defined by
	* Particle Emptiable ($3.9.6)."
	*/
	if ((! XSD10_WXS_HAS_SIMPLE_CONTENT(type)) &&
	    ((! XSD10_WXS_HAS_MIXED_CONTENT(type)) || (! XSD10_WXS_EMPTIABLE(type)))) {
	    /* NOTE that this covers (2.2.2) as well. */
	    xml_xsd10_p_custom_err(pctxt,
		XML_XSD10_SCHEMAP_COS_VALID_DEFAULT_2_1,
		XSD10_WXS_BASIC_CAST type, type->node,
		"For a string to be a valid default, the type definition "
		"must be a simple type or a complex type with mixed content "
		"and a particle emptiable", NULL);
	    return(XML_XSD10_SCHEMAP_COS_VALID_DEFAULT_2_1);
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
    if (XSD10_WXS_IS_SIMPLE(type))
	ret = xml_xsd10_v_check_cvc_simple_type(XSD10_ACTXT_CAST pctxt, node,
	    type, value, val, 1, 1, 0);
    else if (XSD10_WXS_HAS_SIMPLE_CONTENT(type))
	ret = xml_xsd10_v_check_cvc_simple_type(XSD10_ACTXT_CAST pctxt, node,
	    type->contentTypeDef, value, val, 1, 1, 0);
    else
	return (ret);

    if (ret < 0) {
	XSD10_PERROR_INT("xmlSchemaParseCheckCOSValidDefault",
	    "calling xmlSchemaVCheckCVCSimpleType()");
    }

    return (ret);
}

/**
 * Port of xmlSchemaCheckCTPropsCorrect.
 *
 * (4.6) Constraints on Complex Type Definition Schema Components
 * Schema Component Constraint:
 * Complex Type Definition Properties Correct (ct-props-correct)
 * STATUS: (seems) complete
 *
 * @param pctxt  the schema parser context
 * @param type  the complex type definition
 * @returns 0 if the constraints are satisfied, a positive
 * error code if not and -1 if an internal error occurred.
 */
int xml_xsd10_check_ct_props_correct(XmlXsd10ParserCtxt *pctxt, XmlXsd10Type *type)
{
    /*
    * TODO: Correct the error code; XML_SCHEMAP_SRC_CT_1 is used temporarily.
    *
    * SPEC (1) "The values of the properties of a complex type definition must
    * be as described in the property tableau in The Complex Type Definition
    * Schema Component ($3.4.1), modulo the impact of Missing
    * Sub-components ($5.3)."
    */
    if ((type->baseType != NULL) &&
	(XSD10_WXS_IS_SIMPLE(type->baseType)) &&
	(XSD10_WXS_IS_EXTENSION(type) == 0)) {
	/*
	* SPEC (2) "If the {base type definition} is a simple type definition,
	* the {derivation method} must be extension."
	*/
	xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
	    XML_XSD10_SCHEMAP_SRC_CT_1,
	    NULL, XSD10_WXS_BASIC_CAST type,
	    "If the base type is a simple type, the derivation method must be "
	    "'extension'", NULL, NULL);
	return (XML_XSD10_SCHEMAP_SRC_CT_1);
    }
    /*
    * SPEC (3) "Circular definitions are disallowed, except for the `ur-type
    * definition`. That is, it must be possible to reach the `ur-type
    * definition` by repeatedly following the {base type definition}."
    *
    * NOTE (3) is done in xmlSchemaCheckTypeDefCircular().
    */
    /*
    * NOTE that (4) and (5) need the following:
    *   - attribute uses need to be already inherited (apply attr. prohibitions)
    *   - attribute group references need to be expanded already
    *   - simple types need to be typefixed already
    */
    if (type->attrUses &&
	(((XmlXsd10ItemList *) type->attrUses)->nbItems > 1))
    {
	XmlXsd10ItemList *uses = (XmlXsd10ItemList *) type->attrUses;
	XmlXsd10AttributeUse *use, *tmp;
	int i, j, hasId = 0;

	for (i = uses->nbItems -1; i >= 0; i--) {
	    use = (XmlXsd10AttributeUse *) uses->items[i];

	    /*
	    * SPEC ct-props-correct
	    * (4) "Two distinct attribute declarations in the
	    * {attribute uses} must not have identical {name}s and
	    * {target namespace}s."
	    */
	    if (i > 0) {
		for (j = i -1; j >= 0; j--) {
		    tmp = (XmlXsd10AttributeUse *) uses->items[j];
		    if ((XSD10_WXS_ATTRUSE_DECL_NAME(use) ==
			XSD10_WXS_ATTRUSE_DECL_NAME(tmp)) &&
			(XSD10_WXS_ATTRUSE_DECL_TNS(use) ==
			XSD10_WXS_ATTRUSE_DECL_TNS(tmp)))
		    {
			char *str = NULL;

			xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
			    XML_XSD10_SCHEMAP_AG_PROPS_CORRECT,
			    NULL, XSD10_WXS_BASIC_CAST type,
			    "Duplicate %s",
			    xml_xsd10_get_component_designation(&str, use),
			    NULL);
			XSD10_FREE_AND_NULL(str);
			/*
			* Remove the duplicate.
			*/
			if (xml_xsd10_item_list_remove(uses, i) == -1)
			    goto exit_failure;
			goto next_use;
		    }
		}
	    }
	    /*
	    * SPEC ct-props-correct
	    * (5) "Two distinct attribute declarations in the
	    * {attribute uses} must not have {type definition}s which
	    * are or are derived from ID."
	    */
	    if (XSD10_WXS_ATTRUSE_TYPEDEF(use) != NULL) {
		if (xml_xsd10_is_derived_from_built_in_type(
		    XSD10_WXS_ATTRUSE_TYPEDEF(use), XML_XSD10_SCHEMAS_ID))
		{
		    if (hasId) {
			char *str = NULL;

			xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
			    XML_XSD10_SCHEMAP_AG_PROPS_CORRECT,
			    NULL, XSD10_WXS_BASIC_CAST type,
			    "There must not exist more than one attribute "
			    "declaration of type 'xs:ID' "
			    "(or derived from 'xs:ID'). The %s violates this "
			    "constraint",
			    xml_xsd10_get_component_designation(&str, use),
			    NULL);
			XSD10_FREE_AND_NULL(str);
			if (xml_xsd10_item_list_remove(uses, i) == -1)
			    goto exit_failure;
		    }

		    hasId = 1;
		}
	    }
next_use: {}
	}
    }
    return (0);
exit_failure:
    return(-1);
}

/** Port of xmlSchemaAreEqualTypes. */
int xml_xsd10_are_equal_types(XmlXsd10Type *typeA, XmlXsd10Type *typeB)
{
    /*
    * TODO: This should implement component-identity
    * in the future.
    */
    if ((typeA == NULL) || (typeB == NULL))
	return (0);
    return (typeA == typeB);
}

/**
 * Port of xmlSchemaCheckCOSCTDerivedOK.
 *
 * Schema Component Constraint:
 * Type Derivation OK (Complex) (cos-ct-derived-ok)
 *
 * STATUS: completed
 *
 * @param actxt  the schema parser context
 * @param type  the to-be derived complex type definition
 * @param baseType  the base complex type definition
 * @param set  the given set
 * @returns 0 if the constraints are satisfied, or 1
 * if not.
 */
int xml_xsd10_check_cosct_derived_ok(XmlXsd10AbstractCtxt *actxt, XmlXsd10Type *type, XmlXsd10Type *baseType, int set)
{
    int equal = xml_xsd10_are_equal_types(type, baseType);
    /* TODO: Error codes. */
    /*
    * SPEC "For a complex type definition (call it D, for derived)
    * to be validly derived from a type definition (call this
    * B, for base) given a subset of {extension, restriction}
    * all of the following must be true:"
    */
    if (! equal) {
	/*
	* SPEC (1) "If B and D are not the same type definition, then the
	* {derivation method} of D must not be in the subset."
	*/
	if (((set & XSD10_SUBSET_EXTENSION) && (XSD10_WXS_IS_EXTENSION(type))) ||
	    ((set & XSD10_SUBSET_RESTRICTION) && (XSD10_WXS_IS_RESTRICTION(type))))
	    return (1);
    } else {
	/*
	* SPEC (2.1) "B and D must be the same type definition."
	*/
	return (0);
    }
    /*
    * SPEC (2.2) "B must be D's {base type definition}."
    */
    if (type->baseType == baseType)
	return (0);
    /*
    * SPEC (2.3.1) "D's {base type definition} must not be the `ur-type
    * definition`."
    */
    if (XSD10_WXS_IS_ANYTYPE(type->baseType))
	return (1);

    if (XSD10_WXS_IS_COMPLEX(type->baseType)) {
	/*
	* SPEC (2.3.2.1) "If D's {base type definition} is complex, then it
	* must be validly derived from B given the subset as defined by this
	* constraint."
	*/
	return (xml_xsd10_check_cosct_derived_ok(actxt, type->baseType,
	    baseType, set));
    } else {
	/*
	* SPEC (2.3.2.2) "If D's {base type definition} is simple, then it
	* must be validly derived from B given the subset as defined in Type
	* Derivation OK (Simple) ($3.14.6).
	*/
	return (xml_xsd10_check_cosst_derived_ok(actxt, type->baseType,
	    baseType, set));
    }
}

/**
 * Port of xmlSchemaCheckCOSDerivedOK.
 *
 * Calls:
 * Type Derivation OK (Simple) AND Type Derivation OK (Complex)
 *
 * Checks whether `type` can be validly derived from `baseType`.
 *
 * @param actxt  the schema parser context
 * @param type  the derived simple type definition
 * @param baseType  the base type definition
 * @param set  the given set
 * @returns 0 on success, an positive error code otherwise.
 */
int xml_xsd10_check_cos_derived_ok(XmlXsd10AbstractCtxt *actxt, XmlXsd10Type *type, XmlXsd10Type *baseType, int set)
{
    if (XSD10_WXS_IS_SIMPLE(type))
	return (xml_xsd10_check_cosst_derived_ok(actxt, type, baseType, set));
    else
	return (xml_xsd10_check_cosct_derived_ok(actxt, type, baseType, set));
}

/**
 * Port of xmlSchemaCheckCOSCTExtends.
 *
 * (3.4.6) Constraints on Complex Type Definition Schema Components
 * Schema Component Constraint:
 * Derivation Valid (Extension) (cos-ct-extends)
 *
 * STATUS:
 *   missing:
 *     (1.5)
 *     (1.4.3.2.2.2) "Particle Valid (Extension)"
 *
 * @param ctxt  the schema parser context
 * @param type  the complex type definition
 * @returns 0 if the constraints are satisfied, a positive
 * error code if not and -1 if an internal error occurred.
 */
int xml_xsd10_check_cosct_extends(XmlXsd10ParserCtxt *ctxt, XmlXsd10Type *type)
{
    XmlXsd10Type *base = type->baseType;
    /*
    * TODO: Correct the error code; XML_SCHEMAP_COS_CT_EXTENDS_1_1 is used
    * temporarily only.
    */
    /*
    * SPEC (1) "If the {base type definition} is a complex type definition,
    * then all of the following must be true:"
    */
    if (XSD10_WXS_IS_COMPLEX(base)) {
	/*
	* SPEC (1.1) "The {final} of the {base type definition} must not
	* contain extension."
	*/
	if (base->flags & XML_XSD10_SCHEMAS_TYPE_FINAL_EXTENSION) {
	    xml_xsd10_p_custom_err(ctxt,
		XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_1,
		XSD10_WXS_BASIC_CAST type, NULL,
		"The 'final' of the base type definition "
		"contains 'extension'", NULL);
	    return (XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_1);
	}

	/*
	* ATTENTION: The constrains (1.2) and (1.3) are not applied,
	* since they are automatically satisfied through the
	* inheriting mechanism.
	* Note that even if redefining components, the inheriting mechanism
	* is used.
	*/
	/*
	* SPEC (1.4) "One of the following must be true:"
	*/
	if ((type->contentTypeDef != NULL) &&
	    (type->contentTypeDef == base->contentTypeDef)) {
	    /*
	    * SPEC (1.4.1) "The {content type} of the {base type definition}
	    * and the {content type} of the complex type definition itself
	    * must be the same simple type definition"
	    * PASS
	    */
	} else if ((type->contentType == XML_XSD10_SCHEMA_CONTENT_EMPTY) &&
	    (base->contentType == XML_XSD10_SCHEMA_CONTENT_EMPTY) ) {
	    /*
	    * SPEC (1.4.2) "The {content type} of both the {base type
	    * definition} and the complex type definition itself must
	    * be empty."
	    * PASS
	    */
	} else {
	    /*
	    * SPEC (1.4.3) "All of the following must be true:"
	    */
	    if (type->subtypes == NULL) {
		/*
		* SPEC 1.4.3.1 The {content type} of the complex type
		* definition itself must specify a particle.
		*/
		xml_xsd10_p_custom_err(ctxt,
		    XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_1,
		    XSD10_WXS_BASIC_CAST type, NULL,
		    "The content type must specify a particle", NULL);
		return (XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_1);
	    }
	    /*
	    * SPEC (1.4.3.2) "One of the following must be true:"
	    */
	    if (base->contentType == XML_XSD10_SCHEMA_CONTENT_EMPTY) {
		/*
		* SPEC (1.4.3.2.1) "The {content type} of the {base type
		* definition} must be empty.
		* PASS
		*/
	    } else {
		/*
		* SPEC (1.4.3.2.2) "All of the following must be true:"
		*/
		if ((type->contentType != base->contentType) ||
		    ((type->contentType != XML_XSD10_SCHEMA_CONTENT_MIXED) &&
		    (type->contentType != XML_XSD10_SCHEMA_CONTENT_ELEMENTS))) {
		    /*
		    * SPEC (1.4.3.2.2.1) "Both {content type}s must be mixed
		    * or both must be element-only."
		    */
		    xml_xsd10_p_custom_err(ctxt,
			XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_1,
			XSD10_WXS_BASIC_CAST type, NULL,
			"The content type of both, the type and its base "
			"type, must either 'mixed' or 'element-only'", NULL);
		    return (XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_1);
		}
		/*
		* URGENT TODO SPEC (1.4.3.2.2.2) "The particle of the
		* complex type definition must be a `valid extension`
		* of the {base type definition}'s particle, as defined
		* in Particle Valid (Extension) ($3.9.6)."
		*
		* NOTE that we won't check "Particle Valid (Extension)",
		* since it is ensured by the derivation process in
		* xmlSchemaTypeFixup(). We need to implement this when heading
		* for a construction API
		* TODO: !! This is needed to be checked if redefining a type !!
		*/
	    }
	    /*
	    * URGENT TODO (1.5)
	    */
	}
    } else {
	/*
	* SPEC (2) "If the {base type definition} is a simple type definition,
	* then all of the following must be true:"
	*/
	if (type->contentTypeDef != base) {
	    /*
	    * SPEC (2.1) "The {content type} must be the same simple type
	    * definition."
	    */
	    xml_xsd10_p_custom_err(ctxt,
		XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_1,
		XSD10_WXS_BASIC_CAST type, NULL,
		"The content type must be the simple base type", NULL);
	    return (XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_1);
	}
	if (base->flags & XML_XSD10_SCHEMAS_TYPE_FINAL_EXTENSION) {
	    /*
	    * SPEC (2.2) "The {final} of the {base type definition} must not
	    * contain extension"
	    * NOTE that this is the same as (1.1).
	    */
	    xml_xsd10_p_custom_err(ctxt,
		XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_1,
		XSD10_WXS_BASIC_CAST type, NULL,
		"The 'final' of the base type definition "
		"contains 'extension'", NULL);
	    return (XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_1);
	}
    }
    return (0);
}

/**
 * Port of xmlSchemaCheckDerivationOKRestriction.
 *
 * (3.4.6) Constraints on Complex Type Definition Schema Components
 * Schema Component Constraint:
 * Derivation Valid (Restriction, Complex) (derivation-ok-restriction)
 *
 * STATUS:
 *   missing:
 *     (5.4.2) ???
 *
 * ATTENTION:
 * In XML Schema 1.1 this will be:
 * Validation Rule: Checking complex type subsumption
 *
 * @param ctxt  the schema parser context
 * @param type  the complex type definition
 * @returns 0 if the constraints are satisfied, a positive
 * error code if not and -1 if an internal error occurred.
 */
int xml_xsd10_check_derivation_ok_restriction(XmlXsd10ParserCtxt *ctxt, XmlXsd10Type *type)
{
    XmlXsd10Type *base;

    /*
    * TODO: Correct the error code; XML_SCHEMAP_DERIVATION_OK_RESTRICTION_1 is used
    * temporarily only.
    */
    base = type->baseType;
    if (! XSD10_WXS_IS_COMPLEX(base)) {
	xml_xsd10_custom_err(XSD10_ACTXT_CAST ctxt,
	    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_1,
	    type->node, XSD10_WXS_BASIC_CAST type,
	    "The base type must be a complex type", NULL, NULL);
	return(ctxt->err);
    }
    if (base->flags & XML_XSD10_SCHEMAS_TYPE_FINAL_RESTRICTION) {
	/*
	* SPEC (1) "The {base type definition} must be a complex type
	* definition whose {final} does not contain restriction."
	*/
	xml_xsd10_custom_err(XSD10_ACTXT_CAST ctxt,
	    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_1,
	    type->node, XSD10_WXS_BASIC_CAST type,
	    "The 'final' of the base type definition "
	    "contains 'restriction'", NULL, NULL);
	return (ctxt->err);
    }
    /*
    * SPEC (2), (3) and (4)
    * Those are handled in a separate function, since the
    * same constraints are needed for redefinition of
    * attribute groups as well.
    */
    if (xml_xsd10_check_derivation_ok_restriction2to4(ctxt,
	XML_XSD10_SCHEMA_ACTION_DERIVE,
	XSD10_WXS_BASIC_CAST type, XSD10_WXS_BASIC_CAST base,
	(XmlXsd10ItemList *) type->attrUses, (XmlXsd10ItemList *) base->attrUses,
	type->attributeWildcard,
	base->attributeWildcard) == -1)
    {
	return(-1);
    }
    /*
    * SPEC (5) "One of the following must be true:"
    */
    if (base->builtInType == XML_XSD10_SCHEMAS_ANYTYPE) {
	/*
	* SPEC (5.1) "The {base type definition} must be the
	* `ur-type definition`."
	* PASS
	*/
    } else if ((type->contentType == XML_XSD10_SCHEMA_CONTENT_SIMPLE) ||
	    (type->contentType == XML_XSD10_SCHEMA_CONTENT_BASIC)) {
	/*
	* SPEC (5.2.1) "The {content type} of the complex type definition
	* must be a simple type definition"
	*
	* SPEC (5.2.2) "One of the following must be true:"
	*/
	if ((base->contentType == XML_XSD10_SCHEMA_CONTENT_SIMPLE) ||
	    (base->contentType == XML_XSD10_SCHEMA_CONTENT_BASIC))
	{
	    int err;
	    /*
	    * SPEC (5.2.2.1) "The {content type} of the {base type
	    * definition} must be a simple type definition from which
	    * the {content type} is validly derived given the empty
	    * set as defined in Type Derivation OK (Simple) ($3.14.6)."
	    *
	    * ATTENTION TODO: This seems not needed if the type implicitly
	    * derived from the base type.
	    *
	    */
	    err = xml_xsd10_check_cosst_derived_ok(XSD10_ACTXT_CAST ctxt,
		type->contentTypeDef, base->contentTypeDef, 0);
	    if (err != 0) {
		char *strA = NULL, *strB = NULL;

		if (err == -1)
		    return(-1);
		xml_xsd10_custom_err(XSD10_ACTXT_CAST ctxt,
		    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_1,
		    NULL, XSD10_WXS_BASIC_CAST type,
		    "The {content type} %s is not validly derived from the "
		    "base type's {content type} %s",
		    xml_xsd10_get_component_designation(&strA,
			type->contentTypeDef),
		    xml_xsd10_get_component_designation(&strB,
			base->contentTypeDef));
		XSD10_FREE_AND_NULL(strA);
		XSD10_FREE_AND_NULL(strB);
		return(ctxt->err);
	    }
	} else if ((base->contentType == XML_XSD10_SCHEMA_CONTENT_MIXED) &&
	    (xml_xsd10_is_particle_emptiable(
		(XmlXsd10Particle *) base->subtypes))) {
	    /*
	    * SPEC (5.2.2.2) "The {base type definition} must be mixed
	    * and have a particle which is `emptiable` as defined in
	    * Particle Emptiable ($3.9.6)."
	    * PASS
	    */
	} else {
	    xml_xsd10_p_custom_err(ctxt,
		XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_1,
		XSD10_WXS_BASIC_CAST type, NULL,
		"The content type of the base type must be either "
		"a simple type or 'mixed' and an emptiable particle", NULL);
	    return (ctxt->err);
	}
    } else if (type->contentType == XML_XSD10_SCHEMA_CONTENT_EMPTY) {
	/*
	* SPEC (5.3.1) "The {content type} of the complex type itself must
	* be empty"
	*/
	if (base->contentType == XML_XSD10_SCHEMA_CONTENT_EMPTY) {
	    /*
	    * SPEC (5.3.2.1) "The {content type} of the {base type
	    * definition} must also be empty."
	    * PASS
	    */
	} else if (((base->contentType == XML_XSD10_SCHEMA_CONTENT_ELEMENTS) ||
	    (base->contentType == XML_XSD10_SCHEMA_CONTENT_MIXED)) &&
	    xml_xsd10_is_particle_emptiable(
		(XmlXsd10Particle *) base->subtypes)) {
	    /*
	    * SPEC (5.3.2.2) "The {content type} of the {base type
	    * definition} must be elementOnly or mixed and have a particle
	    * which is `emptiable` as defined in Particle Emptiable ($3.9.6)."
	    * PASS
	    */
	} else {
	    xml_xsd10_p_custom_err(ctxt,
		XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_1,
		XSD10_WXS_BASIC_CAST type, NULL,
		"The content type of the base type must be either "
		"empty or 'mixed' (or 'elements-only') and an emptiable "
		"particle", NULL);
	    return (ctxt->err);
	}
    } else if ((type->contentType == XML_XSD10_SCHEMA_CONTENT_ELEMENTS) ||
	XSD10_WXS_HAS_MIXED_CONTENT(type)) {
	/*
	* SPEC (5.4.1.1) "The {content type} of the complex type definition
	* itself must be element-only"
	*/
	if (XSD10_WXS_HAS_MIXED_CONTENT(type) && (! XSD10_WXS_HAS_MIXED_CONTENT(base))) {
	    /*
	    * SPEC (5.4.1.2) "The {content type} of the complex type
	    * definition itself and of the {base type definition} must be
	    * mixed"
	    */
	    xml_xsd10_p_custom_err(ctxt,
		XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_1,
		XSD10_WXS_BASIC_CAST type, NULL,
		"If the content type is 'mixed', then the content type of the "
		"base type must also be 'mixed'", NULL);
	    return (ctxt->err);
	}
	/*
	* SPEC (5.4.2) "The particle of the complex type definition itself
	* must be a `valid restriction` of the particle of the {content
	* type} of the {base type definition} as defined in Particle Valid
	* (Restriction) ($3.9.6).
	*
	* URGENT TODO: (5.4.2)
	*/
    } else {
	xml_xsd10_p_custom_err(ctxt,
	    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_1,
	    XSD10_WXS_BASIC_CAST type, NULL,
	    "The type is not a valid restriction of its base type", NULL);
	return (ctxt->err);
    }
    return (0);
}

/**
 * Port of xmlSchemaCheckCTComponent.
 *
 * (3.4.6) Constraints on Complex Type Definition Schema Components
 *
 * @param ctxt  the schema parser context
 * @param type  the complex type definition
 * @returns 0 if the constraints are satisfied, a positive
 * error code if not and -1 if an internal error occurred.
 */
int xml_xsd10_check_ct_component(XmlXsd10ParserCtxt *ctxt, XmlXsd10Type *type)
{
    int ret;
    /*
    * Complex Type Definition Properties Correct
    */
    ret = xml_xsd10_check_ct_props_correct(ctxt, type);
    if (ret != 0)
	return (ret);
    if (XSD10_WXS_IS_EXTENSION(type))
	ret = xml_xsd10_check_cosct_extends(ctxt, type);
    else
	ret = xml_xsd10_check_derivation_ok_restriction(ctxt, type);
    return (ret);
}

/**
 * Port of xmlSchemaCheckSRCCT.
 *
 * (3.4.3) Constraints on XML Representations of Complex Type Definitions:
 * Schema Representation Constraint:
 * Complex Type Definition Representation OK (src-ct)
 *
 * @param ctxt  the schema parser context
 * @param type  the complex type definition
 * @returns 0 if the constraints are satisfied, a positive
 * error code if not and -1 if an internal error occurred.
 */
int xml_xsd10_check_srcct(XmlXsd10ParserCtxt *ctxt, XmlXsd10Type *type)
{
    XmlXsd10Type *base;
    int ret = 0;

    /*
    * TODO: Adjust the error codes here, as I used
    * XML_SCHEMAP_SRC_CT_1 only yet.
    */
    base = type->baseType;
    if (! XSD10_WXS_HAS_SIMPLE_CONTENT(type)) {
	/*
	* 1 If the <complexContent> alternative is chosen, the type definition
	* `resolved` to by the `actual value` of the base [attribute]
	* must be a complex type definition;
	*/
	if (! XSD10_WXS_IS_COMPLEX(base)) {
	    char *str = NULL;
	    xml_xsd10_p_custom_err(ctxt,
		XML_XSD10_SCHEMAP_SRC_CT_1,
		XSD10_WXS_BASIC_CAST type, type->node,
		"If using <complexContent>, the base type is expected to be "
		"a complex type. The base type '%s' is a simple type",
		xml_xsd10_format_q_name(&str, base->targetNamespace,
		base->name));
	    XSD10_FREE_AND_NULL(str)
	    return (XML_XSD10_SCHEMAP_SRC_CT_1);
	}
    } else {
	/*
	* SPEC
	* 2 If the <simpleContent> alternative is chosen, all of the
	* following must be true:
	* 2.1 The type definition `resolved` to by the `actual value` of the
	* base [attribute] must be one of the following:
	*/
	if (XSD10_WXS_IS_SIMPLE(base)) {
	    if (XSD10_WXS_IS_EXTENSION(type) == 0) {
		char *str = NULL;
		/*
		* 2.1.3 only if the <extension> alternative is also
		* chosen, a simple type definition.
		*/
		/* TODO: Change error code to ..._SRC_CT_2_1_3. */
		xml_xsd10_p_custom_err(ctxt,
		    XML_XSD10_SCHEMAP_SRC_CT_1,
		    XSD10_WXS_BASIC_CAST type, NULL,
		    "If using <simpleContent> and <restriction>, the base "
		    "type must be a complex type. The base type '%s' is "
		    "a simple type",
		    xml_xsd10_format_q_name(&str, base->targetNamespace,
			base->name));
		XSD10_FREE_AND_NULL(str)
		return (XML_XSD10_SCHEMAP_SRC_CT_1);
	    }
	} else {
	    /* Base type is a complex type. */
	    if ((base->contentType == XML_XSD10_SCHEMA_CONTENT_SIMPLE) ||
		(base->contentType == XML_XSD10_SCHEMA_CONTENT_BASIC)) {
		/*
		* 2.1.1 a complex type definition whose {content type} is a
		* simple type definition;
		* PASS
		*/
		if (base->contentTypeDef == NULL) {
		    xml_xsd10_p_custom_err(ctxt, XML_XSD10_SCHEMAP_INTERNAL,
			XSD10_WXS_BASIC_CAST type, NULL,
			"Internal error: xmlSchemaCheckSRCCT, "
			"'%s', base type has no content type",
			type->name);
		    return (-1);
		}
	    } else if ((base->contentType == XML_XSD10_SCHEMA_CONTENT_MIXED) &&
		(XSD10_WXS_IS_RESTRICTION(type))) {

		/*
		* 2.1.2 only if the <restriction> alternative is also
		* chosen, a complex type definition whose {content type}
		* is mixed and a particle emptiable.
		*/
		if (! xml_xsd10_is_particle_emptiable(
		    (XmlXsd10Particle *) base->subtypes)) {
		    ret = XML_XSD10_SCHEMAP_SRC_CT_1;
		} else
		    /*
		    * Attention: at this point the <simpleType> child is in
		    * ->contentTypeDef (put there during parsing).
		    */
		    if (type->contentTypeDef == NULL) {
		    char *str = NULL;
		    /*
		    * 2.2 If clause 2.1.2 above is satisfied, then there
		    * must be a <simpleType> among the [children] of
		    * <restriction>.
		    */
		    /* TODO: Change error code to ..._SRC_CT_2_2. */
		    xml_xsd10_p_custom_err(ctxt,
			XML_XSD10_SCHEMAP_SRC_CT_1,
			XSD10_WXS_BASIC_CAST type, NULL,
			"A <simpleType> is expected among the children "
			"of <restriction>, if <simpleContent> is used and "
			"the base type '%s' is a complex type",
			xml_xsd10_format_q_name(&str, base->targetNamespace,
			base->name));
		    XSD10_FREE_AND_NULL(str)
		    return (XML_XSD10_SCHEMAP_SRC_CT_1);
		}
	    } else {
		ret = XML_XSD10_SCHEMAP_SRC_CT_1;
	    }
	}
	if (ret > 0) {
	    char *str = NULL;
	    if (XSD10_WXS_IS_RESTRICTION(type)) {
		xml_xsd10_p_custom_err(ctxt,
		    XML_XSD10_SCHEMAP_SRC_CT_1,
		    XSD10_WXS_BASIC_CAST type, NULL,
		    "If <simpleContent> and <restriction> is used, the "
		    "base type must be a simple type or a complex type with "
		    "mixed content and particle emptiable. The base type "
		    "'%s' is none of those",
		    xml_xsd10_format_q_name(&str, base->targetNamespace,
		    base->name));
	    } else {
		xml_xsd10_p_custom_err(ctxt,
		    XML_XSD10_SCHEMAP_SRC_CT_1,
		    XSD10_WXS_BASIC_CAST type, NULL,
		    "If <simpleContent> and <extension> is used, the "
		    "base type must be a simple type. The base type '%s' "
		    "is a complex type",
		    xml_xsd10_format_q_name(&str, base->targetNamespace,
		    base->name));
	    }
	    XSD10_FREE_AND_NULL(str)
	}
    }
    /*
    * SPEC (3) "The corresponding complex type definition component must
    * satisfy the conditions set out in Constraints on Complex Type
    * Definition Schema Components ($3.4.6);"
    * NOTE (3) will be done in xmlSchemaTypeFixup().
    */
    /*
    * SPEC (4) If clause 2.2.1 or clause 2.2.2 in the correspondence specification
    * above for {attribute wildcard} is satisfied, the intensional
    * intersection must be expressible, as defined in Attribute Wildcard
    * Intersection ($3.10.6).
    * NOTE (4) is done in xmlSchemaFixupTypeAttributeUses().
    */
    return (ret);
}


#define XSD10_FACET_RESTR_MUTUAL_ERR(fac1, fac2) \
    xml_xsd10_p_custom_err_ext(pctxt,      \
	XML_XSD10_SCHEMAP_INVALID_FACET_VALUE, \
	XSD10_WXS_BASIC_CAST fac1, fac1->node, \
	"It is an error for both '%s' and '%s' to be specified on the "\
	"same type definition", \
	xml_xsd10_facet_type_to_string(fac1->type), \
	xml_xsd10_facet_type_to_string(fac2->type), NULL);

#define XSD10_FACET_RESTR_ERR(fac1, msg) \
    xml_xsd10_p_custom_err(pctxt,      \
	XML_XSD10_SCHEMAP_INVALID_FACET_VALUE, \
	XSD10_WXS_BASIC_CAST fac1, fac1->node, \
	msg, NULL);

#define XSD10_FACET_RESTR_FIXED_ERR(fac) \
    xml_xsd10_p_custom_err(pctxt, \
	XML_XSD10_SCHEMAP_INVALID_FACET_VALUE, \
	XSD10_WXS_BASIC_CAST fac, fac->node, \
	"The base type's facet is 'fixed', thus the value must not " \
	"differ", NULL);

/** Port of xmlSchemaDeriveFacetErr. */
void xml_xsd10_derive_facet_err(XmlXsd10ParserCtxt *pctxt, XmlXsd10Facet *facet1, XmlXsd10Facet *facet2, int lessGreater, int orEqual, int ofBase)
{
    char *msg = NULL;

    msg = xml_xsd10_strdup("'");
    msg = xml_xsd10_strcat(msg, xml_xsd10_facet_type_to_string(facet1->type));
    msg = xml_xsd10_strcat(msg, "' has to be");
    if (lessGreater == 0)
	msg = xml_xsd10_strcat(msg, " equal to");
    if (lessGreater == 1)
	msg = xml_xsd10_strcat(msg, " greater than");
    else
	msg = xml_xsd10_strcat(msg, " less than");

    if (orEqual)
	msg = xml_xsd10_strcat(msg, " or equal to");
    msg = xml_xsd10_strcat(msg, " '");
    msg = xml_xsd10_strcat(msg, xml_xsd10_facet_type_to_string(facet2->type));
    if (ofBase)
	msg = xml_xsd10_strcat(msg, "' of the base type");
    else
	msg = xml_xsd10_strcat(msg, "'");

    xml_xsd10_p_custom_err(pctxt,
	XML_XSD10_SCHEMAP_INVALID_FACET_VALUE,
	XSD10_WXS_BASIC_CAST facet1, NULL,
	(const char *) msg, NULL);

    if (msg != NULL)
	free(msg);
}

/*
 * Port of xmlSchemaDeriveAndValidateFacets.
 *
* xmlSchemaDeriveAndValidateFacets:
*
* Schema Component Constraint: Simple Type Restriction (Facets)
* (st-restrict-facets)
*/
int xml_xsd10_derive_and_validate_facets(XmlXsd10ParserCtxt *pctxt, XmlXsd10Type *type)
{
    XmlXsd10Type *base = type->baseType;
    XmlXsd10FacetLink *link, *cur, *last = NULL;
    XmlXsd10Facet *facet, *bfacet, *flength = NULL, *ftotdig = NULL, *ffracdig = NULL, *fmaxlen = NULL, *fminlen = NULL, */* facets of the current type */
	fmininc = NULL, *fmaxinc = NULL, *fminexc = NULL, *fmaxexc = NULL, *bflength = NULL, *bftotdig = NULL, *bffracdig = NULL, *bfmaxlen = NULL, *bfminlen = NULL, */* facets of the base type */
	bfmininc = NULL, *bfmaxinc = NULL, *bfminexc = NULL, *bfmaxexc = NULL;
    int res; /* err = 0, fixedErr; */

    /*
    * SPEC st-restrict-facets 1:
    * "The {variety} of R is the same as that of B."
    */
    /*
    * SPEC st-restrict-facets 2:
    * "If {variety} is atomic, the {primitive type definition}
    * of R is the same as that of B."
    *
    * NOTE: we leave 1 & 2 out for now, since this will be
    * satisfied by the derivation process.
    * CONSTRUCTION TODO: Maybe needed if using a construction API.
    */
    /*
    * SPEC st-restrict-facets 3:
    * "The {facets} of R are the union of S and the {facets}
    * of B, eliminating duplicates. To eliminate duplicates,
    * when a facet of the same kind occurs in both S and the
    * {facets} of B, the one in the {facets} of B is not
    * included, with the exception of enumeration and pattern
    * facets, for which multiple occurrences with distinct values
    * are allowed."
    */

    if ((type->facetSet == NULL) && (base->facetSet == NULL))
	return (0);

    last = type->facetSet;
    if (last != NULL)
	while (last->next != NULL)
	    last = last->next;

    for (cur = type->facetSet; cur != NULL; cur = cur->next) {
	facet = cur->facet;
	switch (facet->type) {
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
    for (cur = base->facetSet; cur != NULL; cur = cur->next) {
	facet = cur->facet;
	switch (facet->type) {
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
    if (flength && (fminlen || fmaxlen)) {
	XSD10_FACET_RESTR_ERR(flength, "It is an error for both 'length' and "
	    "either of 'minLength' or 'maxLength' to be specified on "
	    "the same type definition")
    }
    /*
    * Mutual exclusions in the same derivation step.
    */
    if ((fmaxinc) && (fmaxexc)) {
	/*
	* SCC "maxInclusive and maxExclusive"
	*/
	XSD10_FACET_RESTR_MUTUAL_ERR(fmaxinc, fmaxexc)
    }
    if ((fmininc) && (fminexc)) {
	/*
	* SCC "minInclusive and minExclusive"
	*/
	XSD10_FACET_RESTR_MUTUAL_ERR(fmininc, fminexc)
    }

    if (flength && bflength) {
	/*
	* SCC "length valid restriction"
	* The values have to be equal.
	*/
	res = xml_xsd10_compare_values(flength->val, bflength->val);
	if (res == -2)
	    goto internal_error;
	if (res != 0)
	    xml_xsd10_derive_facet_err(pctxt, flength, bflength, 0, 0, 1);
	if ((res != 0) && (bflength->fixed)) {
	    XSD10_FACET_RESTR_FIXED_ERR(flength)
	}

    }
    if (fminlen && bfminlen) {
	/*
	* SCC "minLength valid restriction"
	* minLength >= BASE minLength
	*/
	res = xml_xsd10_compare_values(fminlen->val, bfminlen->val);
	if (res == -2)
	    goto internal_error;
	if (res == -1)
	    xml_xsd10_derive_facet_err(pctxt, fminlen, bfminlen, 1, 1, 1);
	if ((res != 0) && (bfminlen->fixed)) {
	    XSD10_FACET_RESTR_FIXED_ERR(fminlen)
	}
    }
    if (fmaxlen && bfmaxlen) {
	/*
	* SCC "maxLength valid restriction"
	* maxLength <= BASE minLength
	*/
	res = xml_xsd10_compare_values(fmaxlen->val, bfmaxlen->val);
	if (res == -2)
	    goto internal_error;
	if (res == 1)
	    xml_xsd10_derive_facet_err(pctxt, fmaxlen, bfmaxlen, -1, 1, 1);
	if ((res != 0) && (bfmaxlen->fixed)) {
	    XSD10_FACET_RESTR_FIXED_ERR(fmaxlen)
	}
    }
    /*
    * SCC "length and minLength or maxLength"
    */
    if (! flength)
	flength = bflength;
    if (flength) {
	if (! fminlen)
	    fminlen = bfminlen;
	if (fminlen) {
	    /* (1.1) length >= minLength */
	    res = xml_xsd10_compare_values(flength->val, fminlen->val);
	    if (res == -2)
		goto internal_error;
	    if (res == -1)
		xml_xsd10_derive_facet_err(pctxt, flength, fminlen, 1, 1, 0);
	}
	if (! fmaxlen)
	    fmaxlen = bfmaxlen;
	if (fmaxlen) {
	    /* (2.1) length <= maxLength */
	    res = xml_xsd10_compare_values(flength->val, fmaxlen->val);
	    if (res == -2)
		goto internal_error;
	    if (res == 1)
		xml_xsd10_derive_facet_err(pctxt, flength, fmaxlen, -1, 1, 0);
	}
    }
    if (fmaxinc) {
	/*
	* "maxInclusive"
	*/
	if (fmininc) {
	    /* SCC "maxInclusive >= minInclusive" */
	    res = xml_xsd10_compare_values(fmaxinc->val, fmininc->val);
	    if (res == -2)
		goto internal_error;
	    if (res == -1) {
		xml_xsd10_derive_facet_err(pctxt, fmaxinc, fmininc, 1, 1, 0);
	    }
	}
	/*
	* SCC "maxInclusive valid restriction"
	*/
	if (bfmaxinc) {
	    /* maxInclusive <= BASE maxInclusive */
	    res = xml_xsd10_compare_values(fmaxinc->val, bfmaxinc->val);
	    if (res == -2)
		goto internal_error;
	    if (res == 1)
		xml_xsd10_derive_facet_err(pctxt, fmaxinc, bfmaxinc, -1, 1, 1);
	    if ((res != 0) && (bfmaxinc->fixed)) {
		XSD10_FACET_RESTR_FIXED_ERR(fmaxinc)
	    }
	}
	if (bfmaxexc) {
	    /* maxInclusive < BASE maxExclusive */
	    res = xml_xsd10_compare_values(fmaxinc->val, bfmaxexc->val);
	    if (res == -2)
		goto internal_error;
	    if (res != -1) {
		xml_xsd10_derive_facet_err(pctxt, fmaxinc, bfmaxexc, -1, 0, 1);
	    }
	}
	if (bfmininc) {
	    /* maxInclusive >= BASE minInclusive */
	    res = xml_xsd10_compare_values(fmaxinc->val, bfmininc->val);
	    if (res == -2)
		goto internal_error;
	    if (res == -1) {
		xml_xsd10_derive_facet_err(pctxt, fmaxinc, bfmininc, 1, 1, 1);
	    }
	}
	if (bfminexc) {
	    /* maxInclusive > BASE minExclusive */
	    res = xml_xsd10_compare_values(fmaxinc->val, bfminexc->val);
	    if (res == -2)
		goto internal_error;
	    if (res != 1) {
		xml_xsd10_derive_facet_err(pctxt, fmaxinc, bfminexc, 1, 0, 1);
	    }
	}
    }
    if (fmaxexc) {
	/*
	* "maxExclusive >= minExclusive"
	*/
	if (fminexc) {
	    res = xml_xsd10_compare_values(fmaxexc->val, fminexc->val);
	    if (res == -2)
		goto internal_error;
	    if (res == -1) {
		xml_xsd10_derive_facet_err(pctxt, fmaxexc, fminexc, 1, 1, 0);
	    }
	}
	/*
	* "maxExclusive valid restriction"
	*/
	if (bfmaxexc) {
	    /* maxExclusive <= BASE maxExclusive */
	    res = xml_xsd10_compare_values(fmaxexc->val, bfmaxexc->val);
	    if (res == -2)
		goto internal_error;
	    if (res == 1) {
		xml_xsd10_derive_facet_err(pctxt, fmaxexc, bfmaxexc, -1, 1, 1);
	    }
	    if ((res != 0) && (bfmaxexc->fixed)) {
		XSD10_FACET_RESTR_FIXED_ERR(fmaxexc)
	    }
	}
	if (bfmaxinc) {
	    /* maxExclusive <= BASE maxInclusive */
	    res = xml_xsd10_compare_values(fmaxexc->val, bfmaxinc->val);
	    if (res == -2)
		goto internal_error;
	    if (res == 1) {
		xml_xsd10_derive_facet_err(pctxt, fmaxexc, bfmaxinc, -1, 1, 1);
	    }
	}
	if (bfmininc) {
	    /* maxExclusive > BASE minInclusive */
	    res = xml_xsd10_compare_values(fmaxexc->val, bfmininc->val);
	    if (res == -2)
		goto internal_error;
	    if (res != 1) {
		xml_xsd10_derive_facet_err(pctxt, fmaxexc, bfmininc, 1, 0, 1);
	    }
	}
	if (bfminexc) {
	    /* maxExclusive > BASE minExclusive */
	    res = xml_xsd10_compare_values(fmaxexc->val, bfminexc->val);
	    if (res == -2)
		goto internal_error;
	    if (res != 1) {
		xml_xsd10_derive_facet_err(pctxt, fmaxexc, bfminexc, 1, 0, 1);
	    }
	}
    }
    if (fminexc) {
	/*
	* "minExclusive < maxInclusive"
	*/
	if (fmaxinc) {
	    res = xml_xsd10_compare_values(fminexc->val, fmaxinc->val);
	    if (res == -2)
		goto internal_error;
	    if (res != -1) {
		xml_xsd10_derive_facet_err(pctxt, fminexc, fmaxinc, -1, 0, 0);
	    }
	}
	/*
	* "minExclusive valid restriction"
	*/
	if (bfminexc) {
	    /* minExclusive >= BASE minExclusive */
	    res = xml_xsd10_compare_values(fminexc->val, bfminexc->val);
	    if (res == -2)
		goto internal_error;
	    if (res == -1) {
		xml_xsd10_derive_facet_err(pctxt, fminexc, bfminexc, 1, 1, 1);
	    }
	    if ((res != 0) && (bfminexc->fixed)) {
		XSD10_FACET_RESTR_FIXED_ERR(fminexc)
	    }
	}
	if (bfmaxinc) {
	    /* minExclusive <= BASE maxInclusive */
	    res = xml_xsd10_compare_values(fminexc->val, bfmaxinc->val);
	    if (res == -2)
		goto internal_error;
	    if (res == 1) {
		xml_xsd10_derive_facet_err(pctxt, fminexc, bfmaxinc, -1, 1, 1);
	    }
	}
	if (bfmininc) {
	    /* minExclusive >= BASE minInclusive */
	    res = xml_xsd10_compare_values(fminexc->val, bfmininc->val);
	    if (res == -2)
		goto internal_error;
	    if (res == -1) {
		xml_xsd10_derive_facet_err(pctxt, fminexc, bfmininc, 1, 1, 1);
	    }
	}
	if (bfmaxexc) {
	    /* minExclusive < BASE maxExclusive */
	    res = xml_xsd10_compare_values(fminexc->val, bfmaxexc->val);
	    if (res == -2)
		goto internal_error;
	    if (res != -1) {
		xml_xsd10_derive_facet_err(pctxt, fminexc, bfmaxexc, -1, 0, 1);
	    }
	}
    }
    if (fmininc) {
	/*
	* "minInclusive < maxExclusive"
	*/
	if (fmaxexc) {
	    res = xml_xsd10_compare_values(fmininc->val, fmaxexc->val);
	    if (res == -2)
		goto internal_error;
	    if (res != -1) {
		xml_xsd10_derive_facet_err(pctxt, fmininc, fmaxexc, -1, 0, 0);
	    }
	}
	/*
	* "minExclusive valid restriction"
	*/
	if (bfmininc) {
	    /* minInclusive >= BASE minInclusive */
	    res = xml_xsd10_compare_values(fmininc->val, bfmininc->val);
	    if (res == -2)
		goto internal_error;
	    if (res == -1) {
		xml_xsd10_derive_facet_err(pctxt, fmininc, bfmininc, 1, 1, 1);
	    }
	    if ((res != 0) && (bfmininc->fixed)) {
		XSD10_FACET_RESTR_FIXED_ERR(fmininc)
	    }
	}
	if (bfmaxinc) {
	    /* minInclusive <= BASE maxInclusive */
	    res = xml_xsd10_compare_values(fmininc->val, bfmaxinc->val);
	    if (res == -2)
		goto internal_error;
	    if (res == 1) {
		xml_xsd10_derive_facet_err(pctxt, fmininc, bfmaxinc, -1, 1, 1);
	    }
	}
	if (bfminexc) {
	    /* minInclusive > BASE minExclusive */
	    res = xml_xsd10_compare_values(fmininc->val, bfminexc->val);
	    if (res == -2)
		goto internal_error;
	    if (res != 1)
		xml_xsd10_derive_facet_err(pctxt, fmininc, bfminexc, 1, 0, 1);
	}
	if (bfmaxexc) {
	    /* minInclusive < BASE maxExclusive */
	    res = xml_xsd10_compare_values(fmininc->val, bfmaxexc->val);
	    if (res == -2)
		goto internal_error;
	    if (res != -1)
		xml_xsd10_derive_facet_err(pctxt, fmininc, bfmaxexc, -1, 0, 1);
	}
    }
    if (ftotdig && bftotdig) {
	/*
	* SCC " totalDigits valid restriction"
	* totalDigits <= BASE totalDigits
	*/
	res = xml_xsd10_compare_values(ftotdig->val, bftotdig->val);
	if (res == -2)
	    goto internal_error;
	if (res == 1)
	    xml_xsd10_derive_facet_err(pctxt, ftotdig, bftotdig,
	    -1, 1, 1);
	if ((res != 0) && (bftotdig->fixed)) {
	    XSD10_FACET_RESTR_FIXED_ERR(ftotdig)
	}
    }
    if (ffracdig && bffracdig) {
	/*
	* SCC  "fractionDigits valid restriction"
	* fractionDigits <= BASE fractionDigits
	*/
	res = xml_xsd10_compare_values(ffracdig->val, bffracdig->val);
	if (res == -2)
	    goto internal_error;
	if (res == 1)
	    xml_xsd10_derive_facet_err(pctxt, ffracdig, bffracdig,
	    -1, 1, 1);
	if ((res != 0) && (bffracdig->fixed)) {
	    XSD10_FACET_RESTR_FIXED_ERR(ffracdig)
	}
    }
    /*
    * SCC "fractionDigits less than or equal to totalDigits"
    */
    if (! ftotdig)
	ftotdig = bftotdig;
    if (! ffracdig)
	ffracdig = bffracdig;
    if (ftotdig && ffracdig) {
	res = xml_xsd10_compare_values(ffracdig->val, ftotdig->val);
	if (res == -2)
	    goto internal_error;
	if (res == 1)
	    xml_xsd10_derive_facet_err(pctxt, ffracdig, ftotdig,
		-1, 1, 0);
    }
    /*
    * *Enumerations* won' be added here, since only the first set
    * of enumerations in the ancestor-or-self axis is used
    * for validation, plus we need to use the base type of those
    * enumerations for whitespace.
    *
    * *Patterns*: won't be add here, since they are ORed at
    * type level and ANDed at ancestor level. This will
    * happen during validation by walking the base axis
    * of the type.
    */
    for (cur = base->facetSet; cur != NULL; cur = cur->next) {
	bfacet = cur->facet;
	/*
	* Special handling of enumerations and patterns.
	* TODO: hmm, they should not appear in the set, so remove this.
	*/
	if ((bfacet->type == XML_XSD10_SCHEMA_FACET_PATTERN) ||
	    (bfacet->type == XML_XSD10_SCHEMA_FACET_ENUMERATION))
	    continue;
	/*
	* Search for a duplicate facet in the current type.
	*/
	link = type->facetSet;
	/* err = 0; */
	/* fixedErr = 0; */
	while (link != NULL) {
	    facet = link->facet;
	    if (facet->type == bfacet->type) {
		switch (facet->type) {
		    case XML_XSD10_SCHEMA_FACET_WHITESPACE:
			/*
			* The whitespace must be stronger.
			*/
			if (facet->whitespace < bfacet->whitespace) {
			    XSD10_FACET_RESTR_ERR(facet,
				"The 'whitespace' value has to be equal to "
				"or stronger than the 'whitespace' value of "
				"the base type")
			}
			if ((bfacet->fixed) &&
			    (facet->whitespace != bfacet->whitespace)) {
			    XSD10_FACET_RESTR_FIXED_ERR(facet)
			}
			break;
		    default:
			break;
		}
		/* Duplicate found. */
		break;
	    }
	    link = link->next;
	}
	/*
	* If no duplicate was found: add the base types's facet
	* to the set.
	*/
	if (link == NULL) {
	    link = (XmlXsd10FacetLink *)
		malloc(sizeof(XmlXsd10FacetLink));
	    if (link == NULL) {
		xml_xsd10_p_err_memory(pctxt);
		return (-1);
	    }
	    link->facet = cur->facet;
	    link->next = NULL;
	    if (last == NULL)
		type->facetSet = link;
	    else
		last->next = link;
	    last = link;
	}

    }

    return (0);
internal_error:
    XSD10_PERROR_INT("xmlSchemaDeriveAndValidateFacets",
	"an error occurred");
    return (-1);
}

/** Port of xmlSchemaFinishMemberTypeDefinitionsProperty. */
int xml_xsd10_finish_member_type_definitions_property(XmlXsd10ParserCtxt *pctxt, XmlXsd10Type *type)
{
    XmlXsd10TypeLink *link, *lastLink, *prevLink, *subLink, *newLink;
    /*
    * The actual value is then formed by replacing any union type
    * definition in the `explicit members` with the members of their
    * {member type definitions}, in order.
    *
    * TODO: There's a bug entry at
    * "http://lists.w3.org/Archives/Public/www-xml-schema-comments/2005JulSep/0287.html"
    * which indicates that we'll keep the union types the future.
    */
    link = type->memberTypes;
    while (link != NULL) {

	if (XSD10_WXS_IS_TYPE_NOT_FIXED(link->type))
	    xml_xsd10_type_fixup(link->type, XSD10_ACTXT_CAST pctxt);

	if (XSD10_WXS_IS_UNION(link->type)) {
	    subLink = xml_xsd10_get_union_simple_type_member_types(link->type);
	    if (subLink != NULL) {
		link->type = subLink->type;
		if (subLink->next != NULL) {
		    lastLink = link->next;
		    subLink = subLink->next;
		    prevLink = link;
		    while (subLink != NULL) {
			newLink = (XmlXsd10TypeLink *)
			    malloc(sizeof(XmlXsd10TypeLink));
			if (newLink == NULL) {
			    xml_xsd10_p_err_memory(pctxt);
			    return (-1);
			}
			newLink->type = subLink->type;
			prevLink->next = newLink;
			prevLink = newLink;
			newLink->next = lastLink;

			subLink = subLink->next;
		    }
		}
	    }
	}
	link = link->next;
    }
    return (0);
}

/** Port of xmlSchemaTypeFixupOptimFacets. */
void xml_xsd10_type_fixup_optim_facets(XmlXsd10Type *type)
{
    int has = 0, needVal = 0, normVal = 0;

    has	= (type->baseType->flags & XML_XSD10_SCHEMAS_TYPE_HAS_FACETS) ? 1 : 0;
    if (has) {
	needVal = (type->baseType->flags &
	    XML_XSD10_SCHEMAS_TYPE_FACETSNEEDVALUE) ? 1 : 0;
	normVal = (type->baseType->flags &
	    XML_XSD10_SCHEMAS_TYPE_NORMVALUENEEDED) ? 1 : 0;
    }
    if (type->facets != NULL) {
	XmlXsd10Facet *fac;

	for (fac = type->facets; fac != NULL; fac = fac->next) {
	    switch (fac->type) {
		case XML_XSD10_SCHEMA_FACET_WHITESPACE:
		    break;
		case XML_XSD10_SCHEMA_FACET_PATTERN:
		    normVal = 1;
		    has = 1;
		    break;
		case XML_XSD10_SCHEMA_FACET_ENUMERATION:
		    needVal = 1;
		    normVal = 1;
		    has = 1;
		    break;
		default:
		    has = 1;
		    break;
	    }
	}
    }
    if (normVal)
	type->flags |= XML_XSD10_SCHEMAS_TYPE_NORMVALUENEEDED;
    if (needVal)
	type->flags |= XML_XSD10_SCHEMAS_TYPE_FACETSNEEDVALUE;
    if (has)
	type->flags |= XML_XSD10_SCHEMAS_TYPE_HAS_FACETS;

    if (has && (! needVal) && XSD10_WXS_IS_ATOMIC(type)) {
	XmlXsd10Type *prim = xml_xsd10_get_primitive_type(type);
	/*
	* OPTIMIZE VAL TODO: Some facets need a computed value.
	*/
	if ((prim->builtInType != XML_XSD10_SCHEMAS_ANYSIMPLETYPE) &&
	    (prim->builtInType != XML_XSD10_SCHEMAS_STRING)) {
	    type->flags |= XML_XSD10_SCHEMAS_TYPE_FACETSNEEDVALUE;
	}
    }
}

/** Port of xmlSchemaTypeFixupWhitespace. */
int xml_xsd10_type_fixup_whitespace(XmlXsd10Type *type)
{


    /*
    * Evaluate the whitespace-facet value.
    */
    if (XSD10_WXS_IS_LIST(type)) {
	type->flags |= XML_XSD10_SCHEMAS_TYPE_WHITESPACE_COLLAPSE;
	return (0);
    } else if (XSD10_WXS_IS_UNION(type))
	return (0);

    if (type->facetSet != NULL) {
	XmlXsd10FacetLink *lin;

	for (lin = type->facetSet; lin != NULL; lin = lin->next) {
	    if (lin->facet->type == XML_XSD10_SCHEMA_FACET_WHITESPACE) {
		switch (lin->facet->whitespace) {
		case XML_XSD10_SCHEMAS_FACET_PRESERVE:
		    type->flags |= XML_XSD10_SCHEMAS_TYPE_WHITESPACE_PRESERVE;
		    break;
		case XML_XSD10_SCHEMAS_FACET_REPLACE:
		    type->flags |= XML_XSD10_SCHEMAS_TYPE_WHITESPACE_REPLACE;
		    break;
		case XML_XSD10_SCHEMAS_FACET_COLLAPSE:
		    type->flags |= XML_XSD10_SCHEMAS_TYPE_WHITESPACE_COLLAPSE;
		    break;
		default:
		    return (-1);
		}
		return (0);
	    }
	}
    }
    /*
    * For all `atomic` datatypes other than string (and types `derived`
    * by `restriction` from it) the value of whiteSpace is fixed to
    * collapse
    */
    {
	XmlXsd10Type *anc;

	for (anc = type->baseType; anc != NULL &&
		anc->builtInType != XML_XSD10_SCHEMAS_ANYTYPE;
		anc = anc->baseType) {

	    if (anc->type == XML_XSD10_SCHEMA_TYPE_BASIC) {
		if (anc->builtInType == XML_XSD10_SCHEMAS_NORMSTRING) {
		    type->flags |= XML_XSD10_SCHEMAS_TYPE_WHITESPACE_REPLACE;

		} else if ((anc->builtInType == XML_XSD10_SCHEMAS_STRING) ||
		    (anc->builtInType == XML_XSD10_SCHEMAS_ANYSIMPLETYPE)) {
		    type->flags |= XML_XSD10_SCHEMAS_TYPE_WHITESPACE_PRESERVE;

		} else
		    type->flags |= XML_XSD10_SCHEMAS_TYPE_WHITESPACE_COLLAPSE;
		break;
	    }
	}
    }
    return (0);
}

/** Port of xmlSchemaFixupSimpleTypeStageOne. */
int xml_xsd10_fixup_simple_type_stage_one(XmlXsd10ParserCtxt *pctxt, XmlXsd10Type *type)
{
    if (type->type != XML_XSD10_SCHEMA_TYPE_SIMPLE)
	return(0);
    if (! XSD10_WXS_IS_TYPE_NOT_FIXED_1(type))
	return(0);
    type->flags |= XML_XSD10_SCHEMAS_TYPE_FIXUP_1;

    if (XSD10_WXS_IS_LIST(type)) {
	/*
	* Corresponds to <simpleType><list>...
	*/
	if (type->subtypes == NULL) {
	    /*
	    * This one is really needed, so get out.
	    */
	    XSD10_PERROR_INT("xmlSchemaFixupSimpleTypeStageOne",
		"list type has no item-type assigned");
	    return(-1);
	}
    } else if (XSD10_WXS_IS_UNION(type)) {
	/*
	* Corresponds to <simpleType><union>...
	*/
	if (type->memberTypes == NULL) {
	    /*
	    * This one is really needed, so get out.
	    */
	    XSD10_PERROR_INT("xmlSchemaFixupSimpleTypeStageOne",
		"union type has no member-types assigned");
	    return(-1);
	}
    } else {
	/*
	* Corresponds to <simpleType><restriction>...
	*/
	if (type->baseType == NULL) {
	    XSD10_PERROR_INT("xmlSchemaFixupSimpleTypeStageOne",
		"type has no base-type assigned");
	    return(-1);
	}
	if (XSD10_WXS_IS_TYPE_NOT_FIXED_1(type->baseType))
	    if (xml_xsd10_fixup_simple_type_stage_one(pctxt, type->baseType) == -1)
		return(-1);
	/*
	* Variety
	* If the <restriction> alternative is chosen, then the
	* {variety} of the {base type definition}.
	*/
	if (XSD10_WXS_IS_ATOMIC(type->baseType))
	    type->flags |= XML_XSD10_SCHEMAS_TYPE_VARIETY_ATOMIC;
	else if (XSD10_WXS_IS_LIST(type->baseType)) {
	    type->flags |= XML_XSD10_SCHEMAS_TYPE_VARIETY_LIST;
	    /*
	    * Inherit the itemType.
	    */
	    type->subtypes = type->baseType->subtypes;
	} else if (XSD10_WXS_IS_UNION(type->baseType)) {
	    type->flags |= XML_XSD10_SCHEMAS_TYPE_VARIETY_UNION;
	    /*
	    * NOTE that we won't assign the memberTypes of the base,
	    * since this will make trouble when freeing them; we will
	    * use a lookup function to access them instead.
	    */
	}
    }
    return(0);
}

/*
 * Port of xmlSchemaFixupSimpleTypeStageTwo.
 *
* 3.14.6 Constraints on Simple Type Definition Schema Components
*/
int xml_xsd10_fixup_simple_type_stage_two(XmlXsd10ParserCtxt *pctxt, XmlXsd10Type *type)
{
    int res, olderrs = pctxt->nberrors;

    if (type->type != XML_XSD10_SCHEMA_TYPE_SIMPLE)
	return(-1);

    if (! XSD10_WXS_IS_TYPE_NOT_FIXED(type))
	return(0);

    type->flags |= XML_XSD10_SCHEMAS_TYPE_INTERNAL_RESOLVED;
    type->contentType = XML_XSD10_SCHEMA_CONTENT_SIMPLE;

    if (type->baseType == NULL) {
	XSD10_PERROR_INT("xmlSchemaFixupSimpleTypeStageTwo",
	    "missing baseType");
	goto exit_failure;
    }
    if (XSD10_WXS_IS_TYPE_NOT_FIXED(type->baseType))
	xml_xsd10_type_fixup(type->baseType, XSD10_ACTXT_CAST pctxt);
    /*
    * If a member type of a union is a union itself, we need to substitute
    * that member type for its member types.
    * NOTE that this might change in WXS 1.1; i.e. we will keep the union
    * types in WXS 1.1.
    */
    if ((type->memberTypes != NULL) &&
	(xml_xsd10_finish_member_type_definitions_property(pctxt, type) == -1))
	return(-1);
    /*
    * SPEC src-simple-type 1
    * "The corresponding simple type definition, if any, must satisfy
    * the conditions set out in Constraints on Simple Type Definition
    * Schema Components ($3.14.6)."
    */
    /*
    * Schema Component Constraint: Simple Type Definition Properties Correct
    * (st-props-correct)
    */
    res = xml_xsd10_check_st_props_correct(pctxt, type);
    XSD10_HFAILURE XSD10_HERROR
    /*
    * Schema Component Constraint: Derivation Valid (Restriction, Simple)
    * (cos-st-restricts)
    */
    res = xml_xsd10_check_cosst_restricts(pctxt, type);
    XSD10_HFAILURE XSD10_HERROR
    /*
    * TODO: Removed the error report, since it got annoying to get an
    * extra error report, if anything failed until now.
    * Enable this if needed.
    *
    * xmlSchemaPErr(ctxt, type->node,
    *    XML_SCHEMAP_SRC_SIMPLE_TYPE_1,
    *    "Simple type '%s' does not satisfy the constraints "
    *    "on simple type definitions.\n",
    *    type->name, NULL);
    */
    /*
    * Schema Component Constraint: Simple Type Restriction (Facets)
    * (st-restrict-facets)
    */
    res = xml_xsd10_check_facet_values(type, pctxt);
    XSD10_HFAILURE XSD10_HERROR
    if ((type->facetSet != NULL) ||
	(type->baseType->facetSet != NULL)) {
	res = xml_xsd10_derive_and_validate_facets(pctxt, type);
	XSD10_HFAILURE XSD10_HERROR
    }
    /*
    * Whitespace value.
    */
    res = xml_xsd10_type_fixup_whitespace(type);
    XSD10_HFAILURE XSD10_HERROR
    xml_xsd10_type_fixup_optim_facets(type);

exit_error:
    if (olderrs != pctxt->nberrors)
	return(pctxt->err);
    return(0);

exit_failure:
    return(-1);
}

/** Port of xmlSchemaFixupComplexType. */
int xml_xsd10_fixup_complex_type(XmlXsd10ParserCtxt *pctxt, XmlXsd10Type *type)
{
    int res = 0, olderrs = pctxt->nberrors;
    XmlXsd10Type *baseType = type->baseType;

    if (! XSD10_WXS_IS_TYPE_NOT_FIXED(type))
	return(0);
    type->flags |= XML_XSD10_SCHEMAS_TYPE_INTERNAL_RESOLVED;
    if (baseType == NULL) {
	XSD10_PERROR_INT("xmlSchemaFixupComplexType",
	    "missing baseType");
	goto exit_failure;
    }
    /*
    * Fixup the base type.
    */
    if (XSD10_WXS_IS_TYPE_NOT_FIXED(baseType))
	xml_xsd10_type_fixup(baseType, XSD10_ACTXT_CAST pctxt);
    if (baseType->flags & XML_XSD10_SCHEMAS_TYPE_INTERNAL_INVALID) {
	/*
	* Skip fixup if the base type is invalid.
	* TODO: Generate a warning!
	*/
	return(0);
    }
    /*
    * This basically checks if the base type can be derived.
    */
    res = xml_xsd10_check_srcct(pctxt, type);
    XSD10_HFAILURE XSD10_HERROR
    /*
    * Fixup the content type.
    */
    if (type->contentType == XML_XSD10_SCHEMA_CONTENT_SIMPLE) {
	/*
	* Corresponds to <complexType><simpleContent>...
	*/
	if ((XSD10_WXS_IS_COMPLEX(baseType)) &&
	    (baseType->contentTypeDef != NULL) &&
	    (XSD10_WXS_IS_RESTRICTION(type))) {
	    XmlXsd10Type *contentBase, *content;
	    /*
	    * SPEC (1) If <restriction> + base type is <complexType>,
	    * "whose own {content type} is a simple type..."
	    */
	    if (type->contentTypeDef != NULL) {
		/*
		* SPEC (1.1) "the simple type definition corresponding to the
		* <simpleType> among the [children] of <restriction> if there
		* is one;"
		* Note that this "<simpleType> among the [children]" was put
		* into ->contentTypeDef during parsing.
		*/
		contentBase = type->contentTypeDef;
		type->contentTypeDef = NULL;
	    } else {
		/*
		* (1.2) "...otherwise (<restriction> has no <simpleType>
		* among its [children]), the simple type definition which
		* is the {content type} of the ... base type."
		*/
		contentBase = baseType->contentTypeDef;
	    }
	    /*
	    * SPEC
	    * "... a simple type definition which restricts the simple
	    * type definition identified in clause 1.1 or clause 1.2
	    * with a set of facet components"
	    *
	    * Create the anonymous simple type, which will be the content
	    * type of the complex type.
	    */
	    content = xml_xsd10_add_type(pctxt, pctxt->schema,
		XML_XSD10_SCHEMA_TYPE_SIMPLE, NULL, type->targetNamespace,
		type->node, 0);
	    if (content == NULL)
		goto exit_failure;
	    /*
	    * We will use the same node as for the <complexType>
	    * to have it somehow anchored in the schema doc.
	    */
	    content->type = XML_XSD10_SCHEMA_TYPE_SIMPLE;
	    content->baseType = contentBase;
	    /*
	    * Move the facets, previously anchored on the
	    * complexType during parsing.
	    */
	    content->facets = type->facets;
	    type->facets = NULL;
	    content->facetSet = type->facetSet;
	    type->facetSet = NULL;

	    type->contentTypeDef = content;
	    if (XSD10_WXS_IS_TYPE_NOT_FIXED(contentBase))
		xml_xsd10_type_fixup(contentBase, XSD10_ACTXT_CAST pctxt);
	    /*
	    * Fixup the newly created type. We don't need to check
	    * for circularity here.
	    */
	    res = xml_xsd10_fixup_simple_type_stage_one(pctxt, content);
	    XSD10_HFAILURE XSD10_HERROR
	    res = xml_xsd10_fixup_simple_type_stage_two(pctxt, content);
	    XSD10_HFAILURE XSD10_HERROR

	} else if ((XSD10_WXS_IS_COMPLEX(baseType)) &&
	    (baseType->contentType == XML_XSD10_SCHEMA_CONTENT_MIXED) &&
	    (XSD10_WXS_IS_RESTRICTION(type))) {
	    /*
	    * SPEC (2) If <restriction> + base is a mixed <complexType> with
	    * an emptiable particle, then a simple type definition which
	    * restricts the <restriction>'s <simpleType> child.
	    */
	    if ((type->contentTypeDef == NULL) ||
		(type->contentTypeDef->baseType == NULL)) {
		/*
		* TODO: Check if this ever happens.
		*/
		xml_xsd10_p_custom_err(pctxt,
		    XML_XSD10_SCHEMAP_INTERNAL,
		    XSD10_WXS_BASIC_CAST type, NULL,
		    "Internal error: xmlSchemaTypeFixup, "
		    "complex type '%s': the <simpleContent><restriction> "
		    "is missing a <simpleType> child, but was not caught "
		    "by xmlSchemaCheckSRCCT()", type->name);
		goto exit_failure;
	    }
	} else if ((XSD10_WXS_IS_COMPLEX(baseType)) && XSD10_WXS_IS_EXTENSION(type)) {
	    /*
	    * SPEC (3) If <extension> + base is <complexType> with
	    * <simpleType> content, "...then the {content type} of that
	    * complex type definition"
	    */
	    if (baseType->contentTypeDef == NULL) {
		/*
		* TODO: Check if this ever happens. xmlSchemaCheckSRCCT
		* should have caught this already.
		*/
		xml_xsd10_p_custom_err(pctxt,
		    XML_XSD10_SCHEMAP_INTERNAL,
		    XSD10_WXS_BASIC_CAST type, NULL,
		    "Internal error: xmlSchemaTypeFixup, "
		    "complex type '%s': the <extension>ed base type is "
		    "a complex type with no simple content type",
		    type->name);
		goto exit_failure;
	    }
	    type->contentTypeDef = baseType->contentTypeDef;
	} else if ((XSD10_WXS_IS_SIMPLE(baseType)) && XSD10_WXS_IS_EXTENSION(type)) {
	    /*
	    * SPEC (4) <extension> + base is <simpleType>
	    * "... then that simple type definition"
	    */
	    type->contentTypeDef = baseType;
	} else {
	    /*
	    * TODO: Check if this ever happens.
	    */
	    xml_xsd10_p_custom_err(pctxt,
		XML_XSD10_SCHEMAP_INTERNAL,
		XSD10_WXS_BASIC_CAST type, NULL,
		"Internal error: xmlSchemaTypeFixup, "
		"complex type '%s' with <simpleContent>: unhandled "
		"derivation case", type->name);
	    goto exit_failure;
	}
    } else {
	int dummySequence = 0;
	XmlXsd10Particle *particle =
	    (XmlXsd10Particle *) type->subtypes;
	/*
	* Corresponds to <complexType><complexContent>...
	*
	* NOTE that the effective mixed was already set during parsing of
	* <complexType> and <complexContent>; its flag value is
	* XML_SCHEMAS_TYPE_MIXED.
	*
	* Compute the "effective content":
	* (2.1.1) + (2.1.2) + (2.1.3)
	*/
	if ((particle == NULL) ||
	    ((particle->type == XML_XSD10_SCHEMA_TYPE_PARTICLE) &&
	    ((particle->children->type == XML_XSD10_SCHEMA_TYPE_ALL) ||
	    (particle->children->type == XML_XSD10_SCHEMA_TYPE_SEQUENCE) ||
	    ((particle->children->type == XML_XSD10_SCHEMA_TYPE_CHOICE) &&
	    (particle->minOccurs == 0))) &&
	    ( ((XmlXsd10TreeItem *) particle->children)->children == NULL))) {
	    if (type->flags & XML_XSD10_SCHEMAS_TYPE_MIXED) {
		/*
		* SPEC (2.1.4) "If the `effective mixed` is true, then
		* a particle whose properties are as follows:..."
		*
		* Empty sequence model group with
		* minOccurs/maxOccurs = 1 (i.e. a "particle emptiable").
		* NOTE that we sill assign it the <complexType> node to
		* somehow anchor it in the doc.
		*/
		if ((particle == NULL) ||
		    (particle->children->type != XML_XSD10_SCHEMA_TYPE_SEQUENCE)) {
		    /*
		    * Create the particle.
		    */
		    particle = xml_xsd10_add_particle(pctxt,
			type->node, 1, 1);
		    if (particle == NULL)
			goto exit_failure;
		    /*
		    * Create the model group.
		    */ /* URGENT TODO: avoid adding to pending items. */
		    particle->children = (XmlXsd10TreeItem *)
			xml_xsd10_add_model_group(pctxt, pctxt->schema,
			XML_XSD10_SCHEMA_TYPE_SEQUENCE, type->node);
		    if (particle->children == NULL)
			goto exit_failure;

		    type->subtypes = (XmlXsd10Type *) particle;
		}
		dummySequence = 1;
		type->contentType = XML_XSD10_SCHEMA_CONTENT_ELEMENTS;
	    } else {
		/*
		* SPEC (2.1.5) "otherwise empty"
		*/
		type->contentType = XML_XSD10_SCHEMA_CONTENT_EMPTY;
	    }
	} else {
	    /*
	    * SPEC (2.2) "otherwise the particle corresponding to the
	    * <all>, <choice>, <group> or <sequence> among the
	    * [children]."
	    */
	    type->contentType = XML_XSD10_SCHEMA_CONTENT_ELEMENTS;
	}
	/*
	* Compute the "content type".
	*/
	if (XSD10_WXS_IS_RESTRICTION(type)) {
	    /*
	    * SPEC (3.1) "If <restriction>..."
	    * (3.1.1) + (3.1.2) */
	    if (type->contentType != XML_XSD10_SCHEMA_CONTENT_EMPTY) {
		if (type->flags & XML_XSD10_SCHEMAS_TYPE_MIXED)
		    type->contentType = XML_XSD10_SCHEMA_CONTENT_MIXED;
	    }
	} else {
	    /*
	    * SPEC (3.2) "If <extension>..."
	    */
	    if (type->contentType == XML_XSD10_SCHEMA_CONTENT_EMPTY) {
		/*
		* SPEC (3.2.1)
		* "If the `effective content` is empty, then the
		*  {content type} of the [...] base ..."
		*/
		type->contentType = baseType->contentType;
		type->subtypes = baseType->subtypes;
		/*
		* Fixes bug #347316:
		* This is the case when the base type has a simple
		* type definition as content.
		*/
		type->contentTypeDef = baseType->contentTypeDef;
		/*
		* NOTE that the effective mixed is ignored here.
		*/
	    } else if (baseType->contentType == XML_XSD10_SCHEMA_CONTENT_EMPTY) {
		/*
		* SPEC (3.2.2)
		*/
		if (type->flags & XML_XSD10_SCHEMAS_TYPE_MIXED)
		    type->contentType = XML_XSD10_SCHEMA_CONTENT_MIXED;
	    } else {
		/*
		* SPEC (3.2.3)
		*/
		if (type->flags & XML_XSD10_SCHEMAS_TYPE_MIXED)
		    type->contentType = XML_XSD10_SCHEMA_CONTENT_MIXED;
		    /*
		    * "A model group whose {compositor} is sequence and whose
		    * {particles} are..."
		    */
		if ((XSD10_WXS_TYPE_PARTICLE(type) != NULL) &&
		    (XSD10_WXS_TYPE_PARTICLE_TERM(type) != NULL) &&
		    ((XSD10_WXS_TYPE_PARTICLE_TERM(type))->type ==
			XML_XSD10_SCHEMA_TYPE_ALL))
		{
		    /*
		    * SPEC cos-all-limited (1)
		    */
		    xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
			/* TODO: error code */
			XML_XSD10_SCHEMAP_COS_ALL_LIMITED,
			XSD10_WXS_ITEM_NODE(type), NULL,
			"The type has an 'all' model group in its "
			"{content type} and thus cannot be derived from "
			"a non-empty type, since this would produce a "
			"'sequence' model group containing the 'all' "
			"model group; 'all' model groups are not "
			"allowed to appear inside other model groups",
			NULL, NULL);

		} else if ((XSD10_WXS_TYPE_PARTICLE(baseType) != NULL) &&
		    (XSD10_WXS_TYPE_PARTICLE_TERM(baseType) != NULL) &&
		    ((XSD10_WXS_TYPE_PARTICLE_TERM(baseType))->type ==
			XML_XSD10_SCHEMA_TYPE_ALL))
		{
		    /*
		    * SPEC cos-all-limited (1)
		    */
		    xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
			/* TODO: error code */
			XML_XSD10_SCHEMAP_COS_ALL_LIMITED,
			XSD10_WXS_ITEM_NODE(type), NULL,
			"A type cannot be derived by extension from a type "
			"which has an 'all' model group in its "
			"{content type}, since this would produce a "
			"'sequence' model group containing the 'all' "
			"model group; 'all' model groups are not "
			"allowed to appear inside other model groups",
			NULL, NULL);

		} else if ((!dummySequence) && (baseType->subtypes != NULL)) {
		    XmlXsd10TreeItem *effectiveContent =
			(XmlXsd10TreeItem *) type->subtypes;
		    /*
		    * Create the particle.
		    */
		    particle = xml_xsd10_add_particle(pctxt,
			type->node, 1, 1);
		    if (particle == NULL)
			goto exit_failure;
		    /*
		    * Create the "sequence" model group.
		    */
		    particle->children = (XmlXsd10TreeItem *)
			xml_xsd10_add_model_group(pctxt, pctxt->schema,
			XML_XSD10_SCHEMA_TYPE_SEQUENCE, type->node);
		    if (particle->children == NULL)
			goto exit_failure;
		    XSD10_WXS_TYPE_CONTENTTYPE(type) = (XmlXsd10Type *) particle;
		    /*
		    * SPEC "the particle of the {content type} of
		    * the ... base ..."
		    * Create a duplicate of the base type's particle
		    * and assign its "term" to it.
		    */
		    particle->children->children =
			(XmlXsd10TreeItem *) xml_xsd10_add_particle(pctxt,
			type->node,
			((XmlXsd10Particle *) baseType->subtypes)->minOccurs,
			((XmlXsd10Particle *) baseType->subtypes)->maxOccurs);
		    if (particle->children->children == NULL)
			goto exit_failure;
		    particle = (XmlXsd10Particle *)
			particle->children->children;
		    particle->children =
			((XmlXsd10Particle *) baseType->subtypes)->children;
		    /*
		    * SPEC "followed by the `effective content`."
		    */
		    particle->next = effectiveContent;
		    /*
		    * This all will result in:
		    * new-particle
		    *   --> new-sequence(
		    *         new-particle
		    *           --> base-model,
		    *         this-particle
		    *	        --> this-model
		    *	    )
		    */
		} else {
		    /*
		    * This is the case when there is already an empty
		    * <sequence> with minOccurs==maxOccurs==1.
		    * Just add the base types's content type.
		    * NOTE that, although we miss to add an intermediate
		    * <sequence>, this should produce no difference to
		    * neither the regex compilation of the content model,
		    * nor to the complex type constraints.
		    */
		    particle->children->children =
			(XmlXsd10TreeItem *) baseType->subtypes;
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
    res = xml_xsd10_fixup_type_attribute_uses(pctxt, type);
    XSD10_HFAILURE XSD10_HERROR
    /*
    * Apply the complex type component constraints; this will not
    * check attributes, since this is done in
    * xmlSchemaFixupTypeAttributeUses().
    */
    res = xml_xsd10_check_ct_component(pctxt, type);
    XSD10_HFAILURE XSD10_HERROR

    if (olderrs != pctxt->nberrors)
	return(pctxt->err);
    else
	return(0);

exit_error:
    type->flags |= XML_XSD10_SCHEMAS_TYPE_INTERNAL_INVALID;
    return(pctxt->err);

exit_failure:
    type->flags |= XML_XSD10_SCHEMAS_TYPE_INTERNAL_INVALID;
    return(-1);
}


/**
 * Port of xmlSchemaTypeFixup.
 *
 * Fixes the content model of the type.
 * URGENT TODO: We need an int result!
 *
 * @param type  the schema type definition
 * @param actxt  the schema parser context
 */
int xml_xsd10_type_fixup(XmlXsd10Type *type, XmlXsd10AbstractCtxt *actxt)
{
    if (type == NULL)
        return(0);
    if (actxt->type != XML_XSD10_SCHEMA_CTXT_PARSER) {
	XSD10_AERROR_INT("xmlSchemaTypeFixup",
	    "this function needs a parser context");
	return(-1);
    }
    if (! XSD10_WXS_IS_TYPE_NOT_FIXED(type))
	return(0);
    if (type->type == XML_XSD10_SCHEMA_TYPE_COMPLEX)
	return(xml_xsd10_fixup_complex_type(XSD10_PCTXT_CAST actxt, type));
    else if (type->type == XML_XSD10_SCHEMA_TYPE_SIMPLE)
	return(xml_xsd10_fixup_simple_type_stage_two(XSD10_PCTXT_CAST actxt, type));
    return(0);
}

/**
 * Port of xmlSchemaCheckFacet.
 *
 * Checks and computes the values of facets.
 *
 * @param facet  the facet
 * @param typeDecl  the schema type definition
 * @param pctxt  the schema parser context or NULL
 * @param name  the optional name of the type
 * @returns 0 if valid, a positive error code if not valid and
 *         -1 in case of an internal or API error.
 */
int xml_xsd10_check_facet_ctxt(XmlXsd10Facet *facet, XmlXsd10Type *typeDecl, XmlXsd10ParserCtxt *pctxt, const char *name)
{
    int ret = 0, ctxtGiven;

    (void) name;

    if ((facet == NULL) || (typeDecl == NULL))
        return(-1);
    /*
    * TODO: will the parser context be given if used from
    * the relaxNG module?
    */
    if (pctxt == NULL)
	ctxtGiven = 0;
    else
	ctxtGiven = 1;

    switch (facet->type) {
        case XML_XSD10_SCHEMA_FACET_MININCLUSIVE:
        case XML_XSD10_SCHEMA_FACET_MINEXCLUSIVE:
        case XML_XSD10_SCHEMA_FACET_MAXINCLUSIVE:
        case XML_XSD10_SCHEMA_FACET_MAXEXCLUSIVE:
	case XML_XSD10_SCHEMA_FACET_ENUMERATION: {
                /*
                 * Okay we need to validate the value
                 * at that point.
                 */
		XmlXsd10Type *base;

		/* 4.3.5.5 Constraints on enumeration Schema Components
		* Schema Component Constraint: enumeration valid restriction
		* It is an `error` if any member of {value} is not in the
		* `value space` of {base type definition}.
		*
		* minInclusive, maxInclusive, minExclusive, maxExclusive:
		* The value `must` be in the
		* `value space` of the `base type`.
		*/
		/*
		* This function is intended to deliver a compiled value
		* on the facet. In this implementation of XML Schemata the
		* type holding a facet, won't be a built-in type.
		* Thus to ensure that other API
		* calls (relaxng) do work, if the given type is a built-in
		* type, we will assume that the given built-in type *is
		* already* the base type.
		*/
		if (typeDecl->type != XML_XSD10_SCHEMA_TYPE_BASIC) {
		    base = typeDecl->baseType;
		    if (base == NULL) {
			XSD10_PERROR_INT("xmlSchemaCheckFacet",
			    "a type user derived type has no base type");
			return (-1);
		    }
		} else
		    base = typeDecl;

		if (! ctxtGiven) {
		    /*
		    * A context is needed if called from RelaxNG.
		    */
		    pctxt = xml_xsd10_new_parser_ctxt("*");
		    if (pctxt == NULL)
			return (-1);
		}
		/*
		* NOTE: This call does not check the content nodes,
		* since they are not available:
		* facet->node is just the node holding the facet
		* definition, *not* the attribute holding the *value*
		* of the facet.
		*/
		ret = xml_xsd10_v_check_cvc_simple_type(
		    XSD10_ACTXT_CAST pctxt, facet->node, base,
		    facet->value, &(facet->val), 1, 1, 0);
                if (ret != 0) {
		    if (ret < 0) {
			/* No error message for RelaxNG. */
			if (ctxtGiven) {
			    xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
				XML_XSD10_SCHEMAP_INTERNAL, facet->node, NULL,
				"Internal error: xmlSchemaCheckFacet, "
				"failed to validate the value '%s' of the "
				"facet '%s' against the base type",
				facet->value, xml_xsd10_facet_type_to_string(facet->type));
			}
			goto internal_error;
		    }
		    ret = XML_XSD10_SCHEMAP_INVALID_FACET_VALUE;
		    /* No error message for RelaxNG. */
		    if (ctxtGiven) {
			char *str = NULL;

			xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
			    ret, facet->node, XSD10_WXS_BASIC_CAST facet,
			    "The value '%s' of the facet does not validate "
			    "against the base type '%s'",
			    facet->value,
			    xml_xsd10_format_q_name(&str,
				base->targetNamespace, base->name));
			XSD10_FREE_AND_NULL(str);
		    }
		    goto exit;
                } else if (facet->val == NULL) {
		    if (ctxtGiven) {
			XSD10_PERROR_INT("xmlSchemaCheckFacet",
			    "value was not computed");
		    }
		    /* TODO */
		}
                break;
            }
        case XML_XSD10_SCHEMA_FACET_PATTERN:
            facet->regexp = xml_regexp_compile(facet->value);
            if (facet->regexp == NULL) {
		ret = XML_XSD10_SCHEMAP_REGEXP_INVALID;
		/* No error message for RelaxNG. */
		if (ctxtGiven) {
		    xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
			ret, facet->node, XSD10_WXS_BASIC_CAST typeDecl,
			"The value '%s' of the facet 'pattern' is not a "
			"valid regular expression",
			facet->value, NULL);
		}
            }
            break;
        case XML_XSD10_SCHEMA_FACET_TOTALDIGITS:
        case XML_XSD10_SCHEMA_FACET_FRACTIONDIGITS:
        case XML_XSD10_SCHEMA_FACET_LENGTH:
        case XML_XSD10_SCHEMA_FACET_MAXLENGTH:
        case XML_XSD10_SCHEMA_FACET_MINLENGTH:

	    if (facet->type == XML_XSD10_SCHEMA_FACET_TOTALDIGITS) {
		ret = xml_xsd10_validate_predefined_type(
		    xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_PINTEGER),
		    facet->value, &(facet->val));
	    } else {
		ret = xml_xsd10_validate_predefined_type(
		    xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_NNINTEGER),
		    facet->value, &(facet->val));
	    }
	    if (ret != 0) {
		if (ret < 0) {
		    /* No error message for RelaxNG. */
		    if (ctxtGiven) {
			XSD10_PERROR_INT("xmlSchemaCheckFacet",
			    "validating facet value");
		    }
		    goto internal_error;
		}
		ret = XML_XSD10_SCHEMAP_INVALID_FACET_VALUE;
		/* No error message for RelaxNG. */
		if (ctxtGiven) {
		    /* error code */
		    xml_xsd10_custom_err4(XSD10_ACTXT_CAST pctxt,
			ret, facet->node, XSD10_WXS_BASIC_CAST typeDecl,
			"The value '%s' of the facet '%s' is not a valid '%s'",
			facet->value,
			xml_xsd10_facet_type_to_string(facet->type),
			(facet->type != XML_XSD10_SCHEMA_FACET_TOTALDIGITS) ?
			    "nonNegativeInteger" :
			    "positiveInteger",
			NULL);
		}
	    }
	    break;

        case XML_XSD10_SCHEMA_FACET_WHITESPACE:{
                if (xml_xsd10_str_equal(facet->value, "preserve")) {
                    facet->whitespace = XML_XSD10_SCHEMAS_FACET_PRESERVE;
                } else if (xml_xsd10_str_equal(facet->value, "replace")) {
                    facet->whitespace = XML_XSD10_SCHEMAS_FACET_REPLACE;
                } else if (xml_xsd10_str_equal(facet->value, "collapse")) {
                    facet->whitespace = XML_XSD10_SCHEMAS_FACET_COLLAPSE;
                } else {
		    ret = XML_XSD10_SCHEMAP_INVALID_FACET_VALUE;
                    /* No error message for RelaxNG. */
		    if (ctxtGiven) {
			/* error was previously: XML_SCHEMAP_INVALID_WHITE_SPACE */
			xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
			    ret, facet->node, XSD10_WXS_BASIC_CAST typeDecl,
			    "The value '%s' of the facet 'whitespace' is not "
			    "valid", facet->value, NULL);
                    }
                }
            }
        default:
            break;
    }
exit:
    if ((! ctxtGiven) && (pctxt != NULL))
	xml_xsd10_free_parser_ctxt(pctxt);
    return (ret);
internal_error:
    if ((! ctxtGiven) && (pctxt != NULL))
	xml_xsd10_free_parser_ctxt(pctxt);
    return (-1);
}

/**
 * Port of xmlSchemaCheckFacetValues.
 *
 * Checks the default values types, especially for facets
 *
 * @param typeDecl  the schema type definition
 * @param pctxt  the schema parser context
 */
int xml_xsd10_check_facet_values(XmlXsd10Type *typeDecl, XmlXsd10ParserCtxt *pctxt)
{
    int res, olderrs = pctxt->nberrors;
    const char *name = typeDecl->name;
    /*
    * NOTE: It is intended to use the facets list, instead
    * of facetSet.
    */
    if (typeDecl->facets != NULL) {
	XmlXsd10Facet *facet = typeDecl->facets;

	/*
	* Temporarily assign the "schema" to the validation context
	* of the parser context. This is needed for NOTATION validation.
	*/
	if (pctxt->vctxt == NULL) {
	    if (xml_xsd10_create_v_ctxt_on_p_ctxt(pctxt) == -1)
		return(-1);
	}
	pctxt->vctxt->schema = pctxt->schema;
	while (facet != NULL) {
	    res = xml_xsd10_check_facet_ctxt(facet, typeDecl, pctxt, name);
	    XSD10_HFAILURE
	    facet = facet->next;
	}
	pctxt->vctxt->schema = NULL;
    }
    if (olderrs != pctxt->nberrors)
	return(pctxt->err);
    return(0);
exit_failure:
    return(-1);
}

/**
 * Port of xmlSchemaGetCircModelGrDefRef.
 *
 * This one is intended to be used by
 * xmlSchemaCheckGroupDefCircular only.
 *
 * @param groupDef  the searched model group
 * @param particle  the first particle
 * @returns the particle with the circular model group definition reference,
 * otherwise NULL.
 */
XmlXsd10TreeItem *xml_xsd10_get_circ_model_gr_def_ref(XmlXsd10ModelGroupDef *groupDef, XmlXsd10TreeItem *particle)
{
    XmlXsd10TreeItem *circ = NULL;
    XmlXsd10TreeItem *term;
    XmlXsd10ModelGroupDef *gdef;

    for (; particle != NULL; particle = particle->next) {
	term = particle->children;
	if (term == NULL)
	    continue;
	switch (term->type) {
	    case XML_XSD10_SCHEMA_TYPE_GROUP:
		gdef = (XmlXsd10ModelGroupDef *) term;
		if (gdef == groupDef)
		    return (particle);
		/*
		* Mark this model group definition to avoid infinite
		* recursion on circular references not yet examined.
		*/
		if (gdef->flags & XML_XSD10_SCHEMA_MODEL_GROUP_DEF_MARKED)
		    continue;
		if (gdef->children != NULL) {
		    gdef->flags |= XML_XSD10_SCHEMA_MODEL_GROUP_DEF_MARKED;
		    circ = xml_xsd10_get_circ_model_gr_def_ref(groupDef,
			gdef->children->children);
		    gdef->flags ^= XML_XSD10_SCHEMA_MODEL_GROUP_DEF_MARKED;
		    if (circ != NULL)
			return (circ);
		}
		break;
	    case XML_XSD10_SCHEMA_TYPE_SEQUENCE:
	    case XML_XSD10_SCHEMA_TYPE_CHOICE:
	    case XML_XSD10_SCHEMA_TYPE_ALL:
		circ = xml_xsd10_get_circ_model_gr_def_ref(groupDef, term->children);
		if (circ != NULL)
		    return (circ);
		break;
	    default:
		break;
	}
    }
    return (NULL);
}

/**
 * Port of xmlSchemaCheckGroupDefCircular.
 *
 * Checks for circular references to model group definitions.
 *
 * @param item  the model group definition
 * @param ctxt  the parser context
 */
void xml_xsd10_check_group_def_circular(XmlXsd10ModelGroupDef *item, XmlXsd10ParserCtxt *ctxt)
{
    /*
    * Schema Component Constraint: Model Group Correct
    * 2 Circular groups are disallowed. That is, within the {particles}
    * of a group there must not be at any depth a particle whose {term}
    * is the group itself.
    */
    if ((item == NULL) ||
	(item->type != XML_XSD10_SCHEMA_TYPE_GROUP) ||
	(item->children == NULL))
	return;
    {
	XmlXsd10TreeItem *circ;

	circ = xml_xsd10_get_circ_model_gr_def_ref(item, item->children->children);
	if (circ != NULL) {
	    char *str = NULL;
	    /*
	    * TODO: The error report is not adequate: this constraint
	    * is defined for model groups but not definitions, but since
	    * there cannot be any circular model groups without a model group
	    * definition (if not using a construction API), we check those
	    * definitions only.
	    */
	    xml_xsd10_p_custom_err(ctxt,
		XML_XSD10_SCHEMAP_MG_PROPS_CORRECT_2,
		NULL, XSD10_WXS_ITEM_NODE(circ),
		"Circular reference to the model group definition '%s' "
		"defined", xml_xsd10_format_q_name(&str,
		    item->targetNamespace, item->name));
	    XSD10_FREE_AND_NULL(str)
	    /*
	    * NOTE: We will cut the reference to avoid further
	    * confusion of the processor. This is a fatal error.
	    */
	    circ->children = NULL;
	}
    }
}

/**
 * Port of xmlSchemaModelGroupToModelGroupDefFixup.
 *
 * Assigns the model group of model group definitions to the "term"
 * of the referencing particle.
 * In xmlSchemaResolveModelGroupParticleReferences the model group
 * definitions were assigned to the "term", since needed for the
 * circularity check.
 *
 * Schema Component Constraint:
 *     All Group Limited (cos-all-limited) (1.2)
 * @param ctxt  the parser context
 * @param mg  the model group
 */
void xml_xsd10_model_group_to_model_group_def_fixup(XmlXsd10ParserCtxt *ctxt, XmlXsd10ModelGroup *mg)
{
    XmlXsd10Particle *particle = XSD10_WXS_MODELGROUP_PARTICLE(mg);

    (void) ctxt;

    while (particle != NULL) {
	if ((XSD10_WXS_PARTICLE_TERM(particle) == NULL) ||
	    ((XSD10_WXS_PARTICLE_TERM(particle))->type !=
		XML_XSD10_SCHEMA_TYPE_GROUP))
	{
	    particle = XSD10_WXS_PTC_CAST particle->next;
	    continue;
	}
	if (XSD10_WXS_MODELGROUPDEF_MODEL(XSD10_WXS_PARTICLE_TERM(particle)) == NULL) {
	    /*
	    * TODO: Remove the particle.
	    */
	    XSD10_WXS_PARTICLE_TERM(particle) = NULL;
	    particle = XSD10_WXS_PTC_CAST particle->next;
	    continue;
	}
	/*
	* Assign the model group to the {term} of the particle.
	*/
	XSD10_WXS_PARTICLE_TERM(particle) =
	    XSD10_WXS_TREE_CAST XSD10_WXS_MODELGROUPDEF_MODEL(XSD10_WXS_PARTICLE_TERM(particle));

	particle = XSD10_WXS_PTC_CAST particle->next;
    }
}

/**
 * Port of xmlSchemaCheckAttrGroupCircularRecur.
 *
 * This one is intended to be used by
 * xmlSchemaCheckAttrGroupCircular only.
 *
 * @param ctxtGr  the searched attribute group
 * @param list  the current attribute list to be processed
 * @returns the circular attribute group reference, otherwise NULL.
 */
XmlXsd10QNameRef *xml_xsd10_check_attr_group_circular_recur(XmlXsd10AttributeGroup *ctxtGr, XmlXsd10ItemList *list)
{
    XmlXsd10AttributeGroup *gr;
    XmlXsd10QNameRef *ref, *circ;
    int i;
    /*
    * We will search for an attribute group reference which
    * references the context attribute group.
    */
    for (i = 0; i < list->nbItems; i++) {
	ref = (XmlXsd10QNameRef *) list->items[i];
	if ((ref->type == XML_XSD10_SCHEMA_EXTRA_QNAMEREF) &&
	    (ref->itemType == XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP) &&
	    (ref->item != NULL))
	{
	    gr = XSD10_WXS_ATTR_GROUP_CAST ref->item;
	    if (gr == ctxtGr)
		return(ref);
	    if (gr->flags & XML_XSD10_SCHEMAS_ATTRGROUP_MARKED)
		continue;
	    /*
	    * Mark as visited to avoid infinite recursion on
	    * circular references not yet examined.
	    */
	    if ((gr->attrUses) &&
		(gr->flags & XML_XSD10_SCHEMAS_ATTRGROUP_HAS_REFS))
	    {
		gr->flags |= XML_XSD10_SCHEMAS_ATTRGROUP_MARKED;
		circ = xml_xsd10_check_attr_group_circular_recur(ctxtGr,
		    (XmlXsd10ItemList *) gr->attrUses);
		gr->flags ^= XML_XSD10_SCHEMAS_ATTRGROUP_MARKED;
		if (circ != NULL)
		    return (circ);
	    }

	}
    }
    return (NULL);
}

/**
 * Port of xmlSchemaCheckAttrGroupCircular.
 *
 * Checks for circular references of attribute groups.
 *
 * @param attrGr  the attribute group definition
 * @param ctxt  the parser context
 */
int xml_xsd10_check_attr_group_circular(XmlXsd10AttributeGroup *attrGr, XmlXsd10ParserCtxt *ctxt)
{
    /*
    * Schema Representation Constraint:
    * Attribute Group Definition Representation OK
    * 3 Circular group reference is disallowed outside <redefine>.
    * That is, unless this element information item's parent is
    * <redefine>, then among the [children], if any, there must
    * not be an <attributeGroup> with ref [attribute] which resolves
    * to the component corresponding to this <attributeGroup>. Indirect
    * circularity is also ruled out. That is, when QName resolution
    * (Schema Document) ($3.15.3) is applied to a `QName` arising from
    * any <attributeGroup>s with a ref [attribute] among the [children],
    * it must not be the case that a `QName` is encountered at any depth
    * which resolves to the component corresponding to this <attributeGroup>.
    */
    if (attrGr->attrUses == NULL)
	return(0);
    else if ((attrGr->flags & XML_XSD10_SCHEMAS_ATTRGROUP_HAS_REFS) == 0)
	return(0);
    else {
	XmlXsd10QNameRef *circ;

	circ = xml_xsd10_check_attr_group_circular_recur(attrGr,
	    (XmlXsd10ItemList *) attrGr->attrUses);
	if (circ != NULL) {
	    char *str = NULL;
	    /*
	    * TODO: Report the referenced attr group as QName.
	    */
	    xml_xsd10_p_custom_err(ctxt,
		XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_GROUP_3,
		NULL, XSD10_WXS_ITEM_NODE(XSD10_WXS_BASIC_CAST circ),
		"Circular reference to the attribute group '%s' "
		"defined", xml_xsd10_get_component_q_name(&str, attrGr));
	    XSD10_FREE_AND_NULL(str);
	    /*
	    * NOTE: We will cut the reference to avoid further
	    * confusion of the processor.
	    * BADSPEC TODO: The spec should define how to process in this case.
	    */
	    circ->item = NULL;
	    return(ctxt->err);
	}
    }
    return(0);
}


/**
 * Port of xmlSchemaExpandAttributeGroupRefs.
 *
 * Substitutes contained attribute group references
 * for their attribute uses. Wildcards are intersected.
 * Attribute use prohibitions are removed from the list
 * and returned via the `prohibs` list.
 * Pointlessness of attr. prohibs, if a matching attr. decl
 * is existent a well, are checked.
 *
 * @param pctxt  the parser context
 * @param item  the schema item
 * @param completeWild  the intersected wildcard to be returned
 * @param list  the attribute uses
 * @param prohibs  the attribute prohibitions
 */
int xml_xsd10_expand_attribute_group_refs(XmlXsd10ParserCtxt *pctxt, XmlXsd10BasicItem *item, XmlXsd10Wildcard **completeWild, XmlXsd10ItemList *list, XmlXsd10ItemList *prohibs)
{
    XmlXsd10AttributeGroup *gr;
    XmlXsd10AttributeUse *use;
    XmlXsd10ItemList *sublist;
    int i, j;
    int created = (*completeWild == NULL) ? 0 : 1;

    if (prohibs)
	prohibs->nbItems = 0;

    for (i = 0; i < list->nbItems; i++) {
	use = (XmlXsd10AttributeUse *) list->items[i];

	if (use->type == XML_XSD10_SCHEMA_EXTRA_ATTR_USE_PROHIB) {
	    if (prohibs == NULL) {
		XSD10_PERROR_INT("xmlSchemaExpandAttributeGroupRefs",
		    "unexpected attr prohibition found");
		return(-1);
	    }
	    /*
	    * Remove from attribute uses.
	    */
	    if (xml_xsd10_item_list_remove(list, i) == -1)
		return(-1);
	    i--;
	    /*
	    * Note that duplicate prohibitions were already
	    * handled at parsing time.
	    */
	    /*
	    * Add to list of prohibitions.
	    */
	    xml_xsd10_item_list_add_size(prohibs, 2, use);
	    continue;
	}
	if ((use->type == XML_XSD10_SCHEMA_EXTRA_QNAMEREF) &&
	    ((XSD10_WXS_QNAME_CAST use)->itemType == XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP))
	{
	    if ((XSD10_WXS_QNAME_CAST use)->item == NULL)
		return(-1);
	    gr = XSD10_WXS_ATTR_GROUP_CAST (XSD10_WXS_QNAME_CAST use)->item;
	    /*
	    * Expand the referenced attr. group.
	    * TODO: remove this, this is done in a previous step, so
	    * already done here.
	    */
	    if ((gr->flags & XML_XSD10_SCHEMAS_ATTRGROUP_WILDCARD_BUILDED) == 0) {
		if (xml_xsd10_attribute_group_expand_refs(pctxt, gr) == -1)
		    return(-1);
	    }
	    /*
	    * Build the 'complete' wildcard; i.e. intersect multiple
	    * wildcards.
	    */
	    if (gr->attributeWildcard != NULL) {
		if (*completeWild == NULL) {
		    *completeWild = gr->attributeWildcard;
		} else {
		    if (! created) {
			XmlXsd10Wildcard *tmpWild;

			 /*
			* Copy the first encountered wildcard as context,
			* except for the annotation.
			*
			* Although the complete wildcard might not correspond
			* to any node in the schema, we will anchor it on
			* the node of the owner component.
			*/
			tmpWild =  xml_xsd10_add_wildcard(pctxt, pctxt->schema,
			    XML_XSD10_SCHEMA_TYPE_ANY_ATTRIBUTE,
			    XSD10_WXS_ITEM_NODE(item));
			if (tmpWild == NULL)
			    return(-1);
			if (xml_xsd10_clone_wildcard_ns_constraints(pctxt,
			    tmpWild, *completeWild) == -1)
			    return (-1);
			tmpWild->processContents = (*completeWild)->processContents;
			*completeWild = tmpWild;
			created = 1;
		    }

		    if (xml_xsd10_intersect_wildcards(pctxt, *completeWild,
			gr->attributeWildcard) == -1)
			return(-1);
		}
	    }
	    /*
	    * Just remove the reference if the referenced group does not
	    * contain any attribute uses.
	    */
	    sublist = ((XmlXsd10ItemList *) gr->attrUses);
	    if ((sublist == NULL) || sublist->nbItems == 0) {
		if (xml_xsd10_item_list_remove(list, i) == -1)
		    return(-1);
		i--;
		continue;
	    }
	    /*
	    * Add the attribute uses.
	    */
	    list->items[i] = sublist->items[0];
	    if (sublist->nbItems != 1) {
		for (j = 1; j < sublist->nbItems; j++) {
		    i++;
		    if (xml_xsd10_item_list_insert(list,
			    sublist->items[j], i) == -1)
			return(-1);
		}
	    }
	}

    }
    /*
    * Handle pointless prohibitions of declared attributes.
    */
    if (prohibs && (prohibs->nbItems != 0) && (list->nbItems != 0)) {
	XmlXsd10AttributeUseProhib *prohib;

	for (i = prohibs->nbItems -1; i >= 0; i--) {
	    prohib = (XmlXsd10AttributeUseProhib *) prohibs->items[i];
	    for (j = 0; j < list->nbItems; j++) {
		use = (XmlXsd10AttributeUse *) list->items[j];

		if ((prohib->name == XSD10_WXS_ATTRUSE_DECL_NAME(use)) &&
		    (prohib->targetNamespace == XSD10_WXS_ATTRUSE_DECL_TNS(use)))
		{
		    char *str = NULL;

		    xml_xsd10_custom_warning(XSD10_ACTXT_CAST pctxt,
			XML_XSD10_SCHEMAP_WARN_ATTR_POINTLESS_PROH,
			prohib->node, NULL,
			"Skipping pointless attribute use prohibition "
			"'%s', since a corresponding attribute use "
			"exists already in the type definition",
			xml_xsd10_format_q_name(&str,
			    prohib->targetNamespace, prohib->name),
			NULL, NULL);
		    XSD10_FREE_AND_NULL(str);
		    /*
		    * Remove the prohibition.
		    */
		    if (xml_xsd10_item_list_remove(prohibs, i) == -1)
			return(-1);
		    break;
		}
	    }
	}
    }
    return(0);
}

/**
 * Port of xmlSchemaAttributeGroupExpandRefs.
 *
 * Computation of:
 * {attribute uses} property
 * {attribute wildcard} property
 *
 * Substitutes contained attribute group references
 * for their attribute uses. Wildcards are intersected.
 *
 * @param pctxt  the parser context
 * @param attrGr  the attribute group definition
 */
int xml_xsd10_attribute_group_expand_refs(XmlXsd10ParserCtxt *pctxt, XmlXsd10AttributeGroup *attrGr)
{
    if ((attrGr->attrUses == NULL) ||
	(attrGr->flags & XML_XSD10_SCHEMAS_ATTRGROUP_WILDCARD_BUILDED))
	return(0);

    attrGr->flags |= XML_XSD10_SCHEMAS_ATTRGROUP_WILDCARD_BUILDED;
    if (xml_xsd10_expand_attribute_group_refs(pctxt, XSD10_WXS_BASIC_CAST attrGr,
	&(attrGr->attributeWildcard), (XmlXsd10ItemList *) attrGr->attrUses, NULL) == -1)
	return(-1);
    return(0);
}

/**
 * Port of xmlSchemaCheckAGPropsCorrect.
 *
 * Substitutes contained attribute group references
 * for their attribute uses. Wildcards are intersected.
 *
 * Schema Component Constraint:
 *    Attribute Group Definition Properties Correct (ag-props-correct)
 * @param pctxt  the parser context
 * @param attrGr  the attribute group definition
 */
int xml_xsd10_check_ag_props_correct(XmlXsd10ParserCtxt *pctxt, XmlXsd10AttributeGroup *attrGr)
{
    /*
    * SPEC ag-props-correct
    * (1) "The values of the properties of an attribute group definition
    * must be as described in the property tableau in The Attribute
    * Group Definition Schema Component ($3.6.1), modulo the impact of
    * Missing Sub-components ($5.3);"
    */

    if ((attrGr->attrUses != NULL) &&
	(XSD10_WXS_LIST_CAST attrGr->attrUses)->nbItems > 1)
    {
	XmlXsd10ItemList *uses = XSD10_WXS_LIST_CAST attrGr->attrUses;
	XmlXsd10AttributeUse *use, *tmp;
	int i, j, hasId = 0;

	for (i = uses->nbItems -1; i >= 0; i--) {
	    use = (XmlXsd10AttributeUse *) uses->items[i];
	    /*
	    * SPEC ag-props-correct
	    * (2) "Two distinct members of the {attribute uses} must not have
	    * {attribute declaration}s both of whose {name}s match and whose
	    * {target namespace}s are identical."
	    */
	    if (i > 0) {
		for (j = i -1; j >= 0; j--) {
		    tmp = (XmlXsd10AttributeUse *) uses->items[j];
		    if ((XSD10_WXS_ATTRUSE_DECL_NAME(use) ==
			XSD10_WXS_ATTRUSE_DECL_NAME(tmp)) &&
			(XSD10_WXS_ATTRUSE_DECL_TNS(use) ==
			XSD10_WXS_ATTRUSE_DECL_TNS(tmp)))
		    {
			char *str = NULL;

			xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
			    XML_XSD10_SCHEMAP_AG_PROPS_CORRECT,
			    attrGr->node, XSD10_WXS_BASIC_CAST attrGr,
			    "Duplicate %s",
			    xml_xsd10_get_component_designation(&str, use),
			    NULL);
			XSD10_FREE_AND_NULL(str);
			/*
			* Remove the duplicate.
			*/
			if (xml_xsd10_item_list_remove(uses, i) == -1)
			    return(-1);
			goto next_use;
		    }
		}
	    }
	    /*
	    * SPEC ag-props-correct
	    * (3) "Two distinct members of the {attribute uses} must not have
	    * {attribute declaration}s both of whose {type definition}s are or
	    * are derived from ID."
	    * TODO: Does 'derived' include member-types of unions?
	    */
	    if (XSD10_WXS_ATTRUSE_TYPEDEF(use) != NULL) {
		if (xml_xsd10_is_derived_from_built_in_type(
		    XSD10_WXS_ATTRUSE_TYPEDEF(use), XML_XSD10_SCHEMAS_ID))
		{
		    if (hasId) {
			char *str = NULL;

			xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
			    XML_XSD10_SCHEMAP_AG_PROPS_CORRECT,
			    attrGr->node, XSD10_WXS_BASIC_CAST attrGr,
			    "There must not exist more than one attribute "
			    "declaration of type 'xs:ID' "
			    "(or derived from 'xs:ID'). The %s violates this "
			    "constraint",
			    xml_xsd10_get_component_designation(&str, use),
			    NULL);
			XSD10_FREE_AND_NULL(str);
			if (xml_xsd10_item_list_remove(uses, i) == -1)
			    return(-1);
		    }
		    hasId = 1;
		}
	    }
next_use: {}
	}
    }
    return(0);
}

/**
 * Port of xmlSchemaResolveAttrGroupReferences.
 *
 * Resolves references to attribute group definitions.
 *
 * @param ref  the schema attribute definition
 * @param ctxt  the schema parser context
 */
int xml_xsd10_resolve_attr_group_references(XmlXsd10QNameRef *ref, XmlXsd10ParserCtxt *ctxt)
{
    XmlXsd10AttributeGroup *group;

    if (ref->item != NULL)
        return(0);
    group = xml_xsd10_get_attribute_group(ctxt->schema,
	ref->name,
	ref->targetNamespace);
    if (group == NULL) {
	xml_xsd10_p_res_comp_attr_err(ctxt,
	    XML_XSD10_SCHEMAP_SRC_RESOLVE,
	    NULL, ref->node,
	    "ref", ref->name, ref->targetNamespace,
	    ref->itemType, NULL);
	return(ctxt->err);
    }
    ref->item = XSD10_WXS_BASIC_CAST group;
    return(0);
}

/**
 * Port of xmlSchemaCheckAttrPropsCorrect.
 *
 * Schema Component Constraint:
 *    Attribute Declaration Properties Correct (a-props-correct)
 *
 * Validates the value constraints of an attribute declaration/use.
 * NOTE that this needs the simple type definitions to be already
 *   built and checked.
 * @param pctxt  a schema parser context
 * @param attr  an schema attribute declaration/use
 */
int xml_xsd10_check_attr_props_correct(XmlXsd10ParserCtxt *pctxt, XmlXsd10Attribute *attr)
{

    /*
    * SPEC a-props-correct (1)
    * "The values of the properties of an attribute declaration must
    * be as described in the property tableau in The Attribute
    * Declaration Schema Component ($3.2.1), modulo the impact of
    * Missing Sub-components ($5.3)."
    */

    if (XSD10_WXS_ATTR_TYPEDEF(attr) == NULL)
	return(0);

    if (attr->defValue != NULL) {
	int ret;

	/*
	* SPEC a-props-correct (3)
	* "If the {type definition} is or is derived from ID then there
	* must not be a {value constraint}."
	*/
	if (xml_xsd10_is_derived_from_built_in_type(
	    XSD10_WXS_ATTR_TYPEDEF(attr), XML_XSD10_SCHEMAS_ID))
	{
	    xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
		XML_XSD10_SCHEMAP_A_PROPS_CORRECT_3,
		NULL, XSD10_WXS_BASIC_CAST attr,
		"Value constraints are not allowed if the type definition "
		"is or is derived from xs:ID",
		NULL, NULL);
	    return(pctxt->err);
	}
	/*
	* SPEC a-props-correct (2)
	* "if there is a {value constraint}, the canonical lexical
	* representation of its value must be `valid` with respect
	* to the {type definition} as defined in String Valid ($3.14.4)."
	* TODO: Don't care about the *canonical* stuff here, this requirement
	* will be removed in WXS 1.1 anyway.
	*/
	ret = xml_xsd10_v_check_cvc_simple_type(XSD10_ACTXT_CAST pctxt,
	    attr->node, XSD10_WXS_ATTR_TYPEDEF(attr),
	    attr->defValue, &(attr->defVal),
	    1, 1, 0);
	if (ret != 0) {
	    if (ret < 0) {
		XSD10_PERROR_INT("xmlSchemaCheckAttrPropsCorrect",
		    "calling xmlSchemaVCheckCVCSimpleType()");
		return(-1);
	    }
	    xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
		XML_XSD10_SCHEMAP_A_PROPS_CORRECT_2,
		NULL, XSD10_WXS_BASIC_CAST attr,
		"The value of the value constraint is not valid",
		NULL, NULL);
	    return(pctxt->err);
	}
    }

    return(0);
}

/** Port of xmlSchemaCheckSubstGroupCircular. */
XmlXsd10Element *xml_xsd10_check_subst_group_circular(XmlXsd10Element *elemDecl, XmlXsd10Element *ancestor)
{
    XmlXsd10Element *ret;

    if (XSD10_WXS_SUBST_HEAD(ancestor) == NULL)
	return (NULL);
    if (XSD10_WXS_SUBST_HEAD(ancestor) == elemDecl)
	return (ancestor);

    if (XSD10_WXS_SUBST_HEAD(ancestor)->flags & XML_XSD10_SCHEMAS_ELEM_CIRCULAR)
	return (NULL);
    XSD10_WXS_SUBST_HEAD(ancestor)->flags |= XML_XSD10_SCHEMAS_ELEM_CIRCULAR;
    ret = xml_xsd10_check_subst_group_circular(elemDecl,
	XSD10_WXS_SUBST_HEAD(ancestor));
    XSD10_WXS_SUBST_HEAD(ancestor)->flags ^= XML_XSD10_SCHEMAS_ELEM_CIRCULAR;

    return (ret);
}

/**
 * Port of xmlSchemaCheckElemPropsCorrect.
 *
 * Schema Component Constraint:
 * Element Declaration Properties Correct (e-props-correct)
 *
 * STATUS:
 *   missing: (6)
 * @param pctxt  a schema parser context
 * @param elemDecl  the element declaration
 */
int xml_xsd10_check_elem_props_correct(XmlXsd10ParserCtxt *pctxt, XmlXsd10Element *elemDecl)
{
    int ret = 0;
    XmlXsd10Type *typeDef = XSD10_WXS_ELEM_TYPEDEF(elemDecl);
    /*
    * SPEC (1) "The values of the properties of an element declaration
    * must be as described in the property tableau in The Element
    * Declaration Schema Component ($3.3.1), modulo the impact of Missing
    * Sub-components ($5.3)."
    */
    if (XSD10_WXS_SUBST_HEAD(elemDecl) != NULL) {
	XmlXsd10Element *head = XSD10_WXS_SUBST_HEAD(elemDecl), *circ;

	xml_xsd10_check_element_decl_component(head, pctxt);
	/*
	* SPEC (3) "If there is a non-`absent` {substitution group
	* affiliation}, then {scope} must be global."
	*/
	if ((elemDecl->flags & XML_XSD10_SCHEMAS_ELEM_GLOBAL) == 0) {
	    xml_xsd10_p_custom_err(pctxt,
		XML_XSD10_SCHEMAP_E_PROPS_CORRECT_3,
		XSD10_WXS_BASIC_CAST elemDecl, NULL,
		"Only global element declarations can have a "
		"substitution group affiliation", NULL);
	    ret = XML_XSD10_SCHEMAP_E_PROPS_CORRECT_3;
	}
	/*
	* TODO: SPEC (6) "Circular substitution groups are disallowed.
	* That is, it must not be possible to return to an element declaration
	* by repeatedly following the {substitution group affiliation}
	* property."
	*/
	if (head == elemDecl)
	    circ = head;
	else if (XSD10_WXS_SUBST_HEAD(head) != NULL)
	    circ = xml_xsd10_check_subst_group_circular(head, head);
	else
	    circ = NULL;
	if (circ != NULL) {
	    char *strA = NULL, *strB = NULL;

	    xml_xsd10_p_custom_err_ext(pctxt,
		XML_XSD10_SCHEMAP_E_PROPS_CORRECT_6,
		XSD10_WXS_BASIC_CAST circ, NULL,
		"The element declaration '%s' defines a circular "
		"substitution group to element declaration '%s'",
		xml_xsd10_get_component_q_name(&strA, circ),
		xml_xsd10_get_component_q_name(&strB, head),
		NULL);
	    XSD10_FREE_AND_NULL(strA)
	    XSD10_FREE_AND_NULL(strB)
	    ret = XML_XSD10_SCHEMAP_E_PROPS_CORRECT_6;
	}
	/*
	* SPEC (4) "If there is a {substitution group affiliation},
	* the {type definition}
	* of the element declaration must be validly derived from the {type
	* definition} of the {substitution group affiliation}, given the value
	* of the {substitution group exclusions} of the {substitution group
	* affiliation}, as defined in Type Derivation OK (Complex) ($3.4.6)
	* (if the {type definition} is complex) or as defined in
	* Type Derivation OK (Simple) ($3.14.6) (if the {type definition} is
	* simple)."
	*
	* NOTE: {substitution group exclusions} means the values of the
	* attribute "final".
	*/

	if (typeDef != XSD10_WXS_ELEM_TYPEDEF(XSD10_WXS_SUBST_HEAD(elemDecl))) {
	    int set = 0;

	    if (head->flags & XML_XSD10_SCHEMAS_ELEM_FINAL_EXTENSION)
		set |= XSD10_SUBSET_EXTENSION;
	    if (head->flags & XML_XSD10_SCHEMAS_ELEM_FINAL_RESTRICTION)
		set |= XSD10_SUBSET_RESTRICTION;

	    if (xml_xsd10_check_cos_derived_ok(XSD10_ACTXT_CAST pctxt, typeDef,
		XSD10_WXS_ELEM_TYPEDEF(head), set) != 0) {
		char *strA = NULL, *strB = NULL, *strC = NULL;

		ret = XML_XSD10_SCHEMAP_E_PROPS_CORRECT_4;
		xml_xsd10_p_custom_err_ext(pctxt,
		    XML_XSD10_SCHEMAP_E_PROPS_CORRECT_4,
		    XSD10_WXS_BASIC_CAST elemDecl, NULL,
		    "The type definition '%s' was "
		    "either rejected by the substitution group "
		    "affiliation '%s', or not validly derived from its type "
		    "definition '%s'",
		    xml_xsd10_get_component_q_name(&strA, typeDef),
		    xml_xsd10_get_component_q_name(&strB, head),
		    xml_xsd10_get_component_q_name(&strC, XSD10_WXS_ELEM_TYPEDEF(head)));
		XSD10_FREE_AND_NULL(strA)
		XSD10_FREE_AND_NULL(strB)
		XSD10_FREE_AND_NULL(strC)
	    }
	}
    }
    /*
    * SPEC (5) "If the {type definition} or {type definition}'s
    * {content type}
    * is or is derived from ID then there must not be a {value constraint}.
    * Note: The use of ID as a type definition for elements goes beyond
    * XML 1.0, and should be avoided if backwards compatibility is desired"
    */
    if ((elemDecl->value != NULL) &&
	((XSD10_WXS_IS_SIMPLE(typeDef) &&
	  xml_xsd10_is_derived_from_built_in_type(typeDef, XML_XSD10_SCHEMAS_ID)) ||
	 (XSD10_WXS_IS_COMPLEX(typeDef) &&
	  XSD10_WXS_HAS_SIMPLE_CONTENT(typeDef) &&
	  xml_xsd10_is_derived_from_built_in_type(typeDef->contentTypeDef,
	    XML_XSD10_SCHEMAS_ID)))) {

	ret = XML_XSD10_SCHEMAP_E_PROPS_CORRECT_5;
	xml_xsd10_p_custom_err(pctxt,
	    XML_XSD10_SCHEMAP_E_PROPS_CORRECT_5,
	    XSD10_WXS_BASIC_CAST elemDecl, NULL,
	    "The type definition (or type definition's content type) is or "
	    "is derived from ID; value constraints are not allowed in "
	    "conjunction with such a type definition", NULL);
    } else if (elemDecl->value != NULL) {
	int vcret;
	XMQNode *node = NULL;

	/*
	* SPEC (2) "If there is a {value constraint}, the canonical lexical
	* representation of its value must be `valid` with respect to the
	* {type definition} as defined in Element Default Valid (Immediate)
	* ($3.3.6)."
	*/
	if (typeDef == NULL) {
	    xml_xsd10_p_err(pctxt, elemDecl->node,
		XML_XSD10_SCHEMAP_INTERNAL,
		"Internal error: xmlSchemaCheckElemPropsCorrect, "
		"type is missing... skipping validation of "
		"the value constraint", NULL, NULL);
	    return (-1);
	}
	if (elemDecl->node != NULL) {
	    if (elemDecl->flags & XML_XSD10_SCHEMAS_ELEM_FIXED)
		node = (XMQNode *) xml_xsd10_tree_has_prop(elemDecl->node,
		    "fixed");
	    else
		node = (XMQNode *) xml_xsd10_tree_has_prop(elemDecl->node,
		    "default");
	}
	vcret = xml_xsd10_parse_check_cos_valid_default(pctxt, node,
	    typeDef, elemDecl->value, &(elemDecl->defVal));
	if (vcret != 0) {
	    if (vcret < 0) {
		XSD10_PERROR_INT("xmlSchemaElemCheckValConstr",
		    "failed to validate the value constraint of an "
		    "element declaration");
		return (-1);
	    }
	    return (vcret);
	}
    }

    return (ret);
}

/**
 * Port of xmlSchemaCheckElemSubstGroup.
 *
 * Schema Component Constraint:
 * Substitution Group (cos-equiv-class)
 *
 * In Libxml2 the subst. groups will be precomputed, in terms of that
 * a list will be built for each subst. group head, holding all direct
 * referents to this head.
 * NOTE that this function needs:
 *   1. circular subst. groups to be checked beforehand
 *   2. the declaration's type to be derived from the head's type
 *
 * STATUS:
 *
 * @param ctxt  a schema parser context
 * @param elemDecl  the element declaration
 */
void xml_xsd10_check_elem_subst_group(XmlXsd10ParserCtxt *ctxt, XmlXsd10Element *elemDecl)
{
    if ((XSD10_WXS_SUBST_HEAD(elemDecl) == NULL) ||
	/* SPEC (1) "Its {abstract} is false." */
	(elemDecl->flags & XML_XSD10_SCHEMAS_ELEM_ABSTRACT))
	return;
    {
	XmlXsd10Element *head;
	XmlXsd10Type *headType, *type;
	int set, methSet;
	/*
	* SPEC (2) "It is validly substitutable for HEAD subject to HEAD's
	* {disallowed substitutions} as the blocking constraint, as defined in
	* Substitution Group OK (Transitive) ($3.3.6)."
	*/
	for (head = XSD10_WXS_SUBST_HEAD(elemDecl); head != NULL;
	    head = XSD10_WXS_SUBST_HEAD(head)) {
	    set = 0;
	    methSet = 0;
	    /*
	    * The blocking constraints.
	    */
	    if (head->flags & XML_XSD10_SCHEMAS_ELEM_BLOCK_SUBSTITUTION)
		continue;
	    headType = head->subtypes;
	    type = elemDecl->subtypes;
	    if (headType == type)
		goto add_member;
	    if (head->flags & XML_XSD10_SCHEMAS_ELEM_BLOCK_RESTRICTION)
		set |= XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION;
	    if (head->flags & XML_XSD10_SCHEMAS_ELEM_BLOCK_EXTENSION)
		set |= XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION;
	    /*
	    * SPEC: Substitution Group OK (Transitive) (2.3)
	    * "The set of all {derivation method}s involved in the
	    * derivation of D's {type definition} from C's {type definition}
	    * does not intersect with the union of the blocking constraint,
	    * C's {prohibited substitutions} (if C is complex, otherwise the
	    * empty set) and the {prohibited substitutions} (respectively the
	    * empty set) of any intermediate {type definition}s in the
	    * derivation of D's {type definition} from C's {type definition}."
	    */
	    /*
	    * OPTIMIZE TODO: Optimize this a bit, since, if traversing the
	    * subst.head axis, the methSet does not need to be computed for
	    * the full depth over and over.
	    */
	    /*
	    * The set of all {derivation method}s involved in the derivation
	    */
	    while ((type != NULL) && (type != headType) &&
                   (type != type->baseType)) {
		if ((XSD10_WXS_IS_EXTENSION(type)) &&
		    ((methSet & XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION) == 0))
		    methSet |= XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION;

		if (XSD10_WXS_IS_RESTRICTION(type) &&
		    ((methSet & XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION) == 0))
		    methSet |= XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION;

		type = type->baseType;
	    }
	    /*
	    * The {prohibited substitutions} of all intermediate types +
	    * the head's type.
	    */
	    type = elemDecl->subtypes->baseType;
	    while (type != NULL) {
		if (XSD10_WXS_IS_COMPLEX(type)) {
		    if ((type->flags &
			    XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION) &&
			((set & XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION) == 0))
		    set |= XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION;
		    if ((type->flags &
			    XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION) &&
			((set & XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION) == 0))
		    set |= XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION;
		} else
		    break;
		if (type == headType)
		    break;
		type = type->baseType;
	    }
	    if ((set != 0) &&
		(((set & XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION) &&
		(methSet & XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION)) ||
		((set & XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION) &&
		(methSet & XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION)))) {
		continue;
	    }
add_member:
	    xml_xsd10_add_element_substitution_member(ctxt, head, elemDecl);
	    if ((head->flags & XML_XSD10_SCHEMAS_ELEM_SUBST_GROUP_HEAD) == 0)
		head->flags |= XML_XSD10_SCHEMAS_ELEM_SUBST_GROUP_HEAD;
	}
    }
}


/**
 * Port of xmlSchemaCheckElementDeclComponent.
 *
 * xmlSchemaCheckElementDeclComponent
 *
 * Validates the value constraints of an element declaration.
 * Adds substitution group members.
 *
 * @param elemDecl  an schema element declaration/particle
 * @param ctxt  a schema parser context
 */
void xml_xsd10_check_element_decl_component(XmlXsd10Element *elemDecl, XmlXsd10ParserCtxt *ctxt)
{
    if (elemDecl == NULL)
	return;
    if (elemDecl->flags & XML_XSD10_SCHEMAS_ELEM_INTERNAL_CHECKED)
	return;
    elemDecl->flags |= XML_XSD10_SCHEMAS_ELEM_INTERNAL_CHECKED;
    if (xml_xsd10_check_elem_props_correct(ctxt, elemDecl) == 0) {
	/*
	* Adds substitution group members.
	*/
	xml_xsd10_check_elem_subst_group(ctxt, elemDecl);
    }
}

/**
 * Port of xmlSchemaResolveModelGroupParticleReferences.
 *
 * Resolves references of a model group's {particles} to
 * model group definitions and to element declarations.
 *
 * @param ctxt  a parser context
 * @param mg  a model group
 */
void xml_xsd10_resolve_model_group_particle_references(XmlXsd10ParserCtxt *ctxt, XmlXsd10ModelGroup *mg)
{
    XmlXsd10Particle *particle = XSD10_WXS_MODELGROUP_PARTICLE(mg);
    XmlXsd10QNameRef *ref;
    XmlXsd10BasicItem *refItem;

    /*
    * URGENT TODO: Test this.
    */
    while (particle != NULL) {
	if ((XSD10_WXS_PARTICLE_TERM(particle) == NULL) ||
	    ((XSD10_WXS_PARTICLE_TERM(particle))->type !=
		XML_XSD10_SCHEMA_EXTRA_QNAMEREF))
	{
	    goto next_particle;
	}
	ref = XSD10_WXS_QNAME_CAST XSD10_WXS_PARTICLE_TERM(particle);
	/*
	* Resolve the reference.
	* NULL the {term} by default.
	*/
	particle->children = NULL;

	refItem = xml_xsd10_get_named_component(ctxt->schema,
	    ref->itemType, ref->name, ref->targetNamespace);
	if (refItem == NULL) {
	    xml_xsd10_p_res_comp_attr_err(ctxt, XML_XSD10_SCHEMAP_SRC_RESOLVE,
		NULL, XSD10_WXS_ITEM_NODE(particle), "ref", ref->name,
		ref->targetNamespace, ref->itemType, NULL);
	    /* TODO: remove the particle. */
	    goto next_particle;
	}
	if (refItem->type == XML_XSD10_SCHEMA_TYPE_GROUP) {
	    if (XSD10_WXS_MODELGROUPDEF_MODEL(refItem) == NULL)
		/* TODO: remove the particle. */
		goto next_particle;
	    /*
	    * NOTE that we will assign the model group definition
	    * itself to the "term" of the particle. This will ease
	    * the check for circular model group definitions. After
	    * that the "term" will be assigned the model group of the
	    * model group definition.
	    */
	    if ((XSD10_WXS_MODELGROUPDEF_MODEL(refItem))->type ==
		    XML_XSD10_SCHEMA_TYPE_ALL) {
		/*
		* SPEC cos-all-limited (1)
		* SPEC cos-all-limited (1.2)
		* "It appears only as the value of one or both of the
		* following properties:"
		* (1.1) "the {model group} property of a model group
		*        definition."
		* (1.2) "the {term} property of a particle [... of] the "
		* {content type} of a complex type definition."
		*/
		xml_xsd10_custom_err(XSD10_ACTXT_CAST ctxt,
		    /* TODO: error code */
		    XML_XSD10_SCHEMAP_COS_ALL_LIMITED,
		    XSD10_WXS_ITEM_NODE(particle), NULL,
		    "A model group definition is referenced, but "
		    "it contains an 'all' model group, which "
		    "cannot be contained by model groups",
		    NULL, NULL);
		/* TODO: remove the particle. */
		goto next_particle;
	    }
	    particle->children = (XmlXsd10TreeItem *) refItem;
	} else {
	    /*
	    * TODO: Are referenced element declarations the only
	    * other components we expect here?
	    */
	    particle->children = (XmlXsd10TreeItem *) refItem;
	}
next_particle:
	particle = XSD10_WXS_PTC_CAST particle->next;
    }
}

/** Port of xmlSchemaAreValuesEqual. */
int xml_xsd10_are_values_equal(XmlXsd10Val *x, XmlXsd10Val *y)
{
    XmlXsd10Type *tx, *ty, *ptx, *pty;
    int ret;

    while (x != NULL) {
	/* Same types. */
	tx = xml_xsd10_get_built_in_type(xml_xsd10_get_val_type(x));
	ty = xml_xsd10_get_built_in_type(xml_xsd10_get_val_type(y));
	ptx = xml_xsd10_get_primitive_type(tx);
	pty = xml_xsd10_get_primitive_type(ty);
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
	    return(0);
	/*
	* We assume computed values to be normalized, so do a fast
	* string comparison for string based types.
	*/
	if ((ptx->builtInType == XML_XSD10_SCHEMAS_STRING) ||
	    XSD10_WXS_IS_ANY_SIMPLE_TYPE(ptx)) {
	    if (! xml_xsd10_str_equal(
		xml_xsd10_value_get_as_string(x),
		xml_xsd10_value_get_as_string(y)))
		return (0);
	} else {
	    ret = xml_xsd10_compare_values_whtsp(
		x, XML_XSD10_SCHEMA_WHITESPACE_PRESERVE,
		y, XML_XSD10_SCHEMA_WHITESPACE_PRESERVE);
	    if (ret == -2)
		return(-1);
	    if (ret != 0)
		return(0);
	}
	/*
	* Lists.
	*/
	x = xml_xsd10_value_get_next(x);
	if (x != NULL) {
	    y = xml_xsd10_value_get_next(y);
	    if (y == NULL)
		return (0);
	} else if (xml_xsd10_value_get_next(y) != NULL)
	    return (0);
	else
	    return (1);
    }
    return (0);
}

/**
 * Port of xmlSchemaResolveAttrUseReferences.
 *
 * Resolves the referenced attribute declaration.
 *
 * @param ause  an attribute use
 * @param ctxt  a parser context
 */
int xml_xsd10_resolve_attr_use_references(XmlXsd10AttributeUse *ause, XmlXsd10ParserCtxt *ctxt)
{
    if ((ctxt == NULL) || (ause == NULL))
	return(-1);
    if ((ause->attrDecl == NULL) ||
	(ause->attrDecl->type != XML_XSD10_SCHEMA_EXTRA_QNAMEREF))
	return(0);

    {
	XmlXsd10QNameRef *ref = XSD10_WXS_QNAME_CAST ause->attrDecl;

	/*
	* TODO: Evaluate, what errors could occur if the declaration is not
	* found.
	*/
	ause->attrDecl = xml_xsd10_get_attribute_decl(ctxt->schema,
	    ref->name, ref->targetNamespace);
        if (ause->attrDecl == NULL) {
	    xml_xsd10_p_res_comp_attr_err(ctxt,
		XML_XSD10_SCHEMAP_SRC_RESOLVE,
		XSD10_WXS_BASIC_CAST ause, ause->node,
		"ref", ref->name, ref->targetNamespace,
		XML_XSD10_SCHEMA_TYPE_ATTRIBUTE, NULL);
            return(ctxt->err);;
        }
    }
    return(0);
}

/**
 * Port of xmlSchemaCheckAttrUsePropsCorrect.
 *
 * Schema Component Constraint:
 * Attribute Use Correct (au-props-correct)
 *
 * @param ctxt  a parser context
 * @param use  an attribute use
 */
int xml_xsd10_check_attr_use_props_correct(XmlXsd10ParserCtxt *ctxt, XmlXsd10AttributeUse *use)
{
    if ((ctxt == NULL) || (use == NULL))
	return(-1);
    if ((use->defValue == NULL) || (XSD10_WXS_ATTRUSE_DECL(use) == NULL) ||
	((XSD10_WXS_ATTRUSE_DECL(use))->type != XML_XSD10_SCHEMA_TYPE_ATTRIBUTE))
	return(0);

    /*
    * SPEC au-props-correct (1)
    * "The values of the properties of an attribute use must be as
    * described in the property tableau in The Attribute Use Schema
    * Component ($3.5.1), modulo the impact of Missing
    * Sub-components ($5.3)."
    */

    if (((XSD10_WXS_ATTRUSE_DECL(use))->defValue != NULL) &&
	((XSD10_WXS_ATTRUSE_DECL(use))->flags & XML_XSD10_SCHEMAS_ATTR_FIXED) &&
        ((use->flags & XML_XSD10_SCHEMA_ATTR_USE_FIXED) == 0))
    {
	xml_xsd10_p_custom_err(ctxt,
	    XML_XSD10_SCHEMAP_AU_PROPS_CORRECT_2,
	    XSD10_WXS_BASIC_CAST use, NULL,
	    "The attribute declaration has a 'fixed' value constraint "
	    ", thus the attribute use must also have a 'fixed' value "
	    "constraint",
	    NULL);
	return(ctxt->err);
    }
    /*
    * Compute and check the value constraint's value.
    */
    if ((use->defVal != NULL) && (XSD10_WXS_ATTRUSE_TYPEDEF(use) != NULL)) {
	int ret;
	/*
	* TODO: The spec seems to be missing a check of the
	* value constraint of the attribute use. We will do it here.
	*/
	/*
	* SPEC a-props-correct (3)
	*/
	if (xml_xsd10_is_derived_from_built_in_type(
	    XSD10_WXS_ATTRUSE_TYPEDEF(use), XML_XSD10_SCHEMAS_ID))
	{
	    xml_xsd10_custom_err(XSD10_ACTXT_CAST ctxt,
		XML_XSD10_SCHEMAP_AU_PROPS_CORRECT,
		NULL, XSD10_WXS_BASIC_CAST use,
		"Value constraints are not allowed if the type definition "
		"is or is derived from xs:ID",
		NULL, NULL);
	    return(ctxt->err);
	}

	ret = xml_xsd10_v_check_cvc_simple_type(XSD10_ACTXT_CAST ctxt,
	    use->node, XSD10_WXS_ATTRUSE_TYPEDEF(use),
	    use->defValue, &(use->defVal),
	    1, 1, 0);
	if (ret != 0) {
	    if (ret < 0) {
		XSD10_PERROR_INT2("xmlSchemaCheckAttrUsePropsCorrect",
		    "calling xmlSchemaVCheckCVCSimpleType()");
		return(-1);
	    }
	    xml_xsd10_custom_err(XSD10_ACTXT_CAST ctxt,
		XML_XSD10_SCHEMAP_AU_PROPS_CORRECT,
		NULL, XSD10_WXS_BASIC_CAST use,
		"The value of the value constraint is not valid",
		NULL, NULL);
	    return(ctxt->err);
	}
    }
    /*
    * SPEC au-props-correct (2)
    * "If the {attribute declaration} has a fixed
    * {value constraint}, then if the attribute use itself has a
    * {value constraint}, it must also be fixed and its value must match
    * that of the {attribute declaration}'s {value constraint}."
    */
    if (((XSD10_WXS_ATTRUSE_DECL(use))->defVal != NULL) &&
	(((XSD10_WXS_ATTRUSE_DECL(use))->flags & XML_XSD10_SCHEMA_ATTR_USE_FIXED) == 0))
    {
	if (! xml_xsd10_are_values_equal(use->defVal,
		(XSD10_WXS_ATTRUSE_DECL(use))->defVal))
	{
	    xml_xsd10_p_custom_err(ctxt,
		XML_XSD10_SCHEMAP_AU_PROPS_CORRECT_2,
		XSD10_WXS_BASIC_CAST use, NULL,
		"The 'fixed' value constraint of the attribute use "
		"must match the attribute declaration's value "
		"constraint '%s'",
		(XSD10_WXS_ATTRUSE_DECL(use))->defValue);
	}
	return(ctxt->err);
    }
    return(0);
}




/**
 * Port of xmlSchemaResolveAttrTypeReferences.
 *
 * Resolves the referenced type definition component.
 *
 * @param item  an attribute declaration
 * @param ctxt  a parser context
 */
int xml_xsd10_resolve_attr_type_references(XmlXsd10Attribute *item, XmlXsd10ParserCtxt *ctxt)
{
    /*
    * The simple type definition corresponding to the <simpleType> element
    * information item in the [children], if present, otherwise the simple
    * type definition `resolved` to by the `actual value` of the type
    * [attribute], if present, otherwise the `simple ur-type definition`.
    */
    if (item->flags & XML_XSD10_SCHEMAS_ATTR_INTERNAL_RESOLVED)
	return(0);
    item->flags |= XML_XSD10_SCHEMAS_ATTR_INTERNAL_RESOLVED;
    if (item->subtypes != NULL)
        return(0);
    if (item->typeName != NULL) {
        XmlXsd10Type *type;

	type = xml_xsd10_get_type(ctxt->schema, item->typeName,
	    item->typeNs);
	if ((type == NULL) || (! XSD10_WXS_IS_SIMPLE(type))) {
	    xml_xsd10_p_res_comp_attr_err(ctxt,
		XML_XSD10_SCHEMAP_SRC_RESOLVE,
		XSD10_WXS_BASIC_CAST item, item->node,
		"type", item->typeName, item->typeNs,
		XML_XSD10_SCHEMA_TYPE_SIMPLE, NULL);
	    return(ctxt->err);
	} else
	    item->subtypes = type;

    } else {
	/*
	* The type defaults to the xs:anySimpleType.
	*/
	item->subtypes = xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_ANYSIMPLETYPE);
    }
    return(0);
}

/**
 * Port of xmlSchemaResolveIDCKeyReferences.
 *
 * Resolve keyRef references to key/unique IDCs.
 * Schema Component Constraint:
 *   Identity-constraint Definition Properties Correct (c-props-correct)
 *
 * @param idc  the identity-constraint definition
 * @param pctxt  the schema parser context
 */
int xml_xsd10_resolve_idc_key_references(XmlXsd10IDC *idc, XmlXsd10ParserCtxt *pctxt)
{
    if (idc->type != XML_XSD10_SCHEMA_TYPE_IDC_KEYREF)
        return(0);
    if (idc->ref->name != NULL) {
	idc->ref->item = (XmlXsd10BasicItem *)
	    xml_xsd10_get_idc(pctxt->schema, idc->ref->name,
		idc->ref->targetNamespace);
        if (idc->ref->item == NULL) {
	    /*
	    * TODO: It is actually not an error to fail to resolve
	    * at this stage. BUT we need to be that strict!
	    */
	    xml_xsd10_p_res_comp_attr_err(pctxt,
		XML_XSD10_SCHEMAP_SRC_RESOLVE,
		XSD10_WXS_BASIC_CAST idc, idc->node,
		"refer", idc->ref->name,
		idc->ref->targetNamespace,
		XML_XSD10_SCHEMA_TYPE_IDC_KEY, NULL);
            return(pctxt->err);
	} else if (idc->ref->item->type == XML_XSD10_SCHEMA_TYPE_IDC_KEYREF) {
	    /*
	    * SPEC c-props-correct (1)
	    */
	    xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
		XML_XSD10_SCHEMAP_C_PROPS_CORRECT,
		NULL, XSD10_WXS_BASIC_CAST idc,
		"The keyref references a keyref",
		NULL, NULL);
	    idc->ref->item = NULL;
	    return(pctxt->err);
	} else {
	    if (idc->nbFields !=
		((XmlXsd10IDC *) idc->ref->item)->nbFields) {
		char *str = NULL;
		XmlXsd10IDC *refer;

		refer = (XmlXsd10IDC *) idc->ref->item;
		/*
		* SPEC c-props-correct(2)
		* "If the {identity-constraint category} is keyref,
		* the cardinality of the {fields} must equal that of
		* the {fields} of the {referenced key}.
		*/
		xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
		    XML_XSD10_SCHEMAP_C_PROPS_CORRECT,
		    NULL, XSD10_WXS_BASIC_CAST idc,
		    "The cardinality of the keyref differs from the "
		    "cardinality of the referenced key/unique '%s'",
		    xml_xsd10_format_q_name(&str, refer->targetNamespace,
			refer->name),
		    NULL);
		XSD10_FREE_AND_NULL(str)
		return(pctxt->err);
	    }
	}
    }
    return(0);
}

/** Port of xmlSchemaResolveAttrUseProhibReferences. */
int xml_xsd10_resolve_attr_use_prohib_references(XmlXsd10AttributeUseProhib *prohib, XmlXsd10ParserCtxt *pctxt)
{
    if (xml_xsd10_get_attribute_decl(pctxt->schema, prohib->name,
	prohib->targetNamespace) == NULL) {

	xml_xsd10_p_res_comp_attr_err(pctxt,
	    XML_XSD10_SCHEMAP_SRC_RESOLVE,
	    NULL, prohib->node,
	    "ref", prohib->name, prohib->targetNamespace,
	    XML_XSD10_SCHEMA_TYPE_ATTRIBUTE, NULL);
	return(XML_XSD10_SCHEMAP_SRC_RESOLVE);
    }
    return(0);
}

#define XSD10_WXS_REDEFINED_TYPE(c) \
(((XmlXsd10Type *) item)->flags & XML_XSD10_SCHEMAS_TYPE_REDEFINED)

#define XSD10_WXS_REDEFINED_MODEL_GROUP_DEF(c) \
(((XmlXsd10ModelGroupDef *) item)->flags & XML_XSD10_SCHEMA_MODEL_GROUP_DEF_REDEFINED)

#define XSD10_WXS_REDEFINED_ATTR_GROUP(c) \
(((XmlXsd10AttributeGroup *) item)->flags & XML_XSD10_SCHEMAS_ATTRGROUP_REDEFINED)

/** Port of xmlSchemaCheckSRCRedefineFirst. */
int xml_xsd10_check_src_redefine_first(XmlXsd10ParserCtxt *pctxt)
{
    int err = 0;
    XmlXsd10Redef *redef = XSD10_WXS_CONSTRUCTOR(pctxt)->redefs;
    XmlXsd10BasicItem *prev, *item;
    int wasRedefined;

    if (redef == NULL)
	return(0);

    do {
	item = redef->item;
	/*
	* First try to locate the redefined component in the
	* schema graph starting with the redefined schema.
	* NOTE: According to this schema bug entry:
	*   http://lists.w3.org/Archives/Public/www-xml-schema-comments/2005OctDec/0019.html
	*   it's not clear if the referenced component needs to originate
	*   from the <redefine>d schema _document_ or the schema; the latter
	*   would include all imported and included sub-schemas of the
	*   <redefine>d schema. Currently the latter approach is used.
	*   SUPPLEMENT: It seems that the WG moves towards the latter
	*   approach, so we are doing it right.
	*
	*/
	prev = xml_xsd10_find_redef_comp_in_graph(
	    redef->targetBucket, item->type,
	    redef->refName, redef->refTargetNs);
	if (prev == NULL) {
	    char *str = NULL;
	    XMQNode *node;

	    /*
	    * SPEC src-redefine:
	    * (6.2.1) "The `actual value` of its own name attribute plus
	    * target namespace must successfully `resolve` to a model
	    * group definition in I."
	    * (7.2.1) "The `actual value` of its own name attribute plus
	    * target namespace must successfully `resolve` to an attribute
	    * group definition in I."

	    *
	    * Note that, if we are redefining with the use of references
	    * to components, the spec assumes the src-resolve to be used;
	    * but this won't assure that we search only *inside* the
	    * redefined schema.
	    */
	    if (redef->reference)
		node = XSD10_WXS_ITEM_NODE(redef->reference);
	    else
		node = XSD10_WXS_ITEM_NODE(item);
	    xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
		/*
		* TODO: error code.
		* Probably XML_SCHEMAP_SRC_RESOLVE, if this is using the
		* reference kind.
		*/
		XML_XSD10_SCHEMAP_SRC_REDEFINE, node, NULL,
		"The %s '%s' to be redefined could not be found in "
		"the redefined schema",
		XSD10_WXS_ITEM_TYPE_NAME(item),
		xml_xsd10_format_q_name(&str, redef->refTargetNs,
		    redef->refName));
	    XSD10_FREE_AND_NULL(str);
	    err = pctxt->err;
	    redef = redef->next;
	    continue;
	}
	/*
	* TODO: Obtaining and setting the redefinition state is really
	* clumsy.
	*/
	wasRedefined = 0;
	switch (item->type) {
	    case XML_XSD10_SCHEMA_TYPE_COMPLEX:
	    case XML_XSD10_SCHEMA_TYPE_SIMPLE:
		if ((XSD10_WXS_TYPE_CAST prev)->flags &
		    XML_XSD10_SCHEMAS_TYPE_REDEFINED)
		{
		    wasRedefined = 1;
		    break;
		}
		/* Mark it as redefined. */
		(XSD10_WXS_TYPE_CAST prev)->flags |= XML_XSD10_SCHEMAS_TYPE_REDEFINED;
		/*
		* Assign the redefined type to the
		* base type of the redefining type.
		* TODO: How
		*/
		((XmlXsd10Type *) item)->baseType =
		    (XmlXsd10Type *) prev;
		break;
	    case XML_XSD10_SCHEMA_TYPE_GROUP:
		if ((XSD10_WXS_MODEL_GROUPDEF_CAST prev)->flags &
		    XML_XSD10_SCHEMA_MODEL_GROUP_DEF_REDEFINED)
		{
		    wasRedefined = 1;
		    break;
		}
		/* Mark it as redefined. */
		(XSD10_WXS_MODEL_GROUPDEF_CAST prev)->flags |=
		    XML_XSD10_SCHEMA_MODEL_GROUP_DEF_REDEFINED;
		if (redef->reference != NULL) {
		    /*
		    * Overwrite the QName-reference with the
		    * referenced model group def.
		    */
		    (XSD10_WXS_PTC_CAST redef->reference)->children =
			XSD10_WXS_TREE_CAST prev;
		}
		redef->target = prev;
		break;
	    case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
		if ((XSD10_WXS_ATTR_GROUP_CAST prev)->flags &
		    XML_XSD10_SCHEMAS_ATTRGROUP_REDEFINED)
		{
		    wasRedefined = 1;
		    break;
		}
		(XSD10_WXS_ATTR_GROUP_CAST prev)->flags |=
		    XML_XSD10_SCHEMAS_ATTRGROUP_REDEFINED;
		if (redef->reference != NULL) {
		    /*
		    * Assign the redefined attribute group to the
		    * QName-reference component.
		    * This is the easy case, since we will just
		    * expand the redefined group.
		    */
		    (XSD10_WXS_QNAME_CAST redef->reference)->item = prev;
		    redef->target = NULL;
		} else {
		    /*
		    * This is the complicated case: we need
		    * to apply src-redefine (7.2.2) at a later
		    * stage, i.e. when attribute group references
		    * have been expanded and simple types have
		    * been fixed.
		    */
		    redef->target = prev;
		}
		break;
	    default:
		XSD10_PERROR_INT("xmlSchemaResolveRedefReferences",
		    "Unexpected redefined component type");
		return(-1);
	}
	if (wasRedefined) {
	    char *str = NULL;
	    XMQNode *node;

	    if (redef->reference)
		node = XSD10_WXS_ITEM_NODE(redef->reference);
	    else
		node = XSD10_WXS_ITEM_NODE(redef->item);

	    xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
		/* TODO: error code. */
		XML_XSD10_SCHEMAP_SRC_REDEFINE,
		node, NULL,
		"The referenced %s was already redefined. Multiple "
		"redefinition of the same component is not supported",
		xml_xsd10_get_component_designation(&str, prev),
		NULL);
	    XSD10_FREE_AND_NULL(str)
	    err = pctxt->err;
	    redef = redef->next;
	    continue;
	}
	redef = redef->next;
    } while (redef != NULL);

    return(err);
}

/** Port of xmlSchemaCheckSRCRedefineSecond. */
int xml_xsd10_check_src_redefine_second(XmlXsd10ParserCtxt *pctxt)
{
    int err = 0;
    XmlXsd10Redef *redef = XSD10_WXS_CONSTRUCTOR(pctxt)->redefs;
    XmlXsd10BasicItem *item;

    if (redef == NULL)
	return(0);

    do {
	if (redef->target == NULL) {
	    redef = redef->next;
	    continue;
	}
	item = redef->item;

	switch (item->type) {
	    case XML_XSD10_SCHEMA_TYPE_SIMPLE:
	    case XML_XSD10_SCHEMA_TYPE_COMPLEX:
		/*
		* Since the spec wants the {name} of the redefined
		* type to be 'absent', we'll NULL it.
		*/
		(XSD10_WXS_TYPE_CAST redef->target)->name = NULL;

		/*
		* TODO: Seems like there's nothing more to do. The normal
		* inheritance mechanism is used. But not 100% sure.
		*/
		break;
	    case XML_XSD10_SCHEMA_TYPE_GROUP:
		/*
		* URGENT TODO:
		* SPEC src-redefine:
		* (6.2.2) "The {model group} of the model group definition
		* which corresponds to it per XML Representation of Model
		* Group Definition Schema Components ($3.7.2) must be a
		* `valid restriction` of the {model group} of that model
		* group definition in I, as defined in Particle Valid
		* (Restriction) ($3.9.6)."
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
		err = xml_xsd10_check_derivation_ok_restriction2to4(pctxt,
		    XML_XSD10_SCHEMA_ACTION_REDEFINE,
		    item, redef->target,
		    (XmlXsd10ItemList *) (XSD10_WXS_ATTR_GROUP_CAST item)->attrUses,
		    (XmlXsd10ItemList *) (XSD10_WXS_ATTR_GROUP_CAST redef->target)->attrUses,
		    (XSD10_WXS_ATTR_GROUP_CAST item)->attributeWildcard,
		    (XSD10_WXS_ATTR_GROUP_CAST redef->target)->attributeWildcard);
		if (err == -1)
		    return(-1);
		break;
	    default:
		break;
	}
	redef = redef->next;
    } while (redef != NULL);
    return(0);
}


/** Port of xmlSchemaAddComponents. */
int xml_xsd10_add_components(XmlXsd10ParserCtxt *pctxt, XmlXsd10Bucket *bucket)
{
    XmlXsd10BasicItem *item;
    int err;
    XmlXsd10Hash **table;
    const char *name;
    int i;

#define XSD10_WXS_GET_GLOBAL_HASH(c, slot) { \
    if (XSD10_WXS_IS_BUCKET_IMPMAIN((c)->type)) \
	table = &(XSD10_WXS_IMPBUCKET((c))->schema->slot); \
    else \
	table = &(XSD10_WXS_INCBUCKET((c))->ownerImport->schema->slot); }

    /*
    * Add global components to the schema's hash tables.
    * This is the place where duplicate components will be
    * detected.
    * TODO: I think normally we should support imports of the
    *   same namespace from multiple locations. We don't do currently,
    *   but if we do then according to:
    *   http://www.w3.org/Bugs/Public/show_bug.cgi?id=2224
    *   we would need, if imported directly, to import redefined
    *   components as well to be able to catch clashing components.
    *   (I hope I'll still know what this means after some months :-()
    */
    if (bucket == NULL)
	return(-1);
    if (bucket->flags & XML_XSD10_SCHEMA_BUCKET_COMPS_ADDED)
	return(0);
    bucket->flags |= XML_XSD10_SCHEMA_BUCKET_COMPS_ADDED;

    for (i = 0; i < bucket->globals->nbItems; i++) {
	item = (XmlXsd10BasicItem *) bucket->globals->items[i];
	table = NULL;
	switch (item->type) {
	    case XML_XSD10_SCHEMA_TYPE_COMPLEX:
	    case XML_XSD10_SCHEMA_TYPE_SIMPLE:
		if (XSD10_WXS_REDEFINED_TYPE(item))
		    continue;
		name = (XSD10_WXS_TYPE_CAST item)->name;
		XSD10_WXS_GET_GLOBAL_HASH(bucket, typeDecl)
		break;
	    case XML_XSD10_SCHEMA_TYPE_ELEMENT:
		name = (XSD10_WXS_ELEM_CAST item)->name;
		XSD10_WXS_GET_GLOBAL_HASH(bucket, elemDecl)
		break;
	    case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE:
		name = (XSD10_WXS_ATTR_CAST item)->name;
		XSD10_WXS_GET_GLOBAL_HASH(bucket, attrDecl)
		break;
	    case XML_XSD10_SCHEMA_TYPE_GROUP:
		if (XSD10_WXS_REDEFINED_MODEL_GROUP_DEF(item))
		    continue;
		name = (XSD10_WXS_MODEL_GROUPDEF_CAST item)->name;
		XSD10_WXS_GET_GLOBAL_HASH(bucket, groupDecl)
		break;
	    case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
		if (XSD10_WXS_REDEFINED_ATTR_GROUP(item))
		    continue;
		name = (XSD10_WXS_ATTR_GROUP_CAST item)->name;
		XSD10_WXS_GET_GLOBAL_HASH(bucket, attrgrpDecl)
		break;
	    case XML_XSD10_SCHEMA_TYPE_IDC_KEY:
	    case XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE:
	    case XML_XSD10_SCHEMA_TYPE_IDC_KEYREF:
		name = (XSD10_WXS_IDC_CAST item)->name;
		XSD10_WXS_GET_GLOBAL_HASH(bucket, idcDef)
		break;
	    case XML_XSD10_SCHEMA_TYPE_NOTATION:
		name = ((XmlXsd10Notation *) item)->name;
		XSD10_WXS_GET_GLOBAL_HASH(bucket, notaDecl)
		break;
	    default:
		XSD10_PERROR_INT("xmlSchemaAddComponents",
		    "Unexpected global component type");
		continue;
	}
	if (*table == NULL) {
	    *table = xml_xsd10_hash_create_dict(10, pctxt->dict);
	    if (*table == NULL) {
		XSD10_PERROR_INT("xmlSchemaAddComponents",
		    "failed to create a component hash table");
		return(-1);
	    }
	}
	err = xml_xsd10_hash_add_entry(*table, name, item);
	if (err != 0) {
	    char *str = NULL;

	    xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
		XML_XSD10_SCHEMAP_REDEFINED_TYPE,
		XSD10_WXS_ITEM_NODE(item),
		XSD10_WXS_BASIC_CAST item,
		"A global %s '%s' does already exist",
		XSD10_WXS_ITEM_TYPE_NAME(item),
		xml_xsd10_get_component_q_name(&str, item));
	    XSD10_FREE_AND_NULL(str);
	}
    }
    /*
    * Process imported/included schemas.
    */
    if (bucket->relations != NULL) {
	XmlXsd10SchemaRelation *rel = bucket->relations;
	do {
	    if ((rel->bucket != NULL) &&
		((rel->bucket->flags & XML_XSD10_SCHEMA_BUCKET_COMPS_ADDED) == 0)) {
		if (xml_xsd10_add_components(pctxt, rel->bucket) == -1)
		    return(-1);
	    }
	    rel = rel->next;
	} while (rel != NULL);
    }
    return(0);
}

/** Port of xmlSchemaFixupComponents. */
int xml_xsd10_fixup_components(XmlXsd10ParserCtxt *pctxt, XmlXsd10Bucket *rootBucket)
{
    XmlXsd10ConstructionCtxt *con = pctxt->constructor;
    XmlXsd10TreeItem *item, **items;
    int nbItems, i, ret = 0;
    XmlXsd10Bucket *oldbucket = con->bucket;
    XmlXsd10Element *elemDecl;

#define XSD10_FIXHFAILURE if (pctxt->err == XML_XSD10_SCHEMAP_INTERNAL) goto exit_failure;

    if ((con->pending == NULL) ||
	(con->pending->nbItems == 0))
	return(0);

    /*
    * Since xmlSchemaFixupComplexType() will create new particles
    * (local components), and those particle components need a bucket
    * on the constructor, we'll assure here that the constructor has
    * a bucket.
    * TODO: Think about storing locals _only_ on the main bucket.
    */
    if (con->bucket == NULL)
	con->bucket = rootBucket;

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
    xml_xsd10_check_src_redefine_first(pctxt);

    /*
    * Add global components to the schemata's hash tables.
    */
    xml_xsd10_add_components(pctxt, rootBucket);

    pctxt->ctxtType = NULL;
    items = (XmlXsd10TreeItem **) con->pending->items;
    nbItems = con->pending->nbItems;
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
    *   - the memberTypes of union types
    *   - the itemType of list types
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
    for (i = 0; i < nbItems; i++) {
	item = items[i];
	switch (item->type) {
	    case XML_XSD10_SCHEMA_TYPE_ELEMENT:
		xml_xsd10_resolve_element_references(
		    (XmlXsd10Element *) item, pctxt);
		XSD10_FIXHFAILURE;
		break;
	    case XML_XSD10_SCHEMA_TYPE_COMPLEX:
	    case XML_XSD10_SCHEMA_TYPE_SIMPLE:
		xml_xsd10_resolve_type_references(
		    (XmlXsd10Type *) item, pctxt);
		XSD10_FIXHFAILURE;
		break;
	    case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE:
		xml_xsd10_resolve_attr_type_references(
		    (XmlXsd10Attribute *) item, pctxt);
		XSD10_FIXHFAILURE;
		break;
	    case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE_USE:
		xml_xsd10_resolve_attr_use_references(
		    (XmlXsd10AttributeUse *) item, pctxt);
		XSD10_FIXHFAILURE;
		break;
	    case XML_XSD10_SCHEMA_EXTRA_QNAMEREF:
		if ((XSD10_WXS_QNAME_CAST item)->itemType ==
		    XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP)
		{
		    xml_xsd10_resolve_attr_group_references(
			XSD10_WXS_QNAME_CAST item, pctxt);
		}
		XSD10_FIXHFAILURE;
		break;
	    case XML_XSD10_SCHEMA_TYPE_SEQUENCE:
	    case XML_XSD10_SCHEMA_TYPE_CHOICE:
	    case XML_XSD10_SCHEMA_TYPE_ALL:
		xml_xsd10_resolve_model_group_particle_references(pctxt,
		    XSD10_WXS_MODEL_GROUP_CAST item);
		XSD10_FIXHFAILURE;
		break;
	    case XML_XSD10_SCHEMA_TYPE_IDC_KEY:
	    case XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE:
	    case XML_XSD10_SCHEMA_TYPE_IDC_KEYREF:
		xml_xsd10_resolve_idc_key_references(
		    (XmlXsd10IDC *) item, pctxt);
		XSD10_FIXHFAILURE;
		break;
	    case XML_XSD10_SCHEMA_EXTRA_ATTR_USE_PROHIB:
		/*
		* Handle attribute prohibition which had a
		* "ref" attribute.
		*/
		xml_xsd10_resolve_attr_use_prohib_references(
		    XSD10_WXS_ATTR_PROHIB_CAST item, pctxt);
		XSD10_FIXHFAILURE;
		break;
	    default:
		break;
	}
    }
    if (pctxt->nberrors != 0)
	goto exit_error;

    /*
    * Now that all references are resolved we
    * can check for circularity of...
    * 1. the base axis of type definitions
    * 2. nested model group definitions
    * 3. nested attribute group definitions
    * TODO: check for circular substitution groups.
    */
    for (i = 0; i < nbItems; i++) {
	item = items[i];
	/*
	* Let's better stop on the first error here.
	*/
	switch (item->type) {
	    case XML_XSD10_SCHEMA_TYPE_COMPLEX:
	    case XML_XSD10_SCHEMA_TYPE_SIMPLE:
		xml_xsd10_check_type_def_circular(
		    (XmlXsd10Type *) item, pctxt);
		XSD10_FIXHFAILURE;
		if (pctxt->nberrors != 0)
		    goto exit_error;
		break;
	    case XML_XSD10_SCHEMA_TYPE_GROUP:
		xml_xsd10_check_group_def_circular(
		    (XmlXsd10ModelGroupDef *) item, pctxt);
		XSD10_FIXHFAILURE;
		if (pctxt->nberrors != 0)
		    goto exit_error;
		break;
	    case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
		xml_xsd10_check_attr_group_circular(
		    (XmlXsd10AttributeGroup *) item, pctxt);
		XSD10_FIXHFAILURE;
		if (pctxt->nberrors != 0)
		    goto exit_error;
		break;
	    default:
		break;
	}
    }
    if (pctxt->nberrors != 0)
	goto exit_error;
    /*
    * Model group definition references:
    * Such a reference is reflected by a particle at the component
    * level. Until now the 'term' of such particles pointed
    * to the model group definition; this was done, in order to
    * ease circularity checks. Now we need to set the 'term' of
    * such particles to the model group of the model group definition.
    */
    for (i = 0; i < nbItems; i++) {
	item = items[i];
	switch (item->type) {
	    case XML_XSD10_SCHEMA_TYPE_SEQUENCE:
	    case XML_XSD10_SCHEMA_TYPE_CHOICE:
		xml_xsd10_model_group_to_model_group_def_fixup(pctxt,
		    XSD10_WXS_MODEL_GROUP_CAST item);
		break;
	    default:
		break;
	}
    }
    if (pctxt->nberrors != 0)
	goto exit_error;
    /*
    * Expand attribute group references of attribute group definitions.
    */
    for (i = 0; i < nbItems; i++) {
	item = items[i];
	switch (item->type) {
            case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
		if ((! XSD10_WXS_ATTR_GROUP_EXPANDED(item)) &&
		    XSD10_WXS_ATTR_GROUP_HAS_REFS(item))
		{
		    xml_xsd10_attribute_group_expand_refs(pctxt,
			XSD10_WXS_ATTR_GROUP_CAST item);
		    XSD10_FIXHFAILURE;
		}
		break;
	    default:
		break;
	}
    }
    if (pctxt->nberrors != 0)
	goto exit_error;
    /*
    * First compute the variety of simple types. This is needed as
    * a separate step, since otherwise we won't be able to detect
    * circular union types in all cases.
    */
    for (i = 0; i < nbItems; i++) {
	item = items[i];
	switch (item->type) {
            case XML_XSD10_SCHEMA_TYPE_SIMPLE:
		if (XSD10_WXS_IS_TYPE_NOT_FIXED_1((XmlXsd10Type *) item)) {
		    xml_xsd10_fixup_simple_type_stage_one(pctxt,
			(XmlXsd10Type *) item);
		    XSD10_FIXHFAILURE;
		}
		break;
	    default:
		break;
	}
    }
    if (pctxt->nberrors != 0)
	goto exit_error;
    /*
    * Detect circular union types. Note that this needs the variety to
    * be already computed.
    */
    for (i = 0; i < nbItems; i++) {
	item = items[i];
	switch (item->type) {
            case XML_XSD10_SCHEMA_TYPE_SIMPLE:
		if (((XmlXsd10Type *) item)->memberTypes != NULL) {
		    xml_xsd10_check_union_type_def_circular(pctxt,
			(XmlXsd10Type *) item);
		    XSD10_FIXHFAILURE;
		}
		break;
	    default:
		break;
	}
    }
    if (pctxt->nberrors != 0)
	goto exit_error;

    /*
    * Do the complete type fixup for simple types.
    */
    for (i = 0; i < nbItems; i++) {
	item = items[i];
	switch (item->type) {
            case XML_XSD10_SCHEMA_TYPE_SIMPLE:
		if (XSD10_WXS_IS_TYPE_NOT_FIXED(XSD10_WXS_TYPE_CAST item)) {
		    xml_xsd10_fixup_simple_type_stage_two(pctxt, XSD10_WXS_TYPE_CAST item);
		    XSD10_FIXHFAILURE;
		}
		break;
	    default:
		break;
	}
    }
    if (pctxt->nberrors != 0)
	goto exit_error;
    /*
    * At this point we need build and check all simple types.
    */
    /*
    * Apply constraints for attribute declarations.
    */
    for (i = 0; i < nbItems; i++) {
	item = items[i];
	switch (item->type) {
	    case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE:
		xml_xsd10_check_attr_props_correct(pctxt, XSD10_WXS_ATTR_CAST item);
		XSD10_FIXHFAILURE;
		break;
	    default:
		break;
	}
    }
    if (pctxt->nberrors != 0)
	goto exit_error;
    /*
    * Apply constraints for attribute uses.
    */
    for (i = 0; i < nbItems; i++) {
	item = items[i];
	switch (item->type) {
	    case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE_USE:
		if (((XmlXsd10AttributeUse *)item)->defValue != NULL) {
		    xml_xsd10_check_attr_use_props_correct(pctxt,
			XSD10_WXS_ATTR_USE_CAST item);
		    XSD10_FIXHFAILURE;
		}
		break;
	    default:
		break;
	}
    }
    if (pctxt->nberrors != 0)
	goto exit_error;

    /*
    * Apply constraints for attribute group definitions.
    */
    for (i = 0; i < nbItems; i++) {
	item = items[i];
	switch (item->type) {
	case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
	    if (( (XSD10_WXS_ATTR_GROUP_CAST item)->attrUses != NULL) &&
		( (XSD10_WXS_LIST_CAST (XSD10_WXS_ATTR_GROUP_CAST item)->attrUses)->nbItems > 1))
	    {
		xml_xsd10_check_ag_props_correct(pctxt, XSD10_WXS_ATTR_GROUP_CAST item);
		XSD10_FIXHFAILURE;
	    }
	    break;
	default:
	    break;
	}
    }
    if (pctxt->nberrors != 0)
	goto exit_error;

    /*
    * Apply constraints for redefinitions.
    */
    if (XSD10_WXS_CONSTRUCTOR(pctxt)->redefs != NULL)
	xml_xsd10_check_src_redefine_second(pctxt);
    if (pctxt->nberrors != 0)
	goto exit_error;

    /*
    * Complex types are built and checked.
    */
    for (i = 0; i < nbItems; i++) {
	item = (XmlXsd10TreeItem *) con->pending->items[i];
	switch (item->type) {
	    case XML_XSD10_SCHEMA_TYPE_COMPLEX:
		if (XSD10_WXS_IS_TYPE_NOT_FIXED(XSD10_WXS_TYPE_CAST item)) {
		    xml_xsd10_fixup_complex_type(pctxt, XSD10_WXS_TYPE_CAST item);
		    XSD10_FIXHFAILURE;
		}
		break;
	    default:
		break;
	}
    }
    if (pctxt->nberrors != 0)
	goto exit_error;

    /*
    * The list could have changed, since xmlSchemaFixupComplexType()
    * will create particles and model groups in some cases.
    */
    items = (XmlXsd10TreeItem **) con->pending->items;
    nbItems = con->pending->nbItems;

    /*
    * Apply some constraints for element declarations.
    */
    for (i = 0; i < nbItems; i++) {
	item = items[i];
	switch (item->type) {
	    case XML_XSD10_SCHEMA_TYPE_ELEMENT:
		elemDecl = (XmlXsd10Element *) item;

		if ((elemDecl->flags & XML_XSD10_SCHEMAS_ELEM_INTERNAL_CHECKED) == 0)
		{
		    xml_xsd10_check_element_decl_component(
			(XmlXsd10Element *) elemDecl, pctxt);
		    XSD10_FIXHFAILURE;
		}

		break;
	    default:
		break;
	}
    }
    if (pctxt->nberrors != 0)
	goto exit_error;

    /*
    * Finally we can build the automaton from the content model of
    * complex types.
    */

    for (i = 0; i < nbItems; i++) {
	item = items[i];
	switch (item->type) {
	    case XML_XSD10_SCHEMA_TYPE_COMPLEX:
		xml_xsd10_build_content_model((XmlXsd10Type *) item, pctxt);
		/* FIXHFAILURE; */
		break;
	    default:
		break;
	}
    }
    if (pctxt->nberrors != 0)
	goto exit_error;
    /*
    * URGENT TODO: cos-element-consistent
    */
    goto exit;

exit_error:
    ret = pctxt->err;
    goto exit;

exit_failure:
    ret = -1;

exit:
    /*
    * Reset the constructor. This is needed for XSI acquisition, since
    * those items will be processed over and over again for every XSI
    * if not cleared here.
    */
    con->bucket = oldbucket;
    con->pending->nbItems = 0;
    if (con->substGroups != NULL) {
	xml_xsd10_hash_free(con->substGroups, xml_xsd10_subst_group_free_entry);
	con->substGroups = NULL;
    }
    if (con->redefs != NULL) {
	xml_xsd10_redef_list_free(con->redefs);
	con->redefs = NULL;
    }
    return(ret);
}
/**
 * Port of xmlSchemaParse.
 *
 * parse a schema definition resource and build an internal
 * XML Schema structure which can be used to validate instances.
 *
 * @param ctxt  a schema validation context
 * @returns the internal XML Schema structure built from the resource or
 *         NULL in case of error
 */
XmlXsd10Schema *xml_xsd10_parse(XmlXsd10ParserCtxt *ctxt)
{
    XmlXsd10Schema *mainSchema = NULL;
    XmlXsd10Bucket *bucket = NULL;
    int res;

    /*
    * This one is used if the schema to be parsed was specified via
    * the API; i.e. not automatically by the validated instance document.
    */

    if (xml_xsd10_init_types() < 0)
        return (NULL);

    if (ctxt == NULL)
        return (NULL);

    /* TODO: Init the context. Is this all we need?*/
    ctxt->nberrors = 0;
    ctxt->err = 0;
    ctxt->counter = 0;

    /* Create the *main* schema. */
    mainSchema = xml_xsd10_new_schema(ctxt);
    if (mainSchema == NULL)
	goto exit_failure;
    /*
    * Create the schema constructor.
    */
    if (ctxt->constructor == NULL) {
	ctxt->constructor = xml_xsd10_construction_ctxt_create(ctxt->dict);
	if (ctxt->constructor == NULL)
	    goto exit_failure;
	/* Take ownership of the constructor to be able to free it. */
	ctxt->ownsConstructor = 1;
    }
    ctxt->constructor->mainSchema = mainSchema;
    /*
    * Locate and add the schema document.
    */
    res = xml_xsd10_add_schema_doc(ctxt, XML_XSD10_SCHEMA_SCHEMA_MAIN,
	ctxt->URL, ctxt->doc, ctxt->buffer, ctxt->size, NULL,
	NULL, NULL, &bucket);
    if (res == -1)
	goto exit_failure;
    if (res != 0)
	goto exit;

    if (bucket == NULL) {
	/* TODO: Error code, actually we failed to *locate* the schema. */
	if (ctxt->URL)
	    xml_xsd10_custom_err(XSD10_ACTXT_CAST ctxt, XML_XSD10_SCHEMAP_FAILED_LOAD,
		NULL, NULL,
		"Failed to locate the main schema resource at '%s'",
		ctxt->URL, NULL);
	else
	    xml_xsd10_custom_err(XSD10_ACTXT_CAST ctxt, XML_XSD10_SCHEMAP_FAILED_LOAD,
		NULL, NULL,
		"Failed to locate the main schema resource",
		    NULL, NULL);
	goto exit;
    }
    /* Then do the parsing for good. */
    if (xml_xsd10_parse_new_doc_with_context(ctxt, mainSchema, bucket) == -1)
	goto exit_failure;
    if (ctxt->nberrors != 0)
	goto exit;

    mainSchema->doc = bucket->doc;
    mainSchema->preserve = ctxt->preserve;

    ctxt->schema = mainSchema;

    if (xml_xsd10_fixup_components(ctxt, XSD10_WXS_CONSTRUCTOR(ctxt)->mainBucket) == -1)
	goto exit_failure;

    /*
    * TODO: This is not nice, since we cannot distinguish from the
    * result if there was an internal error or not.
    */
exit:
    if (ctxt->nberrors != 0) {
	if (mainSchema) {
	    xml_xsd10_free(mainSchema);
	    mainSchema = NULL;
	}
	if (ctxt->constructor) {
	    xml_xsd10_construction_ctxt_free(ctxt->constructor);
	    ctxt->constructor = NULL;
	    ctxt->ownsConstructor = 0;
	}
    }
    ctxt->schema = NULL;
    return(mainSchema);
exit_failure:
    /*
    * Quite verbose, but should catch internal errors, which were
    * not communicated.
    */
    if (mainSchema) {
        xml_xsd10_free(mainSchema);
	mainSchema = NULL;
    }
    if (ctxt->constructor) {
	xml_xsd10_construction_ctxt_free(ctxt->constructor);
	ctxt->constructor = NULL;
	ctxt->ownsConstructor = 0;
    }
    XSD10_PERROR_INT2("xmlSchemaParse",
	"An internal error occurred");
    ctxt->schema = NULL;
    return(NULL);
}

/**
 * Set the callback functions used to handle errors for a validation context
 *
 * @deprecated Use xmlSchemaSetParserStructuredErrors.
 *
 * @param ctxt  a schema validation context
 * @param err  the error callback
 * @param warn  the warning callback
 * @param ctx  contextual data for the callbacks
 */

#undef XML_XSD10_SCHEMA_ACTION_DERIVE
#undef XML_XSD10_SCHEMA_ACTION_REDEFINE
#undef XSD10_WXS_ACTION_STR
#undef XSD10_FACET_RESTR_MUTUAL_ERR
#undef XSD10_FACET_RESTR_ERR
#undef XSD10_FACET_RESTR_FIXED_ERR
#undef XSD10_WXS_REDEFINED_TYPE
#undef XSD10_WXS_REDEFINED_MODEL_GROUP_DEF
#undef XSD10_WXS_REDEFINED_ATTR_GROUP
#undef XSD10_WXS_GET_GLOBAL_HASH
#undef XSD10_FIXHFAILURE

#endif // XML_XSD10_BUILD_MODULE
