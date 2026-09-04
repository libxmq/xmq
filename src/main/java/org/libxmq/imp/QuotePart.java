
package org.libxmq.imp;

/**
 * Describes how a part of a quote is quoted.
 * @param start The start index of the part.
 * @param stop The stop index of the part.
 * @param msq The number of consecutive single quotes.
 * @param mdq The number of consecutive double quotes.
 * @param ent Whether the part is an entity.
 */
public record QuotePart(int start, int stop, int msq, int mdq, boolean ent) {};
