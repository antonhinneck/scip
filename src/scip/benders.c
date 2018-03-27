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

/**@file   benders.c
 * @brief  methods for Benders' decomposition
 * @author Stephen J. Maher
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/
//#define SCIP_DEBUG
//#define SCIP_MOREDEBUG
#include <assert.h>
#include <string.h>

#include "scip/def.h"
#include "scip/set.h"
#include "scip/clock.h"
#include "scip/paramset.h"
#include "scip/lp.h"
#include "scip/prob.h"
#include "scip/pricestore.h"
#include "scip/scip.h"
#include "scip/benders.h"
#include "scip/pub_message.h"
#include "scip/pub_misc.h"
#include "scip/cons_linear.h"

#include "scip/struct_benders.h"

/* Defaults for parameters */
#define SCIP_DEFAULT_TRANSFERCUTS          TRUE  /** Should Benders' cuts generated in LNS heuristics be transferred to the main SCIP instance? */
#define SCIP_DEFAULT_CUTSASCONSS           TRUE  /** Should the transferred cuts be added as constraints? */
#define SCIP_DEFAULT_MIPCHECKFREQ             5  /** the number of iterations that the MIP is checked, -1 for always. */
#define SCIP_DEFAULT_LNSCHECK              TRUE  /** should the Benders' decomposition be used in LNS heuristics */
#define SCIP_DEFAULT_LNSMAXDEPTH             -1  /** the maximum depth at which the LNS check is performed */
#define SCIP_DEFAULT_SUBPROBFRAC            1.0  /** the fraction of subproblems that are solved in each iteration */

#define AUXILIARYVAR_NAME     "##bendersauxiliaryvar" /** the name for the Benders' auxiliary variables in the master problem */

/* event handler properties */
#define NODEFOCUS_EVENTHDLR_NAME         "bendersnodefocus"
#define NODEFOCUS_EVENTHDLR_DESC         "node focus event handler for Benders' decomposition"
#define MIPNODEFOCUS_EVENTHDLR_NAME      "bendersmipsolvenodefocus"
#define MIPNODEFOCUS_EVENTHDLR_DESC      "node focus event handler for the MIP solve method for Benders' decomposition"
#define UPPERBOUND_EVENTHDLR_NAME        "bendersupperbound"
#define UPPERBOUND_EVENTHDLR_DESC        "found solution event handler to terminate subproblem solve for a given upper bound"


struct SCIP_EventhdlrData
{
   int                   filterpos;          /**< the event filter entry */
   int                   numruns;            /**< the number of times that the problem has been solved */
   SCIP_Real             upperbound;         /**< an upper bound for the problem */
   SCIP_Bool             solvemip;           /**< is the event called from a MIP subproblem solve*/
};


/* ---------------- Local methods for event handlers ---------------- */
/** init method for the event handlers */
static
SCIP_RETCODE initEventhandler(
   SCIP*                 scip,               /**< the SCIP data structure */
   SCIP_EVENTHDLR*       eventhdlr           /**< the event handlers data structure */
   )
{
   SCIP_EVENTHDLRDATA* eventhdlrdata;

   assert(scip != NULL);
   assert(eventhdlr != NULL);
   assert(strcmp(SCIPeventhdlrGetName(eventhdlr), MIPNODEFOCUS_EVENTHDLR_NAME) == 0);

   eventhdlrdata = SCIPeventhdlrGetData(eventhdlr);
   assert(eventhdlrdata != NULL);

   eventhdlrdata->filterpos = -1;
   eventhdlrdata->numruns = 0;
   eventhdlrdata->upperbound = -SCIPinfinity(scip);
   eventhdlrdata->solvemip = FALSE;

   return SCIP_OKAY;
}


/** initsol method for the event handlers */
static
SCIP_RETCODE initsolEventhandler(
   SCIP*                 scip,               /**< the SCIP data structure */
   SCIP_EVENTHDLR*       eventhdlr,          /**< the event handlers data structure */
   SCIP_EVENTTYPE        eventtype           /**< event type mask to select events to catch */
   )
{
   SCIP_EVENTHDLRDATA eventhdlrdata;

   assert(scip != NULL);
   assert(eventhdlr != NULL);

   eventhdlrdata = SCIPeventhdlrGetData(eventhdlr);

   SCIP_CALL(SCIPcatchEvent(scip, eventtype, eventhdlr, NULL, &eventhdlrdata->filterpos));

   return SCIP_OKAY;
}

/** the exit method for the event handlers */
static
SCIP_RETCODE exitsolEventhandler(
   SCIP*                 scip,               /**< the SCIP data structure */
   SCIP_EVENTHDLR*       eventhdlr,          /**< the event handlers data structure */
   SCIP_EVENTTYPE        eventtype           /**< event type mask to select events to catch */
   )
{
   SCIP_EVENTHDLRDATA eventhdlrdata;

   assert(scip != NULL);
   assert(eventhdlr != NULL);

   eventhdlrdata = SCIPeventhdlrGetData(eventhdlr);

   if( eventhdlrdata->filterpos >= 0 )
   {
      SCIP_CALL(SCIPdropEvent(scip, eventtype, eventhdlr, NULL, eventhdlrdata->filterpos));
      eventhdlrdata->filterpos = -1;
   }

   return SCIP_OKAY;
}

/** free method for the event handler */
static
SCIP_RETCODE freeEventhandler(
   SCIP*                 scip,               /**< the SCIP data structure */
   SCIP_EVENTHDLR*       eventhdlr           /**< the event handlers data structure */
   )
{
   SCIP_EVENTHDLRDATA* eventhdlrdata;

   assert(scip != NULL);
   assert(eventhdlr != NULL);

   eventhdlrdata = SCIPeventhdlrGetData(eventhdlr);
   assert(eventhdlrdata != NULL);

   SCIPfreeBlockMemory(scip, &eventhdlrdata);

   return SCIP_OKAY;
}



/* ---------------- Callback methods of node focus event handler ---------------- */

/** exec the event handler */
static
SCIP_DECL_EVENTEXEC(eventExecBendersNodefocus)
{  /*lint --e{715}*/
   SCIP_EVENTHDLRDATA eventhdlrdata;

   assert(scip != NULL);
   assert(eventhdlr != NULL);
   assert(strcmp(SCIPeventhdlrGetName(eventhdlr), NODEFOCUS_EVENTHDLR_NAME) == 0);

   eventhdlrdata = SCIPeventhdlrGetData(eventhdlr);

   /* sending an interrupt solve signal to return the control back to the Benders' decomposition plugin.
    * This will ensure the SCIP stage is SCIP_STAGE_SOLVING, allowing the use of probing mode. */
   SCIP_CALL( SCIPinterruptSolve(scip) );

   SCIP_CALL(SCIPdropEvent(scip, SCIP_EVENTTYPE_NODEFOCUSED, eventhdlr, NULL, eventhdlrdata->filterpos));
   eventhdlrdata->filterpos = -1;

   return SCIP_OKAY;
}

/** initialization method of event handler (called after problem was transformed) */
static
SCIP_DECL_EVENTINIT(eventInitBendersNodefocus)
{
   assert(scip != NULL);
   assert(eventhdlr != NULL);
   assert(strcmp(SCIPeventhdlrGetName(eventhdlr), NODEFOCUS_EVENTHDLR_NAME) == 0);

   SCIP_CALL( initEventhandler(scip, eventhdlr) );

   return SCIP_OKAY;
}

/** solving process initialization method of event handler (called when branch and bound process is about to begin) */
static
SCIP_DECL_EVENTINITSOL(eventInitsolBendersNodefocus)
{
   assert(scip != NULL);
   assert(eventhdlr != NULL);
   assert(strcmp(SCIPeventhdlrGetName(eventhdlr), NODEFOCUS_EVENTHDLR_NAME) == 0);

   SCIP_CALL( initsolEventhandler(scip, eventhdlr, SCIP_EVENTTYPE_NODEFOCUSED) );

   return SCIP_OKAY;
}

/** solving process deinitialization method of event handler (called before branch and bound process data is freed) */
static
SCIP_DECL_EVENTEXITSOL(eventExitsolBendersNodefocus)
{
   assert(scip != NULL);
   assert(eventhdlr != NULL);
   assert(strcmp(SCIPeventhdlrGetName(eventhdlr), NODEFOCUS_EVENTHDLR_NAME) == 0);

   SCIP_CALL( exitsolEventhandler(scip, eventhdlr, SCIP_EVENTTYPE_NODEFOCUSED) );

   return SCIP_OKAY;
}

/** deinitialization method of event handler (called before transformed problem is freed) */
static
SCIP_DECL_EVENTFREE(eventFreeBendersNodefocus)
{
   assert(scip != NULL);
   assert(eventhdlr != NULL);
   assert(strcmp(SCIPeventhdlrGetName(eventhdlr), NODEFOCUS_EVENTHDLR_NAME) == 0);

   SCIP_CALL( freeEventhandler(scip, eventhandler) );

   return SCIP_OKAY;
}


/* ---------------- Callback methods of MIP solve node focus event handler ---------------- */

/** exec the event handler */
static
SCIP_DECL_EVENTEXEC(eventExecBendersMipnodefocus)
{  /*lint --e{715}*/
   SCIP_EVENTHDLRDATA* eventhdlrdata;

   assert(scip != NULL);
   assert(eventhdlr != NULL);
   assert(strcmp(SCIPeventhdlrGetName(eventhdlr), MIPNODEFOCUS_EVENTHDLR_NAME) == 0);

   eventhdlrdata = SCIPeventhdlrGetData(eventhdlr);

   /* interrupting the solve so that the control is returned back to the Benders' core. */
   if( eventhdlrdata->numruns == 0 && !eventhdlrdata->solvemip )
      SCIP_CALL( SCIPinterruptSolve(scip) );

   SCIP_CALL(SCIPdropEvent(scip, SCIP_EVENTTYPE_NODEFOCUSED, eventhdlr, NULL, eventhdlrdata->filterpos));
   eventhdlrdata->filterpos = -1;

   eventhdlrdata->numruns++;

   return SCIP_OKAY;
}

/** initialization method of event handler (called after problem was transformed) */
static
SCIP_DECL_EVENTINIT(eventInitBendersMipnodefocus)
{
   assert(scip != NULL);
   assert(eventhdlr != NULL);
   assert(strcmp(SCIPeventhdlrGetName(eventhdlr), MIPNODEFOCUS_EVENTHDLR_NAME) == 0);

   SCIP_CALL( initEventhandler(scip, eventhdlr) );

   return SCIP_OKAY;
}

/** solving process initialization method of event handler (called when branch and bound process is about to begin) */
static
SCIP_DECL_EVENTINITSOL(eventInitsolBendersMipnodefocus)
{
   assert(scip != NULL);
   assert(eventhdlr != NULL);
   assert(strcmp(SCIPeventhdlrGetName(eventhdlr), MIPNODEFOCUS_EVENTHDLR_NAME) == 0);

   SCIP_CALL( initsolEventhandler(scip, eventhdlr, SCIP_EVENTTYPE_NODEFOCUSED) );

   return SCIP_OKAY;
}

/** solving process deinitialization method of event handler (called before branch and bound process data is freed) */
static
SCIP_DECL_EVENTEXITSOL(eventExitsolBendersMipnodefocus)
{
   assert(scip != NULL);
   assert(eventhdlr != NULL);
   assert(strcmp(SCIPeventhdlrGetName(eventhdlr), MIPNODEFOCUS_EVENTHDLR_NAME) == 0);

   SCIP_CALL( exitEventhandler(scip, eventhdlr, SCIP_EVENTTYPE_NODEFOCUSED) );

   return SCIP_OKAY;
}

/** deinitialization method of event handler (called before transformed problem is freed) */
static
SCIP_DECL_EVENTFREE(eventFreeBendersMipnodefocus)
{
   assert(scip != NULL);
   assert(eventhdlr != NULL);
   assert(strcmp(SCIPeventhdlrGetName(eventhdlr), MIPNODEFOCUS_EVENTHDLR_NAME) == 0);

   SCIP_CALL( freeEventhandler(scip, eventhandler) );

   return SCIP_OKAY;
}

/* ---------------- Callback methods of solution found event handler ---------------- */

/** exec the event handler */
static
SCIP_DECL_EVENTEXEC(eventExecBendersUpperbound)
{  /*lint --e{715}*/
   SCIP_EVENTHDLRDATA* eventhdlrdata;
   SCIP_SOL* bestsol;

   assert(scip != NULL);
   assert(eventhdlr != NULL);
   assert(strcmp(SCIPeventhdlrGetName(eventhdlr), UPPERBOUND_EVENTHDLR_NAME) == 0);

   eventhdlrdata = SCIPeventhdlrGetData(eventhdlr);
   assert(eventhdlrdata != NULL);

   bestsol = SCIPgetBestSol(scip);

   if( SCIPisLT(scip, SCIPgetSolOrigObj(scip, bestsol), eventhdlrdata->upperbound) )
      SCIP_CALL( SCIPinterruptSolve(scip) );

   return SCIP_OKAY;
}

/** initialization method of event handler (called after problem was transformed) */
static
SCIP_DECL_EVENTINIT(eventInitBendersUpperbound)
{
   assert(scip != NULL);
   assert(eventhdlr != NULL);
   assert(strcmp(SCIPeventhdlrGetName(eventhdlr), UPPERBOUND_EVENTHDLR_NAME) == 0);

   SCIP_CALL( initEventhandler(scip, eventhdlr) );

   return SCIP_OKAY;
}

/** solving process initialization method of event handler (called when branch and bound process is about to begin) */
static
SCIP_DECL_EVENTINITSOL(eventInitsolBendersUpperbound)
{
   assert(scip != NULL);
   assert(eventhdlr != NULL);
   assert(strcmp(SCIPeventhdlrGetName(eventhdlr), UPPERBOUND_EVENTHDLR_NAME) == 0);

   SCIP_CALL( initsolEventhdlr(scip, eventhdlr, SCIP_EVENTTYPE_BESTSOLFOUND) );

   return SCIP_OKAY;
}

/** solving process deinitialization method of event handler (called before branch and bound process data is freed) */
static
SCIP_DECL_EVENTEXITSOL(eventExitsolBendersUpperbound)
{
   assert(scip != NULL);
   assert(eventhdlr != NULL);
   assert(strcmp(SCIPeventhdlrGetName(eventhdlr), UPPERBOUND_EVENTHDLR_NAME) == 0);

   SCIP_CALL( exitsolEventhandler(scip, eventhdlr, SCIP_EVENTTYPE_BESTSOLFOUND) );

   return SCIP_OKAY;
}

/** deinitialization method of event handler (called before transformed problem is freed) */
static
SCIP_DECL_EVENTFREE(eventFreeBendersUpperbound)
{
   assert(scip != NULL);
   assert(eventhdlr != NULL);
   assert(strcmp(SCIPeventhdlrGetName(eventhdlr), UPPERBOUND_EVENTHDLR_NAME) == 0);

   SCIP_CALL( freeEventhandler(scip, eventhdlr) );

   return SCIP_OKAY;
}

/** updates the upper bound in the event handler data */
static
SCIP_RETCODE updateEventhdlrUpperbound(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   int                   probnumber,         /**< the subproblem number */
   SCIP_Real             upperbound          /**< the upper bound value */
   )
{
   SCIP_EVENTHDLR* eventhdlr;
   SCIP_EVENTHDLRDATA* eventhdlrdata;

   assert(benders != NULL);
   assert(probnumber >= 0 && probnumber < benders->nsubproblems);

   eventhdlr = SCIPfindEventhdlr(SCIPbendersSubproblem(benders, probnumber), UPPERBOUND_EVENTHDLR_NAME);
   assert(eventhdlr != NULL);

   eventhdlrdata = SCIPeventhdlrGetData(eventhdlr);
   assert(eventhdlrdata != NULL);

   eventhdlrdata->upperbound = upperbound;

   return SCIP_OKAY;
}

/* Local methods */

/** A workaround for GCG. This is a temp vardata that is set for the auxiliary variables */
struct SCIP_VarData
{
   int                   vartype;             /**< the variable type. In GCG this indicates whether the variable is a
                                                   master problem or subproblem variable. */
};


/** compares two benders w. r. to their priority */
SCIP_DECL_SORTPTRCOMP(SCIPbendersComp)
{  /*lint --e{715}*/
   return ((SCIP_BENDERS*)elem2)->priority - ((SCIP_BENDERS*)elem1)->priority;
}

/** comparison method for sorting benders w.r.t. to their name */
SCIP_DECL_SORTPTRCOMP(SCIPbendersCompName)
{
   return strcmp(SCIPbendersGetName((SCIP_BENDERS*)elem1), SCIPbendersGetName((SCIP_BENDERS*)elem2));
}

/** method to call, when the priority of a benders was changed */
static
SCIP_DECL_PARAMCHGD(paramChgdBendersPriority)
{  /*lint --e{715}*/
   SCIP_PARAMDATA* paramdata;

   paramdata = SCIPparamGetData(param);
   assert(paramdata != NULL);

   /* use SCIPsetBendersPriority() to mark the benderss unsorted */
   SCIPsetBendersPriority(scip, (SCIP_BENDERS*)paramdata, SCIPparamGetInt(param)); /*lint !e740*/

   return SCIP_OKAY;
}

/** copies the given benders to a new scip */
SCIP_RETCODE SCIPbendersCopyInclude(
   SCIP_BENDERS*         benders,            /**< benders */
   SCIP_SET*             sourceset,          /**< SCIP_SET of SCIP to copy from */
   SCIP_SET*             targetset,          /**< SCIP_SET of SCIP to copy to */
   SCIP_Bool*            valid               /**< was the copying process valid? */
   )
{
   SCIP_BENDERS* targetbenders;  /* the copy of the Benders' decomposition struct in the target set */

   assert(benders != NULL);
   assert(targetset != NULL);
   assert(valid != NULL);
   assert(targetset->scip != NULL);

   (*valid) = FALSE;

   if( benders->benderscopy != NULL && targetset->benders_copybenders )
   {
      SCIPsetDebugMsg(targetset, "including benders %s in subscip %p\n", SCIPbendersGetName(benders), (void*)targetset->scip);
      SCIP_CALL( benders->benderscopy(targetset->scip, benders, valid) );

      /* if the copy was valid, then the Benders cuts are copied. */
      if( (*valid) )
      {
         targetbenders = SCIPsetFindBenders(targetset, SCIPbendersGetName(benders));

         /* storing the pointer to the source scip instance */
         targetbenders->sourcescip = sourceset->scip;

         /* the flag is set to indicate that the Benders' decomposition is a copy */
         targetbenders->iscopy = TRUE;
      }
   }

   return SCIP_OKAY;
}

/** creates a Benders' decomposition structure
 *  To use the Benders' decomposition for solving a problem, it first has to be activated with a call to SCIPactivateBenders().
 */
SCIP_RETCODE SCIPbendersCreate(
   SCIP_BENDERS**        benders,            /**< pointer to Benders' decomposition data structure */
   SCIP_SET*             set,                /**< global SCIP settings */
   SCIP_MESSAGEHDLR*     messagehdlr,        /**< message handler */
   BMS_BLKMEM*           blkmem,             /**< block memory for parameter settings */
   const char*           name,               /**< name of Benders' decomposition */
   const char*           desc,               /**< description of Benders' decomposition */
   int                   priority,           /**< priority of the Benders' decomposition */
   SCIP_Bool             cutlp,              /**< should Benders' cuts be generated for LP solutions */
   SCIP_Bool             cutpseudo,          /**< should Benders' cuts be generated for pseudo solutions */
   SCIP_Bool             cutrelax,           /**< should Benders' cuts be generated for relaxation solutions */
   SCIP_DECL_BENDERSCOPY ((*benderscopy)),   /**< copy method of benders or NULL if you don't want to copy your plugin into sub-SCIPs */
   SCIP_DECL_BENDERSFREE ((*bendersfree)),   /**< destructor of Benders' decomposition */
   SCIP_DECL_BENDERSINIT ((*bendersinit)),   /**< initialize Benders' decomposition */
   SCIP_DECL_BENDERSEXIT ((*bendersexit)),   /**< deinitialize Benders' decomposition */
   SCIP_DECL_BENDERSINITPRE((*bendersinitpre)),/**< presolving initialization method for Benders' decomposition */
   SCIP_DECL_BENDERSEXITPRE((*bendersexitpre)),/**< presolving deinitialization method for Benders' decomposition */
   SCIP_DECL_BENDERSINITSOL((*bendersinitsol)),/**< solving process initialization method of Benders' decomposition */
   SCIP_DECL_BENDERSEXITSOL((*bendersexitsol)),/**< solving process deinitialization method of Benders' decomposition */
   SCIP_DECL_BENDERSGETVAR((*bendersgetvar)),/**< returns the master variable for a given subproblem variable */
   SCIP_DECL_BENDERSCREATESUB((*benderscreatesub)),/**< creates a Benders' decomposition subproblem */
   SCIP_DECL_BENDERSPRESUBSOLVE((*benderspresubsolve)),/**< called prior to the subproblem solving loop */
   SCIP_DECL_BENDERSSOLVESUB((*benderssolvesub)),/**< the solving method for the Benders' decomposition subproblems */
   SCIP_DECL_BENDERSPOSTSOLVE((*benderspostsolve)),/**< called after the subproblems are solved. */
   SCIP_DECL_BENDERSFREESUB((*bendersfreesub)),/**< the freeing method for the Benders' decomposition subproblems */
   SCIP_BENDERSDATA*     bendersdata         /**< Benders' decomposition data */
   )
{
   char paramname[SCIP_MAXSTRLEN];
   char paramdesc[SCIP_MAXSTRLEN];

   assert(benders != NULL);
   assert(name != NULL);
   assert(desc != NULL);

   /* Checking whether the benderssolvesub and the bendersfreesub are both implemented or both are not implemented */
   if( (benderssolvesub == NULL && bendersfreesub != NULL) || (benderssolvesub != NULL && bendersfreesub == NULL) )
   {
      SCIPerrorMessage("Benders' decomposition <%s> requires that both bendersSolvesub%s and bendersFreesub%s are \
         implemented or neither\n", name, name, name);
      return SCIP_INVALIDCALL;
   }

   SCIP_ALLOC( BMSallocMemory(benders) );
   BMSclearMemory(benders);
   SCIP_ALLOC( BMSduplicateMemoryArray(&(*benders)->name, name, strlen(name)+1) );
   SCIP_ALLOC( BMSduplicateMemoryArray(&(*benders)->desc, desc, strlen(desc)+1) );
   (*benders)->priority = priority;
   (*benders)->cutlp = cutlp;
   (*benders)->cutpseudo = cutpseudo;
   (*benders)->cutrelax = cutrelax;
   (*benders)->benderscopy = benderscopy;
   (*benders)->bendersfree = bendersfree;
   (*benders)->bendersinit = bendersinit;
   (*benders)->bendersexit = bendersexit;
   (*benders)->bendersinitpre = bendersinitpre;
   (*benders)->bendersexitpre = bendersexitpre;
   (*benders)->bendersinitsol = bendersinitsol;
   (*benders)->bendersexitsol = bendersexitsol;
   (*benders)->bendersgetvar = bendersgetvar;
   (*benders)->benderscreatesub = benderscreatesub;
   (*benders)->benderspresubsolve = benderspresubsolve;
   (*benders)->benderssolvesub = benderssolvesub;
   (*benders)->benderspostsolve = benderspostsolve;
   (*benders)->bendersfreesub = bendersfreesub;
   (*benders)->bendersdata = bendersdata;
   SCIP_CALL( SCIPclockCreate(&(*benders)->setuptime, SCIP_CLOCKTYPE_DEFAULT) );
   SCIP_CALL( SCIPclockCreate(&(*benders)->bendersclock, SCIP_CLOCKTYPE_DEFAULT) );

   /* add parameters */
   (void) SCIPsnprintf(paramname, SCIP_MAXSTRLEN, "benders/%s/priority", name);
   (void) SCIPsnprintf(paramdesc, SCIP_MAXSTRLEN, "priority of benders <%s>", name);
   SCIP_CALL( SCIPsetAddIntParam(set, messagehdlr, blkmem, paramname, paramdesc,
                  &(*benders)->priority, FALSE, priority, INT_MIN/4, INT_MAX/4,
                  paramChgdBendersPriority, (SCIP_PARAMDATA*)(*benders)) ); /*lint !e740*/

   (void) SCIPsnprintf(paramname, SCIP_MAXSTRLEN, "benders/%s/cutlp", name);
   SCIP_CALL( SCIPsetAddBoolParam(set, messagehdlr, blkmem, paramname,
        "should Benders' cuts be generated for LP solutions?", &(*benders)->cutlp, FALSE, cutlp, NULL, NULL) ); /*lint !e740*/

   (void) SCIPsnprintf(paramname, SCIP_MAXSTRLEN, "benders/%s/cutpseudo", name);
   SCIP_CALL( SCIPsetAddBoolParam(set, messagehdlr, blkmem, paramname,
        "should Benders' cuts be generated for pseudo solutions?", &(*benders)->cutpseudo, FALSE, cutpseudo, NULL, NULL) ); /*lint !e740*/

   (void) SCIPsnprintf(paramname, SCIP_MAXSTRLEN, "benders/%s/cutrelax", name);
   SCIP_CALL( SCIPsetAddBoolParam(set, messagehdlr, blkmem, paramname,
        "should Benders' cuts be generated for relaxation solutions?", &(*benders)->cutrelax, FALSE, cutrelax, NULL, NULL) ); /*lint !e740*/

   /* These parameters are left for the user to decide in a settings file. This departs from the usual SCIP convention
    * where the settings available at the creation of the plugin can be set in the function call. */
   (void) SCIPsnprintf(paramname, SCIP_MAXSTRLEN, "benders/%s/transfercuts", name);
   SCIP_CALL( SCIPsetAddBoolParam(set, messagehdlr, blkmem, paramname,
        "Should Benders' cuts from LNS heuristics be transferred to the main SCIP instance?", &(*benders)->transfercuts,
        FALSE, SCIP_DEFAULT_TRANSFERCUTS, NULL, NULL) ); /*lint !e740*/

   (void) SCIPsnprintf(paramname, SCIP_MAXSTRLEN, "benders/%s/mipcheckfreq", name);
   SCIP_CALL( SCIPsetAddIntParam(set, messagehdlr, blkmem, paramname,
        "The frequency at which the MIP subproblems are checked, -1 for always", &(*benders)->mipcheckfreq, FALSE,
        SCIP_DEFAULT_MIPCHECKFREQ, -1, INT_MAX, NULL, NULL) ); /*lint !e740*/

   (void) SCIPsnprintf(paramname, SCIP_MAXSTRLEN, "benders/%s/lnscheck", name);
   SCIP_CALL( SCIPsetAddBoolParam(set, messagehdlr, blkmem, paramname,
        "Should Benders' decomposition be used in LNS heurisics?", &(*benders)->lnscheck, FALSE, SCIP_DEFAULT_LNSCHECK,
        NULL, NULL) ); /*lint !e740*/

   (void) SCIPsnprintf(paramname, SCIP_MAXSTRLEN, "benders/%s/lnsmaxdepth", name);
   SCIP_CALL( SCIPsetAddIntParam(set, messagehdlr, blkmem, paramname,
        "maximal depth level at which the LNS check is performed (-1: no limit)", &(*benders)->lnsmaxdepth, TRUE,
        SCIP_DEFAULT_LNSMAXDEPTH, -1, SCIP_MAXTREEDEPTH, NULL, NULL) );

   (void) SCIPsnprintf(paramname, SCIP_MAXSTRLEN, "benders/%s/cutsasconss", name);
   SCIP_CALL( SCIPsetAddBoolParam(set, messagehdlr, blkmem, paramname,
        "Should the transferred cuts be added as constraints?", &(*benders)->cutsasconss, FALSE,
        SCIP_DEFAULT_CUTSASCONSS, NULL, NULL) ); /*lint !e740*/

   (void) SCIPsnprintf(paramname, SCIP_MAXSTRLEN, "benders/%s/subprobfrac", name);
   SCIP_CALL( SCIPsetAddRealParam(set, messagehdlr, blkmem, paramname,
        "The fraction of subproblems that are solved in each iteration.", &(*benders)->subprobfrac, FALSE,
        SCIP_DEFAULT_SUBPROBFRAC, 0.0, 1.0, NULL, NULL) ); /*lint !e740*/

   return SCIP_OKAY;
}

/** calls destructor and frees memory of Benders' decomposition */
SCIP_RETCODE SCIPbendersFree(
   SCIP_BENDERS**        benders,            /**< pointer to Benders' decomposition data structure */
   SCIP_SET*             set                 /**< global SCIP settings */
   )
{
   assert(benders != NULL);
   assert(*benders != NULL);
   assert(!(*benders)->initialized);
   assert(set != NULL);

   /* call destructor of Benders' decomposition */
   if( (*benders)->bendersfree != NULL )
   {
      SCIP_CALL( (*benders)->bendersfree(set->scip, *benders) );
   }

   SCIPclockFree(&(*benders)->bendersclock);
   SCIPclockFree(&(*benders)->setuptime);
   BMSfreeMemoryArray(&(*benders)->name);
   BMSfreeMemoryArray(&(*benders)->desc);
   BMSfreeMemory(benders);

   return SCIP_OKAY;
}

/** initialises a MIP subproblem by putting the problem into SCIP_STAGE_SOLVING. This is achieved by calling SCIPsolve
 *  and then interrupting the solve in a node focus event handler.
 *  The LP subproblem is also initialised using this method; however, a different event handler is added. This event
 *  handler will put the LP subproblem into probing mode.
 *  The MIP solving function is called to initialise the subproblem because this function calls SCIPsolve with the
 *  appropriate parameter settings for Benders' decomposition.
 */
static
SCIP_RETCODE initialiseSubproblem(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   int                   probnumber          /**< the subproblem number */
   )
{
   SCIP* subproblem;
   SCIP_Bool infeasible;
   SCIP_Bool cutoff;

   assert(benders != NULL);
   assert(probnumber >= 0 && probnumber < SCIPbendersGetNSubproblems(benders));

   subproblem = SCIPbendersSubproblem(benders, probnumber);
   assert(subproblem != NULL);

   /* Getting the problem into the right SCIP stage for solving */
   SCIP_CALL( SCIPbendersSolveSubproblemMIP(benders, probnumber, &infeasible, SCIP_BENDERSENFOTYPE_LP, FALSE) );

   assert(SCIPgetStage(subproblem) == SCIP_STAGE_SOLVING);

   /* Constructing the LP that can be solved in later iterations */
   SCIP_CALL( SCIPconstructLP(subproblem, &cutoff) );

   return SCIP_OKAY;
}


/** initialises an LP subproblem by putting the problem into probing mode. The probing mode is envoked in a node focus
 *  event handler. This event handler is added just prior to calling the initialise subproblem function.
 */
static
SCIP_RETCODE initialiseLPSubproblem(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   int                   probnumber          /**< the subproblem number */
   )
{
   SCIP* subproblem;
   SCIP_EVENTHDLR* eventhdlr;
   SCIP_EVENTHDLRDATA* eventhdlrdata;
   SCIP_Bool infeasible;
   SCIP_Bool cutoff;

   assert(benders != NULL);
   assert(probnumber >= 0 && probnumber < SCIPbendersGetNSubproblems(benders));

   subproblem = SCIPbendersSubproblem(benders, probnumber);
   assert(subproblem != NULL);

   /* include event handler into SCIP */
   SCIP_CALL( SCIPallocBlockMemory(subproblem, &eventhdlrdata) );
   SCIP_CALL( SCIPincludeEventhdlrBasic(subproblem, &eventhdlr, NODEFOCUS_EVENTHDLR_NAME, NODEFOCUS_EVENTHDLR_DESC,
         eventExecBendersNodefocus, eventhdlrdata) );
   SCIP_CALL( SCIPsetEventhdlrInit(subproblem, eventhdlr, eventInitBendersNodefocus) );
   SCIP_CALL( SCIPsetEventhdlrInitsol(subproblem, eventhdlr, eventInitsolBendersNodefocus) );
   SCIP_CALL( SCIPsetEventhdlrExitsol(subproblem, eventhdlr, eventExitsolBendersNodefocus) );
   SCIP_CALL( SCIPsetEventhdlrFree(subproblem, eventhdlr, eventFreeBendersNodefocus) );
   assert(eventhdlr != NULL);

   /* calling an initial solve to put the problem into probing mode */
   SCIP_CALL( initialiseSubproblem(benders, probnumber) );

   return SCIP_OKAY;
}


/** creates the subproblems and registers it with the Benders' decomposition struct */
static
SCIP_RETCODE createSubproblems(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_SET*             set                 /**< global SCIP settings */
   )
{
   SCIP* subproblem;
   SCIP_EVENTHDLR* eventhdlr;
   int nbinvars;
   int nintvars;
   int nimplintvars;
   int nsubproblems;
   int i;

   assert(benders != NULL);
   assert(set != NULL);

   /* if the subproblems have already been created, then they will not be created again. This is the case if the
    * transformed problem has been freed and then retransformed. The subproblems should only be created when the problem
    * is first transformed. */
   if( benders->subprobscreated )
      return SCIP_OKAY;

   nsubproblems = SCIPbendersGetNSubproblems(benders);

   /* creating all subproblems */
   for( i = 0; i < nsubproblems; i++ )
   {
      /* calling the create subproblem call back method */
      SCIP_CALL( benders->benderscreatesub(set->scip, benders, i) );

      subproblem = SCIPbendersSubproblem(benders, i);

      assert(subproblem != NULL);

      /* setting global limits for the subproblems. This overwrites the limits set by the user */
      SCIP_CALL( SCIPsetIntParam(subproblem, "limits/maxorigsol", 0) );

      /* getting the number of integer and binary variables to determine the problem type */
      SCIP_CALL( SCIPgetVarsData(subproblem, NULL, NULL, &nbinvars, &nintvars, &nimplintvars, NULL) );

      /* if there are no binary and integer variables, then the subproblem is an LP.
       * In this case, the SCIP instance is put into probing mode via the use of an event handler. */
      if( nbinvars == 0 && nintvars == 0 && nimplintvars == 0 )
      {
         SCIPbendersSetSubprobIsLP(benders, i, TRUE);

         /* if the user has not implemented a solve subproblem callback, then the subproblem solves are performed
          * internally. To be more efficient the subproblem is put into probing mode. */
         if( benders->benderssolvesub == NULL && SCIPgetStage(subproblem) <= SCIP_STAGE_PROBLEM )
            SCIP_CALL( initialiseLPSubproblem(benders, i) );
      }
      else
      {
         SCIP_EVENTHDLRDATA* eventhdlrdata_mipnodefocus;
         SCIP_EVENTHDLRDATA* eventhdlrdata_upperbound;

         SCIPbendersSetSubprobIsLP(benders, i, FALSE);

         /* because the subproblems could be reused in the copy, the event handler is not created again.
          * NOTE: This currently works with the benders_default implementation. It may not be very general. */
         if( benders->benderssolvesub == NULL && !benders->iscopy )
         {
            SCIP_CALL( SCIPallocBlockMemory(subproblem, &eventhdlrdata_mipnodefocus) );
            SCIP_CALL( SCIPallocBlockMemory(subproblem, &eventhdlrdata_upperbound) );
            eventhdlrdata_mipnodefocus->numruns = 0;
            eventhdlrdata_mipnodefocus->solvemip = FALSE;
            eventhdlrdata_upperbound->upperbound = -SCIPinfinity(subproblem);

            /* include the first LP solved event handler into the subproblem */
            SCIP_CALL( SCIPincludeEventhdlrBasic(subproblem, &eventhdlr, MIPNODEFOCUS_EVENTHDLR_NAME,
                  MIPNODEFOCUS_EVENTHDLR_DESC, eventExecBendersMipnodefocus, eventhdlrdata_mipnodefocus) );
            SCIP_CALL( SCIPsetEventhdlrInit(subproblem, eventhdlr, eventInitBendersMipnodefocus) );
            SCIP_CALL( SCIPsetEventhdlrInitsol(subproblem, eventhdlr, eventInitsolBendersMipnodefocus) );
            SCIP_CALL( SCIPsetEventhdlrExitsol(subproblem, eventhdlr, eventExitsolBendersMipnodefocus) );
            SCIP_CALL( SCIPsetEventhdlrFree(subproblem, eventhdlr, eventFreeBendersMipnodefocus) );
            assert(eventhdlr != NULL);


            /* include the upper bound interrupt event handler into the subproblem */
            SCIP_CALL( SCIPincludeEventhdlrBasic(subproblem, &eventhdlr, UPPERBOUND_EVENTHDLR_NAME,
                  UPPERBOUND_EVENTHDLR_DESC, eventExecBendersUpperbound, eventhdlrdata_upperbound) );
            SCIP_CALL( SCIPsetEventhdlrInit(subproblem, eventhdlr, eventInitBendersUpperbound) );
            SCIP_CALL( SCIPsetEventhdlrInitsol(subproblem, eventhdlr, eventInitsolBendersUpperbound) );
            SCIP_CALL( SCIPsetEventhdlrExitsol(subproblem, eventhdlr, eventExitsolBendersUpperbound) );
            SCIP_CALL( SCIPsetEventhdlrFree(subproblem, eventhdlr, eventFreeBendersUpperbound) );
            assert(eventhdlr != NULL);
         }
      }
   }

   benders->subprobscreated = TRUE;

   return SCIP_OKAY;

}


/** initializes Benders' decomposition */
SCIP_RETCODE SCIPbendersInit(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_SET*             set                 /**< global SCIP settings */
   )
{
   assert(benders != NULL);
   assert(set != NULL);

   if( benders->initialized )
   {
      SCIPerrorMessage("Benders' decomposition <%s> already initialized\n", benders->name);
      return SCIP_INVALIDCALL;
   }

   if( set->misc_resetstat )
   {
      SCIPclockReset(benders->setuptime);
      SCIPclockReset(benders->bendersclock);

      benders->ncalls = 0;
      benders->ncutsfound = 0;
      benders->ntransferred = 0;
   }

   /* start timing */
   SCIPclockStart(benders->setuptime, set);

   /* creates the subproblems and sets up the probing mode for LP subproblems. This function calls the benderscreatesub
    * callback. */
   SCIP_CALL( createSubproblems(benders, set) );

   if( benders->bendersinit != NULL )
      SCIP_CALL( benders->bendersinit(set->scip, benders) );

   benders->initialized = TRUE;

   /* stop timing */
   SCIPclockStop(benders->setuptime, set);

   return SCIP_OKAY;
}


/** calls exit method of Benders' decomposition */
SCIP_RETCODE SCIPbendersExit(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_SET*             set                 /**< global SCIP settings */
   )
{
   assert(benders != NULL);
   assert(set != NULL);

   if( !benders->initialized )
   {
      SCIPerrorMessage("Benders' decomposition <%s> not initialized\n", benders->name);
      return SCIP_INVALIDCALL;
   }

   /* start timing */
   SCIPclockStart(benders->setuptime, set);

   if( benders->bendersexit != NULL )
      SCIP_CALL( benders->bendersexit(set->scip, benders) );

   benders->initialized = FALSE;

   /* stop timing */
   SCIPclockStop(benders->setuptime, set);

   return SCIP_OKAY;
}

/** informs the Benders' decomposition that the presolving process is being started */
SCIP_RETCODE SCIPbendersInitpre(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_SET*             set,                /**< global SCIP settings */
   SCIP_STAT*            stat                /**< dynamic problem statistics */
   )
{
   assert(benders != NULL);
   assert(set != NULL);
   assert(stat != NULL);

   /* call presolving initialization method of Benders' decomposition */
   if( benders->bendersinitpre != NULL )
   {
      /* start timing */
      SCIPclockStart(benders->setuptime, set);

      SCIP_CALL( benders->bendersinitpre(set->scip, benders) );

      /* stop timing */
      SCIPclockStop(benders->setuptime, set);
   }

   return SCIP_OKAY;
}


/** informs the Benders' decomposition that the presolving process has completed */
SCIP_RETCODE SCIPbendersExitpre(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_SET*             set,                /**< global SCIP settings */
   SCIP_STAT*            stat                /**< dynamic problem statistics */
   )
{
   assert(benders != NULL);
   assert(set != NULL);
   assert(stat != NULL);

   /* call presolving  deinitialization method of Benders' decomposition */
   if( benders->bendersexitpre != NULL )
   {
      /* start timing */
      SCIPclockStart(benders->setuptime, set);

      SCIP_CALL( benders->bendersexitpre(set->scip, benders) );

      /* stop timing */
      SCIPclockStop(benders->setuptime, set);
   }

   return SCIP_OKAY;
}

/** informs Benders' decomposition that the branch and bound process is being started */
SCIP_RETCODE SCIPbendersInitsol(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_SET*             set                 /**< global SCIP settings */
   )
{
   assert(benders != NULL);
   assert(set != NULL);

   /* call solving process initialization method of Benders' decomposition */
   if( benders->bendersinitsol != NULL )
   {
      /* start timing */
      SCIPclockStart(benders->setuptime, set);

      SCIP_CALL( benders->bendersinitsol(set->scip, benders) );

      /* stop timing */
      SCIPclockStop(benders->setuptime, set);
   }

   return SCIP_OKAY;
}

/** informs Benders' decomposition that the branch and bound process data is being freed */
SCIP_RETCODE SCIPbendersExitsol(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_SET*             set                 /**< global SCIP settings */
   )
{
   assert(benders != NULL);
   assert(set != NULL);

   /* call solving process deinitialization method of Benders' decomposition */
   if( benders->bendersexitsol != NULL )
   {
      /* start timing */
      SCIPclockStart(benders->setuptime, set);

      SCIP_CALL( benders->bendersexitsol(set->scip, benders) );

      /* stop timing */
      SCIPclockStop(benders->setuptime, set);
   }

   return SCIP_OKAY;
}

/** activates benders such that it is called in LP solving loop */
SCIP_RETCODE SCIPbendersActivate(
   SCIP_BENDERS*         benders,            /**< the Benders' decomposition structure */
   SCIP_SET*             set,                /**< global SCIP settings */
   int                   nsubproblems        /**< the number subproblems used in this decomposition */
   )
{
   int i;

   assert(benders != NULL);
   assert(set != NULL);
   assert(set->stage == SCIP_STAGE_INIT || set->stage == SCIP_STAGE_PROBLEM);

   if( !benders->active )
   {
      benders->active = TRUE;
      set->nactivebenders++;
      set->benderssorted = FALSE;

      benders->nsubproblems = nsubproblems;

      /* allocating memory for the subproblems arrays */
      SCIP_ALLOC( BMSallocMemoryArray(&benders->subproblems, benders->nsubproblems) );
      SCIP_ALLOC( BMSallocMemoryArray(&benders->auxiliaryvars, benders->nsubproblems) );
      SCIP_ALLOC( BMSallocMemoryArray(&benders->subprobobjval, benders->nsubproblems) );
      SCIP_ALLOC( BMSallocMemoryArray(&benders->bestsubprobobjval, benders->nsubproblems) );
      SCIP_ALLOC( BMSallocMemoryArray(&benders->subprobislp, benders->nsubproblems) );
      SCIP_ALLOC( BMSallocMemoryArray(&benders->mastervarscont, benders->nsubproblems) );

      for( i = 0; i < benders->nsubproblems; i++ )
      {
         benders->subproblems[i] = NULL;
         benders->auxiliaryvars[i] = NULL;
         benders->subprobobjval[i] = SCIPsetInfinity(set);
         benders->bestsubprobobjval[i] = SCIPsetInfinity(set);
         benders->subprobislp[i] = FALSE;
         benders->mastervarscont[i] = FALSE;
      }
   }

   return SCIP_OKAY;
}

/** deactivates benders such that it is no longer called in LP solving loop */
void SCIPbendersDeactivate(
   SCIP_BENDERS*         benders,            /**< the Benders' decomposition structure */
   SCIP_SET*             set                 /**< global SCIP settings */
   )
{
   assert(benders != NULL);
   assert(set != NULL);
   assert(set->stage == SCIP_STAGE_INIT || set->stage == SCIP_STAGE_PROBLEM);

   if( benders->active )
   {
#ifndef NDEBUG
      /* checking whether the auxiliary variables and subproblems are all NULL */
      int nsubproblems;
      int i;

      nsubproblems = SCIPbendersGetNSubproblems(benders);

      for( i = 0; i < nsubproblems; i++ )
      {
         assert(benders->auxiliaryvars[i] == NULL);
         assert(benders->subproblems[i] == NULL);
      }
#endif

      benders->active = FALSE;
      set->nactivebenders--;
      set->benderssorted = FALSE;

      /* freeing the memory allocated during the activation of the Benders' decomposition */
      BMSfreeMemoryArray(&benders->mastervarscont);
      BMSfreeMemoryArray(&benders->subprobislp);
      BMSfreeMemoryArray(&benders->bestsubprobobjval);
      BMSfreeMemoryArray(&benders->subprobobjval);
      BMSfreeMemoryArray(&benders->auxiliaryvars);
      BMSfreeMemoryArray(&benders->subproblems);
   }
}

/** returns whether the given Benders decomposition is in use in the current problem */
SCIP_Bool SCIPbendersIsActive(
   SCIP_BENDERS*         benders             /**< the Benders' decomposition structure */
   )
{
   assert(benders != NULL);

   return benders->active;
}

/** solves the subproblem using the current master problem solution. */
/*  TODO: consider allowing the possibility to pass solution information back from the subproblems instead of the scip
 *  instance. This would allow the use of different solvers for the subproblems, more importantly allowing the use of an
 *  LP solver for LP subproblems. */
SCIP_RETCODE SCIPbendersExec(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_SET*             set,                /**< global SCIP settings */
   SCIP_SOL*             sol,                /**< primal CIP solution */
   SCIP_RESULT*          result,             /**< result of the pricing process */
   SCIP_Bool*            infeasible,         /**< is the master problem infeasible with respect to the Benders' cuts? */
   SCIP_BENDERSENFOTYPE  type,               /**< the type of solution being enforced */
   SCIP_Bool             checkint            /**< should the integer solution be checked by the subproblems */
   )
{
   /* This is the main algorithm loop for the Benders' decomposition framework.
      The loop has been removed for this stage of the merge request. */

   return SCIP_OKAY;
}

/** frees the subproblems. */
SCIP_RETCODE SCIPbendersFreeSubproblem(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_SET*             set,                /**< global SCIP settings */
   int                   probnum             /**< the subproblem number */
   )
{
   assert(benders != NULL);
   assert(benders->bendersfreesub != NULL || (benders->bendersfreesub == NULL && benders->benderssolvesub == NULL));
   assert(probnum >= 0 && probnum < benders->nsubproblems);

   if( benders->bendersfreesub != NULL )
      SCIP_CALL( benders->bendersfreesub(set->scip, benders, probnum) );

   return SCIP_OKAY;
}

/** returns the corresponding master or subproblem variable for the given variable.
 * This provides a call back for the variable mapping between the master and subproblems */
SCIP_RETCODE SCIPbendersGetVar(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_SET*             set,                /**< global SCIP settings */
   SCIP_VAR*             var,                /**< the variable for which the corresponding variable is desired */
   SCIP_VAR**            mappedvar,          /**< the variable that is mapped to var */
   int                   probnumber          /**< the problem number for the desired variable, -1 for the master problem */
   )
{
   assert(benders != NULL);
   assert(set != NULL);
   assert(var != NULL);
   assert(mappedvar != NULL);
   assert(benders->bendersgetvar != NULL);

   (*mappedvar) = NULL;

   /* if the variable name matches the auxiliary variable, then the master variable is returned as NULL */
   if( strstr(SCIPvarGetName(var), AUXILIARYVAR_NAME) != NULL )
      return SCIP_OKAY;

   SCIP_CALL( benders->bendersgetvar(set->scip, benders, var, mappedvar, probnumber) );

   return SCIP_OKAY;
}

/** gets user data of Benders' decomposition */
SCIP_BENDERSDATA* SCIPbendersGetData(
   SCIP_BENDERS*         benders             /**< Benders' decomposition */
   )
{
   assert(benders != NULL);

   return benders->bendersdata;
}

/** sets user data of Benders' decomposition; user has to free old data in advance! */
void SCIPbendersSetData(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_BENDERSDATA*     bendersdata         /**< new Benders' decomposition user data */
   )
{
   assert(benders != NULL);

   benders->bendersdata = bendersdata;
}

/** stores the original parameters from the subproblem */
static
SCIP_RETCODE storeOrigSubprobParams(
   SCIP*                 scip,               /**< the SCIP data structure */
   SCIP_SUBPROBPARAMS*   origparams          /**< the original subproblem parameters */
   )
{
   assert(scip != NULL);
   assert(origparams != NULL);

   SCIP_CALL( SCIPgetBoolParam(scip, "conflict/enable", &origparams->conflict_enable) );
   SCIP_CALL( SCIPgetIntParam(scip, "lp/disablecutoff", &origparams->lp_disablecutoff) );
   SCIP_CALL( SCIPgetIntParam(scip, "lp/scaling", &origparams->lp_scaling) );
   SCIP_CALL( SCIPgetCharParam(scip, "lp/initalgorithm", &origparams->lp_initalg) );
   SCIP_CALL( SCIPgetCharParam(scip, "lp/resolvealgorithm", &origparams->lp_resolvealg) );
   SCIP_CALL( SCIPgetBoolParam(scip, "misc/alwaysgetduals", &origparams->misc_alwaysgetduals) );
   SCIP_CALL( SCIPgetBoolParam(scip, "misc/scaleobj", &origparams->misc_scaleobj) );
   SCIP_CALL( SCIPgetBoolParam(scip, "misc/catchctrlc", &origparams->misc_catchctrlc) );
   SCIP_CALL( SCIPgetIntParam(scip, "propagating/maxrounds", &origparams->prop_maxrounds) );
   SCIP_CALL( SCIPgetIntParam(scip, "propagating/maxroundsroot", &origparams->prop_maxroundsroot) );
   SCIP_CALL( SCIPgetIntParam(scip, "constraints/linear/propfreq", &origparams->cons_linear_propfreq) );

   return SCIP_OKAY;
}

/** sets the parameters for the subproblem */
static
SCIP_RETCODE setSubprobParams(
   SCIP*                 scip                /**< the SCIP data structure */
   )
{
   assert(scip != NULL);

   /* Do we have to disable presolving? If yes, we have to store all presolving parameters. */
   SCIP_CALL( SCIPsetPresolving(scip, SCIP_PARAMSETTING_OFF, TRUE) );

   /* Disabling heuristics so that the problem is not trivially solved */
   SCIP_CALL( SCIPsetHeuristics(scip, SCIP_PARAMSETTING_OFF, TRUE) );

   /* store parameters that are changed for the generation of the subproblem cuts */
   SCIP_CALL( SCIPsetParam(scip, "conflict/enable", FALSE) );

   SCIP_CALL( SCIPsetIntParam(scip, "lp/disablecutoff", 1) );
   SCIP_CALL( SCIPsetIntParam(scip, "lp/scaling", 0) );

   SCIP_CALL( SCIPsetCharParam(scip, "lp/initalgorithm", 'd') );
   SCIP_CALL( SCIPsetCharParam(scip, "lp/resolvealgorithm", 'd') );

   SCIP_CALL( SCIPsetBoolParam(scip, "misc/alwaysgetduals", TRUE) );
   SCIP_CALL( SCIPsetBoolParam(scip, "misc/scaleobj", FALSE) );

   /* do not abort subproblem on CTRL-C */
   SCIP_CALL( SCIPsetBoolParam(scip, "misc/catchctrlc", FALSE) );

   SCIP_CALL( SCIPsetIntParam(scip, "display/verblevel", (int)SCIP_VERBLEVEL_NONE) );

   SCIP_CALL( SCIPsetIntParam(scip, "propagating/maxrounds", 0) );
   SCIP_CALL( SCIPsetIntParam(scip, "propagating/maxroundsroot", 0) );

   SCIP_CALL( SCIPsetIntParam(scip, "constraints/linear/propfreq", -1) );

   return SCIP_OKAY;
}

/** resets the original parameters from the subproblem */
static
SCIP_RETCODE resetOrigSubprobParams(
   SCIP*                 scip,               /**< the SCIP data structure */
   SCIP_SUBPROBPARAMS*   origparams          /**< the original subproblem parameters */
   )
{
   assert(scip != NULL);
   assert(origparams != NULL);

   SCIP_CALL( SCIPsetBoolParam(scip, "conflict/enable", origparams->conflict_enable) );
   SCIP_CALL( SCIPsetIntParam(scip, "lp/disablecutoff", origparams->lp_disablecutoff) );
   SCIP_CALL( SCIPsetIntParam(scip, "lp/scaling", origparams->lp_scaling) );
   SCIP_CALL( SCIPsetCharParam(scip, "lp/initalgorithm", origparams->lp_initalg) );
   SCIP_CALL( SCIPsetCharParam(scip, "lp/resolvealgorithm", origparams->lp_resolvealg) );
   SCIP_CALL( SCIPsetBoolParam(scip, "misc/alwaysgetduals", origparams->misc_alwaysgetduals) );
   SCIP_CALL( SCIPsetBoolParam(scip, "misc/scaleobj", origparams->misc_scaleobj) );
   SCIP_CALL( SCIPsetBoolParam(scip, "misc/catchctrlc", origparams->misc_catchctrlc) );
   SCIP_CALL( SCIPsetIntParam(scip, "propagating/maxrounds", origparams->prop_maxrounds) );
   SCIP_CALL( SCIPsetIntParam(scip, "propagating/maxroundsroot", origparams->prop_maxroundsroot) );
   SCIP_CALL( SCIPsetIntParam(scip, "constraints/linear/propfreq", origparams->cons_linear_propfreq) );

   return SCIP_OKAY;
}

/** solves the Benders' decomposition subproblem. */
SCIP_RETCODE SCIPbendersSolveSubproblemMIP(
   SCIP_BENDERS*         benders,            /**< the Benders' decomposition data structure */
   int                   probnumber,         /**< the subproblem number */
   SCIP_Bool*            infeasible,         /**< returns whether the current subproblem is infeasible */
   SCIP_BENDERSENFOTYPE  type,               /**< the enforcement type calling this function */
   SCIP_Bool             initialisation,   /**< indicates whether the MIP is solved as part of an initalisation */
   SCIP_Bool             solvemip            /**< directly solve the MIP subproblem */
   )
{
   SCIP* subproblem;
   SCIP_SUBPROBPARAMS* origparams;
   SCIP_Bool mipchecksolve;         /* flag to indicate whether the MIP problem is solved during the CHECK.
                                       In this case, the subproblems may be interrupted because of the upper bound. */

   assert(benders != NULL);
   assert(infeasible != NULL);

   (*infeasible) = FALSE;
   mipchecksolve = FALSE;


   /* TODO: This should be solved just as an LP, so as a MIP. There is too much overhead with the MIP.
    * Need to change status check for checking the LP. */
   subproblem = SCIPbendersSubproblem(benders, probnumber);

   /* allocating memory for the parameter storage */
   SCIP_CALL( SCIPallocBlockMemory(subproblem, &origparams) );

   /* store the original parameters of the subproblem */
   SCIP_CALL( storeOrigSubprobParams(subproblem, origparams) );

   /* If the solve has been stopped for the subproblem, then we need to restart it to complete the solve. The subproblem
    * is stopped when it is a MIP so that LP cuts and IP cuts can be generated. */
   if( SCIPgetStage(subproblem) == SCIP_STAGE_SOLVING )
   {
      /* the subproblem should be in probing mode. Otherwise, the eventhandler did not work correctly */
      assert( SCIPinProbing(subproblem) );

      /* the probing mode needs to be stopped so that the MIP can be solved */
      SCIP_CALL( SCIPendProbing(subproblem) );

      /* the problem was interrupted in the event handler, so SCIP needs to be informed that the problem is to be restarted */
      SCIP_CALL( SCIPrestartSolve(subproblem) );

      /* if the solve type is for CHECK, then the FEASIBILITY emphasis setting is used. */
      if( type == SCIP_BENDERSENFOTYPE_CHECK )
      {
         //SCIP_CALL( SCIPsetEmphasis(subproblem, SCIP_PARAMEMPHASIS_FEASIBILITY, TRUE) );

         SCIP_CALL( SCIPsetHeuristics(subproblem, SCIP_PARAMSETTING_FAST, TRUE) );

         /* the number of solution improvements is limited to try and prove feasibility quickly */
         /* NOTE: This should be a parameter */
         //SCIP_CALL( SCIPsetIntParam(subproblem, "limits/bestsol", 5) );
      }

      mipchecksolve = TRUE;
   }
   else if( solvemip )
   {
      /* if the MIP will be solved directly, then the probing mode needs to be skipped. This is done by dropping the
       * node focus event */
      SCIP_EVENTHDLR* eventhdlr;
      SCIP_EVENTHDLRDATA* eventhdlrdata;

      eventhdlr = SCIPfindEventhdlr(subproblem, MIPNODEFOCUS_EVENTHDLR_NAME);
      eventhdlrdata = SCIPeventhdlrGetData(eventhdlr);

      eventhdlrdata->solvemip = TRUE;
   }
   else
   {
      /* if the problem is not in probing mode, then we need to solve the LP. That requires all methods that will
       * modify the structure of the problem need to be deactivated */

      /* setting the subproblem parameters */
      SCIP_CALL( setSubprobParams(subproblem) );

      //if( type != CHECK )
         //SCIP_CALL( SCIPsetIntParam(subproblem, "limits/nodes", 0) );
#ifdef SCIP_MOREDEBUG
      SCIP_CALL( SCIPsetBoolParam(subproblem, "display/lpinfo", TRUE) );
#endif
   }

#ifdef SCIP_MOREDEBUG
      SCIP_CALL( SCIPsetIntParam(subproblem, "display/verblevel", (int)SCIP_VERBLEVEL_FULL) );
#endif

   SCIP_CALL( SCIPsolve(subproblem) );

   /* if the problem is still in the solving stage, then this indicates that the LP needs to be solved for the Benders'
    * cuts. */
   assert((SCIPgetStage(subproblem) == SCIP_STAGE_SOLVING && (initialisation || mipchecksolve)) ||
      (SCIPgetStage(subproblem) == SCIP_STAGE_SOLVED && !initialisation));

   if( SCIPgetStatus(subproblem) == SCIP_STATUS_INFEASIBLE )
      (*infeasible) = TRUE;
   else if( SCIPgetStatus(subproblem) != SCIP_STATUS_OPTIMAL && SCIPgetStatus(subproblem) != SCIP_STATUS_USERINTERRUPT
      && SCIPgetStatus(subproblem) != SCIP_STATUS_BESTSOLLIMIT )
      assert(FALSE);

   //SCIP_CALL( SCIPprintStatistics(subprob, NULL) );

   /* resetting the subproblem parameters */
   SCIP_CALL( resetOrigSubprobParams(subproblem, origparams) );

   /* freeing the parameter storage */
   SCIPfreeBlockMemory(subproblem, &origparams);

   return SCIP_OKAY;
}

/** sets copy callback of benders */
void SCIPbendersSetCopy(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_DECL_BENDERSCOPY ((*benderscopy))    /**< copy callback of benders */
   )
{
   assert(benders != NULL);

   benders->benderscopy = benderscopy;
}

/** sets destructor callback of Benders' decomposition */
void SCIPbendersSetFree(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_DECL_BENDERSFREE ((*bendersfree))    /**< destructor of Benders' decomposition */
   )
{
   assert(benders != NULL);

   benders->bendersfree = bendersfree;
}

/** sets initialization callback of Benders' decomposition */
void SCIPbendersSetInit(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_DECL_BENDERSINIT((*bendersinit))     /**< initialize the Benders' decomposition */
   )
{
   assert(benders != NULL);

   benders->bendersinit = bendersinit;
}

/** sets deinitialization callback of Benders' decomposition */
void SCIPbendersSetExit(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_DECL_BENDERSEXIT((*bendersexit))     /**< deinitialize the Benders' decomposition */
   )
{
   assert(benders != NULL);

   benders->bendersexit = bendersexit;
}

/** sets presolving initialization callback of Benders' decomposition */
void SCIPbendersSetInitpre(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_DECL_BENDERSINITPRE((*bendersinitpre))/**< initialize presolving for Benders' decomposition */
   )
{
   assert(benders != NULL);

   benders->bendersinitpre = bendersinitpre;
}

/** sets presolving deinitialization callback of Benders' decomposition */
void SCIPbendersSetExitpre(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_DECL_BENDERSEXITPRE((*bendersexitpre))/**< deinitialize presolving for Benders' decomposition */
   )
{
   assert(benders != NULL);

   benders->bendersexitpre = bendersexitpre;
}

/** sets solving process initialization callback of Benders' decomposition */
void SCIPbendersSetInitsol(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_DECL_BENDERSINITSOL((*bendersinitsol))/**< solving process initialization callback of Benders' decomposition */
   )
{
   assert(benders != NULL);

   benders->bendersinitsol = bendersinitsol;
}

/** sets solving process deinitialization callback of Benders' decomposition */
void SCIPbendersSetExitsol(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_DECL_BENDERSEXITSOL((*bendersexitsol))/**< solving process deinitialization callback of Benders' decomposition */
   )
{
   assert(benders != NULL);

   benders->bendersexitsol = bendersexitsol;
}

/** sets the pre subproblem solve callback of Benders' decomposition */
void SCIPbendersSetPresubsolve(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_DECL_BENDERSPRESUBSOLVE((*benderspresubsolve))/**< called prior to the subproblem solving loop */
   )
{
   assert(benders != NULL);

   benders->benderspresubsolve = benderspresubsolve;
}

/** sets solve callback of Benders' decomposition */
void SCIPbendersSetSolvesub(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_DECL_BENDERSSOLVESUB((*benderssolvesub))/**< solving method for a Benders' decomposition subproblem */
   )
{
   assert(benders != NULL);

   benders->benderssolvesub = benderssolvesub;
}

/** sets post-solve callback of Benders' decomposition */
void SCIPbendersSetPostsolve(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_DECL_BENDERSPOSTSOLVE((*benderspostsolve))/**< solving process deinitialization callback of Benders' decomposition */
   )
{
   assert(benders != NULL);

   benders->benderspostsolve = benderspostsolve;
}

/** sets free subproblem callback of Benders' decomposition */
void SCIPbendersSetFreesub(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_DECL_BENDERSFREESUB((*bendersfreesub))/**< the freeing callback for the subproblem */
   )
{
   assert(benders != NULL);

   benders->bendersfreesub = bendersfreesub;
}

/** gets name of Benders' decomposition */
const char* SCIPbendersGetName(
   SCIP_BENDERS*         benders             /**< Benders' decomposition */
   )
{
   assert(benders != NULL);

   return benders->name;
}

/** gets description of Benders' decomposition */
const char* SCIPbendersGetDesc(
   SCIP_BENDERS*         benders             /**< Benders' decomposition */
   )
{
   assert(benders != NULL);

   return benders->desc;
}

/** gets priority of Benders' decomposition */
int SCIPbendersGetPriority(
   SCIP_BENDERS*         benders             /**< Benders' decomposition */
   )
{
   assert(benders != NULL);

   return benders->priority;
}

/** sets priority of Benders' decomposition */
void SCIPbendersSetPriority(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   SCIP_SET*             set,                /**< global SCIP settings */
   int                   priority            /**< new priority of the Benders' decomposition */
   )
{
   assert(benders != NULL);
   assert(set != NULL);

   benders->priority = priority;
   set->benderssorted = FALSE;
}

/** gets the number of subproblems for the Benders' decomposition */
int SCIPbendersGetNSubproblems(
   SCIP_BENDERS*         benders             /**< the Benders' decomposition data structure */
   )
{
   assert(benders != NULL);

   return benders->nsubproblems;
}

/** returns the SCIP instance for a given subproblem */
SCIP* SCIPbendersSubproblem(
   SCIP_BENDERS*         benders,            /**< the Benders' decomposition data structure */
   int                   probnumber          /**< the subproblem number */
   )
{
   assert(benders != NULL);
   assert(probnumber >= 0 && probnumber < benders->nsubproblems);

   return benders->subproblems[probnumber];
}

/** gets the number of times, the benders was called and tried to find a variable with negative reduced costs */
int SCIPbendersGetNCalls(
   SCIP_BENDERS*         benders             /**< Benders' decomposition */
   )
{
   assert(benders != NULL);

   return benders->ncalls;
}

/** gets the number of optimality cuts found by the collection of Benders' decomposition subproblems */
int SCIPbendersGetNCutsFound(
   SCIP_BENDERS*         benders             /**< Benders' decomposition */
   )
{
   assert(benders != NULL);

   return benders->ncutsfound;
}

/** gets time in seconds used in this benders for setting up for next stages */
SCIP_Real SCIPbendersGetSetupTime(
   SCIP_BENDERS*         benders             /**< Benders' decomposition */
   )
{
   assert(benders != NULL);

   return SCIPclockGetTime(benders->setuptime);
}

/** gets time in seconds used in this benders */
SCIP_Real SCIPbendersGetTime(
   SCIP_BENDERS*         benders             /**< Benders' decomposition */
   )
{
   assert(benders != NULL);

   return SCIPclockGetTime(benders->bendersclock);
}

/** enables or disables all clocks of \p benders, depending on the value of the flag */
void SCIPbendersEnableOrDisableClocks(
   SCIP_BENDERS*         benders,            /**< the benders for which all clocks should be enabled or disabled */
   SCIP_Bool             enable              /**< should the clocks of the benders be enabled? */
   )
{
   assert(benders != NULL);

   SCIPclockEnableOrDisable(benders->setuptime, enable);
   SCIPclockEnableOrDisable(benders->bendersclock, enable);
}

/** is Benders' decomposition initialized? */
SCIP_Bool SCIPbendersIsInitialized(
   SCIP_BENDERS*         benders             /**< Benders' decomposition */
   )
{
   assert(benders != NULL);

   return benders->initialized;
}

/** are Benders' cuts generated from the LP solutions? */
SCIP_Bool SCIPbendersCutLP(
   SCIP_BENDERS*         benders             /**< Benders' decomposition */
   )
{
   assert(benders != NULL);

   return benders->cutlp;
}

/** are Benders' cuts generated from the pseudo solutions? */
SCIP_Bool SCIPbendersCutPseudo(
   SCIP_BENDERS*         benders             /**< Benders' decomposition */
   )
{
   assert(benders != NULL);

   return benders->cutpseudo;
}

/** are Benders' cuts generated from the relaxation solutions? */
SCIP_Bool SCIPbendersCutRelaxation(
   SCIP_BENDERS*         benders             /**< Benders' decomposition */
   )
{
   assert(benders != NULL);

   return benders->cutrelax;
}

/* sets the flag indicating whether a subproblem is an LP. It is possible that this can change during the solving
 * process. One example is when the three-phase method is employed, where the first phase solves the of both the master
 * and subproblems and by the third phase the integer subproblem is solved. */
void SCIPbendersSetSubprobIsLP(
   SCIP_BENDERS*         benders,            /**< Benders' decomposition */
   int                   probnumber,         /**< the subproblem number */
   SCIP_Bool             islp                /**< flag to indicate whether the subproblem is an LP */
   )
{
   assert(benders != NULL);
   assert(probnumber >= 0 && probnumber < SCIPbendersGetNSubproblems(benders));

   if( islp && !benders->subprobislp[probnumber] )
      benders->nlpsubprobs++;
   else if( !islp && benders->subprobislp[probnumber] )
      benders->nlpsubprobs--;

   benders->subprobislp[probnumber] = islp;

   assert(benders->nlpsubprobs >= 0 && benders->nlpsubprobs <= benders->nsubproblems);
}
