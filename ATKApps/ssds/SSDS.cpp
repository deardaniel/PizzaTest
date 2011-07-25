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
/*       File: SSDS.cpp -    Simple Spoken Dialog System       */
/* ----------------------------------------------------------- */

char * ssds_version="!HVER!SSDS: 1.6.0 [SJY 01/06/07]";

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif
#include "AMonitor.h"
#include "ASource.h"
#include "ACode.h"
#include "ARec.h"

class SSDSBufferCallback : public ABufferCallback {
   void ABufferReceivedPacket(ABuffer &buffer, APacket &packet);

};

void SSDSBufferCallback::ABufferReceivedPacket(ABuffer &buffer, APacket &packet)
{
   printf("Received packet: %s.\n", buffer.GetName().c_str());
   packet.Show();
}


// ---------------- Globals To Define the Recognition System -----------------

// Information Channels (plumbing)
ABuffer *auChan;   // carries audio from source to Coder
ABuffer *feChan;   // carries feat vecs from Coder to Recogniser
ABuffer *ansChan;  // carries answers from Rec back to Application
ABufferCallback *bufferCallback;
// Active components (threads)
ASource *ain;      // audio source
ACode *acode;      // coder
ARec *arec;      // viterbi recogniser
AMonitor *amon;    // system monitor
// Global resources
ARMan *rman;       // resource manager for dict, grammars and HMMSet
AHmms *hset;       // HMM set is global since it never changes
ADict *dict;       // ditto dictionary (though it can be edited if desired)
AGram *ggrm;       // global grammar, always active

bool terminated=false;

// output a prompt (just a text string for this simple demo )
void Talk(string s, string curtag = string(""))
{
#ifdef __APPLE__
	CFStringRef text = CFStringCreateWithCString(NULL, s.c_str(), kCFStringEncodingUTF8);
	CFStringRef tag = CFStringCreateWithCString(NULL, curtag.c_str(), kCFStringEncodingUTF8);

	CFStringRef values[2] = { text, tag };
	CFStringRef keys[2] = { CFSTR("Text"), CFSTR("Tag") };

	CFDictionaryRef userInfo = CFDictionaryCreate(NULL, (const void **)keys, (const void **)values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFRelease(text);
	CFRelease(tag);

	CFNotificationCenterPostNotification(CFNotificationCenterGetLocalCenter(), CFSTR("Talk"), NULL, userInfo, false);
	CFRelease(userInfo);
#endif
   printf("\n%s\n",s.c_str());
}


// ------------------ Simple Question/Answer Class ----------------------

// the status of each slot determines the dialogue flow.  each slot
// starts as unknown.  When an answer provided with high confidence
// (>minconf), the slot is immediately grounded.  Otherwise the slot
// status moves to unconfirmed ready for explicit checking.

typedef enum { unknown, unconfirmed, grounded, cancelled } SlotStatus;
static const float minconf = 0.5;

class QA {
public:
   QA(const string& aname,			// name of qa object
      const string& aprompt,		// query prompt
      const string& gramfile,		// name of grammar file
      const string& ahelp);		// help message
   void Listen(string rgroup);	// listen using given res group and update slot value
   void Ask();							// ask question, record answer and set status
   void Check();						// check answer and set status
   void GetSlot();               // get a value for the slot by asking and checking
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
   g2->OpenEdit();
   GramSubN *s = g2->main;       // prepend "NO" to front with skip to end
   GramNode *ent = s->NewNullNode(99);
   GramNode *no = s->NewWordNode("NO","no");
   GramNode *yes =s->NewCallNode("confirm","yes");
   s->AddLink(ent,no); s->AddLink(ent,yes); s->AddLink(yes,s->exit);
   s->AddLink(no,s->entry); s->AddLink(no,s->exit);
   s->entry = ent;
   g2->CloseEdit();
   chk->AddGram(g2);             // Add the new "correction" grammar
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
case unknown:	  s = "unknown";	break;
case unconfirmed: s = "unconfirmed";	break;
case grounded:	  s = "grounded";	break;
case cancelled:	  s = "cancelled";	break;
   }
   printf("Slot: %s:  status=%s  value=%s  [cur=%s(%s) %.1f]\n",
      name.c_str(),s.c_str(),value.c_str(),curtag.c_str(),curwords.c_str(),curconf);
}

// listen to rec packets and construct slot value
void QA::Listen(string rgroup)
{
   APacket p;
   APhraseData *pd;
   string s="";
   float conf=0.0;
   int numwords=0;

   if(terminated) return;
   curtag=""; curwords="";
   // set the resource group to use and start recognising
   arec->SendMessage("usegrp("+rgroup+")");
   arec->SendMessage("start()");
   // listen to response of form  "tag(w1 w2 ...)"
   do {
      p = ansChan->GetPacket();
      pd = (APhraseData *)p.GetData();
      if (p.GetKind() == StringPacket){
         AStringData * sd = (AStringData *)p.GetData();
         if (sd->data.find("TERMINATED") != string::npos) {
            terminated = TRUE;
         }
      }
      else {
         if (pd->ptype == OpenTag_PT){
            assert(s == "" && numwords == 0);
            do {
               p = ansChan->GetPacket(); pd = (APhraseData *)p.GetData();
               if (pd->ptype == Word_PT) {
                  if (s=="") s = pd->word; else s = s + " " + pd->word;
                  // printf("{%s[%.2f]}",pd->word.c_str(),pd->confidence);
                  conf += pd->confidence; ++numwords;
               }
            } while(pd->ptype != CloseTag_PT && pd->ptype != End_PT);
            curtag = pd->tag;
            curwords = s;
         }
      }
   } while ((pd->ptype != End_PT) && (!terminated));
   arec->SendMessage("stop()");
   curconf = numwords==0?0.0:conf/numwords;
   printf("\n  Response:  %s(%s)[%.1f]\n",curtag.c_str(),curwords.c_str(),curconf);
}

// ask for information
void QA::Ask()
{
   bool ok = false;

   do {
      Talk(prompt, name);
      Listen(ask->gname);
      if (curtag == "command") {
         if (curwords == "HELP") {
            Talk(help, name);
         }else if (curwords == "CANCEL"){
            ok = true; status = cancelled;
         }
      }
      if (curtag == name){
         if (curconf > minconf){
            status = grounded;
         }else{
            status = unconfirmed;
         }
         value = curtag+"("+curwords+")";
         ok = true;
      }
   } while (!ok && !terminated);

}

// confirm current slot value
void QA::Check()
{
   bool ok = false;

   do {
      Talk("Are your sure you want "+value+" ?", "check");
      Listen(chk->gname);
      if (curtag == "command") {
         if (curwords == "HELP") {
            Talk("I am trying to confirm your last input", "check");
            Talk("Please say Yes or No.  If No, you can also give the correct value", "check");
         }else if (curwords == "CANCEL"){
            ok = true; status = cancelled;
         }
      }
      else if (curtag == "yes"){
         ok = true; status = grounded;
      }else	if (curtag == name){
         if (curconf > minconf){
            status = grounded;
         }else{
            status = unconfirmed;
         }
         value = curtag+"("+curwords+")";
         ok = true;
      }else {
         ok = true; status = unknown;   // user must have said No without correction
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


// ---------------------- Initialisation Code ---------------------------

void Initialise(int argc, char *argv[])
{
   if (InitHTK(argc,argv,ssds_version)<SUCCESS){
      HRError(9999,"SSDS: cannot initialise HTK\n");
      throw HTK_Error(9999);
   }
   printf("\nSSDS: ATK Simple Spoken Dialog System\n");
   printf("=====================================\n\n");
}

void BuildRecogniser()
{
   // create some plumbing
   auChan = new ABuffer("auChan");
   feChan = new ABuffer("feChan");
   ansChan = new ABuffer("ansChan");

   bufferCallback = new SSDSBufferCallback();
   feChan->AddCallback(bufferCallback);
  
   // create a resource manager
   rman = new ARMan;
   // create active components linked by buffers
   ain = new ASource("AIn",auChan);
   acode = new ACode("ACode",auChan,feChan);
   arec = new ARec("ARec",feChan,ansChan,rman);
   // create global fixed resources
   hset = new AHmms("HmmSet"); // load info in config
   AObsData *od = acode->GetSpecimen();
   if (!hset->CheckCompatible(&(od->data))){
      HRError(9999,"SSDS: HMM set is not compatible with Coder");
      throw ATK_Error(9999);
   }
   dict = new ADict("ADict");  // load info in config
   ggrm = new AGram("GGram","global.net");
   rman->StoreHMMs(hset);
   rman->StoreDict(dict);
   rman->StoreGram(ggrm);
   // finally create Monitor
   amon = new AMonitor;
   amon->AddComponent(ain);   //register components
   amon->AddComponent(acode); //with the monitor
   amon->AddComponent(arec);
}

void StartRecogniser()
{
   // Start up each component thread
   amon->Start();
   ain->Start();
   // uncomment following if completely hands-free
   ain->SendMessage("start()");
   acode->Start();
//   acode->SendMessage("start()");
   arec->Start();
//   arec->SendMessage("start()");
   // note recogniser will not actually do anything until it
   // receives a "start" command
}

void ShutDown()
{
   printf("Shutting down\n");
   // wait till components shutdown
   ain->Join(); acode->Join();  arec->Join();
   // then kill monitor
   amon->Terminate();
   HJoinMonitor();
}

// ------------------------- Application Code ---------------------------

void RunApplication()
{
   // create some qa objects
   QA *top = new QA("topping",
      "What topping would you like?",
      "topping.net",
      "I have a variety of toppings\n - try your favourite combination");
   QA *qty = new QA("quantity",
      "How many pizzas would you like?",
      "howmany.net",
      "I need to know how many pizzas to deliver");

   // Start recogniser running
   StartRecogniser();

   do{
      top->GetSlot();
      if (!terminated && top->status == grounded) qty->GetSlot();
      if (top->status == grounded && qty->status == grounded) {
         Talk("Your order is: \n   "+top->value+"  "+qty->value, "completed");
      }
      top->Reset(); qty->Reset();
      printf("\n\n=====================================\n\n");
   }while(!terminated);

   ShutDown();
}

// --------------------------- Main Program ----------------------------

#ifdef __APPLE__
CFStringRef CopyMacBundlePath()
{
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    assert(mainBundle);
	
    CFURLRef mainBundleURL = CFBundleCopyBundleURL(mainBundle);
    assert(mainBundleURL);
	
    CFStringRef cfStringRef = CFURLCopyFileSystemPath( mainBundleURL, kCFURLPOSIXPathStyle);
    assert(cfStringRef);
	
    CFRelease(mainBundleURL);
	
    return cfStringRef;
}

void *startSSDS(void *)
{
	CFStringRef bundlePath = CopyMacBundlePath();
	CFRelease(bundlePath);
	char confPath[1024];
	chdir(CFStringGetCStringPtr(bundlePath, CFStringGetFastestEncoding(bundlePath)));
	sprintf(confPath, "%s/Test/ssds.cfg", CFStringGetCStringPtr(bundlePath, CFStringGetFastestEncoding(bundlePath)));

	printf("CWD: %s\n", confPath);
	int argc = 3;
	const char *argv[] = { "SSDS", "-C", "ssds.cfg" };
#else
int main(int argc, char *argv[])
{
#endif
   try {
      Initialise(argc, (char **)argv);
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
	CFRelease(confPath);
#ifndef __APPLE__
   return 0;
#endif
}

// ------------------------- End SSDS.cpp -----------------------------
