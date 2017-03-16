/*
 * cache.c
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

void init_cache_aux(cache *c, int size){
  int bitsOffset, bitsSet; // No. de bits para direccionar offset y set
  int auxMask; // ayuda a generar la máscara del set
    
  /* initialize the cache, and cache statistics data structures */
  c->size = size;
  c->associativity = cache_assoc;
  c->n_sets = size/(cache_assoc * cache_block_size);
  c->LRU_head = (Pcache_line *)malloc(sizeof(Pcache_line)*c->n_sets);
  c->LRU_tail = (Pcache_line*)malloc(sizeof(Pcache_line)*c->n_sets);
  c->set_contents = (int*)malloc(sizeof(int)*c->n_sets);
  c->contents = 0;
   
  // Calcula campos de set y de offset
  bitsSet = LOG2(c->n_sets);
  bitsOffset = LOG2(cache_block_size);
    
  auxMask = (1<<bitsSet) - 1;
    
  c->index_mask = auxMask << bitsOffset;
  c->index_mask_offset = bitsOffset;  

  for(int i=0; i< c->n_sets; i++){
      c->LRU_head[i] = NULL;
      c->LRU_tail[i] = NULL;
      c->set_contents[i] = 0;
  }
}

void init_cache()
{
  // printf("Checando tipo de caché\n");
  if(cache_split){
    // printf("Inicializando...");
    init_cache_aux(&c1, cache_dsize);
    // printf("Caché 1 listo...");
    init_cache_aux(&c2, cache_isize);  
    // printf("Caché 2 listo.\n");
  } else {
    // printf("Inicializando...");
    init_cache_aux(&c1, cache_usize);  
    // printf("Caché listo.\n");
  }

  // printf("\n\nInicializando estadísticas...");
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
  // printf("Listo\n\n");
}

/************************************************************/

/************************************************************/


void perform_access_instruction_load(
	cache *c, 
	unsigned addr, 
	int index,
	int block_size_in_words,
	unsigned int tag)
{
	cache_stat_inst.accesses++;

	if(c->LRU_head[index] == NULL){  // Lista vacía
	    
	    cache_stat_inst.misses++;
	    cache_stat_inst.demand_fetches += block_size_in_words;
	    
	    Pcache_line new_item = malloc(sizeof(cache_line));
	    
	    new_item->tag = tag;
	    new_item->dirty = 0;
	    c->set_contents[index] = 1;
	    insert(&c->LRU_head[index], &c->LRU_tail[index], new_item);
	} else { // Si la lista no está vacía
		

	  if(c->set_contents[index] == c->associativity){
	    // Si no hay espacio en la lista
	    
	    Pcache_line cl = c->LRU_head[index];
	    int tag_found = FALSE; // Si encuentra el tag en algún nodo

	    for(int i = 0; i < c->set_contents[index]; i ++){
	      if(cl->tag == tag) {
	        tag_found = TRUE;
	        
	        break;
	      }
	      // if(cl->LRU_next == NULL) break;
	    	cl = cl->LRU_next; // se mueve al siguiente nodo
	    }

	    if(tag_found){ // hit
	    	
	    	// si encontró el tag, se debe pasar el nodo al principio de la lista
	    	delete(&c->LRU_head[index], &c->LRU_tail[index], cl);
	    	insert(&c->LRU_head[index], &c->LRU_tail[index], cl);

	    } else { // miss
	    	
	    	cache_stat_inst.demand_fetches += block_size_in_words;
	    	cache_stat_inst.misses++;
	    	cache_stat_inst.replacements++;
	    	// Si el tag no estaba, debe eliminar el nodo de la cola
	    	// y crear uno nuevo para insertar en el head

	    	
	    	Pcache_line new_item = malloc(sizeof(cache_line));
	    	
	    	new_item->tag = tag;
	    	new_item->dirty = 0;

	    	// inserta el nuevo elemento
	    	insert(&c->LRU_head[index], &c->LRU_tail[index], new_item);
	    	
	    	if(c->LRU_tail[index]->dirty) { // Hay que guardar bloque
	    	    cache_stat_data.copies_back += block_size_in_words;
	    	}

	    	// elimina último elemento
	    	delete(&c->LRU_head[index], &c->LRU_tail[index], c->LRU_tail[index]);
	    	
	    }


	  } else { // Si sí hay espacio en la lista
	    // Recorrer lista para ver si está el tag
	    int tag_found = FALSE; // Si encuentra el tag en algún nodo
	    Pcache_line cl = c->LRU_head[index];
	    
	    for(int i = 0; i < c->set_contents[index]; i ++){
	      if(cl->tag == tag) {
	        tag_found = TRUE;
	        break;
	      }
	      // if(cl->LRU_next == NULL) break;
	    	cl = cl->LRU_next; // se mueve al siguiente nodo
	    }

	    if(tag_found){ // hit
	    	// si encontró el tag, se debe pasar el nodo al principio de la lista
	    	delete(&c->LRU_head[index], &c->LRU_tail[index], cl);
	    	insert(&c->LRU_head[index], &c->LRU_tail[index], cl);

	    } else { // miss
	    	
	    	cache_stat_inst.demand_fetches += block_size_in_words;
	    	cache_stat_inst.misses++;
	    	// Si el tag no estaba, crea nuevo elemento para insertar 
	    	// al principio de la lista
	    	Pcache_line new_item = malloc(sizeof(cache_line));
	    	new_item->tag = tag;
	    	new_item->dirty = 0;

	    	// inserta el nuevo elemento
	    	insert(&(c->LRU_head[index]), &(c->LRU_tail[index]), new_item);

	    	// Aumenta el contador de número de nodos
	    	c->set_contents[index]++; 
	    }
	  }
	}
}



void perform_access_data_load(
	cache *c, 
	unsigned addr, 
	int index,
	int block_size_in_words,
	unsigned int tag) 
{
	cache_stat_data.accesses++;

  if(c->LRU_head[index] == NULL){  // Lista vacía
      
      cache_stat_data.misses++;
      cache_stat_data.demand_fetches += block_size_in_words;
      
      Pcache_line new_item = malloc(sizeof(cache_line));
      
      new_item->tag = tag;
      new_item->dirty = 0;
      c->set_contents[index] = 1;
      insert(&c->LRU_head[index], &c->LRU_tail[index], new_item);

  } else { // Si la lista no está vacía
  	
    if(c->set_contents[index] == c->associativity){
      // Si no hay espacio en la lista

      Pcache_line cl = c->LRU_head[index];
      int tag_found = FALSE; // Si encuentra el tag en algún nodo
      for(int i = 0; i < c->set_contents[index]; i ++){
        if(cl->tag == tag) {
          tag_found = TRUE;
          break;
        }
        // if(cl->LRU_next == NULL) break;
      	cl = cl->LRU_next; // se mueve al siguiente nodo
      }

      if(tag_found){ // hit
      	// si encontró el tag, se debe pasar el nodo al principio de la lista
      	delete(&c->LRU_head[index], &c->LRU_tail[index], cl);
      	insert(&c->LRU_head[index], &c->LRU_tail[index], cl);

      } else { // miss
      	cache_stat_data.demand_fetches += block_size_in_words;
      	cache_stat_data.misses++;
      	cache_stat_data.replacements++;
      	// Si el tag no estaba, debe eliminar el nodo de la cola
      	// y crear uno nuevo para insertar en el head
      	Pcache_line new_item = malloc(sizeof(cache_line));
      	new_item->tag = tag;
      	new_item->dirty = 0;

      	if(c->LRU_tail[index]->dirty) { // Hay que guardar bloque
      	    cache_stat_data.copies_back += block_size_in_words;
      	}

      	// elimina último elemento
      	delete(&c->LRU_head[index], &c->LRU_tail[index], c->LRU_tail[index]);
      	// inserta el nuevo elemento
      	insert(&(c->LRU_head[index]), &(c->LRU_tail[index]), new_item);
      }


    } else { // Si sí hay espacio en la lista
      // Recorrer lista para ver si está el tag
      int tag_found = FALSE; // Si encuentra el tag en algún nodo
      Pcache_line cl = c->LRU_head[index];
      
      for(int i = 0; i < c->set_contents[index]; i ++){
        if(cl->tag == tag) {
          tag_found = TRUE;
          break;
        }
        // if(cl->LRU_next == NULL) break;
      	cl = cl->LRU_next; // se mueve al siguiente nodo
      }

      if(tag_found){ // hit
      	// si encontró el tag, se debe pasar el nodo al principio de la lista
      	delete(&c->LRU_head[index], &c->LRU_tail[index], cl);
      	insert(&c->LRU_head[index], &c->LRU_tail[index], cl);

      } else { // miss
      	
      	cache_stat_data.demand_fetches += block_size_in_words;
      	cache_stat_data.misses++;
      	// Si el tag no estaba, crea nuevo elemento para insertar 
      	// al principio de la lista
      	Pcache_line new_item = malloc(sizeof(cache_line));
      	new_item->tag = tag;
      	new_item->dirty = 0;

      	// inserta el nuevo elemento
      	insert(&(c->LRU_head[index]), &(c->LRU_tail[index]), new_item);

      	// Aumenta el contador de número de nodos
      	c->set_contents[index]++; 
      }
    }
  }
}


void perform_access_data_store(
	cache *c, 
	unsigned addr, 
	int index,
	int block_size_in_words,
	unsigned int tag) 
{
	cache_stat_data.accesses++;

	if(c->LRU_head[index] == NULL){  // Lista vacía

		// En política WNA, se actualiza la información directamente en la memoria
        if(cache_writealloc==0){
         cache_stat_data.copies_back += 1;
         cache_stat_data.misses++;
        }
	    
	    else{        
	    cache_stat_data.misses++;
	    cache_stat_data.demand_fetches += block_size_in_words;
	    
	    Pcache_line new_item = malloc(sizeof(cache_line));
	    
	    new_item->tag = tag;
	    new_item->dirty = 1;

	    // En política WT, se debe actualizar simultáneamente en memoria la palabra modificada en cache
        if(cache_writeback==0){
         cache_stat_data.copies_back += 1;
         new_item->dirty = 0;     
        }              

	    c->set_contents[index] = 1;
	    insert(&c->LRU_head[index], &c->LRU_tail[index], new_item);
	    }

	} else { // Si la lista no está vacía		      

	  if(c->set_contents[index] == c->associativity){	  	
	    // Si no hay espacio en la lista

	    Pcache_line cl = c->LRU_head[index];
	    int tag_found = FALSE; // Si encuentra el tag en algún nodo
	    for(int i = 0; i < c->set_contents[index]; i ++){
	      if(cl->tag == tag) {
	        tag_found = TRUE;
	        break;
	      }
	      // if(cl->LRU_next == NULL) break;	
	    	cl = cl->LRU_next; // se mueve al siguiente nodo
	    }

	    if(tag_found){ // hit
	    	// si encontró el tag, se debe pasar el nodo al principio de la lista
	    	delete(&c->LRU_head[index], &c->LRU_tail[index], cl);
	    	insert(&c->LRU_head[index], &c->LRU_tail[index], cl);

	    	// Ahora el bit está sucio
	    	c->LRU_head[index]->dirty = 1;

	    	// En política WT, se debe actualizar simultáneamente en memoria la palabra modificada en cache
            if(cache_writeback==0){
               cache_stat_data.copies_back += 1; 
               // El dirty bit es 0, pues la palabra ya está actualizada en memoria principal
               c->LRU_head[index]->dirty=0;
            }

	    } else { // miss

	    	// En política WNA, se actualiza la información directamente en la memoria
	    	if(cache_writealloc==0){
	    	   cache_stat_data.copies_back += 1;
        	   cache_stat_data.misses++;
            }
	    	
	    	else{
	    	cache_stat_data.demand_fetches += block_size_in_words;
	    	cache_stat_data.misses++;
	    	cache_stat_data.replacements++;
	    	// Si el tag no estaba, debe eliminar el nodo de la cola
	    	// y crear uno nuevo para insertar en el head
	    	Pcache_line new_item = malloc(sizeof(cache_line));
	    	new_item->tag = tag;
	    	new_item->dirty = 1;

	    	if(c->LRU_tail[index]->dirty) { // Hay que guardar bloque
      	    cache_stat_data.copies_back += block_size_in_words;
      	    }
            
            // En política WT, se debe actualizar simultáneamente en memoria la palabra modificada en cache
      	    if(cache_writeback==0){    
               cache_stat_data.copies_back += 1;     
               new_item->dirty = 0;
            }

	    	// elimina último elemento
	    	delete(&c->LRU_head[index], &c->LRU_tail[index], c->LRU_tail[index]);
	    	// inserta el nuevo elemento
	    	insert(&(c->LRU_head[index]), &(c->LRU_tail[index]), new_item);
	        }
	    }


	  } else { // Si sí hay espacio en la lista
	    // Recorrer lista para ver si está el tag
	    int tag_found = FALSE; // Si encuentra el tag en algún nodo
	    Pcache_line cl = c->LRU_head[index];
	    
	    for(int i = 0; i < c->set_contents[index]; i ++){
	      if(cl->tag == tag) {
	        tag_found = TRUE;
	        break;
	      }
	      // if(cl->LRU_next == NULL) break;
	    	cl = cl->LRU_next; // se mueve al siguiente nodo
	    }

	    if(tag_found){ // hit
	    	// si encontró el tag, se debe pasar el nodo al principio de la lista
	    	delete(&c->LRU_head[index], &c->LRU_tail[index], cl);
	    	insert(&c->LRU_head[index], &c->LRU_tail[index], cl);

	    	// Ahora el bit está sucio
	    	c->LRU_head[index]->dirty = 1;

	    	// En política WT, se debe actualizar simultáneamente en memoria la palabra modificada en cache
	    	if(cache_writeback==0){
               cache_stat_data.copies_back += 1;
               // Se pone en 0 el dirty bit, pues ya se actualizo en memoria principal la palabra
               c->LRU_head[index]->dirty = 0;
            } 

	    } else { // miss	    	
	    	if(cache_writealloc==0){
              cache_stat_data.misses++;
              cache_stat_data.copies_back += 1;
            }

            else{
	    	cache_stat_data.demand_fetches += block_size_in_words;
	    	cache_stat_data.misses++;
	    	// Si el tag no estaba, crea nuevo elemento para insertar 
	    	// al principio de la lista
	    	Pcache_line new_item = malloc(sizeof(cache_line));
	    	new_item->tag = tag;
	    	new_item->dirty = 1;

	    	// En política WT, se debe actualizar simultáneamente en memoria la palabra modificada en cache
	    	if(cache_writeback==0){
              cache_stat_data.copies_back += 1;
              // El dirty bit es 0, pues ya se actualizó en memoria principal la palabra
              new_item->dirty = 0;         
            }              

	    	// inserta el nuevo elemento
	    	insert(&(c->LRU_head[index]), &(c->LRU_tail[index]), new_item);

	    	// Aumenta el contador de número de nodos
	    	c->set_contents[index]++; 
	        }
	    }
	  }
	}
}


void perform_access_aux_unified(cache *c, unsigned addr, unsigned access_type){
  static int nl=0;
  int index;  // para acceder a la linea correspondiente en el set
  unsigned int bitsSet, bitsOffset,tagMask,tag;
  int block_size_in_words = cache_block_size/WORD_SIZE;

  // Calcula tag
  bitsSet = LOG2(c->n_sets);
  bitsOffset = LOG2(cache_block_size);
  tagMask = MASK_ORIG << (bitsOffset + bitsSet);
  tag = addr & tagMask;

  index = (addr & c->index_mask) >> c->index_mask_offset;
  
  nl++;
  
  switch(access_type){
      case TRACE_INST_LOAD:
          perform_access_instruction_load(c, addr, index, block_size_in_words, tag);
          break;

      case TRACE_DATA_LOAD:
          perform_access_data_load(c, addr, index, block_size_in_words, tag);
          break;

      case TRACE_DATA_STORE:          
          perform_access_data_store(c, addr, index, block_size_in_words, tag);
          break;
  }
}





void perform_access_aux_split(cache *c1, cache *c2, unsigned addr, unsigned access_type){
  static int nl=0;
  int index_c1, index_c2;  // para acceder a la linea correspondiente en el set
  unsigned int bitsSet_c1, bitsSet_c2, bitsOffset, tagMask_c1, tagMask_c2, tag_c1, tag_c2;
  int block_size_in_words = cache_block_size/WORD_SIZE;

  // Calcula tag
  bitsSet_c1 = LOG2(c1->n_sets);
  bitsSet_c2 = LOG2(c2->n_sets);
  bitsOffset = LOG2(cache_block_size);
  tagMask_c1 = MASK_ORIG << (bitsOffset + bitsSet_c1);
  tagMask_c2 = MASK_ORIG << (bitsOffset + bitsSet_c2);
  tag_c1 = addr & tagMask_c1;
  tag_c2 = addr & tagMask_c2;
  index_c1 = (addr & c1->index_mask) >> c1->index_mask_offset;
  index_c2 = (addr & c2->index_mask) >> c2->index_mask_offset;
  nl++;
  
  switch(access_type){
      case TRACE_INST_LOAD:
          perform_access_instruction_load(c2, addr, index_c2, block_size_in_words, tag_c2);
          break;

      case TRACE_DATA_LOAD:
          perform_access_data_load(c1, addr, index_c1, block_size_in_words, tag_c1);
          break;

      case TRACE_DATA_STORE:          
          perform_access_data_store(c1, addr, index_c1, block_size_in_words, tag_c1);
          break;
  }
}


void perform_access(addr, access_type)
  unsigned addr, access_type;
{    
    if(cache_split){
      perform_access_aux_split(&c1, &c2, addr, access_type);  
    } else {
      perform_access_aux_unified(&c1, addr, access_type);
    }   
}
/************************************************************/

/************************************************************/
void flush()
{
  int block_size_in_words = cache_block_size/WORD_SIZE;
  Pcache_line cl;

  for(int i=0; i < c1.n_sets; i++){
  	cl = c1.LRU_head[i];
  	for(int j = 0; j < c1.set_contents[i]; j++){
  	  if(cl->dirty) cache_stat_data.copies_back += block_size_in_words;
  		cl = cl->LRU_next; // se mueve al siguiente nodo
  	}
  }

  // Si se tiene un caché dividido, tiene que vaciar el segundo también.
  if(cache_split) {
    for(int i=0; i < c2.n_sets; i++){
    	cl = c2.LRU_head[i];
    	for(int j = 0; j < c2.set_contents[i]; j++){
    	  if(cl->dirty) cache_stat_data.copies_back += block_size_in_words;
    		cl = cl->LRU_next; // se mueve al siguiente nodo
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
  printf("  demand fetch:\t%d\n", cache_stat_inst.demand_fetches + 
	 cache_stat_data.demand_fetches);
  printf("  copies back:\t%d\n", cache_stat_inst.copies_back +
	 cache_stat_data.copies_back);
}
/************************************************************/