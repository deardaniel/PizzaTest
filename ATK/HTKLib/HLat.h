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
/* author: Gunnar Evermann <ge204@eng.cam.ac.uk>               */
/* ----------------------------------------------------------- */
/*         Copyright:                                          */
/*         2001-2006  Cambridge University                     */
/*                    Engineering Department                   */
/*                                                             */
/*   Use of this software is governed by a License Agreement   */
/*    ** See the file License for the Conditions of Use  **    */
/*    **     This banner notice must not be removed      **    */
/*                                                             */
/* ----------------------------------------------------------- */
/*       File: HLat.h:  Lattice Manipulation                   */
/* ----------------------------------------------------------- */

/* !HVER!HLat: 1.6.0 [SJY 01/06/07] */


#ifndef _HLAT_H_
#define _HLAT_H_

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------ Initialisation --------------------------- */

void InitLat (void);
/*
   register module & set configuration parameters
*/

/* ------------------------ Datatype ----------------------------- */

/* Forward-Backward info structre attached to LNodes
     use doubles for imporved accuracy
*/
typedef struct FBlnodeInfo {
   LogDouble fwlike;     /* forward likelihood */
   LogDouble bwlike;     /* backward likelihood */
} FBinfo;

#define LNodeFw(ln)  (((FBinfo *) (ln)->hook)->fwlike)
#define LNodeBw(ln)  (((FBinfo *) (ln)->hook)->bwlike)

typedef enum {LATFB_SUM, LATFB_MAX} LatFBType;


/* ------------------------ Prototypes --------------------------- */

Transcription *LatFindBest (MemHeap *heap, Lattice *lat, int N);

void LatSetScores (Lattice *lat);

Lattice *LatPrune (MemHeap *heap, Lattice *lat, LogDouble thresh, float arcsPerSec);

void CalcStats (Lattice *lat);

LNode *LatStartNode (Lattice *lat);

LNode *LatEndNode (Lattice *lat);

void LatSetBoundaryWords (char *start, char *end, char  *startLM, char *endLM);

void LatCheck (Lattice *lat);

void FixBadLat (Lattice *lat);

void FixPronProbs (Lattice *lat, Vocab *voc);

Boolean LatTopSort (Lattice *lat, LNode **topOrder);

void LatAttachInfo (MemHeap *heap, size_t size, Lattice *lat);
void LatDetachInfo (MemHeap *heap, Lattice *lat);

LogDouble LatForwBackw (Lattice *lat, LatFBType type);

Lattice *LatExpand (MemHeap *heap, Lattice *lat, LModel *lm);

Lattice *GetLattice (char *fn, char *path, char *ext,
                     /* arguments of ReadLattice() below */
                     MemHeap *heap, Vocab *voc,
                     Boolean shortArc, Boolean add2Dict);


#ifdef __cplusplus
}
#endif

#endif  /* _HLAT_H_ */
