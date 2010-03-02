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
#pragma ident "@(#) $Id: struct_primalex.h,v 1.1.2.1 2010/03/02 17:20:52 bzfwolte Exp $"

/**@file   struct_primalex.h
 * @brief  datastructures for collecting exact primal CIP solutions and exact primal information
 * @author Kati Wolter
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_STRUCT_PRIMALEX_H__
#define __SCIP_STRUCT_PRIMALEX_H__


#include "scip/def.h"
#include "scip/type_solex.h"
#include "scip/type_primalex.h"



/** exact primal data and solution storage */
struct SCIP_Primalex
{
   SCIP_SOLEX**          sols;               /**< exact primal CIP solutions */
   int                   solssize;           /**< size of sols array */
   int                   nsols;              /**< number of exact primal CIP solutions stored in sols array */
};


#endif
