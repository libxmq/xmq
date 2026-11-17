/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */
package org.libxmq.imp.ixml.earley;

import java.util.ArrayList;
import java.util.List;

/**
 * The result of the Earley recognizer: the input and one Earley set for each
 * input position, from 0 (before the first code point) to n (after the last).
 */
public final class Chart
{
    /** The input, as code points. */
    public final int[] input;
    /** The sets, sets.get(k) is the set at position k. */
    public final List<EarleySet> sets = new ArrayList<>();

    Chart(int[] input)
    {
        this.input = input;
        for (int k = 0; k <= input.length; k++) sets.add(new EarleySet(k));
    }

    /** The set at position k.
     * @param k the input position, 0 to input.length
     * @return the set */
    public EarleySet set(int k)
    {
        return sets.get(k);
    }

    /** The number of code points in the input.
     * @return the input length */
    public int length()
    {
        return input.length;
    }
}
