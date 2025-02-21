/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2020 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not visit scip.zib.de.         */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   sepa_interminor.c
 * @ingroup DEFPLUGINS_SEPA
 * @brief  minor separator with intersection cuts
 * @author Felipe Serrano
 * @author Antonia Chmiela
 *
 * Let X be the matrix of auxiliary variables added for bilinear terms, X_{ij} = x_ix_j.
 * The separator enforces quadratic constraints det(2x2 minor of X) = 0 via intersection cuts.
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_SEPA_INTERMINOR_H__
#define __SCIP_SEPA_INTERMINOR_H__


#include "scip/scip.h"

#ifdef __cplusplus
extern "C" {
#endif

/** creates the interminor separator and includes it in SCIP
 *
 * @ingroup SeparatorIncludes
 */
SCIP_EXPORT
SCIP_RETCODE SCIPincludeSepaInterminor(
   SCIP*                 scip                /**< SCIP data structure */
   );

#ifdef __cplusplus
}
#endif

#endif
