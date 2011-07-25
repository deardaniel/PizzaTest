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
/*       File: ANGram.cpp -  NGram Language Model Class        */
/* ----------------------------------------------------------- */


char * angram_version="!HVER!ANGram: 1.6.0 [SJY 01/06/07]";

#include "ANGram.h"

#define T_LOAD 001     /* NGram LM file Loading */

static int trace = 0;

// Initialise grammar from given file
void ANGram::InitFromFile(const char * ngramFN)
{
   FILE *nf;
   char gFN[512];
   Boolean ok;

   if (trace&T_LOAD) printf("Loading NGram LM File %s\n",ngramFN);
   // Open the ngram file and attach to a source
   strcpy(gFN,ngramFN);
   ok = ( (nf = fopen(gFN,"r")) != NULL)?TRUE:FALSE;
   if (!ok){
      if (gFN[0]=='/') {
         gFN[0] = gFN[1]; gFN[1]=':';
         ok = ((nf = fopen(gFN,"r")) != NULL)?TRUE:FALSE;
      }
   }
   if (!ok){
      HRError(11210,"ANGram: Cannot open NGram file %s",ngramFN);
      throw ATK_Error(11210);
   }
   fclose(nf);

   CreateHeap(&lmHeap,"LModel mem",MSTAK,1,0.5,1000,20000);

   lm = ReadLModel(&lmHeap,gFN);
    if (lm == NULL){
      HRError(11210,"ANGram: Cannot create NGram LM from file %s",ngramFN);
      throw ATK_Error(11210);
   }
   if (trace&T_LOAD) printf("NGram LM File %s loaded\n",ngramFN);
}

// Constructor: builds an NGram LM from name:info in the config file
ANGram::ANGram(const string& name):AResource(name)
{
   ConfParam *cParm[MAXGLOBS];       /* config parameters */
   int numParm,i;
   char buf[100],buf1[100],gramFN[100];

   isOpen = TRUE; gramFN[0] = '\0';
   lm = NULL;
   // Read configuration file
   strcpy(buf,name.c_str());
   for (i=0; i<int(strlen(buf)); i++) buf[i] = toupper(buf[i]);
   numParm = GetConfig(buf, TRUE, cParm, MAXGLOBS);
   if (numParm>0){
      if (GetConfInt(cParm,numParm,"TRACE",&i)) trace = i;
      if (GetConfStr(cParm,numParm,"NGRAMFILE",buf1)) strcpy(gramFN,buf1);
   }
   if (gramFN[0] != '\0') {
      InitFromFile(gramFN);
   }
   isOpen = FALSE;
}

// Construct gram from specified NGram file
ANGram::ANGram(const string& name, const string& fname):AResource(name)
{
   ConfParam *cParm[MAXGLOBS];       /* config parameters */
   int numParm,i;
   char buf[100];

   isOpen = TRUE;
   // Read configuration file
   strcpy(buf,name.c_str());
   for (i=0; i<int(strlen(buf)); i++) buf[i] = toupper(buf[i]);
   numParm = GetConfig(buf, TRUE, cParm, MAXGLOBS);
   if (numParm>0){
      if (GetConfInt(cParm,numParm,"TRACE",&i)) trace = i;
   }
   if (fname!="") {
      InitFromFile(fname.c_str());
   }
   isOpen = FALSE;
}

// destructor
ANGram::~ANGram()
{
   DeleteHeap(&lmHeap);
}


// ------------------------ End ANGram.cpp ---------------------
