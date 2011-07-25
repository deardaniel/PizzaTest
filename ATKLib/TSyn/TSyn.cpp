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
/*   File: TSyn.cpp -     Test the Synthesis component         */
/* ----------------------------------------------------------- */

static const char * version="!HVER!TSyn: 1.6.0 [SJY 01/06/07]";

#include "AMonitor.h"
#include "ASyn.h"
#include "FliteSynthesiser.h"

static int seqnum = 0;         // current output message seq number
static ASource *asrc;          // the source/sink component
static ASyn *asyn;             // the synthesiser
static FSynthesiser *synDevice;// flite synthesiser
static ABuffer *adcToMain;     // sampled input data from ADC
static ABuffer *synToSink;     // sampled output data to DAC
static ABuffer *sinkToSyn;     // ack channel from sink to syn
static ABuffer *synToMain;     // rep buffer from syn

enum SynStatus{prompting,recording,replaying,waiting};
static SynStatus state;

// SendOutput: send text to syn
void SendOutput(string text)
{
   ++seqnum;
   printf("TSyn: sending \"%s[%d]\" to syn\n",text.c_str(),seqnum);
   asyn->SendMessage("talk("+text+")");
}

// DiscardPacket: discard incoming packet
void DiscardPacket()
{
   APacket p = adcToMain->GetPacket();
}

// IsStartPkt: get next adcToMain packet and check that its a START pkt
Boolean IsStartPacket()
{
   if (adcToMain->IsEmpty())
      HError(999,"IsStartPacket: String packet expected but adcToMain empty");
   APacket p = adcToMain->GetPacket();
   if (p.GetKind() == StringPacket){
      AStringData * sd = (AStringData *) p.GetData();
      if (sd->GetMarker() == "START") return TRUE;
   }
   return FALSE;
}

// IsStopPacket: if next adcToMain is a STOP packet, get it and return true.
Boolean IsStopPacket()
{
   APacket p = adcToMain->PeekPacket();
   if (p.GetKind() == StringPacket){
      AStringData * sd = (AStringData *) p.GetData();
      if (sd->GetMarker() != "STOP"){
         adcToMain->PopPacket();
         return TRUE;
      }
   }
   return FALSE;
}

void GetAck(string& ack)
{
   APacket p = synToMain->GetPacket();
   if (p.GetKind() != CommandPacket)
      HError(999,"GetAck: String packet expected");
   ACommandData * cd = (ACommandData *) p.GetData();
   ack = cd->GetCommand();
   printf("TSyn: ack rxed - "); cd->Show(); printf("\n");
}

void RandWait(int n)
{
   int w = RandomValue()*n;
   // int w = n;
   HPauseThread(w);
}


#define synToMainID 20
#define adcToMainID 21

int main(int argc, char *argv[])
{
   HEventRec e;
   string ack;
   char *arg;

   try {
      if (InitHTK(argc,argv,version)<SUCCESS){
      // if (NCInitHTK("TSource.cfg",version)<SUCCESS){
         ReportErrors("Main",0); exit(-1);
      }
      printf("TSyn: Synthesiser Test\n");

       // Create Buffers
      adcToMain = new ABuffer("adcToMain");
      synToSink = new ABuffer("synToSink");
      synToMain = new ABuffer("synToMain");
      sinkToSyn = new ABuffer("sinkToSyn");

      adcToMain->RequestBufferEvents(adcToMainID);
      synToMain->RequestBufferEvents(synToMainID);

      // Create Source/Sink
      asrc = new ASource("ASource",adcToMain);

      // Create Synthesiser
      synDevice = new FSynthesiser();
      asyn = new ASyn("ASyn", synToMain, synToSink, sinkToSyn, asrc, synDevice);

      // Create Monitor and Start it
      AMonitor amon;
      amon.AddComponent(asrc);
      amon.AddComponent(asyn);
      amon.Start();

      // Prompt for input, and replay it
      state = prompting; printf("   state->prompting\n");
      printf("Starting Synthesiser Test\n");
      asrc->Start();
      asyn->Start();
      SendOutput("Please provide input");
      RandWait(1000);
      asrc->SendMessage("start()");
      while (!asrc->IsTerminated()){
         //printf("TSyn: waiting for event ...");
         e = HGetEvent(0,0);
         // printf("... got %d\n",e.event);
         if (e.event == HTBUFFER) {
            // printf("    HTBUFFER %d, state=%d, inpkts=%d, ackpkts=%d\n",
            //  e.c,state,adcToMain->NumPackets(),synToMain->NumPackets());
            switch(e.c){
            case adcToMainID:
               if (!adcToMain->IsEmpty()){
                  if (state==prompting || state==waiting || state==replaying) {
                     if (IsStartPacket()){
                        if (state != waiting){
                           asyn->SendMessage("mute()");
                        }
                        state=recording; printf("   state->recording\n");
                        RandWait(2000);
                        asrc->SendMessage("stop()");
                     }
                  }else if (state==recording) {
                     if (IsStopPacket()){
                        state=replaying; printf("   state->replaying\n");
                        SendOutput("Recording Done");
                     }else{
                        DiscardPacket();
                     }
                  }
               }
               break;
            case synToMainID:
               if (! synToMain->IsEmpty()){
                  GetAck(ack);
                  if (ack=="finished") {
                     if (state==prompting){
                        state = waiting; printf("   state->waiting\n");
                     } else if (state==replaying) {
                        state=prompting; printf("   state->prompting\n");
                        SendOutput("Please provide even more input if you would be so kind");
                        RandWait(2000);
                        asrc->SendMessage("start()");
                     }
                  }
               }
               break;
            }
         }
      }
      asrc->Join();
      asyn->Join();
      // Shutdown
      printf("Waiting for monitor\n");fflush(stdout);
      amon.Terminate();
      HJoinMonitor();
      return 0;
   }
   catch (ATK_Error e){ ReportErrors("ATK",e.i); }
   catch (HTK_Error e){ ReportErrors("HTK",e.i); }
   return 0;
}
