/* ----------------------------------------------------------- */
/*                                                             */
/*               _ ___       _   _   _                         */
/*              /_\ | |_/ _ /_\ |_| |_|                        */
/*              | | | | \   | | |   |                          */
/*              =========   =========                          */
/*                                                             */
/*      Application Example using the ATK Real-Time API        */
/*                                                             */
/*       Machine Intelligence Laboratory (Speech Group)        */
/*        Cambridge University Engineering Department          */
/*                  http://mi.eng.cam.ac.uk/                   */
/*                                                             */
/*               Copyright Steve Young 2000-2005               */
/*                                                             */
/*   Use of this software is governed by a License Agreement   */
/*    ** See the file License for the Conditions of Use  **    */
/*    **     This banner notice must not be removed      **    */
/*                                                             */
/* ----------------------------------------------------------- */
/*           File: AVite.cpp -    HVite replacement            */
/* ----------------------------------------------------------- */

char * avite_version="!HVER!AVite: 1.6.0 [SJY 01/06/07]";

// Modification history:
// 27/04/03  Minor edits
// 30/10/03  Output MLF generation changed so only file names are output not full path
// 01/11/03  Bug in rec parameter handling (config vars always overwritten)
// 01/11/03  Support for NGrams added, including -g option
// 10/04/04  Bug in setting of max model fixed
// 20/04/04  Support for word level alignments added

#include "AMonitor.h"
#include "ASource.h"
#include "ACode.h"
#include "ARec.h"

#define T_TOP 00001      /* Basic progress reporting */
static int trace = 0;

// ---------------- Globals To Define the Recognition System -----------------

// Information Channels (plumbing)
ABuffer *auChan;   // carries audio from source to Coder
ABuffer *feChan;   // carries feat vecs from Coder to Recogniser
ABuffer *ansChan;  // carries answers from Rec back to Application
// Active components (threads)
ASource *ain;      // audio source
ACode *acode;      // coder
ARec *arec;        // viterbi recogniser
AMonitor *amon;    // system monitor
// Global resources
ARMan *rman;       // resource manager for dict, grammars and HMMSet
AHmms *hset;       // HMM set is global since it never changes
ADict *dict;       // ditto dictionary (though it can be edited if desired)
AGram *gram;       // global recognition grammar, always active
ANGram *ngram;     // NGram if any

// Command line arguments

string hmmlist;    // file containing hmmlist
string mmffile0;   // main hmm mmf file
string mmffile1;   // aux mmf file
string dctfile;    // dictionary file
string grmfile;    // grammar file for recognition
string ngrmfile;   // ngram lm file
string scpfile;    // list of files to test
string outfile;    // name of output file
char * mlffile;    // input mlf file for alignment

Boolean wordPenSet = FALSE;    // rec control constants
float wordPen;
Boolean prScaleSet = FALSE;
float prScale;
Boolean lmScaleSet = FALSE;
float lmScale;
Boolean ngScaleSet = FALSE;
float ngScale;
Boolean genBeamSet = FALSE;
float genBeam;
Boolean wordBeamSet = FALSE;
float wordBeam;
Boolean maxActiveSet = FALSE;
int maxActive;
int nToks = 0;

char * labForm = NULL;     // output label reformat
Boolean doAlign = FALSE;   // enable alignment mode
list<string> scpList;      // list of wav files to process
string align_ext = "lab";  // extension to use in align mlf
string align_s, align_e;   // optional align start end words
FILE *outf;
char *xformFN = NULL;

// ---------------------- Initialisation Code ---------------------------

void ReportUsage(void)
{
   printf("\nUSAGE: AVite [options] VocabFile HMMList DataFiles...\n\n");
   printf(" Option                                   Default\n\n");
   printf(" -i s    Output transcriptions to MLF s      off\n");
   printf(" -g s    Load n-gram language model from s   none\n");
   printf(" -n i    Set num tokens to i                 0\n");
   printf(" -o s    output label formating FSN          none\n");
   printf(" -p f    inter model trans penalty (log)     1.0\n");
   printf(" -q f    ngram lm scale factor               0.0\n");
   printf(" -r f    pronunciation scale factor          1.0\n");
   printf(" -s f    link lm scale factor                1.0\n");
   printf(" -t f    set general beam threshold          225.0\n");
   printf(" -u i    set pruning max active              0\n");
   printf(" -v f    set word beam threshold             175.0\n");
   printf(" -w s    recognise from network              off\n");
   printf(" -z s e  bracket align transcripts with s/e  none\n");
   printf(" -J tmf  Load transform model file tmf\n");
   PrintStdOpts("HIX");
   printf("\n\n");
}

void checkneeded(const string &s, char * mess, char * sw)
{
   if (s == ""){
      HRError(3219,"AVite: %s needed [use switch %s]",mess,sw);
      throw ATK_Error(3219);
   }
}

void Initialise(int argc, char *argv[])
{
   char *s;

   DisableSCPHandling();
   if (InitHTK(argc,argv,avite_version,TRUE)<SUCCESS){
      HRError(9999,"AVite: cannot initialise HTK\n");
      throw HTK_Error(9999);
   }
   if (!InfoPrinted() && NumArgs() == 0)
      ReportUsage();
   if (NumArgs() == 0) Exit(0);
   EnableBTrees();   /* allows unseen triphones to be synthesised */
   while (NextArg() == SWITCHARG) {
      s = GetSwtArg();
      if (strlen(s)!=1) {
         HRError(3219,"AVite: Bad switch %s; must be single letter",s); throw ATK_Error(3219);
      }
      switch(s[0]){
      case 'i':
         outfile = GetStrArg();
         break;
      case 'g':
         ngrmfile = GetStrArg();
         break;
      case 'n':
         nToks = GetChkedInt(0,MAX_TOKS,s);
         break;
      case 'o':
         if (NextArg()!=STRINGARG)
            HError(3219,"HVite: Output label format expected");
         labForm = GetStrArg();
         break;
      case 'p':
         wordPen = GetChkedFlt(-1000.0,1000.0,s);
         wordPenSet = TRUE;
         break;
      case 'q':
         ngScale = GetChkedFlt(0.0,1000.0,s);
         ngScaleSet = TRUE;
         break;
      case 'r':
         prScale = GetChkedFlt(0.0,1000.0,s);
         prScaleSet = TRUE;
         break;
      case 's':
         lmScale = GetChkedFlt(0.0,1000.0,s);
         lmScaleSet = TRUE;
         break;
      case 't':
         genBeam = GetChkedFlt(0.0,1.0E20F,s);
         if (genBeam == 0.0)  genBeam = -LZERO;
         genBeamSet = TRUE;
         break;
      case 'u':
         maxActive = GetChkedInt(0,100000,s);
	      maxActiveSet = TRUE;
         break;
      case 'v':
         wordBeam = GetChkedFlt(0.0,1.0E20F,s);
         if (wordBeam == 0.0) wordBeam = -LZERO;
         wordBeamSet = TRUE;
         break;
      case 'w':
         grmfile = GetStrArg();
         break;
      case 'z':
         if (NextArg()!=STRINGARG)
            HError(3219,"AVite: Align start word expected");
         align_s = GetStrArg();
         if (NextArg()!=STRINGARG)
            HError(3219,"AVite: Align end word expected");
         align_e = GetStrArg();
         break;
      case 'H':
         if (mmffile1 != ""){
            HRError(3219,"AVite: max of two MMF files allowed"); throw ATK_Error(3219);
         }
         if (mmffile0 == "")
            mmffile0 = string(GetStrArg());
         else
            mmffile1 = string(GetStrArg());
         break;
      case 'I':
         mlffile = GetStrArg(); doAlign=TRUE;
         LoadMasterFile(mlffile);
         break;
      case 'S':
         scpfile = GetStrArg();     break;
      case 'T':
         trace = GetChkedInt(0,511,s); break;
      case 'X':
         if (NextArg()!=STRINGARG)
            HError(3219,"AVite: Align label file extension expected");
         align_ext = GetStrArg(); break;
      case 'J':
	 if (NextArg()!=STRINGARG)
	    HError(2319,"AVite: input transform filename expected");
	 xformFN = GetStrArg();
	 break;
      default:
         HRError(3219,"AVite: Unknown switch %s",s); throw ATK_Error(3219);
      }
   }
   if (NextArg()!=STRINGARG) {
      HRError(3219,"AVite: Dictionary file name expected"); throw ATK_Error(3219);
   }
   dctfile = string(GetStrArg());
   if (NextArg()!=STRINGARG) {
      HError(3219,"AVite: HMM list file name expected"); throw ATK_Error(3219);
   }
   hmmlist = string(GetStrArg());
   if (doAlign) {
      checkneeded(mlffile,"Alignment MLF File","-I");
   }else{
      checkneeded(grmfile,"Grammar File","-w");
   }
   checkneeded(scpfile,"File List","-S");
   checkneeded(mmffile0,"Model File","-H");
   checkneeded(outfile,"Output MLF","-i");
   // open output mlf
   outf = fopen(outfile.c_str(),"w");
   if (outf==NULL) {
      HRError(3219,"AVite: cant create output mlf %s",outfile.c_str());
      throw ATK_Error(3219);
   }
}

// LoadSCPFileList: read list of files in scpfile and store in scpList
void LoadSCPFileList()
{
   Source src;
   char buf[1000];
   int i;

   strcpy(buf,scpfile.c_str());
   ReturnStatus r = InitSource(buf, &src, NoFilter);
   if (r != SUCCESS) {
      HRError(3220,"AVite: cannot open scp file %s",buf);
      throw ATK_Error(3220);
   }
   while (ReadLine(&src,buf)){
      i = strlen(buf)-1;
      while (i>=0 && isspace(buf[i])) i--;
      buf[i+1] = '\0';
      scpList.push_back(string(buf));
   }
}

// NextSCPFile: return next file from scpList, "" if none
string NextSCPFile()
{
   string s;

   if (scpList.size() == 0) return "";
   s = scpList.front(); scpList.pop_front();
   return s;
}

// BuildRecogniser: put together the ATK components needed to build a recogniser
void BuildRecogniser()
{
   // create some plumbing
   auChan = new ABuffer("auChan");
   feChan = new ABuffer("feChan");
   ansChan = new ABuffer("ansChan");
   // create a resource manager
   rman = new ARMan;
   // create active components linked by buffers
   ain = new ASource("AIn",auChan,scpfile);
   acode = new ACode("ACode",auChan,feChan);
   arec = new ARec("ARec",feChan,ansChan,rman,nToks);
   if (wordPenSet)  arec->wordPen  = wordPen;
   if (prScaleSet)  arec->prScale  = prScale;
   if (lmScaleSet)  arec->lmScale  = lmScale;
   if (ngScaleSet)  arec->ngScale  = ngScale;

   if (genBeamSet)  {
      arec->genBeam  = genBeam;
      if (arec->nBeam < arec->genBeam)
         arec->nBeam = arec->genBeam;
   }
   if (wordBeamSet) arec->wordBeam = wordBeam;
   if (maxActiveSet) arec->maxActive = maxActive;

   // load hmm set and ensure it is compatible with coder
   hset = new AHmms("HmmSet",hmmlist,mmffile0,mmffile1);
   AObsData *od = acode->GetSpecimen();
   if (!hset->CheckCompatible(&(od->data))){
      HRError(3221,"AVite: HMM set is not compatible with Coder");
      throw ATK_Error(3221);
   }
   rman->StoreHMMs(hset);

   // Init input xform
   if (xformFN != NULL) {
     hset->AddXForm(xformFN);
   }

   // load the dictionary
   dict = new ADict("ADict",dctfile);
   rman->StoreDict(dict);

   // add resource group
   ResourceGroup *rg = rman->NewGroup("main");
   rg->AddHMMs(hset);
   rg->AddDict(dict);

   // add global grammar network
   if (doAlign){
      // for alignment create main subnet with o--NULL[0]--NULL[1]--o
      gram = new AGram("GGram","");
      gram->OpenEdit();
      GramSubN * stub = gram->NewSubN("stub");
      GramNode * snode = stub->NewNullNode(0);
      GramNode * enode = stub->NewNullNode(1);
      stub->AddLink(snode,enode,1.0);
      stub->SetEnds(snode,enode);
      gram->main = stub;
      gram->CloseEdit();
   }else{
      // for recognition load a grammar file
      gram = new AGram("GGram",grmfile);
   }
   rman->StoreGram(gram);
   rg->AddGram(gram);

   // add ngram if specified
   if (ngrmfile != "") {
      ngram = new ANGram("NGram",ngrmfile);
      rman->StoreNGram(ngram);
      rg->AddNGram(ngram);
   }

   // finally create Monitor
   //  amon = new AMonitor;
   //  amon->AddComponent(ain);   //register components
   //  amon->AddComponent(acode); //with the monitor
   //  amon->AddComponent(arec);
   //  amon->Start();
}


// StartRecogniser: start recogniser component but leave it in idle state
void StartRecogniser()
{
   // Start up each component thread
   ain->Start();
   acode->Start();
   arec->Start();
   // Set run mode to one-shot  (need ability to switch grammars in align mode)
   arec->SendMessage("setmode(01221");
}

// StartRecognising: activate both the audio source and the recogniser
void StartRecognising()
{
   ain->SendMessage("start()");
   arec->SendMessage("start()");
}

void ShutDown()
{
   fclose(outf);
   // ask remaining threads to terminate
   // ain->SendMessage("terminate()");
   // acode->SendMessage("terminate()");
   // ain->Join(); acode->Join();  arec->Join();
   //   HJoinMonitor();
}

// --------------------------- Alignment Grammar Code -----------------------

// MkAlignGram: edit existing grammar to match new wavfile
void MkAlignGram(string wavfile){

   // Open the grammar for editing and strip out any old stuff
   gram->OpenEdit();
   GramSubN *main = gram->main;
   GramNode *snode = main->FindbyNullId(0);
   GramNode *enode = main->FindbyNullId(1);
   if (snode==NULL || enode==NULL){
      HRError(3200,"AVite::MkAlignGram cannot find start/end nodes");
      throw ATK_Error(3200);
   }
   GramNode *n = snode;
   GramNode *nsucc = snode->succ.front().node;   // NB only one successor always
   if (nsucc == enode){
      main->DeleteLink(snode,enode);
   }else{
      n = nsucc; nsucc=n->succ.front().node;
      while (n != enode){
         main->DeleteNode(n);
         n = nsucc; nsucc=n->succ.front().node;
      }
   }
   // Find new wavfile's transcription
   MemHeap theap;
   CreateHeap(&theap,"transheap",MSTAK,1,0.5,1000,10000);
   string s = wavfile;
   string::size_type posn = s.find(".wav");
   if (posn == string::npos){
      HRError(3200,"AVite::MkAlignGram cannot find .wav in %s",s.c_str());
      throw ATK_Error(3200);
   }
   s.replace(posn+1,3,align_ext);
   posn = s.find_last_of('/');
   if (posn != string::npos){
      s.replace(0,posn+1,"");
   }
   char ss[256];
   strcpy(ss,s.c_str());
   Transcription *t = LOpen(&theap,ss,HTK);

   // Step thru transcription and build linear word graph
   LabList *ll = GetLabelList(t,1);
   n = snode;
   if (align_s != ""){
      nsucc = main->NewWordNode(align_s);
      main->AddLink(n,nsucc,1.0);
      n = nsucc;
   }
   for (LLink p=ll->head->succ;  p!=ll->tail; p=p->succ){
      //printf("LABEL: %s\n",p->labid->name);
      nsucc = main->NewWordNode(string(p->labid->name));
      main->AddLink(n,nsucc,1.0);
      n = nsucc;
   }
   if (align_e != ""){
      nsucc = main->NewWordNode(align_e);
      main->AddLink(n,nsucc,1.0);
      n = nsucc;
   }
   main->AddLink(nsucc,enode,1.0);

   // All done
   DeleteHeap(&theap);
   gram->CloseEdit();
}

// ------------------------- File Processing Code ---------------------------

void ProcessFiles()
{
   APacket p;
   APhraseData *pd;
   PacketKind pk;
   Boolean starting;
   Boolean running;
   HTime st,en;
   float ac,lm, x;
   float lastScore;
   string wavfile,traceout;

   StartRecogniser();
   fprintf(outf,"#!MLF!#\n");  fflush(outf);
   wavfile = NextSCPFile();
   if (wavfile==""){
      HRError(3200,"AVite: scp list appears to be empty"); throw ATK_Error(3200);
   }
   if (doAlign) MkAlignGram(wavfile);
   StartRecognising();
   starting = TRUE; running = TRUE;
   do {
      p = ansChan->GetPacket();
      if ((pk=p.GetKind()) == PhrasePacket){
         pd = (APhraseData *)p.GetData();
         if (starting){
            if (pd->ptype != Start_PT){
               HRError(3200,"AVite: expecting Start_PT, got %d",pd->ptype); throw ATK_Error(3200);
            }
            if (pd->tag == "ENDOFLIST"){
               if (trace&T_TOP) {printf(" END OF LIST \n"); fflush(stdout); }
               running = FALSE;
               if (wavfile != ""){
                  HRError(3200,"AVite: source has sent ENDOFLIST but expected %s",wavfile.c_str());
                  throw ATK_Error(3200);
               }

            }else{
               if (pd->tag != wavfile){
                  HRError(3200,"AVite: source has sent '%s' but expected '%s'",pd->tag.c_str(),wavfile.c_str());
                  throw ATK_Error(3200);
               }
               st = p.GetStartTime();
               lm = 0; ac = 0; lastScore = 0.0;
               if (trace&T_TOP) {
                  printf("File: %s\n",pd->tag.c_str()); fflush(stdout);
                  traceout = "Reco:";
               }
               string::size_type posn = pd->tag.find(".wav");
               if (posn == string::npos){
                  HRError(3200,"AVite: cannot find .wav in file name %s",pd->tag.c_str());
                  throw ATK_Error(3200);
               }
               pd->tag.replace(posn,4,".rec");
               posn = pd->tag.find_last_of('/');
               if (posn != string::npos){
                  pd->tag.replace(0,posn+1,"");
               }
               fprintf(outf,"\"%s\"\n",pd->tag.c_str());  fflush(outf);
               starting = FALSE;
            }
         }else {
            if (pd->ptype == Word_PT){
               lm += pd->lm; ac += pd->ac;
               if (trace&T_TOP) traceout += " "+pd->word;
               fprintf(outf,"%.0f %.0f %s",p.GetStartTime(),p.GetEndTime(),pd->word.c_str());
               fflush(outf);
               if (labForm == NULL || strchr(labForm,'S')==NULL) {
                  if (labForm == NULL || strchr(labForm,'F')==NULL)
                     x = pd->score-lastScore;
                  else
                     x = pd->confidence;
                  if (labForm != NULL && strchr(labForm,'N')!=NULL)  {
                     st = p.GetStartTime(); en = p.GetEndTime();
                     assert(acode->GetSampPeriod() > 0.0);
                     float nx = (en - st)/acode->GetSampPeriod();
                     if (nx>0.0) x /= nx;
                  }
                  fprintf(outf," %f",x);  fflush(outf);
               }
               fprintf(outf,"\n");   fflush(outf);
               lastScore = pd->score;
            } else {
	      if (pd->ptype == CloseTag_PT) {
		int a, b;
		sscanf(pd->tag.c_str(), "%d / %d", &a, &b);
		if(a!=b)
		  fprintf(outf,"///\n");   fflush(outf);
	      }
	      if (pd->ptype == End_PT) {
		  en = p.GetStartTime();
                  assert(acode->GetSampPeriod() > 0.0);
                  float nx = (en - st)/acode->GetSampPeriod();
                  int n = (int) (nx+0.5);
                  if (trace&T_TOP) {
                     printf("%s\nInfo: frames=%d loglike=%.2f aclike=%.1f lmlike=%.1f nact=%.1f\n",
                     traceout.c_str(),n,pd->score/nx,ac,lm,pd->nact); fflush(stdout);
                  }
                  fprintf(outf,".\n");   fflush(outf);
                  wavfile = NextSCPFile();
                  if (wavfile != ""){
                     if (doAlign) MkAlignGram(wavfile);
                     StartRecognising();
                     starting = TRUE;
                  }else{
                     running = FALSE;
                  }
               }
            }
         }
      }
   }while (running);
   ShutDown();
}

// --------------------------- Main Program ----------------------------

int main(int argc, char *argv[])
{
   try {
      Initialise(argc,argv);
      LoadSCPFileList();
      BuildRecogniser();
      ProcessFiles();
      return 0;
   }
   catch (ATK_Error e){
      int n = HRErrorCount();
      printf("ATK Error %d\n",e.i);
      for (int i=1; i<=n; i++)
         printf("  %d. %s\n",i,HRErrorGetMess(i));
      fflush(stdout);
   }
   catch (HTK_Error e){
      int n = HRErrorCount();
      printf("HTK Error %d\n",e.i);
      for (int i=1; i<=n; i++)
         printf("  %d. %s\n",i,HRErrorGetMess(i));
      fflush(stdout);
   }
   return 0;
}

// ------------------------- End AVite.cpp -----------------------------
