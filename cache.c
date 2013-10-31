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
	int i;
	int n_sets = (cache_usize / cache_block_size) / cache_assoc;
	int nontag_bits = LOG2(n_sets) + LOG2(cache_block_size);

	// I-cache
	c1.size = cache_usize;                                       /* cache size */
	c1.associativity = cache_assoc;                              /* cache associativity */
	c1.n_sets = n_sets;                                          /* number of cache sets */
	c1.index_mask = (((2 << nontag_bits) - 1) >> LOG2(cache_block_size)) << LOG2(cache_block_size);/* mask to find cache index */
	c1.index_mask_offset = LOG2(cache_block_size);               /* number of zero bits in mask */
	c1.LRU_head = (Pcache_line *)malloc(sizeof(Pcache_line)*c1.n_sets);
	memset(c1.LRU_head, 0, sizeof(Pcache_line)*c1.n_sets);
	line = (cache_line *)malloc(sizeof(cache_line)*c1.n_sets);
	memset(line, 0, sizeof(cache_line)*c1.n_sets);
	for (i=0; i<c1.n_sets; i++) {
		c1.LRU_head[i] = line + i;
	}

	if (cache_split == FALSE)
		return;

	// D-cache
	c2.size = cache_usize;                                       /* cache size */
	c2.associativity = cache_assoc;                              /* cache associativity */
	c2.n_sets = n_sets;                                          /* number of cache sets */
	c2.index_mask = (((2 << nontag_bits) - 1) >> LOG2(cache_block_size)) << LOG2(cache_block_size);/* mask to find cache index */
	c2.index_mask_offset = LOG2(cache_block_size);               /* number of zero bits in mask */
	c2.LRU_head = (Pcache_line *)malloc(sizeof(Pcache_line)*c2.n_sets);
	memset(c2.LRU_head, 0, sizeof(Pcache_line)*c2.n_sets);
	line = (cache_line *)malloc(sizeof(cache_line)*c2.n_sets);
	memset(line, 0, sizeof(cache_line)*c2.n_sets);
	for (i=0; i<c2.n_sets; i++) {
		c2.LRU_head[i] = line + i;
	}

}
/************************************************************/

/************************************************************/
extern void data_copy_cache2mem(int *dirty);
void inst_copy_mem2cache(int *old_tag, int new_tag)
{
	cache_stat_inst.demand_fetches += (cache_block_size>>2);
	*old_tag = new_tag;
}

void inst_load_hit(int *dirty)
{
	// do nothing
}

void inst_load_miss(int empty, int *dirty, int *old_tag, int new_tag)
{
	cache_stat_inst.misses ++;
	if (!empty) {
		if (cache_writeback && *dirty)
			data_copy_cache2mem(dirty);
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

void data_copy_cache2mem(int *dirty)
{
	cache_stat_data.copies_back += (cache_block_size>>2);
	*dirty = 0;
}

void data_load_hit(int *dirty)
{
	// do nothing
}

void data_load_miss(int empty, int *dirty, int *old_tag, int new_tag)
{
	cache_stat_data.misses ++;
	if (!empty) {
		if (cache_writeback && *dirty)
			data_copy_cache2mem(dirty);
		cache_stat_data.replacements ++;
	}
	data_copy_mem2cache(old_tag, new_tag);
}

void data_write_hit(int *dirty)
{
	if (cache_writeback) {
		*dirty = 1;
	} else {
		data_copy_cache2mem(dirty);
	}
}

void data_write_miss(int empty, int *dirty, int *old_tag, int new_tag)
{
	cache_stat_data.misses ++;
	if (cache_writealloc) {
		if (!empty) {
			if (cache_writeback && *dirty)
				data_copy_cache2mem(dirty);
			cache_stat_data.replacements ++;
		}
		data_copy_mem2cache(old_tag, new_tag);
	} else {
		int dummy;
		data_copy_cache2mem(&dummy);
	}
}
/************************************************************/

extern int cc;
#define DPRINTF /*(cc<=22 || cc>=26)*/TRUE ? 0 : dprintf
/************************************************************/
void perform_access(unsigned addr, unsigned access_type)
{
	/* handle an access to the cache */
	int non_tag_bits = ceil(LOG2_FL(c1.n_sets)) + LOG2(cache_block_size);
	int i, tag = addr >> non_tag_bits;
	int cac_idx =  ((addr & c1.index_mask) >> c1.index_mask_offset) % c1.n_sets;
	Pcache_line cac_line = (Pcache_line)c1.LRU_head[cac_idx];

	if (access_type!=4) {
		DPRINTF("=================\n[%d]Add=0x%08X\nTag=   0x%05X\nIdx=     0x%03X(%d)\n", cc, addr, tag, cac_idx, cac_idx);
		for (i=0; i<c1.n_sets; i++) {
			if (c1.LRU_head[i]->tag)
				DPRINTF("Slot[%03d]: 0x%05X %d\n", i, c1.LRU_head[i]->tag, c1.LRU_head[i]->dirty);
		}
	}

	if (cc==2069)
		cc = cc;

	/* update access */
	switch (access_type) {
	case TRACE_INST_LOAD://2
		cache_stat_inst.accesses ++;
		if (!cac_line->tag) {
			// Miss
			//DPRINTF("[INST] Load[%d] 0x%05X => Miss(Empty)\n", cac_idx, tag);
			inst_load_miss(1, &cac_line->dirty, &cac_line->tag, tag);
		} else if (tag != cac_line->tag) {
			// Miss
			//DPRINTF("[INST] Load[%d] 0x%05X => Miss(Wrong)\n", cac_idx, tag);
			inst_load_miss(0, &cac_line->dirty, &cac_line->tag, tag);
		} else {
			// Hit
			//DPRINTF("[INST] Load[%d] 0x%05X => Hit\n", cac_idx, tag);
			inst_load_hit(&cac_line->dirty);
		}
		break;
    case TRACE_DATA_LOAD://0
		cache_stat_data.accesses ++;
		if (!cac_line->tag) {
			// Miss
			DPRINTF("[DATA] Load[%d] 0x%05X => Miss(Empty)\n", cac_idx, tag);
			data_load_miss(1, &cac_line->dirty, &cac_line->tag, tag);
		} else if (tag != cac_line->tag) {
			// Miss
			DPRINTF("[DATA] Load[%d] 0x%05X => Miss(Wrong)\n", cac_idx, tag);
			data_load_miss(0, &cac_line->dirty, &cac_line->tag, tag);
		} else {
			// Hit
			DPRINTF("[DATA] Load[%d] 0x%05X => Hit\n", cac_idx, tag);
			data_load_hit(&cac_line->dirty);
		}
		break;
    case TRACE_DATA_STORE://1
		cache_stat_data.accesses ++;
		if (!cac_line->tag) {
			// Miss
			DPRINTF("[DATA] Writ[%d] 0x%05X => Miss(Empty)\n", cac_idx, tag);
			data_write_miss(1, &cac_line->dirty, &cac_line->tag, tag);
		} else if (tag != cac_line->tag) {
			// Miss
			DPRINTF("[DATA] Writ[%d] 0x%05X => Miss(Wrong)\n", cac_idx, tag);
			data_write_miss(0, &cac_line->dirty, &cac_line->tag, tag);
		} else {
			// Hit
			DPRINTF("[DATA] Writ[%d] 0x%05X => Hit\n", cac_idx, tag);
			data_write_hit(&cac_line->dirty);
		}
		break;
	}

	if (access_type!=4) {
		DPRINTF("INST miss %d repl %d  DATA miss %d repl %d\n", 
			cache_stat_inst.misses, cache_stat_inst.replacements,
			cache_stat_data.misses, cache_stat_data.replacements);
	}
}
/************************************************************/

/************************************************************/
void flush()
{
	/* flush the cache */
	int i;
	for (i=0; i<c1.n_sets; i++) {
		//if (c1.LRU_head[i]->dirty) {
			int dummy;
			data_copy_cache2mem(&dummy);
		//}
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
