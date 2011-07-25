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
/*        File: ATee.h - Interface to packet buffers        */
/* ----------------------------------------------------------- */

#ifndef _ATK_TEE
#define _ATK_TEE

#include "AComponent.h"

// ATEE ====================================
class ATee: public AComponent {
 public:
  ATee(const string &name, ABuffer *inb, ABuffer *outb1, ABuffer *outb2);
  void Start(HPriority priority=HPRIO_NORM);
 private:
  friend TASKTYPE TASKMOD CopyFe(void *p);
  void ExecCommand(const string &cmdname);
  ABuffer *in, *out1, *out2;
};

#endif
