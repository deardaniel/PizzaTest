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
/*      File: AComponent.h -     Virtual Component Class       */
/* ----------------------------------------------------------- */

/* !HVER!AComponent: 1.6.0 [SJY 01/06/07] */

#ifndef _ATK_COMPONENT
#define _ATK_COMPONENT

#include "ABuffer.h"

#define MSGEVENTID 255

class AComponent {
public:
  AComponent(){}
  AComponent(const string & name, const int numPrBufLines=1);

  // Start the component executing
  void Start(HPriority pr, TASKTYPE (TASKMOD *task)(void *));

  // Send message to component.  A message is a text string of form
  //            command(arg1,arg2,...)
  // every component supports the following commands
  //    suspend()   - suspend processing
  //    resume()    - resume processing following a suspend
  //    terminate() - soft kill
  // returns false if command does not parse
  Boolean SendMessage(const string message);

  // Forward the current command message to tgt
  void ForwardMessage(AComponent *tgt);

  // Wait for the component to stop and join with it, result is thread
  // exit status
  int Join();

  // Request that incoming messages generate a buffer event
  void RequestMessageEvents();

  // Check if there is a message pending and exec it, if wait
  // then wait until there is a message
  void ChkMessage(Boolean wait = FALSE);

  // Terminate the component
  void Terminate ();

  // Task status queries
  Boolean IsTerminated(){return terminated;}
  Boolean IsSuspended(){return suspended;}

  // Command execution, derived type must supply this.
  virtual void ExecCommand(const string & cmdname) = 0;

  // Get args from ccmd; called by installed commands
  // returns FALSE on error
  Boolean GetIntArg(int & arg, int lo, int hi);
  Boolean GetStrArg(string & arg);
  Boolean GetFltArg(float & arg, float lo, float hi);

  // Component task information
  string  cname;             // name of component
  HThread thread;            // the task itself
  int prBufLines;            // number of lines in thread printf buffer
protected:
  // Task control flags
  Boolean terminated;        // set to request a soft kill
  Boolean suspended;         // set to indicate process is suspended
  Boolean started;           // set to indicate has been started

  // Current command being executed by ChkMessage
  APacket cmd;               // the command packet itself
  int     carg;              // index of next arg in cmd
  ABuffer *mbuf;             // buffer used by SendMessage
};

typedef AComponent * AComponentPtr;

#endif
// ---------------------- End of AComponent.h -----------------------------
