/* Hamming distance bitset search, adapted from:
https://github.com/pochmann/metric-tree-demo/blob/master/tree.c
original author:
https://github.com/depp/metric-tree-demo*/

#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#ifndef DO_PRINT
#define DO_PRINT 0
#endif

#ifndef VERBOSE
#define VERBOSE 1
#endif

#ifndef HAMMING_DISTANCE
#define HAMMING_DISTANCE 2
#endif

__attribute__((malloc))
static void *
xmalloc(size_t sz) {
    void *p;
    if (!sz)
        return NULL;
    p = malloc(sz);
    if (!p) {
        printf("error: malloc\n");
        exit(1);
    }
    return p;
}

unsigned long
xatoul(const char *p) {
    char *e;
    unsigned long x;
    x = strtoul(p, &e, 0);
    return x;
}

typedef uint32_t bkey_t;

static unsigned num_nodes = 0; // declared in global scope for printing in main
static size_t tree_size = 0; // ditto

struct buf {
    bkey_t *keys; // pointer to an uint32_t
    size_t n, a;
};

static bkey_t
addkey(struct buf *restrict b, bkey_t k) {
    size_t na;
    bkey_t *np;
    if (b->n >= b->a) {
        na = b->a ? 2*b->a : 16;
        np = xmalloc(sizeof(*np) * na);
        memcpy(np, b->keys, sizeof(*np) * b->n);
        free(b->keys);
        b->keys = np;
        b->a = na;
    }
    b->keys[b->n++] = k;
    return k;
}

static bkey_t *
generate_keys(uint32_t fps[], unsigned long nkeys) {
    bkey_t *keys;
    unsigned long i;
    // Empty bitset for all 2^32 possible keys to prevent duplicates.
    size_t bn = 1 << (32 - 5);
    uint32_t *bits = xmalloc(sizeof(uint32_t) * bn);
    for (i = 0; i < bn; ++i)
        bits[i] = 0;

    // Create unique keys.
    keys = malloc(sizeof(*keys) * nkeys);
    for (i = 0; i < nkeys; ) {
        bkey_t key = fps[i];
        if (!((bits[key >> 5] >> (key & 31)) & 1)) {
            keys[i++] = key;
            // printf("%s\n", keystr(key));
            bits[key >> 5] |= 1 << (key & 31);
        }
    }
    free(bits);
    return keys;
}

struct bitset {
    uint32_t *bits; // global scope 
};

static struct bitset *
mktree_bitset(const bkey_t *restrict keys, size_t num_keys) {
    struct bitset* node; // each node is 32 bit bitset
    node = xmalloc(sizeof(*node));
    size_t bn = 1 << (32 - 5); // 2**27 = 134217728
    node->bits = xmalloc(sizeof(uint32_t) * bn);
    num_nodes += 1;
    tree_size += (1 << (32 - 3)) + sizeof(*node); // 2**29 +8 => 536870920
    for (size_t i = 0; i < bn; i++)
        node->bits[i] = 0;
    for (size_t i = 0; i < num_keys; i++) {
        bkey_t key = keys[i];
        node->bits[key >> 5] |= 1 << (key & 31);
    }
    return node;
}

static bkey_t
search_bitset(struct buf *restrict b, uint32_t *bits,
              bkey_t ref, unsigned maxd, bkey_t bit,
              bkey_t found) { // pass 0 here as initial value of found
    if ((bits[ref >> 5] >> (ref & 31)) & 1)
        found = addkey(b, ref);
    if (maxd == 0)
        return found;
    while (bit) { // iterate through all 31 bits; from 31 to 0
        found = search_bitset(b, bits, ref ^ bit, maxd - 1, bit >> 1, found);
        bit >>= 1;
    }
    return found; // was 1. No similar key found 
}

static bkey_t
query_bitset(struct buf *restrict b, struct bitset *restrict root,
             bkey_t ref) {
    return search_bitset(b, root->bits, ref, HAMMING_DISTANCE, 1 << 31, 0);
}


/* Main ==================== */

typedef void *(*mktree_t)(bkey_t *, size_t);
typedef size_t (*query_t)(struct buf *, void *, bkey_t);

int main(int argc, char *argv[]) {
    struct buf q = { 0, 0, 0 };
    unsigned long nkeys;
    void *root;
    bkey_t ref, *keys;

    mktree_t mktree;
    query_t query;
    mktree = (mktree_t) mktree_bitset;
    query = (query_t) query_bitset;

    if (argc != 2) {
        printf("A uint32_t argument must be supplied");
        return 1;
    }

    ref = xatoul(argv[1]);
     
    uint32_t fps[] = { 3926103320,
                       4283886574,
                       2780175709,
                       3284479930,
                       1923677470,
                       1596497511,
                       629345177,
                       2432890560,
                       696849934,
                       1992245486
                      };

    nkeys = sizeof(fps) / sizeof(fps[0]);
    keys = generate_keys(fps, nkeys); // **TO DO** PASS POINTER INSTEAD
    root = mktree(keys, nkeys); // build data structure
    free(keys); // no longer needed
    q.n = 0;
    bkey_t similar = query(&q, root, ref); // QUERY STRUCTURE
    
    if (similar != 0)
        printf("%u\n", similar);
    return 0;
}
