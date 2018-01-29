
struct idstat {     /* node for tracking requests */
    struct idstat *next;/* ptr to next idstat node */
    int rid;            /* request ID */
    int state;          /* request state: */
    /*    0 = UNKNOWN (only used during node creation) */
    /*    1 = DEFERRED */
    /*    2 = ALLOCATED */
    /*    3 = DEALLOCATED */
    uint size;          /* region size (power of 2) */
    uint addr;          /* region address */
} *rlist,       /* list of request status nodes */
    *rle;         /* a single entry on rlist */


struct freg {       /* node for a free/unused region */
    struct freg *next;  /* ptr to next node on the same list */
    uint size;      /* size of the region (is this really needed?) */
    uint addr;      /* starting address of the region */
} *fnode,       /* a single free region node */
    **flist;      /* array of struct freg pointers -- list heads */
/* flist[0] for msize, flist[ns-1] for asize */
/* lists kept in ascending order on addr */


struct areq {       /* node for a deferred allocation request */
    struct areq *next;  /* ptr to next request */
    struct idstat *p;   /* ptr to idstat node */
} *anode,       /* a single deferred allocation request node */
    *panode,      /* predecessor of anode */
    *def;         /* head of list of deferred requests */
