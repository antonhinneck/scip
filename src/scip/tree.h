/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2004 Tobias Achterberg                              */
/*                                                                           */
/*                  2002-2004 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the SCIP Academic Licence.        */
/*                                                                           */
/*  You should have received a copy of the SCIP Academic License             */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#pragma ident "@(#) $Id: tree.h,v 1.63 2004/09/07 18:22:21 bzfpfend Exp $"

/**@file   tree.h
 * @brief  internal methods for branch and bound tree
 * @author Tobias Achterberg
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __TREE_H__
#define __TREE_H__


#include "def.h"
#include "memory.h"
#include "type_set.h"
#include "type_stat.h"
#include "type_event.h"
#include "type_lp.h"
#include "type_var.h"
#include "type_tree.h"
#include "type_branch.h"
#include "pub_tree.h"

#ifndef NDEBUG
#include "struct_tree.h"
#endif



/*
 * Node methods
 */

/** creates a child node of the active node */
extern
RETCODE SCIPnodeCreateChild(
   NODE**           node,               /**< pointer to node data structure */
   MEMHDR*          memhdr,             /**< block memory */
   SET*             set,                /**< global SCIP settings */
   STAT*            stat,               /**< problem statistics */
   TREE*            tree,               /**< branch and bound tree */
   Real             nodeselprio         /**< node selection priority of new node */
   );

/** frees node */
extern
RETCODE SCIPnodeFree(
   NODE**           node,               /**< node data */
   MEMHDR*          memhdr,             /**< block memory buffer */
   SET*             set,                /**< global SCIP settings */
   TREE*            tree,               /**< branch and bound tree */
   LP*              lp                  /**< current LP data */
   );

/** increases the reference counter of the LP state in the fork or subroot node */
extern
void SCIPnodeCaptureLPIState(
   NODE*            node,               /**< fork/subroot node */
   int              nuses               /**< number to add to the usage counter */
   );

/** decreases the reference counter of the LP state in the fork or subroot node */
extern
RETCODE SCIPnodeReleaseLPIState(
   NODE*            node,               /**< fork/subroot node */
   MEMHDR*          memhdr,             /**< block memory buffers */
   LP*              lp                  /**< current LP data */
   );

/** activates a leaf node */
extern
RETCODE SCIPnodeActivate(
   NODE*            node,               /**< leaf node to activate (or NULL to deactivate all nodes) */
   MEMHDR*          memhdr,             /**< block memory */
   SET*             set,                /**< global SCIP settings */
   STAT*            stat,               /**< problem statistics */
   TREE*            tree,               /**< branch and bound tree */
   LP*              lp,                 /**< current LP data */
   BRANCHCAND*      branchcand,         /**< branching candidate storage */
   EVENTQUEUE*      eventqueue,         /**< event queue */
   Real             cutoffbound         /**< cutoff bound: all nodes with lowerbound >= cutoffbound are cut off */
   );

/** cuts off node and whole sub tree from branch and bound tree */
extern
void SCIPnodeCutoff(
   NODE*            node,               /**< node that should be cut off */
   SET*             set                 /**< global SCIP settings */
   );

/** adds constraint locally to the node and captures it; activates constraint, if node is active;
 *  if a local constraint is added to the root node, it is automatically upgraded into a global constraint
 */
extern
RETCODE SCIPnodeAddCons(
   NODE*            node,               /**< node to add constraint to */
   MEMHDR*          memhdr,             /**< block memory */
   SET*             set,                /**< global SCIP settings */
   STAT*            stat,               /**< problem statistics */
   TREE*            tree,               /**< branch and bound tree */
   CONS*            cons                /**< constraint to add */
   );

/** disables constraint's separation, enforcing, and propagation capabilities at the node, and captures constraint;
 *  disables constraint, if node is active
 */
extern
RETCODE SCIPnodeDisableCons(
   NODE*            node,               /**< node to add constraint to */
   MEMHDR*          memhdr,             /**< block memory */
   SET*             set,                /**< global SCIP settings */
   STAT*            stat,               /**< problem statistics */
   TREE*            tree,               /**< branch and bound tree */
   CONS*            cons                /**< constraint to disable */
   );

/** adds bound change with inference information to active node, child of active node, or probing node;
 *  if possible, adjusts bound to integral value
 */
extern
RETCODE SCIPnodeAddBoundinfer(
   NODE*            node,               /**< node to add bound change to */
   MEMHDR*          memhdr,             /**< block memory */
   SET*             set,                /**< global SCIP settings */
   STAT*            stat,               /**< problem statistics */
   TREE*            tree,               /**< branch and bound tree */
   LP*              lp,                 /**< current LP data */
   BRANCHCAND*      branchcand,         /**< branching candidate storage */
   EVENTQUEUE*      eventqueue,         /**< event queue */
   VAR*             var,                /**< variable to change the bounds for */
   Real             newbound,           /**< new value for bound */
   BOUNDTYPE        boundtype,          /**< type of bound: lower or upper bound */
   CONS*            infercons,          /**< constraint that deduced the bound change, or NULL */
   int              inferinfo,          /**< user information for inference to help resolving the conflict */
   Bool             probingchange       /**< is the bound change a temporary setting due to probing? */
   );

/** adds bound change to active node, or child of active node; if possible, adjusts bound to integral value */
extern
RETCODE SCIPnodeAddBoundchg(
   NODE*            node,               /**< node to add bound change to */
   MEMHDR*          memhdr,             /**< block memory */
   SET*             set,                /**< global SCIP settings */
   STAT*            stat,               /**< problem statistics */
   TREE*            tree,               /**< branch and bound tree */
   LP*              lp,                 /**< current LP data */
   BRANCHCAND*      branchcand,         /**< branching candidate storage */
   EVENTQUEUE*      eventqueue,         /**< event queue */
   VAR*             var,                /**< variable to change the bounds for */
   Real             newbound,           /**< new value for bound */
   BOUNDTYPE        boundtype,          /**< type of bound: lower or upper bound */
   Bool             probingchange       /**< is the bound change a temporary setting due to probing? */
   );

/** adds hole change to active node, or child of active node */
extern
RETCODE SCIPnodeAddHolechg(
   NODE*            node,               /**< node to add bound change to */
   MEMHDR*          memhdr,             /**< block memory */
   SET*             set,                /**< global SCIP settings */
   STAT*            stat,               /**< problem statistics */
   HOLELIST**       ptr,                /**< changed list pointer */
   HOLELIST*        newlist,            /**< new value of list pointer */
   HOLELIST*        oldlist             /**< old value of list pointer */
   );


/** if given value is larger than the node's lower bound, sets the node's lower bound to the new value */
extern
void SCIPnodeUpdateLowerbound(
   NODE*            node,               /**< node to update lower bound for */
   STAT*            stat,               /**< problem statistics */
   Real             newbound            /**< new lower bound for the node (if it's larger than the old one) */
   );




/*
 * Tree methods
 */

/** creates an initialized tree data structure */
extern
RETCODE SCIPtreeCreate(
   TREE**           tree,               /**< pointer to tree data structure */
   MEMHDR*          memhdr,             /**< block memory buffers */
   SET*             set,                /**< global SCIP settings */
   STAT*            stat,               /**< problem statistics */
   LP*              lp,                 /**< current LP data */
   NODESEL*         nodesel             /**< node selector to use for sorting leaves in the priority queue */
   );

/** frees tree data structure */
extern
RETCODE SCIPtreeFree(
   TREE**           tree,               /**< pointer to tree data structure */
   MEMHDR*          memhdr,             /**< block memory buffers */
   SET*             set,                /**< global SCIP settings */
   LP*              lp                  /**< current LP data */
   );

/** returns the node selector associated with the given node priority queue */
extern
NODESEL* SCIPtreeGetNodesel(
   TREE*            tree                /**< branch and bound tree */
   );

/** sets the node selector used for sorting the nodes in the priority queue, and resorts the queue if necessary */
extern
RETCODE SCIPtreeSetNodesel(
   TREE*            tree,               /**< branch and bound tree */
   SET*             set,                /**< global SCIP settings */
   STAT*            stat,               /**< problem statistics */
   NODESEL*         nodesel             /**< node selector to use for sorting the nodes in the queue */
   );

/** cuts off nodes with lower bound not better than given upper bound */
extern
RETCODE SCIPtreeCutoff(
   TREE*            tree,               /**< branch and bound tree */
   MEMHDR*          memhdr,             /**< block memory */
   SET*             set,                /**< global SCIP settings */
   LP*              lp,                 /**< current LP data */
   Real             cutoffbound         /**< cutoff bound: all nodes with lowerbound >= cutoffbound are cut off */
   );

/** constructs the LP and loads LP state for fork/subroot of the active node */
extern
RETCODE SCIPtreeLoadLP(
   TREE*            tree,               /**< branch and bound tree */
   MEMHDR*          memhdr,             /**< block memory */
   SET*             set,                /**< global SCIP settings */
   STAT*            stat,               /**< dynamic problem statistics */
   LP*              lp                  /**< current LP data */
   );

/** branches on a variable; if solution value x' is fractional, two child nodes are created
 *  (x <= floor(x'), x >= ceil(x')), if solution value is integral, three child nodes are created
 *  (x <= x'-1, x == x', x >= x'+1)
 */
extern
RETCODE SCIPtreeBranchVar(
   TREE*            tree,               /**< branch and bound tree */
   MEMHDR*          memhdr,             /**< block memory */
   SET*             set,                /**< global SCIP settings */
   STAT*            stat,               /**< problem statistics data */
   LP*              lp,                 /**< current LP data */
   BRANCHCAND*      branchcand,         /**< branching candidate storage */
   EVENTQUEUE*      eventqueue,         /**< event queue */
   VAR*             var                 /**< variable to branch on */
   );

/** switches to probing mode */
extern
RETCODE SCIPtreeStartProbing(
   TREE*            tree,               /**< branch and bound tree */
   MEMHDR*          memhdr,             /**< block memory */
   SET*             set                 /**< global SCIP settings */
   );

/** switches back from probing to normal operation mode, restores bounds of all variables and restores active constraints
 *  arrays of active node
 */
extern
RETCODE SCIPtreeEndProbing(
   TREE*            tree,               /**< branch and bound tree */
   MEMHDR*          memhdr,             /**< block memory buffers */
   SET*             set,                /**< global SCIP settings */
   STAT*            stat,               /**< problem statistics */
   LP*              lp,                 /**< current LP data */
   BRANCHCAND*      branchcand,         /**< branching candidate storage */
   EVENTQUEUE*      eventqueue          /**< event queue */
   );


#ifndef NDEBUG

/* In debug mode, the following methods are implemented as function calls to ensure
 * type validity.
 */

/** gets number of leaves */
extern
int SCIPtreeGetNLeaves(
   TREE*            tree                /**< branch and bound tree */
   );

/** gets number of nodes (children + siblings + leaves) */
extern   
int SCIPtreeGetNNodes(
   TREE*            tree                /**< branch and bound tree */
   );

/** gets current node of the tree */
extern
NODE* SCIPtreeGetCurrentNode(
   TREE*            tree                /**< branch and bound tree */
   );

/** gets depth of current node in the tree, or -1 if no current node exists */
extern
int SCIPtreeGetCurrentDepth(
   TREE*            tree                /**< branch and bound tree */
   );

/** returns, whether the LP was or is to be solved in the current node */
extern
Bool SCIPtreeHasCurrentNodeLP(
   TREE*            tree                /**< branch and bound tree */
   );

/** gets active node of the tree */
extern
NODE* SCIPtreeGetActiveNode(
   TREE*            tree                /**< branch and bound tree */
   );

/** gets depth of active node in the tree, or -1 if no active node exists */
extern
int SCIPtreeGetActiveDepth(
   TREE*            tree                /**< branch and bound tree */
   );

/** returns, whether the LP was or is to be solved in the active node */
extern
Bool SCIPtreeHasActiveNodeLP(
   TREE*            tree                /**< branch and bound tree */
   );

/** sets mark to solve or to ignore the LP while processing the active node */
extern
void SCIPtreeSetActiveNodeLP(
   TREE*            tree,               /**< branch and bound tree */
   Bool             solvelp             /**< should the LP be solved in active node? */
   );

/** returns whether the current node is a temporary probing node */
extern
Bool SCIPtreeProbing(
   TREE*            tree                /**< branch and bound tree */
   );

/** returns the temporary probing node, or NULL if the current node is not the probing node */
extern
NODE* SCIPtreeGetProbingNode(
   TREE*            tree                /**< branch and bound tree */
   );

#else

/* In optimized mode, the methods are implemented as defines to reduce the number of function calls and
 * speed up the algorithms.
 */

#define SCIPtreeGetNLeaves(tree)        SCIPnodepqLen((tree)->leaves)
#define SCIPtreeGetNNodes(tree)         ((tree)->nchildren + (tree)->nsiblings + SCIPtreeGetNLeaves(tree))
#define SCIPtreeGetActiveNode(tree)     (tree)->actnode
#define SCIPtreeGetActiveDepth(tree)    ((tree)->actnode != NULL ? (tree)->actnode->depth : -1)
#define SCIPtreeHasActiveNodeLP(tree)   (tree)->actnodehaslp
#define SCIPtreeSetActiveNodeLP(tree,solvelp)  ((tree)->actnodehaslp = solvelp)
#define SCIPtreeGetCurrentNode(tree)    ((tree)->probingnode != NULL ? (tree)->probingnode : SCIPtreeGetActiveNode(tree))
#define SCIPtreeGetCurrentDepth(tree)   ((tree)->probingnode != NULL ? (tree)->probingnode->depth \
      : SCIPtreeGetActiveDepth(tree))
#define SCIPtreeHasCurrentNodeLP(tree)  ((tree)->probingnode != NULL ? FALSE : SCIPtreeHasActiveNodeLP(tree))
#define SCIPtreeProbing(tree)           ((tree)->probingnode != NULL)
#define SCIPtreeGetProbingNode(tree)    (tree)->probingnode

#endif


/** gets the best child of the active node w.r.t. the node selection priority assigned by the branching rule */
extern
NODE* SCIPtreeGetPrioChild(
   TREE*            tree                /**< branch and bound tree */
   );

/** gets the best sibling of the active node w.r.t. the node selection priority assigned by the branching rule */
extern
NODE* SCIPtreeGetPrioSibling(
   TREE*            tree                /**< branch and bound tree */
   );

/** gets the best child of the active node w.r.t. the node selection strategy */
extern
NODE* SCIPtreeGetBestChild(
   TREE*            tree,               /**< branch and bound tree */
   SET*             set                 /**< global SCIP settings */
   );

/** gets the best sibling of the active node w.r.t. the node selection strategy */
extern
NODE* SCIPtreeGetBestSibling(
   TREE*            tree,               /**< branch and bound tree */
   SET*             set                 /**< global SCIP settings */
   );

/** gets the best leaf from the node queue w.r.t. the node selection strategy */
extern
NODE* SCIPtreeGetBestLeaf(
   TREE*            tree                /**< branch and bound tree */
   );

/** gets the best node from the tree (child, sibling, or leaf) w.r.t. the node selection strategy */
extern
NODE* SCIPtreeGetBestNode(
   TREE*            tree,               /**< branch and bound tree */
   SET*             set                 /**< global SCIP settings */
   );

/** gets the minimal lower bound of all nodes in the tree */
extern
Real SCIPtreeGetLowerbound(
   TREE*            tree,               /**< branch and bound tree */
   SET*             set                 /**< global SCIP settings */
   );

/** gets the node with minimal lower bound of all nodes in the tree (child, sibling, or leaf) */
extern
NODE* SCIPtreeGetLowerboundNode(
   TREE*            tree,               /**< branch and bound tree */
   SET*             set                 /**< global SCIP settings */
   );

/** gets the average lower bound of all nodes in the tree */
extern
Real SCIPtreeGetAvgLowerbound(
   TREE*            tree,               /**< branch and bound tree */
   Real             cutoffbound         /**< global cutoff bound */
   );

#endif
