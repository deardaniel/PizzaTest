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
/*     File: AComponent.cpp -     Virtual Component Class      */
/* ----------------------------------------------------------- */

char * acomponent_version="!HVER!AComponent: 1.6.0 [SJY 01/06/07]";

#include "AComponent.h"

AComponent::AComponent(const string & name, const int numPrBufLines)
{
   cname = name; thread = 0;
   terminated = FALSE;
   suspended = FALSE;
   started = FALSE;
   mbuf = new ABuffer(name+":mbuf");
   mbuf->SetFilter(CommandPacket);
   prBufLines = numPrBufLines;
}

// Forward command to another component
void AComponent::ForwardMessage(AComponent *tgt)
{
   tgt->mbuf->PutPacket(cmd);
}

// Send message in string form via mbuf
Boolean AComponent::SendMessage(const string message)
{
   char buf[1000],*obrak,*cbrak,*s,*t;
   int i;

   // copy message to a C string and make sure commands are lower case
   strcpy(buf,message.c_str());
   for (i=0; i<int(strlen(buf)) && buf[i] != '('; i++) buf[i] = tolower(buf[i]);

   // make sure it has form of cmd(..)
   obrak = strchr(buf,'('); if (obrak==NULL) return FALSE;
   cbrak = strrchr(buf,')'); if (cbrak==NULL) return FALSE;
   *obrak = '\0'; s=obrak+1;

   // now create command container
   if (strlen(buf)<1)  return FALSE;
   ACommandData *cd = (ACommandData *)new ACommandData(string(buf));

   // for string argument in quotes.
   if(*s=='\"' && *(cbrak-1)=='\"'){
     *cbrak='\0';
     if(!cd->AddArg(string(s))) return FALSE;
     s=cbrak;
   }

   // and add the arguments
   while (s < cbrak){
      t = strchr(s,','); if (t==NULL) t = cbrak;
      *t = '\0';
      if (*s=='+' || *s=='-' || isdigit(*s)){
         if(strchr(s,'.')==NULL){  // integer
            if (*s=='0'){   // octal
               i=0; ++s;
               while (*s) i = i*8 + (*s++ - '0');
               if (!cd->AddArg(i)) return FALSE;
            }else {        //
               if (!cd->AddArg((int)atoi(s))) return FALSE;
            }
         }else{                    // float
            if (!cd->AddArg((float)atof(s))) return FALSE;
         }
      }else
         if (!cd->AddArg(string(s))) return FALSE;
      s = t+1;
   }

   // finally send the packet
   APacket p(cd);
   mbuf->PutPacket(p);
   return TRUE;
}

// Request that incoming messages generate a buffer event
void AComponent::RequestMessageEvents()
{
   mbuf->RequestBufferEvents(MSGEVENTID);
}

// Check message buffer and execute first message if any
void AComponent::ChkMessage(Boolean wait)
{
   ACommandData *cd;

   if (wait || !mbuf->IsEmpty()){
      cmd = mbuf->GetPacket(); carg=0;
      cd = (ACommandData *)cmd.GetData();
      string cmdname = cd->GetCommand();
      if (cmdname == "terminate")
         terminated = TRUE;
      else if (cmdname == "suspend")
         suspended = TRUE;
      else if (cmdname == "resume")
         suspended = FALSE;
      else
         ExecCommand(cmdname);
   }
}

// Functions to retrieve args from current command
Boolean AComponent::GetStrArg(string & arg)
{
   ACommandData *cd = (ACommandData *)cmd.GetData();

   if (carg>cd->NumCmdArgs()) return FALSE;
   if (!cd->IsString(carg)) return FALSE;
   arg = cd->GetString(carg);
   carg++;
   return TRUE;
}

Boolean AComponent::GetIntArg(int & arg, int lo, int hi)
{
   ACommandData *cd = (ACommandData *)cmd.GetData();

   if (carg>cd->NumCmdArgs()) return FALSE;
   if (!cd->IsNumber(carg)) return FALSE;
   arg = cd->GetInt(carg);
   if (hi>lo && (arg<lo || arg>hi)) return FALSE;
   carg++;
   return TRUE;
}

Boolean AComponent::GetFltArg(float & arg, float lo, float hi)
{
   ACommandData *cd = (ACommandData *)cmd.GetData();

   if (carg>cd->NumCmdArgs()) return FALSE;
   if (!cd->IsNumber(carg)) return FALSE;
   arg = cd->GetFloat(carg);
   if (hi>lo && (arg<lo || arg>hi)) return FALSE;
   carg++;
   return TRUE;
}

// Start the component in its own thread with priority pr.  Pass this
// to the thread so that it can still access its members
void AComponent::Start(HPriority pr, TASKTYPE (TASKMOD *task)(void *))
{
   if(!started)
      thread = HCreateThread(cname.c_str(),prBufLines,pr,task,(void *)this);
   started=TRUE;
}

// Wait to join with the components thread
int AComponent::Join()
{
   int status;
   HJoinThread(thread,&status);
   return status;
}

// Terminate component
void AComponent::Terminate()
{
   // HKillThread(thread);
   terminated = TRUE;
}


// --------------------End of AComponent.cpp -------------------------
