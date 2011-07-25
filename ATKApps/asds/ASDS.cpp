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
/*               Copyright Steve Young 2000-2006               */
/*                                                             */
/*   Use of this software is governed by a License Agreement   */
/*    ** See the file License for the Conditions of Use  **    */
/*    **     This banner notice must not be removed      **    */
/*                                                             */
/* ----------------------------------------------------------- */
/*       File: ASDS.cpp - Asynchronous Spoken Dialog System    */
/* ----------------------------------------------------------- */


char * asds_version="!HVER!ASDS: 1.6.0 [SJY 01/06/07]";

#include "AMonitor.h"
#include "AIO.h"

// ---------------- Globals To Define the Recognition System -----------------
class QA;

// Information Channel - connects to AIO output
static ABuffer *inChan;        // input channel from AIO

// Active components (threads)
// NB: AIO will start-up its own ASource,ASyn,ACode & ARec
static AIO *aio;               // the AIO subsystem
static AMonitor *amon;         // system monitor

// Global resources - these will be passed to AIO on creation
ARMan *rman;       // resource manager for dict, grammars and HMMSet
AHmms *hset;       // HMM set is global since it never changes
ADict *dict;       // ditto dictionary (though it can be edited if desired)
AGram *ggrm;       // global grammar, always active
QA *qty, *top;     // question/answer instances

// ------------------ Simple Question/Answer Class ----------------------

// the status of each slot determines the dialogue flow.  each slot
// starts as unknown.  When an answer is provided with high confidence
// (>minconf), the slot is immediately grounded.  Otherwise the slot
// status moves to unconfirmed ready for explicit checking.

typedef enum { unknown, unconfirmed, grounded, cancelled } SlotStatus;

// the system listens to the user in QA.listen, this implements a simple
// state machine where the states are:
typedef enum { aActive,aInProcess,aTimeout,aDone,aTerminated} ASRStatus;
typedef enum { sActive, sDone, sInt } SynStatus;

static Boolean terminated = FALSE;

static const float minconf = 0.5;

class QA {
public:
   QA(const string& aname,			// name of qa object
      const string& aprompt,		// query prompt
      const string& gramfile,		// name of grammar file
      const string& ahelp);		// help message
   ASRStatus Listen(string prompt, string rgroup, SynStatus& ss);
            // output prompt and listen using given res group and update slot value
   void Ask();							// ask question, record answer and set status
   void Check();						// check answer and set status
   void GetSlot();               // get a value for the slot by asking and checking
   string GetValue();            // strip tag and return actual value
   void Show();						// show current slot status
   void Reset();
   SlotStatus status;				// slot status
   string  value;						// slot value
   float  curconf;					// current input
   string curtag;
   string curwords;
private:
   string name;                  // name of slot (also name of semantic tag)
   string prompt;						// question
   string help;						// help string
   ResourceGroup *ask;
   ResourceGroup *chk;
};

// construct a QA object with given prompt and grammar
QA::QA(const string& aname, const string& aprompt,
       const string& gramfile, const string& ahelp)
{
   // save the prompts
   prompt = aprompt; help = ahelp; name = aname;
   // create grammar specific to this question
   AGram *g1 = new AGram(name,gramfile);
   rman->StoreGram(g1);
   ask = rman->NewGroup(name+"-ask");
   ask->AddHMMs(hset);           // Add the global resources
   ask->AddDict(dict);
   ask->AddGram(ggrm);           // include global grammar in parallel with
   ask->AddGram(g1);             // qa specific grammar
   // create copy of ask but prepend "No" to front of qa grammar
   // and add confirm in parallel
   chk = rman->NewGroup(name+"-chk");
   chk->AddHMMs(hset);           // Add the global resources
   chk->AddDict(dict);
   chk->AddGram(ggrm);           // include global grammar
   AGram *g2 = new AGram(*g1);   // copy g1
   rman->StoreGram(g2);
   g2->OpenEdit();               // Modify copy of g1 by prepending
   GramSubN *s = g2->main;       // "NO" to front with skip to end
   GramNode *ent = s->NewNullNode(99);
   GramNode *no  = s->NewWordNode("NO","no");
   GramNode *yes = s->NewCallNode("confirm","yes");   // Add confirm in //
   s->AddLink(ent,no); s->AddLink(ent,yes); s->AddLink(yes,s->exit);
   s->AddLink(no,s->entry); s->AddLink(no,s->exit);
   s->entry = ent;
   g2->CloseEdit();
   chk->AddGram(g2);    // Add the new "correction" grammar
   // initial status is unknown
   status = unknown; value = "";
}

// Reset slot values for a new dialogue
void QA::Reset()
{
   status = unknown; value = "";
}

// For debugging only
void QA::Show()
{
   string s = "???";

   switch (status){
      case unknown:	   s = "unknown";	break;
      case unconfirmed: s = "unconfirmed";	break;
      case grounded:	   s = "grounded";	break;
      case cancelled:   s = "cancelled";	break;
   }
   printf("Slot: %s:  status=%s  value=%s  [cur=%s(%s) %.1f]\n",
      name.c_str(),s.c_str(),value.c_str(),curtag.c_str(),curwords.c_str(),curconf);
}

// listen to rec packets and construct slot value
// return system status when finished listening
ASRStatus QA::Listen(string prompt, string rgroup, SynStatus& ss)
{
   APacket p;
   string s="";
   float conf=0.0;
   int numwords=0;
   ASRStatus as;

   curtag=""; curwords="";
   // set the resource group to use and start recognising
   string grp = "setgroup("+rgroup+")";
   string prmpt = "ask(\""+prompt+"\")";
   printf("%s --> %s\n",prmpt.c_str(),grp.c_str());
   aio->SendMessage(grp);
   aio->SendMessage(prmpt);
   // listen to response of form  "tag(w1 w2 ...)"
   // Following implements a simple state machine ss= syn state as=asr state
   ss = sActive; as = aActive;
   do {
      AStringData *sd;
      ACommandData *cd;
      APhraseData *pd;
      string cmd;
      p = inChan->GetPacket();
      switch(p.GetKind()) {
         case StringPacket:
            sd = (AStringData *)p.GetData();
            if (sd->data.find("TERMINATED") != string::npos) {
               as = aTerminated;
            }
            break;
         case CommandPacket:
            cd = (ACommandData *)p.GetData();
            cmd = cd->GetCommand();
            if (cmd=="synFinished")
               ss = sDone;
            else if (cmd=="synInterrupted")
               ss = sInt;
            else if (cmd=="asrTimeOut")
               as = aTimeout;
            break;
         case PhrasePacket:
            pd = (APhraseData *)p.GetData();
            if (as == aActive && pd->ptype == OpenTag_PT){
               assert(s == "" && numwords == 0);
               as = aInProcess;
            }else if (as == aInProcess && pd->ptype == Word_PT) {
               if (s=="") s = pd->word; else s = s + " " + pd->word;
               // printf("{%s[%.2f]}",pd->word.c_str(),pd->confidence);
               conf += pd->confidence; ++numwords;
            }else if (as == aInProcess &&
               (pd->ptype == CloseTag_PT || pd->ptype == End_PT)){
               curtag = pd->tag;
               curwords = s;
               as = aDone;
            }
            break;
      }
   } while (as <= aInProcess);
   curconf = numwords==0?0.0:conf/numwords;
   printf("\n  Response:  %s(%s)[%.1f]\n",curtag.c_str(),curwords.c_str(),curconf);
   return as;
}

// ask for information
void QA::Ask()
{
   Boolean ok = FALSE;
   ASRStatus as; SynStatus ss;
   do {
      as=Listen(prompt,ask->gname,ss);
      if (as==aDone){
         if (curtag == "command") {
            if (curwords == "HELP") {
               aio->SendMessage("tell(\""+help+"\")");
            }else if (curwords == "CANCEL"){
               aio->SendMessage("tell(\"Cancelled\")");
               ok = TRUE; status = cancelled;
            }
         }
         if (curtag == name){
            if (curconf > minconf){
               status = grounded;
            }else{
               status = unconfirmed;
            }
            value = curtag+"("+curwords+")";
            ok = TRUE;
         }
      }else if (as==aTimeout) {
         printf("Timeout\n");
         aio->SendMessage("tell(\"Sorry, I didnt hear that!\")");
      }else if (as==aTerminated) {
         terminated = TRUE;
      }
   } while (!ok && !terminated);
}

// confirm current slot value
void QA::Check()
{
   Boolean ok = FALSE;
   ASRStatus as; SynStatus ss;
   string prompt;
   do {
      prompt = "Are sure you want "+GetValue()+" ?";
      as = Listen(prompt,chk->gname,ss);
      if (as==aDone){
         if (curtag == "command") {
            if (curwords == "HELP") {
               aio->SendMessage("tell(\"I am trying to confirm your last input.\")");
               aio->SendMessage("tell(\"Please say Yes or No.  If No, you can also give the correct value.\")");
            }else if (curwords == "CANCEL"){
               ok = TRUE; status = cancelled;
            }
         }
         if (curtag == "yes"){
            ok = TRUE; status = grounded;
         }else	if (curtag == name){
            if (curconf > minconf){
               status = grounded;
            }else{
               status = unconfirmed;
            }
            value = curtag+"("+curwords+")";
            ok = TRUE;
         }else {
            ok = TRUE; status = unknown;   // user must have said No without correction
         }
      }else if (as==aTimeout) {
         aio->SendMessage("Sorry, I didnt hear that!");
      }else if (as==aTerminated) {
         terminated = TRUE;
      }
   } while (!ok && !terminated);
}

void QA::GetSlot()
{
   do {
      Ask();
      while (status == unconfirmed) Check();
   }while (status != grounded && status != cancelled && !terminated);
}

string QA::GetValue()
{
   int n = value.find("(");
   if (n==string::npos) return "";
   ++n;
   int m = value.length() - n -1;
   string val = value.substr(n,m);
   for (int i = 0; i<m; i++){
      int ch = val[i];
      val[i] = tolower(ch);
   }
   return val;
}

// ---------------------- Initialisation Code ---------------------------

void Initialise(int argc, char *argv[])
{
   if (InitHTK(argc,argv,asds_version)<SUCCESS){
      HRError(9999,"ASDS: cannot initialise HTK\n");
      throw HTK_Error(9999);
   }
   printf("\nASDS: ATK Asynchronous Spoken Dialog System\n");
   printf("===========================================\n\n");
   printf("\nWARNING - this demo uses an open microphone.\n");
   printf("Use headphones to ensure that speech output\n");
   printf("is not picked up by the microphone.\n\n");
   printf("     Press Return to Continue\n\n");
   getchar();

}

void BuildRecogniser()
{
   // create the return channel from AIO
   inChan = new ABuffer("inChan");
   // create a resource manager
   rman = new ARMan;
   // create global fixed resources
   hset = new AHmms("HmmSet");
   dict = new ADict("ADict");
   ggrm = new AGram("GGram","global.net");
   rman->StoreHMMs(hset);
   rman->StoreDict(dict);
   rman->StoreGram(ggrm);
   ResourceGroup *main = rman->NewGroup("main");
   main->AddHMMs(hset); main->AddDict(dict); main->AddGram(ggrm);
   // create the qa objects, these will create the resource groups
   top = new QA("topping",
      "What topping would you like?",
      "topping.net",
      "I have a variety of toppings\n - try your favourite combination");
   qty = new QA("quantity",
      "How many pizzas would you like?",
      "howmany.net",
      "I need to know how many pizzas to deliver");

   // now we have resources, create aio
   aio = new AIO("aio",inChan,rman);
   aio->DefineFiller("ER");
   aio->DefineFiller("SIL");
   aio->DefineFiller("OH");

   // finally create Monitor and attach it to AIO
   amon = new AMonitor;
   aio->AttachMonitor(amon);
}

void StartRecogniser()
{
   // Start up each component thread
   amon->Start();
   aio->Start();
}

void ShutDown()
{
   printf("Shutting down\n");
   // wait till components shutdown
   aio->SendMessage("closedown()");
   // then kill monitor
   amon->Terminate();
   HJoinMonitor();
}

// ------------------------- Application Code ---------------------------

void RunApplication()
{
   // Start recogniser running
   StartRecogniser();

   do{
      top->GetSlot();
      if (!terminated && top->status == grounded) qty->GetSlot();
      if (top->status == grounded && qty->status == grounded) {
         string order = "Your order is "+qty->GetValue()+" "+top->GetValue();
         if (qty->GetValue() == "one")
            order += " pizza.";
         else
            order += " pizzas.";
         printf("Order: %s\n",order.c_str());
         aio->SendMessage("tell(\""+order+"\")");
      }
      top->Reset(); qty->Reset();
      printf("\n\n=====================================\n\n");
   }while(!terminated);
   ShutDown();
}

// --------------------------- Main Program ----------------------------

int main(int argc, char *argv[])
{
   try {
      Initialise(argc,argv);
      BuildRecogniser();
      RunApplication();
   }
   catch (ATK_Error e){
      int n = HRErrorCount();
      printf("ATK Error %d\n",e.i);
      for (int i=1; i<=n; i++)
         printf("  %d. %s\n",i,HRErrorGetMess(i));
   }
   catch (HTK_Error e){
      int n = HRErrorCount();
      printf("HTK Error %d\n",e.i);
      for (int i=1; i<=n; i++)
         printf("  %d. %s\n",i,HRErrorGetMess(i));
   }
   return 0;
}

// ------------------------- End ASDS.cpp -----------------------------
