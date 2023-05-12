// Area optimization using decision diagrams without constructing them

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <regex>
#define MAX_VARS 16  // the largest allowed number of inputs
#define MAX_SIZE 256 // the number of initially allocated objects

/*************************************************************
                     Various helpers
**************************************************************/

// operations on variables and literals
static inline int  kc_v2l( int Var, int c )    { assert(Var >= 0 && !(c >> 1)); return Var + Var + c; }
static inline int  kc_l2v( int Lit )           { assert(Lit >= 0); return Lit >> 1;                   }
static inline int  kc_l2c( int Lit )           { assert(Lit >= 0); return Lit & 1;                    }
static inline int  kc_lnot( int Lit )          { assert(Lit >= 0); return Lit ^ 1;                    }
static inline int  kc_lnotc( int Lit, int c )  { assert(Lit >= 0); return Lit ^ (int)(c > 0);         }
static inline int  kc_lreg( int Lit )          { assert(Lit >= 0); return Lit & ~01;                  }

// min/max/abs
static inline int  kc_abs( int a        )      { return a < 0 ? -a : a;                               }
static inline int  kc_max( int a, int b )      { return a > b ?  a : b;                               }
static inline int  kc_min( int a, int b )      { return a < b ?  a : b;                               }

// truth table size in 64-bit words
static inline int  kc_truth_word_num( int n )  { return n <= 6 ? 1 : 1 << (n-6);                      }

// swapping two variables
#define KC_SWAP(Type, a, b)  { Type t = a; a = b; b = t; }

/*************************************************************
                 Vector of 32-bit integers
**************************************************************/

typedef struct kc_vi_
{
    int size;
    int cap;
    int *ptr;
} kc_vi;

// iterator through the entries in the vector
#define kc_vi_for_each_entry(v, entry, i) \
    for (i = 0; (i < (v)->size) && (((entry) = kc_vi_read((v), i)), 1); i++)

static inline void kc_vi_start(kc_vi *v, int cap)
{
    v->size = 0;
    v->cap = cap;
    v->ptr = (int *)malloc(sizeof(int) * v->cap);
}
static inline void kc_vi_stop(kc_vi *v) { free(v->ptr); }
static inline int *kc_vi_array(kc_vi *v) { return v->ptr; }
static inline int kc_vi_read(kc_vi *v, int k)
{
    assert(k < v->size);
    return v->ptr[k];
}
static inline void kc_vi_write(kc_vi *v, int k, int e)
{
    assert(k < v->size);
    v->ptr[k] = e;
}
static inline int kc_vi_size(kc_vi *v) { return v->size; }
static inline void kc_vi_resize(kc_vi *v, int k)
{
    assert(k <= v->size);
    v->size = k;
} // only safe to shrink !!
static inline int kc_vi_pop(kc_vi *v)
{
    assert(v->size > 0);
    return v->ptr[--v->size];
}
static inline void kc_vi_grow(kc_vi *v)
{
    if (v->size < v->cap)
        return;
    int newcap = (v->cap < 4) ? 8 : (v->cap / 2) * 3;
    v->ptr = (int *)realloc(v->ptr, sizeof(int) * newcap);
    if (v->ptr == NULL)
    {
        printf("Failed to realloc memory from %.1f MB to %.1f MB.\n", 4.0 * v->cap / (1 << 20), 4.0 * newcap / (1 << 20));
        fflush(stdout);
    }
    v->cap = newcap;
}
static inline void kc_vi_push(kc_vi *v, int e)
{
    kc_vi_grow(v);
    v->ptr[v->size++] = e;
}
static inline void kc_vi_fill(kc_vi *v, int n, int fill)
{
    int i;
    for (i = 0; i < n; i++)
        kc_vi_push(v, fill);
}
static inline int kc_vi_remove(kc_vi *v, int e)
{
    int j;
    for (j = 0; j < v->size; j++)
        if (v->ptr[j] == e)
            break;
    if (j == v->size)
        return 0;
    for (; j < v->size - 1; j++)
        v->ptr[j] = v->ptr[j + 1];
    kc_vi_resize(v, v->size - 1);
    return 1;
}
static inline int kc_vi_print(kc_vi *v)
{
    printf("Array with %d entries:", v->size);
    int i, entry;
    kc_vi_for_each_entry(v, entry, i)
        printf(" %d", entry);
    printf("\n");
}

/*************************************************************
                 Vector of truth tables
**************************************************************/

#ifdef _WIN32
typedef unsigned __int64 kc_uint64; // 32-bit windows
#else
typedef long long unsigned kc_uint64; // other platforms
#endif

typedef struct kc_vt_
{
    int size;
    int cap;
    int words;
    kc_uint64 *ptr;
} kc_vt;

static inline kc_uint64 *kc_vt_array(kc_vt *v) { return v->ptr; }
static inline int kc_vt_words(kc_vt *v) { return v->words; }
static inline int kc_vt_size(kc_vt *v) { return v->size; }
static inline void kc_vt_resize(kc_vt *v, int ttNum)
{
    assert(ttNum <= v->size);
    v->size = ttNum;
} // only safe to shrink !!
static inline void kc_vt_shrink(kc_vt *v, int num)
{
    assert(num <= v->size);
    v->size -= num;
}
static inline kc_uint64 *kc_vt_read(kc_vt *v, int ttId)
{
    assert(ttId < v->size);
    return v->ptr + ttId * v->words;
}
static inline void kc_vt_grow(kc_vt *v)
{
    if (v->size == v->cap)
    {
        int newcap = (v->cap < 4) ? 8 : (v->cap / 2) * 3;
        v->ptr = (kc_uint64 *)realloc(v->ptr, 8 * newcap * v->words);
        if (v->ptr == NULL)
        {
            printf("Failed to realloc memory from %.1f MB to %.1f MB.\n", 4.0 * v->cap * v->words / (1 << 20), 4.0 * newcap * v->words / (1 << 20));
            fflush(stdout);
        }
        v->cap = newcap;
    }
}
static inline kc_uint64 *kc_vt_append(kc_vt *v)
{
    kc_vt_grow(v);
    return v->ptr + v->size++ * v->words;
}
static inline void kc_vt_move(kc_vt *v, kc_vt *v2, int ttId)
{
    assert(v->words == v2->words);
    memmove(kc_vt_append(v), kc_vt_read(v2, ttId), 8 * v->words);
}

/*************************************************************
               Initialization of truth tables
**************************************************************/

static kc_uint64 s_Truths6[6] = {
    0xAAAAAAAAAAAAAAAA,
    0xCCCCCCCCCCCCCCCC,
    0xF0F0F0F0F0F0F0F0,
    0xFF00FF00FF00FF00,
    0xFFFF0000FFFF0000,
    0xFFFFFFFF00000000};
static kc_uint64 s_Truths6Neg[6] = {
    0x5555555555555555,
    0x3333333333333333,
    0x0F0F0F0F0F0F0F0F,
    0x00FF00FF00FF00FF,
    0x0000FFFF0000FFFF,
    0x00000000FFFFFFFF};

static inline void kc_vt_start(kc_vt *v, int cap, int words)
{
    v->size = 0;
    v->cap = cap;
    v->words = words;
    v->ptr = (kc_uint64 *)malloc(8 * v->cap * v->words);
}
static inline void kc_vt_start_truth(kc_vt *v, int nvars)
{
    int i, k;
    v->words = kc_truth_word_num(nvars);
    v->size = 2 * (nvars + 1);
    v->cap = 6 * (nvars + 1);
    v->ptr = (kc_uint64 *)malloc(8 * v->words * v->cap);
    memset(v->ptr, 0, 8 * v->words);
    memset(v->ptr + v->words, 0xFF, 8 * v->words);
    for (i = 0; i < 2 * nvars; i++)
    {
        kc_uint64 *tt = v->ptr + (i + 2) * v->words;
        if (i / 2 < 6)
            for (k = 0; k < v->words; k++)
                tt[k] = s_Truths6[i / 2];
        else
            for (k = 0; k < v->words; k++)
                tt[k] = (k & (1 << (i / 2 - 6))) ? ~(kc_uint64)0 : 0;
        if (i & 1)
            for (k = 0; k < v->words; k++)
                tt[k] = ~tt[k];
        // printf( "lit = %2d  ", i+2 ); kc_vt_print(v, i+2);
    }
}
static inline void kc_vt_stop(kc_vt *v) { free(v->ptr); }
static inline void kc_vt_dup(kc_vt *vNew, kc_vt *v)
{
    kc_vt_start(vNew, v->cap, v->words);
    memmove(kc_vt_array(vNew), kc_vt_array(v), 8 * v->size * v->words);
    vNew->size = v->size;
}

/*************************************************************
             Boolean operations on truth tables
**************************************************************/

static inline int kc_vt_and(kc_vt *v, int ttA, int ttB)
{
    kc_uint64 *pF = kc_vt_append(v);
    int i;
    kc_uint64 *pA = kc_vt_read(v, ttA);
    kc_uint64 *pB = kc_vt_read(v, ttB);
    for (i = 0; i < v->words; i++)
        pF[i] = pA[i] & pB[i];
    return v->size - 1;
}
static inline int kc_vt_xor(kc_vt *v, int ttA, int ttB)
{
    kc_uint64 *pF = kc_vt_append(v);
    int i;
    kc_uint64 *pA = kc_vt_read(v, ttA);
    kc_uint64 *pB = kc_vt_read(v, ttB);
    for (i = 0; i < v->words; i++)
        pF[i] = pA[i] ^ pB[i];
    return v->size - 1;
}
static inline int kc_vt_inv(kc_vt *v, int ttA)
{
    kc_uint64 *pF = kc_vt_append(v);
    int i;
    kc_uint64 *pA = kc_vt_read(v, ttA);
    for (i = 0; i < v->words; i++)
        pF[i] = ~pA[i];
    return v->size - 1;
}
static inline int kc_vt_is_equal(kc_vt *v, int ttA, int ttB)
{
    kc_uint64 *pA = kc_vt_read(v, ttA);
    kc_uint64 *pB = kc_vt_read(v, ttB);
    int i;
    for (i = 0; i < v->words; i++)
        if (pA[i] != pB[i])
            return 0;
    return 1;
}
static inline int kc_vt_is_equal2(kc_vt *vA, int ttA, kc_vt *vB, int ttB)
{
    kc_uint64 *pA = kc_vt_read(vA, ttA);
    kc_uint64 *pB = kc_vt_read(vB, ttB);
    int i;
    assert(vA->words == vB->words);
    for (i = 0; i < vA->words; i++)
        if (pA[i] != pB[i])
            return 0;
    return 1;
}
static inline int kc_vt_is_const0(kc_vt *v, int ttA)
{
    kc_uint64 *pA = kc_vt_read(v, ttA);
    int i;
    for (i = 0; i < v->words; i++)
        if (pA[i])
            return 0;
    return 1;
}
static inline int kc_vt_is_const1(kc_vt *v, int ttA)
{
    kc_uint64 *pA = kc_vt_read(v, ttA);
    int i;
    for (i = 0; i < v->words; i++)
        if (~pA[i])
            return 0;
    return 1;
}
static inline int kc_vt_has_var(kc_vt *v, int ttId, int iVar)
{
    kc_uint64 *tt = kc_vt_read(v, ttId);
    if (iVar < 6)
    {
        int i, Shift = (1 << iVar);
        for (i = 0; i < v->words; i++)
            if (((tt[i] >> Shift) & s_Truths6Neg[iVar]) != (tt[i] & s_Truths6Neg[iVar]))
                return 1;
        return 0;
    }
    else
    {
        int i, Step = (1 << (iVar - 6));
        kc_uint64 *tLimit = tt + v->words;
        for (; tt < tLimit; tt += 2 * Step)
            for (i = 0; i < Step; i++)
                if (tt[i] != tt[Step + i])
                    return 1;
        return 0;
    }
}
static inline int kc_vt_cof0(kc_vt *v, int ttId, int iVar)
{
    kc_uint64 *ttNew = kc_vt_append(v);
    kc_uint64 *tt = kc_vt_read(v, ttId);
    assert(iVar >= 0);
    if (iVar <= 5)
    {
        int w, shift = (1 << iVar);
        for (w = 0; w < v->words; w++)
            ttNew[w] = ((tt[w] & s_Truths6Neg[iVar]) << shift) | (tt[w] & s_Truths6Neg[iVar]);
    }
    else // if ( iVar > 5 )
    {
        kc_uint64 *pLimit = tt + v->words;
        int i, iStep = kc_truth_word_num(iVar);
        for (; tt < pLimit; tt += 2 * iStep, ttNew += 2 * iStep)
            for (i = 0; i < iStep; i++)
            {
                ttNew[i] = tt[i];
                ttNew[i + iStep] = tt[i];
            }
    }
    return v->size - 1;
}
static inline int kc_vt_cof1(kc_vt *v, int ttId, int iVar)
{
    kc_uint64 *ttNew = kc_vt_append(v);
    kc_uint64 *tt = kc_vt_read(v, ttId);
    assert(iVar >= 0);
    if (iVar <= 5)
    {
        int w, shift = (1 << iVar);
        for (w = 0; w < v->words; w++)
            ttNew[w] = (tt[w] & s_Truths6[iVar]) | ((tt[w] & s_Truths6[iVar]) >> shift);
    }
    else // if ( iVar > 5 )
    {
        kc_uint64 *pLimit = tt + v->words;
        int i, iStep = kc_truth_word_num(iVar);
        for (; tt < pLimit; tt += 2 * iStep, ttNew += 2 * iStep)
            for (i = 0; i < iStep; i++)
            {
                ttNew[i] = tt[i + iStep];
                ttNew[i + iStep] = tt[i + iStep];
            }
    }
    return v->size - 1;
}

/*************************************************************
             Swapping variables in truth tables
**************************************************************/

static kc_uint64 s_PPMasks[5][6][3] = {
    {
        {0x0000000000000000, 0x0000000000000000, 0x0000000000000000}, // 0 0
        {0x9999999999999999, 0x2222222222222222, 0x4444444444444444}, // 0 1
        {0xA5A5A5A5A5A5A5A5, 0x0A0A0A0A0A0A0A0A, 0x5050505050505050}, // 0 2
        {0xAA55AA55AA55AA55, 0x00AA00AA00AA00AA, 0x5500550055005500}, // 0 3
        {0xAAAA5555AAAA5555, 0x0000AAAA0000AAAA, 0x5555000055550000}, // 0 4
        {0xAAAAAAAA55555555, 0x00000000AAAAAAAA, 0x5555555500000000}  // 0 5
    },
    {
        {0x0000000000000000, 0x0000000000000000, 0x0000000000000000}, // 1 0
        {0x0000000000000000, 0x0000000000000000, 0x0000000000000000}, // 1 1
        {0xC3C3C3C3C3C3C3C3, 0x0C0C0C0C0C0C0C0C, 0x3030303030303030}, // 1 2
        {0xCC33CC33CC33CC33, 0x00CC00CC00CC00CC, 0x3300330033003300}, // 1 3
        {0xCCCC3333CCCC3333, 0x0000CCCC0000CCCC, 0x3333000033330000}, // 1 4
        {0xCCCCCCCC33333333, 0x00000000CCCCCCCC, 0x3333333300000000}  // 1 5
    },
    {
        {0x0000000000000000, 0x0000000000000000, 0x0000000000000000}, // 2 0
        {0x0000000000000000, 0x0000000000000000, 0x0000000000000000}, // 2 1
        {0x0000000000000000, 0x0000000000000000, 0x0000000000000000}, // 2 2
        {0xF00FF00FF00FF00F, 0x00F000F000F000F0, 0x0F000F000F000F00}, // 2 3
        {0xF0F00F0FF0F00F0F, 0x0000F0F00000F0F0, 0x0F0F00000F0F0000}, // 2 4
        {0xF0F0F0F00F0F0F0F, 0x00000000F0F0F0F0, 0x0F0F0F0F00000000}  // 2 5
    },
    {
        {0x0000000000000000, 0x0000000000000000, 0x0000000000000000}, // 3 0
        {0x0000000000000000, 0x0000000000000000, 0x0000000000000000}, // 3 1
        {0x0000000000000000, 0x0000000000000000, 0x0000000000000000}, // 3 2
        {0x0000000000000000, 0x0000000000000000, 0x0000000000000000}, // 3 3
        {0xFF0000FFFF0000FF, 0x0000FF000000FF00, 0x00FF000000FF0000}, // 3 4
        {0xFF00FF0000FF00FF, 0x00000000FF00FF00, 0x00FF00FF00000000}  // 3 5
    },
    {
        {0x0000000000000000, 0x0000000000000000, 0x0000000000000000}, // 4 0
        {0x0000000000000000, 0x0000000000000000, 0x0000000000000000}, // 4 1
        {0x0000000000000000, 0x0000000000000000, 0x0000000000000000}, // 4 2
        {0x0000000000000000, 0x0000000000000000, 0x0000000000000000}, // 4 3
        {0x0000000000000000, 0x0000000000000000, 0x0000000000000000}, // 4 4
        {0xFFFF00000000FFFF, 0x00000000FFFF0000, 0x0000FFFF00000000}  // 4 5
    }};

static inline void kc_vt_swap_vars(kc_vt *v, int ttId, int iVar, int jVar)
{
    if (iVar == jVar)
        return;
    if (jVar < iVar)
        KC_SWAP(int, iVar, jVar)
    assert(kc_truth_word_num(iVar + 1) <= v->words);
    assert(kc_truth_word_num(jVar + 1) <= v->words);
    kc_uint64 *tt = kc_vt_read(v, ttId);
    if (jVar <= 5)
    {
        kc_uint64 *s_PMasks = s_PPMasks[iVar][jVar];
        int w, shift = (1 << jVar) - (1 << iVar);
        for (w = 0; w < v->words; w++)
            tt[w] = (tt[w] & s_PMasks[0]) | ((tt[w] & s_PMasks[1]) << shift) | ((tt[w] & s_PMasks[2]) >> shift);
    }
    else if (iVar <= 5 && jVar > 5)
    {
        kc_uint64 low2High, high2Low;
        kc_uint64 *pLimit = tt + v->words;
        int j, jStep = kc_truth_word_num(jVar);
        int shift = 1 << iVar;
        for (; tt < pLimit; tt += 2 * jStep)
            for (j = 0; j < jStep; j++)
            {
                low2High = (tt[j] & s_Truths6[iVar]) >> shift;
                high2Low = (tt[j + jStep] << shift) & s_Truths6[iVar];
                tt[j] = (tt[j] & ~s_Truths6[iVar]) | high2Low;
                tt[j + jStep] = (tt[j + jStep] & s_Truths6[iVar]) | low2High;
            }
    }
    else
    {
        kc_uint64 *pLimit = tt + v->words;
        int i, iStep = kc_truth_word_num(iVar);
        int j, jStep = kc_truth_word_num(jVar);
        for (; tt < pLimit; tt += 2 * jStep)
            for (i = 0; i < jStep; i += 2 * iStep)
                for (j = 0; j < iStep; j++)
                    KC_SWAP(kc_uint64, tt[iStep + i + j], tt[jStep + i + j])
    }
}

/*************************************************************
                 Printing truth tables
**************************************************************/

// printing in hexadecimal
static inline void kc_vt_print_int(kc_vt *v, int ttA)
{
    kc_uint64 *pA = kc_vt_read(v, ttA);
    int k, Digit, nDigits = v->words * 16;
    for (k = nDigits - 1; k >= 0; k--)
    {
        Digit = (int)((pA[k / 16] >> ((k % 16) * 4)) & 15);
        if (Digit < 10)
            printf("%d", Digit);
        else
            printf("%c", 'A' + Digit - 10);
    }
}
static inline void kc_vt_print(kc_vt *v, int ttA)
{
    kc_vt_print_int(v, ttA);
    printf("\n");
}
static inline void kc_vt_print_all(kc_vt *v)
{
    int i;
    printf("The array contains %d truth tables of size %d words:\n", v->size, v->words);
    for (i = 0; i < v->size; i++)
    {
        printf("%2d : ", i);
        kc_vt_print(v, i);
    }
}

// printing in binary
static inline void kc_vt_print2_int(kc_vt *v, int ttA)
{
    kc_uint64 *pA = kc_vt_read(v, ttA);
    int k;
    for (k = v->words * 64 - 1; k >= 0; k--)
        printf("%c", '0' + (int)((pA[k / 64] >> (k % 64)) & 1));
}
static inline void kc_vt_print2(kc_vt *v, int ttA)
{
    kc_vt_print2_int(v, ttA);
    printf("\n");
}
static inline void kc_vt_print2_all(kc_vt *v)
{
    int i;
    printf("The array contains %d truth tables of size %d words:\n", v->size, v->words);
    for (i = 0; i < v->size; i++)
    {
        printf("%2d : ", i);
        kc_vt_print2(v, i);
    }
}

/*************************************************************
                 Gate graph data structure
**************************************************************/

typedef struct kc_gg_
{
    int nins;    // the number of primary inputs
    int size;    // the number of objects, including const0, primary inputs, and internal nodes if any
    int cap;     // the number of objects allocated
    int tid;     // the current traversal ID
    kc_vi tids;  // the last visited tranversal ID of each object
    kc_vi fans;  // the fanins of objects
    kc_vi tops;  // the output literals
    kc_vt funcs; // the truth tables used for temporary cofactoring
    kc_vt tts;   // the truth tables of each literal (pos and neg polarity of each object)
    kc_vt outs;  // the primary output function(s) given by the user
} kc_gg;

// reading fanins
static inline int kc_gg_fanin(kc_gg *p, int v, int n)
{
    assert(n == 0 || n == 1);
    return kc_vi_read(&p->fans, 2 * v + n);
}
static inline int kc_gg_is_xor(kc_gg *p, int v) { return kc_gg_fanin(p, v, 0) > kc_gg_fanin(p, v, 1); }
static inline int kc_gg_is_node(kc_gg *p, int v) { return v >= 1 + p->nins; }
static inline int kc_gg_is_pi(kc_gg *p, int v) { return v >= 1 && v <= p->nins; }
static inline int kc_gg_is_const0(kc_gg *p, int v) { return v == 0; }
static inline int kc_gg_pi_num(kc_gg *p) { return p->nins; }
static inline int kc_gg_po_num(kc_gg *p) { return kc_vt_size(&p->outs); }
static inline int kc_gg_node_num(kc_gg *p) { return p->size - 1 - p->nins; }

// managing traversal IDs
static inline int kc_gg_tid_increment(kc_gg *p)
{
    assert(p->tid < 0x7FFFFFFF);
    p->tid++;
    return p->tid;
}
static inline int kc_gg_tid_is_cur(kc_gg *p, int v) { return kc_vi_read(&p->tids, v) == p->tid; }
static inline int kc_gg_tid_set_cur(kc_gg *p, int v)
{
    kc_vi_write(&p->tids, v, p->tid);
    return 1;
}
static inline int kc_gg_tid_update(kc_gg *p, int v)
{
    if (kc_gg_tid_is_cur(p, v))
        return 0;
    return kc_gg_tid_set_cur(p, v);
}

// constructor and destructor
static inline kc_gg *kc_gg_start(int nins, kc_vt *outs)
{
    kc_gg *gg = (kc_gg *)malloc(sizeof(kc_gg));
    gg->nins = nins;
    gg->size = 1 + nins;
    gg->cap = MAX_SIZE;
    gg->tid = 1;
    kc_vi_start(&gg->tids, 2 * gg->cap);
    kc_vi_fill(&gg->tids, gg->size, 0);
    kc_vi_start(&gg->fans, 2 * gg->cap);
    kc_vi_fill(&gg->fans, 2 * gg->size, -1);
    kc_vi_start(&gg->tops, outs->size);
    kc_vt_start(&gg->funcs, 3 * gg->size, kc_truth_word_num(nins));
    kc_vt_start_truth(&gg->tts, nins);
    kc_vt_dup(&gg->outs, outs);
    return gg;
}
static inline void kc_gg_stop(kc_gg *gg)
{
    if (gg == NULL)
        return;
    kc_vt_stop(&gg->outs);
    kc_vt_stop(&gg->funcs);
    kc_vt_stop(&gg->tts);
    kc_vi_stop(&gg->tids);
    kc_vi_stop(&gg->fans);
    kc_vi_stop(&gg->tops);
    free(gg);
}

// managing internal nodes
static inline int kc_gg_hash_node(kc_gg *gg, int lit1, int lit2, int ttId)
{
    int i;
    // compare nodes (structural hashing)
    for (i = 1 + gg->nins; i < gg->size; i++)
        if (kc_gg_fanin(gg, i, 0) == lit1 && kc_gg_fanin(gg, i, 1) == lit2)
            return kc_v2l(i, 0);
    // compare functions (functional hashing)
    for (i = 0; i < 2 * gg->size; i++)
        if (kc_vt_is_equal(&gg->tts, ttId, i))
            return i;
    return -1;
}
static inline int kc_gg_append_node(kc_gg *gg, int lit1, int lit2, int ttId)
{
    gg->size++;
    kc_vi_push(&gg->fans, lit1);
    kc_vi_push(&gg->fans, lit2);
    kc_vi_push(&gg->tids, 0);
    kc_vt_inv(&gg->tts, ttId);
    assert(gg->tts.size == 2 * gg->size); // one truth table for each literal
    return kc_v2l(gg->size - 1, 0);
}

// managing internal functions
static inline int kc_gg_hash_function(kc_gg *gg, int ttId)
{
    int i;
    for (i = 0; i < 2 * gg->size; i++)
        if (kc_vt_is_equal2(&gg->tts, i, &gg->funcs, ttId))
            return i;
    return -1;
}

// Boolean operations
static inline int kc_gg_and(kc_gg *gg, int lit1, int lit2)
{
    if (lit1 == 0)
        return 0;
    if (lit2 == 0)
        return 0;
    if (lit1 == 1)
        return lit2;
    if (lit2 == 1)
        return lit1;
    if (lit1 == lit2)
        return lit1;
    if ((lit1 ^ lit2) == 1)
        return 0;
    if (lit1 > lit2)
        KC_SWAP(int, lit1, lit2)
    assert(lit1 < lit2);
    int ttId = kc_vt_and(&gg->tts, lit1, lit2);
    int lit = kc_gg_hash_node(gg, lit1, lit2, ttId);
    if (lit == -1)
        return kc_gg_append_node(gg, lit1, lit2, ttId);
    kc_vt_resize(&gg->tts, ttId);
    return lit;
}
static inline int kc_gg_xor(kc_gg *gg, int lit1, int lit2)
{
    if (lit1 == 1)
        return (lit2 ^ 1);
    if (lit2 == 1)
        return (lit1 ^ 1);
    if (lit1 == 0)
        return lit2;
    if (lit2 == 0)
        return lit1;
    if (lit1 == lit2)
        return 0;
    if ((lit1 ^ lit2) == 1)
        return 1;
    if (lit1 < lit2)
        KC_SWAP(int, lit1, lit2)
    assert(lit1 > lit2);
    int ttId = kc_vt_xor(&gg->tts, lit1, lit2);
    int lit = kc_gg_hash_node(gg, lit1, lit2, ttId);
    if (lit == -1)
        return kc_gg_append_node(gg, lit1, lit2, ttId);
    kc_vt_resize(&gg->tts, ttId);
    return lit;
}
static inline int kc_gg_or(kc_gg *gg, int lit1, int lit2) { return kc_lnot(kc_gg_and(gg, kc_lnot(lit1), kc_lnot(lit2))); }
static inline int kc_gg_mux(kc_gg *gg, int ctrl, int lit1, int lit0) { return kc_gg_or(gg, kc_gg_and(gg, ctrl, lit1), kc_gg_and(gg, kc_lnot(ctrl), lit0)); }
static inline int kc_gg_and_xor(kc_gg *gg, int ctrl, int lit1, int lit0) { return kc_gg_xor(gg, kc_gg_and(gg, ctrl, lit1), lit0); }

// counting nodes
int kc_gg_node_count_rec(kc_gg *gg, int lit)
{
    int res = 1, var = kc_l2v(lit);
    if (var <= gg->nins || !kc_gg_tid_update(gg, var))
        return 0;
    res += kc_gg_node_count_rec(gg, kc_vi_read(&gg->fans, lit));
    res += kc_gg_node_count_rec(gg, kc_vi_read(&gg->fans, kc_lnot(lit)));
    return res;
}
int kc_gg_node_count1(kc_gg *gg, int lit)
{
    kc_gg_tid_increment(gg);
    return kc_gg_node_count_rec(gg, lit);
}
int kc_gg_node_count2(kc_gg *gg, int lit0, int lit1)
{
    kc_gg_tid_increment(gg);
    return kc_gg_node_count_rec(gg, lit0) + kc_gg_node_count_rec(gg, lit1);
}
int kc_gg_node_count(kc_gg *gg)
{
    int i, top, Count = 0;
    kc_gg_tid_increment(gg);
    kc_vi_for_each_entry(&gg->tops, top, i)
        Count += kc_gg_node_count_rec(gg, top);
    return Count;
}

// counting levels
int kc_gg_level_rec(kc_gg *gg, int *levs, int lit)
{
    int res0, res1, var = kc_l2v(lit);
    if (var <= gg->nins || !kc_gg_tid_update(gg, var))
        return levs[var];
    res0 = kc_gg_level_rec(gg, levs, kc_vi_read(&gg->fans, lit));
    res1 = kc_gg_level_rec(gg, levs, kc_vi_read(&gg->fans, kc_lnot(lit)));
    return (levs[var] = 1 + kc_max(res0, res1));
}
int kc_gg_level(kc_gg *gg)
{
    int i, top, levMax = 0;
    int *levs = (int *)calloc(sizeof(int), gg->size);
    kc_gg_tid_increment(gg);
    kc_vi_for_each_entry(&gg->tops, top, i)
        levMax = kc_max(levMax, kc_gg_level_rec(gg, levs, top));
    free(levs);
    return levMax;
}

// printing the graph
void kc_gg_print_lit(int lit, int nVars)
{
    assert(lit >= 0);
    if (lit < 2)
        printf("%d", lit);
    else if (lit < 2 * (nVars + 1))
        printf("%s%c", kc_l2c(lit) ? "~" : "", (char)(96 + kc_l2v(lit)));
    else
        printf("%s%02d", kc_l2c(lit) ? "~n" : "n", kc_l2v(lit));
}
void kc_gg_print(kc_gg *gg, int verbose)
{
    int fPrintGraphs = verbose;
    int fPrintTruths = gg->nins <= 8;
    int i, top, nLevels, nCount[2] = {0};
    if (!fPrintGraphs)
    {
        printf("The graph contains %d nodes and spans %d levels.\n", kc_gg_node_count(gg), kc_gg_level(gg));
        return;
    }
    // mark used nodes with the new travId
    nLevels = kc_gg_level(gg);
    // print const and inputs
    if (fPrintTruths)
        kc_vt_print_int(&gg->tts, 0), printf(" ");
    printf("n%02d = 0\n", 0);
    for (i = 1; i <= gg->nins; i++)
    {
        if (fPrintTruths)
            kc_vt_print_int(&gg->tts, 2 * i), printf(" ");
        printf("n%02d = %c\n", i, (char)(96 + i));
    }
    // print used nodes
    int count = 1;
    for (i = gg->nins + 1; i < gg->size; i++)
        if (kc_gg_tid_is_cur(gg, i))
        {
            printf("%d ", count++);
            if (fPrintTruths)
                kc_vt_print_int(&gg->tts, 2 * i), printf(" ");
            printf("n%02d = ", i);
            kc_gg_print_lit(kc_gg_fanin(gg, i, 0), gg->nins);
            
            printf(" %c ", kc_gg_is_xor(gg, i) ? '^' : '&');
            kc_gg_print_lit(kc_gg_fanin(gg, i, 1), gg->nins);
            printf("\n");
            nCount[kc_gg_is_xor(gg, i)]++;
        }
    // print outputs
    kc_vi_for_each_entry(&gg->tops, top, i)
    {
        if (fPrintTruths)
            kc_vt_print_int(&gg->tts, top), printf(" ");
        printf("po%d = ", i);
        kc_gg_print_lit(top, gg->nins);
        printf("\n");
    }
    printf("The graph contains %d nodes (%d ands and %d xors) and spans %d levels.\n",
           nCount[0] + nCount[1], nCount[0], nCount[1], nLevels);
}

// duplicates AIG while only copying used nodes (also expands xors into ands as needed)
kc_gg *kc_gg_dup(kc_gg *gg, int only_and)
{
    kc_gg *ggNew = kc_gg_start(gg->nins, &gg->outs);
    int i, top, *pCopy = (int *)calloc(sizeof(int), 2 * gg->size);
    for (i = 0; i < 2 * (1 + gg->nins); i++)
        pCopy[i] = i;
    for (i = 1 + gg->nins; i < gg->size; i++)
        if (kc_gg_tid_is_cur(gg, i))
        {
            int lit0 = kc_gg_fanin(gg, i, 0);
            int lit1 = kc_gg_fanin(gg, i, 1);
            if (!kc_gg_is_xor(gg, i))
                pCopy[2 * i] = kc_gg_and(ggNew, pCopy[lit0], pCopy[lit1]);
            else if (only_and)
                pCopy[2 * i] = kc_gg_mux(ggNew, pCopy[lit0], kc_lnot(pCopy[lit1]), pCopy[lit1]);
            else
                pCopy[2 * i] = kc_gg_xor(ggNew, pCopy[lit0], pCopy[lit1]);
            pCopy[2 * i + 1] = kc_lnot(pCopy[2 * i]);
        }
    kc_vi_for_each_entry(&gg->tops, top, i)
        kc_vi_push(&ggNew->tops, pCopy[top]);
    free(pCopy);
    return ggNew;
}

// compares truth tables against the specification
void kc_gg_verify(kc_gg *gg)
{
    int i, top, nFailed = 0;
    kc_vi_for_each_entry(&gg->tops, top, i) if (!kc_vt_is_equal2(&gg->outs, i, &gg->tts, top))
        printf("Verification failed for output %d.\n", i),
        nFailed++;
    if (nFailed == 0)
        printf("Verification succeeded.  ");
}

/*************************************************************
                    AIGER interface
**************************************************************/

static void kc_aiger_write_uint(FILE *pFile, unsigned x)
{
    unsigned char ch;
    while (x & ~0x7f)
    {
        ch = (x & 0x7f) | 0x80;
        fputc(ch, pFile);
        x >>= 7;
    }
    ch = x;
    fputc(ch, pFile);
}
static void kc_aiger_write(char *pFileName, int *pObjs, int nObjs, int nIns, int nLatches, int nOuts, int nAnds, int *pOuts)
{
    std::string str(pFileName);
    FILE *pFile = fopen(("./outputs/" + str).c_str(), "wb");
    int i;
    if (pFile == NULL)
    {
        fprintf(stdout, "kc_aiger_write(): Cannot open the output file \"%s\".\n", pFileName);
        return;
    }
    fprintf(pFile, "aig %d %d %d %d %d\n", nIns + nLatches + nAnds, nIns, nLatches, nOuts, nAnds);
    for (i = 0; i < nLatches; i++)
        fprintf(pFile, "%d\n", pOuts[nOuts + i]);
    for (i = 0; i < nOuts; i++)
        fprintf(pFile, "%d\n", pOuts[i]);
    for (i = 0; i < nAnds; i++)
    {
        int uLit = 2 * (1 + nIns + nLatches + i);
        int uLit0 = pObjs[uLit + 0];
        int uLit1 = pObjs[uLit + 1];
        kc_aiger_write_uint(pFile, uLit - uLit1);
        kc_aiger_write_uint(pFile, uLit1 - uLit0);
    }
    fprintf(pFile, "c\n");
    fclose(pFile);
}
static void kc_gg_aiger_write(char *pFileName, kc_gg *gg, int fVerbose)
{
    kc_gg *ggNew = kc_gg_dup(gg, 1);
    // kc_gg_print( ggNew, fVerbose );
    kc_aiger_write(pFileName, kc_vi_array(&ggNew->fans), -1, kc_gg_pi_num(ggNew), 0, kc_gg_po_num(ggNew), kc_gg_node_num(ggNew), kc_vi_array(&ggNew->tops));
    if (fVerbose)
        printf("Written graph with %d inputs, %d outputs, and %d and-nodes into AIGER file \"%s\".\n",
               kc_gg_pi_num(ggNew), kc_gg_po_num(ggNew), kc_gg_node_num(ggNew), pFileName);
    kc_gg_stop(ggNew);
}

/*************************************************************
                  Permutation generation
**************************************************************/

// generate next permutation in lexicographic order
static void kc_get_next_perm(int *currPerm, int nVars, kc_vt *tts)
{
    int t, i = nVars - 1;
    while (i >= 0 && currPerm[i - 1] >= currPerm[i])
        i--;
    if (i >= 0)
    {
        int j = nVars;
        while (j > i && currPerm[j - 1] <= currPerm[i - 1])
            j--;
        KC_SWAP(int, currPerm[i - 1], currPerm[j - 1])
        if (tts)
            for (t = 0; t < tts->size; t++)
                kc_vt_swap_vars(tts, t, i - 1, j - 1);
        i++;
        j = nVars;
        while (i < j)
        {
            KC_SWAP(int, currPerm[i - 1], currPerm[j - 1])
            if (tts)
                for (t = 0; t < tts->size; t++)
                    kc_vt_swap_vars(tts, t, i - 1, j - 1);
            i++;
            j--;
        }
    }
}
static int kc_factorial(int nVars)
{
    int i, Res = 1;
    for (i = 1; i <= nVars; i++)
        Res *= i;
    return Res;
}

extern "C"
{
    // permutation testing procedure
    void kc_perm_test()
    {
        int i, k, nVars = 5, currPerm[MAX_VARS] = {0};
        for (i = 0; i < nVars; i++)
            currPerm[i] = i;
        int fact = kc_factorial(nVars);
        for (i = 0; i < fact; i++)
        {
            printf("%3d :", i);
            for (k = 0; k < nVars; k++)
                printf(" %d", currPerm[k]);
            printf("\n");
            kc_get_next_perm(currPerm, nVars, NULL);
        }
    }
}

/*************************************************************
                  Recursive synthesis
**************************************************************/

// takes the function and the top-most variable; returns literal of the circuit
int synthesis_and_rec(kc_gg *gg, int ttId, int varId)
{
    int iLit;
    if ((iLit = kc_gg_hash_function(gg, ttId)) >= 0)
        return iLit;
    // if ( kc_vt_is_const0(&gg->funcs, ttId) ) return 0;
    // if ( kc_vt_is_const1(&gg->funcs, ttId) ) return 1;
    if (!kc_vt_has_var(&gg->funcs, ttId, varId))
        return synthesis_and_rec(gg, ttId, varId - 1);
    int f0 = kc_vt_cof0(&gg->funcs, ttId, varId);
    int f1 = kc_vt_cof1(&gg->funcs, ttId, varId);
    int lit0 = synthesis_and_rec(gg, f0, varId - 1);
    int lit1 = synthesis_and_rec(gg, f1, varId - 1);
    kc_vt_shrink(&gg->funcs, 2);
    return kc_gg_mux(gg, kc_v2l(1 + varId, 0), lit1, lit0);
}
int synthesis_xor_rec(kc_gg *gg, int ttId, int varId)
{
    int iLit;
    if ((iLit = kc_gg_hash_function(gg, ttId)) >= 0)
        return iLit;
    // if ( kc_vt_is_const0(&gg->funcs, ttId) ) return 0;
    // if ( kc_vt_is_const1(&gg->funcs, ttId) ) return 1;
    if (!kc_vt_has_var(&gg->funcs, ttId, varId))
        return synthesis_xor_rec(gg, ttId, varId - 1);
    int f0 = kc_vt_cof0(&gg->funcs, ttId, varId);
    int f1 = kc_vt_cof1(&gg->funcs, ttId, varId);
    int f2 = kc_vt_xor(&gg->funcs, f0, f1);
    int lit0 = synthesis_xor_rec(gg, f0, varId - 1);
    int lit1 = synthesis_xor_rec(gg, f1, varId - 1);
    int lit2 = synthesis_xor_rec(gg, f2, varId - 1);
    kc_vt_shrink(&gg->funcs, 3);
    int n01 = kc_gg_node_count2(gg, lit0, lit1) + 1 + 2 * (lit0 >= 2 && lit1 >= 2);
    int n02 = kc_gg_node_count2(gg, lit0, lit2) + 1 + 1 * (lit0 >= 2 && lit1 >= 2);
    int n12 = kc_gg_node_count2(gg, lit1, lit2) + 1 + 1 * (lit0 >= 2 && lit1 >= 2);
    int min = kc_min(n01, kc_min(n02, n12));
    if (min == n01) // Shannon
        return kc_gg_mux(gg, kc_v2l(1 + varId, 0), lit1, lit0);
    if (min == n02) // positive Davio
        return kc_gg_and_xor(gg, kc_v2l(1 + varId, 0), lit2, lit0);
    if (min == n12) // negative Davio
        return kc_gg_and_xor(gg, kc_v2l(1 + varId, 1), lit2, lit1);
    return -1;
}

/*************************************************************
                  Reading input data
**************************************************************/

static inline int kc_log2(unsigned n)
{
    int r;
    if (n < 2)
        return (int)n;
    for (r = 0, n--; n; n >>= 1, r++)
    {
    };
    return r;
}
static inline int kc_hex_to_int(char c)
{
    int Digit = 0;
    if (c >= '0' && c <= '9')
        Digit = c - '0';
    else if (c >= 'A' && c <= 'F')
        Digit = c - 'A' + 10;
    else if (c >= 'a' && c <= 'f')
        Digit = c - 'a' + 10;
    else
        assert(0);
    assert(Digit >= 0 && Digit < 16);
    return Digit;
}
static inline kc_uint64 kc_truth_stretch(kc_uint64 t, int nVars)
{
    assert(nVars >= 0);
    if (nVars == 0)
        nVars++, t = (t & 0x1) | ((t & 0x1) << 1);
    if (nVars == 1)
        nVars++, t = (t & 0x3) | ((t & 0x3) << 2);
    if (nVars == 2)
        nVars++, t = (t & 0xF) | ((t & 0xF) << 4);
    if (nVars == 3)
        nVars++, t = (t & 0xFF) | ((t & 0xFF) << 8);
    if (nVars == 4)
        nVars++, t = (t & 0xFFFF) | ((t & 0xFFFF) << 16);
    if (nVars == 5)
        nVars++, t = (t & 0xFFFFFFFF) | ((t & 0xFFFFFFFF) << 32);
    assert(nVars == 6);
    return t;
}

static inline int kc_read_line(kc_vt *outs, kc_vi *chars, int nVars)
{
    int i, entry;
    assert(chars->size > 0);
    if (nVars == 0)
    {
        nVars = kc_log2(chars->size);
        kc_vt_start(outs, 0, kc_truth_word_num(nVars));
    }
    assert(nVars == kc_log2(chars->size));
    if ((1 << nVars) != chars->size)
    {
        printf("The input string length (%d chars) does not match the size (%d bits) of the truth table of %d-var function.\n",
               chars->size, 1 << nVars, nVars);
        kc_vt_stop(outs);
        kc_vi_stop(chars);
        return 0;
    }
    kc_uint64 *tt = kc_vt_append(outs);
    memset(tt, 0, 8 * outs->words);
    kc_vi_for_each_entry(chars, entry, i) if (entry == (int)'1')
        tt[(chars->size - 1 - i) >> 6] |= ((kc_uint64)1) << ((chars->size - 1 - i) & 63);
    if (nVars < 6)
        tt[0] = kc_truth_stretch(tt[0], nVars);
    kc_vi_resize(chars, 0);
    return nVars;
}
static inline int kc_read_file(FILE *pFile, kc_vt *outs)
{
    kc_vi Chars, *chars = &Chars;
    kc_vi_start(chars, 1000);
    int c, nVars = 0;
    while ((c = fgetc(pFile)) != EOF)
    {
        if (c == '\r' || c == '\t' || c == ' ')
            continue;
        if (c != '\n')
            kc_vi_push(chars, c);
        else if (chars->size && (nVars = kc_read_line(outs, chars, nVars)) == 0)
            return 0;
    }
    if (chars->size && (nVars = kc_read_line(outs, chars, nVars)) == 0)
        return 0;
    kc_vi_stop(chars);
    return nVars;
}
static inline int kc_read_input_data(char *pInput, kc_vt *outs)
{
    if (strstr(pInput, "."))
    { // pInput is a file name
        FILE *pFile = fopen(pInput, "rb");
        if (pFile == NULL)
        {
            printf("Cannot open file \"%s\" for reading.\n", pInput);
            return 0;
        }
        int nVars = kc_read_file(pFile, outs);
        // kc_vt_print2_all( outs );
        printf("Finished entring %d-input %d-output function from file \"%s\".\n", nVars, outs->size, pInput);
        fclose(pFile);
        return nVars;
    }
    else
    { // pInput is a truth table
        kc_uint64 *tt, Num = 0;
        int i;
        int nChars = strlen(pInput);
        int nVars = kc_log2(4 * nChars);
        if ((1 << nVars) != 4 * nChars)
        {
            printf("The input string length (%d chars) does not match the size (%d bits) of the truth table of %d-var function.\n",
                   nChars, 1 << nVars, nVars);
            return 0;
        }
        kc_vt_start(outs, 1, kc_truth_word_num(nVars));
        tt = kc_vt_append(outs);
        for (i = nChars - 1; i >= 0; i--)
        {
            Num |= (kc_uint64)kc_hex_to_int(pInput[nChars - 1 - i]) << ((i & 0xF) * 4);
            if ((i & 0xF) == 0)
                tt[i >> 4] = Num, Num = 0;
        }
        if (nVars < 6)
            tt[0] = kc_truth_stretch(tt[0], nVars);
        kc_vt_print_all(outs);
        printf("Finished entring %d-input %d-output function.\n", nVars, 1);
        return nVars;
    }
}

/*************************************************************
                  Top level procedures
**************************************************************/

// solve the problem for one variable order
static inline kc_gg *kc_top_level_call_one(int nvars, kc_vt *outs, int and_only, int verbose)
{
    kc_gg *gg = kc_gg_start(nvars, outs);
    int i, top;
    for (i = 0; i < outs->size; i++)
    {
        kc_vt_resize(&gg->funcs, 0);
        kc_vt_move(&gg->funcs, &gg->outs, i);
        if (and_only)
            top = synthesis_and_rec(gg, 0, nvars - 1);
        else
            top = synthesis_xor_rec(gg, 0, nvars - 1);
        kc_vi_push(&gg->tops, top);
    }
    return gg;
}

// solve the problem for all variable orders
static inline void kc_top_level_call_perm(int nvars, kc_vt *outs, int and_only, int verbose)
{
    kc_vt Best, *best = &Best;
    kc_vt_start(best, outs->size, outs->words);
    // initialize permutation
    int i, k, currPerm[MAX_VARS] = {0};
    for (i = 0; i < nvars; i++)
        currPerm[i] = i;
    // go through permutations and find the best one
    int CostBest = 0x7FFFFFFF;
    int fact = kc_factorial(nvars);
    for (i = 0; i < fact; i++)
    {
        kc_gg *ggTemp = kc_top_level_call_one(nvars, outs, and_only, verbose);
        int CostThis = kc_gg_node_count(ggTemp);
        if (CostBest > CostThis)
        {
            CostBest = CostThis;
            kc_vt_stop(best);
            kc_vt_dup(best, outs);
        }
        kc_gg_stop(ggTemp);
        if (verbose)
        {
            printf("%3d :", i);
            for (k = 0; k < nvars; k++)
                printf(" %d", currPerm[k]);
            printf(" : cost = %3d", CostThis);
            printf("\n");
        }
        kc_get_next_perm(currPerm, nvars, outs);
    }
    // update the truth table according to the best permutation
    kc_vt_stop(outs);
    kc_vt_dup(outs, best);
    kc_vt_stop(best);
}

// dump the result of solving the problem into a file (for example, "stats.txt")
static inline void kc_top_level_stats(char *pInput, int nvars, int nouts, int Cost)
{
    char *pDumpFile = (char *)"stats.txt";
    FILE *pFile = fopen(pDumpFile, "a+");
    if (pFile == NULL)
    {
        printf("Cannot open file \"%s\" for reading.\n", pDumpFile);
        return;
    }
    fprintf(pFile, "%s %d %d %d\n", pInput, nvars, nouts, Cost);
    fclose(pFile);
    printf("Added statistics for \"%s\" to the file \"%s\".\n", pInput, pDumpFile);
}

extern "C"
{

    // solving one instance of a problem
    int kc_top_level_call(char *input, int try_perm, int and_only, int verbose)
    {
        clock_t clkStart = clock();
        kc_vt Outs, *outs = &Outs;
        int nvars = kc_read_input_data(input, outs);
        if (nvars == 0)
            return 0;
        assert(nvars <= MAX_VARS);
        if (try_perm)
            kc_top_level_call_perm(nvars, outs, and_only, verbose);
        kc_gg *gg = kc_top_level_call_one(nvars, outs, and_only, verbose);
        kc_gg_print(gg, verbose);
        kc_gg_verify(gg);
        printf("Time =%6.2f sec\n", (float)(clock() - clkStart) / CLOCKS_PER_SEC);
        std::string str(input);
        size_t found = (str.find_last_of("/"));
        str = (str.substr(found+1, 4));
        
        str = str + ".aig";
        printf("%s\n", str.c_str());
        kc_gg_aiger_write((char *)str.c_str(), gg, 1);
        kc_top_level_stats((char *)str.c_str(), nvars, outs->size, kc_gg_node_count(gg));
        kc_gg_stop(gg);
        kc_vt_stop(outs);
        return 1;
    }

    // solving all problems in the list
    int kc_top_level_list(char *pInput, int try_perm, int and_only, int verbose)
    {
        FILE *pFile = fopen(pInput, "rb");
        if (pFile == NULL)
        {
            printf("Cannot open file \"%s\" for reading.\n", pInput);
            return 0;
        }
        char Buffer[1000];
        int nProbs = 0;
        while (fscanf(pFile, "%s", Buffer) == 1)
        {
            printf("\nSolving problem \"%s\".\n", Buffer);
            kc_top_level_call(Buffer, try_perm, and_only, verbose), nProbs++;
        }
        fclose(pFile);
        printf("\nFinished solving %d problems from the list \"%s\".\n", nProbs, pInput);
        return 1;
    }
}

/*************************************************************
                   main() procedure
**************************************************************/

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        printf("usage:  %s [-p] [-a] [-v] <string>\n", argv[0]);
        printf("        this program synthesized circuits from truth tables\n");
        printf("        -p : enables trying all variable permutations\n");
        printf("        -a : enables using only and-gates (no xor-gates)\n");
        printf("        -v : enables verbose output\n");
        printf("  <string> : a truth table in hex notation or a file name\n");
        return 1;
    }
    else
    {
        int try_perm = 0;
        int and_only = 0;
        int verbose = 0;
        int i;
        for (i = 1; i < argc; i++)
        {
            if (argv[i][0] == '-' && argv[i][1] == 'p' && argv[i][2] == '\0')
                try_perm ^= 1;
            if (argv[i][0] == '-' && argv[i][1] == 'a' && argv[i][2] == '\0')
                and_only ^= 1;
            if (argv[i][0] == '-' && argv[i][1] == 'v' && argv[i][2] == '\0')
                verbose ^= 1;
        }
        if (strstr(argv[argc - 1], ".filelist")) // solve several problems
            return kc_top_level_list(argv[argc - 1], try_perm, and_only, verbose);
        else // solve one problem
            return kc_top_level_call(argv[argc - 1], try_perm, and_only, verbose);
    }
}

/*************************************************************
                     End of file
**************************************************************/
