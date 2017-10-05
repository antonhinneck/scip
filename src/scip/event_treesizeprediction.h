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

/**@file   event_treesizeprediction
 * @ingroup EVENTS 
 * @brief  eventhdlr for tree-size prediction related events
 * @author Pierre Le Bodic
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_EVENT_TREESIZEPREDICTION_H__
#define __SCIP_EVENT_TREESIZEPREDICTION_H__


#include "scip/scip.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Returns an estimate of the tree size that remains to explore, -1 if no estimate is available */
EXTERN
SCIP_Real SCIPtreeSizeGetEstimateRemaining(
   SCIP*                 scip                /**< SCIP data structure */
   );

/** Returns an estimate of the total tree size to explore, -1 if no estimate is available */
EXTERN
SCIP_Real SCIPtreeSizeGetEstimateTotal(
   SCIP*                 scip                /**< SCIP data structure */
   );

/** creates event handler for tree-size prediction event */
EXTERN
SCIP_RETCODE SCIPincludeEventHdlrTreeSizePrediction(
   SCIP*                 scip                /**< SCIP data structure */
   );

#ifdef __cplusplus
}
#endif

#endif
