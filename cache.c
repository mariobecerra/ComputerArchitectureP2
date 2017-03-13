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

void init_cache_aux(cache *c, int size){
  int bitsOffset, bitsSet; // No. de bits para direccionar offset y set
  int auxMask; // ayuda a generar la máscara del set
    
  /* initialize the cache, and cache statistics data structures */
  c->size = size;
  c->associativity = cache_assoc;
  c->n_sets = c->size /(words_per_block*WORD_SIZE);
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
  printf("Checando tipo de caché\n");
  if(cache_split){
    printf("Inicializando...");
    init_cache_aux(&c1, cache_dsize);
    printf("Caché 1 listo...");
    init_cache_aux(&c2, cache_isize);  
    printf("Caché 2 listo.\n");
  } else {
    printf("Inicializando...");
    init_cache_aux(&c1, cache_usize);  
    printf("Caché listo.\n");
  }

  printf("\n\nInicializando estadísticas...");
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
  printf("Listo\n\n");
}

/************************************************************/

/************************************************************/

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
  // printf("Index: %d\n", index);
  nl++;
  
  switch(access_type){
      case TRACE_INST_LOAD:
          // printf("\n\tCaso: %d (trace inst load)\n", TRACE_INST_LOAD);
          cache_stat_inst.accesses++;

          if(c->LRU_head[index] == NULL){  // Lista vacía
              // printf("\n\t\tCompulsory miss\n");
              cache_stat_inst.misses++;
              cache_stat_inst.demand_fetches += block_size_in_words;
              // c->LRU_head[index] = malloc(sizeof(cache_line)); 
              // c->LRU_head[index]->tag = tag;
              // c->LRU_head[index]->dirty = 0;
              // printf("\n\t\t\t\tVa a crear nuevo item\n");
              Pcache_line new_item = (Pcache_line)malloc(sizeof(cache_line));
              // printf("\n\t\t\t\tCreó nuevo item\n");
              new_item->tag = tag;
              new_item->dirty = 0;
              c->set_contents[index] = 1;
              insert(&c->LRU_head[index], &c->LRU_tail[index], new_item);
          } else { // Si la lista no está vacía
          	// printf("\n\tLista no vacía\n");

            if(c->set_contents[index] == c->associativity){
            	// printf("\n\tSet contents (número de nodos): %d\n", c->set_contents[index]);
            	// printf("\n\t\tLista llena.\n");
              
              // Si no hay espacio en la lista
              // printf("\n\t\t\tAsigna a cl\n");
              Pcache_line cl = c->LRU_head[index];
              int tag_found = FALSE; // Si encuentra el tag en algún nodo
              
              // printf("\n\t\t\tEntra a for\n");
              // for(int i = 0; i < c->set_contents[index]; i ++){
              // 	if(cl->LRU_next == NULL) break;
              // 	cl = cl->LRU_next; // se mueve al siguiente nodo
              //   if(cl->tag == tag) {
              //     tag_found = TRUE;
              //     // printf("\n\t\t\tSe va a salir de la lista por tag\n");
              //     break;
              //   }
              // }
              for(int i = 0; i < c->set_contents[index]; i ++){
                if(cl->tag == tag) {
                  tag_found = TRUE;
                  // printf("\n\t\t\tSe va a salir de la lista por tag\n");
                  break;
                }
                if(cl->LRU_next == NULL) break;
              	cl = cl->LRU_next; // se mueve al siguiente nodo
              }
              // printf("\n\t\t\tSale de for\n");

              if(tag_found){ // hit
              	// printf("\n\t\t\tFue hit\n");
              	// si encontró el tag, se debe pasar el nodo al principio de la lista
              	delete(&c->LRU_head[index], &c->LRU_tail[index], cl);
              	insert(&c->LRU_head[index], &c->LRU_tail[index], cl);

              } else { // miss
              	// printf("\n\t\t\tFue miss\n");
              	cache_stat_inst.demand_fetches += block_size_in_words;
              	cache_stat_inst.misses++;
              	cache_stat_inst.replacements++;
              	// Si el tag no estaba, debe eliminar el nodo de la cola
              	// y crear uno nuevo para insertar en el head

              	// printf("\n\t\t\t\tVa a crear nuevo item\n");
              	Pcache_line new_item = (Pcache_line)malloc(sizeof(cache_line));
              	// printf("\n\t\t\t\tCreó nuevo item\n");
              	new_item->tag = tag;
              	new_item->dirty = 0;
              	// new_item->LRU_next = (Pcache_line)NULL;
              	// new_item->LRU_prev = (Pcache_line)NULL;

              	// inserta el nuevo elemento
              	// printf("\n\t\t\t\tVa a insertar nuevo item\n");
              	insert(&c->LRU_head[index], &c->LRU_tail[index], new_item);
              	// printf("\n\t\t\t\tInsertó nuevo item.\n");
              	// elimina último elemento
              	// printf("\n\t\t\t\tTag de cola: %d\n", c->LRU_tail[index]->tag);
              	// printf("\n\t\t\t\tVa a eliminar cola\n");
              	delete(&c->LRU_head[index], &c->LRU_tail[index], c->LRU_tail[index]);
              	// printf("\n\t\t\t\tEliminó cola\n");
              }


            } else { // Si sí hay espacio en la lista
              // Recorrer lista para ver si está el tag
              int tag_found = FALSE; // Si encuentra el tag en algún nodo
              Pcache_line cl = c->LRU_head[index];
              // printf("\n\tSet contents (número de nodos): %d\n", c->set_contents[index]);
              // printf("\n\t\tHay espacio en la lista.\n");
              
              for(int i = 0; i < c->set_contents[index]; i ++){
                if(cl->tag == tag) {
                  tag_found = TRUE;
                  // printf("\n\t\t\tSe va a salir de la lista por tag\n");
                  break;
                }
                if(cl->LRU_next == NULL) break;
              	cl = cl->LRU_next; // se mueve al siguiente nodo
              }

              if(tag_found){ 
              	// printf("\n\t\tEncontró tag (hit).\n");
              	// si encontró el tag, se debe pasar el nodo al principio de la lista
              	// printf("\n\t\tVa a mover el elemento al principio de la lista.\n");
              	delete(&c->LRU_head[index], &c->LRU_tail[index], cl);
              	insert(&c->LRU_head[index], &c->LRU_tail[index], cl);
              	// printf("\n\t\tYa lo movió.\n");

              } else {
              	// printf("\n\t\tNo encontró tag (miss).\n");
              	cache_stat_inst.demand_fetches += block_size_in_words;
              	cache_stat_inst.misses++;
              	// Si el tag no estaba, crea nuevo elemento para insertar 
              	// al principio de la lista
              	Pcache_line new_item = (Pcache_line)malloc(sizeof(cache_line));
              	new_item->tag = tag;
              	new_item->dirty = 0;
              	// new_item->LRU_next = (Pcache_line)NULL;
              	// new_item->LRU_prev = (Pcache_line)NULL;

              	// inserta el nuevo elemento
              	// printf("\n\t\t\t\tTag de cola: %d\n", c->LRU_tail[index]->tag);
              	insert(&(c->LRU_head[index]), &(c->LRU_tail[index]), new_item);
              	// printf("\n\t\tInsertó nuevo elemento.\n");

              	// Aumenta el contador de número de nodos
              	c->set_contents[index]++; 
              	// printf("\n\t\t\tAhora tiene %d nodos.\n", c->set_contents[index]);
              }


            }

          }
          break;





            // printf("\nRecorre lista\n");
            // Pcache_line cl = c->LRU_head[index];
            // int tag_found = FALSE;
            // int num_nodes = 1;
            // while(TRUE){
            //   printf("\n\tRecorriendo lista: %d\n", num_nodes - 1);
            //   if(cl->LRU_next == NULL) {
            //     printf("\n\t\tSe va a salir de la lista por NULL\n");
            //     break;
            //   } else {
            //     num_nodes++;  
            //     cl = cl->LRU_next; // se mueve al siguiente nodo
            //     if(cl->tag == tag) {
            //       tag_found = TRUE;
            //       printf("\n\t\tSe va a salir de la lista por tag\n");
            //       break;
            //     }
            //   }
            // }
            // if(!tag_found){ // Miss
            //   printf("\nHubo miss (no encontró tag)\n");
            //   cache_stat_inst.misses++;
            //   cache_stat_inst.demand_fetches += block_size_in_words;
            //   if(num_nodes == c->associativity){ // Si no hay espacio en la lista
            //     // reemplazar
            //     printf("\nReemplazar\n");
            //     cache_stat_inst.replacements++;

            //     // si el dirty bit de la cola está sucio, escribe en memoria
            //     printf("si el dirty bit de la cola está sucio, escribe en memoria\n");
            //     if(c->LRU_tail[index]->dirty) cache_stat_inst.copies_back++; 
            //     printf("bebebebe");

            //     // Elimina la cola
            //     printf("\nVa a eliminar...");
            //     delete(&(c->LRU_head[index]), &(c->LRU_tail[index]), c->LRU_tail[index]);
            //     printf("Eliminó\n");

            //     // Crea nuevo elemento para insertar al principio de la lista
            //     Pcache_line new_item = (Pcache_line)malloc(sizeof(cache_line));
            //     new_item->tag = tag;
            //     new_item->dirty = 0;
            //     new_item->LRU_next = (Pcache_line)NULL;
            //     new_item->LRU_prev = (Pcache_line)NULL;
            //     // inserta el nuevo elemento
            //     insert(&(c->LRU_head[index]), &(c->LRU_tail[index]), new_item);

            //   } else { // Si sí hay espacio en la lista
            //     // agregar nodo
            //     printf("\nVa a agregar nodo\n");

            //     // Crea nuevo elemento para insertar al principio de la lista
            //     Pcache_line new_item = (Pcache_line)malloc(sizeof(cache_line));
            //     new_item->tag = tag;
            //     new_item->dirty = 0;
            //     new_item->LRU_next = (Pcache_line)NULL;
            //     new_item->LRU_prev = (Pcache_line)NULL;

            //     // inserta el nuevo elemento
            //     insert(&(c->LRU_head[index]), &(c->LRU_tail[index]), new_item);
            //   }  
            //} 

      case TRACE_DATA_LOAD:
          // printf("\n\tCaso: %d (trace data load)\n", TRACE_DATA_LOAD);
          cache_stat_data.accesses++;

          if(c->LRU_head[index] == NULL){  // Lista vacía
              // printf("\n\tCompulsory miss\n");
              cache_stat_data.misses++;
              cache_stat_data.demand_fetches += block_size_in_words;
              // c->LRU_head[index] = malloc(sizeof(cache_line)); 
              // c->LRU_head[index]->tag = tag;
              // c->LRU_head[index]->dirty = 0;
              // c->set_contents[index] = 1;
              // printf("\n\t\t\t\tVa a crear nuevo item\n");
              Pcache_line new_item = (Pcache_line)malloc(sizeof(cache_line));
              // printf("\n\t\t\t\tCreó nuevo item\n");
              new_item->tag = tag;
              new_item->dirty = 0;
              c->set_contents[index] = 1;
              insert(&c->LRU_head[index], &c->LRU_tail[index], new_item);

          } else { // Si la lista no está vacía
          	// printf("\n\tLista no vacía\n");
            if(c->set_contents[index] == c->associativity){
            	// printf("\n\t\tLista llena.\n");
              // Si no hay espacio en la lista
              Pcache_line cl = c->LRU_head[index];
              int tag_found = FALSE; // Si encuentra el tag en algún nodo
              // for(int i = 0; i < c->set_contents[index]; i ++){
              // 	if(cl->LRU_next == NULL) break;
              // 	cl = cl->LRU_next; // se mueve al siguiente nodo
              //   if(cl->tag == tag) {
              //     tag_found = TRUE;
              //     printf("\n\t\t\tSe va a salir de la lista por tag\n");
              //     break;
              //   }
              // }
              for(int i = 0; i < c->set_contents[index]; i ++){
                if(cl->tag == tag) {
                  tag_found = TRUE;
                  // printf("\n\t\t\tSe va a salir de la lista por tag\n");
                  break;
                }
                if(cl->LRU_next == NULL) break;
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
              	Pcache_line new_item = (Pcache_line)malloc(sizeof(cache_line));
              	new_item->tag = tag;
              	new_item->dirty = 0;
              	new_item->LRU_next = (Pcache_line)NULL;
              	new_item->LRU_prev = (Pcache_line)NULL;

              	// elimina último elemento
              	delete(&c->LRU_head[index], &c->LRU_tail[index], c->LRU_tail[index]);
              	// inserta el nuevo elemento
              	insert(&(c->LRU_head[index]), &(c->LRU_tail[index]), new_item);
              }


            } else { // Si sí hay espacio en la lista
              // Recorrer lista para ver si está el tag
              int tag_found = FALSE; // Si encuentra el tag en algún nodo
              Pcache_line cl = c->LRU_head[index];
              // printf("\n\tSet contents (número de nodos): %d\n", c->set_contents[index]);
              // printf("\n\t\tHay espacio en la lista.\n");
              
              for(int i = 0; i < c->set_contents[index]; i ++){
                if(cl->tag == tag) {
                  tag_found = TRUE;
                  // printf("\n\t\t\tSe va a salir de la lista por tag\n");
                  break;
                }
                if(cl->LRU_next == NULL) break;
              	cl = cl->LRU_next; // se mueve al siguiente nodo
              }

              if(tag_found){ 
              	// si encontró el tag, se debe pasar el nodo al principio de la lista
              	delete(&c->LRU_head[index], &c->LRU_tail[index], cl);
              	insert(&c->LRU_head[index], &c->LRU_tail[index], cl);

              } else {
              	cache_stat_data.demand_fetches += block_size_in_words;
              	cache_stat_data.misses++;
              	// Si el tag no estaba, crea nuevo elemento para insertar 
              	// al principio de la lista
              	Pcache_line new_item = (Pcache_line)malloc(sizeof(cache_line));
              	new_item->tag = tag;
              	new_item->dirty = 0;
              	new_item->LRU_next = (Pcache_line)NULL;
              	new_item->LRU_prev = (Pcache_line)NULL;

              	// inserta el nuevo elemento
              	// printf("\n\t\t\t\tTag de cola: %d\n", c->LRU_tail[index]->tag);
              	insert(&(c->LRU_head[index]), &(c->LRU_tail[index]), new_item);

              	// Aumenta el contador de número de nodos
              	c->set_contents[index]++; 
              }


            }
          }

          break;



          // cache_stat_data.accesses++;
          // if(c->LRU_head[index] == NULL){  // Compulsory miss
          //     printf("\n\tCompulsory miss\n");
          //     cache_stat_data.misses++;
          //     c->LRU_head[index]=malloc(sizeof(cache_line)); 
          //     c->LRU_head[index]->tag=tag;
          //     c->LRU_head[index]->dirty=0;
          //     cache_stat_data.demand_fetches+=block_size_in_words;
          // } else {
          //   printf("\n\tRecorre lista\n");
          //   Pcache_line cl = c->LRU_head[index];
          //   int tag_found = FALSE;
          //   int num_nodes = 1;
          //   while(TRUE){
          //     printf("\n\t\tRecorriendo lista: %d\n", num_nodes - 1);
          //     if(cl->LRU_next == NULL) {
          //       printf("\n\t\t\tSe va a salir de la lista por NULL\n");
          //       break;
          //     } else {
          //       num_nodes++;  
          //       cl = cl->LRU_next; // se mueve al siguiente nodo
          //       if(cl->tag == tag) {
          //         tag_found = TRUE;
          //         printf("\n\t\t\tSe va a salir de la lista por tag\n");
          //         break;
          //       }
          //     }
          //   }
          //   if(!tag_found){ // Miss
          //     printf("\n\tHubo miss (no encontró tag)\n");
          //     cache_stat_data.misses++;
          //     cache_stat_data.demand_fetches += block_size_in_words;
          //     if(num_nodes == c->associativity){ // Si no hay espacio en la lista
          //       // reemplazar
          //       printf("\n\t\tVa a reemplazar\n");
          //       cache_stat_data.replacements++;

          //       // si el dirty bit de la cola está sucio, escribe en memoria
          //       if(c->LRU_tail[index]->dirty) cache_stat_data.copies_back++; 

          //       // Elimina la cola
          //       delete(&(c->LRU_head[index]), &(c->LRU_tail[index]), c->LRU_tail[index]);

          //       // Crea nuevo elemento para insertar al principio de la lista
          //       Pcache_line new_item = (Pcache_line)malloc(sizeof(cache_line));
          //       new_item->tag = tag;
          //       new_item->dirty = 0;
          //       new_item->LRU_next = (Pcache_line)NULL;
          //       new_item->LRU_prev = (Pcache_line)NULL;
          //       // inserta el nuevo elemento
          //       insert(&(c->LRU_head[index]), &(c->LRU_tail[index]), new_item);

          //     } else { // Si sí hay espacio en la lista
          //       // agregar nodo
          //       printf("\n\t\tVa a agregar nodo\n");
          //       // Crea nuevo elemento para insertar al principio de la lista
          //       Pcache_line new_item = (Pcache_line)malloc(sizeof(cache_line));
          //       new_item->tag = tag;
          //       new_item->dirty = 0;
          //       new_item->LRU_next = (Pcache_line)NULL;
          //       new_item->LRU_prev = (Pcache_line)NULL;

          //       // inserta el nuevo elemento
          //       insert(&(c->LRU_head[index]), &(c->LRU_tail[index]), new_item);
          //     }  
          //   } 
          // }
          // break;

//           cache_stat_data.accesses++;
//           if(c->LRU_head[index]==NULL){  // Compulsory miss
//               cache_stat_data.misses++;
//               c->LRU_head[index]=malloc(sizeof(cache_line)); 
//               c->LRU_head[index]->tag=tag;
//               c->LRU_head[index]->dirty=0;
//               cache_stat_data.demand_fetches+=block_size_in_words;
//           } else if(c->LRU_head[index]->tag!=tag){  // Cache miss
//               if(c->LRU_head[index]->dirty) { // Hay que guardar bloque
//                   cache_stat_data.copies_back+=block_size_in_words;
//               }
//               cache_stat_data.misses++;
//               cache_stat_data.replacements++;
//               cache_stat_data.demand_fetches+=block_size_in_words;
//               c->LRU_head[index]->tag=tag;
//               c->LRU_head[index]->dirty=0;
//           }
//           break;

      case TRACE_DATA_STORE:
          // printf("\n\tCaso: %d (trace data store)\n", TRACE_DATA_STORE);
          cache_stat_data.accesses++;

          if(c->LRU_head[index] == NULL){  // Lista vacía
              // printf("\n\tCompulsory miss\n");
              cache_stat_data.misses++;
              cache_stat_data.demand_fetches += block_size_in_words;
              // c->LRU_head[index] = malloc(sizeof(cache_line)); 
              // c->LRU_head[index]->tag = tag;
              // c->LRU_head[index]->dirty = 1;
              // c->set_contents[index] = 1;
              // printf("\n\t\t\t\tVa a crear nuevo item\n");
              Pcache_line new_item = (Pcache_line)malloc(sizeof(cache_line));
              // printf("\n\t\t\t\tCreó nuevo item\n");
              new_item->tag = tag;
              new_item->dirty = 0;
              c->set_contents[index] = 1;
              insert(&c->LRU_head[index], &c->LRU_tail[index], new_item);
          } else { // Si la lista no está vacía
          	// printf("\n\tLista no vacía\n");

            if(c->set_contents[index] == c->associativity){
            	// printf("\n\t\tLista llena.\n");
              // Si no hay espacio en la lista
              Pcache_line cl = c->LRU_head[index];
              int tag_found = FALSE; // Si encuentra el tag en algún nodo
              // for(int i = 0; i < c->set_contents[index]; i ++){
              // 	if(cl->LRU_next == NULL) break;
              // 	cl = cl->LRU_next; // se mueve al siguiente nodo
              //   if(cl->tag == tag) {
              //     tag_found = TRUE;
              //     printf("\n\t\t\tSe va a salir de la lista por tag\n");
              //     break;
              //   }
              // }
              for(int i = 0; i < c->set_contents[index]; i ++){
                if(cl->tag == tag) {
                  tag_found = TRUE;
                  // printf("\n\t\t\tSe va a salir de la lista por tag\n");
                  break;
                }
                if(cl->LRU_next == NULL) break;
              	cl = cl->LRU_next; // se mueve al siguiente nodo
              }

              if(tag_found){ // hit
              	// si encontró el tag, se debe pasar el nodo al principio de la lista
              	delete(&c->LRU_head[index], &c->LRU_tail[index], cl);
              	insert(&c->LRU_head[index], &c->LRU_tail[index], cl);

              	// Hacer if de si el bit estaba clean, ponerlo dirty

              } else { // miss
              	cache_stat_data.demand_fetches += block_size_in_words;
              	cache_stat_data.misses++;
              	cache_stat_data.replacements++;
              	// Si el tag no estaba, debe eliminar el nodo de la cola
              	// y crear uno nuevo para insertar en el head
              	Pcache_line new_item = (Pcache_line)malloc(sizeof(cache_line));
              	new_item->tag = tag;
              	new_item->dirty = 1;
              	new_item->LRU_next = (Pcache_line)NULL;
              	new_item->LRU_prev = (Pcache_line)NULL;

              	// elimina último elemento
              	delete(&c->LRU_head[index], &c->LRU_tail[index], c->LRU_tail[index]);
              	// inserta el nuevo elemento
              	insert(&(c->LRU_head[index]), &(c->LRU_tail[index]), new_item);

              	// Hacer if de si el bit estaba dirty, aumentar el número de copiesback

              }


            } else { // Si sí hay espacio en la lista
              // Recorrer lista para ver si está el tag
              int tag_found = FALSE; // Si encuentra el tag en algún nodo
              Pcache_line cl = c->LRU_head[index];
              // printf("\n\tSet contents (número de nodos): %d\n", c->set_contents[index]);
              // printf("\n\t\tHay espacio en la lista.\n");
              
              for(int i = 0; i < c->set_contents[index]; i ++){
                if(cl->tag == tag) {
                  tag_found = TRUE;
                  // printf("\n\t\t\tSe va a salir de la lista por tag\n");
                  break;
                }
                if(cl->LRU_next == NULL) break;
              	cl = cl->LRU_next; // se mueve al siguiente nodo
              }

              if(tag_found){ 
              	// si encontró el tag, se debe pasar el nodo al principio de la lista
              	delete(&c->LRU_head[index], &c->LRU_tail[index], cl);
              	insert(&c->LRU_head[index], &c->LRU_tail[index], cl);

              	// Hacer if de si el bit estaba clean, ponerlo dirty

              } else {
              	cache_stat_data.demand_fetches += block_size_in_words;
              	cache_stat_data.misses++;
              	// Si el tag no estaba, crea nuevo elemento para insertar 
              	// al principio de la lista
              	Pcache_line new_item = (Pcache_line)malloc(sizeof(cache_line));
              	new_item->tag = tag;
              	new_item->dirty = 0;
              	new_item->LRU_next = (Pcache_line)NULL;
              	new_item->LRU_prev = (Pcache_line)NULL;

              	// inserta el nuevo elemento
              	insert(&(c->LRU_head[index]), &(c->LRU_tail[index]), new_item);

              	// Hacer if de si el bit estaba dirty, aumentar el número de copiesback

              	// Aumenta el contador de número de nodos
              	c->set_contents[index]++; 
              }


            }
          }

          break;



          // cache_stat_data.accesses++;

          // if(c->LRU_head[index] == NULL){  // Compulsory miss
          //   printf("\n\tCompulsory miss\n");
          //     cache_stat_data.misses++;
          //     c->LRU_head[index]=malloc(sizeof(cache_line)); 
          //     c->LRU_head[index]->tag = tag;
          //     c->LRU_head[index]->dirty = 1;
          //     cache_stat_data.demand_fetches += block_size_in_words;
          // } else {
          //   printf("\n\tRecorre lista\n");
          //   Pcache_line cl = c->LRU_head[index];
          //   int tag_found = FALSE;
          //   int num_nodes = 1;
          //   while(TRUE){
          //     printf("\n\t\tRecorriendo lista: %d\n", num_nodes - 1);
          //     if(cl->LRU_next == NULL) {
          //       break;
          //     } else {
          //       num_nodes++;  
          //       cl = cl->LRU_next; // se mueve al siguiente nodo
          //       if(cl->tag == tag) {
          //         tag_found = TRUE;
          //         cl->dirty = 1;
          //         break;
          //       }
          //     }
          //   }
          //   if(!tag_found){ // Miss
          //     printf("\n\tHubo miss (no encontró tag)\n");
          //     cache_stat_data.misses++;
          //     cache_stat_data.demand_fetches += block_size_in_words;
          //     if(num_nodes == c->associativity){ // Si no hay espacio en la lista
          //       // reemplazar
          //       printf("\n\t\tVa a reemplazar\n");
          //       cache_stat_data.replacements++;

          //       // si el dirty bit de la cola está sucio, escribe en memoria
          //       if(c->LRU_tail[index]->dirty) cache_stat_data.copies_back++; 

          //       // Elimina la cola
          //       delete(&(c->LRU_head[index]), &(c->LRU_tail[index]), c->LRU_tail[index]);

          //       // Crea nuevo elemento para insertar al principio de la lista
          //       Pcache_line new_item = (Pcache_line)malloc(sizeof(cache_line));
          //       new_item->tag = tag;
          //       new_item->dirty = 1;
          //       new_item->LRU_next = (Pcache_line)NULL;
          //       new_item->LRU_prev = (Pcache_line)NULL;
          //       // inserta el nuevo elemento
          //       insert(&(c->LRU_head[index]), &(c->LRU_tail[index]), new_item);

          //     } else { // Si sí hay espacio en la lista
          //       // agregar nodo
          //       printf("\n\t\tVa a agregar nodo\n");
          //       // Crea nuevo elemento para insertar al principio de la lista
          //       Pcache_line new_item = (Pcache_line)malloc(sizeof(cache_line));
          //       new_item->tag = tag;
          //       new_item->dirty = 1;
          //       new_item->LRU_next = (Pcache_line)NULL;
          //       new_item->LRU_prev = (Pcache_line)NULL;

          //       // inserta el nuevo elemento
          //       insert(&(c->LRU_head[index]), &(c->LRU_tail[index]), new_item);
          //     }  
          //   } 
          // }
          // break;


          // cache_stat_data.accesses++;
          
          // if(c->LRU_head[index]==NULL){  // Compulsory miss
          //     cache_stat_data.misses++;
          //     c->LRU_head[index]=malloc(sizeof(cache_line));  
          //     c->LRU_head[index]->tag=tag;
          //     c->LRU_head[index]->dirty=1;
          //     cache_stat_data.demand_fetches+=block_size_in_words;
          // } else if(c->LRU_head[index]->tag!=tag){  // Cache miss
          //     if(c->LRU_head[index]->dirty) { // Hay que guardar bloque
          //         cache_stat_data.copies_back+=block_size_in_words;
          //     }
          //     cache_stat_data.misses++;
          //     cache_stat_data.replacements++;
          //     cache_stat_data.demand_fetches+=block_size_in_words;
          //     c->LRU_head[index]->tag=tag;
          //     c->LRU_head[index]->dirty=1;
          // }
          // else
          //     c->LRU_head[index]->dirty=1;
          // break;
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
          cache_stat_inst.accesses++;
          if(c2->LRU_head[index_c2]==NULL){  // Compulsory miss
              cache_stat_inst.misses++;
              c2->LRU_head[index_c2]=malloc(sizeof(cache_line));
              c2->LRU_head[index_c2]->tag = tag_c2;
              c2->LRU_head[index_c2]->dirty = 0;
              cache_stat_inst.demand_fetches+=block_size_in_words;
          } else if(c2->LRU_head[index_c2]->tag != tag_c2){  // Cache miss
              if(c2->LRU_head[index_c2]->dirty) { // Hay que guardar bloque
                  cache_stat_data.copies_back += block_size_in_words;
              }
              cache_stat_inst.misses++;
              cache_stat_inst.replacements++;
              cache_stat_inst.demand_fetches+=block_size_in_words;
              c2->LRU_head[index_c2]->tag = tag_c2;
              c2->LRU_head[index_c2]->dirty = 0;
          }
          break;
      case TRACE_DATA_LOAD:
          cache_stat_data.accesses++;
          
          if(c1->LRU_head[index_c1]==NULL){  // Compulsory miss
              cache_stat_data.misses++;
              c1->LRU_head[index_c1] = malloc(sizeof(cache_line));
              c1->LRU_head[index_c1]->tag = tag_c1;
              c1->LRU_head[index_c1]->dirty = 0;
              cache_stat_data.demand_fetches += block_size_in_words;
          } else if(c1->LRU_head[index_c1]->tag != tag_c1){  // Cache miss
              if(c1->LRU_head[index_c1]->dirty) { // Hay que guardar bloque
                  cache_stat_data.copies_back += block_size_in_words;
              }
              cache_stat_data.misses++;
              cache_stat_data.replacements++;
              cache_stat_data.demand_fetches+=block_size_in_words;
              c1->LRU_head[index_c1]->tag = tag_c1;
              c1->LRU_head[index_c1]->dirty = 0;
          }
          break;
      case TRACE_DATA_STORE:
          cache_stat_data.accesses++;
          
          if(c1->LRU_head[index_c1]==NULL){  // Compulsory miss
              cache_stat_data.misses++;
              c1->LRU_head[index_c1] = malloc(sizeof(cache_line));
              c1->LRU_head[index_c1]->tag = tag_c1;
              c1->LRU_head[index_c1]->dirty = 1;
              cache_stat_data.demand_fetches += block_size_in_words;
          } else if(c1->LRU_head[index_c1]->tag != tag_c1){  // Cache miss
              if(c1->LRU_head[index_c1]->dirty) { // Hay que guardar bloque
                  cache_stat_data.copies_back += block_size_in_words;
              }
              cache_stat_data.misses++;
              cache_stat_data.replacements++;
              cache_stat_data.demand_fetches += block_size_in_words;
              c1->LRU_head[index_c1]->tag = tag_c1;
              c1->LRU_head[index_c1]->dirty = 1;
          }
          else
              c1->LRU_head[index_c1]->dirty = 1;
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
  /* flush the cache */
    for(int i=0; i<c1.n_sets; i++){
        if(c1.LRU_head[i]!=NULL){
            if(c1.LRU_head[i]->dirty){
                cache_stat_data.copies_back+=block_size_in_words;
            }
        }
    }

    // Si se tiene un caché dividido, tiene que vaciar el segundo también.
    if(cache_split) {
      for(int i=0; i<c2.n_sets; i++){
        if(c2.LRU_head[i]!=NULL){
            if(c2.LRU_head[i]->dirty){
                cache_stat_data.copies_back+=block_size_in_words;
            }
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