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

/**@file   graph_sol.c
 * @brief  includes methods working on solutions (i.e. trees) to Steiner tree problems
 * @author Daniel Rehfeldt
 *
 * Methods for manipulating solutions (i.e. trees) to Steiner tree problems, such as pruning.
 * Also includes methods for obtaining information about solutions.
 *
 * A list of all interface methods can be found in graph.h
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

/*lint -esym(766,stdlib.h) -esym(766,malloc.h)         */
/*lint -esym(766,string.h) */
//#define SCIP_DEBUG

#include "graph.h"
#include "probdata_stp.h"
#include "portab.h"


/** Deletes subtree from given node, marked by dfspos.
 *  NOTE: recursive method. */
static
void pcsubtreeDelete(
   const GRAPH*          g,                  /**< graph structure */
   int                   subtree_root,       /**< root of the subtree */
   int                   dfspos[],           /**< array to mark DFS positions of nodes */
   int                   result[],           /**< ST edges */
   STP_Bool              connected[]         /**< ST nodes */
)
{
   const int dfspos_root = dfspos[subtree_root];

   assert(dfspos_root > 0);
   assert(connected[subtree_root]);
   assert(g->mark[subtree_root]);

   connected[subtree_root] = FALSE;
   g->mark[subtree_root] = FALSE;

   SCIPdebugMessage("strong prune deletes tree vertex %d \n", subtree_root);

   for( int e = g->outbeg[subtree_root]; e != EAT_LAST; e = g->oeat[e] )
   {
      if( result[e] == CONNECT )
      {
         const int neighbor = g->head[e];

         assert(dfspos[neighbor] >= 0);
         assert(!graph_pc_knotIsDummyTerm(g, neighbor));
         assert(dfspos[neighbor] != dfspos_root);

         /* is neighbor a DFS child of the root?  */
         if( dfspos[neighbor] > dfspos_root)
         {
            result[e] = UNKNOWN;
#ifdef SCIP_DEBUG
            SCIPdebugMessage("strong prune deletes tree edge ");
            graph_edge_printInfo(g, e);
#endif
            pcsubtreeDelete(g, neighbor, dfspos, result, connected);
         }
      }
   }
}


/** Prunes subtree from given node such that it becomes most profitable and returns the profit.
 *  NOTE: recursive method. */
static
SCIP_Real pcsubtreePruneForProfit(
   const GRAPH*          g,                  /**< graph structure */
   const SCIP_Real*      cost,               /**< edge costs */
   int                   subtree_root,       /**< root of the subtree */
   int                   dfspos[],           /**< array to mark DFS positions of nodes */
   int                   result[],           /**< ST edges */
   STP_Bool              connected[],        /**< ST nodes */
   int*                  dfscount            /**< counter */
)
{
   SCIP_Real profit = g->prize[subtree_root];

   if( !graph_pc_isPc(g) )
   {
      assert(graph_pc_isMw(g));

      if( LT(profit, 0.0) )
         profit = 0.0;
   }

   assert(0 <= *dfscount && *dfscount < g->knots);

   dfspos[subtree_root] = ++(*dfscount);

   SCIPdebugMessage("strong-prune from root %d \n", subtree_root);

   for( int e = g->outbeg[subtree_root]; e != EAT_LAST; e = g->oeat[e] )
   {
      if( result[e] == CONNECT )
      {
         const int neighbor = g->head[e];

         assert(dfspos[neighbor] >= 0);
         assert(!graph_pc_knotIsDummyTerm(g, neighbor));

         /* not visited yet? */
         if( dfspos[neighbor] == 0 )
         {
            const SCIP_Real neighbor_profit = pcsubtreePruneForProfit(g, cost, neighbor, dfspos, result, connected, dfscount);
            const SCIP_Real extension_profit = neighbor_profit - cost[e];

            if( LT(extension_profit, 0.0) )
            {
               result[e] = UNKNOWN;
#ifdef SCIP_DEBUG
               SCIPdebugMessage("strong prune deletes tree edge ");
               graph_edge_printInfo(g, e);
#endif
               pcsubtreeDelete(g, neighbor, dfspos, result, connected);
            }
            else
            {
               profit += extension_profit;
            }
         }
      }
   }

   return profit;
}


/** computes trivial solution and sets result edges */
static inline
void pcsolGetTrivialEdges(
   const GRAPH*          g,                  /**< graph structure */
   const STP_Bool*       connected,          /**< ST nodes */
   int*                  result              /**< MST solution, which does not include artificial terminals */
)
{
   const int root = g->source;
   assert(!graph_pc_isRootedPcMw(g));

#ifndef NEDBUG
   for( int i = 0; i < g->edges; i++ )
      assert(UNKNOWN == result[i]);
#endif

   for( int a = g->outbeg[root]; a != EAT_LAST; a = g->oeat[a] )
   {
      const int head = g->head[a];
      if( Is_term(g->term[head]) )
      {
         assert(connected[head]);
         result[a] = CONNECT;
      }
   }
}

/** computes MST on marked graph and sets result edges */
static inline
SCIP_RETCODE pcsolGetMstEdges(
   SCIP*                 scip,               /**< SCIP data structure */
   const GRAPH*          g,                  /**< graph structure */
   const SCIP_Real*      cost,               /**< edge costs */
   int                   root,               /**< root of solution */
   int*                  result              /**< MST solution, which does not include artificial terminals */
)
{
   PATH* mst;
   const int nnodes = graph_get_nNodes(g);
   const int* const gmark = g->mark;

   SCIP_CALL( SCIPallocBufferArray(scip, &mst, nnodes) );
   graph_path_exec(scip, g, MST_MODE, root, cost, mst);

   for( int i = 0; i < nnodes; i++ )
   {
      if( gmark[i] && (mst[i].edge != UNKNOWN) )
      {
         assert(g->path_state[i] == CONNECT);
         assert(g->head[mst[i].edge] == i);
         assert(result[mst[i].edge] == -1);

         result[mst[i].edge] = CONNECT;
      }
   }

   SCIPfreeBufferArray(scip, &mst);

   return SCIP_OKAY;
}


/** Gets root of solution for unrooted PC/MW.
 *  Returns -1 if the solution is empty. */
static inline
int pcsolGetRoot(
   SCIP*                 scip,               /**< SCIP data structure */
   const GRAPH*          g,                  /**< graph structure */
   const STP_Bool*       connected           /**< ST nodes */
   )
{
   int proot = -1;
   const int nnodes = graph_get_nNodes(g);
   const int groot = g->source;

   assert(graph_pc_isPcMw(g));
   assert(!graph_pc_isRootedPcMw(g));

   /* todo remove this hack, better ask for the SCIP stage */
   if( SCIPprobdataGetNTerms(scip) == g->terms && SCIPprobdataGetNNodes(scip) == nnodes )
   {
      int min = nnodes;
      const int* termsorder = SCIPprobdataGetPctermsorder(scip);

      for( int k = 0; k < nnodes; k++ )
      {
         if( termsorder[k] < min && connected[k] )
         {
            assert(Is_pseudoTerm(g->term[k]));

            min = termsorder[k];
            proot = k;
         }
      }

      assert(min >= 0);
      assert(proot == -1 || min < nnodes);
   }
   else
   {
      for( int a = g->outbeg[groot]; a != EAT_LAST; a = g->oeat[a] )
      {
         const int head = g->head[a];
         if( !Is_term(g->term[head]) && connected[head] )
         {
            proot = head;
            break;
         }
      }
   }

   return proot;
}


/** mark nodes of the solution in the graph */
static inline
void pcsolMarkGraphNodes(
   const STP_Bool*       connected,          /**< ST nodes */
   const GRAPH*          g                   /**< graph structure */
   )
{
   int* const gmark = g->mark;
   const int nnodes = graph_get_nNodes(g);
   const SCIP_Bool rpcmw = graph_pc_isRootedPcMw(g);

   assert(g->extended);

   if( rpcmw )
   {
      for( int i = 0; i < nnodes; i++ )
      {
         if( connected[i] && !graph_pc_knotIsDummyTerm(g, i) )
            gmark[i] = TRUE;
         else
            gmark[i] = FALSE;

         assert(gmark[i] || !graph_pc_knotIsFixedTerm(g, i));
      }

      assert(gmark[g->source]);
   }
   else
   {
      const int* const gterm = g->term;

      for( int i = 0; i < nnodes; i++ )
      {
         if( connected[i] && !Is_term(gterm[i]) )
            gmark[i] = TRUE;
         else
            gmark[i] = FALSE;
      }
   }
}


/** prune a Steiner tree in such a way that all leaves are terminals */
static inline
void pcsolPrune(
   const GRAPH*          g,                  /**< graph structure */
   int*                  result,             /**< MST solution, which does not include artificial terminals */
   STP_Bool*             connected           /**< ST nodes */
   )
{
   const int nnodes = graph_get_nNodes(g);
   int count;

   SCIPdebugMessage("starting (simple) pruning \n");

   do
   {
      count = 0;

      for( int i = nnodes - 1; i >= 0; --i )
      {
         int j;
         if( !g->mark[i] || g->path_state[i] != CONNECT || Is_term(g->term[i]) )
            continue;

         for( j = g->outbeg[i]; j != EAT_LAST; j = g->oeat[j] )
            if( result[j] == CONNECT )
               break;

         if( j == EAT_LAST )
         {
            /* there has to be exactly one incoming edge
             */
            assert(!Is_term(g->term[i]) && !Is_pseudoTerm(g->term[i]));

            for( j = g->inpbeg[i]; j != EAT_LAST; j = g->ieat[j] )
            {
               if( result[j] == CONNECT )
               {
                  SCIPdebugMessage("prune delete vertex %d \n", i);
#ifdef SCIP_DEBUG
                  SCIPdebugMessage("prune delete edge ");
                  graph_edge_printInfo(g, j);
#endif

                  result[j] = UNKNOWN;
                  g->mark[i] = FALSE;
                  connected[i] = FALSE;
                  count++;
                  break;
               }
            }
            assert(j != EAT_LAST);
         }
      }
   }
   while( count > 0 );

#ifndef NDEBUG
   /* make sure there is no unconnected vertex */
   for( int i = 0; i < nnodes; i++ )
   {
      if( connected[i] && i != g->source )
      {
         int j;
         for( j = g->inpbeg[i]; j != EAT_LAST; j = g->ieat[j] )
         {
            if( result[j] == CONNECT )
               break;
         }

         assert(j != EAT_LAST);
      }
   }
#endif
}

/** connects dummy terminals to given (pre-) PC solution */
static
void pcsolConnectDummies(
   const GRAPH*          g,                  /**< graph structure */
   int                   solroot,            /**< root of solution */
   int*                  result,             /**< MST solution, which does not include artificial terminals */
   STP_Bool*             connected           /**< ST nodes */
   )
{
   const int nnodes = graph_get_nNodes(g);
   const SCIP_Bool rpcmw = graph_pc_isRootedPcMw(g);

   /* connect all terminals */
   for( int i = 0; i < nnodes; i++ )
   {
      if( Is_term(g->term[i]) && i != g->source )
      {
         const SCIP_Bool isFixedTerm = (rpcmw && g->mark[i]);

         assert(isFixedTerm == graph_pc_knotIsFixedTerm(g, i));
         assert(isFixedTerm || g->grad[i] == 2);
         assert(isFixedTerm || g->inpbeg[i] >= 0);

         if( isFixedTerm )
         {
            assert(connected[i]);

            continue;
         }
         else
         {
            const int e1 = g->inpbeg[i];
            const int e2 = g->ieat[e1];
            const int k1 = g->tail[e1];
            const int k2 = g->tail[e2];

            connected[i] = TRUE;

            assert(graph_pc_knotIsDummyTerm(g, i));
            assert(g->ieat[e2] == EAT_LAST);
            assert(k1 == g->source || k2 == g->source);

            if( k1 != g->source && g->mark[k1] )
               result[e1] = CONNECT;
            else if( k2 != g->source && g->mark[k2] )
               result[e2] = CONNECT;
            else if( k1 == g->source )
               result[e1] = CONNECT;
            else if( k2 == g->source )
               result[e2] = CONNECT;

            /* xor: exactly one of e1 and e2 is used */
            assert((result[e1] != CONNECT) != (result[e2] != CONNECT));
         }
      }
      else if( i == solroot && !rpcmw )
      {
         int e1;
         for( e1 = g->inpbeg[i]; e1 != EAT_LAST; e1 = g->ieat[e1] )
            if( g->tail[e1] == g->source )
               break;
         assert(e1 != EAT_LAST);
         result[e1] = CONNECT;
      }
   }

   if( !rpcmw )
      connected[g->source] = TRUE;

   assert(connected[g->source]);
}


/** Finds optimal prize-collecting Steiner tree on given tree. */
static
SCIP_RETCODE strongPruneSteinerTreePc(
   SCIP*                 scip,               /**< SCIP data structure */
   const GRAPH*          g,                  /**< graph structure */
   const SCIP_Real*      cost,               /**< edge costs */
   int                   solroot,            /**< root of the solution */
   int*                  result,             /**< ST edges */
   STP_Bool*             connected           /**< ST nodes */
)
{
   int* dfspos;
   const int nnodes = graph_get_nNodes(g);
   int dfscount = 0;
   SCIP_Real profit;
#ifndef NDEBUG
   const int nsoledges = graph_solGetNedges(g, result);
#endif

   assert(solroot >= 0);
   assert(connected[solroot]);
   assert(graph_pc_isPcMw(g));
   assert(!graph_pc_knotIsDummyTerm(g, solroot));
   assert(g->extended);

   /* todo find best root? */

   SCIP_CALL( SCIPallocBufferArray(scip, &dfspos, nnodes) );

   BMSclearMemoryArray(dfspos, nnodes);

   /* compute the subtree */
   profit = pcsubtreePruneForProfit(g, cost, solroot, dfspos, result, connected, &dfscount);

   assert(nsoledges + 1 == dfscount);

   if( LT(profit, 0.0) )
   {
      assert(!graph_pc_isRootedPcMw(g));
      assert(!Is_anyTerm(g->term[solroot]));
      assert(EQ(g->prize[solroot], 0.0));

      // todo can this ever happen?
      // if so, better have a flag here, because we dont wannt set edges to dummies here...
      return SCIP_ERROR;
//      SCIPdebugMessage("Best subtree is negative! Take empty solution \n");
//      pcsolGetTrivialEdges(g, connected, result);
   }

   SCIPfreeBufferArray(scip, &dfspos);

   return SCIP_OKAY;
}


/** prune a Steiner tree in such a way that all leaves are terminals */
static
SCIP_RETCODE pruneSteinerTreeStp(
   SCIP*                 scip,               /**< SCIP data structure */
   const GRAPH*          g,                  /**< graph structure */
   const SCIP_Real*      cost,               /**< edge costs */
   int*                  result,             /**< ST edges, which need to be set to UNKNOWN */
   STP_Bool*             connected           /**< ST nodes */
   )
{
   PATH* mst;
   int count;
   const int nnodes = graph_get_nNodes(g);
#ifndef NEDBUG
   int nconnected = 0;
#endif

   assert(scip != NULL);
   assert(cost != NULL);
   assert(result != NULL);
   assert(connected != NULL);

#ifndef NEDBUG
   for( int i = 0; i < g->edges; i++ )
      assert(UNKNOWN == result[i]);

   for( int i = nnodes - 1; i >= 0; --i )
      if( connected[i] )
         nconnected++;

   assert(nconnected >= g->terms);
   assert(g->source >= 0);
   assert(g->source < nnodes);
#endif

   SCIP_CALL( SCIPallocBufferArray(scip, &mst, nnodes) );

   /* compute the MST */
   for( int i = nnodes - 1; i >= 0; --i )
      g->mark[i] = connected[i];

   graph_path_exec(scip, g, MST_MODE, g->source, cost, mst);

   for( int i = nnodes - 1; i >= 0; --i )
   {
      if( connected[i] && (mst[i].edge != -1) )
      {
         assert(g->head[mst[i].edge] == i);
         assert(result[mst[i].edge] == UNKNOWN);

         result[mst[i].edge] = 0;
      }
   }

   /* prune */
   do
   {
      SCIPdebug(fputc('C', stdout));
      SCIPdebug(fflush(stdout));

      count = 0;

      for( int i = nnodes - 1; i >= 0; --i )
      {
         int j;

         if( !g->mark[i] )
            continue;

         if( g->term[i] == 0 )
            continue;

         for( j = g->outbeg[i]; j != EAT_LAST; j = g->oeat[j] )
            if( result[j] == 0 )
               break;

         if( j == EAT_LAST )
         {
            /* there has to be exactly one incoming edge
             */
            for( j = g->inpbeg[i]; j != EAT_LAST; j = g->ieat[j] )
            {
               if( result[j] == 0 )
               {
                  result[j]    = -1;
                  g->mark[i]   = FALSE;
                  connected[i] = FALSE;
                  count++;
                  break;
               }
            }
         }
      }
   }
   while( count > 0 );

   SCIPfreeBufferArray(scip, &mst);

   return SCIP_OKAY;
}


/* prune the (rooted) prize collecting Steiner tree in such a way that all leaves are terminals
 * NOTE: graph is not really const, mark is changed! todo */
static
SCIP_RETCODE pruneSteinerTreePc(
   SCIP*                 scip,               /**< SCIP data structure */
   const GRAPH*          g,                  /**< graph structure */
   const SCIP_Real*      cost,               /**< edge costs */
   int*                  result,             /**< ST edges (need to be set to UNKNOWN) */
   STP_Bool*             connected           /**< ST nodes */
   )
{
   const SCIP_Bool rpcmw = graph_pc_isRootedPcMw(g);
   int solroot = g->source;

#ifndef NEDBUG
   int* result_dbg;
   STP_Bool* connected_dbg;
   const int nedges = graph_get_nEdges(g);
   for( int i = 0; i < nedges; i++ )
      assert(UNKNOWN == result[i]);
#endif

   assert(scip && cost && result && connected);
   assert(g->extended);

   pcsolMarkGraphNodes(connected, g);

   if( !rpcmw )
   {
      solroot = pcsolGetRoot(scip, g, connected);

      /* trivial solution? */
      if( solroot == -1 )
      {
         printf("trivial solution in pruning \n");

         pcsolGetTrivialEdges(g, connected, result);

         return SCIP_OKAY;
      }
   }

   assert(0 <= solroot && solroot < g->knots);
   assert(g->mark[solroot]);
   SCIPdebugMessage("(non-artificial) solution root=%d \n", solroot);

   SCIP_CALL( pcsolGetMstEdges(scip, g, cost, solroot, result) );

#ifndef NDEBUG
   for( int i = 0; i < g->knots; ++i )
      assert((g->path_state[i] == CONNECT) == g->mark[i]);

   SCIP_CALL( SCIPallocBufferArray(scip, &result_dbg, nedges) );
   SCIP_CALL( SCIPallocBufferArray(scip, &connected_dbg, g->knots) );

   BMScopyMemoryArray(result_dbg, result, nedges);
   BMScopyMemoryArray(connected_dbg, connected, g->knots);
#endif

   // todo for MW write some unit checks and tests first!
   if( graph_pc_isPc(g) )
   {
      SCIP_CALL( strongPruneSteinerTreePc(scip, g, cost, solroot, result, connected) );
   }

   pcsolConnectDummies(g, solroot, result, connected);

   /* simple pruning todo omit */
   pcsolPrune(g, result, connected);

#ifndef NDEBUG
   pcsolConnectDummies(g, solroot, result_dbg, connected_dbg);
   pcsolPrune(g, result_dbg, connected_dbg);

   assert(LE(graph_solGetObj(g, result, 0.0, nedges), graph_solGetObj(g, result_dbg, 0.0, nedges)));

   SCIPfreeBufferArray(scip, &connected_dbg);
   SCIPfreeBufferArray(scip, &result_dbg);
#endif

   assert(graph_solIsValid(scip, g, result));

   return SCIP_OKAY;
}

/*
 * Interface methods
 */

/** Prune solution given by included nodes.
 *  NOTE: For PC/RPC this method will get the original edge costs before pruning! */
SCIP_RETCODE graph_solPrune(
   SCIP*                 scip,               /**< SCIP data structure */
   const GRAPH*          g,                  /**< graph structure */
   int*                  result,             /**< ST edges (out) */
   STP_Bool*             connected           /**< ST nodes (in/out) */
   )
{
   const int nedges = graph_get_nEdges(g);

   assert(scip && result && connected);
   assert(g->stp_type != STP_DHCSTP);

   for( int e = 0; e < nedges; e++ )
      result[e] = UNKNOWN;

   if( graph_pc_isPcMw(g) )
   {
      SCIP_Real* edgecosts = NULL;
      assert(g->extended);

      /* do we have biased edge costs? */
      if( graph_pc_isPc(g) )
      {
         SCIP_CALL( SCIPallocBufferArray(scip, &edgecosts, nedges) );

         graph_pc_getOrgCosts(scip, g, edgecosts);
      }
      else
      {
         edgecosts = g->cost;
      }

      SCIP_CALL( pruneSteinerTreePc(scip, g, edgecosts, result, connected) );

      if( graph_pc_isPc(g) )
         SCIPfreeBufferArray(scip, &edgecosts);
   }
   else
   {
      SCIP_CALL( pruneSteinerTreeStp(scip, g, g->cost, result, connected) );
   }

   assert(graph_solIsValid(scip, g, result));

   return SCIP_OKAY;
}


/** prune solution given by included nodes */
SCIP_RETCODE graph_solPruneFromNodes(
   SCIP*                 scip,               /**< SCIP data structure */
   const GRAPH*          g,                  /**< graph structure */
   int*                  result,             /**< ST edges */
   STP_Bool*             connected           /**< ST nodes */
   )
{
   assert(scip && g && result && connected);
   assert(g->stp_type != STP_DHCSTP);

   SCIP_CALL( graph_solPrune(scip, g, result, connected) );

   return SCIP_OKAY;
}


/** prune solution given by included edges */
SCIP_RETCODE graph_solPruneFromEdges(
   SCIP*                 scip,               /**< SCIP data structure */
   const GRAPH*          g,                  /**< graph structure */
   int*                  result              /**< ST edges */
   )
{
   STP_Bool* connected;
   const int nnodes = graph_get_nNodes(g);
   const int nedges = graph_get_nEdges(g);

   assert(scip && result);
   assert(graph_solIsValid(scip, g, result));

   SCIP_CALL( SCIPallocBufferArray(scip, &connected, nnodes) );

   for( int k = 0; k < nnodes; k++ )
      connected[k] = FALSE;

   for( int e = 0; e < nedges; e++ )
   {
      if( CONNECT == result[e] )
      {
         connected[g->head[e]] = TRUE;
         connected[g->tail[e]] = TRUE;
      }
   }

#ifdef SCIP_DEBUG
   SCIPdebugMessage("prune from edges: \n");
   graph_solPrint(g, result);
#endif

   SCIP_CALL( graph_solPruneFromNodes(scip, g, result, connected) );

   SCIPfreeBufferArray(scip, &connected);

   return SCIP_OKAY;
}


/** Prunes solution with respect to the provided edges costs.
 *  NOTE: method exists purely for optimization, so that unbiased costs for PC do not have to computed again! */
SCIP_RETCODE graph_solPruneFromTmHeur(
   SCIP*                 scip,               /**< SCIP data structure */
   const GRAPH*          g,                  /**< graph structure */
   const SCIP_Real*      cost,               /**< (possibly biased) edge costs */
   int*                  result,             /**< ST edges */
   STP_Bool*             connected           /**< ST nodes */
   )
{
   const int nedges = graph_get_nEdges(g);

   assert(scip && cost && result && connected);

   if( g->stp_type != STP_DHCSTP )
   {
      for( int e = 0; e < nedges; e++ )
         result[e] = UNKNOWN;
   }

   if( graph_pc_isPcMw(g) )
   {
      if( graph_pc_isPc(g) )
      {
         assert(cost);
         SCIP_CALL( pruneSteinerTreePc(scip, g, cost, result, connected) );
      }
      else
      {
         assert(!cost);
         SCIP_CALL( pruneSteinerTreePc(scip, g, g->cost, result, connected) );
      }
   }
   else
      SCIP_CALL( pruneSteinerTreeStp(scip, g, (g->stp_type != STP_DHCSTP) ? g->cost : cost, result, connected) );

   return SCIP_OKAY;
}


/** changes solution according to given root */
SCIP_RETCODE graph_solReroot(
   SCIP*                 scip,               /**< SCIP data structure */
   GRAPH*                g,                  /**< the graph */
   int*                  result,             /**< solution array (CONNECT/UNKNOWN) */
   int                   newroot             /**< the new root */
   )
{
   int* queue;
   int* gmark;
   int size;
   const int nnodes = graph_get_nNodes(g);

   assert(scip != NULL);
   assert(g != NULL);
   assert(result != NULL);
   assert(newroot >= 0 && newroot < nnodes);
   assert(Is_term(g->term[newroot]));

   if( g->grad[newroot] == 0 )
      return SCIP_OKAY;

   SCIP_CALL( SCIPallocBufferArray(scip, &gmark, nnodes) );
   SCIP_CALL( SCIPallocBufferArray(scip, &queue, nnodes) );

   for( int k = 0; k < nnodes; k++ )
      gmark[k] = FALSE;

   gmark[newroot] = TRUE;
   size = 0;
   queue[size++] = newroot;

   /* BFS loop */
   while( size )
   {
      const int node = queue[--size];

      /* traverse outgoing arcs */
      for( int a = g->outbeg[node]; a != EAT_LAST; a = g->oeat[a] )
      {
         const int head = g->head[a];

         if( !gmark[head] && (result[a] == CONNECT || result[flipedge(a)] == CONNECT ) )
         {
            if( result[flipedge(a)] == CONNECT  )
            {
               result[a] = CONNECT;
               result[flipedge(a)] = UNKNOWN;
            }
            gmark[head] = TRUE;
            queue[size++] = head;
         }
      }
   }

   /* adjust solution if infeasible */
   for( int k = 0; k < nnodes; k++ )
   {
      if( !gmark[k] )
      {
         for( int a = g->outbeg[k]; a != EAT_LAST; a = g->oeat[a] )
         {
            result[a] = UNKNOWN;
            result[flipedge(a)] = UNKNOWN;
         }

         /* not yet connected terminal? */
         if( Is_term(g->term[k]) )
         {
            int a;
            assert(g->stp_type != STP_SPG);

            for( a = g->inpbeg[k]; a != EAT_LAST; a = g->ieat[a] )
            {
               const int node = g->tail[a];
               if( gmark[node] && node != newroot )
               {
                  result[a] = CONNECT;
                  break;
               }
            }
            if( a == EAT_LAST )
            {
               for( a = g->inpbeg[k]; a != EAT_LAST; a = g->ieat[a] )
               {
                  const int node = g->tail[a];
                  if( node == newroot )
                  {
                     result[a] = CONNECT;
                     break;
                  }
               }
            }
            else
               gmark[k] = TRUE;
         }
      }
   }

   SCIPfreeBufferArray(scip, &queue);
   SCIPfreeBufferArray(scip, &gmark);

#ifndef NDEBUG
   {
      const int realroot = g->source;
      g->source = newroot;
      assert(graph_solIsValid(scip, g, result));
      g->source = realroot;
   }
#endif

   return SCIP_OKAY;
}


/** checks whether edge(s) of given primal solution have been deleted */
SCIP_Bool graph_solIsUnreduced(
   SCIP*                 scip,               /**< SCIP data structure */
   const GRAPH*          graph,              /**< graph data structure */
   const int*            result              /**< solution array, indicating whether an edge is in the solution */
   )
{
   const int nedges = graph_get_nEdges(graph);

   assert(scip != NULL);
   assert(result != NULL);

   for( int i = 0; i < nedges; i++ )
   {
      if( result[i] == CONNECT && graph->oeat[i] == EAT_FREE )
         return FALSE;
   }

   return TRUE;
}

/** verifies whether a given primal solution is feasible */
SCIP_Bool graph_solIsValid(
   SCIP*                 scip,               /**< SCIP data structure */
   const GRAPH*          graph,              /**< graph data structure */
   const int*            result              /**< solution array, indicating whether an edge is in the solution */
   )
{
   int* queue = NULL;
   STP_Bool* reached = NULL;
   int size;
   int nterms;
   int termcount;
   const int nnodes = graph_get_nNodes(graph);
   const int root = graph->source;
   SCIP_Bool countpseudo;

   assert(scip && result);
   assert(root >= 0);

#ifndef NDEBUG
   for( int e = 0; e < graph->edges; ++e )
      assert(result[e] == CONNECT || result[e] == UNKNOWN);
#endif

   SCIP_CALL_ABORT( SCIPallocBufferArray(scip, &reached, nnodes) );
   SCIP_CALL_ABORT( SCIPallocBufferArray(scip, &queue, nnodes) );

   if( graph_pc_isPcMw(graph) && !graph->extended )
   {
      countpseudo = TRUE;
      nterms = graph_pc_nProperPotentialTerms(graph);

      if( !graph_pc_isRootedPcMw(graph) )
         nterms++;
   }
   else
   {
      countpseudo = FALSE;
      nterms = graph->terms;
   }

   for( int i = 0; i < nnodes; i++ )
      reached[i] = FALSE;

   /* BFS until all terminals are reached */

   termcount = 1;
   size = 0;
   reached[root] = TRUE;
   queue[size++] = root;

   while( size )
   {
      const int node = queue[--size];

      for( int e = graph->outbeg[node]; e != EAT_LAST; e = graph->oeat[e] )
      {
         if( result[e] == CONNECT )
         {
            const int i = graph->head[e];

            /* cycle? */
            if( reached[i] )
            {
               SCIPfreeBufferArray(scip, &queue);
               SCIPfreeBufferArray(scip, &reached);

               SCIPdebugMessage("solution contains a cycle ... \n");
               return FALSE;
            }

            if( countpseudo )
            {
               if( Is_pseudoTerm(graph->term[i]) || graph_pc_knotIsFixedTerm(graph, i) )
                  termcount++;
            }
            else
            {
               if( Is_term(graph->term[i]) )
                  termcount++;
            }

            reached[i] = TRUE;
            queue[size++] = i;
         }
      }
   }

#ifdef SCIP_DEBUG
   if( termcount != nterms )
   {
      printf("termcount %d graph->terms %d \n", termcount, nterms);
      printf("root %d \n", root);

      for( int i = 0; i < nnodes; i++ )
      {
         const int isMandatoryTerm = countpseudo?
               (Is_pseudoTerm(graph->term[i]) || graph_pc_knotIsFixedTerm(graph, i)) : Is_term(graph->term[i]);

         if( !reached[i] && isMandatoryTerm )
         {
            if( graph_pc_isPc(graph) && graph_pc_termIsNonLeafTerm(graph, i) )
               continue;

            printf("fail: ");
            graph_knot_printInfo(graph, i);

            for( int e = graph->inpbeg[i]; e != EAT_LAST; e = graph->ieat[e] )
            {
               printf("...neighbor: ");
               graph_knot_printInfo(graph, graph->tail[e]);
            }
         }
      }

      graph_solPrint(graph, result);
   }
#endif

   SCIPfreeBufferArray(scip, &queue);
   SCIPfreeBufferArray(scip, &reached);

   return (termcount == nterms);
}


/** prints given solution */
void graph_solPrint(
   const GRAPH*          graph,              /**< graph data structure */
   const int*            result              /**< solution array, indicating whether an edge is in the solution */
   )
{
   const int nedges = graph_get_nEdges(graph);

   assert(result);

   printf("solution tree edges: \n");

   for( int e = 0; e < nedges; ++e )
   {
      assert(result[e] == CONNECT || result[e] == UNKNOWN);

      if( CONNECT == result[e] )
      {
         printf("   ");
         graph_edge_printInfo(graph, e);
      }
   }
}


/** mark endpoints of edges in given list */
void graph_solSetNodeList(
   const GRAPH*          g,              /**< graph data structure */
   STP_Bool*             solnode,        /**< solution nodes array (TRUE/FALSE) */
   IDX*                  listnode        /**< edge list */
   )
{
   int i;
   IDX* curr;

   assert(g != NULL);
   assert(solnode != NULL);

   curr = listnode;

   while( curr != NULL )
   {
      i = curr->index;

      solnode[g->head[i]] = TRUE;
      solnode[g->tail[i]] = TRUE;

      curr = curr->parent;
   }
}

/** compute solution value for given edge-solution array (CONNECT/UNKNOWN) and offset */
SCIP_Real graph_solGetObj(
   const GRAPH*          g,                  /**< the graph */
   const int*            soledge,            /**< solution */
   SCIP_Real             offset,             /**< offset */
   int                   nedges              /**< number of edges todo delete */
   )
{
   SCIP_Real obj = offset;
   const SCIP_Real* const edgecost = g->cost;

   assert(nedges == g->edges);
   assert(!graph_pc_isPcMw(g) || g->extended);

   for( int e = 0; e < nedges; e++ )
      if( soledge[e] == CONNECT )
         obj += edgecost[e];

   return obj;
}


/** computes number of edges in solution value */
int graph_solGetNedges(
   const GRAPH*          g,                  /**< the graph */
   const int*            soledge             /**< solution */
   )
{
   const int nedges = graph_get_nEdges(g);
   int edgecount = 0;

   assert(soledge);

   for( int e = 0; e < nedges; e++ )
      if( soledge[e] == CONNECT )
         edgecount++;

   return edgecount;
}


/** marks vertices for given edge-solution array (CONNECT/UNKNOWN) */
void graph_solSetVertexFromEdge(
   const GRAPH*          g,                  /**< the graph */
   const int*            result,             /**< solution array (CONNECT/UNKNOWN) */
   STP_Bool*             solnode             /**< marks whether node is in solution */
)
{
   const int nedges = g->edges;
   const int nnodes = g->knots;

   assert(g && result && solnode);

   for( int i = 0; i < nnodes; i++ )
      solnode[i] = FALSE;

   solnode[g->source] = TRUE;

   for( int e = 0; e < nedges; e++ )
   {
      if( result[e] == CONNECT )
      {
         assert(g->oeat[e] != EAT_FREE);

         solnode[g->head[e]] = TRUE;
      }
   }

#ifndef NDEBUG
   for( int e = 0; e < nedges; e++ )
      if( result[e] == CONNECT )
         assert(solnode[g->head[e]] && solnode[g->tail[e]]);
#endif
}

/** get original solution */
SCIP_RETCODE graph_solGetOrg(
   SCIP*           scip,               /**< SCIP data structure */
   const GRAPH*    transgraph,         /**< the transformed graph */
   const GRAPH*    orggraph,           /**< the original graph */
   const int*      transsoledge,       /**< solution for transformed problem */
   int*            orgsoledge          /**< new retransformed solution */
)
{
   STP_Bool* orgnodearr;
   IDX** const ancestors = transgraph->ancestors;

   const int transnedges = transgraph->edges;
   const int orgnnodes = orggraph->knots;
   const SCIP_Bool pcmw = graph_pc_isPcMw(transgraph);

   assert(transgraph != NULL && orggraph != NULL && transsoledge != NULL && orgsoledge != NULL);
   assert(transgraph->ancestors != NULL);
   assert(transgraph->stp_type == orggraph->stp_type);

   SCIP_CALL( SCIPallocBufferArray(scip, &orgnodearr, orgnnodes) );

   for( int k = 0; k < orgnnodes; k++ )
      orgnodearr[k] = FALSE;

   for( int e = 0; e < transnedges; e++ )
      if( transsoledge[e] == CONNECT )
         graph_solSetNodeList(orggraph, orgnodearr, ancestors[e]);

   /* retransform edges fixed during graph reduction */
   graph_solSetNodeList(orggraph, orgnodearr, graph_get_fixedges(transgraph));

   if( pcmw )
   {
      // potentially single-vertex solution?
      if( graph_pc_isRootedPcMw(transgraph) && transgraph->terms == 1 && graph_pc_nFixedTerms(orggraph) == 1 )
         orgnodearr[orggraph->source] = TRUE;

      SCIP_CALL( graph_solMarkPcancestors(scip, transgraph->pcancestors, orggraph->tail, orggraph->head, orgnnodes,
            orgnodearr, NULL, NULL, NULL, NULL ) );
   }

   /* prune solution (in original graph) */
   SCIP_CALL( graph_solPrune(scip, orggraph, orgsoledge, orgnodearr) );

   SCIPfreeBufferArray(scip, &orgnodearr);

   assert(graph_solIsValid(scip, orggraph, orgsoledge));

   return SCIP_OKAY;
}



/** mark original solution */
SCIP_RETCODE graph_solMarkPcancestors(
   SCIP*           scip,               /**< SCIP data structure */
   IDX**           pcancestors,        /**< the ancestors */
   const int*      tails,              /**< tails array */
   const int*      heads,              /**< heads array */
   int             orgnnodes,          /**< original number of nodes */
   STP_Bool*       solnodemark,        /**< solution nodes mark array */
   STP_Bool*       soledgemark,        /**< solution edges mark array or NULL */
   int*            solnodequeue,       /**< solution nodes queue or NULL  */
   int*            nsolnodes,          /**< number of solution nodes or NULL */
   int*            nsoledges           /**< number of solution edges or NULL */
)
{
   int* queue;
   int nnodes;
   int nedges = (nsoledges != NULL)? *nsoledges : 0;
   int qstart;
   int qend;

   assert(scip != NULL && tails != NULL && heads != NULL && pcancestors != NULL && solnodemark != NULL);

   if( solnodequeue != NULL )
      queue = solnodequeue;
   else
      SCIP_CALL( SCIPallocBufferArray(scip, &queue, orgnnodes) );

   if( nsolnodes == NULL )
   {
      assert(solnodequeue == NULL);
      nnodes = 0;
      for( int k = 0; k < orgnnodes; k++ )
         if( solnodemark[k] )
            queue[nnodes++] = k;
   }
   else
   {
      nnodes = *nsolnodes;
      assert(solnodequeue != NULL);
   }

   qstart = 0;
   qend = nnodes;

   while( qend != qstart )
   {
      int k = qstart;

      assert(qstart < qend);
      qstart = qend;

      for( ; k < qend; k++ )
      {
         const int ancestornode = queue[k];

         assert(solnodemark[ancestornode]);

         for( IDX* curr = pcancestors[ancestornode]; curr != NULL; curr = curr->parent )
         {
            const int ancestoredge = curr->index;
            assert(tails[ancestoredge] < orgnnodes && heads[ancestoredge] < orgnnodes);

            if( soledgemark != NULL && !soledgemark[ancestoredge] )
            {
               soledgemark[ancestoredge] = TRUE;
               nedges++;
            }
            if( !solnodemark[tails[ancestoredge]] )
            {
               solnodemark[tails[ancestoredge]] = TRUE;
               queue[nnodes++] = tails[ancestoredge];
            }
            if( !solnodemark[heads[ancestoredge]] )
            {
               solnodemark[heads[ancestoredge]] = TRUE;
               queue[nnodes++] = heads[ancestoredge];
            }
         }
      }
      qend = nnodes;
   }

   if( nsolnodes != NULL )
      *nsolnodes = nnodes;

   if( nsoledges != NULL )
      *nsoledges = nedges;

   if( solnodequeue == NULL )
      SCIPfreeBufferArray(scip, &queue);

   return SCIP_OKAY;
}
