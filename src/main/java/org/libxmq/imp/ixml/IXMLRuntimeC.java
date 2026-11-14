/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */
package org.libxmq.imp.ixml;

/**
 * The C runtime for adaptive prediction, emitted into generated parsers that
 * have decisions without a static lookahead DFA. It is a port of
 * IXMLLookahead.predictAdaptive and predictDynamic (ALL(*) style):
 *
 * - The grammar is available as ATN tables (PFX_atn_*, emitted by
 *   IXMLGenerateC.genAtn).
 * - The parser keeps its return stack as an interned stack (p->dyn_ctx), a
 *   call that is not the last term of its alternative pushes the position
 *   after the call (tail calls push nothing, like in the ATN).
 * - A prediction first walks a lazily built SLL DFA, cached per decision and
 *   the top PFX_CTX_K return positions of the stack (the rest is UNKNOWN,
 *   ie all call sites). A conflict between alternatives that have an
 *   UNKNOWN stack falls back to the exact LL simulation with the full
 *   stack. Costs resolve conflicts, an ambiguity with an exact stack picks
 *   the first of the cheapest alternatives.
 *
 * PFX is replaced with the prefix of the generated parser.
 */
final class IXMLRuntimeC
{
    private IXMLRuntimeC()
    {
    }

    static String code(String prefix)
    {
        return CODE.replace("PFX", prefix);
    }

    private static final String CODE = """
// ------------------------------------------------------------------------------------
// Adaptive prediction (ALL(*) style) for the decisions without a static lookahead DFA.

typedef struct PFX_Stack PFX_Stack;
#define PFX_WAIT_EOF (-1)
#define PFX_DONE (-2)
#define PFX_CTX_K 4

struct PFX_Stack
{
    int rule, alt, idx;
    PFX_Stack *next;
    PFX_Stack *chain;
};
static PFX_Stack PFX_unknown_ = { -3, 0, 0, NULL, NULL };
#define PFX_UNKNOWN (&PFX_unknown_)

typedef struct PFX_Neg PFX_Neg;
struct PFX_Neg
{
    int n;      // Number of (predicate, offset) pairs.
    int *pairs;
    unsigned hash;
    PFX_Neg *chain;
};
static PFX_Neg PFX_dead_ = { -1, NULL, 0, NULL };
#define PFX_DEAD (&PFX_dead_)

typedef struct
{
    int rule, alt, idx, predicted, cost;
    PFX_Stack *stack;
    PFX_Neg *neg;
} PFX_Config;

static unsigned PFX_mix(unsigned h, unsigned v)
{
    h ^= v + 0x9e3779b9u + (h << 6) + (h >> 2);
    return h;
}

static unsigned PFX_ptr_hash(const void *p)
{
    uintptr_t v = (uintptr_t)p;
    return (unsigned)(v ^ (v >> 32));
}

static void *PFX_xmalloc(size_t n)
{
    void *p = malloc(n);
    if (!p) { fprintf(stderr, "out of memory\\n"); exit(1); }
    return p;
}

static void *PFX_xcalloc(size_t n, size_t s)
{
    void *p = calloc(n, s);
    if (!p) { fprintf(stderr, "out of memory\\n"); exit(1); }
    return p;
}

// ---- Interned stacks.

static PFX_Stack **PFX_stacks;
static size_t PFX_stacks_cap, PFX_stacks_n;

static unsigned PFX_stack_hash(int r, int a, int i, PFX_Stack *next)
{
    return PFX_mix(PFX_mix(PFX_mix(PFX_mix(17u, (unsigned)r), (unsigned)a), (unsigned)i), PFX_ptr_hash(next));
}

static PFX_Stack *PFX_push(int r, int a, int i, PFX_Stack *next)
{
    if (!PFX_stacks)
    {
        PFX_stacks_cap = 4096;
        PFX_stacks = (PFX_Stack**)PFX_xcalloc(PFX_stacks_cap, sizeof(PFX_Stack*));
    }
    unsigned h = PFX_stack_hash(r, a, i, next);
    size_t b = h & (PFX_stacks_cap - 1);
    for (PFX_Stack *s = PFX_stacks[b]; s; s = s->chain)
    {
        if (s->rule == r && s->alt == a && s->idx == i && s->next == next) return s;
    }
    if (PFX_stacks_n * 2 > PFX_stacks_cap)
    {
        size_t ncap = PFX_stacks_cap * 2;
        PFX_Stack **nt = (PFX_Stack**)PFX_xcalloc(ncap, sizeof(PFX_Stack*));
        for (size_t k = 0; k < PFX_stacks_cap; k++)
        {
            PFX_Stack *s = PFX_stacks[k];
            while (s)
            {
                PFX_Stack *nx = s->chain;
                size_t nb = PFX_stack_hash(s->rule, s->alt, s->idx, s->next) & (ncap - 1);
                s->chain = nt[nb];
                nt[nb] = s;
                s = nx;
            }
        }
        free(PFX_stacks);
        PFX_stacks = nt;
        PFX_stacks_cap = ncap;
        b = h & (ncap - 1);
    }
    PFX_Stack *s = (PFX_Stack*)PFX_xmalloc(sizeof(PFX_Stack));
    s->rule = r;
    s->alt = a;
    s->idx = i;
    s->next = next;
    s->chain = PFX_stacks[b];
    PFX_stacks[b] = s;
    PFX_stacks_n++;
    return s;
}

// The top k return positions with an UNKNOWN bottom (exact if shallower).
static PFX_Stack *PFX_truncate(PFX_Stack *s, int k)
{
    if (s == NULL || s == PFX_UNKNOWN) return s;
    if (k == 0) return PFX_UNKNOWN;
    return PFX_push(s->rule, s->alt, s->idx, PFX_truncate(s->next, k - 1));
}

static bool PFX_has_unknown(PFX_Stack *s)
{
    for (; s; s = s->next) if (s == PFX_UNKNOWN) return true;
    return false;
}

// ---- Interned negative lookahead constraints.

static PFX_Neg **PFX_negs;
static size_t PFX_negs_cap, PFX_negs_n;

static int PFX_pair_cmp(const void *a, const void *b)
{
    const int *x = (const int*)a, *y = (const int*)b;
    if (x[0] != y[0]) return x[0] < y[0] ? -1 : 1;
    if (x[1] != y[1]) return x[1] < y[1] ? -1 : 1;
    return 0;
}

// Intern the pairs (sorted, duplicates removed), NULL if empty. Takes the pairs array.
static PFX_Neg *PFX_neg_make(int *pairs, int n)
{
    if (n == 0) { free(pairs); return NULL; }
    qsort(pairs, (size_t)n, 2 * sizeof(int), PFX_pair_cmp);
    int m = 0;
    for (int i = 0; i < n; i++)
    {
        if (m > 0 && pairs[2*(m-1)] == pairs[2*i] && pairs[2*(m-1)+1] == pairs[2*i+1]) continue;
        pairs[2*m] = pairs[2*i];
        pairs[2*m+1] = pairs[2*i+1];
        m++;
    }
    unsigned h = 31;
    for (int i = 0; i < 2*m; i++) h = PFX_mix(h, (unsigned)pairs[i]);
    if (!PFX_negs)
    {
        PFX_negs_cap = 256;
        PFX_negs = (PFX_Neg**)PFX_xcalloc(PFX_negs_cap, sizeof(PFX_Neg*));
    }
    size_t b = h & (PFX_negs_cap - 1);
    for (PFX_Neg *g = PFX_negs[b]; g; g = g->chain)
    {
        if (g->hash == h && g->n == m && !memcmp(g->pairs, pairs, sizeof(int) * 2 * (size_t)m))
        {
            free(pairs);
            return g;
        }
    }
    if (PFX_negs_n * 2 > PFX_negs_cap)
    {
        size_t ncap = PFX_negs_cap * 2;
        PFX_Neg **nt = (PFX_Neg**)PFX_xcalloc(ncap, sizeof(PFX_Neg*));
        for (size_t k = 0; k < PFX_negs_cap; k++)
        {
            PFX_Neg *g = PFX_negs[k];
            while (g)
            {
                PFX_Neg *nx = g->chain;
                size_t nb = g->hash & (ncap - 1);
                g->chain = nt[nb];
                nt[nb] = g;
                g = nx;
            }
        }
        free(PFX_negs);
        PFX_negs = nt;
        PFX_negs_cap = ncap;
        b = h & (ncap - 1);
    }
    PFX_Neg *g = (PFX_Neg*)PFX_xmalloc(sizeof(PFX_Neg));
    g->n = m;
    g->pairs = pairs;
    g->hash = h;
    g->chain = PFX_negs[b];
    PFX_negs[b] = g;
    PFX_negs_n++;
    return g;
}

// A cache of the common case of PFX_neg_add: a predicate added to no constraints.
static PFX_Neg **PFX_single;
static size_t PFX_nsingle;

static PFX_Neg *PFX_neg_add(PFX_Neg *g, int pred)
{
    if (g == NULL)
    {
        if ((size_t)pred >= PFX_nsingle)
        {
            size_t nn = (size_t)pred + 64;
            PFX_single = (PFX_Neg**)realloc(PFX_single, nn * sizeof(PFX_Neg*));
            if (!PFX_single) { fprintf(stderr, "out of memory\\n"); exit(1); }
            memset(PFX_single + PFX_nsingle, 0, (nn - PFX_nsingle) * sizeof(PFX_Neg*));
            PFX_nsingle = nn;
        }
        if (PFX_single[pred]) return PFX_single[pred];
    }
    int n = g ? g->n : 0;
    int *pairs = (int*)PFX_xmalloc(sizeof(int) * 2 * (size_t)(n + 1));
    if (n) memcpy(pairs, g->pairs, sizeof(int) * 2 * (size_t)n);
    pairs[2*n] = pred;
    pairs[2*n+1] = 0;
    PFX_Neg *r = PFX_neg_make(pairs, n + 1);
    if (g == NULL) PFX_single[pred] = r;
    return r;
}

static bool PFX_set_has(int set, int c)
{
    return PFX_inset(PFX_atn_set[set], (size_t)PFX_atn_setn[set], PFX_atn_ascii[set], c);
}

// The constraints after consuming sym, PFX_DEAD if a predicate matched completely.
static PFX_Neg *PFX_neg_step(PFX_Neg *g, int sym)
{
    if (g == NULL || sym == PFX_EOF) return NULL;
    // The common case: no predicate matches sym, all constraints are satisfied.
    bool any = false;
    for (int i = 0; i < g->n && !any; i++)
    {
        int pr = g->pairs[2*i], k = g->pairs[2*i+1];
        if (PFX_set_has(PFX_atn_predsets[PFX_atn_pred0[pr] + k], sym)) any = true;
    }
    if (!any) return NULL;
    int *pairs = (int*)PFX_xmalloc(sizeof(int) * 2 * (size_t)g->n);
    int m = 0;
    for (int i = 0; i < g->n; i++)
    {
        int pr = g->pairs[2*i], k = g->pairs[2*i+1];
        int len = PFX_atn_pred0[pr+1] - PFX_atn_pred0[pr];
        if (!PFX_set_has(PFX_atn_predsets[PFX_atn_pred0[pr] + k], sym)) continue;
        if (k + 1 == len) { free(pairs); return PFX_DEAD; }
        pairs[2*m] = pr;
        pairs[2*m+1] = k + 1;
        m++;
    }
    return PFX_neg_make(pairs, m);
}

// ---- Configuration sets.

typedef struct { uint32_t gen, idx, hash; } PFX_Slot;

typedef struct
{
    PFX_Config *v;
    size_t n, cap;
    PFX_Slot *hs;   // Open addressing hash of indexes into v, a slot is used if gen matches.
    size_t hcap;
    uint32_t gen;
} PFX_Set;

static inline unsigned PFX_config_hash(const PFX_Config *c)
{
    uint64_t h = (uint64_t)(uint32_t)c->rule * 0x9E3779B97F4A7C15ull;
    h ^= ((uint64_t)(uint32_t)c->alt << 16) ^ ((uint64_t)(uint32_t)c->idx << 32);
    h ^= (uint64_t)(uint32_t)c->predicted * 0xC2B2AE3D27D4EB4Full ^ (uint64_t)(uint32_t)c->cost * 0x165667B19E3779F9ull;
    h ^= (uint64_t)(uintptr_t)c->stack * 0xFF51AFD7ED558CCDull;
    h ^= (uint64_t)(uintptr_t)c->neg * 0xC4CEB9FE1A85EC53ull;
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDull;
    h ^= h >> 29;
    return (unsigned)h;
}

static bool PFX_config_eq(const PFX_Config *a, const PFX_Config *b)
{
    return a->rule == b->rule && a->alt == b->alt && a->idx == b->idx && a->predicted == b->predicted
        && a->cost == b->cost && a->stack == b->stack && a->neg == b->neg;
}

static void PFX_set_free(PFX_Set *s)
{
    free(s->v);
    free(s->hs);
    memset(s, 0, sizeof(*s));
}

// Empty the set but keep its memory, the generation makes all slots unused.
static void PFX_set_clear(PFX_Set *s)
{
    s->n = 0;
    s->gen++;
    if (s->gen == 0)
    {
        if (s->hs) memset(s->hs, 0, s->hcap * sizeof(PFX_Slot));
        s->gen = 1;
    }
}

static void PFX_set_rehash(PFX_Set *s, size_t ncap)
{
    PFX_Slot *old = s->hs;
    size_t ocap = s->hcap;
    s->hs = (PFX_Slot*)PFX_xcalloc(ncap, sizeof(PFX_Slot));
    s->hcap = ncap;
    if (s->gen == 0) s->gen = 1;
    for (size_t i = 0; i < ocap; i++)
    {
        if (old[i].gen != s->gen) continue;
        size_t b = old[i].hash & (ncap - 1);
        while (s->hs[b].gen == s->gen) b = (b + 1) & (ncap - 1);
        s->hs[b] = old[i];
    }
    free(old);
}

// Add the configuration, returns false if it was already there.
static bool PFX_set_add(PFX_Set *s, const PFX_Config *c)
{
    if ((s->n + 1) * 2 > s->hcap) PFX_set_rehash(s, s->hcap ? s->hcap * 2 : 64);
    unsigned h = PFX_config_hash(c);
    size_t b = h & (s->hcap - 1);
    while (s->hs[b].gen == s->gen)
    {
        if (s->hs[b].hash == h && PFX_config_eq(&s->v[s->hs[b].idx], c)) return false;
        b = (b + 1) & (s->hcap - 1);
    }
    if (s->n == s->cap)
    {
        s->cap = s->cap ? s->cap * 2 : 32;
        s->v = (PFX_Config*)realloc(s->v, s->cap * sizeof(PFX_Config));
        if (!s->v) { fprintf(stderr, "out of memory\\n"); exit(1); }
    }
    s->v[s->n] = *c;
    s->hs[b].gen = s->gen;
    s->hs[b].idx = (uint32_t)s->n;
    s->hs[b].hash = h;
    s->n++;
    return true;
}

// Is the configuration in the set?
static bool PFX_set_contains(const PFX_Set *s, const PFX_Config *c)
{
    if (!s->hcap) return false;
    unsigned h = PFX_config_hash(c);
    size_t b = h & (s->hcap - 1);
    while (s->hs[b].gen == s->gen)
    {
        if (s->hs[b].hash == h && PFX_config_eq(&s->v[s->hs[b].idx], c)) return true;
        b = (b + 1) & (s->hcap - 1);
    }
    return false;
}

typedef struct { PFX_Config *v; size_t n, cap; } PFX_Vec;

static void PFX_vec_push(PFX_Vec *t, const PFX_Config *c)
{
    if (t->n == t->cap)
    {
        t->cap = t->cap ? t->cap * 2 : 64;
        t->v = (PFX_Config*)realloc(t->v, t->cap * sizeof(PFX_Config));
        if (!t->v) { fprintf(stderr, "out of memory\\n"); exit(1); }
    }
    t->v[t->n++] = *c;
}

// ---- The ATN closure.

static bool PFX_is_transition(const PFX_Config *c)
{
    if (c->rule < 0) return true;
    int k = PFX_atn_alt0[c->rule] + c->alt;
    int len = PFX_atn_term0[k+1] - PFX_atn_term0[k];
    return c->idx < len && PFX_atn_kind[PFX_atn_term0[k] + c->idx] == 0;
}

// Follow calls, returns, insertions, negative lookaheads and rule ends.
static PFX_Set PFX_seen;
static PFX_Vec PFX_todo;

static void PFX_closure_raw(const PFX_Config *seeds, size_t nseeds, PFX_Set *result)
{
    PFX_Set seen = PFX_seen;   // Reused between closures, the memory is kept.
    PFX_Vec todo = PFX_todo;
    PFX_set_clear(&seen);
    todo.n = 0;
    for (size_t i = nseeds; i > 0; i--) PFX_vec_push(&todo, &seeds[i-1]);
    while (todo.n)
    {
        PFX_Config c = todo.v[--todo.n];
        if (!PFX_set_add(&seen, &c)) continue;
        if (PFX_is_transition(&c)) { PFX_set_add(result, &c); continue; }
        int k = PFX_atn_alt0[c.rule] + c.alt;
        int t0 = PFX_atn_term0[k];
        int len = PFX_atn_term0[k+1] - t0;
        PFX_Config n = c;
        if (c.idx == len)
        {
            // The end of the rule.
            if (c.stack == NULL)
            {
                n.rule = PFX_WAIT_EOF; n.alt = 0; n.idx = 0;
                PFX_vec_push(&todo, &n);
            }
            else if (c.stack == PFX_UNKNOWN)
            {
                // SLL: continue into every call site.
                if (c.rule == PFX_ATN_START)
                {
                    n.rule = PFX_WAIT_EOF; n.alt = 0; n.idx = 0; n.stack = NULL;
                    PFX_vec_push(&todo, &n);
                }
                for (int s = PFX_atn_site0[c.rule]; s < PFX_atn_site0[c.rule+1]; s++)
                {
                    n.rule = PFX_atn_sites[3*s];
                    n.alt = PFX_atn_sites[3*s+1];
                    n.idx = PFX_atn_sites[3*s+2] + 1;
                    n.stack = PFX_UNKNOWN;
                    PFX_vec_push(&todo, &n);
                }
            }
            else
            {
                n.rule = c.stack->rule; n.alt = c.stack->alt; n.idx = c.stack->idx; n.stack = c.stack->next;
                PFX_vec_push(&todo, &n);
            }
            continue;
        }
        int kind = PFX_atn_kind[t0 + c.idx];
        int arg = PFX_atn_arg[t0 + c.idx];
        if (kind == 1)
        {
            // A call, a tail call pushes nothing.
            PFX_Stack *s = c.stack;
            if (c.idx + 1 < len) s = PFX_push(c.rule, c.alt, c.idx + 1, c.stack);
            int nalts = PFX_atn_alt0[arg+1] - PFX_atn_alt0[arg];
            for (int a = 0; a < nalts; a++)
            {
                n.rule = arg; n.alt = a; n.idx = 0; n.stack = s; n.cost = c.cost + PFX_atn_cost[arg];
                PFX_vec_push(&todo, &n);
            }
        }
        else if (kind == 2)
        {
            // A negative lookahead constrains the following input.
            if (PFX_atn_pred0[arg+1] == PFX_atn_pred0[arg]) continue; // !'' never succeeds.
            n.idx = c.idx + 1;
            n.neg = PFX_neg_add(c.neg, arg);
            PFX_vec_push(&todo, &n);
        }
        else
        {
            // An insertion consumes nothing.
            n.idx = c.idx + 1;
            PFX_vec_push(&todo, &n);
        }
    }
    PFX_seen = seen;
    PFX_todo = todo;
}

// The union of two sets of constraints.
static PFX_Neg *PFX_neg_union(PFX_Neg *a, PFX_Neg *b)
{
    if (!a) return b;
    if (!b || a == b) return a;
    int *pairs = (int*)PFX_xmalloc(sizeof(int) * 2 * (size_t)(a->n + b->n));
    memcpy(pairs, a->pairs, sizeof(int) * 2 * (size_t)a->n);
    memcpy(pairs + 2 * a->n, b->pairs, sizeof(int) * 2 * (size_t)b->n);
    return PFX_neg_make(pairs, a->n + b->n);
}

// The closure of a single configuration only depends on its position and stack:
// the cost is added, the predicted alternative is carried and the constraints
// accumulate. So the closure of (rule, alt, idx, stack) with cost 0, no
// constraints and predicted 0 is computed once and adjusted for each use.
typedef struct PFX_Memo
{
    int rule, alt, idx;
    PFX_Stack *stack;
    PFX_Config *v;
    size_t n;
    struct PFX_Memo *chain;
} PFX_Memo;
static PFX_Memo **PFX_memos;
static size_t PFX_memos_cap, PFX_memos_n;

static PFX_Memo *PFX_memo(const PFX_Config *c)
{
    if (!PFX_memos)
    {
        PFX_memos_cap = 4096;
        PFX_memos = (PFX_Memo**)PFX_xcalloc(PFX_memos_cap, sizeof(PFX_Memo*));
    }
    unsigned h = PFX_stack_hash(c->rule, c->alt, c->idx, c->stack);
    size_t b = h & (PFX_memos_cap - 1);
    for (PFX_Memo *m = PFX_memos[b]; m; m = m->chain)
    {
        if (m->rule == c->rule && m->alt == c->alt && m->idx == c->idx && m->stack == c->stack) return m;
    }
    if (PFX_memos_n * 2 > PFX_memos_cap)
    {
        size_t ncap = PFX_memos_cap * 2;
        PFX_Memo **nt = (PFX_Memo**)PFX_xcalloc(ncap, sizeof(PFX_Memo*));
        for (size_t k = 0; k < PFX_memos_cap; k++)
        {
            PFX_Memo *m = PFX_memos[k];
            while (m)
            {
                PFX_Memo *nx = m->chain;
                size_t nb = PFX_stack_hash(m->rule, m->alt, m->idx, m->stack) & (ncap - 1);
                m->chain = nt[nb];
                nt[nb] = m;
                m = nx;
            }
        }
        free(PFX_memos);
        PFX_memos = nt;
        PFX_memos_cap = ncap;
        b = h & (ncap - 1);
    }
    PFX_Config seed = { c->rule, c->alt, c->idx, 0, 0, c->stack, NULL };
    PFX_Set set = {0};
    PFX_closure_raw(&seed, 1, &set);
    PFX_Memo *m = (PFX_Memo*)PFX_xmalloc(sizeof(PFX_Memo));
    m->rule = c->rule;
    m->alt = c->alt;
    m->idx = c->idx;
    m->stack = c->stack;
    m->v = set.v;
    m->n = set.n;
    free(set.hs);
    m->chain = PFX_memos[b];
    PFX_memos[b] = m;
    PFX_memos_n++;
    return m;
}

static void PFX_closure(const PFX_Config *seeds, size_t nseeds, PFX_Set *result)
{
    for (size_t i = 0; i < nseeds; i++)
    {
        const PFX_Config *c = &seeds[i];
        if (c->rule < 0)
        {
            PFX_set_add(result, c);
            continue;
        }
        PFX_Memo *m = PFX_memo(c);
        for (size_t k = 0; k < m->n; k++)
        {
            PFX_Config x = m->v[k];
            x.predicted = c->predicted;
            x.cost += c->cost;
            x.neg = PFX_neg_union(c->neg, x.neg);
            PFX_set_add(result, &x);
        }
    }
}

static bool PFX_matches(const PFX_Config *c, int sym)
{
    if (c->rule == PFX_WAIT_EOF) return sym == PFX_EOF;
    if (c->rule == PFX_DONE) return false;
    int k = PFX_atn_alt0[c->rule] + c->alt;
    return PFX_set_has(PFX_atn_arg[PFX_atn_term0[k] + c->idx], sym);
}

// The configurations that move on sym, advanced and closed.
// The configurations that move on sym, advanced (not closed), added to out.
static void PFX_advance(const PFX_Config *v, size_t n, int sym, PFX_Set *out)
{
    for (size_t i = 0; i < n; i++)
    {
        if (!PFX_matches(&v[i], sym)) continue;
        PFX_Neg *g = PFX_neg_step(v[i].neg, sym);
        if (g == PFX_DEAD) continue;
        PFX_Config m = v[i];
        m.neg = g;
        if (m.rule == PFX_WAIT_EOF) { m.rule = PFX_DONE; m.stack = NULL; }
        else m.idx++;
        PFX_set_add(out, &m);
    }
}

static PFX_Vec PFX_moved;

static void PFX_move(const PFX_Config *v, size_t n, int sym, PFX_Set *out)
{
    PFX_Vec moved = PFX_moved; // Reused, the memory is kept.
    moved.n = 0;
    for (size_t i = 0; i < n; i++)
    {
        if (!PFX_matches(&v[i], sym)) continue;
        PFX_Neg *g = PFX_neg_step(v[i].neg, sym);
        if (g == PFX_DEAD) continue;
        PFX_Config m = v[i];
        m.neg = g;
        if (m.rule == PFX_WAIT_EOF) { m.rule = PFX_DONE; m.stack = NULL; }
        else m.idx++;
        PFX_vec_push(&moved, &m);
    }
    PFX_closure(moved.v, moved.n, out);
    PFX_moved = moved;
}

// ---- Resolving conflicts with costs.

static const PFX_Config *PFX_sort_base;

static int PFX_key_cmp(const PFX_Config *a, const PFX_Config *b)
{
    if (a->rule != b->rule) return a->rule < b->rule ? -1 : 1;
    if (a->alt != b->alt) return a->alt < b->alt ? -1 : 1;
    if (a->idx != b->idx) return a->idx < b->idx ? -1 : 1;
    if (a->stack != b->stack) return (uintptr_t)a->stack < (uintptr_t)b->stack ? -1 : 1;
    if (a->neg != b->neg) return (uintptr_t)a->neg < (uintptr_t)b->neg ? -1 : 1;
    return 0;
}

static int PFX_resolve_cmp(const void *x, const void *y)
{
    const PFX_Config *a = &PFX_sort_base[*(const size_t*)x], *b = &PFX_sort_base[*(const size_t*)y];
    int k = PFX_key_cmp(a, b);
    if (k) return k;
    if (a->predicted != b->predicted) return a->predicted < b->predicted ? -1 : 1;
    if (a->cost != b->cost) return a->cost < b->cost ? -1 : 1;
    return 0;
}

// Where two alternatives reach the same position, stack and constraints, only the
// cheapest survives (the first of equally cheap ones). With sll, a tie at a stack
// with an UNKNOWN bottom is a conflict: returns false. The kept configurations are
// stored in out (in the original order).
static bool PFX_resolve(const PFX_Config *v, size_t n, bool sll, PFX_Config **out, size_t *nout)
{
    size_t *ix = (size_t*)PFX_xmalloc(sizeof(size_t) * (n ? n : 1));
    bool *drop = (bool*)PFX_xcalloc(n ? n : 1, sizeof(bool));
    for (size_t i = 0; i < n; i++) ix[i] = i;
    PFX_sort_base = v;
    qsort(ix, n, sizeof(size_t), PFX_resolve_cmp);
    bool ok = true;
    size_t g = 0;
    while (g < n)
    {
        size_t e = g + 1;
        while (e < n && PFX_key_cmp(&v[ix[g]], &v[ix[e]]) == 0) e++;
        // Group g..e has the same key, sorted on predicted and then cost.
        int min = 0x7fffffff, first = -1, cheapest = 0, nalts = 0;
        for (size_t i = g; i < e; i++)
        {
            if (i > g && v[ix[i]].predicted == v[ix[i-1]].predicted) continue;
            nalts++;
            int cost = v[ix[i]].cost; // The cheapest of this alternative.
            if (cost < min) { min = cost; first = v[ix[i]].predicted; cheapest = 1; }
            else if (cost == min) cheapest++;
        }
        if (nalts > 1)
        {
            if (cheapest > 1 && sll && PFX_has_unknown(v[ix[g]].stack)) { ok = false; break; }
            for (size_t i = g; i < e; i++) if (v[ix[i]].predicted != first) drop[ix[i]] = true;
        }
        g = e;
    }
    if (ok)
    {
        PFX_Config *r = (PFX_Config*)PFX_xmalloc(sizeof(PFX_Config) * (n ? n : 1));
        size_t m = 0;
        for (size_t i = 0; i < n; i++) if (!drop[i]) r[m++] = v[i];
        *out = r;
        *nout = m;
    }
    free(ix);
    free(drop);
    return ok;
}

// ---- The runtime SLL DFA.

typedef struct PFX_DState PFX_DState;
typedef struct PFX_Edge { int sym; PFX_DState *to; struct PFX_Edge *next; } PFX_Edge;
struct PFX_DState
{
    int rule;
    PFX_Config *key;    // The seeds (the configurations that moved here), the identity.
    size_t nkey;
    unsigned hash;
    int predict;        // The alternative, -1 needs more lookahead, -2 no alternative.
    bool conflict;      // Use the exact LL simulation.
    PFX_DState *ascii[128];
    PFX_Edge *other;
    PFX_DState *chain;
};

static PFX_DState **PFX_dstates;
static size_t PFX_dstates_cap, PFX_dstates_n;

static int PFX_config_cmp(const void *x, const void *y)
{
    const PFX_Config *a = (const PFX_Config*)x, *b = (const PFX_Config*)y;
    int k = PFX_key_cmp(a, b);
    if (k) return k;
    if (a->predicted != b->predicted) return a->predicted < b->predicted ? -1 : 1;
    if (a->cost != b->cost) return a->cost < b->cost ? -1 : 1;
    return 0;
}

// The resolved closure of the most recently used state, the configurations
// that move on. States only keep their seeds, the closure is recomputed (with
// the memoized closures this is cheap) when a new edge is needed.
static PFX_DState *PFX_cfg_state;
static PFX_Config *PFX_cfg;
static size_t PFX_cfg_n;

// Compute the closure of the seeds and resolve it into PFX_cfg, false on a conflict.
static bool PFX_resolve_seeds(const PFX_Config *seeds, size_t n)
{
    static PFX_Set set; // Reused, the memory is kept.
    PFX_set_clear(&set);
    PFX_closure(seeds, n, &set);
    free(PFX_cfg);
    PFX_cfg = NULL;
    PFX_cfg_n = 0;
    PFX_cfg_state = NULL;
    return PFX_resolve(set.v, set.n, true, &PFX_cfg, &PFX_cfg_n);
}

static void PFX_state_cfg(PFX_DState *d)
{
    if (PFX_cfg_state == d) return;
    PFX_resolve_seeds(d->key, d->nkey);
    PFX_cfg_state = d;
}

// The (cached) DFA state for the seeds in set (the set is not changed). The
// identity of a state is its set of seeds, hashed independently of the order
// and compared by membership. The state's prediction comes from the resolved
// closure, which is left in PFX_cfg.
static PFX_DState *PFX_dstate(int ri, PFX_Set *set)
{
    unsigned h = PFX_mix(13u, (unsigned)ri);
    unsigned sum = 0;
    for (size_t i = 0; i < set->n; i++) sum += PFX_config_hash(&set->v[i]);
    h = PFX_mix(h, sum);
    h = PFX_mix(h, (unsigned)set->n);
    if (!PFX_dstates)
    {
        PFX_dstates_cap = 1024;
        PFX_dstates = (PFX_DState**)PFX_xcalloc(PFX_dstates_cap, sizeof(PFX_DState*));
    }
    size_t b = h & (PFX_dstates_cap - 1);
    for (PFX_DState *d = PFX_dstates[b]; d; d = d->chain)
    {
        if (d->hash != h || d->rule != ri || d->nkey != set->n) continue;
        size_t i = 0;
        while (i < d->nkey && PFX_set_contains(set, &d->key[i])) i++;
        if (i == d->nkey) return d;
    }
    if (PFX_dstates_n * 2 > PFX_dstates_cap)
    {
        size_t ncap = PFX_dstates_cap * 2;
        PFX_DState **nt = (PFX_DState**)PFX_xcalloc(ncap, sizeof(PFX_DState*));
        for (size_t k = 0; k < PFX_dstates_cap; k++)
        {
            PFX_DState *d = PFX_dstates[k];
            while (d)
            {
                PFX_DState *nx = d->chain;
                size_t nb = d->hash & (ncap - 1);
                d->chain = nt[nb];
                nt[nb] = d;
                d = nx;
            }
        }
        free(PFX_dstates);
        PFX_dstates = nt;
        PFX_dstates_cap = ncap;
        b = h & (ncap - 1);
    }
    PFX_DState *d = (PFX_DState*)PFX_xcalloc(1, sizeof(PFX_DState));
    d->rule = ri;
    d->nkey = set->n;
    d->key = (PFX_Config*)PFX_xmalloc(sizeof(PFX_Config) * (set->n ? set->n : 1));
    memcpy(d->key, set->v, sizeof(PFX_Config) * set->n);
    d->hash = h;
    d->predict = -1;
    if (!PFX_resolve_seeds(d->key, d->nkey))
    {
        d->conflict = true;
    }
    else
    {
        PFX_cfg_state = d;
        int predicted = -1;
        bool unique = true;
        for (size_t i = 0; i < PFX_cfg_n; i++)
        {
            if (predicted == -1) predicted = PFX_cfg[i].predicted;
            else if (predicted != PFX_cfg[i].predicted) { unique = false; break; }
        }
        if (predicted == -1) d->predict = -2;
        else if (unique) d->predict = predicted;
    }
    d->chain = PFX_dstates[b];
    PFX_dstates[b] = d;
    PFX_dstates_n++;
    return d;
}

static PFX_DState *PFX_edge(PFX_DState *d, int sym)
{
    if (sym >= 0 && sym < 128) return d->ascii[sym];
    for (PFX_Edge *e = d->other; e; e = e->next) if (e->sym == sym) return e->to;
    return NULL;
}

static void PFX_add_edge(PFX_DState *d, int sym, PFX_DState *to)
{
    if (sym >= 0 && sym < 128) { d->ascii[sym] = to; return; }
    PFX_Edge *e = (PFX_Edge*)PFX_xmalloc(sizeof(PFX_Edge));
    e->sym = sym;
    e->to = to;
    e->next = d->other;
    d->other = e;
}

// The start states per decision and truncated context.
typedef struct PFX_Start { int rule; PFX_Stack *ctx; PFX_DState *d; struct PFX_Start *chain; } PFX_Start;
static PFX_Start *PFX_starts[4096];

static PFX_DState *PFX_start(int ri, PFX_Stack *tctx)
{
    size_t b = PFX_mix(PFX_ptr_hash(tctx), (unsigned)ri) & 4095;
    for (PFX_Start *s = PFX_starts[b]; s; s = s->chain) if (s->rule == ri && s->ctx == tctx) return s->d;
    int nalts = PFX_atn_alt0[ri+1] - PFX_atn_alt0[ri];
    PFX_Set set = {0};
    for (int a = 0; a < nalts; a++)
    {
        PFX_Config c = { ri, a, 0, a, 0, tctx, NULL };
        PFX_set_add(&set, &c);
    }
    PFX_Start *s = (PFX_Start*)PFX_xmalloc(sizeof(PFX_Start));
    s->rule = ri;
    s->ctx = tctx;
    s->d = PFX_dstate(ri, &set);
    PFX_set_free(&set);
    s->chain = PFX_starts[b];
    PFX_starts[b] = s;
    return s->d;
}

// The exact LL simulation with the full return stack.
static int PFX_ll(PFX_Parser *p, int ri)
{
    int nalts = PFX_atn_alt0[ri+1] - PFX_atn_alt0[ri];
    PFX_Config *seeds = (PFX_Config*)PFX_xmalloc(sizeof(PFX_Config) * (size_t)nalts);
    for (int a = 0; a < nalts; a++)
    {
        PFX_Config c = { ri, a, 0, a, 0, (PFX_Stack*)p->dyn_ctx, NULL };
        seeds[a] = c;
    }
    PFX_Set set = {0};
    PFX_closure(seeds, (size_t)nalts, &set);
    free(seeds);
    int result = -1;
    for (size_t i = p->pos; ; i++)
    {
        PFX_Config *cfg;
        size_t n;
        PFX_resolve(set.v, set.n, false, &cfg, &n);
        PFX_set_free(&set);
        int predicted = -1;
        bool unique = true;
        for (size_t k = 0; k < n; k++)
        {
            if (predicted == -1) predicted = cfg[k].predicted;
            else if (predicted != cfg[k].predicted) { unique = false; break; }
        }
        if (predicted == -1 || unique)
        {
            free(cfg);
            result = predicted;
            break;
        }
        if (i > p->n)
        {
            // All input consumed and still ambiguous, pick the first.
            int min = 0x7fffffff;
            for (size_t k = 0; k < n; k++) if (cfg[k].predicted < min) min = cfg[k].predicted;
            free(cfg);
            result = min;
            break;
        }
        PFX_move(cfg, n, PFX_peek(p, i), &set);
        free(cfg);
    }
    return result;
}

// Predict the alternative of the decision ri at the current position.
static int PFX_adaptive(PFX_Parser *p, int ri)
{
    PFX_DState *d = PFX_start(ri, PFX_truncate((PFX_Stack*)p->dyn_ctx, PFX_CTX_K));
    for (size_t i = p->pos; ; i++)
    {
        if (d->conflict) break;
        if (d->predict >= 0) return d->predict;
        if (d->predict == -2) return -1;
        if (i > p->n) break;
        int sym = PFX_peek(p, i);
        PFX_DState *next = PFX_edge(d, sym);
        if (!next)
        {
            static PFX_Set seeds; // Reused, the memory is kept.
            PFX_state_cfg(d);
            PFX_set_clear(&seeds);
            PFX_advance(PFX_cfg, PFX_cfg_n, sym, &seeds);
            next = PFX_dstate(ri, &seeds);
            PFX_add_edge(d, sym, next);
        }
        d = next;
    }
    return PFX_ll(p, ri);
}

""";
}
