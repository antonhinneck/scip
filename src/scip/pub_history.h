/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2021 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not visit scipopt.org.         */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   pub_history.h
 * @ingroup PUBLICCOREAPI
 * @brief  public methods for branching and inference history structure
 * @author Stefan Heinz
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_PUB_HISTORY_H__
#define __SCIP_PUB_HISTORY_H__

#include "scip/def.h"
#include "scip/type_history.h"

#ifdef NDEBUG
#include "scip/struct_history.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** gets the conflict score of the history entry */
SCIP_EXPORT
SCIP_Real SCIPhistoryGetVSIDS(
   SCIP_HISTORY*         history,            /**< branching and inference history */
   SCIP_BRANCHDIR        dir                 /**< branching direction */
   );

/** gets the average conflict length of the history entry */
SCIP_EXPORT
SCIP_Real SCIPhistoryGetAvgConflictlength(
   SCIP_HISTORY*         history,            /**< branching and inference history */
   SCIP_BRANCHDIR        dir                 /**< branching direction */
   );

/** get number of cutoffs counter */
SCIP_EXPORT
SCIP_Real SCIPhistoryGetCutoffSum(
   SCIP_HISTORY*         history,            /**< branching and inference history */
   SCIP_BRANCHDIR        dir                 /**< branching direction (downwards, or upwards) */
   );

/** get number of inferences counter */
SCIP_EXPORT
SCIP_Real SCIPhistoryGetInferenceSum(
   SCIP_HISTORY*         history,            /**< branching and inference history */
   SCIP_BRANCHDIR        dir                 /**< branching direction (downwards, or upwards) */
   );

/** return the number of (domain) values for which a history exists */
SCIP_EXPORT
int SCIPvaluehistoryGetNValues(
   SCIP_VALUEHISTORY*    valuehistory        /**< value based history */
   );

/** return the array containing the histories for the individual (domain) values */
SCIP_EXPORT
SCIP_HISTORY** SCIPvaluehistoryGetHistories(
   SCIP_VALUEHISTORY*    valuehistory        /**< value based history */
   );

/** return the array containing the (domain) values for which a history exists */
SCIP_EXPORT
SCIP_Real* SCIPvaluehistoryGetValues(
   SCIP_VALUEHISTORY*    valuehistory        /**< value based history */
   );

#ifdef NDEBUG

/* In optimized mode, the methods are implemented as defines to reduce the number of function calls and
 * speed up the algorithms.
 */

#define SCIPhistoryGetVSIDS(history,dir)   ((history)->vsids[dir])
#define SCIPhistoryGetAvgConflictlength(history,dir) ((history)->conflengthsum[dir] > 0.0 \
      ? (SCIP_Real)(history)->nactiveconflicts[dir]/(SCIP_Real)(history)->conflengthsum[dir] : 0.0)
#define SCIPhistoryGetCutoffSum(history,dir)        ((history)->cutoffsum[dir])
#define SCIPhistoryGetInferenceSum(history,dir)     ((history)->inferencesum[dir])
#define SCIPvaluehistoryGetNValues(valuehistory)     (valuehistory)->nvalues
#define SCIPvaluehistoryGetHistories(valuehistory)      (valuehistory)->histories
#define SCIPvaluehistoryGetValues(valuehistory)      (valuehistory)->values

#endif


#ifdef __cplusplus
}
#endif

#endif
