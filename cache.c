/*
 * cache.c
 Primera version: Mapeo directo, cache unificado
 */


#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "cache.h"
#include "main.h"

/* De MASK_ORIG quedarán "n" 1 a la izquierda
 para identificar el tag de la direccion */
#define MASK_ORIG 0xFFFFFFFF  // 32 bits a 1

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
void set_cache_param(param, value)
  int param;
  int value;
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

void init_cache_aux(cache *c){
  int bitsOffset, bitsSet; // No. de bits para direccionar offset y set
  int auxMask; // ayuda a generar la mascara del set
    
  /* initialize the cache, and cache statistics data structures */
  c->size = cache_usize;
  c->associativity = cache_assoc;
  c->n_sets = c->size /(words_per_block*WORD_SIZE);
  c->LRU_head = (Pcache_line *)malloc(sizeof(Pcache_line)*c->n_sets);
  c->LRU_tail = NULL;
  c->set_contents = NULL;
   
    
   // Calcula campos de set y de offset
    // Tengo un problema con la macro LOG2
    bitsSet=((int)rint((log((double)(c->n_sets)))/(log(2.0))));
//    bitsOffset=((int)rint((log((double)(words_per_block)))/(log(2.0))));
    bitsOffset=((int)rint((log((double)(cache_block_size)))/(log(2.0))));
    
    auxMask=(1<<bitsSet)-1;
    
    c->index_mask = auxMask<< bitsOffset;
    c->index_mask_offset=bitsOffset;  // Cuánto lo debo correr a la derecha
//    printf("mask=%X\n",c->index_mask);
    
//    printf("bits set= %d, bits offset = %d mask= %04x\n",bitsSet,bitsOffset,auxMask);
    // Hay que poner los apuntadores a lineas de cache en cero
    for(int i=0; i< c->n_sets; i++)
        c->LRU_head[i]=NULL;
}

void init_cache()
{
    init_cache_aux(&c1);

    // int bitsOffset, bitsSet; // No. de bits para direccionar offset y set
    // int auxMask; // ayuda a generar la mascara del set
//     
  // /* initialize the cache, and cache statistics data structures */
    // c1.size= cache_usize;
    // c1.associativity=cache_assoc;
    // c1.n_sets=c1.size /(words_per_block*WORD_SIZE);
    // c1.LRU_head=(Pcache_line *)malloc(sizeof(Pcache_line)*c1.n_sets);
    // c1.LRU_tail=NULL;
    // c1.set_contents=NULL;
//    
//     
   // // Calcula campos de set y de offset
    // // Tengo un problema con la macro LOG2
    // bitsSet=((int)rint((log((double)(c1.n_sets)))/(log(2.0))));
// //    bitsOffset=((int)rint((log((double)(words_per_block)))/(log(2.0))));
    // bitsOffset=((int)rint((log((double)(cache_block_size)))/(log(2.0))));
//     
    // auxMask=(1<<bitsSet)-1;
//     
    // c1.index_mask = auxMask<< bitsOffset;
    // c1.index_mask_offset=bitsOffset;  // Cuánto lo debo correr a la derecha
// //    printf("mask=%X\n",c1.index_mask);
//     
// //    printf("bits set= %d, bits offset = %d mask= %04x\n",bitsSet,bitsOffset,auxMask);
    // // Hay que poner los apuntadores a lineas de cache en cero
    // for(int i=0; i<c1.n_sets; i++)
        // c1.LRU_head[i]=NULL;
    
    // Nos aseguramos que los contadores estén en cero
    // Dependiendo del compilador, esto puede o no ser necesario.
    // C no garantiza la inicialización
    
    cache_stat_data.accesses=0;
    cache_stat_data.copies_back=0;
    cache_stat_data.demand_fetches=0;
    cache_stat_data.misses=0;
    cache_stat_data.replacements=0;
    cache_stat_inst.accesses=0;
    cache_stat_inst.copies_back=0;
    cache_stat_inst.demand_fetches=0;
    cache_stat_inst.misses=0;
    cache_stat_inst.replacements=0;
}

/************************************************************/

/************************************************************/
void perform_access(addr, access_type)
  unsigned addr, access_type;
{
    static int nl=0;
    int index;  // para acceder a la linea correspondiente en el set
    unsigned int bitsSet, bitsOffset,tagMask,tag;
    int block_size_in_words = cache_block_size/WORD_SIZE;

    // Calcula tag
    bitsSet=((int)rint((log((double)(c1.n_sets)))/(log(2.0))));
    bitsOffset=((int)rint((log((double)(cache_block_size)))/(log(2.0))));
    tagMask=MASK_ORIG<<(bitsOffset+bitsSet);
    tag=addr&tagMask;
    
    //printf("Dir = %X, tag =%X\n",addr,tag);
    
    index = (addr & c1.index_mask) >> c1.index_mask_offset;
    nl++;
    
    switch(access_type){
        case TRACE_INST_LOAD:
            cache_stat_inst.accesses++;
            if(c1.LRU_head[index]==NULL){  // Compulsory miss
                cache_stat_inst.misses++;
                c1.LRU_head[index]=malloc(sizeof(cache_line));  // Deberias validar que hay memoria!!
                c1.LRU_head[index]->tag=tag;
                c1.LRU_head[index]->dirty=0;
                cache_stat_inst.demand_fetches+=block_size_in_words;
            } else if(c1.LRU_head[index]->tag!=tag){  // Cache miss
                if(c1.LRU_head[index]->dirty) { // Hay que guardar bloque
                    cache_stat_data.copies_back+=block_size_in_words;
     //               cache_stat_inst.demand_fetches+=block_size_in_words;
                }
    //            printf("Linea: %d. Index: %04x. Remplazo cache con tag viejo: %04X y nuevo %04x\n",nl,index,c1.LRU_head[index]->tag,tag);
                cache_stat_inst.misses++;
                cache_stat_inst.replacements++;
                cache_stat_inst.demand_fetches+=block_size_in_words;
                c1.LRU_head[index]->tag=tag;
                c1.LRU_head[index]->dirty=0;
            }
            break;
        case TRACE_DATA_LOAD:
            cache_stat_data.accesses++;
            
            if(c1.LRU_head[index]==NULL){  // Compulsory miss
                cache_stat_data.misses++;
                c1.LRU_head[index]=malloc(sizeof(cache_line));  // Deberias validar que hay memoria!!
                c1.LRU_head[index]->tag=tag;
                c1.LRU_head[index]->dirty=0;
                cache_stat_data.demand_fetches+=block_size_in_words;
            } else if(c1.LRU_head[index]->tag!=tag){  // Cache miss
                if(c1.LRU_head[index]->dirty) { // Hay que guardar bloque
                    cache_stat_data.copies_back+=block_size_in_words;
                }
                cache_stat_data.misses++;
                cache_stat_data.replacements++;
                cache_stat_data.demand_fetches+=block_size_in_words;
                c1.LRU_head[index]->tag=tag;
                c1.LRU_head[index]->dirty=0;
            }
            break;
        case TRACE_DATA_STORE:
            cache_stat_data.accesses++;
            
            if(c1.LRU_head[index]==NULL){  // Compulsory miss
                cache_stat_data.misses++;
                c1.LRU_head[index]=malloc(sizeof(cache_line));  // Deberias validar que hay memoria!!
                c1.LRU_head[index]->tag=tag;
                c1.LRU_head[index]->dirty=1;
                cache_stat_data.demand_fetches+=block_size_in_words;
            } else if(c1.LRU_head[index]->tag!=tag){  // Cache miss
                if(c1.LRU_head[index]->dirty) { // Hay que guardar bloque
                    cache_stat_data.copies_back+=block_size_in_words;
                }
                cache_stat_data.misses++;
                cache_stat_data.replacements++;
                cache_stat_data.demand_fetches+=block_size_in_words;
                c1.LRU_head[index]->tag=tag;
                c1.LRU_head[index]->dirty=1;
            }
            else
                c1.LRU_head[index]->dirty=1;
            break;
    }

}
/************************************************************/

/************************************************************/
void flush()
{
  int block_size_in_words = cache_block_size/WORD_SIZE;
  /* flush the cache */
    for(int i=0; i<c1.n_sets; i++){
        if(c1.LRU_head[i]!=NULL){
            if(c1.LRU_head[i]->dirty){
                cache_stat_data.copies_back+=block_size_in_words;
            }
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
  printf("*** CACHE SETTINGS ***\n");
  if (cache_split) {
    printf("  Split I- D-cache\n");
    printf("  I-cache size: \t%d\n", cache_isize);
    printf("  D-cache size: \t%d\n", cache_dsize);
  } else {
    printf("  Unified I- D-cache\n");
    printf("  Size: \t%d\n", cache_usize);
  }
  printf("  Associativity: \t%d\n", cache_assoc);
  printf("  Block size: \t%d\n", cache_block_size);
  printf("  Write policy: \t%s\n", 
	 cache_writeback ? "WRITE BACK" : "WRITE THROUGH");
  printf("  Allocation policy: \t%s\n",
	 cache_writealloc ? "WRITE ALLOCATE" : "WRITE NO ALLOCATE");
}
/************************************************************/

/************************************************************/
void print_stats()
{
  printf("\n*** CACHE STATISTICS ***\n");

  printf(" INSTRUCTIONS\n");
  printf("  accesses:  %d\n", cache_stat_inst.accesses);
  printf("  misses:    %d\n", cache_stat_inst.misses);
  if (!cache_stat_inst.accesses)
    printf("  miss rate: 0 (0)\n"); 
  else
    printf("  miss rate: %2.4f (hit rate %2.4f)\n", 
	 (float)cache_stat_inst.misses / (float)cache_stat_inst.accesses,
	 1.0 - (float)cache_stat_inst.misses / (float)cache_stat_inst.accesses);
  printf("  replace:   %d\n", cache_stat_inst.replacements);

  printf(" DATA\n");
  printf("  accesses:  %d\n", cache_stat_data.accesses);
  printf("  misses:    %d\n", cache_stat_data.misses);
  if (!cache_stat_data.accesses)
    printf("  miss rate: 0 (0)\n"); 
  else
    printf("  miss rate: %2.4f (hit rate %2.4f)\n", 
	 (float)cache_stat_data.misses / (float)cache_stat_data.accesses,
	 1.0 - (float)cache_stat_data.misses / (float)cache_stat_data.accesses);
  printf("  replace:   %d\n", cache_stat_data.replacements);

  printf(" TRAFFIC (in words)\n");
  printf("  demand fetch:  %d\n", cache_stat_inst.demand_fetches + 
	 cache_stat_data.demand_fetches);
  printf("  copies back:   %d\n", cache_stat_inst.copies_back +
	 cache_stat_data.copies_back);
}
/************************************************************/