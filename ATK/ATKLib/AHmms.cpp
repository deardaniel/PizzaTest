/* ----------------------------------------------------------- */
/*                                                             */
/*                        _ ___                                */
/*                       /_\ | |_/                             */
/*                       | | | | \                             */
/*                       =========                             */
/*                                                             */
/*        Real-time API for HTK-base Speech Recognition        */
/*                                                             */
/*       Machine Intelligence Laboratory (Speech Group)        */
/*        Cambridge University Engineering Department          */
/*                  http://mi.eng.cam.ac.uk/                   */
/*                                                             */
/*               Copyright CUED 2000-2007                      */
/*                                                             */
/*   Use of this software is governed by a License Agreement   */
/*    ** See the file License for the Conditions of Use  **    */
/*    **     This banner notice must not be removed      **    */
/*                                                             */
/* ----------------------------------------------------------- */
/*     File: AHmms.cpp -   Implementation of HmmSet  Class     */
/* ----------------------------------------------------------- */

char * ahmms_version="!HVER!AHmms: 1.6.0 [SJY 01/06/07]";

#include "AHmms.h"

#define T_LOAD 001     /* HMM Set Loading */
#define T_INFO 002     /* Print info on HMM set */

// Constructor: builds a HMMSet from name:info in the config file
AHmms::AHmms(const string& name):AResource(name)
{
   ConfParam *cParm[MAXGLOBS];       /* config parameters */
   int numParm;
   int i;
   Boolean hasMMF = FALSE;
   XFormAdded = FALSE;
   char buf[100],buf1[100],buf2[256];
   string xformfn;

   hmmList="hmmlist"; hmmExt=hmmDir="";
   trace = 0;

   // Read configuration file
   strcpy(buf,name.c_str());
   for (i=0; i<int(strlen(buf)); i++) buf[i] = toupper(buf[i]);
   numParm = GetConfig(buf, TRUE, cParm, MAXGLOBS);
   if (numParm>0){
      if (GetConfStr(cParm,numParm,"HMMLIST",buf2)) hmmList = buf2;
      if (GetConfStr(cParm,numParm,"HMMDIR",buf2)) hmmDir = buf2;
      if (GetConfStr(cParm,numParm,"XFORMNAME",buf2)) xformfn = buf2;
      if (GetConfInt(cParm,numParm,"TRACE",&i)) trace = i;
      for (i=0; i<10; i++){
         mmfn[i]="";
         sprintf(buf1,"MMF%d",i);
         if (GetConfStr(cParm,numParm,buf1,buf2)){
            if (i>0 && mmfn[i-1]==""){
               HRError(10500,"AHmms:  mmf0, mmf1, .... must be used sequentially");
               throw ATK_Error(10500);
            }
            mmfn[i]= buf2; hasMMF = TRUE;
         }
      }
   }

   // Create Empty HMM Set
   hset = new HMMSet;
   CreateHeap(&hmem, buf,  MSTAK, 1, 0.0, 100000, 800000 );
   CreateHMMSet(hset,&hmem,TRUE);

   // Add any mmf files
   if (hasMMF){
      for (i=0; i<10; i++) {
         if (mmfn[i]=="") break;
         strcpy(buf,mmfn[i].c_str());
         if (trace&T_LOAD)
            printf("AHmms: adding mmf%d=%s\n",i,buf);
         AddMMF(hset,buf);
      }
   }

   // Load the model data
   if (trace&T_LOAD)
      printf("Loading models ...."); fflush(stdout);
   strcpy(buf,hmmList.c_str());
   if(MakeHMMSet(hset,buf)<SUCCESS) {
      HRError(10500,"AHmms: MakeHMMSet failed [hmmlist=%s]",buf);
      throw HTK_Error(10500);
   }
   strcpy(buf1,hmmDir.c_str()); strcpy(buf2,hmmExt.c_str());
   if(LoadHMMSet(hset,buf1,buf2)<SUCCESS) {
      HRError(9999,"AHmms: LoadHMMSet failed [Dir=%s, Ext=%s",buf1,buf2);
      throw HTK_Error(10500);
   }
   ConvDiagC(hset,TRUE);

   InitAdapt(&xfinfo);
   if (xformfn!="" && !XFormAdded)
     AddXForm((char *)xformfn.c_str());

   if (trace&T_LOAD)  printf("done\n");
   if (trace&T_INFO) PrintHSetProfile(stdout,hset);
}

AHmms::AHmms(const string& name, const string& hmmlist, const string& mmf0,
             const string& mmf1, int trace):AResource(name)
{
   ConfParam *cParm[MAXGLOBS];       /* config parameters */
   int numParm;
   int i;
   char buf[100];

   trace=0;

   // Read configuration file
   strcpy(buf,name.c_str());
   for (i=0; i<int(strlen(buf)); i++) buf[i] = toupper(buf[i]);
   numParm = GetConfig(buf, TRUE, cParm, MAXGLOBS);
   if (numParm>0){
      if (GetConfInt(cParm,numParm,"TRACE",&i)) trace = i;
   }

   // Create Empty HMM Set
   hset = new HMMSet;
   CreateHeap(&hmem, buf,  MSTAK, 1, 0.0, 100000, 800000 );
   CreateHMMSet(hset,&hmem,TRUE);

   // Add mmf file
   strcpy(buf,mmf0.c_str());   // load main MMF (mandatory)
   AddMMF(hset,buf);
   strcpy(buf,mmf1.c_str());   // load aux MMF (optional)
   if (strlen(buf)>0) AddMMF(hset,buf);

   // Load the model data
   if (trace&T_LOAD)
      printf("Loading models ...."); fflush(stdout);
   hmmList = hmmlist;
   strcpy(buf,hmmList.c_str());
   if(MakeHMMSet(hset,buf)<SUCCESS) {
      HRError(10500,"AHmms: MakeHMMSet failed [hmmlist=%s]",buf);
      throw HTK_Error(10500);
   }
   if(LoadHMMSet(hset,"","")<SUCCESS) {
      HRError(9999,"AHmms: LoadHMMSet failed");
      throw HTK_Error(10500);
   }
   ConvDiagC(hset,TRUE);
   InitAdapt(&xfinfo);
   if (trace&T_LOAD)  printf("done\n");
   if (trace&T_INFO) PrintHSetProfile(stdout,hset);
}


AHmms::~AHmms()
{
   DeleteHeap(&hmem);
   delete hset;
}

// Get properties
ParmKind AHmms::GetParmKind() { return hset->pkind; }
HSetKind AHmms::GetKind(){ return hset->hsKind; }
HMMSet *AHmms::GetHMMSet(){ return hset; }
int AHmms::GetNumLogHMM(){ return hset->numLogHMM; }
int AHmms::GetNumPhyHMM(){ return hset->numPhyHMM; }
int AHmms::GetNumStates(){ return hset->numStates; }
int AHmms::GetNumSharedStates(){ return hset->numSharedStates; }
int AHmms::GetNumMix(){  return hset->numMix; }
int AHmms::GetNumSharedMix(){ return hset->numSharedMix; }
int AHmms::GetNumTransP(){ return hset->numTransP; }

// Check compatibility with given coder
Boolean AHmms::CheckCompatible(Observation *o)
{
  if (o->pk != hset->pkind) return FALSE;
   if (o->swidth[0] != hset->swidth[0]) return FALSE;
   for (int i = 1; i<=hset->swidth[0]; i++)
      if (o->swidth[i] != hset->swidth[i]) return FALSE;
      return TRUE;
}


Boolean AHmms::AddXForm(char *name)
{
  xfinfo.useInXForm = TRUE;
  xfinfo.inXFormFN = name;
  XFormAdded = ALoadXForm(hset, &xfinfo);
  return XFormAdded;
}
// ------------------------ End AHmms.cpp ---------------------


