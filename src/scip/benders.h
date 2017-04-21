/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2017 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   benders.h
 * @ingroup INTERNALAPI
 * @brief  internal methods for Benders' decomposition
 * @author Stephen J. Maher
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_BENDERS_H__
#define __SCIP_BENDERS_H__


#include "scip/def.h"
#include "blockmemshell/memory.h"
#include "scip/type_retcode.h"
#include "scip/type_result.h"
#include "scip/type_set.h"
#include "scip/type_lp.h"
#include "scip/type_prob.h"
#include "scip/type_pricestore.h"
#include "scip/type_benders.h"
#include "scip/type_benderscut.h"
#include "scip/pub_benders.h"

#ifdef __cplusplus
extern "C" {
#endif


/** copies the given benders to a new scip */
extern
SCIP_RETCODE SCIPbendersCopyInclude(
   SCIP_BENDERS*         benders,            /**< benders */
   SCIP_SET*             set,                /**< SCIP_SET of SCIP to copy to */
   SCIP_Bool*            valid               /**< was the copying process valid? */
   );

/** creates a variable benders */
extern
SCIP_RETCODE SCIPbendersCreate(
   SCIP_BENDERS**        benders,            /**< pointer to variable benders data structure */
   SCIP_SET*             set,                /**< global SCIP settings */
   SCIP_MESSAGEHDLR*     messagehdlr,        /**< message handler */
   BMS_BLKMEM*           blkmem,             /**< block memory for parameter settings */
   const char*           name,               /**< name of variable benders */
   const char*           desc,               /**< description of variable benders */
   int                   priority,           /**< priority of the variable benders */
   int                   nsubproblems,       /**< the number subproblems used in this decomposition */
   SCIP_Bool             cutlp,              /**< should Benders' cuts be generated for LP solutions */
   SCIP_Bool             cutpseudo,          /**< should Benders' cuts be generated for pseudo solutions */
   SCIP_Bool             cutrelax,           /**< should Benders' cuts be generated for relaxation solutions */
   SCIP_DECL_BENDERSCOPY ((*benderscopy)),   /**< copy method of benders or NULL if you don't want to copy your plugin into sub-SCIPs */
   SCIP_DECL_BENDERSFREE ((*bendersfree)),   /**< destructor of variable benders */
   SCIP_DECL_BENDERSINIT ((*bendersinit)),   /**< initialize variable benders */
   SCIP_DECL_BENDERSEXIT ((*bendersexit)),   /**< deinitialize variable benders */
   SCIP_DECL_BENDERSINITPRE((*bendersinitpre)),/**< presolving initialization method for Benders' decomposition */
   SCIP_DECL_BENDERSEXITPRE((*bendersexitpre)),/**< presolving deinitialization method for Benders' decomposition */
   SCIP_DECL_BENDERSINITSOL((*bendersinitsol)),/**< solving process initialization method of variable benders */
   SCIP_DECL_BENDERSEXITSOL((*bendersexitsol)),/**< solving process deinitialization method of variable benders */
   SCIP_DECL_BENDERSGETMASTERVAR((*bendersgetmastervar)),/**< returns the master variable for a given subproblem variable */
   SCIP_DECL_BENDERSEXEC ((*bendersexec)),   /**< the execution method of the Benders' decomposition algorithm */
   SCIP_DECL_BENDERSSOLVESUB((*benderssolvesub)),/**< the solving method for the Benders' decomposition subproblems */
   SCIP_DECL_BENDERSPOSTSOLVE((*benderspostsolve)),/**< called after the subproblems are solved. */
   SCIP_DECL_BENDERSFREESUB((*bendersfreesub)),/**< the freeing method for the Benders' decomposition subproblems */
   SCIP_BENDERSDATA*     bendersdata         /**< variable benders data */
   );

/** calls destructor and frees memory of variable benders */
extern
SCIP_RETCODE SCIPbendersFree(
   SCIP_BENDERS**        benders,            /**< pointer to variable benders data structure */
   SCIP_SET*             set                 /**< global SCIP settings */
   );

/** initializes variable benders */
extern
SCIP_RETCODE SCIPbendersInit(
   SCIP_BENDERS*         benders,            /**< variable benders */
   SCIP_SET*             set                 /**< global SCIP settings */
   );

/** calls exit method of variable benders */
extern
SCIP_RETCODE SCIPbendersExit(
   SCIP_BENDERS*         benders,            /**< variable benders */
   SCIP_SET*             set                 /**< global SCIP settings */
   );

/** informs the Benders' decomposition that the presolving process is being started */
extern
SCIP_RETCODE SCIPbendersInitpre(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_SET*             set,                /**< global SCIP settings */
   SCIP_STAT*            stat                /**< dynamic problem statistics */
   );

/** informs the Benders' decomposition that the presolving process has completed */
extern
SCIP_RETCODE SCIPbendersExitpre(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_SET*             set,                /**< global SCIP settings */
   SCIP_STAT*            stat                /**< dynamic problem statistics */
   );

/** informs variable benders that the branch and bound process is being started */
extern
SCIP_RETCODE SCIPbendersInitsol(
   SCIP_BENDERS*         benders,            /**< variable benders */
   SCIP_SET*             set                 /**< global SCIP settings */
   );

/** informs variable benders that the branch and bound process data is being freed */
extern
SCIP_RETCODE SCIPbendersExitsol(
   SCIP_BENDERS*         benders,            /**< variable benders */
   SCIP_SET*             set                 /**< global SCIP settings */
   );

/** enables or disables all clocks of \pbenders,depending on the value of the flag */
extern
void SCIPbendersEnableOrDisableClocks(
   SCIP_BENDERS*         benders,            /**< the benders for which all clocks should be enabled or disabled */
   SCIP_Bool             enable              /**< should the clocks of the benders be enabled? */
   );

/** solves the subproblem using the current master problem solution. */
extern
SCIP_RETCODE SCIPbendersExec(
   SCIP_BENDERS*         benders,            /**< variable benders */
   SCIP_SET*             set,                /**< global SCIP settings */
   SCIP_SOL*             sol,                /**< primal CIP solution */
   SCIP_RESULT*          result,             /**< result of the pricing process */
   SCIP_Bool             check               /**< is the execution method called as a check. i.e. no cuts are required */
   );

/** solves the subproblems. */
extern
SCIP_RETCODE SCIPbendersSolveSubproblem(
   SCIP_BENDERS*         benders,            /**< variable benders */
   SCIP_SET*             set,                /**< global SCIP settings */
   SCIP_SOL*             sol,                /**< primal CIP solution */
   int                   probnum,            /**< the subproblem number */
   SCIP_Bool*            infeasible          /**< returns whether the current subproblem is infeasible */
   );

/** frees the subproblems. */
extern
SCIP_RETCODE SCIPbendersFreeSubproblem(
   SCIP_BENDERS*         benders,            /**< variable benders */
   SCIP_SET*             set,                /**< global SCIP settings */
   int                   probnum             /**< the subproblem number */
   );

/** returns the variable for the given variable of the subproblem. This provides a call back for the mapping between the
 * master and subproblems */
extern
SCIP_VAR* SCIPbendersGetMasterVar(
   SCIP_BENDERS*         benders,            /**< variable benders */
   SCIP_SET*             set,                /**< global SCIP settings */
   SCIP_VAR*             var                 /**< the copy of the master variable in the subproblem */
   );

/** sets priority of variable benders */
extern
void SCIPbendersSetPriority(
   SCIP_BENDERS*         benders,            /**< variable benders */
   SCIP_SET*             set,                /**< global SCIP settings */
   int                   priority            /**< new priority of the variable benders */
   );

/** sets copy callback of benders */
extern
void SCIPbendersSetCopy(
   SCIP_BENDERS*         benders,            /**< variable benders */
   SCIP_DECL_BENDERSCOPY ((*benderscopy))    /**< copy callback of benders */
   );

/** sets destructor callback of benders */
extern
void SCIPbendersSetFree(
   SCIP_BENDERS*         benders,            /**< benders */
   SCIP_DECL_BENDERSFREE ((*bendersfree))    /**< destructor of benders */
   );

/** sets initialization callback of benders */
extern
void SCIPbendersSetInit(
   SCIP_BENDERS*         benders,            /**< benders */
   SCIP_DECL_BENDERSINIT((*bendersinit))     /**< initialize benders */
   );

/** sets deinitialization callback of benders */
extern
void SCIPbendersSetExit(
   SCIP_BENDERS*         benders,            /**< benders */
   SCIP_DECL_BENDERSEXIT((*bendersexit))     /**< deinitialize benders */
   );

/** sets presolving initialization callback of benders */
void SCIPbendersSetInitpre(
   SCIP_BENDERS*         benders,            /**< benders */
   SCIP_DECL_BENDERSINITPRE((*bendersinitpre))     /**< initialize benders */
   );

/** sets presolving deinitialization callback of benders */
void SCIPbendersSetExitpre(
   SCIP_BENDERS*         benders,            /**< benders */
   SCIP_DECL_BENDERSEXITPRE((*bendersexitpre))     /**< deinitialize benders */
   );

/** sets solving process initialization callback of benders */
extern
void SCIPbendersSetInitsol(
   SCIP_BENDERS*         benders,            /**< benders */
   SCIP_DECL_BENDERSINITSOL((*bendersinitsol))/**< solving process initialization callback of benders */
   );

/** sets solving process deinitialization callback of benders */
extern
void SCIPbendersSetExitsol(
   SCIP_BENDERS*         benders,            /**< benders */
   SCIP_DECL_BENDERSEXITSOL((*bendersexitsol))/**< solving process deinitialization callback of benders */
   );

/** sets post-solve callback of benders */
extern
void SCIPbendersSetPostsolve(
   SCIP_BENDERS*         benders,            /**< benders */
   SCIP_DECL_BENDERSPOSTSOLVE((*benderspostsolve))/**< solving process deinitialization callback of benders */
   );

/** sets the Benders' cuts sorted flags in the Benders' decomposition */
extern
void SCIPbendersSetBenderscutsSorted(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition structure */
   SCIP_Bool             sorted              /**< the value to set the sorted flag to */
   );

/** sorts benders cuts by priorities */
extern
void SCIPbendersSortBenderscuts(
   SCIP_BENDERS*         benders             /**< benders */
   );

/** sorts benders cuts by name */
extern
void SCIPbendersSortBenderscutsName(
   SCIP_BENDERS*         benders             /**< benders */
   );
#ifdef __cplusplus
}
#endif

#endif
