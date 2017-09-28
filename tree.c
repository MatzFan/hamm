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

#ifndef HAVE_POPCNT
#define HAVE_POPCNT 0
#endif

// static uint32_t rand_x0, rand_x1, rand_c;
// #define RAND_A 4284966893U

// void
// seedrand(void)
// {
//     time_t t;
//     time(&t);
//     rand_x0 = t;
//     fprintf(stderr, "seed: %u\n", rand_x0);
//     rand_x1 = 0x038acaf3U;
//     rand_c = 0xa2cc5886U;
// }

// uint32_t
// irand(void)
// {
//     uint64_t y = (uint64_t)rand_x0 * RAND_A + rand_c;
//     rand_x0 = rand_x1;
//     rand_x1 = y;
//     rand_c = y >> 32;
//     return y;
// }

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

static char keybuf[33];

static const char *
keystr(bkey_t k)
{
    unsigned i;
    for (i = 0; i < 32; ++i) {
        keybuf[31 - i] = '0' + (k & 1);
        k >>= 1;
    }
    keybuf[32] = '\0';
    return keybuf;
}

static const char *
keystr2(bkey_t k, bkey_t ref)
{
    unsigned i;
    bkey_t d = ref ^ k;
    for (i = 0; i < 32; ++i) {
        keybuf[31 - i] = (d & 1) ? ('0' + (k & 1)) : '.';
        d >>= 1;
        k >>= 1;
    }
    keybuf[32] = '\0';
    return keybuf;
}

static unsigned num_nodes = 0;
static size_t tree_size = 0;

struct buf {
    bkey_t *keys;
    size_t n, a;
};

static void
addkey(struct buf *restrict b, bkey_t k)
{
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

// static bkey_t *
// generate_keys(unsigned long nkeys) {
//     puts("Generating keys...");

//     bkey_t *keys;
//     unsigned long i;

//     // Empty bitset for all 2^32 possible keys to prevent duplicates.
//     size_t bn = 1 << (32 - 5);
//     uint32_t *bits = xmalloc(sizeof(uint32_t) * bn);
//     for (i = 0; i < bn; ++i)
//         bits[i] = 0;

//     // Create unique keys.
//     keys = malloc(sizeof(*keys) * nkeys);
//     for (i = 0; i < nkeys; ) {

//         bkey_t key = irand(); // irand() returns a uint32_t

//         if (!((bits[key >> 5] >> (key & 31)) & 1)) {
//             keys[i++] = key;
//             bits[key >> 5] |= 1 << (key & 31);
//         }
//     }

//     // Don't need the bitset anymore.
//     free(bits);

//     return keys;
// }

/* Read keys from file ============== */

static bkey_t * read_keys (const char* file_name, unsigned long nkeys) {

    bkey_t *keys;
    unsigned long i;

    // Empty bitset for all 2^32 possible keys to prevent duplicates.
    size_t bn = 1 << (32 - 5);
    uint32_t *bits = xmalloc(sizeof(uint32_t) * bn);
    for (i = 0; i < bn; ++i)
        bits[i] = 0;

    // printf("%lu\n", sizeof(*keys));
    // printf("%lu\n", nkeys);
    keys = malloc(sizeof(*keys) * nkeys);

    // printf("%s\n", file_name);

    unsigned long j = 0;
    FILE* file = fopen (file_name, "r");
    if (fscanf (file, "%lu", &j) == 0)
        EXIT_FAILURE;
    while (!feof (file)) {  
        bkey_t key = j;
        printf("%u\n", key);


        // if (!((bits[key >> 5] >> (key & 31)) & 1)) {
        //     keys[j++] = key;
        //     bits[key >> 5] |= 1 << (key & 31);
        // }

        if (fscanf (file, "%lu", &j) == 0)
            EXIT_FAILURE;
    }
    fclose (file);
    return keys;
}

/* Bitset search ==================== */

struct bitset {
    uint32_t *bits;
};

static struct bitset *
mktree_bitset(const bkey_t *restrict keys, size_t n)
{
    struct bitset* node;
    node = xmalloc(sizeof(*node));
    size_t bn = 1 << (32 - 5);
    node->bits = xmalloc(sizeof(uint32_t) * bn);
    num_nodes += 1;
    tree_size += (1 << (32 - 3)) + sizeof(*node);
    for (size_t i = 0; i < bn; i++)
        node->bits[i] = 0;
    for (size_t i = 0; i < n; i++) {
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
    if ((bits[ref >> 5] >> (ref & 31)) & 1)
        addkey(b, ref);
    if (maxd == 0)
        return 1;
    while (bit) {
        count += search_bitset(b, bits, ref ^ bit, maxd - 1, bit >> 1); // recurse
        bit >>= 1;
    }
    return count;
}

static size_t
query_bitset(struct buf *restrict b, struct bitset *restrict root,
             bkey_t ref, unsigned maxd)
{
    return search_bitset(b, root->bits, ref, maxd, 1 << 31);
}

/* Main ==================== */

typedef void *(*mktree_t)(bkey_t *, size_t);
typedef size_t (*query_t)(struct buf *, void *, bkey_t, unsigned);

int main(int argc, char *argv[])
{
    // double tm, qc;
    // double tm;
    clock_t ckref, t;
    struct buf q = { 0, 0, 0 };
    unsigned long nkeys, dist, j; // seconds was here
    void *root;
    bkey_t ref, *keys;
    unsigned long long total, totalcmp;
    size_t nc;
    mktree_t mktree;

    query_t query;

    if (argc != 2) {
        fputs("Usage: DIST\n", stderr);
        return 1;
    }

    mktree = (mktree_t) mktree_bitset;
    query = (query_t) query_bitset;
    
    nkeys = xatoul(argv[1]);
    // seconds = xatoul(argv[3]);
    
    // seedrand();
    printf("Keys: %lu\n", nkeys);
    // printf("Seconds (at least): %lu\n", seconds);
    putchar('\n');

    // keys = generate_keys(nkeys);

    // keys = NULL;

    keys = read_keys("/home/me/dev/metric-tree-demo/fingerprints", 10); // 2nd arg is nkeys
    
    ckref = clock();
    root = mktree(keys, nkeys); // root is the tree structure
    free(keys);
    t = clock();
    printf("Time: %.3f sec\n",
           (double)(t - ckref) / CLOCKS_PER_SEC);
    printf("Nodes: %u\n", num_nodes);
    printf("Tree size: %lu\n", tree_size);

    total = 0;
    totalcmp = 0;
    dist = 2; // hamm dist
   
    // if (VERBOSE) {
    //     putchar('\n');
    //     printf("Distance: %lu\n", dist);
    // }

    // nquery = 0;
    ckref = clock();
    // while (nquery < 3 || (tm = clock() - ckref) / CLOCKS_PER_SEC < seconds) {
    //     for (i = nquery + 1; i > 0; --i) {
            // ref = irand();

    ref = 2598365869; // number to be compared

    q.n = 0;
    nc = query(&q, root, ref, dist);

    printf("%lu\n", nc); // 529 ?????

    totalcmp += nc;
    total += q.n;
    if (DO_PRINT) {
        printf("Query: %s\n", keystr(ref));
        for (j = 0; j < q.n; ++j)
            printf("       %s\n", keystr2(q.keys[j], ref));
    }
    //         ++nquery;
    //     }
    // }

    // qc = (double) CLOCKS_PER_SEC * (double) nquery;
    // if (VERBOSE) {
    //     printf("Rate: %f query/sec\n", qc / tm);
    //     printf("Time: %f msec/query\n", 1000.0 * tm / qc);
    //     printf("Queries: %lu\n", nquery);
    //     printf("Hits: %f\n", total / (double)nquery);
    //     printf("Coverage: %f%%\n",
    //            100.0 * (double)totalcmp / ((double)nkeys * nquery));
    //     printf("Cmp/result: %f\n", (double)totalcmp / (double)total);
    // } else {
    //     printf("%2lu %10.2f %10lu\n", dist, qc / tm, nquery);
    // }
    return 0;
}
