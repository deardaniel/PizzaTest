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
/*   File: APacket.cpp -   Implementation of Packet Classes    */
/* ----------------------------------------------------------- */

char * apacket_version="!HVER!APacket: 1.6.0 [SJY 01/06/07]";

// Modification history:
//   13/12/02 - modified Show to make it neater
//   14/07/04 - added min/max display to wavepacket.Show()
//   02/09/04 - (MNS) added a mutex lock for shared count data
//   26/09/04 - use lighweight global lock for efficiency

#include "APacket.h"


// ------------------ APacketHeader -----------------

// Standard Constructor
APacketHeader::APacketHeader(APacketData * apd)
{
   startTime = endTime = 0;
   count = 1;  theData = apd;
}

// Destructor
APacketHeader::~APacketHeader()
{
   assert(theData != 0);
   delete theData;
}

// Print the header data and then show the data, if any
void APacketHeader::Show()
{
   char *k;

   switch(theData->kind){
  	case EmptyPacket:		k = "Empty"; break;
  	case StringPacket:		k = "String"; break;
  	case CommandPacket:		k = "Command"; break;
  	case WavePacket:		k = "Wave"; break;
  	case ObservationPacket:         k = "Observation"; break;
  	case PhrasePacket:		k = "Phrase"; break;
  	case AnyPacket:			k = "Any"; break;
   }
   printf("%s Packet[%p] refs=%d; time=(%.2f-%.2f) ms\n",
      k,this,count,startTime/10000,endTime/10000);
   assert(theData != 0);
   theData->Show();
}

// -------------------- APacket ---------------------------

// Empty packet constructor
APacket::APacket()
{
   AEmptyData *p = new AEmptyData();
   thePkt = new APacketHeader(p);
}

// Basic constructor to create new packet with supplied data
APacket::APacket(APacketData *apd)
{
   thePkt = new APacketHeader(apd);
}

// Create a packet sharing some existing header+data
APacket::APacket(const APacket& pkt)
{
  HGlobalLock();
  thePkt = pkt.thePkt; ++thePkt->count;
  HGlobalUnlock();
}

// Redefine assignment to share data
APacket& APacket::operator=(const APacket& pkt)
{
   HGlobalLock();
   pkt.thePkt->count++;
   if (--thePkt->count == 0) delete thePkt;
   thePkt = pkt.thePkt;
   HGlobalUnlock();
   return *this;

}

// Destructor, delete data when no more refs
APacket::~APacket()
{
  HGlobalLock();
  if (thePkt != 0 && --thePkt->count <= 0)
    delete thePkt;
  HGlobalUnlock();
}

// Show the packet and its contents
void APacket::Show()
{
   thePkt->Show();
}

//  Get/Put Properties
HTime APacket::GetStartTime(){ return thePkt->startTime;}
void APacket::SetStartTime(HTime t){ thePkt->startTime = t;}
HTime APacket::GetEndTime(){ return thePkt->endTime;}
void APacket::SetEndTime(HTime t){ thePkt->endTime = t;}

// Return kind of packet
PacketKind APacket::GetKind()
{
   assert(thePkt != 0 && thePkt->theData != 0);
   return thePkt->theData->kind;
}

// Return a pointer to embedded data packet
APacketData *APacket::GetData()
{
   assert(thePkt != 0);
   return thePkt->theData;
}

// =============== Actual Container Types ================

// -------------------- Empty -----------------------

AEmptyData::AEmptyData()
{
   kind=EmptyPacket;
}

void AEmptyData::Show()
{
   printf("Data: <>\n");
}

// ------------------ Commands -----------------------

// Command Constructor/Destructor
ACommandData::ACommandData(const string& cmd)
{
   cmdname=cmd; numArgs=0;
   kind = CommandPacket;
}
ACommandData::~ACommandData()
{
   for (int i=0; i<numArgs; i++){
      if (arg[i].kind == args) delete arg[i].value.s;
   }
}

// Methods to add arguments
Boolean ACommandData::AddArg(const string& s){
   if (numArgs==MAXCMDARG) return FALSE;
   arg[numArgs].value.s = new string(s);
   arg[numArgs++].kind = args;
   return TRUE;
}
Boolean ACommandData::AddArg(const int i){
   if (numArgs==MAXCMDARG) return FALSE;
   arg[numArgs].value.i = i;
   arg[numArgs++].kind = argi;
   return TRUE;
}
Boolean ACommandData::AddArg(const float f){
   if (numArgs==MAXCMDARG) return FALSE;
   arg[numArgs].value.f = f;
   arg[numArgs++].kind = argf;
   return TRUE;
}

// Return the command
string ACommandData::GetCommand(){
   return cmdname;
}

// Get number of args
int ACommandData::NumCmdArgs(){
   return numArgs;
}

// Test kind of n'th arg
Boolean ACommandData::IsString(int n){
   if (! (n<numArgs)) return FALSE;
   return (arg[n].kind == args)?TRUE:FALSE;
}
Boolean ACommandData::IsNumber(int n){
   if (! (n<numArgs)) return FALSE;
   return (arg[n].kind != args)?TRUE:FALSE;
}

// Return arg values
string ACommandData::GetString(int n){
   if (! (n<numArgs)) return "";
   return *arg[n].value.s;
}
float ACommandData::GetFloat(int n){
   if (! (n<numArgs)) return 0.0;
   return arg[n].value.f;
}
int ACommandData::GetInt(int n){
   if (! (n<numArgs)) return 0;
   return arg[n].value.i;
}

// Display the command
void ACommandData::Show()
{
   printf(" %s(",cmdname.c_str());
   for (int i=0; i<numArgs; i++){
      switch(arg[i].kind){
      case args: printf("%s",arg[i].value.s->c_str()); break;
      case argi: printf("%i",arg[i].value.i); break;
      case argf: printf("%f",arg[i].value.f); break;
      }
      if (i<numArgs-1) printf(",");
   }
   printf(")\n");
}

// -------------------- String -----------------------

AStringData::AStringData()
{
   data=""; kind=StringPacket;
}

AStringData::AStringData(const string& s)
{
   data=s; kind=StringPacket;
}

string AStringData::GetSource()
{
   int n = data.find("::");
   if (n==string::npos) {
      HRError(0,"AStringData:GetSource bad format %s\n",data.c_str());
      throw ATK_Error(999);
   }
   return data.substr(0,n);
}

string AStringData::GetMarker(Boolean tagOnly)
{
   int n = data.find("::");
   if (n==string::npos) {
      HRError(0,"AStringData:GetMarker bad format %s\n",data.c_str());
      throw ATK_Error(999);
   }
   n += 2;
   int m = data.length() - n;
   string s = data.substr(n,m);
   if (tagOnly){
      int k = 0;
      while (k<m && isalpha(s[k])) k++;
      s = s.substr(0,k);
   }
   return s;
}

void AStringData::Show()
{
   printf(" %s\n",data.c_str());
}

// -------------------- Wave -------------------------

// Wave array fixed size (for now)
AWaveData::AWaveData()
{
   kind = WavePacket;
   wused = 0;
}

// Wave array copied from given source x
AWaveData::AWaveData(const int n, short *x)
{
   assert(n<=WAVEPACKETSIZE);
   kind = WavePacket;
   for (int i=0; i<n; i++) data[i] = x[i];
   wused = n;
}

// Display first few samples of waveform
void AWaveData::Show()
{
   int n = 0,min=100000,max=-100000;

   printf(" %d samples",wused);
   if (wused>0){
      for (int i = 0; i<wused; i++){
         if (data[i]>max) max = data[i];
         if (data[i]<min) min = data[i];
      }
      printf(" [min=%d, max=%d]\n",min,max);
      for (int row = 1; row<=3; row++){
         printf("   ");
         for (int col = 1; col<=10; col++){
            if (n>=wused)
               break;
            else
               printf("%5d",data[n++]);
         }
         printf("\n");
         if (n>wused) break;
      }
      if (n<wused) printf("      ...\n");
   }else{
      printf("\n");
   }
}

// -------------------- ObservationPacket -------------------------

// Create the observation structure from given info
AObsData::AObsData(BufferInfo *info, int numStreams)
{
   Vector v; int size,*ip;

   kind = ObservationPacket;
   // Set up stream widths
   ZeroStreamWidths(numStreams,data.swidth);
   SetStreamWidths(info->tgtPK,info->tgtVecSize,data.swidth,&(data.eSep));
   // Make the observation - assume not discrete
   data.pk = info->tgtPK; data.bk = data.pk&(~HASNULLE);
   for (int i=1; i<=numStreams; i++){
      size = data.swidth[i];
      v = (Vector) new float[size+1];
      ip = (int *) v; *ip = size;
      data.fv[i] = v;
   }
}

// Show the contents of the observation
void AObsData::Show()
{
   const int n = 10;

   ExplainObservation(&data, n);
   PrintObservation(0,&data, n);
}

// Delete the internal feature vectors
AObsData::~AObsData()
{
   for (int i=1; i<=data.swidth[0]; i++)
      delete[] data.fv[i];
}

// ---------------------- PhrasePacket ---------------------------

// basic constructor
APhraseData::APhraseData(PhraseType pt, int thisseq, int lastseq)
{
   kind = PhrasePacket;
   ptype = pt; seqn = thisseq; pred = lastseq;
   alt = -1; word=""; tag=""; ac = lm = score = 0; nact = 0;
   confidence = -1;
}

void APhraseData::Show()
{
   printf(" %3d <--[%3d]: ",pred,seqn);
   switch(ptype){
   case Start_PT: printf("!START"); break;
   case End_PT:   printf("!END"); break;
   case Quit_PT:  printf("!QUIT"); break;
   case OpenTag_PT:
      printf("!OPENTAG");
      if (alt>=0) printf("[alt=%d]",alt);
      break;
   case CloseTag_PT:
      printf("!CLOSETAG <%s>",tag.c_str()); break;
   case Null_PT:
      printf("!NULL");
      if (tag != "") printf("<%s>",tag.c_str());
      break;
   case Word_PT:
      if (tag != "")
         printf("!WORD=%s <%s> [ac=%.1f,lm=%.1f,cf=%.2f]",
         word.c_str(),tag.c_str(),ac,lm,confidence);
      else
         printf("!WORD=%s [ac=%.1f,lm=%.1f,cf=%.2f]",
         word.c_str(),ac,lm,confidence);
      break;
   }
   printf(" Score=%.4f, nact=%.1f\n",score,nact);
}

// ----------------------End of APacket.cpp -----------------------






