/* ----------------------------------------------------------- */
/*           _ ___   	     ___                               */
/*          |_| | |_/	  |_| | |_/    SPEECH                  */
/*          | | | | \  +  | | | | \    RECOGNITION             */
/*          =========	  =========    SOFTWARE                */
/*                                                             */
/* ================> ATK COMPATIBLE VERSION <================= */
/*                                                             */
/* ----------------------------------------------------------- */
/* developed at:                                               */
/*                                                             */
/*      Machine Intelligence Laboratory (Speech Group)         */
/*      Cambridge University Engineering Department            */
/*      http://mi.eng.cam.ac.uk/                               */
/*                                                             */
/*      Entropic Cambridge Research Laboratory                 */
/*      (now part of Microsoft)                                */
/*                                                             */
/* ----------------------------------------------------------- */
/*         Copyright: Microsoft Corporation                    */
/*          1995-2000 Redmond, Washington USA                  */
/*                    http://www.microsoft.com                 */
/*                                                             */
/*          2000-2007 Cambridge University                     */
/*                    Engineering Department                   */
/*                                                             */
/*   Use of this software is governed by a License Agreement   */
/*    ** See the file License for the Conditions of Use  **    */
/*    **     This banner notice must not be removed      **    */
/*                                                             */
/* ----------------------------------------------------------- */
/*   File: HNBest.h   Core NBest definitions                   */
/* ----------------------------------------------------------- */

/* !HVER!HNBest: 1.6.0 [SJY 01/06/07] */

#define MAX_HEAP_DEGREE 31       /* Max degree of heap (IE ~2^31 entries) */
#define NB_HASH_SIZE 236807      /* Hash table size ( bigger == faster ) */

typedef struct nbnentry NBNodeEntry;
typedef struct nodeheap NBNodeHeap;

enum {
   neh_invalid, neh_unused, neh_used, neh_expanded
};

/* Node entry record */
struct nbnentry {
   Ptr info;                  /* User info */
   unsigned int mark:1;       /* Mark needed for consolidation */
   unsigned int status:2;     /* Status flags */
   unsigned int degree:5;     /* Degree of entry */
   unsigned int id:24;        /* Unique Identity number */
   double score;              /* Score */
   NBNodeEntry *parent;       /* Parent entry */
   NBNodeEntry *child;        /* Child entry */
   NBNodeEntry *succ;         /* Circular doubly linked list, forward */
   NBNodeEntry *pred;         /* Circular doubly linked list, backward */
};

struct nodeheap {
   Ptr info;                  /* User info */

   MemHeap nehHeap;           /* Heap on which to allocate entries */
   NBNodeEntry *head;         /* Most likely entry */
   int count;                 /* Number of entries open */
   int exps;                  /* Number of entries expanded */
   int length;                /* Length of head list */
   int id;                    /* Counter for numbering entries */
   int isize;                 /* Sizeof info structure */
};

NBNodeEntry *GetNodeEntry(NBNodeHeap *nodeheap);
NBNodeEntry *NewNodeEntry(double score,NBNodeEntry *neh,NBNodeHeap *nodeheap);
void DeleteNodeEntry(NBNodeEntry *neh,NBNodeHeap *nodeheap);
void ExpandNodeEntry(NBNodeEntry *ent,double score,NBNodeHeap *nodeheap);
NBNodeHeap *CreateNodeHeap(size_t nehSize,size_t fhiSize);
void InitNodeHeap(NBNodeHeap *nodeheap);
void DeleteNodeHeap(NBNodeHeap *nodeheap);

/* --------------- General Lattice stuff ------------------ */

void SortLNodes(Lattice *lat,Boolean onTime,Boolean nOnly);
/*
   Sort LNodes into a suitable order for doing NBest/output
   Note that this guarantees the topological sort so only
    sorts on time if this doesn't break the topological order
    If nOnly is true does not alter order of nodes but sets
    ln->n to the correct sorted position
*/

#define LNODE_FWD  1  /* ln->score = best path likelihood from start */
#define LNODE_REV -1  /* ln->score = best path likelihood from end */
#define LAT_SCORE  0  /* [ln,la]->score = relative likelihood from best path */

LogDouble LatLAhead(Lattice *lat,int dir);
/*
   Calculate best path likelihoods through lattice and return
   total likelihood of most likely path from start to end of lattice
    if (dir == LNODE_FWD) set each lnode score to likelihood of most
     likely path from start of lattice to node.
    if (dir == LNODE_REV) set each lnode score to likelihood of most
     likely path from end of lattice to node.
    if (dir == LAT_SCORE) set each both lnode and larc scores to relative
     likelihood of most likely path through lnode/larc.
*/

Lattice *PruneLattice(MemHeap *heap,Lattice *lat,float thresh);
/*
   Returned pruned version of supplied lattice.
   Lattice should already have score fields set and all nodes and links
   with scores smaller than thresh  will be removed.  As scores will
   normally be log likelihoods thresh should equal -beam.
*/

/* Reorder the farc chain for each node to enusre that the first */
/* one represents the most likely path */
void SortLArcs(Lattice *lat,int dir);
/*
   Called after LatLAhead(lat,LAT_SCORE) will sort each lnode foll/pred list
   of larcs into descending relative likelihood order - thus allowing more
   efficient calculation of NBest.
*/

/* --------- Lattice NBest stuff --------- */

#define lnType(ln)         (((ln)->n & LNODE_TMASK))
#define lnSetType(ln,type) (((ln)->n = (((ln)->n & ~LNODE_TMASK) + (type))))


typedef enum {
   EQUIV_NONE=0,    /* Type of alternatives considered distinct */
   EQUIV_WORD=1,    /*  Words (different pronunciations merge) */
   EQUIV_PRON=2,    /*  Pronunciations */
   EQUIV_OUTSYM=3   /*  Output symbols (Ie textual representation) */
}
AnsMatchType;

typedef struct latnodeentryinfo {
   NBNodeEntry *prev;               /* Previous one of these in path */
   LogDouble like;              /* Likelihood to this point */
   LNode *ln;                   /* Node we have reached */
   LArc *la;                    /* Arc in route */
   LArc *narc;                  /* Next arc to process */
   unsigned int hv;             /* Hash value for path matching */
   NBNodeEntry *hlink;              /* Next one in hash linked list */
}LatNodeEntryInfo;

typedef struct latnodeheapinfo {
   int n;                       /* Number of answers */
   AnsMatchType type;           /* Type of answers considered distinct */
   LogDouble like;              /* Best path likelihood */
   Lattice *lat;                /* Lattice thorugh which doing NBest */
   int ansHashSize;             /* Size of hash table below */
   NBNodeEntry **ansHashTab;    /* Hash table for finding matching answers */
}LatNodeHeapInfo;

enum {
   LNODE_VOID=0,      /* LNode can safely be ignored */
   LNODE_START=2,     /* LNode reachable from start */
   LNODE_END=3,       /* LNode reachable from end */
   LNODE_VIABLE=4,    /* LNode viable part of path (reachable from st & end) */
   LNODE_VIABLE_ST=6, /* LNode represents a possible path start */
   LNODE_VIABLE_EN=7, /* LNode represents a possible path end */
   LNODE_TMASK=15,
   LNODE_FROM_ST=16,  /* LNode reachable from start */
   LNODE_FROM_EN=32   /* LNode reachable from end */
};

Ptr LNodeMatchPtr(LNode *ln,AnsMatchType type);
/*
   Return a pointer representing results type for given node
*/

NBNodeEntry *LatNextBestNBEntry(NBNodeHeap *nodeheap);
/*
   Find the next best alternative from the heap.
   Will return NULL if one cannot be found
*/

NBNodeHeap *PrepareLatNBest(int n,AnsMatchType type,Lattice *lat,Boolean noSort);
/*
   Prepare a lattice/nodeheap for generating NBest alternatives.
   The value of n is not critical but is used (in conjunction with the
   lattice itself) to choose sizes for structures/hash tables.  Large
   values will increase speed at the expense of additional memory
   requirements.
*/

/* ------------------------ End of HNBest.h ----------------------- */
