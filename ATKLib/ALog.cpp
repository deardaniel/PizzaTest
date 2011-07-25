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
/*        File: ALog.h - Logging agent (text and speech        */
/* ----------------------------------------------------------- */

char * alog_version="!HVER!ALog: 1.6.0 [SJY 01/06/07]";

#include "ALog.h"

#ifdef WIN32
#include <Windows.h>
#endif
#ifdef UNIX
#include <sys/stat.h>
#endif

#define INBUFID 1

#define T_TOP 0001    // top level tracing

ALog::ALog(const string &name, ABuffer *inb, const string & logDirInit,
           const string &userIdInit, const string &hostIdInit ): AComponent(name,4)
{
   // Preform initializations from constructor
   in = inb;
   logDirName = logDirInit;
   sessionDirPrefix= hostIdInit;
   wavFilePrefix = userIdInit;
   // Read config values from Config file
   ConfParam *cParm[MAXGLOBS];       /* will store config parameters */
   char buf[1000];
   int numParm;
   int i;
   double f;
   strcpy(buf,name.c_str());
   for (i=0; i<strlen(buf); i++) buf[i] = toupper(buf[i]);
   numParm = GetConfig(buf, TRUE, cParm, MAXGLOBS);
   if (numParm>0){
      if (GetConfStr(cParm,numParm,"LOGDIRNAME",buf)) logDirName = buf;
      if (GetConfStr(cParm,numParm,"USERID",buf)) wavFilePrefix = buf;
      if (GetConfStr(cParm,numParm,"HOSTID",buf)) sessionDirPrefix = buf;
      if (GetConfInt(cParm,numParm,"TRACE",&i)) trace = i;
   }
   SetSessionDirName();
   // init class vars
   nSamples=0; nUtts=0; isOutputing=FALSE; starttime=0; endtime=0;
}

// return a time-stamp string
string ALog::TimeStamp()
{
   struct tm *nt;
   char buf[1000];
   time_t long_time;
   time( &long_time );           /* Get time as long integer. */
   nt = localtime( &long_time ); /* Convert to local time. */
   sprintf(buf,"%02d%02d%02d_%02d%02d%02d", nt->tm_year-100,
      nt->tm_mon + 1,nt->tm_mday,nt->tm_hour,nt->tm_min,nt->tm_sec);
   return string(buf);
}

// compute unique session dir name
void ALog::SetSessionDirName()
{
   sessionDirName = sessionDirPrefix+"-"+TimeStamp();
}

// check for session dir and if necessary create it
// and add session.cfg
void ALog::MakeSessionDirectory()
{
   string path=logDirName+"/"+sessionDirName;
#ifdef WIN32
   if (CreateDirectory(path.c_str(),NULL)){
#endif
#ifdef UNIX
   if (! mkdir(path.c_str(),0777)){
#endif
      string cfg=path+"/session.cfg";
      FILE *f = fopen(cfg.c_str(),"w");
      if (f==NULL)
         HError(999,"MakeSessionDirectory: cant create session.cfg");
      WriteConfig(f);
   }
}

// compute wave file name
void ALog::SetWavFileName()
{
   char buf[50];
   sprintf(buf,"-%03d-",nUtts);
   string nu = string(buf);
   string fname = wavFilePrefix+nu+TimeStamp()+".wav";
   wavFileName = logDirName+"/"+sessionDirName+"/"+fname;
}

TASKTYPE TASKMOD ALog_Task(void * p);
void ALog::Start(HPriority priority)
{
   AComponent::Start(priority,ALog_Task);
}

void ALog::CheckWrite(void *data, int size, int n, FILE *fp)
{
  if(fwrite(data,size,n,fp)!=n)
    HError(1011,"WriteWave: cannot write to file %s.\n",wavFileName.c_str());
}

/* WriteWave:  Write a waveform file */
void ALog::WriteWav()
{
	APacket p;
	AWaveData *w, *toStore;
	HTime st=starttime, et=endtime;
	if(st>=et) return;

	nSamples=0;
	p=in->PeekPacket();
	while(p.GetEndTime()<st-10000000){
		in->PopPacket();
		p=in->PeekPacket();
	}
	while(p.GetStartTime()<=et){
		if(p.GetKind()==WavePacket){
			w = (AWaveData *)p.GetData();
			toStore=new AWaveData(w->wused, w->data);
			waveCache.push_back(toStore);
			nSamples+=w->wused;
		}
		in->PopPacket();
		p=in->PeekPacket();
	}
   MakeSessionDirectory();
	FILE *fp; char str[128];
	fp= fopen(wavFileName.c_str(),"wb");
	if (!fp)
		HError(1011,"WriteWave: cannot create file %s.",wavFileName.c_str());
	sprintf(str,"RIFF");
	CheckWrite(str,1,4,fp);
	long tmp=nSamples*2+36;
	CheckWrite(&tmp,4,1,fp);
	sprintf(str,"WAVEfmt ");
	CheckWrite(str,1,8,fp);
	tmp=16;
	CheckWrite(&tmp,4,1,fp);
	short val=1;
	CheckWrite(&val,2,1,fp);
	val=1;
	CheckWrite(&val,2,1,fp);
	tmp=16000;
	CheckWrite(&tmp,4,1,fp);
	tmp=16000*2;
	CheckWrite(&tmp,4,1,fp);
	val=2;
	CheckWrite(&val,2,1,fp);
	val=16;
	CheckWrite(&val,2,1,fp);
	sprintf(str,"data");
	CheckWrite(str,1,4,fp);
	tmp=nSamples*2;
	CheckWrite(&tmp,4,1,fp);

	while(!waveCache.empty()){
		AWaveData *wd;
		wd=waveCache.front();
		CheckWrite(wd->data,2,wd->wused,fp);
		delete(wd);
		waveCache.pop_front();
	}

	fclose(fp);
	if(trace&T_TOP)
		printf("wrote wav to %s\n",wavFileName.c_str());

}

// write LOG
void ALog::WriteLog()
{
   FILE *fp;
   char fn[1000];

   sprintf(fn,"%s/%s/session.log",logDirName.c_str(),sessionDirName.c_str());
   MakeSessionDirectory();
   fp = fopen(fn, "a");
   if (!fp) HError(1011,"WriteLog: cannot create Log file %s",fn);
   if(fprintf(fp,"%s %s\n",wavFileName.c_str(), recStr.c_str())<0)
      HError(1011,"WriteLog: cannot write to file %s.",fn);
   fclose(fp);
}

void ALog::ClearCounters()
{
   nSamples=0;
   nUtts++;
   recStr="";
}

void ALog::OutputWavFile()
{
	isOutputing=TRUE;
	SetWavFileName();
	WriteWav();
	WriteLog();
	isOutputing=FALSE;
}

// write LOG
void ALog::LogStr(string strtolog)
{
   FILE *fp;
   char fn[1000];

   sprintf(fn,"%s/%s/session.log",logDirName.c_str(),sessionDirName.c_str());
   MakeSessionDirectory();
   fp = fopen(fn, "a");
   if (!fp) HError(1011,"WriteLog: cannot create Log file %s.",fn);
   if(fprintf(fp,"%s\n", strtolog.c_str())<0)
      HError(1011,"LogStr: cannot write to file %s.",fn);
   fclose(fp);
}

// Implement the command interface
void ALog::ExecCommand(const string & cmdname)
{
   string strarg, str;
   if (cmdname == "startrec"){
      ClearCounters();
   }else if (cmdname == "stoprec"){
      OutputWavFile();
   }else if (cmdname == "appendstr" ){
      if(!GetStrArg(strarg))
         HPostMessage(HThreadSelf(),"appendstr - str expected\n");
      else
         recStr=recStr+" "+strarg;
   }else if (cmdname == "logstr" ){
      strarg="";
      while(GetStrArg(str))
         strarg+=str+",";
      if(strarg=="")
         HPostMessage(HThreadSelf(),"logstr - str expected\n");
      strarg.erase(strarg.length()-1,1);
      LogStr(strarg);
   }else if (cmdname == "setlogdir" ){
      if(!GetStrArg(strarg))
         HPostMessage(HThreadSelf(),"setlogdir - dirname expected\n");
      else
         logDirName=strarg;
   }else if (cmdname == "setuserid" ){
      if(!GetStrArg(strarg))
         HPostMessage(HThreadSelf(),"setusername - userid expected\n");
      else
         wavFilePrefix=strarg;
   }else if (cmdname == "sethostid" ){
      if(!GetStrArg(strarg))
         HPostMessage(HThreadSelf(),"sethostid - name expected\n");
      else {
         sessionDirPrefix=strarg;
         SetSessionDirName();
      }
   }else {
      char c[100];
      sprintf(c,"unknown command: %s\n",cmdname.c_str());
      HPostMessage(HThreadSelf(),c);
   }
}

TASKTYPE TASKMOD ALog_Task(void * p)
{
   ALog *alog = (ALog *)p;
   char buf[100];
   HEventRec e;
   PacketKind inpk;
   APacket pkt;
   AWaveData *w;
   HTime pst;
   AStringData *sd;

   try{
      alog->in->RequestBufferEvents(INBUFID);
      alog->RequestMessageEvents();

      while (!alog->IsTerminated()){
         e = HGetEvent(0,0);
         if (e.event==HTBUFFER){
            switch(e.c) {
              case MSGEVENTID:
                   alog->ChkMessage();
                   while (alog->IsSuspended()) alog->ChkMessage(TRUE);
                   break;
              case INBUFID:
                   break;
            }
         }
      }

      HExitThread(0);
      return 0;
   }
   catch (ATK_Error e){ ReportErrors("ATK",e.i); return 0;}
   catch (HTK_Error e){ ReportErrors("HTK",e.i); return 0;}
}
