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
#pragma ident "@(#) $Id: presol.h,v 1.16 2004/06/29 17:55:05 bzfpfend Exp $"

/**@file   presol.h
 * @brief  internal methods for presolvers
 * @author Tobias Achterberg
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __PRESOL_H__
#define __PRESOL_H__


#include "def.h"
#include "memory.h"
#include "type_retcode.h"
#include "type_result.h"
#include "type_set.h"
#include "type_scip.h"
#include "type_presol.h"
#include "pub_presol.h"



/** creates a presolver */
extern
RETCODE SCIPpresolCreate(
   PRESOL**         presol,             /**< pointer to store presolver */
   SET*             set,                /**< global SCIP settings */
   MEMHDR*          memhdr,             /**< block memory for parameter settings */
   const char*      name,               /**< name of presolver */
   const char*      desc,               /**< description of presolver */
   int              priority,           /**< priority of the presolver */
   DECL_PRESOLFREE  ((*presolfree)),    /**< destructor of presolver to free user data (called when SCIP is exiting) */
   DECL_PRESOLINIT  ((*presolinit)),    /**< initialization method of presolver (called after problem was transformed) */
   DECL_PRESOLEXIT  ((*presolexit)),    /**< deinitialization method of presolver (called before transformed problem is freed) */
   DECL_PRESOLINITPRE((*presolinitpre)),/**< presolving initialization method of presolver (called when presolving is about to begin) */
   DECL_PRESOLEXITPRE((*presolexitpre)),/**< presolving deinitialization method of presolver (called after presolving has been finished) */
   DECL_PRESOLEXEC  ((*presolexec)),    /**< execution method of presolver */
   PRESOLDATA*      presoldata          /**< presolver data */
   );

/** frees memory of presolver */
extern
RETCODE SCIPpresolFree(
   PRESOL**         presol,             /**< pointer to presolver data structure */
   SCIP*            scip                /**< SCIP data structure */   
   );

/** initializes presolver */
extern
RETCODE SCIPpresolInit(
   PRESOL*          presol,             /**< presolver */
   SCIP*            scip                /**< SCIP data structure */   
   );

/** deinitializes presolver */
extern
RETCODE SCIPpresolExit(
   PRESOL*          presol,             /**< presolver */
   SCIP*            scip                /**< SCIP data structure */   
   );

/** informs presolver that the presolving process is being started */
extern
RETCODE SCIPpresolInitpre(
   PRESOL*          presol,             /**< presolver */
   SCIP*            scip,               /**< SCIP data structure */   
   RESULT*          result              /**< pointer to store the result of the callback method */
   );

/** informs presolver that the presolving process is finished */
extern
RETCODE SCIPpresolExitpre(
   PRESOL*          presol,             /**< presolver */
   SCIP*            scip,               /**< SCIP data structure */   
   RESULT*          result              /**< pointer to store the result of the callback method */
   );

/** executes presolver */
extern
RETCODE SCIPpresolExec(
   PRESOL*          presol,             /**< presolver */
   SET*             set,                /**< global SCIP settings */
   int              nrounds,            /**< number of presolving rounds already done */
   int*             nfixedvars,         /**< pointer to total number of variables fixed of all presolvers */
   int*             naggrvars,          /**< pointer to total number of variables aggregated of all presolvers */
   int*             nchgvartypes,       /**< pointer to total number of variable type changes of all presolvers */
   int*             nchgbds,            /**< pointer to total number of variable bounds tightend of all presolvers */
   int*             naddholes,          /**< pointer to total number of domain holes added of all presolvers */
   int*             ndelconss,          /**< pointer to total number of deleted constraints of all presolvers */
   int*             nupgdconss,         /**< pointer to total number of upgraded constraints of all presolvers */
   int*             nchgcoefs,          /**< pointer to total number of changed coefficients of all presolvers */
   int*             nchgsides,          /**< pointer to total number of changed left/right hand sides of all presolvers */
   RESULT*          result              /**< pointer to store the result of the callback method */
   );

/** sets priority of presolver */
extern
void SCIPpresolSetPriority(
   PRESOL*          presol,             /**< presolver */
   SET*             set,                /**< global SCIP settings */
   int              priority            /**< new priority of the presolver */
   );


#endif
