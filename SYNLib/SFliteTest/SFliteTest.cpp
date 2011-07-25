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
/*               Copyright CUED 2000-2006                      */
/*                                                             */
/*   Use of this software is governed by a License Agreement   */
/*    ** See the file License for the Conditions of Use  **    */
/*    **     This banner notice must not be removed      **    */
/*                                                             */
/* ----------------------------------------------------------- */
/*   File: SFliteTest.cpp -     Test FLite                     */
/* ----------------------------------------------------------- */


static const char * version="!HVER!SFliteTest:   2.0.0 [SJY 27/11/06]";

#include "flite.h"
#include "cmu_us_kal16.h"
#include "AHTK.h"

int main(int argc, char *argv[])
{
   char *s,*fn;
   cst_voice *voice;     // synthesis voice
   cst_utterance *utt;   // current utterance
   cst_wave *cstwave;       // synthesised wave
   Wave w;  // HTK wave
   short *p;
   HTime sampPeriod = 625.0;
   int n;
   MemHeap mem;
   AudioOut a;

   try {
      if (InitHTK(argc,argv,version)<SUCCESS){
         ReportErrors("Main",0); exit(-1);
      }
      if (NumArgs() !=2) {
         printf("SFliteTest synthstring file\n"); exit(0);
      }
      CreateHeap(&mem,"heap",MSTAK,1,0.0,10000,100000);
      s = GetStrArg();
      fn = GetStrArg();
      printf("Synth: %s -> %s\n",s,fn);
      // initialise Edinburgh cst lib
      cst_regex_init();
      // setup the voice
      voice = register_cmu_us_kal16(NULL);
      // convert text to waveform
      utt = flite_synth_text(s,voice);
      if (utt==NULL) {
         HRError(12001,"SFliteTest: cant synthesise %s\n",s);
         throw ATK_Error(12001);
      }
      cstwave = utt_wave(utt);
      p = cstwave->samples; n = cstwave->num_samples;
      w = OpenWaveOutput(&mem,&sampPeriod,n);
      printf("%d samples created\n",n);
      PutWaveSample(w,n,p);
      if (CloseWaveOutput(w,WAV,fn)<SUCCESS){
         ReportErrors("Main",0); exit(-1);
      }
      // explore structure
      const cst_item *it, *itlast = NULL;
      float x,y;
      int i;
      string lastword="0"; x = 0;
      for (i=1,it = relation_head(utt_relation(utt, "Segment")); it!=NULL; it = item_next(it),i++)
      {
         printf("Segment %d\n",i);
         y = item_feat_float(it,"end");
         string ph = string(ffeature_string(it,"p.name"));
         string wd = string(ffeature_string(it,"R:SylStructure.parent.parent.name"));
         //printf("end = %f ph=%s wd=%s\n",y,ph.c_str(),wd.c_str());
         if (wd != lastword){
            printf("**** end of %s = %f\n",lastword.c_str(),x);
            lastword=wd;
         }
         x = y;
      }
      //if (itlast!=NULL) {
      //   word = string(ffeature_string(itlast,"R:SylStructure.parent.parent.name"));
      //   idx = text.find(word);
      //}


      return 0;
   }



   catch (ATK_Error e){ ReportErrors("ATK",e.i); }
   catch (HTK_Error e){ ReportErrors("HTK",e.i); }
   return 0;
}

