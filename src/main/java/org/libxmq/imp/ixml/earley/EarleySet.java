/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */
package org.libxmq.imp.ixml.earley;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * The Earley set for one input position: the items that are alive there.
 * Set k holds the items whose matched part ends at position k.
 *
 * The items are kept in the order they were added, so the parser can work
 * through the set with an index while new items are appended to it.
 * An item is only added once.
 */
public final class EarleySet
{
    /** The input position of this set. */
    public final int position;
    /** The token read at this position (see Tokens), -1 in the last set, at the end of the input. */
    public final int token;
    private final List<Item> items = new ArrayList<>();
    private final Set<Item> seen = new HashSet<>();

    EarleySet(int position, int token)
    {
        this.position = position;
        this.token = token;
    }

    /** Add the item, unless it is already in the set.
     * @param item the item
     * @return true if the item was added */
    boolean add(Item item)
    {
        if (!seen.add(item)) return false;
        items.add(item);
        return true;
    }

    /** Returns true if the item is in the set.
     * @param item the item
     * @return true if present */
    public boolean contains(Item item)
    {
        return seen.contains(item);
    }

    /** Remove an item, used when an item is forbidden by a not.
     * @param item the item */
    void remove(Item item)
    {
        if (seen.remove(item)) items.remove(item);
    }

    /** The item at index i, in the order the items were added.
     * @param i the index
     * @return the item */
    public Item get(int i)
    {
        return items.get(i);
    }

    /** The number of items.
     * @return the size */
    public int size()
    {
        return items.size();
    }

    /** Returns true if there are no items: the parse died before this position.
     * @return true if empty */
    public boolean isEmpty()
    {
        return items.isEmpty();
    }

    /** The items, in the order they were added.
     * @return a read only view */
    public List<Item> items()
    {
        return java.util.Collections.unmodifiableList(items);
    }
}
