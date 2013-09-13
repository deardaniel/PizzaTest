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
#ifndef _ATK_ALOG
#define _ATK_ALOG
#include "AComponent.h"

typedef list<AWaveData*> WaveDataList;


// ALOG COMPONENT -----------------------------------------
class ALog: public AComponent {
 public:
  HTime starttime;
  HTime endtime;
  ALog(const string &name, ABuffer *inb, const string & logDirInit,
       const string &userIdInit, const string &hostIdInit );
  void Start(HPriority priority=HPRIO_NORM);

 private:
  friend TASKTYPE TASKMOD ALog_Task(void *p);
  void ExecCommand(const string &cmdname);
  void MakeSessionDirectory();
  void SetSessionDirName();
  void SetWavFileName();
  void WriteWav();
  void WriteLog();
  void ClearCounters();
  void OutputWavFile();
  void CheckWrite(void *data, int size, int n, FILE *fp);
  void LogStr(string strtolog);
  string TimeStamp();
  WaveDataList waveCache;
  ABuffer *in;
  long int nSamples;
  int nUtts;
  int trace;
  string logDirName;       //root of log dirs - set externally
  string sessionDirName;   //host name + date + time
  string sessionDirPrefix; //host name - set externally
  string wavFileName;      //full name of current wav file
  string wavFilePrefix;    //user name - set externally

  string recStr;
  Boolean isOutputing;
};
#endif
