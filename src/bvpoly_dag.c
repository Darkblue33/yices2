/*
 * DAG OF BIT-VECTOR EXPRESSIONS
 */

#include <assert.h>

#include "memalloc.h"
#include "bit_tricks.h"
#include "bv64_constants.h"
#include "index_vectors.h"

#include "bvpoly_dag.h"



/*
 * LIST OPERATIONS
 */


/*
 * Initialize list[k] to a singleton list
 */
static inline void init_list(bvc_item_t *list, int32_t k) {
  list[k].pre = k;
  list[k].next = k;
}


/*
 * Add i before k in list[k]
 */
static inline void list_add(bvc_item_t *list, int32_t k, int32_t i) {
  int32_t j;

  assert(i != k);

  j = list[k].pre;
  list[j].next = i;
  list[i].pre = j;
  list[i].next = k;
  list[k].pre = i;
}


/*
 * Remove i from its current list
 */
static inline void list_remove(bvc_item_t *list, int32_t i) {
  int32_t j, k;

  j = list[i].pre;
  k = list[i].next;
  list[j].next = k;
  list[k].pre = j;
}



/*
 * Add n to one of the three node lists:
 * - list[0]  --> leaves
 * - list[-1] --> elementary nodes
 * - list[-2] --> default list
 */
static inline void bvc_dag_add_to_leaves(bvc_dag_t *dag, int32_t n) {
  assert(0 < n && n <= dag->nelems);
  list_add(dag->list, 0, n);
}

static inline void bvc_dag_add_to_elementary_list(bvc_dag_t *dag, int32_t n) {
  assert(0 < n && n <= dag->nelems);
  list_add(dag->list, -1, n);
}

static inline void bvc_dag_add_to_default_list(bvc_dag_t *dag, int32_t n) {
  assert(0 < n && n <= dag->nelems);
  list_add(dag->list, -2, n);
}


/*
 * Move n to a different list
 */
static inline void bvc_dag_move_to_leaves(bvc_dag_t *dag, int32_t n) {
  assert(0 < n && n <= dag->nelems);
  list_remove(dag->list, n);
  list_add(dag->list, 0, n);
}

static inline void bvc_dag_move_to_elementary_list(bvc_dag_t *dag, int32_t n) {
  assert(0 < n && n <= dag->nelems);
  list_remove(dag->list, n);
  list_add(dag->list, -1, n);
}





/*
 * DAG OPERATIONS
 */

/*
 * Initialize dag:
 * - n = initial size. If n=0, use the default size.
 */
void init_bvc_dag(bvc_dag_t *dag, uint32_t n) {
  bvc_item_t *tmp;

  if (n == 0) {
    n = DEF_BVC_DAG_SIZE;
  }
  if (n >= MAX_BVC_DAG_SIZE) {
    out_of_memory();
  }
  assert(n > 0);

  dag->desc = (bvc_header_t **) safe_malloc(n * sizeof(bvc_header_t *));
  dag->use = (int32_t **) safe_malloc(n * sizeof(int32_t *));
  tmp = (bvc_item_t *) safe_malloc((n + 2) * sizeof(bvc_item_t));
  dag->list = tmp + 2;

  dag->desc[0] = NULL;
  dag->use[0] = NULL;
  init_list(dag->list, -2);
  init_list(dag->list, -1);
  init_list(dag->list, 0);  

  dag->nelems = 0;
  dag->size = n;

  init_int_htbl(&dag->htbl, 0);
  init_int_bvset(&dag->vset, 0);  // use bvset default size (1024)
  init_int_hmap(&dag->vmap, 128);

  init_objstore(&dag->leaf_store, sizeof(bvc_leaf_t), 500);
  init_objstore(&dag->offset_store, sizeof(bvc_offset_t), 500);
  init_objstore(&dag->mono_store, sizeof(bvc_mono_t), 500);
  init_objstore(&dag->prod_store, sizeof(bvc_prod_t) + PROD_STORE_LEN * sizeof(varexp_t), 100);
  init_objstore(&dag->sum_store1, sizeof(bvc_sum_t) + SUM_STORE1_LEN * sizeof(int32_t), 500);
  init_objstore(&dag->sum_store2, sizeof(bvc_sum_t) + SUM_STORE2_LEN * sizeof(int32_t), 500);

  init_bvconstant(&dag->aux);
  init_ivector(&dag->buffer, 10);
}



/*
 * Increase the size (by 50%)
 */
static void extend_bvc_dag(bvc_dag_t *dag) {
  bvc_item_t *tmp;
  uint32_t n;

  n = dag->size + 1;
  n += (n >> 1);
  if (n >= MAX_BVC_DAG_SIZE) {
    out_of_memory();
  }

  assert(n > dag->size);

  dag->desc = (bvc_header_t **) safe_realloc(dag->desc, n * sizeof(bvc_header_t *));
  dag->use = (int32_t **) safe_realloc(dag->use, n * sizeof(int32_t *));
  tmp = dag->list - 2;
  tmp = (bvc_item_t *) safe_realloc(tmp, (n + 2) * sizeof(bvc_item_t));
  dag->list = tmp + 2;

  dag->size = n;
}


/*
 * Add a new node n with descriptor d
 * - set use[n] to NULL
 * - list[n] is not initialized
 */
static int32_t bvc_dag_add_node(bvc_dag_t *dag, bvc_header_t *d) {
  uint32_t i;

  i = dag->nelems + 1;
  if (i == dag->size) {
    extend_bvc_dag(dag);
  }
  assert(i < dag->size);

  dag->desc[i] = d;
  dag->use[i] = NULL;

  dag->nelems = i;

  return i;
}


/*
 * Free memory used by descriptor d
 * - free d iteslf if it's not form a store (i.e., d->len is too large)
 * - free d->constant.w if d->bitsize > 64
 */
static void delete_descriptor(bvc_header_t *d) {
  switch (d->tag) {
  case BVC_LEAF:
    break;

  case BVC_OFFSET:
    if (d->bitsize > 64) {
      bvconst_free(offset_node(d)->constant.w, (d->bitsize + 31) >> 5);
    }
    break;

  case BVC_MONO:
    if (d->bitsize > 64) {
      bvconst_free(mono_node(d)->coeff.w, (d->bitsize + 31) >> 5);
    }
    break;

  case BVC_PROD:
    if (prod_node(d)->len > PROD_STORE_LEN) {
      safe_free(d);
    }
    break;

  case BVC_SUM:
    if (sum_node(d)->len > SUM_STORE2_LEN) {
      safe_free(d);
    }
    break;
  }
}


/*
 * Delete the DAG
 */
void delete_bvc_dag(bvc_dag_t *dag) {
  uint32_t i, n;

  n = dag->nelems;
  for (i=1; i<=n; i++) {
    delete_descriptor(dag->desc[i]);
    delete_index_vector(dag->use[i]);
  }

  safe_free(dag->desc);
  safe_free(dag->use);
  safe_free(dag->list - 2);

  dag->desc = NULL;
  dag->use = NULL;
  dag->list = NULL;

  delete_int_htbl(&dag->htbl);
  delete_int_bvset(&dag->vset);
  delete_int_hmap(&dag->vmap);

  delete_objstore(&dag->leaf_store);
  delete_objstore(&dag->offset_store);
  delete_objstore(&dag->mono_store);
  delete_objstore(&dag->prod_store);
  delete_objstore(&dag->sum_store1);
  delete_objstore(&dag->sum_store2);

  delete_bvconstant(&dag->aux);
  delete_ivector(&dag->buffer);
}


/*
 * Empty: remove all nodes
 */
void reset_bvc_dag(bvc_dag_t *dag) {
  uint32_t i, n;

  n = dag->nelems;
  for (i=1; i<=n; i++) {
    delete_descriptor(dag->desc[i]);
    delete_index_vector(dag->use[i]);
  }

  dag->nelems = 0;

  // reset the three lists
  init_list(dag->list, -2);
  init_list(dag->list, -1);
  init_list(dag->list, 0);  

  reset_int_htbl(&dag->htbl);
  reset_int_bvset(&dag->vset);
  int_hmap_reset(&dag->vmap);

  reset_objstore(&dag->leaf_store);
  reset_objstore(&dag->offset_store);
  reset_objstore(&dag->mono_store);
  reset_objstore(&dag->prod_store);
  reset_objstore(&dag->sum_store1);
  reset_objstore(&dag->sum_store2);  

  ivector_reset(&dag->buffer);
}



/*
 * Check whether x is in vset (i.e., there's a node attached to x)
 */
static inline bool bvc_dag_var_is_present(bvc_dag_t *dag, int32_t x) {
  assert(x > 0);
  return int_bvset_member(&dag->vset, x);
}

/*
 * Get the node mapped to x: 
 */
static inline int32_t bvc_dag_node_of_var(bvc_dag_t *dag, int32_t x) {
  int_hmap_pair_t *p;

  assert(bvc_dag_var_is_present(dag, x));
  p = int_hmap_find(&dag->vmap, x);
  assert(p != NULL && 0 < p->val && p->val <= dag->nelems);
  return p->val;
}


/*
 * Add mapping [x --> n]
 */
static inline void bvc_dag_map_var_to_node(bvc_dag_t *dag, int32_t x, int32_t n) {
  int_hmap_pair_t *p;

  assert(! bvc_dag_var_is_present(dag, x));
  int_bvset_add(&dag->vset, x);

  p = int_hmap_get(&dag->vmap, x);
  assert(p->val == -1);
  p->val = n;
}


/*
 * Add i to the use list of n
 */
static inline void bvc_dag_add_dependency(bvc_dag_t *dag, int32_t n, int32_t i) {
  assert(0 < n && n <= dag->nelems && 0 < i && i <= dag->nelems && i != n);
  add_index_to_vector(dag->use + n, i);
}





/*
 * NODE DESCRIPTOR ALLOCATION
 */

/*
 * Descriptor allocation
 * - for prod and sum, n = length of the sum or product
 */
static inline bvc_leaf_t *alloc_leaf(bvc_dag_t *dag) {
  return (bvc_leaf_t *) objstore_alloc(&dag->leaf_store);
}

static inline bvc_offset_t *alloc_offset(bvc_dag_t *dag) {
  return (bvc_offset_t *) objstore_alloc(&dag->offset_store);
}

static inline bvc_mono_t *alloc_mono(bvc_dag_t *dag) {
  return (bvc_mono_t *) objstore_alloc(&dag->mono_store);
}

static bvc_prod_t *alloc_prod(bvc_dag_t *dag, uint32_t n) {
  void *tmp;

  if (n <= PROD_STORE_LEN) {
    tmp = objstore_alloc(&dag->prod_store);
  } else if (n <= MAX_BVC_PROD_LEN) {
    tmp = safe_malloc(sizeof(bvc_prod_t) + n * sizeof(varexp_t));
  } else {
    out_of_memory();
  }

  return (bvc_prod_t *) tmp;
}

static bvc_sum_t *alloc_sum(bvc_dag_t *dag, uint32_t n) {
  void *tmp;

  if (n <= SUM_STORE1_LEN) {
    tmp = objstore_alloc(&dag->sum_store1);
  } else if (n <= SUM_STORE2_LEN) {
    tmp = objstore_alloc(&dag->sum_store2);
  } else if (n <= MAX_BVC_SUM_LEN) {
    tmp = safe_malloc(sizeof(bvc_sum_t) + n * sizeof(int32_t));
  } else {
    out_of_memory();
  }

  return (bvc_sum_t *) tmp;
}


/*
 * De-allocation
 */
static inline void free_leaf(bvc_dag_t *dag, bvc_leaf_t *d) {
  objstore_free(&dag->leaf_store, d);
}

static void free_offset(bvc_dag_t *dag, bvc_offset_t *d) {
  if (d->header.bitsize > 64) {
    bvconst_free(d->constant.w, (d->header.bitsize + 31) >> 5);
  }
  objstore_free(&dag->offset_store, d);
}

static void free_mono(bvc_dag_t *dag, bvc_mono_t *d) {
  if (d->header.bitsize > 64) {
    bvconst_free(d->coeff.w, (d->header.bitsize + 31) >> 5);
  }
  objstore_free(&dag->mono_store, d);
}

static void free_prod(bvc_dag_t *dag, bvc_prod_t *d) {
  if (d->len <= PROD_STORE_LEN) {
    objstore_free(&dag->prod_store, d);
  } else {
    safe_free(d);
  }
}

static void free_sum(bvc_dag_t *dag, bvc_sum_t *d) {
  if (d->len <= SUM_STORE1_LEN) {
    objstore_free(&dag->sum_store1, d);
  } else if (d->len <= SUM_STORE2_LEN) {
    objstore_free(&dag->sum_store2, d);
  } else {
    safe_free(d);
  }
}






/*
 * NODE CONSTRUCTION
 */

/*
 * Create a leaf node for variable x
 * - x must be positive
 * - x must not be mapped (i.e., not in dag->vset)
 * - returns node index n and stores the mapping [x --> n]
 *   in dag->vmap.
 */
int32_t bvc_dag_add_leaf(bvc_dag_t *dag, int32_t x, uint32_t bitsize) {
  bvc_leaf_t *d;
  int32_t n;

  d = alloc_leaf(dag);
  d->header.tag = BVC_LEAF;
  d->header.var = x;
  d->header.bitsize = bitsize;

  n = bvc_dag_add_node(dag, &d->header);

  bvc_dag_add_to_leaves(dag, n);
  bvc_dag_map_var_to_node(dag, x, n);

  return n;
}



/*
 * Create an offset node q := [offset a, n]
 * - x = variable to attach to q (or -1)
 */
static int32_t bvc_dag_add_offset64(bvc_dag_t *dag, uint64_t a, int32_t n, int32_t x, uint32_t bitsize) {
  bvc_offset_t *d;
  int32_t q;

  assert(1 <= bitsize && bitsize <= 64 && a == norm64(a, bitsize) && (x == -1 || x > 0));

  d = alloc_offset(dag);
  d->header.tag = BVC_OFFSET;
  d->header.var = x;
  d->header.bitsize = bitsize;
  d->node = n;
  d->constant.c = a;

  q = bvc_dag_add_node(dag, &d->header);

  bvc_dag_add_to_elementary_list(dag, q);
  if (x > 0) {
    bvc_dag_map_var_to_node(dag, x, q);
  }

  return q;
}


static int32_t bvc_dag_add_offset(bvc_dag_t *dag, uint32_t *a, int32_t n, int32_t x, uint32_t bitsize) {
  bvc_offset_t *d;
  uint32_t *c;
  uint32_t k;
  int32_t q;

  assert(bitsize > 64 && (x == -1 || x > 0));

  // make a copy of a: a must be normalized so the copy will be normalized too
  k = (bitsize + 31) >> 5;
  c = bvconst_alloc(k);
  bvconst_set(c, k, a);
  assert(bvconst_is_normalized(c, bitsize));

  d = alloc_offset(dag);
  d->header.tag = BVC_OFFSET;
  d->header.var = x;
  d->header.bitsize = bitsize;
  d->node = n;
  d->constant.w = c;

  q = bvc_dag_add_node(dag, &d->header);

  bvc_dag_add_to_elementary_list(dag, q);
  if (x > 0) {
    bvc_dag_map_var_to_node(dag, x, q);
  }

  return q;
}



/*
 * Create an monomial node q := [mono a, n]
 * - x = variable to attach to q (or -1)
 */
static int32_t bvc_dag_add_mono64(bvc_dag_t *dag, uint64_t a, int32_t n, int32_t x, uint32_t bitsize) {
  bvc_mono_t *d;
  int32_t q;

  assert(1 <= bitsize && bitsize <= 64 && a == norm64(a, bitsize) && 
	 0 < n && n <= dag->nelems && (x == -1 || x > 0));

  d = alloc_mono(dag);
  d->header.tag = BVC_MONO;
  d->header.var = x;
  d->header.bitsize = bitsize;
  d->node = n;
  d->coeff.c = a;

  q = bvc_dag_add_node(dag, &d->header);

  bvc_dag_add_to_elementary_list(dag, q);
  if (x > 0) {
    bvc_dag_map_var_to_node(dag, x, q);
  }

  return q;
}


static int32_t bvc_dag_add_mono(bvc_dag_t *dag, uint32_t *a, int32_t n, int32_t x, uint32_t bitsize) {
  bvc_mono_t *d;
  uint32_t *c;
  uint32_t k;
  int32_t q;

  assert(bitsize > 64 && 0 < n && n <= dag->nelems && (x == -1 || x > 0));

  // make a copy of a.
  // a must be normalized so the copy will be normalized too
  k = (bitsize + 31) >> 5;
  c = bvconst_alloc(k);
  bvconst_set(c, k, a);
  assert(bvconst_is_normalized(c, bitsize));

  d = alloc_mono(dag);
  d->header.tag = BVC_MONO;
  d->header.var = x;
  d->header.bitsize = bitsize;
  d->node = n;
  d->coeff.w = c;

  q = bvc_dag_add_node(dag, &d->header);

  bvc_dag_add_to_elementary_list(dag, q);
  if (x > 0) {
    bvc_dag_map_var_to_node(dag, x, q);
  }

  return q;
}









/*
 * Get a node mapped to x
 * - if there's none, create a leaf node
 */
static int32_t bvc_dag_get_node_of_var(bvc_dag_t *dag, int32_t x, uint32_t bitsize) {
  if (bvc_dag_var_is_present(dag, x)) {
    return bvc_dag_node_of_var(dag, x);
  } else {
    return bvc_dag_add_leaf(dag, x, bitsize);
  }
}




/*
 * Convert a polynomial expression of a variable x to a node n
 * - x must be positive
 * - there mustn't be a node mapped to x yet
 * - store the mapping [x --> n] in dag->vmap
 * - return the node index
 *
 * All variables of p that are not mapped already are converted to
 * leaf nodes.
 */
int32_t bvc_dag_add_pprod(bvc_dag_t *dag, int32_t x, pprod_t *p, uint32_t bitsize) {
  return -1;
}

int32_t bvc_dag_add_poly64(bvc_dag_t *dag, int32_t x, bvpoly64_t *p) {
  return -1;
}

int32_t bvc_dag_add_poly(bvc_dag_t *dag, int32_t x, bvpoly_t *p) {
  return -1;
}


