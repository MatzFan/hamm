/* Metric tree sample implementation.

   This generates a bunch of pseudorandom 32-bit integers, inserts
   them into an index, and queries the index for points within a
   certain distance of the given point.

   That is,

   Let S = { N pseudorandom 32-bit integers }
   Let d(x,y) be the (base-2) Hamming distance between x and y
   Let q(x,r) = { y in S : d(x,y) <= r }

   There are three implementations in here which can be selected at runtime.

   "bk" is a BK-Tree.  Each internal node has a center point, and each
   child node contains a set of all points a certain distance away
   from the center.

   "vp" is a VP-Tree.  Each internal node has a center point and two
   children.  The "near" child contains all points contained in a
   closed ball of a certain radius around the center, and the "far"
   node contains all other points.

   "linear" is a linear search.

   The tree implementations use a linear search for leaf nodes.  The
   maximum number of points in a leaf node is configurable at runtime,
   but 1000 is a good number.  If the number is low, say 1, then the
   memory usage of the tree implementations will skyrocket to
   unreasonable levels: more than 24 bytes per element.

   Note that VP trees are slightly faster than BK trees for this
   problem, and neither tree implementation significantly outperforms
   linear search (that is, by a factor of two or more) for r > 6.  */

#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#ifndef DO_PRINT
#define DO_PRINT 1
#endif

#ifndef VERBOSE
#define VERBOSE 1
#endif

#ifndef HAMMING_DISTANCE
#define HAMMING_DISTANCE 2
#endif

__attribute__((malloc))
static void *
xmalloc(size_t sz)
{
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
xatoul(const char *p) // converts argv char * to lu
{
    char *e;
    unsigned long x;
    x = strtoul(p, &e, 0);
    return x;
}

typedef uint32_t bkey_t;

static char keybuf[33]; // one more than bitset size

static const char *keystr(bkey_t k) // used to print binary keystring for a key
{
    unsigned i;
    for (i = 0; i < 32; ++i) { // iterate all bits
        keybuf[31 - i] = '0' + (k & 1); // add 0 or 1 to buffer Little Endian
        k >>= 1;
    }
    keybuf[32] = '\0';
    return keybuf;
}

static const char *keystr2(bkey_t k, bkey_t ref)
{
    unsigned i;
    bkey_t d = ref ^ k; // xor - why????
    for (i = 0; i < 32; ++i) {
        keybuf[31 - i] = (d & 1) ? ('0' + (k & 1)) : '.';
        d >>= 1;
        k >>= 1;
    }
    keybuf[32] = '\0';
    return keybuf;
}

static unsigned num_nodes = 0; // declared in global scope for printing in main
static size_t tree_size = 0; // ditto

struct buf {
    bkey_t *keys;
    size_t n, a;
};

static void
addkey(struct buf *restrict b, bkey_t k)
{
    printf("Adding key\n");
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
}

/* Read keys from file ============== */

// static bkey_t * read_keys (const char* file_name, unsigned long nkeys) {

//     bkey_t *keys;
//     // unsigned long i;

//     // Empty bitset for all 2^32 possible keys to prevent duplicates.
//     size_t bn = 1 << (32 - 5);
//     uint32_t *bits = xmalloc(sizeof(uint32_t) * bn);
//     // for (i = 0; i < bn; ++i)
//     //     bits[i] = 0;

//     keys = malloc(sizeof(*keys) * nkeys);

//     FILE* file = fopen (file_name, "r");
//     // if (fscanf (file, "%lu", &j) == 0)
//     //     EXIT_FAILURE;
//     unsigned long j = 0;
//     char line[34]; // 32 digits = \0 + \nS
//     char *ptr;
//     while (fgets(line, sizeof(line), file) != NULL) { /* read a line */
//         line[strcspn(line, "\n")] = 0; // strips newline
//         bkey_t key = strtoul(line, &ptr, 2); // read leading num as base 2

//         if (!((bits[key >> 5] >> (key & 31)) & 1)) {
//             keys[j++] = key;
//             bits[key >> 5] |= 1 << (key & 31);
//         }
//         j++;
//     }
//     fclose (file);
//     putchar('\n');
//     printf("Keys[9]: %s\n", keystr(keys[9]));
//     return keys;
// }


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
    putchar('\n');
    free(bits);

    return keys;
}


/* Bitset search ==================== */

struct bitset {
    uint32_t *bits; // global scope 
};

static struct bitset *
mktree_bitset(const bkey_t *restrict keys, size_t num_keys)
{
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

static size_t
search_bitset(struct buf *restrict b, uint32_t *bits,
              bkey_t ref, unsigned maxd, bkey_t bit)
{
    size_t count = 1;
    if ((bits[ref >> 5] >> (ref & 31)) & 1) // bits is tree root
        addkey(b, ref); // b is buffer address - to record found keys???
    if (maxd == 0) // exit condition: no match found??
        return 1;
    while (bit) { // iterate through all 31 bits; from 31 to 0
        /* recurse, with ref with bit x set to 0 and bit incremented */
        search_bitset(b, bits, ref ^ bit, maxd - 1, bit >> 1);
        bit >>= 1;
    }
    return count; // was match found ,if got to here??????
}

static size_t
query_bitset(struct buf *restrict b, struct bitset *restrict root,
             bkey_t ref)
{
    return search_bitset(b, root->bits, ref, HAMMING_DISTANCE, 1 << 31); // 2**31
}

/* Main ==================== */

typedef void *(*mktree_t)(bkey_t *, size_t);
typedef size_t (*query_t)(struct buf *, void *, bkey_t);

int main() // int argc, char *argv[]
{
    uint32_t fps[10] = { 3926103320,
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
    // double tm, qc;
    // double tm;
    clock_t ckref, t;
    struct buf q = { 0, 0, 0 };
    unsigned long j; // dist, seconds was here
    unsigned long nkeys;
    void *root;
    bkey_t ref, *keys;
    unsigned long long total; // totalcmp;
    size_t nc;
    mktree_t mktree;
    query_t query;

    mktree = (mktree_t) mktree_bitset;
    query = (query_t) query_bitset;

    nkeys = sizeof(fps) / sizeof(fps[0]);
    keys = generate_keys(fps, nkeys); // **TO DO** PASS POINTER INSTEAD

    ckref = clock();
    root = mktree(keys, nkeys); // build data structure
    free(keys); // no longer needed
    t = clock();
    printf("Time: %.3f sec\n",
           (double)(t - ckref) / CLOCKS_PER_SEC);
    printf("Nodes: %u\n", num_nodes);
    printf("Tree size: %lu\n", tree_size);

    total = 0;
    ckref = clock();
    ref = 1992245614; // number to be compared

    q.n = 0;
    nc = query(&q, root, ref); // QUERY STRUCTURE

    printf("Count: %lu\n", nc);

    // totalcmp += nc;
    total += q.n;
    if (DO_PRINT) {
        printf("Ref is %s\n", keystr(ref));
        for (j = 0; j < q.n; ++j)
            printf("       %s\n", keystr2(q.keys[j], ref));
    }
    
    return 0;
}
