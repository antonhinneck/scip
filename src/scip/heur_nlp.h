/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2009 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#pragma ident "@(#) $Id: heur_nlp.h,v 1.7 2009/12/01 11:54:53 bzfviger Exp $"

/**@file   heur_nlp.h
 * @brief  NLP local search primal heuristic
 * @author Stefan Vigerske
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef HEUR_NLP_H_
#define HEUR_NLP_H_

#include "scip/scip.h"

#ifdef __cplusplus
extern "C" {
#endif

/** creates the NLP local search primal heuristic and includes it in SCIP */
extern
SCIP_RETCODE SCIPincludeHeurNlp(
   SCIP*                 scip                /**< SCIP data structure */
   );

/** updates the starting point for the NLP heuristic
 * 
 * Is called by a constraint handler that handles nonlinear constraints when a check on feasibility of a solution fails.
 */
extern
SCIP_RETCODE SCIPheurNlpUpdateStartpoint(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_HEUR*            heur,               /**< NLP heuristic */
   SCIP_SOL*             solcand,            /**< solution candidate */
   SCIP_Real             violation           /**< constraint violation of solution candidate */
   );

/** Sets up an NLP so that applyNlpHeur can be called.
 * 
 * Looks at the current problem and if it has continuous variables in nonlinear constraints.
 * If so and if there is an NLP solver available, then sets up the NLP and sets *success to TRUE.
 * Otherwise returns with *success == FALSE.
 */
extern
SCIP_RETCODE SCIPsetupNlpHeur(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_HEUR*            heur,               /**< NLP heuristic */
   SCIP_Bool*            success             /**< if not NULL, indicates whether an NLP was setup */
   );

/** main procedure of the NLP heuristic */
extern
SCIP_RETCODE SCIPapplyNlpHeur(
   SCIP*                 scip,               /**< original SCIP data structure                                   */
   SCIP_HEUR*            heur,               /**< heuristic data structure                                       */
   SCIP_RESULT*          result,             /**< result data structure                                          */
   SCIP_SOL*             refpoint,           /**< point to take fixation of discrete variables from, and startpoint for NLP solver; if NULL, then LP solution is used */
   SCIP_Longint          itercontingent,     /**< iteration limit for NLP solver                                 */
   SCIP_Real             timelimit,          /**< time limit for NLP solver                                      */
   SCIP_Longint*         iterused            /**< buffer to store number of iterations used by NLP solver, or NULL if not of interest */
   );

#ifdef __cplusplus
}
#endif

#endif /*HEUR_NLP_H_*/
