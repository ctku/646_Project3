/*
 * 
 * cache.c
 * 
 * Donald Yeung
 */


#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "cache.h"
#include "main.h"

/* cache configuration parameters */
static int cache_split = 0;
static int cache_usize = DEFAULT_CACHE_SIZE;
static int cache_isize = DEFAULT_CACHE_SIZE; 
static int cache_dsize = DEFAULT_CACHE_SIZE;
static int cache_block_size = DEFAULT_CACHE_BLOCK_SIZE;
static int words_per_block = DEFAULT_CACHE_BLOCK_SIZE / WORD_SIZE;
static int cache_assoc = DEFAULT_CACHE_ASSOC;
static int cache_writeback = DEFAULT_CACHE_WRITEBACK;
static int cache_writealloc = DEFAULT_CACHE_WRITEALLOC;

/* cache model data structures */
static Pcache icache;
static Pcache dcache;
static cache c1;
static cache c2;
static cache_stat cache_stat_inst;
static cache_stat cache_stat_data;

/************************************************************/
void set_cache_param(int param, int value)
{
  switch (param) {
  case CACHE_PARAM_BLOCK_SIZE:
    cache_block_size = value;
    words_per_block = value / WORD_SIZE;
    break;
  case CACHE_PARAM_USIZE:
    cache_split = FALSE;
    cache_usize = value;
    break;
  case CACHE_PARAM_ISIZE:
    cache_split = TRUE;
    cache_isize = value;
    break;
  case CACHE_PARAM_DSIZE:
    cache_split = TRUE;
    cache_dsize = value;
    break;
  case CACHE_PARAM_ASSOC:
    cache_assoc = value;
    break;
  case CACHE_PARAM_WRITEBACK:
    cache_writeback = TRUE;
    break;
  case CACHE_PARAM_WRITETHROUGH:
    cache_writeback = FALSE;
    break;
  case CACHE_PARAM_WRITEALLOC:
    cache_writealloc = TRUE;
    break;
  case CACHE_PARAM_NOWRITEALLOC:
    cache_writealloc = FALSE;
    break;
  default:
    printf("error set_cache_param: bad parameter value\n");
    exit(-1);
  }

}
/************************************************************/

/************************************************************/
void init_cache()
{
	/* initialize the cache, and cache statistics data structures */
	cache_line *line;
	int i, nontag_bits;

	// I-cache (or united)
	c1.size =  (cache_split) ? cache_isize : cache_usize;        /* cache size */
	c1.associativity = cache_assoc;                              /* cache associativity */
	c1.n_sets = (c1.size / cache_block_size) / cache_assoc;;     /* number of cache sets */
	nontag_bits = LOG2(c1.n_sets) + LOG2(cache_block_size);
	c1.index_mask = (((2 << nontag_bits) - 1) >> LOG2(cache_block_size)) << LOG2(cache_block_size);/* mask to find cache index */
	c1.index_mask_offset = LOG2(cache_block_size);               /* number of zero bits in mask */
	c1.LRU_head = (Pcache_line *)malloc(sizeof(Pcache_line)*c1.n_sets);
	c1.LRU_tail = (Pcache_line *)malloc(sizeof(Pcache_line)*c1.n_sets);
	memset(c1.LRU_head, 0, sizeof(Pcache_line)*c1.n_sets);
	memset(c1.LRU_tail, 0, sizeof(Pcache_line)*c1.n_sets);
	c1.set_contents = (int *)malloc(sizeof(int)*c1.n_sets);
	memset(c1.set_contents, 0, sizeof(int)*c1.n_sets);

	// D-cache
	c2.size = (cache_split) ? cache_dsize : cache_usize;;        /* cache size */
	c2.associativity = cache_assoc;                              /* cache associativity */
	c2.n_sets = (c2.size / cache_block_size) / cache_assoc;;     /* number of cache sets */
	nontag_bits = LOG2(c2.n_sets) + LOG2(cache_block_size);
	c2.index_mask = (((2 << nontag_bits) - 1) >> LOG2(cache_block_size)) << LOG2(cache_block_size);/* mask to find cache index */
	c2.index_mask_offset = LOG2(cache_block_size);               /* number of zero bits in mask */
	if (cache_split) {
		c2.LRU_head = (Pcache_line *)malloc(sizeof(Pcache_line)*c2.n_sets);
		c2.LRU_tail = (Pcache_line *)malloc(sizeof(Pcache_line)*c2.n_sets);
		memset(c2.LRU_head, 0, sizeof(Pcache_line)*c2.n_sets);
		memset(c2.LRU_tail, 0, sizeof(Pcache_line)*c2.n_sets);
		c2.set_contents = (int *)malloc(sizeof(int)*c2.n_sets);
		memset(c2.set_contents, 0, sizeof(int)*c2.n_sets);
	} else {
		c2.LRU_head = c1.LRU_head;
		c2.LRU_tail = c1.LRU_tail;
		c2.set_contents = c1.set_contents;
	}
}
/************************************************************/

/************************************************************/
extern void data_copy_cache2mem(int *dirty, int type);
void inst_copy_mem2cache(int *old_tag, int new_tag)
{
	cache_stat_inst.demand_fetches += (cache_block_size>>2);
	*old_tag = new_tag;
}

void inst_load_hit()
{
	// do nothing
}

void inst_load_miss(int empty, int replace, int old_dirty, int *new_dirty, int *old_tag, int new_tag)
{
	cache_stat_inst.misses ++;
	if (!empty) {
		if (cache_writeback && old_dirty)
			data_copy_cache2mem(new_dirty, CB_1LINE);
		if (replace)
			cache_stat_inst.replacements ++;
	}
	inst_copy_mem2cache(old_tag, new_tag);
}
/************************************************************/

/************************************************************/
void data_copy_mem2cache(int *old_tag, int new_tag)
{
	cache_stat_data.demand_fetches += (cache_block_size>>2);
	*old_tag = new_tag;
}

void data_copy_cache2mem(int *dirty, int type)
{
	int size = (type == CB_1WORD) ? 1 : (cache_block_size>>2);

	cache_stat_data.copies_back += size;
	*dirty = 0;
}

void data_load_hit()
{
	// do nothing
}

void data_load_miss(int empty, int replace, int old_dirty, int *new_dirty, int *old_tag, int new_tag)
{
	cache_stat_data.misses ++;
	if (!empty) {
		if (cache_writeback && old_dirty)
			data_copy_cache2mem(new_dirty, CB_1LINE);
		if (replace)
			cache_stat_data.replacements ++;
	}
	data_copy_mem2cache(old_tag, new_tag);
}

void data_write_hit(int *dirty)
{
	// write through always generate 1 word to CB stats for DATA_STORE
	if (!cache_writeback) {
		data_copy_cache2mem(dirty, CB_1WORD);
	}

	if (cache_writeback) {
		*dirty = 1;
	}
}

void data_write_miss(int empty, int replace, int old_dirty, int *new_dirty, int *old_tag, int new_tag)
{
	// write through always generate 1 word to CB stats for DATA_STORE
	if (!cache_writeback) {
		int dummy;
		data_copy_cache2mem(&dummy, CB_1WORD);
	}

	cache_stat_data.misses ++;
	if (cache_writealloc) {
		if (!empty) {
			if (cache_writeback && old_dirty)
				data_copy_cache2mem(new_dirty, CB_1LINE);
			if (replace)
				cache_stat_data.replacements ++;
		}
		data_copy_mem2cache(old_tag, new_tag);
		// then CPU write to cache again
		if (cache_writeback) {
			*new_dirty = 1;
		}
	} else {
		// copy data from cpu to mem, no replacement
		int dummy;
		data_copy_cache2mem(&dummy, CB_1WORD);
	}
}
/************************************************************/
extern int cc;
#define DPRINTF /*(cc<=22 || cc>=26)*/TRUE ? 0 : dprintf
/************************************************************/
void perform_access(unsigned addr, unsigned access_type)
{
	/* handle an access to the cache */
	int i, c1_nontag_bits = 0, c2_nontag_bits = 0, c1_tag = 0, c2_tag = 0, c1_idx = 0, c2_idx = 0, c1_no = 0, c2_no = 0;
	Pcache_line c1_line, c2_line;
	Pcache_line new_node;

	c1_nontag_bits = ceil(LOG2_FL(c1.n_sets)) + LOG2(cache_block_size);
	c1_tag = addr >> c1_nontag_bits;
	c1_idx =  ((addr & c1.index_mask) >> c1.index_mask_offset) % c1.n_sets;
	c1_line = (Pcache_line)c1.LRU_head[c1_idx];
	c1_no = c1.set_contents[c1_idx];

	if (cache_split) {
		c2_nontag_bits = ceil(LOG2_FL(c2.n_sets)) + LOG2(cache_block_size);
		c2_tag = addr >> c2_nontag_bits;
		c2_idx =  ((addr & c2.index_mask) >> c2.index_mask_offset) % c2.n_sets;
		c2_line = (Pcache_line)c2.LRU_head[c2_idx];
		c2_no = c2.set_contents[c2_idx];
	} else {
		c2_tag = c1_tag;
		c2_idx = c1_idx;
		c2_line = c1_line;
		c2_no = c1_no;
	}

	/* update access */
	switch (access_type) {
	case TRACE_INST_LOAD://2
		cache_stat_inst.accesses ++;
		if (c1_no == 0) {
			// Miss
			new_node = (Pcache_line)malloc(sizeof(cache_line));
			memset(new_node, 0, sizeof(cache_line));
			insert(&c1.LRU_head[c1_idx], &c1.LRU_tail[c1_idx], new_node);
			c1.set_contents[c1_idx] ++;
			inst_load_miss(1, 0, new_node->dirty, &new_node->dirty, &new_node->tag, c1_tag);
		} else {
			Pcache_line cur = (Pcache_line)c1.LRU_head[c1_idx];
			int found = 0;
			while (cur) {
				if (c1_tag == cur->tag) {
					found = 1;
					break;
				}
				cur = cur->LRU_next;
			}
			if (!found) {
				// Miss
				if (c1_no < cache_assoc) {
					// Insertable
					new_node = (Pcache_line)malloc(sizeof(cache_line));
					memset(new_node, 0, sizeof(cache_line));
					insert(&c1.LRU_head[c1_idx], &c1.LRU_tail[c1_idx], new_node);
					c1.set_contents[c1_idx] ++;
					inst_load_miss(0, 0, new_node->dirty, &new_node->dirty, &new_node->tag, c1_tag);
				} else {
					// Not Insertable, need replace LRU
					// delete(tail)
					int old_dirty = c1.LRU_tail[c1_idx]->dirty;
					delete(&c1.LRU_head[c1_idx], &c1.LRU_tail[c1_idx], c1.LRU_tail[c1_idx]);
					c1.set_contents[c1_idx] --;
					// insert(new)
					new_node = (Pcache_line)malloc(sizeof(cache_line));
					memset(new_node, 0, sizeof(cache_line));
					insert(&c1.LRU_head[c1_idx], &c1.LRU_tail[c1_idx], new_node);
					c1.set_contents[c1_idx] ++;
					inst_load_miss(0, 1, old_dirty, &new_node->dirty, &new_node->tag, c1_tag);
				}
			} else {
				// Hit
				if (c1_no > 1) {
					// switch nodes to maintain order of LRU
					delete(&c1.LRU_head[c1_idx], &c1.LRU_tail[c1_idx], cur);
					insert(&c1.LRU_head[c1_idx], &c1.LRU_tail[c1_idx], cur);
				}
				inst_load_hit();
			}
		}
		break;
    case TRACE_DATA_LOAD://0
		cache_stat_data.accesses ++;
		if (c2_no == 0) {
			// Miss
			new_node = (Pcache_line)malloc(sizeof(cache_line));
			memset(new_node, 0, sizeof(cache_line));
			insert(&c2.LRU_head[c2_idx], &c2.LRU_tail[c2_idx], new_node);
			c2.set_contents[c2_idx] ++;
			data_load_miss(1, 0, new_node->dirty, &new_node->dirty, &new_node->tag, c2_tag);
		} else {
			Pcache_line cur = (Pcache_line)c2.LRU_head[c2_idx];
			int found = 0;
			while (cur) {
				if (c2_tag == cur->tag) {
					found = 1;
					break;
				}
				cur = cur->LRU_next;
			}
			if (!found) {
				// Miss
				if (c2_no < cache_assoc) {
					// Insertable
					new_node = (Pcache_line)malloc(sizeof(cache_line));
					memset(new_node, 0, sizeof(cache_line));
					insert(&c2.LRU_head[c2_idx], &c2.LRU_tail[c2_idx], new_node);
					c2.set_contents[c2_idx] ++;
					data_load_miss(0, 0, new_node->dirty, &new_node->dirty, &new_node->tag, c2_tag);
				} else {
					// Not Insertable, need replace LRU
					// delete(tail)
					int old_dirty = c2.LRU_tail[c2_idx]->dirty;
					delete(&c2.LRU_head[c2_idx], &c2.LRU_tail[c2_idx], c2.LRU_tail[c2_idx]);
					if (old_dirty==1)
						old_dirty = old_dirty;
					c2.set_contents[c2_idx] --;
					// insert(new)
					new_node = (Pcache_line)malloc(sizeof(cache_line));
					memset(new_node, 0, sizeof(cache_line));
					insert(&c2.LRU_head[c2_idx], &c2.LRU_tail[c2_idx], new_node);
					c2.set_contents[c2_idx] ++;
					data_load_miss(0, 1, old_dirty, &new_node->dirty, &new_node->tag, c2_tag);
				}
			} else {
				// Hit
				if (c2_no > 1) {
					// switch nodes to maintain order of LRU
					delete(&c2.LRU_head[c2_idx], &c2.LRU_tail[c2_idx], cur);
					insert(&c2.LRU_head[c2_idx], &c2.LRU_tail[c2_idx], cur);
				}
				data_load_hit();
			}
		}
		break;
    case TRACE_DATA_STORE://1
		cache_stat_data.accesses ++;
		if (cache_writealloc) {
			if (c2_no == 0) {
				// Miss
				new_node = (Pcache_line)malloc(sizeof(cache_line));
				memset(new_node, 0, sizeof(cache_line));
				insert(&c2.LRU_head[c2_idx], &c2.LRU_tail[c2_idx], new_node);
				c2.set_contents[c2_idx] ++;
				data_write_miss(1, 0, new_node->dirty, &new_node->dirty, &new_node->tag, c2_tag);
			} else {
				Pcache_line cur = (Pcache_line)c2.LRU_head[c2_idx];
				int found = 0;
				while (cur) {
					if (c2_tag == cur->tag) {
						found = 1;
						break;
					}
					cur = cur->LRU_next;
				}
				if (!found) {
					// Miss
					if (c2_no < cache_assoc) {
						// Insertable
						new_node = (Pcache_line)malloc(sizeof(cache_line));
						memset(new_node, 0, sizeof(cache_line));
						insert(&c2.LRU_head[c2_idx], &c2.LRU_tail[c2_idx], new_node);
						c2.set_contents[c2_idx] ++;
						data_write_miss(0, 0, new_node->dirty, &new_node->dirty, &new_node->tag, c2_tag);
					} else {
						// Not Insertable, need replace LRU
						// delete(tail)
						int old_dirty = c2.LRU_tail[c2_idx]->dirty;
						delete(&c2.LRU_head[c2_idx], &c2.LRU_tail[c2_idx], c2.LRU_tail[c2_idx]);
						c2.set_contents[c2_idx] --;
						// insert(new)
						new_node = (Pcache_line)malloc(sizeof(cache_line));
						memset(new_node, 0, sizeof(cache_line));
						insert(&c2.LRU_head[c2_idx], &c2.LRU_tail[c2_idx], new_node);
						c2.set_contents[c2_idx] ++;
						data_write_miss(0, 1, old_dirty, &new_node->dirty, &new_node->tag, c2_tag);
					}
				} else {
					// Hit
					if (c2_no > 1) {
						// switch nodes to maintain order of LRU
						delete(&c2.LRU_head[c2_idx], &c2.LRU_tail[c2_idx], cur);
						insert(&c2.LRU_head[c2_idx], &c2.LRU_tail[c2_idx], cur);
					}
					data_write_hit(&cur->dirty);
				}
			}
		} else {
			// Write non allocate
			Pcache_line cur = (Pcache_line)c2.LRU_head[c2_idx];
			int found = 0, dummy = 0;
			while (cur) {
				if (c2_tag == cur->tag) {
					found = 1;
					break;
				}
				cur = cur->LRU_next;
			}
			// no cache will be modified
			if (found) {
				if (c2_no > 1) {
					// switch nodes to maintain order of LRU
					delete(&c2.LRU_head[c2_idx], &c2.LRU_tail[c2_idx], cur);
					insert(&c2.LRU_head[c2_idx], &c2.LRU_tail[c2_idx], cur);
				}
				data_write_hit(&cur->dirty);
			} else {
				data_write_miss(0, 0, 0, &dummy, &dummy, 0);
			}
		}
		break;
	}
}
/************************************************************/

/************************************************************/
void flush()
{
	/* flush the cache */
	int i, j;
	for (i=0; i<c1.n_sets; i++) {
		Pcache_line cur = c1.LRU_head[i];
		while (cur) {
			if (cur->dirty)
				data_copy_cache2mem(&c1.LRU_head[i]->dirty, CB_1LINE);
			cur = cur->LRU_next;
		}
	}
	for (i=0; i<c2.n_sets; i++) {
		Pcache_line cur = c2.LRU_head[i];
		while (cur) {
			if (cur->dirty)
				data_copy_cache2mem(&c2.LRU_head[i]->dirty, CB_1LINE);
			cur = cur->LRU_next;
		}
	}
}
/************************************************************/

/************************************************************/
void delete(head, tail, item)
  Pcache_line *head, *tail;
  Pcache_line item;
{
  if (item->LRU_prev) {
    item->LRU_prev->LRU_next = item->LRU_next;
  } else {
    /* item at head */
    *head = item->LRU_next;
  }

  if (item->LRU_next) {
    item->LRU_next->LRU_prev = item->LRU_prev;
  } else {
    /* item at tail */
    *tail = item->LRU_prev;
  }
}
/************************************************************/

/************************************************************/
/* inserts at the head of the list */
void insert(head, tail, item)
  Pcache_line *head, *tail;
  Pcache_line item;
{
  item->LRU_next = *head;
  item->LRU_prev = (Pcache_line)NULL;

  if (item->LRU_next)
    item->LRU_next->LRU_prev = item;
  else
    *tail = item;

  *head = item;
}
/************************************************************/

/************************************************************/
void dump_settings()
{
  printf("Cache Settings:\n");
  if (cache_split) {
    printf("\tSplit I- D-cache\n");
    printf("\tI-cache size: \t%d\n", cache_isize);
    printf("\tD-cache size: \t%d\n", cache_dsize);
  } else {
    printf("\tUnified I- D-cache\n");
    printf("\tSize: \t%d\n", cache_usize);
  }
  printf("\tAssociativity: \t%d\n", cache_assoc);
  printf("\tBlock size: \t%d\n", cache_block_size);
  printf("\tWrite policy: \t%s\n", 
	 cache_writeback ? "WRITE BACK" : "WRITE THROUGH");
  printf("\tAllocation policy: \t%s\n",
	 cache_writealloc ? "WRITE ALLOCATE" : "WRITE NO ALLOCATE");
}
/************************************************************/

/************************************************************/
void print_stats()
{
  printf("*** CACHE STATISTICS ***\n");
  printf("  INSTRUCTIONS\n");
  printf("  accesses:  %d\n", cache_stat_inst.accesses);
  printf("  misses:    %d\n", cache_stat_inst.misses);
  printf("  miss rate: %f\n", 
	 (float)cache_stat_inst.misses / (float)cache_stat_inst.accesses);
  printf("  replace:   %d\n", cache_stat_inst.replacements);

  printf("  DATA\n");
  printf("  accesses:  %d\n", cache_stat_data.accesses);
  printf("  misses:    %d\n", cache_stat_data.misses);
  printf("  miss rate: %f\n", 
	 (float)cache_stat_data.misses / (float)cache_stat_data.accesses);
  printf("  replace:   %d\n", cache_stat_data.replacements);

  printf("  TRAFFIC (in words)\n");
  printf("  demand fetch:  %d\n", cache_stat_inst.demand_fetches + 
	 cache_stat_data.demand_fetches);
  printf("  copies back:   %d\n", cache_stat_inst.copies_back +
	 cache_stat_data.copies_back);
}
/************************************************************/
