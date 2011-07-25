/* ----------------------------------------------------------- */
/*           _ ___   	     ___                               */
/*          |_| | |_/	  |_| | |_/    SPEECH                  */
/*          | | | | \  +  | | | | \    RECOGNITION             */
/*          =========	  =========    SOFTWARE                */
/*                                                             */
/* ================> ATK COMPATIBLE VERSION <================= */
/*                                                             */
/* ----------------------------------------------------------- */
/* developed at:                                               */
/*                                                             */
/*      Machine Intelligence Laboratory (Speech Group)         */
/*      Cambridge University Engineering Department            */
/*      http://mi.eng.cam.ac.uk/                               */
/*                                                             */
/* ----------------------------------------------------------- */
/*         Copyright:                                          */
/*                                                             */
/*                    M.J.F. Gales and                         */
/*          2001-2007 Cambridge University                     */
/*                    Engineering Department                   */
/*                                                             */
/*   Use of this software is governed by a License Agreement   */
/*    ** See the file License for the Conditions of Use  **    */
/*    **     This banner notice must not be removed      **    */
/*                                                             */
/* ----------------------------------------------------------- */
/*         File: HAdapt.h      Adaptation Library module       */
/* ----------------------------------------------------------- */

#ifndef _HADAPT_H_
#define _HADAPT_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ObsCache{
   int time;
   Vector obs;
   float det;
   struct _ObsCache *next;
} ObsCache;                        /* observation cache to save rotated observations */

typedef struct {
  char *outSpkrPat;
  char *inSpkrPat;
  char *paSpkrPat;
  char *outXFormExt;
  char *inXFormExt;
  char *paXFormExt;
  char *alXFormExt;
  char *outXFormDir;
  char *paXFormDir;
  char *alXFormDir;
  Boolean useOutXForm;
  Boolean useInXForm;
  Boolean usePaXForm;
  char *xformTMF;
  Boolean saveSpkrModels;
  Boolean saveBinary;
  AdaptXForm *inXForm;
  AdaptXForm *outXForm;
  AdaptXForm *paXForm;
  HMMSet *al_hset;
  AdaptXForm *al_inXForm;
  char *inXFormFN;    /* ATK input xform */
  ObsCache *headoc;
} XFInfo;


/* -------------------- Initialisation Functions -------------------------- */

void InitAdapt(XFInfo *xfinfo);

/*
   Initialise configuration parameters
*/

AdaptXForm *GetMLLRDiagCov(AdaptXForm *xform);

/* ---------------- Accumulation Control Functions ------------------------ */

void SetBaseAccsTime(int t);
void TidyBaseAccs();
/*
  Modifies the internal time of current frames. Is used to ensure that
  last frame is correctly added in using UpdateBaseTriMat
*/


void AccAdaptFrame(double Lr, Vector svec, MixPDF *mp, int t);
/*
   Accumulate frame stats into specific mixture comp transformed using parent
*/

void ZeroAdaptAccs(HMMSet *hset, AdaptXForm *xform);
/*
   Zero all adaptation accumulates
*/

/* ---------------- Applying Transform Functions ------------------------ */

void SetXForm(XFInfo *xfinfo, HMMSet *hset, AdaptXForm *xform);
/*
  Set the current transform to xform. This must be executed
  prior to applying any adaptation transforms. Setting xform
  to NULL turns off the input transformation.
*/

void SetParentXForm(XFInfo *xfinfo, HMMSet *hset, AdaptXForm *xform);
/*
  Set the parent transform to xform. If this is not set the
  default functionality is to build a transform on top of
  the input transform if any. Setting xform to NULL means
  build a transform on the original model set and feature
  space.
*/

void ApplyHMMSetXForm(HMMSet *hset, AdaptXForm* xform);
/*
  Apply current transform (and parents) to complete model set.
*/

void ApplyCompXForm(MixPDF *mp, AdaptXForm* xform);
/*
  Apply current transform (and parents) to a component.
*/

Vector ApplyCompFXForm(MixPDF *mp, Vector svec, AdaptXForm* xform, LogFloat *det, int t);
/*
  Apply linear transform  (and parents) to observation for a component
  return a vector of the transformed parameters.
  IMPORTANT: Do not alter the values of the returned vector
*/


void ResetObsCache(XFInfo *xfinfo);
/*
  Reset the observation cache
*/

void ResetXFormHMMSet(HMMSet *hset);
/*
  Return the model set to it's original state
  IMPORTANT: if HADAPT:STOREMINFO=FALSE is used this
  will have no affect
*/

Boolean ALoadXForm(HMMSet *hset, XFInfo *xfinfo);
/*
   ATK load feature transform (CMLLR) to associate with the model set.
*/

/* ---------------  Transform Copying Functions ----------------------- */

LinXForm *CopyLinXForm(MemHeap *x, LinXForm *xf);
/*
  Create a linxform that is a copy of xf
*/

XFormSet *CopyXFormSet(MemHeap *x, XFormSet *xfset);
/*
  Create a XFormSet that is a copy of xf
*/

AdaptXForm *CopyAdaptXForm(MemHeap *x, AdaptXForm *xform);
/*
  Create an AdaptXForm that is a copy of xf
*/


/* ---------------  Transform Estimation Functions ----------------------- */

AdaptXForm *CreateAdaptXForm(HMMSet *hset, char* xformName);
/*
  Creates a new output transform. xformName will eventually
  be used as the macroname for the transform.
*/

Boolean GenAdaptXForm(HMMSet *hset, AdaptXForm* xform);
/*
  Estimate the transform using the information and regression
  trees specified in the configuration files. Returns FALSE
  if there was insufficient data to generate a transform.
*/

Boolean HardAssign(AdaptXForm *xform);
/*
   Whether the transform uses hard assignment or not - required
   for HModel to determine how to read/write transform
*/

#ifdef __cplusplus
}
#endif

#endif  /* _HADAPT_H_ */

/* ---------------------------- END HAdapt.h ------------------------------ */
