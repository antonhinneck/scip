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
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   expr_erf.h
 * @brief  handler for Gaussian error function expressions
 * @author Benjamin Mueller
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_EXPR_ERF_H__
#define __SCIP_EXPR_ERF_H__

#include "scip/scip.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @{
 *
 * @name Gaussian error function expression
 *
 * This expression handler provides the Gaussian error function, that is
 *
 * \f[
 *   x \mapsto \frac{2}{\sqrt{\pi}}\int_0^x \exp(-t^2) dt.
 * \f]
 *
 * @attention The implementation of this expression handler is incomplete.
 * It is not usable for most use cases so far.
 * @{
 */

/** creates an erf expression
 *
 * @attention The implementation of `erf` expressions is incomplete.
 * They are not usable for most use cases so far.
 */
SCIP_EXPORT
SCIP_RETCODE SCIPcreateExprErf(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_EXPR**           expr,               /**< pointer where to store expression */
   SCIP_EXPR*            child,              /**< child expression */
   SCIP_DECL_EXPR_OWNERCREATE((*ownercreate)), /**< function to call to create ownerdata */
   void*                 ownercreatedata     /**< data to pass to ownercreate */
   );

/** indicates whether expression is of erf-type */
SCIP_EXPORT
SCIP_Bool SCIPisExprErf(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_EXPR*            expr                /**< expression */
   );

/** @}
  * @}
  */

/** creates the handler for erf expressions and includes it into SCIP
 *
 * @attention The implementation of this expression handler is incomplete.
 * It is not usable for most use cases so far.
 */
SCIP_EXPORT
SCIP_RETCODE SCIPincludeExprhdlrErf(
   SCIP*                 scip                /**< SCIP data structure */
   );

#ifdef __cplusplus
}
#endif

#endif /* __SCIP_EXPR_ERF_H__ */
