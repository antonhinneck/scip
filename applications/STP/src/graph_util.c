/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2019 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not visit scip.zib.de.         */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   graph_util.c
 * @brief  includes graph utility methods for Steiner problems
 * @author Daniel Rehfeldt
 *
 * Graph utility methods for Steiner problems, such as CSR data structure
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

/*lint -esym(766,stdlib.h) -esym(766,malloc.h)         */
/*lint -esym(766,string.h) */

#define DHEAP_MAX_KEY  (1e20)
#define DHEAP_MIN_KEY  -(1e20)

#include "graph.h"
#include "portab.h"


/** CSR like graph storage */
struct compressed_sparse_storages_depository
{
   int*                  all_starts;         /**< all start positions (size datasize_max) */
   int*                  all_heads;          /**< all edge heads      (size datasize_max) */
   SCIP_Real*            all_costs;          /**< all edge costs      (size datasize_max) */
   int*                  csr_ptrsStart;      /**< gives start index to start of start array (size ncsrs_max + 1) */
   int*                  csr_ptrsData;       /**< gives start index to start of heads/costs arrays (size ncsrs_max + 1) */
   SCIP_Bool*            csr_isEmpty;        /**< per entry: is empty?       (size ncsrs_max) */
   int                   datasize_max;       /**< maximum size of depository */
   int                   ncsrs_curr;         /**< current number of CSRS */
   int                   ncsrs_max;          /**< maximum number of CSRS */
#ifndef NDEBUG
   int*                  csr_nedges;         /**< per entry: number of (directed!) edges (size ncsrs_max) */
   int*                  csr_nnodes;         /**< per entry: number of nodes (size ncsrs_max) */
#endif
};


/** gets top index */
static inline
int csrdepoGetTopIndex(
   const CSRDEPO*        depository          /**< the depository */
)
{
   assert(depository);
   assert(depository->ncsrs_curr >= 1);

   return (depository->ncsrs_curr - 1);
}


/** gets number of nodes of CSR stored at position 'index' */
static inline
int csrdepoGetNnodes(
   const CSRDEPO*        depository,         /**< the depository */
   int                   index               /**< the index of the CSR */
   )
{
   assert(index >= 0 && index < depository->ncsrs_curr);

#ifndef NDEBUG
   {
      const int nnodes = depository->csr_ptrsStart[index + 1] - depository->csr_ptrsStart[index] - 1;
      assert(nnodes == depository->csr_nnodes[index]);
      assert(nnodes >= 1);
   }
#endif

   return (depository->csr_ptrsStart[index + 1] - depository->csr_ptrsStart[index] - 1);
}


/** gets number of edges of CSR stored at position 'index' */
static inline
int csrdepoGetNedges(
   const CSRDEPO*        depository,         /**< the depository */
   int                   index               /**< the index of the CSR */
   )
{
   assert(index >= 0 && index < depository->ncsrs_curr);

#ifndef NDEBUG
   {
      const int nedges = depository->csr_ptrsData[index + 1] - depository->csr_ptrsData[index];
      assert(nedges == depository->csr_nedges[index]);
   }
#endif


   return (depository->csr_ptrsData[index + 1] - depository->csr_ptrsData[index]);
}


/** fills the CSR structure */
static inline
void csrdepoFillCSR(
   const CSRDEPO*        depository,         /**< the depository */
   int                   index,              /**< the index */
   CSR*                  csr                 /**< pointer to CSR struct to fill */
)
{
   const int nnodes = csrdepoGetNnodes(depository, index);
   const int nedges = csrdepoGetNedges(depository, index);
   const int ptr_start = depository->csr_ptrsStart[index];
   const int ptr_data = depository->csr_ptrsData[index];

   /* fill the entries of the CSR */
   csr->start = &(depository->all_starts[ptr_start]);
   csr->head = &(depository->all_heads[ptr_data]);
   csr->cost = &(depository->all_costs[ptr_data]);
   csr->nedges = nedges;
   csr->nnodes = nnodes;
}


/*
 * CSR Depository
 */

/** initializes CSR depository */
SCIP_RETCODE graph_csrdepo_init(
   SCIP*                 scip,               /**< SCIP data structure */
   int                   ncsrs_max,          /**< maximum number of CSRs */
   int                   datasize_max,       /**< the maximum capacity */
   CSRDEPO**             depository          /**< the depository */
   )
{
   CSRDEPO* depo;

   assert(scip);
   assert(ncsrs_max >= 1);
   assert(ncsrs_max < datasize_max);

   SCIP_CALL( SCIPallocMemory(scip, depository) );

   depo = *depository;

   SCIP_CALL( SCIPallocMemoryArray(scip, &(depo->all_starts), datasize_max) );
   SCIP_CALL( SCIPallocMemoryArray(scip, &(depo->all_heads), datasize_max) );
   SCIP_CALL( SCIPallocMemoryArray(scip, &(depo->all_costs), datasize_max) );

   SCIP_CALL( SCIPallocMemoryArray(scip, &(depo->csr_ptrsStart), ncsrs_max + 1) );
   SCIP_CALL( SCIPallocMemoryArray(scip, &(depo->csr_ptrsData), ncsrs_max + 1) );
   SCIP_CALL( SCIPallocMemoryArray(scip, &(depo->csr_isEmpty), ncsrs_max) );

   depo->ncsrs_curr = 0;
   depo->ncsrs_max = ncsrs_max;
   depo->datasize_max = datasize_max;

#ifndef NDEBUG
   SCIP_CALL( SCIPallocMemoryArray(scip, &(depo->csr_nedges), ncsrs_max) );
   SCIP_CALL( SCIPallocMemoryArray(scip, &(depo->csr_nnodes), ncsrs_max) );

   for( int i = 0; i < ncsrs_max; ++i )
   {
      depo->csr_nedges[i] = -1;
      depo->csr_nnodes[i] = -1;
   }
#endif

   depo->all_starts[0] = 0;
   depo->csr_ptrsStart[0] = 0;
   depo->csr_ptrsData[0] = 0;

   assert(graph_csrdepo_isEmpty(*depository));

   return SCIP_OKAY;
}


/** frees CSR depository */
void graph_csrdepo_free(
   SCIP*                 scip,               /**< SCIP data structure */
   CSRDEPO**             depository          /**< the depository */
   )
{
   CSRDEPO* depo;

   assert(scip && depository && *depository);

   depo = *depository;

#ifndef NDEBUG
   SCIPfreeMemoryArray(scip, &(depo->csr_nnodes));
   SCIPfreeMemoryArray(scip, &(depo->csr_nedges));
#endif

   SCIPfreeMemoryArray(scip, &(depo->csr_isEmpty));
   SCIPfreeMemoryArray(scip, &(depo->csr_ptrsData));
   SCIPfreeMemoryArray(scip, &(depo->csr_ptrsStart));

   SCIPfreeMemoryArray(scip, &(depo->all_costs));
   SCIPfreeMemoryArray(scip, &(depo->all_heads));
   SCIPfreeMemoryArray(scip, &(depo->all_starts));

   SCIPfreeMemory(scip, depository);
}


/** gets CSR from depository */
void graph_csrdepo_getCSR(
   const CSRDEPO*        depository,         /**< the depository */
   int                   index,              /**< the index of the CSR to get */
   CSR*                  csr                 /**< pointer to CSR struct to fill */
   )
{
   assert(depository && csr);
   assert(index >= 0 && index < depository->ncsrs_curr);
   assert(!depository->csr_isEmpty[index]);

   csrdepoFillCSR(depository, index, csr);

   assert(csr->start[0] == 0);
}


/** gets current size of data (from all CSRs) in depository */
int graph_csrdepo_getDataSize(
   const CSRDEPO*        depository          /**< the depository */
   )
{
   assert(depository);
   assert(depository->ncsrs_curr >= 0);

   if( depository->ncsrs_curr == 0 )
   {
      return 0;
   }
   else
   {
      const int top_index = csrdepoGetTopIndex(depository);
      const int top_nedges = csrdepoGetNedges(depository, top_index);
      const int top_nnodes = csrdepoGetNnodes(depository, top_index);
      const int datasize_edges = depository->csr_ptrsData[top_index] + top_nedges;
      const int datasize_nodes = depository->csr_ptrsStart[top_index] + top_nnodes + 1;
      const int datasize = MAX(datasize_edges, datasize_nodes);

#ifndef NDEBUG
      assert(top_nedges >= 0);
      assert(top_nnodes >= 0);
      assert(datasize <= depository->datasize_max);
#endif

      return datasize;
   }
}


/** gets number of CSRs in depository */
int graph_csrdepo_getNcsrs(
   const CSRDEPO*        depository          /**< the depository */
   )
{
   assert(depository);
   assert(depository->ncsrs_curr >= 0);

   return depository->ncsrs_curr;
}


/** removes top of CSR depository */
void graph_csrdepo_removeTop(
   CSRDEPO*              depository          /**< the depository */
   )
{
   assert(depository);
   assert(depository->ncsrs_curr >= 1);

#ifndef NDEBUG
   {
      const int top_index = csrdepoGetTopIndex(depository);
      depository->csr_nedges[top_index] = -1;
      depository->csr_nnodes[top_index] = -1;
   }
#endif

   depository->ncsrs_curr--;

   assert(graph_csrdepo_isEmpty(depository) || !graph_csrdepo_hasEmptyTop(depository));
}


/** cleans CSR depository */
void graph_csrdepo_clean(
   CSRDEPO*              depository          /**< the depository */
   )
{
   assert(depository);

   for( int i = depository->ncsrs_curr - 1; i >= 0; ++i )
   {
      graph_csrdepo_removeTop(depository);
   }

   assert(graph_csrdepo_isEmpty(depository));
}


/** adds empty top to CSR depository */
void graph_csrdepo_addEmptyTop(
   CSRDEPO*              depository,         /**< the depository */
   int                   nnodes,             /**< nodes of new top */
   int                   nedges              /**< number of (directed!) edges of new top */
   )
{
   int topindex;

   assert(depository);
   assert(nnodes >= 1 && nedges >= 0);
   assert(graph_csrdepo_isEmpty(depository) || !graph_csrdepo_hasEmptyTop(depository));
   assert(MAX(nnodes, nedges) + graph_csrdepo_getDataSize(depository) < depository->datasize_max);
   assert(depository->ncsrs_curr < depository->ncsrs_max);

   depository->ncsrs_curr++;

   topindex = csrdepoGetTopIndex(depository);

   depository->csr_ptrsStart[topindex + 1] = depository->csr_ptrsStart[topindex] + nnodes + 1;
   depository->csr_ptrsData[topindex + 1] = depository->csr_ptrsData[topindex] + nedges;
   depository->csr_isEmpty[topindex] = TRUE;

#ifndef NDEBUG
   assert(-1 == depository->csr_nnodes[topindex]);
   assert(-1 == depository->csr_nedges[topindex]);

   depository->csr_nnodes[topindex] = nnodes;
   depository->csr_nedges[topindex] = nedges;

   assert(graph_csrdepo_hasEmptyTop(depository));
#endif
}


/** is the CSR depository empty? */
SCIP_Bool graph_csrdepo_isEmpty(
   const CSRDEPO*        depository          /**< the depository */
   )
{
   assert(depository);
   assert(depository->ncsrs_curr >= 0);

   return (depository->ncsrs_curr == 0);
}


/** is top of CSR depository empty? */
SCIP_Bool graph_csrdepo_hasEmptyTop(
   const CSRDEPO*        depository          /**< the depository */
   )
{
   const int topindex = csrdepoGetTopIndex(depository);

   return depository->csr_isEmpty[topindex];
}


/** Gets empty top of current depository.
 *  Will be assumed to not be empty anymore! */
void graph_csrdepo_getEmptyTop(
   const CSRDEPO*        depository,         /**< the depository */
   CSR*                  csr                 /**< pointer to csr struct to fill */
   )
{
   const int topindex = csrdepoGetTopIndex(depository);

   assert(depository->csr_isEmpty[topindex]);
   assert(topindex >= 0 && topindex < depository->ncsrs_max);
   assert(csr);

   csrdepoFillCSR(depository, topindex, csr);

   depository->csr_isEmpty[topindex] = FALSE;
}

/** Gets non-empty (!) top of current depository. */
void graph_csrdepo_getTop(
   const CSRDEPO*        depository,         /**< the depository */
   CSR*                  csr                 /**< pointer to csr struct to fill */
   )
{
   const int topindex = csrdepoGetTopIndex(depository);

   assert(topindex >= 0 && topindex < depository->ncsrs_max);
   assert(!depository->csr_isEmpty[topindex]);
   assert(csr);

   csrdepoFillCSR(depository, topindex, csr);
}


/*
 * Dijkstra heap
 */

/** clean the heap */
void
graph_heap_clean(
   SCIP_Bool             cleanposition,      /**< clean position array? */
   DHEAP*                heap                /**< the heap  */
   )
{
   int* const position = heap->position;
   const int capacity = heap->capacity;

   assert(heap && position);

   heap->size = 0;

   if( cleanposition )
   {
      for( int i = 0; i < capacity; i++ )
         position[i] = UNKNOWN;
   }

   assert(graph_heap_isClean(heap));
}


/** is the heap clean? */
SCIP_Bool
graph_heap_isClean(
   const DHEAP*          heap                /**< the heap  */
   )
{
   const int* const position = heap->position;
   const int capacity = heap->capacity;

   assert(heap && position);

   if( heap->size != 0 )
   {
      SCIPdebugMessage("heap size not 0! (=%d)\n", heap->size);
      return FALSE;
   }

   for( int i = 0; i < capacity; i++ )
   {
      if( UNKNOWN != position[i] )
      {
         SCIPdebugMessage("position %d is not clean \n", i);
         return FALSE;
      }
   }

   return TRUE;
}



/** creates new heap. If entries array is provided, it must be of size capacity + 2  */
SCIP_RETCODE graph_heap_create(
   SCIP*                 scip,               /**< SCIP */
   int                   capacity,           /**< heap capacity */
   int*                  position,           /**< heap position array or NULL */
   DENTRY*               entries,            /**< entries array or NULL */
   DHEAP**               heap                /**< the heap  */
   )
{
   int* position_heap;
   DENTRY* entries_heap;

   assert(scip && heap);
   assert(capacity >= 1);

   SCIP_CALL( SCIPallocMemory(scip, heap) );

   if( position )
      position_heap = position;
   else
      SCIP_CALL( SCIPallocMemoryArray(scip, &(position_heap), capacity) );

   if( entries )
      entries_heap = entries;
   else
      SCIP_CALL( SCIPallocMemoryArray(scip, &(entries_heap), capacity + 2) );

   (*heap)->capacity = capacity;
   (*heap)->position = position_heap;
   (*heap)->entries = entries_heap;

   /* sentinel */
   entries_heap[0].key = DHEAP_MIN_KEY;

   /* debug sentinel */
   entries_heap[capacity + 1].key = DHEAP_MAX_KEY;

   graph_heap_clean(TRUE, *heap);

   return SCIP_OKAY;
}

/** frees the heap */
void graph_heap_free(
   SCIP*                 scip,               /**< SCIP */
   SCIP_Bool             freePositions,      /**< free positions array? */
   SCIP_Bool             freeEntries,        /**< free entries array? */
   DHEAP**               heap                /**< the heap  */
   )
{
   assert(scip && heap);

   if( freePositions )
      SCIPfreeMemoryArray(scip, &((*heap)->position));

   if( freeEntries )
      SCIPfreeMemoryArray(scip, &((*heap)->entries));

   SCIPfreeMemory(scip, heap);
}

/** deletes heap minimum */
void graph_heap_deleteMin(
   int*                  node,               /**< pointer to value of minimum */
   SCIP_Real*            key,                /**< pointer to key of minimum */
   DHEAP*                heap                /**< the heap  */
   )
{
   *key = heap->entries[1].key;

   graph_heap_deleteMinGetNode(node, heap);
}

/** deletes heap minimum */
void graph_heap_deleteMinGetNode(
   int*                  node,               /**< pointer to node stored in minimum (set by method) */
   DHEAP*                heap                /**< the heap  */
   )
{
   assert(node);
   *node = graph_heap_deleteMinReturnNode(heap);
}


/** deletes heap minimum and returns corresponding node */
int graph_heap_deleteMinReturnNode(
   DHEAP*                heap                /**< the heap  */
   )
{
   int* const RESTRICT position = heap->position;
   DENTRY* const RESTRICT entries = heap->entries;
   SCIP_Real fill;
   int parent;
   int hole = 1;
   int child = 2;
   int node;
   const int lastentry = heap->size--;

   assert(heap && position && entries);
   assert(heap->size >= 0);

   node = entries[1].node;

   assert(position[node] == 1);

   position[node] = CONNECT;

   /* move down along min-path */
   while( child < lastentry )
   {
      const SCIP_Real key1 = entries[child].key;
      const SCIP_Real key2 = entries[child + 1].key;
      assert(hole >= 1);
      assert(key1 < DHEAP_MAX_KEY && key2 < DHEAP_MAX_KEY);

      /* second child with smaller key? */
      if( key1 > key2 )
      {
         entries[hole].key = key2;
         child++;
      }
      else
      {
         entries[hole].key = key1;
      }

      assert(entries[hole].node >= 0 && entries[hole].node < heap->capacity);

      entries[hole].node = entries[child].node;
      position[entries[hole].node] = hole;

      hole = child;
      child *= 2;
   }

   /* now hole is at last tree level, fill it with last heap entry and move it up */

   fill = entries[lastentry].key;
   parent = hole / 2;

   assert(fill < DHEAP_MAX_KEY && entries[parent].key < DHEAP_MAX_KEY);

   while( entries[parent].key > fill )
   {
      assert(hole >= 1);

      entries[hole] = entries[parent];

      assert(entries[hole].node >= 0 && entries[hole].node < heap->capacity);

      position[entries[hole].node] = hole;
      hole = parent;
      parent /= 2;

      assert(entries[parent].key < DHEAP_MAX_KEY);
   }

   /* finally, fill the hole */
   entries[hole].key = fill;
   entries[hole].node = entries[lastentry].node;

   assert(entries[hole].node >= 0 && entries[hole].node < heap->capacity);

   if( hole != lastentry )
      position[entries[hole].node] = hole;

#ifndef NDEBUG
   entries[lastentry].key = DHEAP_MAX_KEY;    /* set debug sentinel */
#endif

   return node;
}


/** corrects node position in heap according to new key (or newly inserts the node) */
void graph_heap_correct(
   int                   node,               /**< the node */
   SCIP_Real             newkey,             /**< the new key (needs to be smaller than current one) */
   DHEAP*                heap                /**< the heap  */
   )
{
   int* const RESTRICT position = heap->position;
   DENTRY* const RESTRICT entries = heap->entries;
   int hole;
   int parent;
   SCIP_Real parentkey;

   assert(heap && position && entries);
   assert(newkey < DHEAP_MAX_KEY && newkey > DHEAP_MIN_KEY);
   assert(heap->size <= heap->capacity);
   assert(node >= 0 && node <= heap->capacity);
   assert(position[node] != CONNECT);

   /* node not yet in heap? */
   if( position[node] == UNKNOWN )
   {
      assert(heap->size < heap->capacity);
      hole = ++(heap->size);
   }
   else
   {
      assert(position[node] >= 1);
      hole = position[node];

      assert(entries[hole].node == node);
      assert(GE(entries[hole].key, newkey));
   }

   parent = hole / 2;
   parentkey = entries[parent].key;

   assert(parentkey < DHEAP_MAX_KEY);

   /* move hole up */
   while( parentkey > newkey )
   {
      assert(hole >= 1);

      entries[hole].key = parentkey;
      entries[hole].node = entries[parent].node;
      position[entries[hole].node] = hole;
      hole = parent;
      parent /= 2;
      parentkey = entries[parent].key;
      assert(parentkey < DHEAP_MAX_KEY);
   }

   /* fill the hole */
   entries[hole].key = newkey;
   entries[hole].node = node;
   position[node] = hole;
}


/*
 * CSR
 */


/** allocates empty (and invalid!) CSR storage */
SCIP_RETCODE graph_csr_alloc(
   SCIP*                 scip,               /**< SCIP data structure */
   int                   nnodes,             /**< nodes */
   int                   nedges,             /**< edges */
   CSR**                 csr                 /**< CSR */
   )
{
   CSR* csrd;

   assert(scip);
   assert(nnodes >= 1 && nedges >= 0);

   SCIP_CALL( SCIPallocMemory(scip, csr) );

   csrd = *csr;

   csrd->nedges = nedges;
   csrd->nnodes = nnodes;

   SCIP_CALL( SCIPallocMemoryArray(scip, &(csrd->start), nnodes + 1) );
   SCIP_CALL( SCIPallocMemoryArray(scip, &(csrd->head), nedges) );
   SCIP_CALL( SCIPallocMemoryArray(scip, &(csrd->cost), nedges) );

   return SCIP_OKAY;
}


/** copies CSR storage */
void graph_csr_copy(
   const CSR*            csr_in,             /**< CSR source */
   CSR*                  csr_out             /**< CSR target */
   )
{
   assert(csr_in && csr_out);
   assert(csr_in->nnodes == csr_out->nnodes);
   assert(csr_in->nedges == csr_out->nedges);
   assert(csr_in->nnodes > 0 && csr_in->nedges > 0);

   assert(graph_csr_isValid(csr_in, FALSE));

   BMScopyMemoryArray(csr_out->start, csr_in->start, csr_in->nnodes + 1);
   BMScopyMemoryArray(csr_out->head, csr_in->head, csr_in->nedges);
   BMScopyMemoryArray(csr_out->cost, csr_in->cost, csr_in->nedges);

   assert(graph_csr_isValid(csr_out, FALSE));
}


/** frees dynamic CSR storage */
void graph_csr_free(
   SCIP*                 scip,               /**< SCIP data structure */
   CSR**                 csr                 /**< CSR */
   )
{
   CSR* csrd;

   assert(scip && csr);

   csrd = *csr;

   assert(csrd != NULL && csrd->nnodes >= 1);

   SCIPfreeMemoryArray(scip, &(csrd->cost));
   SCIPfreeMemoryArray(scip, &(csrd->head));
   SCIPfreeMemoryArray(scip, &(csrd->start));

   SCIPfreeMemory(scip, csr);
}


/** initializes CSR storage of graph */
SCIP_RETCODE graph_init_csr(
   SCIP*                 scip,               /**< SCIP data structure */
   GRAPH*                g                   /**< the graph */
   )
{
   CSR* csr;
   int* start_csr;
   int* head_csr;
   SCIP_Real* cost_csr;
   const int nedges = g->edges;
   const int nnodes = g->knots;
   const SCIP_Bool pcmw = graph_pc_isPcMw(g);

   assert(scip && g);
   assert(nnodes >= 1);
   assert(g->csr_storage == NULL);
   assert(!pcmw || !g->extended);

   SCIP_CALL( graph_csr_alloc(scip, nnodes, nedges, &(g->csr_storage)) );

   csr = g->csr_storage;
   start_csr = csr->start;
   head_csr = csr->head;
   cost_csr = csr->cost;

   /* now fill the data in */

   start_csr[0] = 0;

   for( int k = 0; k < nnodes; k++ )
   {
      int pos = start_csr[k];

      if( !pcmw || g->mark[k] )
      {
         for( int e = g->outbeg[k]; e != EAT_LAST; e = g->oeat[e] )
         {
            const int ehead = g->head[e];

            if( pcmw && !g->mark[ehead] )
               continue;

            assert(g->cost[e] < FARAWAY && g->cost[flipedge(e)] < FARAWAY);

            head_csr[pos] = ehead;
            cost_csr[pos++] = g->cost[e];
         }
      }

      assert((pos == start_csr[k] + g->grad[k]) || pcmw);

      start_csr[k + 1] = pos;
   }

   assert(start_csr[nnodes] <= nedges);
   assert(graph_valid_csr(g, TRUE));

   return SCIP_OKAY;
}


/** frees dynamic CSR storage of graph */
void graph_free_csr(
   SCIP*                 scip,               /**< SCIP data structure */
   GRAPH*                g                   /**< the graph */
   )
{
   assert(g->csr_storage);

   graph_csr_free(scip, &(g->csr_storage));

   assert(g->csr_storage == NULL);
}


/** is CSR storage valid? */
SCIP_Bool graph_csr_isValid(
   const CSR*            csr,                /**< the CSR graph */
   SCIP_Bool             verbose             /**< be verbose? */
)
{
   const int* start = csr->start;
   const int nedges = csr->nedges;
   const int nnodes = csr->nnodes;
   const int* head = csr->head;

   if( start[0] != 0 )
   {
      if( verbose )
         printf("CSR: start first corrupted \n");

      return FALSE;
   }

   if( start[nnodes] != nedges )
   {
      if( verbose )
         printf("CSR: start last corrupted \n");

      return FALSE;
   }

   for( int i = 0; i < nnodes; i++ )
   {
      if( start[i] > start[i + 1] )
      {
         if( verbose )
            printf("CSR: ranges corrupted \n");

         return FALSE;
      }
   }

   for( int i = 0; i < nedges; i++ )
   {
      const int v = head[i];

      if( v < 0 || v >= nnodes )
      {
         if( verbose )
            printf("CSR: neighbor entry corrupted \n");

         return FALSE;
      }
   }

   return TRUE;
}


/** is CSR storage of graph valid? */
SCIP_Bool graph_valid_csr(
   const GRAPH*          g,                  /**< the graph */
   SCIP_Bool             verbose             /**< be verbose? */
)
{
   const CSR* csr = g->csr_storage;

   assert(csr && csr->head && csr->cost);

   if( csr->nnodes != g->knots || csr->nedges != g->edges )
   {
      if( verbose )
         printf("CSR: wrong node/edge count \n");

      return FALSE;
   }

   return graph_csr_isValid(csr, verbose);
}


/*
 * DCSR
 */


/** initializes dynamic CSR storage */
SCIP_RETCODE graph_init_dcsr(
   SCIP*                 scip,               /**< SCIP data structure */
   GRAPH*                g                   /**< the graph */
   )
{
   DCSR* dcsr;
   RANGE* range_csr;
   int* head_csr;
   int* edgeid_csr;
   int* id2csr_csr;
   SCIP_Real* cost_csr;
   const int nedges = g->edges;
   const int nnodes = g->knots;
   const SCIP_Bool pcmw = graph_pc_isPcMw(g);

   assert(scip && g);
   assert(nnodes >= 1);
   assert(g->dcsr_storage == NULL);
   assert(!pcmw || !g->extended);

   SCIP_CALL( SCIPallocMemory(scip, &dcsr) );
   g->dcsr_storage = dcsr;

   dcsr->nedges = nedges;
   dcsr->nnodes = nnodes;

   SCIP_CALL( SCIPallocMemoryArray(scip, &(range_csr), nnodes) );
   SCIP_CALL( SCIPallocMemoryArray(scip, &(head_csr), nedges) );
   SCIP_CALL( SCIPallocMemoryArray(scip, &(edgeid_csr), nedges) );
   SCIP_CALL( SCIPallocMemoryArray(scip, &(id2csr_csr), nedges) );
   SCIP_CALL( SCIPallocMemoryArray(scip, &(cost_csr), nedges) );

   dcsr->range = range_csr;
   dcsr->head = head_csr;
   dcsr->edgeid = edgeid_csr;
   dcsr->id2csredge = id2csr_csr;
   dcsr->cost = cost_csr;
   dcsr->cost2 = NULL;
   dcsr->cost3 = NULL;

   graph_mark(g);

   /* now fill the data in */

   for( int e = 0; e < nedges; e++ )
      id2csr_csr[e] = -1;

   range_csr[0].start = 0;

   for( int k = 0; k < nnodes; k++ )
   {
      int pos = range_csr[k].start;

      if( !pcmw || g->mark[k] )
      {
         for( int e = g->outbeg[k]; e != EAT_LAST; e = g->oeat[e] )
         {
            const int ehead = g->head[e];

            if( pcmw && !g->mark[ehead] )
               continue;

            assert(g->cost[e] < FARAWAY && g->cost[flipedge(e)] < FARAWAY);

            id2csr_csr[e] = pos;
            head_csr[pos] = ehead;
            edgeid_csr[pos] = e;
            cost_csr[pos++] = g->cost[e];
         }
      }

      assert(pos == range_csr[k].start + g->grad[k] || pcmw);

      range_csr[k].end = pos;

      if( k != nnodes - 1 )
         range_csr[k + 1].start = range_csr[k].end;
   }

   assert(range_csr[nnodes - 1].end <= nedges);
   assert(graph_valid_dcsr(g, TRUE));

   return SCIP_OKAY;
}

/** frees dynamic CSR storage */
void graph_free_dcsr(
   SCIP*                 scip,               /**< SCIP data structure */
   GRAPH*                g                   /**< the graph */
   )
{
   DCSR* dcsr = g->dcsr_storage;

   assert(scip && g);
   assert(dcsr != NULL && dcsr->nnodes >= 1);

   SCIPfreeMemoryArray(scip, &(dcsr->cost));
   SCIPfreeMemoryArray(scip, &(dcsr->id2csredge));
   SCIPfreeMemoryArray(scip, &(dcsr->edgeid));
   SCIPfreeMemoryArray(scip, &(dcsr->head));
   SCIPfreeMemoryArray(scip, &(dcsr->range));

   SCIPfreeMemory(scip, &(g->dcsr_storage));

   assert(g->dcsr_storage == NULL);
}

/** updates dynamic CSR storage */
void graph_update_dcsr(
   SCIP*                 scip,               /**< SCIP data structure */
   GRAPH*                g                   /**< the graph */
   )
{
   assert(scip && g);

   assert(0 && "implement"); // check whether enough edges and nodes, otherwise reallocate
}

/** deletes CSR indexed edge */
void graph_dcsr_deleteEdge(
   DCSR*                 dcsr,               /**< DCSR container */
   int                   tail,               /**< tail of edge */
   int                   e_csr               /**< CSR indexed edge */
)
{
   RANGE* const range = dcsr->range;
   int* const head = dcsr->head;
   int* const edgeid = dcsr->edgeid;
   int* const id2csredge = dcsr->id2csredge;
   SCIP_Real* const cost = dcsr->cost;
   SCIP_Real* const cost2 = dcsr->cost2;
   SCIP_Real* const cost3 = dcsr->cost3;
   int last;

   assert(dcsr);
   assert(tail >= 0 && tail < dcsr->nnodes);
   assert(e_csr >= 0 && e_csr < dcsr->nedges);
   assert(range[tail].start <= e_csr && e_csr < range[tail].end);

   last = --(range[tail].end);

#ifndef NDEBUG
   id2csredge[edgeid[e_csr]] = -1;
#endif

   /* e_csr not already deleted? */
   if( e_csr != last )
   {
      head[e_csr] = head[last];
      edgeid[e_csr] = edgeid[last];
      id2csredge[edgeid[last]] = e_csr;
      cost[e_csr] = cost[last];

      if( cost2 )
         cost2[e_csr] = cost2[last];

      if( cost3 )
         cost3[e_csr] = cost3[last];
   }

#ifndef NDEBUG
   head[last] = -1;
   edgeid[last] = -1;
   cost[last] = -FARAWAY;
   if( cost2 )
      cost2[last] = -FARAWAY;
   if( cost3 )
      cost3[last] = -FARAWAY;
#endif

}

/** deletes CSR indexed edge and anti-parallel one */
void graph_dcsr_deleteEdgeBi(
   SCIP*                 scip,               /**< SCIP data structure */
   DCSR*                 dcsr,               /**< DCSR container */
   int                   e_csr               /**< CSR indexed edge */
)
{
   int* const head = dcsr->head;
   int* const edgeid = dcsr->edgeid;
   int* const id2csredge = dcsr->id2csredge;
   const int erev_csr = id2csredge[flipedge(edgeid[e_csr])];
   const int i1 = head[erev_csr];
   const int i2 = head[e_csr];

   assert(scip && dcsr);
   assert(e_csr >= 0 && edgeid[e_csr] >= 0);
   assert(erev_csr >= 0);
   assert(SCIPisEQ(scip, dcsr->cost[e_csr], dcsr->cost[erev_csr]));

   SCIPdebugMessage("delete %d %d \n", i1, i2);

   graph_dcsr_deleteEdge(dcsr, i2, erev_csr);
   graph_dcsr_deleteEdge(dcsr, i1, e_csr);
}

/** is DCSR storage of graph valid? */
SCIP_Bool graph_valid_dcsr(
   const GRAPH*          g,                  /**< the graph */
   SCIP_Bool             verbose             /**< be verbose? */
)
{
   const DCSR* const dcsr = g->dcsr_storage;
   const RANGE* const range = dcsr->range;
   const int* const head = dcsr->head;
   const int* const edgeid = dcsr->edgeid;
   const int* const id2csredge = dcsr->id2csredge;
   const int nnodes = dcsr->nnodes;
   const int nedges = dcsr->nedges;

   assert(g && dcsr && range && head && edgeid && id2csredge);

   if( nnodes != g->knots || nedges != g->edges )
   {
      if( verbose )
         printf("DCSR: wrong node/edge cound \n");
      return FALSE;
   }

   for( int i = 0; i < nnodes; i++ )
   {
      const int start = range[i].start;
      const int end = range[i].end;

      if( start > end )
      {
         if( verbose )
            printf("DCSR: ranges corrupted \n");

         return FALSE;
      }

      for( int e = start; e < end; e++ )
      {
         const int ordedge = edgeid[e];

         assert(ordedge >= 0 && ordedge < nedges);

         if( id2csredge[ordedge] != e )
         {
            if( verbose )
               printf("DCSR: id assignment corrupted \n");

            return FALSE;
         }

         if( head[e] != g->head[ordedge] || i != g->tail[ordedge] )
         {
            if( verbose )
               printf("DCSR: edge assignment corrupted \n");

            printf(" %d == %d %d == %d \n",  head[e], g->head[ordedge], i, g->tail[ordedge]);

            return FALSE;
         }
      }
   }

   return TRUE;
}

/*
 * Limited Dijkstra storage
 */


/** initializes (allocates and fills) limited Dijkstra structure members */
SCIP_RETCODE graph_dijkLimited_init(
   SCIP*                 scip,               /**< SCIP */
   const GRAPH*          g,                  /**< the graph */
   DIJK*                 dijkdata            /**< data for limited Dijkstra */
)
{
   const int nnodes = g->knots;
   SCIP_Real* distance;
   STP_Bool* visited;

   assert(scip && g && dijkdata);

   SCIP_CALL( SCIPallocMemoryArray(scip, &(dijkdata->distance), nnodes) );
   SCIP_CALL( SCIPallocMemoryArray(scip, &(dijkdata->visitlist), nnodes) );
   SCIP_CALL( SCIPallocMemoryArray(scip, &(dijkdata->visited), nnodes) );

   graph_heap_create(scip, nnodes, NULL, NULL, &(dijkdata->dheap));

   dijkdata->nvisits = -1;

   distance = dijkdata->distance;
   visited = dijkdata->visited;

   for( int k = 0; k < nnodes; k++ )
   {
      visited[k] = FALSE;
      distance[k] = FARAWAY;
   }

   return SCIP_OKAY;
}

/** cleans limited Dijkstra structure members */
void graph_dijkLimited_clean(
   const GRAPH*          g,                  /**< the graph */
   DIJK*                 dijkdata            /**< data for limited Dijkstra */
)
{
   const int nnodes = g->knots;
   STP_Bool* const visited = dijkdata->visited;
   SCIP_Real* const distance = dijkdata->distance;
   dijkdata->nvisits = -1;

   for( int k = 0; k < nnodes; k++ )
   {
      visited[k] = FALSE;
      distance[k] = FARAWAY;
   }

   graph_heap_clean(TRUE, dijkdata->dheap);
}


/** reset data of limited Dijkstra structure */
void graph_dijkLimited_reset(
   const GRAPH*          g,                  /**< the graph */
   DIJK*                 dijkdata            /**< data for limited Dijkstra */
)
{
   STP_Bool* const visited = dijkdata->visited;
   int* const visitlist = dijkdata->visitlist;
   int* const state = dijkdata->dheap->position;
   SCIP_Real* const distance = dijkdata->distance;
   const int nvisits = dijkdata->nvisits;

   assert(dijkdata && g);

   for( int k = 0; k < nvisits; k++ )
   {
      const int node = visitlist[k];
      assert(node >= 0 && node < g->knots);

      visited[node] = FALSE;
      distance[node] = FARAWAY;
      state[node] = UNKNOWN;
   }

   graph_heap_clean(FALSE, dijkdata->dheap);

#ifndef NDEBUG
   for( int k = 0; k < g->knots; k++ )
   {
      assert(visited[k] == FALSE);
      assert(state[k] == UNKNOWN);
      assert(distance[k] == FARAWAY);
   }
#endif
}


/** frees limited Dijkstra structure member */
void graph_dijkLimited_freeMembers(
   SCIP*                 scip,               /**< SCIP */
   DIJK*                 dijkdata            /**< data for limited Dijkstra */
)
{
   SCIPfreeMemoryArray(scip, &(dijkdata->distance));
   SCIPfreeMemoryArray(scip, &(dijkdata->visitlist));
   SCIPfreeMemoryArray(scip, &(dijkdata->visited));

   graph_heap_free(scip, TRUE, TRUE, &(dijkdata->dheap));
}
