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

/**@file   relax_stpdp.h
 * @ingroup RELAXATORS
 * @brief  Steiner tree dynamic programming relaxator
 * @author Daniel Rehfeldt
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_RELAX_STPDP_H__
#define __SCIP_RELAX_STPDP_H__


#include "scip/scip.h"
#include "graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/** creates the STP relaxator and includes it in SCIP */
SCIP_EXPORT
SCIP_RETCODE SCIPincludeRelaxStpdp(
   SCIP*                 scip                /**< SCIP data structure */
   );


/** is using the relaxator promising? */
SCIP_EXPORT
SCIP_Bool SCIPStpDpRelaxIsPromising(
   SCIP*                 scip,               /**< SCIP data structure */
   GRAPH*                graph               /**< graph */
   );


/** activates */
SCIP_EXPORT
SCIP_RETCODE SCIPStpDpRelaxActivate(
   SCIP*                 scip                /**< SCIP data structure */
   );


/** is active? */
SCIP_EXPORT
SCIP_Bool SCIPStpDpRelaxIsActive(
   SCIP*                 scip                /**< SCIP data structure */
   );

#ifdef __cplusplus
}
#endif

#endif
